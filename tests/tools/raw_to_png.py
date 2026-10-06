#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# raw_to_png.py - QA-SCREEN-CAPTURE P2 (D-W8-20, D-W8-32, D-W8-33): the
# READER of the wire relay's frame captures. Test tooling, not product
# code. Standard library only (L-07): the PNG is written by hand with
# zlib + struct.
#
# INPUT contract (writer: tests/container/wire_relay/wire_frame_writer.hpp):
#   conn<N>_surface<S>.raw   stride * height bytes, no header
#   conn<N>_surface<S>.meta  "width=", "height=", "stride=", "format=" lines
#   conn<N>_no_frame.txt     "nenhum quadro" (connection never committed)
# format is the raw wl_shm.format value: 0 = ARGB8888, 1 = XRGB8888, both
# little-endian words, so the bytes in memory are B, G, R, A (X for 1).
#
# VERDICT (CAPTURE-ZERO-FRAME-VERDICT, plan v3 D-W8-32 / closure 5(i)):
# the relay does not fail on "nenhum quadro"; THIS reader does. Zero
# images converted is a rejection (L-40), and so is any file it could
# not account for (orphan .raw/.meta, unknown name, bad geometry).
#
# ALPHA (read this before writing a probe): wl_shm ARGB8888 carries
# PREMULTIPLIED alpha (the compositor blends it that way). This tool does
# NOT un-premultiply: color channels are copied byte for byte and the
# PNG's alpha channel is marked straight, so an ordinary viewer shows a
# semi-transparent pixel DARKER than the client meant. For fully opaque
# or fully transparent pixels nothing changes. D-W8-33 compares this
# image against the client's own readback, byte for byte, so both sides
# must be read in the same convention; P3 states the expected value of
# every semi-transparent probe in that convention, never "straight".
#
# USAGE: raw_to_png.py <capture_dir> <out_dir>   |   raw_to_png.py --selftest
# Prints one "png <name> md5=<hex> <w>x<h>" line per image and a final
# "raw_to_png: images=<n> no_frame=<n> failed=<n>" line, always.

import hashlib
import re
import struct
import subprocess
import sys
import tempfile
import zlib
from collections import namedtuple
from pathlib import Path

SCRIPT_NAME = "raw_to_png.py"
PNG_SIGNATURE = bytes([137, 80, 78, 71, 13, 10, 26, 10])
FORMAT_ARGB8888 = 0
FORMAT_XRGB8888 = 1
META_KEYS = ("width", "height", "stride", "format")
RAW_RE = re.compile(r"^conn(\d+)_surface(\d+)\.raw$")
META_RE = re.compile(r"^conn(\d+)_surface(\d+)\.meta$")
NO_FRAME_RE = re.compile(r"^conn(\d+)_no_frame\.txt$")
NO_FRAME_TEXT = "nenhum quadro"


class CaptureError(Exception):
    pass


def parse_meta(text):
    """Parses the .meta text into {key: int}; every key exactly once,
    nothing else, all decimal integers."""
    meta = {}
    for line in text.splitlines():
        if not line.strip():
            continue
        key, separator, value = line.partition("=")
        if not separator or key not in META_KEYS or key in meta:
            raise CaptureError(f"meta line not understood or repeated: {line!r}")
        if not (value.isascii() and value.isdigit()):
            raise CaptureError(f"meta value is not a decimal integer: {line!r}")
        meta[key] = int(value)
    missing = [key for key in META_KEYS if key not in meta]
    if missing:
        raise CaptureError(f"meta lacks keys: {missing}")
    return meta


def raw_to_rgba_rows(raw, meta):
    """One bytes object of width*4 RGBA bytes per row. Row padding
    (stride > width*4) is dropped. ARGB8888 memory is B,G,R,A."""
    width, height, stride, fmt = (meta[key] for key in META_KEYS)
    if width < 1 or height < 1:
        raise CaptureError(f"empty geometry {width}x{height}")
    if fmt not in (FORMAT_ARGB8888, FORMAT_XRGB8888):
        raise CaptureError(f"unsupported wl_shm format {fmt}")
    if stride < width * 4:
        raise CaptureError(f"stride {stride} < width*4 = {width * 4}")
    if len(raw) != stride * height:
        raise CaptureError(f"raw has {len(raw)} bytes, stride*height = {stride * height}")
    rows = []
    for y in range(height):
        row = bytearray(width * 4)
        base = y * stride
        for x in range(width):
            blue, green, red, alpha = raw[base + x * 4 : base + x * 4 + 4]
            row[x * 4 : x * 4 + 4] = bytes(
                [red, green, blue, alpha if fmt == FORMAT_ARGB8888 else 255]
            )
        rows.append(bytes(row))
    return rows


def png_chunk(chunk_type, data):
    crc = zlib.crc32(chunk_type + data) & 0xFFFFFFFF
    return struct.pack(">I", len(data)) + chunk_type + data + struct.pack(">I", crc)


def encode_png(width, height, rgba_rows):
    """8-bit RGBA, non-interlaced PNG; every scanline uses filter 0."""
    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    scanlines = b"".join(b"\x00" + row for row in rgba_rows)
    return (
        PNG_SIGNATURE
        + png_chunk(b"IHDR", header)
        + png_chunk(b"IDAT", zlib.compress(scanlines))
        + png_chunk(b"IEND", b"")
    )


def convert_directory(capture_dir, out_dir):
    """Returns (images, no_frame, failures); images is a list of
    (png_name, md5, width, height); failures is a list of messages."""
    capture_dir = Path(capture_dir)
    if not capture_dir.is_dir():
        raise CaptureError(f"capture directory does not exist: {capture_dir}")
    out_dir = Path(out_dir)
    names = sorted(entry.name for entry in capture_dir.iterdir() if entry.is_file())
    images, failures, no_frame = [], [], 0
    for name in names:
        raw_match = RAW_RE.match(name)
        if raw_match:
            stem = name[: -len(".raw")]
            if f"{stem}.meta" not in names:
                failures.append(f"{name}: no .meta beside it")
                continue
            try:
                images.append(convert_one(capture_dir, out_dir, stem))
            except (CaptureError, OSError, ValueError) as error:
                failures.append(f"{name}: {error}")
        elif META_RE.match(name):
            if f"{name[: -len('.meta')]}.raw" not in names:
                failures.append(f"{name}: no .raw beside it")
        elif NO_FRAME_RE.match(name):
            text = read_marker_text(capture_dir / name)
            if text == NO_FRAME_TEXT:
                no_frame += 1
            else:
                failures.append(f"{name}: unexpected text {text!r}")
        else:
            failures.append(f"{name}: not a capture file")
    return images, no_frame, failures


def read_marker_text(path):
    try:
        return path.read_text().strip()
    except (OSError, ValueError) as error:
        return f"<unreadable: {error}>"


def convert_one(capture_dir, out_dir, stem):
    meta = parse_meta((capture_dir / f"{stem}.meta").read_text())
    rows = raw_to_rgba_rows((capture_dir / f"{stem}.raw").read_bytes(), meta)
    png = encode_png(meta["width"], meta["height"], rows)
    out_dir.mkdir(parents=True, exist_ok=True)
    png_name = f"{stem}.png"
    (out_dir / png_name).write_bytes(png)
    return png_name, hashlib.md5(png).hexdigest(), meta["width"], meta["height"]


# -- selftest --------------------------------------------------------
# Expected values below come from the SPECIFICATIONS (PNG: RFC 2083 /
# W3C PNG; wl_shm ARGB8888 little-endian), written by hand, never read
# back from this program's output. The CLI controls run the real process
# (subprocess) and read its real exit code.

CHECKS = []
CaptureSpec = namedtuple("CaptureSpec", "width height stride fmt")
# 2x2 ARGB8888, stride 12 (4 bytes of padding per row), little-endian
# memory B,G,R,A; expected RGBA rows: (1,2,3,4)(5,6,7,8) / (9..12)(13..16).
PAD = bytes([0xEE] * 4)
RAW_2X2 = bytes([3, 2, 1, 4, 7, 6, 5, 8]) + PAD + bytes([11, 10, 9, 12, 15, 14, 13, 16]) + PAD
SPEC_2X2 = CaptureSpec(2, 2, 12, 0)
ROWS_2X2 = [bytes([1, 2, 3, 4, 5, 6, 7, 8]), bytes([9, 10, 11, 12, 13, 14, 15, 16])]


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


def chunk_walk(png):
    """Independent minimal PNG chunk splitter: yields (type, data, crc)."""
    pos = len(PNG_SIGNATURE)
    while pos < len(png):
        (length,) = struct.unpack(">I", png[pos : pos + 4])
        yield png[pos + 4 : pos + 8], png[pos + 8 : pos + 8 + length], png[
            pos + 8 + length : pos + 12 + length
        ]
        pos += 12 + length


def write_capture(directory, name, pixels, spec):
    (directory / f"{name}.raw").write_bytes(pixels)
    (directory / f"{name}.meta").write_text(
        f"width={spec.width}\nheight={spec.height}\nstride={spec.stride}\nformat={spec.fmt}\n"
    )


def selftest_png_encoding():
    png = encode_png(1, 1, [bytes([0x11, 0x22, 0x33, 0x80])])
    check("assinatura PNG", png[:8] == bytes([137, 80, 78, 71, 13, 10, 26, 10]))
    chunks = list(chunk_walk(png))
    check("ordem IHDR, IDAT, IEND", [c[0] for c in chunks] == [b"IHDR", b"IDAT", b"IEND"])
    check(
        "IHDR 1x1, 8 bits, tipo de cor 6 (RGBA)",
        chunks[0][1] == bytes([0, 0, 0, 1, 0, 0, 0, 1, 8, 6, 0, 0, 0]),
    )
    # CRC-32 constants fixed by the format: IHDR of a 1x1 RGBA image and
    # the empty IEND chunk (both are the well-known literals).
    check("CRC do IHDR 1x1 RGBA", chunks[0][2] == bytes([0x1F, 0x15, 0xC4, 0x89]))
    check("CRC do IEND", chunks[2][2] == bytes([0xAE, 0x42, 0x60, 0x82]))
    check("IEND vazio", chunks[2][1] == b"")
    check(
        "IDAT: filtro 0 mais RGBA, em zlib",
        zlib.decompress(chunks[1][1]) == bytes([0, 0x11, 0x22, 0x33, 0x80]),
    )


def selftest_channel_conversion():
    rows = raw_to_rgba_rows(RAW_2X2, parse_meta("width=2\nheight=2\nstride=12\nformat=0\n"))
    check("ARGB8888 little-endian vira RGBA, sem o enchimento do stride", rows == ROWS_2X2)
    xrows = raw_to_rgba_rows(RAW_2X2, dict(SPEC_2X2._asdict(), format=1))
    check(
        "XRGB8888: o byte X nunca vira transparencia (alfa 255)",
        xrows == [bytes([1, 2, 3, 255, 5, 6, 7, 255]), bytes([9, 10, 11, 255, 13, 14, 15, 255])],
    )


def selftest_meta_rejections():
    good = parse_meta("width=2\nheight=2\nstride=12\nformat=0\n")
    check("meta lida", good == {"width": 2, "height": 2, "stride": 12, "format": 0})
    check("meta sem chave reprova", rejects(parse_meta, "width=2\nheight=2\nstride=8\n"))
    check(
        "meta com chave repetida ou lixo reprova",
        rejects(parse_meta, "width=2\nwidth=2\nheight=2\nstride=8\nformat=0\n")
        and rejects(parse_meta, "width=dois\nheight=2\nstride=8\nformat=0\n"),
    )
    check(
        "digito nao ASCII (expoente) reprova como CaptureError, nao como traceback",
        rejects(parse_meta, "width=\u00b2\nheight=2\nstride=8\nformat=0\n"),
    )


def selftest_geometry_rejections():
    meta = dict(SPEC_2X2._asdict(), format=0)
    check(
        "stride menor que width*4 reprova",
        rejects(raw_to_rgba_rows, RAW_2X2, dict(meta, stride=4))
        and rejects(raw_to_rgba_rows, bytes(14), dict(meta, stride=7)),
    )
    check("raw menor que stride*height reprova", rejects(raw_to_rgba_rows, RAW_2X2[:-1], meta))
    check("raw MAIOR que stride*height reprova", rejects(raw_to_rgba_rows, RAW_2X2 + b"\x00", meta))
    check("formato desconhecido reprova", rejects(raw_to_rgba_rows, RAW_2X2, dict(meta, format=7)))
    check("largura zero reprova", rejects(raw_to_rgba_rows, b"", dict(meta, width=0, stride=0)))


def selftest_directory_controls():
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        good = root / "good"
        good.mkdir()
        write_capture(good, "conn1_surface3", RAW_2X2, SPEC_2X2)
        images, no_frame, failures = convert_directory(good, root / "out_good")
        check("BOM: uma imagem, zero falhas", len(images) == 1 and no_frame == 0 and failures == [])
        written = (root / "out_good" / images[0][0]).read_bytes()
        check("BOM: md5 informado e o do arquivo gravado", images[0][1] == hashlib.md5(written).hexdigest())
        check("BOM: dimensoes e PNG gravado", (images[0][2], images[0][3]) == (2, 2)
              and written == encode_png(2, 2, ROWS_2X2))
        nf = root / "only_no_frame"
        nf.mkdir()
        (nf / "conn1_no_frame.txt").write_text(NO_FRAME_TEXT)
        images, no_frame, failures = convert_directory(nf, root / "out_nf")
        check("RUIM: so 'nenhum quadro' conta o marcador e devolve zero imagens",
              images == [] and no_frame == 1 and failures == [])
        empty = root / "empty"
        empty.mkdir()
        images, no_frame, _ = convert_directory(empty, root / "out_empty")
        check("VAZIO: zero imagens e zero marcadores", images == [] and no_frame == 0)
        check("diretorio inexistente reprova", rejects(convert_directory, root / "nope", root / "o"))


def build_orphan_dir(root):
    orphan = root / "orphan"
    orphan.mkdir()
    write_capture(orphan, "conn1_surface3", RAW_2X2, SPEC_2X2)
    (orphan / "conn2_surface9.raw").write_bytes(RAW_2X2)
    (orphan / "conn1_surface4.meta").write_text("width=1\nheight=1\nstride=4\nformat=0\n")
    (orphan / "stranger.bin").write_bytes(b"x")
    (orphan / "conn3_no_frame.txt").write_text("outro texto")
    return orphan


def selftest_orphans():
    with tempfile.TemporaryDirectory() as tmp:
        images, _, failures = convert_directory(build_orphan_dir(Path(tmp)), Path(tmp) / "out")
        check("falha do raw sem meta diz isso",
              any("conn2_surface9.raw: no .meta" in f for f in failures))
        check("orfaos, nome desconhecido e marcador com texto errado viram 4 falhas",
              len(images) == 1 and len(failures) == 4)


def selftest_undecodable_meta():
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        write_capture(root, "conn1_surface3", RAW_2X2, SPEC_2X2)
        (root / "conn1_surface3.meta").write_bytes(b"\xff\xfe width=2\n")
        images, _, failures = convert_directory(root, root / "out")
        check("meta que nao e UTF-8 vira falha contada, sem traceback",
              images == [] and len(failures) == 1 and "conn1_surface3.raw" in failures[0])


def run_cli(*args):
    return subprocess.run([sys.executable, str(Path(__file__).resolve()), *args],
                          capture_output=True, text=True, check=False)


def selftest_real_process_exit_codes():
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        good = root / "good"
        good.mkdir()
        write_capture(good, "conn1_surface3", RAW_2X2, SPEC_2X2)
        nf = root / "nf"
        nf.mkdir()
        (nf / "conn1_no_frame.txt").write_text(NO_FRAME_TEXT)
        empty = root / "empty"
        empty.mkdir()
        proc = run_cli(str(good), str(root / "o1"))
        check("processo: bom sai 0 e imprime png com md5",
              proc.returncode == 0 and "images=1" in proc.stdout and "md5=" in proc.stdout)
        check("processo: so marcador sai 1", run_cli(str(nf), str(root / "o2")).returncode == 1)
        check("processo: vazio sai 1", run_cli(str(empty), str(root / "o3")).returncode == 1)
        check("processo: orfao sai 1",
              run_cli(str(build_orphan_dir(root)), str(root / "o4")).returncode == 1)
        check("processo: diretorio inexistente sai 1", run_cli(str(root / "x"), str(root / "o5")).returncode == 1)
        check("processo: sem argumentos sai 2", run_cli().returncode == 2)


def selftest_main():
    selftest_png_encoding()
    selftest_channel_conversion()
    selftest_meta_rejections()
    selftest_geometry_rejections()
    selftest_directory_controls()
    selftest_orphans()
    selftest_undecodable_meta()
    selftest_real_process_exit_codes()
    print(f"selftest: {len(CHECKS)} controles OK")


def real_main(args):
    """Returns the process exit code: 0 only with at least one image and
    no failure."""
    if len(args) != 2:
        print(f"usage: {SCRIPT_NAME} <capture_dir> <out_dir>  |  --selftest", file=sys.stderr)
        return 2
    try:
        images, no_frame, failures = convert_directory(args[0], args[1])
    except (CaptureError, OSError) as error:
        print(f"{SCRIPT_NAME}: {error}", file=sys.stderr)
        return 1
    for png_name, digest, width, height in images:
        print(f"png {png_name} md5={digest} {width}x{height}")
    for failure in failures:
        print(f"{SCRIPT_NAME}: FAIL {failure}", file=sys.stderr)
    print(f"{SCRIPT_NAME}: images={len(images)} no_frame={no_frame} failed={len(failures)}")
    if not images:
        print(f"{SCRIPT_NAME}: zero images converted: REJECTED (L-40)", file=sys.stderr)
        return 1
    return 1 if failures else 0


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
    else:
        sys.exit(real_main(args))


if __name__ == "__main__":
    main()
