#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# capture_vs_readback.py - QA-SCREEN-CAPTURE P3 (D-W8-33): compares the
# wire relay's capture of a frame against the client's own glReadPixels
# of the SAME frame, taken before the swap, pixel by pixel, alpha
# included. Test tooling, not product code. Standard library only (L-07).
#
# CAPTURE side (writer: tests/container/wire_relay/wire_frame_writer.hpp,
# reader of the same files: tests/tools/raw_to_png.py): exactly one
# conn<N>_surface<S>.raw/.meta pair in the directory. wl_shm format 0
# (ARGB8888): little-endian words, so the bytes are B,G,R,A, top row
# first, premultiplied alpha, rows `stride` bytes apart. Format 1
# (XRGB8888) is REJECTED: it forces alpha 255 in the reader, so an alpha
# comparison against it would be a comparison that cannot fail.
#
# READBACK side (writer: tests/container/capture_known_color_smoke.cpp):
# the bytes glReadPixels returned, untouched: R,G,B,A, BOTTOM row first,
# tightly packed. Its .meta says so ("width=", "height=", "origin=
# bottom_left", "order=rgba"); anything else is rejected, never guessed.
# THIS tool does the row flip and the R/B swap, in one place, so the
# convention is mapped explicitly and tested by selftest, not hidden in
# the client.
#
# VERDICT (L-40): zero pixels compared is a rejection; any differing
# pixel is a rejection; the summary line is always printed.
#
# USAGE: capture_vs_readback.py <capture_dir> <readback.raw> <readback.meta>
#        capture_vs_readback.py --selftest

import re
import subprocess
import sys
import tempfile
from pathlib import Path

from raw_to_png import CaptureError, parse_meta

SCRIPT_NAME = "capture_vs_readback.py"
RAW_RE = re.compile(r"^conn(\d+)_surface(\d+)\.raw$")
FORMAT_ARGB8888 = 0
READBACK_KEYS = ("width", "height", "origin", "order")
READBACK_FIXED = {"origin": "bottom_left", "order": "rgba"}
MAX_REPORTED = 3


def parse_readback_meta(text):
    meta = {}
    for line in text.splitlines():
        if not line.strip():
            continue
        key, separator, value = line.partition("=")
        if not separator or key not in READBACK_KEYS or key in meta:
            raise CaptureError(f"readback meta line not understood or repeated: {line!r}")
        meta[key] = value
    missing = [key for key in READBACK_KEYS if key not in meta]
    if missing:
        raise CaptureError(f"readback meta lacks keys: {missing}")
    for key, wanted in READBACK_FIXED.items():
        if meta[key] != wanted:
            raise CaptureError(f"readback {key}={meta[key]!r}, only {wanted!r} is understood")
    for key in ("width", "height"):
        if not (meta[key].isascii() and meta[key].isdigit()):
            raise CaptureError(f"readback {key} is not a decimal integer: {meta[key]!r}")
        meta[key] = int(meta[key])
    return meta


def find_single_capture(capture_dir):
    capture_dir = Path(capture_dir)
    if not capture_dir.is_dir():
        raise CaptureError(f"capture directory does not exist: {capture_dir}")
    stems = sorted(m.group(0)[: -len(".raw")] for entry in capture_dir.iterdir()
                   if entry.is_file() and (m := RAW_RE.match(entry.name)))
    if not stems:
        raise CaptureError(f"no captured frame (no conn<N>_surface<S>.raw) in {capture_dir}")
    if len(stems) > 1:
        raise CaptureError(f"{len(stems)} captured surfaces, exactly one expected: {stems}")
    return capture_dir / f"{stems[0]}.raw", capture_dir / f"{stems[0]}.meta"


def load_capture(capture_dir):
    raw_path, meta_path = find_single_capture(capture_dir)
    meta = parse_meta(meta_path.read_text())
    if meta["format"] != FORMAT_ARGB8888:
        raise CaptureError(f"capture format {meta['format']} is not ARGB8888 (0): alpha not comparable")
    raw = raw_path.read_bytes()
    if meta["stride"] < meta["width"] * 4 or len(raw) != meta["stride"] * meta["height"]:
        raise CaptureError("capture raw size does not match stride*height")
    return raw, meta


def capture_pixel(raw, meta, x, y):
    """The capture's pixel as R,G,B,A (ARGB8888 memory is B,G,R,A)."""
    base = y * meta["stride"] + x * 4
    blue, green, red, alpha = raw[base : base + 4]
    return (red, green, blue, alpha)


def readback_pixel(raw, meta, x, y):
    """The readback's pixel at TOP-origin row y, as R,G,B,A (the bytes are
    bottom row first)."""
    base = (meta["height"] - 1 - y) * meta["width"] * 4 + x * 4
    return tuple(raw[base : base + 4])


def compare_pixels(cap_raw, cap_meta, rb_raw, rb_meta):
    """Returns (pixels, differing, samples)."""
    pixels, differing, samples = 0, 0, []
    for y in range(cap_meta["height"]):
        for x in range(cap_meta["width"]):
            cap, rb = capture_pixel(cap_raw, cap_meta, x, y), readback_pixel(rb_raw, rb_meta, x, y)
            pixels += 1
            if cap != rb:
                differing += 1
                if len(samples) < MAX_REPORTED:
                    samples.append((x, y, cap, rb))
    return pixels, differing, samples


def compare(capture_dir, readback_raw_path, readback_meta_path):
    cap_raw, cap_meta = load_capture(capture_dir)
    rb_meta = parse_readback_meta(Path(readback_meta_path).read_text())
    rb_raw = Path(readback_raw_path).read_bytes()
    if (cap_meta["width"], cap_meta["height"]) != (rb_meta["width"], rb_meta["height"]):
        raise CaptureError(f"geometry differs: capture {cap_meta['width']}x{cap_meta['height']}, "
                           f"readback {rb_meta['width']}x{rb_meta['height']}")
    if len(rb_raw) != rb_meta["width"] * rb_meta["height"] * 4 or not rb_raw:
        raise CaptureError(f"readback raw has {len(rb_raw)} bytes, width*height*4 expected")
    return compare_pixels(cap_raw, cap_meta, rb_raw, rb_meta)


# -- selftest --------------------------------------------------------
# Expected values come from the SPECIFICATIONS (wl_shm ARGB8888 is B,G,R,A
# little-endian; glReadPixels RGBA is bottom row first), written by hand.
# The 2x2 image is asymmetric in BOTH axes and in every channel, so a
# missing flip or a missing R/B swap changes a byte (a symmetric image
# would pass both mistakes).

CHECKS = []
# top row: P(1,2,3,4) Q(5,6,7,8); bottom row: R(9,10,11,12) S(13,14,15,16), as R,G,B,A.
CAPTURE_BGRA = bytes([3, 2, 1, 4, 7, 6, 5, 8]) + bytes([0xEE] * 4) + bytes(
    [11, 10, 9, 12, 15, 14, 13, 16]) + bytes([0xEE] * 4)
CAPTURE_META = "width=2\nheight=2\nstride=12\nformat=0\n"
READBACK_RGBA = bytes([9, 10, 11, 12, 13, 14, 15, 16, 1, 2, 3, 4, 5, 6, 7, 8])
READBACK_META = "width=2\nheight=2\norigin=bottom_left\norder=rgba\n"


def check(name, condition):
    CHECKS.append(name)
    if not condition:
        print(f"selftest: {name} FALHOU", file=sys.stderr)
        sys.exit(1)


def rejects(function, *args):
    try:
        function(*args)
    except CaptureError:
        return True
    return False


def write_case(root, raw=CAPTURE_BGRA, meta=CAPTURE_META, readback=READBACK_RGBA,
               readback_meta=READBACK_META):
    root = Path(root)
    capture = root / "capture"
    capture.mkdir(exist_ok=True)
    (capture / "conn1_surface3.raw").write_bytes(raw)
    (capture / "conn1_surface3.meta").write_text(meta)
    (root / "rb.raw").write_bytes(readback)
    (root / "rb.meta").write_text(readback_meta)
    return capture, root / "rb.raw", root / "rb.meta"


def selftest_equality_and_difference():
    with tempfile.TemporaryDirectory() as tmp:
        result = compare(*write_case(tmp))
        check("quadro igual (flip e R/B mapeados, padding de linha ignorado): 4 pixels, 0 diferentes",
              result[0] == 4 and result[1] == 0)
    with tempfile.TemporaryDirectory() as tmp:
        altered = bytearray(READBACK_RGBA)
        altered[3] = 13  # alpha of the bottom-left readback pixel only
        pixels, differing, samples = compare(*write_case(tmp, readback=bytes(altered)))
        check("so o alfa diferente reprova (alfa incluido)",
              pixels == 4 and differing == 1 and samples[0][:2] == (0, 1))
    with tempfile.TemporaryDirectory() as tmp:
        unflipped = bytes([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16])
        check("readback sem o flip de linhas reprova (imagem assimetrica)",
              compare(*write_case(tmp, readback=unflipped))[1] == 4)
    with tempfile.TemporaryDirectory() as tmp:
        bgra = bytes([11, 10, 9, 12, 15, 14, 13, 16, 3, 2, 1, 4, 7, 6, 5, 8])
        check("readback com R e B trocados reprova",
              compare(*write_case(tmp, readback=bgra))[1] == 4)


def selftest_rejections():
    with tempfile.TemporaryDirectory() as tmp:
        case = write_case(tmp, meta="width=2\nheight=2\nstride=12\nformat=1\n")
        check("captura em XRGB8888 (formato 1) e recusada", rejects(compare, *case))
    with tempfile.TemporaryDirectory() as tmp:
        case = write_case(tmp, readback_meta="width=3\nheight=2\norigin=bottom_left\norder=rgba\n")
        check("geometria diferente e recusada", rejects(compare, *case))
    with tempfile.TemporaryDirectory() as tmp:
        case = write_case(tmp, readback_meta=READBACK_META.replace("bottom_left", "top_left"))
        check("origem do readback desconhecida e recusada, nunca adivinhada", rejects(compare, *case))
    with tempfile.TemporaryDirectory() as tmp:
        case = write_case(tmp, readback=READBACK_RGBA[:-4])
        check("readback com tamanho errado e recusado", rejects(compare, *case))
    with tempfile.TemporaryDirectory() as tmp:
        case = write_case(tmp, readback=b"", readback_meta="width=0\nheight=0\norigin=bottom_left\norder=rgba\n")
        check("readback vazio e recusado", rejects(compare, *case))


def selftest_capture_directory_controls():
    with tempfile.TemporaryDirectory() as tmp:
        (Path(tmp) / "capture").mkdir()
        (Path(tmp) / "rb.raw").write_bytes(READBACK_RGBA)
        (Path(tmp) / "rb.meta").write_text(READBACK_META)
        check("diretorio de captura vazio e recusado (L-40: zero quadros)",
              rejects(compare, Path(tmp) / "capture", Path(tmp) / "rb.raw", Path(tmp) / "rb.meta"))
    with tempfile.TemporaryDirectory() as tmp:
        case = write_case(tmp)
        (case[0] / "conn1_surface4.raw").write_bytes(CAPTURE_BGRA)
        (case[0] / "conn1_surface4.meta").write_text(CAPTURE_META)
        check("duas superficies na captura: ambiguo, recusado", rejects(compare, *case))
    check("diretorio inexistente e recusado", rejects(compare, "/nonexistent-capture-dir", "a", "b"))


def run_cli(*args):
    return subprocess.run([sys.executable, str(Path(__file__).resolve()), *args],
                          capture_output=True, text=True, check=False)


def selftest_real_process_exit_codes():
    with tempfile.TemporaryDirectory() as tmp:
        case = [str(part) for part in write_case(tmp)]
        proc = run_cli(*case)
        check("processo: igual sai 0 e imprime pixels=4 differing=0",
              proc.returncode == 0 and "pixels=4 differing=0" in proc.stdout)
    with tempfile.TemporaryDirectory() as tmp:
        altered = bytearray(READBACK_RGBA)
        altered[0] ^= 0xFF
        case = [str(part) for part in write_case(tmp, readback=bytes(altered))]
        proc = run_cli(*case)
        check("processo: um byte diferente sai 1 e imprime o resumo",
              proc.returncode == 1 and "pixels=4 differing=1" in proc.stdout)
    check("processo: sem argumentos sai 2", run_cli().returncode == 2)
    check("processo: captura inexistente sai 1", run_cli("/nonexistent", "a", "b").returncode == 1)


def selftest_main():
    selftest_equality_and_difference()
    selftest_rejections()
    selftest_capture_directory_controls()
    selftest_real_process_exit_codes()
    print(f"selftest: {len(CHECKS)} controles OK")


def real_main(args):
    """0 only when at least one pixel was compared and none differs."""
    if len(args) != 3:
        print(f"usage: {SCRIPT_NAME} <capture_dir> <readback.raw> <readback.meta>  |  --selftest",
              file=sys.stderr)
        return 2
    try:
        pixels, differing, samples = compare(*args)
    except (CaptureError, OSError, ValueError) as error:
        print(f"{SCRIPT_NAME}: {error}", file=sys.stderr)
        return 1
    print(f"{SCRIPT_NAME}: pixels={pixels} differing={differing}")
    for x, y, cap, rb in samples:
        print(f"{SCRIPT_NAME}: FAIL pixel ({x},{y}) capture RGBA={cap} readback RGBA={rb}", file=sys.stderr)
    if pixels == 0:
        print(f"{SCRIPT_NAME}: zero pixels compared: REJECTED (L-40)", file=sys.stderr)
        return 1
    return 1 if differing else 0


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
    else:
        sys.exit(real_main(args))


if __name__ == "__main__":
    main()
