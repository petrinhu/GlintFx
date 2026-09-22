#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# extract_css_named_colors.py - GFSS-COLOR-ORACLE (TODO.md, GODS_LAWS.md
# L-07/L-27/L-29/L-36/L-40): generates tests/fixtures/css_color_4_named_
# colors.txt from a LOCAL, never-fetched-at-test-time copy of the CSS
# Color Module Level 4 specification source (Overview.bs), so
# tests/gfss_named_colors_doc_oracle_test.cpp has something INDEPENDENT
# of src/gfss/named_colors.cpp's own k_table to compare against - see
# that test file's own header comment for the achado this closes (the
# old test in gfss_color_parse_test.cpp compared parse_color() against
# named_color_at(), the SAME table feeding the code under test, proving
# only that the program agrees with itself).
#
# THIS SCRIPT NEVER TOUCHES THE NETWORK (GODS_LAWS.md L-29's own "sem
# clonar" - this is data extraction from a public standard's TEXT, read
# to learn its SHAPE, not code copied from any implementation): the
# spec source is a LOCAL file path the caller supplies, fetched once by
# a human/agent OUTSIDE this script and never committed to this repo
# (GODS_LAWS.md L-07/L-08: it is third-party specification TEXT owned
# by the W3C, not data this project owns - only the EXTRACTED 4-column
# table, with its own provenance header, is committed).
#
# THE EXPRESSION, pinned in TODO.md's own GFSS-COLOR-ORACLE item and
# reproduced here verbatim: <dfn>[a-z]+</dfn><td>#[0-9a-f]{6}<td>[0-9]+
# [0-9]+ [0-9]+ - matches ONE opaque named-color table row in the exact
# substring form the pinned commit's Overview.bs emits it (measured
# against that commit, not invented): each <tr> under SS6.1's own
# <table class="named-color-table"> carries a <th scope=row><dfn>NAME
# </dfn><td>#HEXHEX<td>R G B run, and this regex targets exactly that
# run, ignoring every other markup on the line (the leading <td
# style="background:..."> swatch cells this run always follows).
#
# CROSS-CHECKED TWICE PER ROW, NOT TRUSTED ON FAITH: SS6.1's own table
# states each color's channels TWICE on the same line - a #hex column
# and a decimal column - and extract_rows() below requires the two to
# agree before accepting a row. A regex that drifted across a row
# boundary (matching the wrong <dfn> against the wrong <td> pair) is
# far more likely to produce a hex/decimal MISMATCH than to produce a
# consistent, wrong triple in both encodings at once - this is the
# SAME "verify inside our own extraction, never assume" posture named_
# colors.hpp's own header comment already applies to its two
# independently-fetched cross-check sources.
#
# GODS_LAWS.md L-40 (piso de varredura nao-vazia), applied to the
# EXACT count, not merely "more than zero": run_extraction() below
# rejects zero rows (the generic L-40 floor) AND rejects any count
# other than the expected one (148 in real use) - a spec table that
# gained or lost a color between the pinned commit and a differently-
# fetched local copy must be caught here, not silently narrowed into
# whatever the regex happened to find.
#
# Usage:
#   extract_css_named_colors.py <local-Overview.bs-path> <output-fixture-path>
#   extract_css_named_colors.py --selftest
#
# --selftest exercises run_extraction() directly against small, in-
# memory spec-text fixtures (never the real, un-committed Overview.bs -
# GODS_LAWS.md L-07/L-08) with the three controls GODS_LAWS.md L-40
# requires of every gate (positive, negative, empty-scan floor) plus
# this tool's own hex/decimal cross-check control and a full write-
# then-read-back round trip of render_fixture()/FIXTURE_HEADER.
#
# Each function below does one thing (GODS_LAWS.md L-17).

import argparse
import contextlib
import io
import re
import sys
import tempfile
import os

SCRIPT_NAME = "extract_css_named_colors.py"

# The exact substring form measured against the pinned commit (see this
# file's own header comment) - one match per opaque named-color row.
ROW_PATTERN = re.compile(r"<dfn>([a-z]+)</dfn><td>#([0-9a-f]{6})<td>([0-9]+) ([0-9]+) ([0-9]+)")

PINNED_COMMIT = "c61efd6e898c001dc05ccd42c816dbcd67af2f07"
PINNED_SOURCE_URL = (
    f"https://raw.githubusercontent.com/w3c/csswg-drafts/{PINNED_COMMIT}/css-color-4/Overview.bs"
)
PINNED_LICENSE_URL = "https://www.w3.org/Consortium/Legal/copyright-software"
EXPECTED_ROW_COUNT = 148


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


# --- extraction, pure (no filesystem, no sys.exit - real_main() and
# --selftest both drive the SAME function, so the tested path IS the
# real path) --------------------------------------------------------


class ExtractionError(Exception):
    """Raised when a row's own two channel encodings disagree - see
    this file's own header comment for why that is the far more likely
    symptom of a mis-aimed match than a consistent wrong value."""


def extract_rows(spec_text):
    """Every (name, r, g, b) tuple ROW_PATTERN finds in `spec_text`, in
    the order encountered (never sorted - the spec's own table is
    already alphabetical, matching src/gfss/named_colors.cpp's own
    k_table order, so a reviewer's diff against a re-fetched copy stays
    a straight line-for-line compare, and so does a diff against this
    project's OWN table)."""
    rows = []
    for match in ROW_PATTERN.finditer(spec_text):
        name, hex_value, r, g, b = match.groups()
        hex_r, hex_g, hex_b = (int(hex_value[i : i + 2], 16) for i in (0, 2, 4))
        dec_r, dec_g, dec_b = int(r), int(g), int(b)
        if (hex_r, hex_g, hex_b) != (dec_r, dec_g, dec_b):
            raise ExtractionError(
                f"linha de '{name}': hex #{hex_value} (-> {hex_r} {hex_g} {hex_b}) e decimal "
                f"{r} {g} {b} discordam dentro do proprio Overview.bs - extracao recusada"
            )
        rows.append((name, dec_r, dec_g, dec_b))
    return rows


def validate_row_count(rows, expected_count):
    """None when `rows` count is acceptable (exactly `expected_count`),
    an error message string otherwise. Zero is always rejected first,
    with its own wording (GODS_LAWS.md L-40's own floor), even when
    `expected_count` also happens to be zero (that can never be a real
    call - EXPECTED_ROW_COUNT is a positive constant - but a helper
    that silently accepted "0 == 0" would be exactly the sucesso
    silencioso L-40 forbids)."""
    if len(rows) == 0:
        return (
            "varredura vazia - 0 linha(s) casaram a expressao (GODS_LAWS.md L-40: "
            "zero itens e falha, nunca sucesso silencioso)"
        )
    if len(rows) != expected_count:
        return (
            f"contagem inesperada: {len(rows)} linha(s) casaram a expressao, esperava "
            f"exatamente {expected_count} - a copia local do Overview.bs pode ser de outro "
            "commit, ou a tabela da especificacao mudou; a fixture nao e escrita com uma "
            "contagem nao confirmada"
        )
    return None


def run_extraction(spec_text, expected_count):
    """The whole pipeline (extract + validate count), pure. Returns
    (rows, error) - error is None on success, and rows is None on
    failure. Never touches the filesystem, never calls sys.exit -
    real_main() and every --selftest control below call this SAME
    function, so the path under test in --selftest IS the real path."""
    try:
        rows = extract_rows(spec_text)
    except ExtractionError as exc:
        return None, str(exc)
    error = validate_row_count(rows, expected_count)
    if error is not None:
        return None, error
    return rows, None


# --- fixture rendering ------------------------------------------------


def render_fixture_body(rows):
    return "".join(f"{name} {r} {g} {b}\n" for name, r, g, b in rows)


def fixture_header(row_count):
    return f"""# SPDX-License-Identifier: AGPL-3.0-or-later
#
# tests/fixtures/css_color_4_named_colors.txt
#
# GENERATED by tests/tools/extract_css_named_colors.py (TODO.md
# GFSS-COLOR-ORACLE, GODS_LAWS.md L-27/L-29/L-40) from a LOCAL,
# never-fetched-at-test-time copy of the CSS Color Module Level 4
# specification source, pinned to one commit:
#
#   source commit: {PINNED_COMMIT}
#   source url:    {PINNED_SOURCE_URL}
#   license:       W3C Software and Document License
#                  ({PINNED_LICENSE_URL})
#   expression:    <dfn>[a-z]+</dfn><td>#[0-9a-f]{{6}}<td>[0-9]+ [0-9]+ [0-9]+
#   row count:     {row_count}
#
# To REGENERATE and compare byte-for-byte: fetch Overview.bs at the
# SAME commit above to a local file (never committed to this repo -
# GODS_LAWS.md L-07/L-08: this is third-party specification TEXT, not
# data this project owns) and run:
#
#   tests/tools/extract_css_named_colors.py <local Overview.bs path> \\
#       tests/fixtures/css_color_4_named_colors.txt
#
# Each data line below is "name r g b" (ASCII-lowercase name, decimal
# 0-255 channel triple), one SS6.1 opaque named color per line, in the
# SAME order the spec's own table uses (already alphabetical -
# src/gfss/named_colors.cpp's own k_table mirrors it). "transparent"
# (SS6.3) is deliberately NOT one of these {row_count} rows - it is not
# part of the opaque named-colors table at all, and is checked
# separately in tests/gfss_named_colors_doc_oracle_test.cpp against the
# literal rgba(0,0,0,0) SS6.3 itself defines.
#
"""


def render_fixture(rows):
    return fixture_header(len(rows)) + render_fixture_body(rows)


# --- real mode ----------------------------------------------------------


def real_main(args):
    parser = argparse.ArgumentParser(prog=SCRIPT_NAME)
    parser.add_argument("spec_path", help="local copy of Overview.bs (never fetched here)")
    parser.add_argument("output_path", help="fixture file to (over)write")
    parsed = parser.parse_args(args)

    try:
        with open(parsed.spec_path, "r", encoding="utf-8") as handle:
            spec_text = handle.read()
    except OSError as exc:
        fail(f"nao consegui ler '{parsed.spec_path}': {exc}")
        return

    rows, error = run_extraction(spec_text, EXPECTED_ROW_COUNT)
    if error is not None:
        fail(f"'{parsed.spec_path}': {error}")
        return

    try:
        with open(parsed.output_path, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(render_fixture(rows))
    except OSError as exc:
        fail(f"nao consegui escrever '{parsed.output_path}': {exc}")
        return

    print(
        f"{SCRIPT_NAME}: {len(rows)} linha(s) extraida(s) de '{parsed.spec_path}', fixture "
        f"escrita em '{parsed.output_path}'"
    )


# --- selftest -------------------------------------------------------


def _make_capture():
    def capture(fn):
        buffer = io.StringIO()
        with contextlib.redirect_stdout(buffer), contextlib.redirect_stderr(buffer):
            result = fn()
        return result, buffer.getvalue()

    return capture


# One valid, agreeing row per color - a tiny stand-in table, never the
# real (un-committed) Overview.bs.
def _spec_text(rows):
    lines = []
    for name, r, g, b in rows:
        hex_value = f"{r:02x}{g:02x}{b:02x}"
        lines.append(
            f'\t\t\t<tr>\n\t\t\t\t<td style="background:{name}">&nbsp;<td '
            f'style="background:#{hex_value}">&nbsp;<th scope=row><dfn>{name}</dfn>'
            f"<td>#{hex_value}<td>{r} {g} {b}"
        )
    return "\n".join(lines) + "\n"


def selftest_positive_control():
    rows = [("aliceblue", 240, 248, 255), ("azure", 240, 255, 255)]
    text = _spec_text(rows)
    got, error = run_extraction(text, expected_count=2)
    if error is not None or got != rows:
        print(
            f"selftest: controle POSITIVO FALHOU (got={got!r} error={error!r}, esperava "
            f"{rows!r} sem erro)",
            file=sys.stderr,
        )
        return False
    print("selftest: controle POSITIVO OK (2 linha(s) validas extraidas com os canais corretos)")
    return True


def selftest_negative_hex_decimal_mismatch_control():
    # aliceblue's real hex is #f0f8ff (240 248 255); the decimal column
    # here is deliberately wrong (1 2 3) - the row a mis-aimed regex
    # match would most plausibly produce (this file's own header
    # comment).
    text = (
        '\t\t\t<tr>\n\t\t\t\t<td style="background:aliceblue">&nbsp;<td '
        'style="background:#f0f8ff">&nbsp;<th scope=row><dfn>aliceblue</dfn>'
        "<td>#f0f8ff<td>1 2 3\n"
    )
    got, error = run_extraction(text, expected_count=1)
    if got is not None or error is None or "aliceblue" not in error or "discordam" not in error:
        print(
            f"selftest: controle NEGATIVO (hex x decimal) FALHOU (got={got!r} error={error!r})",
            file=sys.stderr,
        )
        return False
    print(
        "selftest: controle NEGATIVO (hex x decimal) OK (linha com hex e decimal discordantes "
        "recusada, aliceblue citado)"
    )
    return True


def selftest_empty_scan_control():
    got, error = run_extraction("nenhuma linha de cor aqui\n", expected_count=2)
    if got is not None or error is None or "varredura vazia" not in error:
        print(f"selftest: controle de VARREDURA VAZIA FALHOU (got={got!r} error={error!r})",
              file=sys.stderr)
        return False
    print("selftest: controle de VARREDURA VAZIA OK (0 linha(s) casadas, recusado)")
    return True


def selftest_wrong_count_control():
    rows = [("aliceblue", 240, 248, 255)]
    text = _spec_text(rows)
    got, error = run_extraction(text, expected_count=2)
    if got is not None or error is None or "contagem inesperada" not in error:
        print(f"selftest: controle de CONTAGEM INESPERADA FALHOU (got={got!r} error={error!r})",
              file=sys.stderr)
        return False
    print(
        "selftest: controle de CONTAGEM INESPERADA OK (1 linha extraida, 2 esperadas, recusado)"
    )
    return True


def selftest_fixture_round_trip_control():
    rows = [("aliceblue", 240, 248, 255), ("azure", 240, 255, 255), ("beige", 245, 245, 220)]
    text = render_fixture(rows)
    scratch_dir = tempfile.mkdtemp(prefix="glintfx-extract-css-colors-selftest-")
    try:
        spec_path = os.path.join(scratch_dir, "Overview.bs")
        output_path = os.path.join(scratch_dir, "css_color_4_named_colors.txt")
        with open(spec_path, "w", encoding="utf-8") as handle:
            handle.write(_spec_text(rows))
        capture = _make_capture()
        global EXPECTED_ROW_COUNT
        saved_expected = EXPECTED_ROW_COUNT
        EXPECTED_ROW_COUNT = len(rows)
        try:
            _, output_text = capture(lambda: real_main([spec_path, output_path]))
        finally:
            EXPECTED_ROW_COUNT = saved_expected
        if not os.path.isfile(output_path):
            print(
                f"selftest: controle ROUND-TRIP FALHOU (fixture nao foi escrita, saida: "
                f"{output_text!r})",
                file=sys.stderr,
            )
            return False
        with open(output_path, "r", encoding="utf-8") as handle:
            written = handle.read()
        if written != text:
            print(
                "selftest: controle ROUND-TRIP FALHOU (fixture escrita por real_main() difere "
                "de render_fixture() chamada direto)",
                file=sys.stderr,
            )
            return False
        # SPDX header must land in the FIRST 3 lines - check_spdx.py's
        # own REQUIRED_HEADER scan window (tests/tools/check_spdx.py,
        # file_has_header()) - a fixture failing that gate would be a
        # second, independent bug this control also has to catch.
        first_three_lines = "".join(written.splitlines(keepends=True)[:3])
        if "SPDX-License-Identifier: AGPL-3.0-or-later" not in first_three_lines:
            print(
                "selftest: controle ROUND-TRIP FALHOU (cabecalho SPDX nao esta nas 3 primeiras "
                "linhas - check_spdx.py reprovaria esta fixture)",
                file=sys.stderr,
            )
            return False
        print(
            f"selftest: controle ROUND-TRIP OK ({len(rows)} linha(s), fixture escrita por "
            "real_main() bate byte a byte com render_fixture() direto, cabecalho SPDX presente)"
        )
        return True
    finally:
        for root, _dirs, files in os.walk(scratch_dir, topdown=False):
            for name in files:
                os.remove(os.path.join(root, name))
        os.rmdir(scratch_dir)


def selftest_main():
    controls = [
        selftest_positive_control(),
        selftest_negative_hex_decimal_mismatch_control(),
        selftest_empty_scan_control(),
        selftest_wrong_count_control(),
        selftest_fixture_round_trip_control(),
    ]
    if not all(controls):
        print(f"{SCRIPT_NAME} --selftest: FALHOU (ver acima)", file=sys.stderr)
        sys.exit(1)
    print(f"{SCRIPT_NAME} --selftest: os {len(controls)} controles OK")


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
    else:
        real_main(args)


if __name__ == "__main__":
    main()
