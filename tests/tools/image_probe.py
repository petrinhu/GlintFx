#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# image_probe.py - QA-SCREEN-CAPTURE P2 (D-W8-20, D-W8-33): pixel probes
# over a PNG, the second half of the capture reader (the first is
# tests/tools/raw_to_png.py). Test tooling, not product code. Standard
# library only (L-07): the PNG is decoded by hand with zlib + struct.
#
# PROBE FILE, one probe per line, '#' starts a comment:
#   <x> <y> <RRGGBBAA> [<tolerance>]
# x, y decimal, origin at the top-left corner of the image; RRGGBBAA hex,
# alpha included (the alpha probe is what rejects a half-transparent
# frame); tolerance is the largest allowed per-channel absolute
# difference, default 0.
#
# DECODER scope: non-interlaced, 8 bits per channel, color type 2 (RGB,
# alpha taken as 255) or 6 (RGBA), filters 0..4, any number of IDAT
# chunks. Anything else is rejected, never guessed.
#
# VERDICT (L-40): zero probes is a rejection, an out-of-range probe is a
# failed probe, and the summary line is always printed.
#
# USAGE: image_probe.py <image.png> <probes.txt>   |   image_probe.py --selftest

import struct
import sys
import tempfile
import zlib
from pathlib import Path

SCRIPT_NAME = "image_probe.py"
# The \r\n inside the PNG signature is a byte of the format, not an
# environment fact (checkout line ending): GODS_LAWS.md L-40 declaration.
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
CHANNELS_BY_COLOR_TYPE = {2: 3, 6: 4}


class ProbeError(Exception):
    pass


def split_chunks(png):
    if png[:8] != PNG_SIGNATURE:
        raise ProbeError("not a PNG: bad signature")
    chunks, pos = [], 8
    while pos < len(png):
        if pos + 12 > len(png):
            raise ProbeError("truncated chunk header")
        (length,) = struct.unpack(">I", png[pos : pos + 4])
        chunk_type = png[pos + 4 : pos + 8]
        data = png[pos + 8 : pos + 8 + length]
        crc = png[pos + 8 + length : pos + 12 + length]
        if len(data) != length or len(crc) != 4:
            raise ProbeError(f"truncated chunk {chunk_type!r}")
        if struct.unpack(">I", crc)[0] != zlib.crc32(chunk_type + data) & 0xFFFFFFFF:
            raise ProbeError(f"bad CRC in chunk {chunk_type!r}")
        chunks.append((chunk_type, data))
        pos += 12 + length
    return chunks


def paeth(left, up, up_left):
    estimate = left + up - up_left
    d_left, d_up, d_up_left = abs(estimate - left), abs(estimate - up), abs(estimate - up_left)
    if d_left <= d_up and d_left <= d_up_left:
        return left
    return up if d_up <= d_up_left else up_left


def unfilter_scanline(filter_type, line, previous, bpp):
    out = bytearray(len(line))
    for i, value in enumerate(line):
        left = out[i - bpp] if i >= bpp else 0
        up = previous[i]
        up_left = previous[i - bpp] if i >= bpp else 0
        if filter_type == 0:
            predictor = 0
        elif filter_type == 1:
            predictor = left
        elif filter_type == 2:
            predictor = up
        elif filter_type == 3:
            predictor = (left + up) // 2
        elif filter_type == 4:
            predictor = paeth(left, up, up_left)
        else:
            raise ProbeError(f"unknown filter type {filter_type}")
        out[i] = (value + predictor) & 0xFF
    return bytes(out)


def decode_png(png):
    """Returns (width, height, rows) with rows = list of width*4 RGBA bytes."""
    chunks = split_chunks(png)
    if not chunks or chunks[0][0] != b"IHDR" or chunks[-1][0] != b"IEND":
        raise ProbeError("chunk order: IHDR first and IEND last are required")
    width, height, depth, color_type, compression, filtering, interlace = struct.unpack(
        ">IIBBBBB", chunks[0][1]
    )
    if depth != 8 or color_type not in CHANNELS_BY_COLOR_TYPE or interlace != 0:
        raise ProbeError(f"unsupported PNG: depth={depth} color={color_type} interlace={interlace}")
    if compression != 0 or filtering != 0 or width < 1 or height < 1:
        raise ProbeError("unsupported or empty PNG header")
    channels = CHANNELS_BY_COLOR_TYPE[color_type]
    data = zlib.decompress(b"".join(d for t, d in chunks if t == b"IDAT"))
    line_length = width * channels
    if len(data) != height * (1 + line_length):
        raise ProbeError("decompressed size does not match the header")
    rows, previous = [], bytes(line_length)
    for y in range(height):
        base = y * (1 + line_length)
        line = unfilter_scanline(data[base], data[base + 1 : base + 1 + line_length], previous, channels)
        previous = line
        if channels == 4:
            rows.append(line)
        else:
            rows.append(b"".join(line[i : i + 3] + b"\xff" for i in range(0, len(line), 3)))
    return width, height, rows


def parse_probes(text):
    """Returns a list of (line_number, x, y, (r, g, b, a), tolerance)."""
    probes = []
    for number, line in enumerate(text.splitlines(), start=1):
        body = line.split("#", 1)[0].strip()
        if not body:
            continue
        fields = body.split()
        if len(fields) not in (3, 4) or not (fields[0].isdigit() and fields[1].isdigit()):
            raise ProbeError(f"line {number}: expected '<x> <y> <RRGGBBAA> [<tolerance>]'")
        color_text = fields[2]
        if len(color_text) != 8 or any(c not in "0123456789abcdefABCDEF" for c in color_text):
            raise ProbeError(f"line {number}: color must be 8 hex digits RRGGBBAA")
        tolerance = 0
        if len(fields) == 4:
            if not fields[3].isdigit():
                raise ProbeError(f"line {number}: tolerance must be a decimal integer")
            tolerance = int(fields[3])
        color = tuple(int(color_text[i : i + 2], 16) for i in range(0, 8, 2))
        probes.append((number, int(fields[0]), int(fields[1]), color, tolerance))
    return probes


def run_probes(width, height, rows, probes):
    """Returns the list of failure messages (empty = all probes passed)."""
    failures = []
    for number, x, y, expected, tolerance in probes:
        if x >= width or y >= height:
            failures.append(f"probe line {number}: ({x},{y}) is outside {width}x{height}")
            continue
        actual = tuple(rows[y][x * 4 : x * 4 + 4])
        if any(abs(a - e) > tolerance for a, e in zip(actual, expected)):
            failures.append(
                f"probe line {number}: ({x},{y}) expected {expected} (tolerance {tolerance}) got {actual}"
            )
    return failures


# -- selftest --------------------------------------------------------
# PNGs here are assembled by hand from the PNG specification (forward
# filters from the spec formulas), independent of the decoder above.


def build_png(width, height, color_type, scanlines):
    def chunk(kind, data):
        return (
            struct.pack(">I", len(data))
            + kind
            + data
            + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
        )

    header = struct.pack(">IIBBBBB", width, height, 8, color_type, 0, 0, 0)
    return (
        PNG_SIGNATURE
        + chunk(b"IHDR", header)
        + chunk(b"IDAT", zlib.compress(b"".join(scanlines)))
        + chunk(b"IEND", b"")
    )


def forward_filter(filter_type, line, previous, bpp):
    """The encoder direction of each PNG filter, straight from the spec."""
    out = bytearray()
    for i, value in enumerate(line):
        left = line[i - bpp] if i >= bpp else 0
        up = previous[i]
        up_left = previous[i - bpp] if i >= bpp else 0
        if filter_type == 0:
            predictor = 0
        elif filter_type == 1:
            predictor = left
        elif filter_type == 2:
            predictor = up
        elif filter_type == 3:
            predictor = (left + up) // 2
        else:
            estimate = left + up - up_left
            candidates = [(abs(estimate - left), 0, left), (abs(estimate - up), 1, up),
                          (abs(estimate - up_left), 2, up_left)]
            predictor = min(candidates)[2]
        out.append((value - predictor) & 0xFF)
    return bytes([filter_type]) + bytes(out)


def selftest_main():
    checks = []

    def check(name, condition):
        checks.append(name)
        if not condition:
            print(f"selftest: {name} FALHOU", file=sys.stderr)
            sys.exit(1)

    def rejects(function, *args):
        try:
            function(*args)
        except ProbeError:
            return True
        return False

    # A 3x5 RGBA image, one row per filter type (0..4) plus a second Paeth
    # row, with values chosen so that left/up/up-left all differ.
    pixels = [
        bytes([10, 200, 30, 255, 40, 50, 250, 128, 7, 8, 9, 10]),
        bytes([11, 190, 35, 255, 90, 60, 240, 100, 1, 2, 3, 4]),
        bytes([250, 180, 40, 0, 80, 70, 230, 90, 200, 201, 202, 203]),
        bytes([12, 170, 45, 255, 70, 80, 220, 80, 100, 99, 98, 97]),
        bytes([13, 160, 50, 255, 60, 90, 210, 70, 5, 250, 5, 250]),
    ]
    lines, previous = [], bytes(12)
    for index, row in enumerate(pixels):
        lines.append(forward_filter(min(index, 4), row, previous, 4))
        previous = row
    png = build_png(3, 5, 6, lines)
    width, height, rows = decode_png(png)
    check("decodifica RGBA com os filtros 0 a 4", (width, height, rows) == (3, 5, pixels))

    # Paeth tie: left=0, up=30, up-left=10 gives distances 20, 10, 10, and
    # the spec says a tie between up and up-left picks UP (order: left,
    # up, up-left). Same value in all four channels of each pixel.
    tie_top = bytes([10] * 4 + [30] * 4)
    tie_bottom = bytes([0] * 4 + [77] * 4)
    tie_png = build_png(2, 2, 6, [bytes([0]) + tie_top, forward_filter(4, tie_bottom, tie_top, 4)])
    check("Paeth: empate entre acima e acima-esquerda escolhe acima",
          decode_png(tie_png) == (2, 2, [tie_top, tie_bottom]))

    rgb = build_png(1, 1, 2, [bytes([0, 1, 2, 3])])
    check("RGB (tipo 2) vira RGBA opaco", decode_png(rgb) == (1, 1, [bytes([1, 2, 3, 255])]))
    check("assinatura errada reprova", rejects(decode_png, b"x" + png[1:]))
    check("CRC corrompido reprova", rejects(decode_png, png[:-1] + bytes([png[-1] ^ 1])))
    check("PNG truncado reprova", rejects(decode_png, png[:-20]))
    check("filtro desconhecido reprova", rejects(decode_png, build_png(1, 1, 6, [bytes([9, 0, 0, 0, 0])])))
    check("tamanho descomprimido errado reprova", rejects(decode_png, build_png(1, 1, 6, [bytes([0, 0, 0])])))

    probes = parse_probes("# comment\n0 0 0AC81EFF\n1 0 2832FA80 0\n2 0 0708090A 2\n")
    check("parse: tres sondas, comentario ignorado", len(probes) == 3 and probes[2][4] == 2)
    check("sondas que batem passam", run_probes(width, height, rows, probes) == [])

    def failures_for(text):
        return run_probes(width, height, rows, parse_probes(text))

    check("cor trocada reprova", len(failures_for("0 0 C8100AFF\n")) == 1)
    check("alfa 0,5 no lugar de 1,0 reprova", len(failures_for("0 0 0AC81E80\n")) == 1)
    check("um nivel de diferenca reprova com tolerancia 0", len(failures_for("0 0 0BC81EFF\n")) == 1)
    check("um nivel de diferenca passa com tolerancia 1", failures_for("0 0 0BC81EFF 1\n") == [])
    check("sonda fora da imagem reprova", len(failures_for("3 0 0AC81EFF\n")) == 1
          and len(failures_for("0 5 0AC81EFF\n")) == 1)
    check("sonda mal formada reprova", rejects(parse_probes, "0 0 0AC81E\n")
          and rejects(parse_probes, "0 0 0AC81EGG\n") and rejects(parse_probes, "a b 0AC81EFF\n")
          and rejects(parse_probes, "0 0 0AC81EFF -1\n"))

    # Entry point: the three L-40 controls.
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        (root / "image.png").write_bytes(png)
        (root / "good.txt").write_text("0 0 0AC81EFF\n1 1 5A3CF064\n")
        (root / "bad.txt").write_text("0 0 C8100AFF\n")
        (root / "empty.txt").write_text("# only a comment\n\n")
        check("main: sondas boas saem 0", run_quiet(root / "image.png", root / "good.txt") == 0)
        check("main: sonda ruim sai 1", run_quiet(root / "image.png", root / "bad.txt") == 1)
        check("main: zero sondas sai 1", run_quiet(root / "image.png", root / "empty.txt") == 1)
        check("main: PNG inexistente sai 1", run_quiet(root / "nope.png", root / "good.txt") == 1)
    print(f"selftest: {len(checks)} controles OK")


def run_quiet(png_path, probes_path):
    import contextlib
    import io

    with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
        return real_main([str(png_path), str(probes_path)])


def real_main(args):
    """Returns the exit code: 0 only with at least one probe, all passed."""
    if len(args) != 2:
        print(f"usage: {SCRIPT_NAME} <image.png> <probes.txt>  |  --selftest", file=sys.stderr)
        return 2
    try:
        width, height, rows = decode_png(Path(args[0]).read_bytes())
        probes = parse_probes(Path(args[1]).read_text())
    except (ProbeError, OSError, zlib.error) as error:
        print(f"{SCRIPT_NAME}: {error}", file=sys.stderr)
        return 1
    failures = run_probes(width, height, rows, probes)
    for failure in failures:
        print(f"{SCRIPT_NAME}: FAIL {failure}", file=sys.stderr)
    print(f"{SCRIPT_NAME}: probes={len(probes)} failed={len(failures)}")
    if not probes:
        print(f"{SCRIPT_NAME}: zero probes: REJECTED (L-40)", file=sys.stderr)
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
