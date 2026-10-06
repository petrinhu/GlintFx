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
import subprocess
import sys
import tempfile
import zlib
from pathlib import Path

SCRIPT_NAME = "image_probe.py"
PNG_SIGNATURE = bytes([137, 80, 78, 71, 13, 10, 26, 10])
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


def is_decimal(text):
    return text.isascii() and text.isdigit()


def parse_probes(text):
    """Returns a list of (line_number, x, y, (r, g, b, a), tolerance)."""
    probes = []
    for number, line in enumerate(text.splitlines(), start=1):
        body = line.split("#", 1)[0].strip()
        if not body:
            continue
        fields = body.split()
        if len(fields) not in (3, 4) or not (is_decimal(fields[0]) and is_decimal(fields[1])):
            raise ProbeError(f"line {number}: expected '<x> <y> <RRGGBBAA> [<tolerance>]'")
        color_text = fields[2]
        if len(color_text) != 8 or any(c not in "0123456789abcdefABCDEF" for c in color_text):
            raise ProbeError(f"line {number}: color must be 8 hex digits RRGGBBAA")
        tolerance = 0
        if len(fields) == 4:
            if not is_decimal(fields[3]):
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
# filters from the spec formulas), independent of the decoder above. The
# CLI controls run the real process and read its real exit code.

CHECKS = []


def check(name, condition):
    CHECKS.append(name)
    if not condition:
        print(f"selftest: {name} FALHOU", file=sys.stderr)
        sys.exit(1)


def rejects(function, *args):
    try:
        function(*args)
    except ProbeError:
        return True
    return False


def png_chunk(kind, data):
    crc = struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
    return struct.pack(">I", len(data)) + kind + data + crc


def build_png(width, height, color_type, scanlines):
    header = struct.pack(">IIBBBBB", width, height, 8, color_type, 0, 0, 0)
    body = PNG_SIGNATURE + png_chunk(b"IHDR", header)
    body += png_chunk(b"IDAT", zlib.compress(b"".join(scanlines)))
    return body + png_chunk(b"IEND", b"")


def forward_filter(filter_type, line, previous, bpp):
    """The encoder direction of each PNG filter, straight from the spec;
    Paeth ties resolve in the order left, up, up-left."""
    out = bytearray()
    for i, value in enumerate(line):
        left = line[i - bpp] if i >= bpp else 0
        up = previous[i]
        up_left = previous[i - bpp] if i >= bpp else 0
        if filter_type == 4:
            estimate = left + up - up_left
            candidates = [(abs(estimate - left), 0, left), (abs(estimate - up), 1, up),
                          (abs(estimate - up_left), 2, up_left)]
            predictor = min(candidates)[2]
        else:
            predictor = (0, left, up, (left + up) // 2)[filter_type]
        out.append((value - predictor) & 0xFF)
    return bytes([filter_type]) + bytes(out)


def paeth_tie_png(top, bottom):
    """2x2 image whose second row uses filter 4 over `top`."""
    return build_png(2, 2, 6, [bytes([0]) + top, forward_filter(4, bottom, top, 4)])


def selftest_filters():
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
    check("decodifica RGBA com os filtros 0 a 4", decode_png(build_png(3, 5, 6, lines)) == (3, 5, pixels))
    rgb = build_png(1, 1, 2, [bytes([0, 1, 2, 3])])
    check("RGB (tipo 2) vira RGBA opaco", decode_png(rgb) == (1, 1, [bytes([1, 2, 3, 255])]))


def selftest_paeth_ties():
    # up vs up-left tie: left=0, up=30, up-left=10 -> distances 20, 10, 10, UP wins.
    top, bottom = bytes([10] * 4 + [30] * 4), bytes([0] * 4 + [77] * 4)
    check("Paeth: empate acima x acima-esquerda escolhe acima",
          decode_png(paeth_tie_png(top, bottom)) == (2, 2, [top, bottom]))
    # left vs up-left tie: left=30, up=0, up-left=10 -> distances 10, 20, 10, LEFT wins.
    top, bottom = bytes([10] * 4 + [0] * 4), bytes([30] * 4 + [77] * 4)
    check("Paeth: empate esquerda x acima-esquerda escolhe esquerda",
          decode_png(paeth_tie_png(top, bottom)) == (2, 2, [top, bottom]))


def selftest_png_rejections():
    png = build_png(1, 1, 6, [bytes([0, 1, 2, 3, 4])])
    check("assinatura errada reprova", rejects(decode_png, b"x" + png[1:]))
    check("CRC corrompido reprova", rejects(decode_png, png[:-1] + bytes([png[-1] ^ 1])))
    check("PNG cortado no meio de um bloco reprova", rejects(decode_png, png[:-20]))
    check("PNG sem IEND, com blocos intactos, reprova",
          rejects(decode_png, png[:-12]))  # IEND is exactly 12 bytes: length, type, CRC
    check("filtro desconhecido reprova", rejects(decode_png, build_png(1, 1, 6, [bytes([9, 0, 0, 0, 0])])))
    check("tamanho descomprimido errado reprova", rejects(decode_png, build_png(1, 1, 6, [bytes([0, 0, 0])])))


def probe_failures(text):
    top = bytes([10, 200, 30, 255, 40, 50, 250, 128, 7, 8, 9, 10])
    rows = [top, bytes([11, 190, 35, 255, 90, 60, 240, 100, 1, 2, 3, 4])]
    return run_probes(3, 2, rows, parse_probes(text))


def selftest_probes():
    probes = parse_probes("# comment\n0 0 0AC81EFF\n1 0 2832FA80 0\n2 0 0708090A 2\n")
    check("parse: tres sondas, comentario ignorado", len(probes) == 3 and probes[2][4] == 2)
    check("sondas que batem passam", probe_failures("0 0 0AC81EFF\n1 0 2832FA80\n1 1 5A3CF064\n") == [])
    check("cor trocada reprova", len(probe_failures("0 0 C8100AFF\n")) == 1)
    check("alfa 0,5 no lugar de 1,0 reprova", len(probe_failures("0 0 0AC81E80\n")) == 1)
    check("um nivel de diferenca reprova com tolerancia 0", len(probe_failures("0 0 0BC81EFF\n")) == 1)
    check("um nivel de diferenca passa com tolerancia 1", probe_failures("0 0 0BC81EFF 1\n") == [])
    check("sonda fora da imagem reprova",
          len(probe_failures("3 0 0AC81EFF\n")) == 1 and len(probe_failures("0 2 0AC81EFF\n")) == 1)
    check("sonda mal formada reprova", rejects(parse_probes, "0 0 0AC81E\n")
          and rejects(parse_probes, "0 0 0AC81EGG\n") and rejects(parse_probes, "a b 0AC81EFF\n")
          and rejects(parse_probes, "0 0 0AC81EFF -1\n"))
    check("digito nao ASCII (expoente) reprova como ProbeError",
          rejects(parse_probes, "\u00b2 0 0AC81EFF\n"))


def run_cli(*args):
    return subprocess.run([sys.executable, str(Path(__file__).resolve()), *args],
                          capture_output=True, text=True, check=False)


def selftest_real_process_exit_codes():
    png = build_png(2, 1, 6, [bytes([0, 10, 200, 30, 255, 40, 50, 250, 128])])
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        (root / "image.png").write_bytes(png)
        (root / "good.txt").write_text("0 0 0AC81EFF\n1 0 2832FA80\n")
        (root / "bad.txt").write_text("0 0 C8100AFF\n")
        (root / "alpha.txt").write_text("0 0 0AC81E80\n")
        (root / "empty.txt").write_text("# only a comment\n\n")
        image = str(root / "image.png")
        proc = run_cli(image, str(root / "good.txt"))
        check("processo: sondas boas saem 0 com resumo",
              proc.returncode == 0 and "probes=2 failed=0" in proc.stdout)
        check("processo: cor fora da tolerancia sai 1", run_cli(image, str(root / "bad.txt")).returncode == 1)
        check("processo: alfa errado sai 1", run_cli(image, str(root / "alpha.txt")).returncode == 1)
        (root / "binary.txt").write_bytes(b"\xff\xfe\x00")
        proc = run_cli(image, str(root / "binary.txt"))
        check("processo: arquivo de sondas nao UTF-8 sai 1 sem traceback",
              proc.returncode == 1 and "Traceback" not in proc.stderr)
        check("processo: zero sondas sai 1", run_cli(image, str(root / "empty.txt")).returncode == 1)
        check("processo: PNG inexistente sai 1",
              run_cli(str(root / "nope.png"), str(root / "good.txt")).returncode == 1)
        check("processo: sem argumentos sai 2", run_cli().returncode == 2)


def selftest_main():
    selftest_filters()
    selftest_paeth_ties()
    selftest_png_rejections()
    selftest_probes()
    selftest_real_process_exit_codes()
    print(f"selftest: {len(CHECKS)} controles OK")


def real_main(args):
    """Returns the exit code: 0 only with at least one probe, all passed."""
    if len(args) != 2:
        print(f"usage: {SCRIPT_NAME} <image.png> <probes.txt>  |  --selftest", file=sys.stderr)
        return 2
    try:
        width, height, rows = decode_png(Path(args[0]).read_bytes())
        probes = parse_probes(Path(args[1]).read_text())
    except (ProbeError, OSError, ValueError, zlib.error) as error:
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
