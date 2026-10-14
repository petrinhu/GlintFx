#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# composed_photo_facts.py - QA-COMPOSED-PATH G2 (plano-fechamento-w8-r2.md sec. 5.4, D-W8-254/267):
# the facts of one composed photo, read from what tests/container/composed_photo.sh copied out of
# the container. Test tooling, not product code. Standard library only (L-07).
#
# It answers two questions, and writes the capture where tests/tools/raw_to_png.py reads it:
#   1. Which card composed? photo_verdict(): "Compositing Type: OpenGL", and ONE "OpenGL renderer
#      string:" that names the expected maker and is neither llvmpipe nor softpipe (software never
#      counts). A photo without this proof does not count (the line QA-COMPOSED-PATH, D-W8-34).
#   2. What did KWin's ScreenShot2 hand over? capture_meta() reads the a{sv} busctl printed for
#      CaptureActiveWindow, and refuses a size other than the expected one (the demo window is
#      640x480, D-W8-72, examples/first_window/first_window.cpp) and a scale other than 1.
#      QImage::Format_ARGB32_Premultiplied (6) is, byte for byte, wl_shm ARGB8888 (0) on a
#      little-endian host (B, G, R, A, premultiplied: the convention raw_to_png.py documents),
#      and Format_RGB32 (4) is XRGB8888 (1). Every other format is refused, never
#      guessed: a straight-alpha ARGB32 (5) would be read in the wrong convention.
# The criterion was written BEFORE the data (L-43): the table of selftest_verdict() below.
#
# USAGE: composed_photo_facts.py <achados> <intel|nvidia> <capture_dir> <WIDTHxHEIGHT>
#        composed_photo_facts.py --selftest
# The run prints every fact and a closing "composed_photo_facts: ..." line, always; rc 0 only when
# both answers pass and conn1_surface1.raw/.meta were written.

import pathlib
import re
import shutil
import sys
import tempfile

from selftest_controls import CHECKS, check

QIMAGE_TO_WL_SHM = {6: 0, 4: 1}
SIZE = re.compile(r"^([1-9][0-9]*)x([1-9][0-9]*)$")
PAIR = re.compile(r'"([A-Za-z][A-Za-z0-9_-]*)" ([a-z]) ("[^"]*"|\S+)')
SOFTWARE = ("llvmpipe", "softpipe")


def photo_verdict(support_text, maker):
    lines = support_text.splitlines()
    compositing = [line for line in lines if line.strip().startswith("Compositing Type:")]
    renderer = [line for line in lines if line.strip().startswith("OpenGL renderer string:")]
    if len(compositing) != 1 or compositing[0].split(":", 1)[1].strip() != "OpenGL":
        return False, f"compositing={compositing!r}"
    if len(renderer) != 1:
        return False, f"renderer={renderer!r}"
    name = renderer[0].split(":", 1)[1].strip()
    lowered = name.lower()
    if any(soft in lowered for soft in SOFTWARE) or maker not in lowered:
        return False, f"renderer={name!r} fabricante_esperado={maker}"
    return True, f"compositing=OpenGL renderer={name!r}"


def capture_meta(cap_text, expected_size):
    found = {key: value for key, kind, value in PAIR.findall(cap_text) if kind != "s"}
    try:
        width, height, stride, qformat = (int(found[k]) for k in ("width", "height", "stride", "format"))
        scale = float(found["scale"])
    except (KeyError, ValueError):
        return None, "resposta sem width, height, stride, format ou scale"
    if (width, height) != expected_size:
        return None, f"tamanho {width}x{height}, esperado {expected_size[0]}x{expected_size[1]}"
    if scale != 1.0:
        return None, f"escala {scale}, esperada 1"
    if qformat not in QIMAGE_TO_WL_SHM:
        return None, f"formato QImage {qformat} recusado (so 6 e 4 tem equivalente exato em wl_shm)"
    if width <= 0 or height <= 0 or stride < width * 4:
        return None, f"geometria invalida {width}x{height} stride={stride}"
    return (width, height, stride, QIMAGE_TO_WL_SHM[qformat]), f"{width}x{height} stride={stride} qimage={qformat}"


def write_capture(found_dir, meta, capture_dir):
    width, height, stride, wl_format = meta
    raw = pathlib.Path(found_dir) / "img.raw"
    if raw.stat().st_size < stride * height:
        return False, f"img.raw com {raw.stat().st_size} bytes, menos que stride*height"
    out = pathlib.Path(capture_dir)
    out.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(raw, out / "conn1_surface1.raw")
    (out / "conn1_surface1.meta").write_text(
        f"width={width}\nheight={height}\nstride={stride}\nformat={wl_format}\n", encoding="utf-8")
    return True, f"captura gravada em {out}"


def run(found_dir, maker, capture_dir, expected_size):
    d = pathlib.Path(found_dir)
    ok_support, why_support = photo_verdict((d / "support.txt").read_text(encoding="utf-8", errors="replace"), maker)
    print(f"placa: {'ok' if ok_support else 'REPROVA'} {why_support}")
    meta, why_meta = capture_meta((d / "cap.out").read_text(encoding="utf-8", errors="replace"), expected_size)
    print(f"captura: {'ok' if meta else 'REPROVA'} {why_meta}")
    written = False
    if meta:
        written, why_written = write_capture(d, meta, capture_dir)
        print(f"gravacao: {'ok' if written else 'REPROVA'} {why_written}")
    ok = ok_support and written
    print(f"composed_photo_facts: placa={'ok' if ok_support else 'reprova'} captura={'ok' if written else 'reprova'}")
    return 0 if ok else 1


def refused_cleanly(support_text, maker):
    # A crash is not a refusal: the criterion must refuse by its own rule (G2-fix1, mutant M09 of the N3 review).
    try:
        return not photo_verdict(support_text, maker)[0]
    except Exception:  # noqa: BLE001 - any exception means the rule did not decide
        return False


def selftest_verdict():
    good_intel = "Compositing Type: OpenGL\nOpenGL renderer string: Mesa Intel(R) Iris(R) Xe Graphics\n"
    good_nvidia = "Compositing Type: OpenGL\nOpenGL renderer string: NVIDIA GeForce RTX 3050/PCIe/SSE2\n"
    check("QPainter reprova, mesmo com o renderizador da Intel", refused_cleanly(
        "Compositing Type: QPainter\nOpenGL renderer string: Mesa Intel(R) Graphics\n", "intel"))
    check("OpenGL com llvmpipe reprova", refused_cleanly(
        "Compositing Type: OpenGL\nOpenGL renderer string: llvmpipe (LLVM 19)\n", "intel"))
    check("OpenGL na Intel, esperando intel, passa", photo_verdict(good_intel, "intel")[0])
    check("OpenGL na NVIDIA, esperando intel, reprova", refused_cleanly(good_nvidia, "intel"))
    check("OpenGL na NVIDIA, esperando nvidia, passa", photo_verdict(good_nvidia, "nvidia")[0])
    check("texto vazio reprova", refused_cleanly("", "intel"))
    check("OpenGL sem linha de renderizador reprova", refused_cleanly("Compositing Type: OpenGL\n", "intel"))
    check("software que nomeia o fabricante reprova", refused_cleanly(
        "Compositing Type: OpenGL\nOpenGL renderer string: llvmpipe (LLVM 19, intel host)\n", "intel"))
    check("dois renderizadores reprovam, mesmo o primeiro bom", refused_cleanly(
        good_intel + "OpenGL renderer string: llvmpipe (LLVM 19)\n", "intel"))


def selftest_capture():
    cap = '"type" s "raw" "width" u {w} "height" u {h} "scale" d {k} "stride" u {s} "format" u {f}'
    vga = (640, 480)
    meta, _ = capture_meta(cap.format(w=640, h=480, k=1, s=2560, f=6), vga)
    check("formato 6 vira wl_shm 0", meta == (640, 480, 2560, 0))
    meta, _ = capture_meta(cap.format(w=640, h=480, k=1, s=2560, f=4), vga)
    check("formato 4 vira wl_shm 1", meta == (640, 480, 2560, 1))
    check("formato 5 (alfa reto) reprova", capture_meta(cap.format(w=640, h=480, k=1, s=2560, f=5), vga)[0] is None)
    check("resposta sem format reprova", capture_meta('"width" u 640 "height" u 480 "scale" d 1', vga)[0] is None)
    check("stride menor que 4 bytes por pixel reprova",
          capture_meta(cap.format(w=640, h=480, k=1, s=2556, f=6), vga)[0] is None)
    check("resposta vazia reprova", capture_meta("", vga)[0] is None)
    check("1280x960 reprova, esperando 640x480",
          capture_meta(cap.format(w=1280, h=960, k=1, s=5120, f=6), vga)[0] is None)
    check("800x600 reprova, esperando 640x480",
          capture_meta(cap.format(w=800, h=600, k=1, s=3200, f=6), vga)[0] is None)
    check("640x480 reprova, esperando 800x600",
          capture_meta(cap.format(w=640, h=480, k=1, s=2560, f=6), (800, 600))[0] is None)
    check("escala 2 reprova", capture_meta(cap.format(w=640, h=480, k=2, s=2560, f=6), vga)[0] is None)
    check("resposta sem escala reprova",
          capture_meta('"width" u 640 "height" u 480 "stride" u 2560 "format" u 6', vga)[0] is None)


def selftest_run():
    with tempfile.TemporaryDirectory() as tmp:
        found = pathlib.Path(tmp) / "achados"
        found.mkdir()
        (found / "support.txt").write_text(
            "Compositing Type: OpenGL\nOpenGL renderer string: Mesa Intel(R) Graphics\n", encoding="utf-8")
        (found / "cap.out").write_text('"width" u 2 "height" u 1 "scale" d 1 "stride" u 8 "format" u 6',
                                       encoding="utf-8")
        (found / "img.raw").write_bytes(bytes(range(8)))
        out = pathlib.Path(tmp) / "captura"
        check("rodada boa: rc 0", run(found, "intel", out, (2, 1)) == 0)
        check("rodada boa: meta gravado no formato do raw_to_png",
              (out / "conn1_surface1.meta").read_text(encoding="utf-8") == "width=2\nheight=1\nstride=8\nformat=0\n")
        check("rodada boa: raw copiado byte a byte", (out / "conn1_surface1.raw").read_bytes() == bytes(range(8)))
        check("fabricante errado, raw bom: rc 1", run(found, "nvidia", pathlib.Path(tmp) / "c3", (2, 1)) == 1)
        (found / "img.raw").write_bytes(bytes(4))
        check("raw curto, placa boa: rc 1", run(found, "intel", pathlib.Path(tmp) / "c2", (2, 1)) == 1)


def main():
    if sys.argv[1:] == ["--selftest"]:
        selftest_verdict()
        selftest_capture()
        selftest_run()
        print(f"selftest: {len(CHECKS)} controles OK")
        return 0
    size = SIZE.match(sys.argv[4]) if len(sys.argv) == 5 else None
    if size is None or sys.argv[2] not in ("intel", "nvidia"):
        print("uso: composed_photo_facts.py <achados> <intel|nvidia> <capture_dir> <LARGURAxALTURA> | --selftest",
              file=sys.stderr)
        return 2
    return run(sys.argv[1], sys.argv[2], sys.argv[3], (int(size.group(1)), int(size.group(2))))


if __name__ == "__main__":
    sys.exit(main())
