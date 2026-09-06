#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_gfx_option_ids.py - CI gate for C-OPT (docs/plano-w6b-placa-e-
# laco.md fatia 2a, D-W6b-16, GODS_LAWS.md L-26/L-40).
#
# WHAT THIS PROTECTS: gltfx_gfx_option (include/glintfx/platform/gl/
# gfx_option.hpp) is an APPEND-ONLY public enum - once an id ships, its
# NUMBER and its NAME never change, and neither is ever handed to a
# different option later (the same contract gltfx_err_code already
# carries, err_code.hpp's own header comment). Nothing in C++ itself
# enforces that discipline across an edit - a reviewer reordering two
# `X = N,` lines by hand, or reusing a retired id's own number for a
# new option, compiles clean and passes every runtime test (a runtime
# test only ever sees the CURRENT numbers, never the history). This
# gate reads the SOURCE TEXT of the two files the id lives in - the
# enum itself, and the registry table that names each id's own DATA
# string (src/platform/gl/gfx_option_registry.hpp) - and reproves four
# shapes of that failure MECHANICALLY, the same "the gate reads real
# files, never trusts a comment" discipline check_public_name_
# collision.py/check_dup_laws.py already use:
#
#   1. every enumerator has an EXPLICIT numeric value (no implicit
#      auto-increment a reviewer could silently shift by inserting a
#      line above it);
#   2. the values are STRICTLY INCREASING in the file's own declaration
#      order (append-only: a value is never reordered to sit before
#      one that used to come earlier - unlike unscoped auto-increment,
#      an explicit-but-reordered value can slip past a naive parser
#      that only checks for duplicates, so this gate checks ORDER, not
#      merely uniqueness);
#   3. no id, once shipped, is ever handed to a DIFFERENT option later
#      (RETIRED_OPTION_IDS below - empty today, since v1 has retired
#      nothing yet, structurally ready for the day it does, the same
#      "the row stays, documented as retired" contract err_code.hpp's
#      own header comment already states for gltfx_err_code);
#   4. the registry table (gfx_option_registry.hpp) names the SAME set
#      of ids, in the SAME order, and its own "dado" name string (the
#      one a consumer's saved settings file writes and reads back,
#      docs/api-conventions.md's third register) is never duplicated
#      across two rows.
#
# Usage:
#   check_gfx_option_ids.py <source-root-directory>
#   check_gfx_option_ids.py --selftest
#
# --selftest runs the THREE controls this fatia's own plan names
# verbatim (docs/plano-w6b-placa-e-laco.md, fatia 2a's own row:
# "autoteste com os tres controles" / "positivo, negativo: id repetido,
# varredura vazia") against disposable fixtures under a scratch
# directory, never against the real tracked tree - the same pattern
# check_layers.py's own --selftest already established.

import os
import re
import shutil
import sys
import tempfile

SCRIPT_NAME = "check_gfx_option_ids.py"

ENUM_HEADER_RELATIVE_PATH = os.path.join("include", "glintfx", "platform", "gl", "gfx_option.hpp")
REGISTRY_HEADER_RELATIVE_PATH = os.path.join("src", "platform", "gl", "gfx_option_registry.hpp")

# Item 3 above: (id, name) pairs a PAST option once shipped under, and
# will never ship under again for a DIFFERENT meaning. Empty today - v1
# has retired nothing - append here, forever, the day an option is
# ever retired (mirror err_code.hpp's own "the row stays, documented as
# retired" contract).
RETIRED_OPTION_IDS = ()  # tuple of (int, str) pairs, e.g. (9, "old_name")

_ENUM_BLOCK_PATTERN = re.compile(
    r"enum\s+class\s+gltfx_gfx_option\s*:\s*std::uint16_t\s*\{(.*?)\};", re.DOTALL
)
_ENUM_ENUMERATOR_PATTERN = re.compile(r"(\w+)\s*=\s*(\d+)\s*,")

_TABLE_BLOCK_PATTERN = re.compile(
    r"k_gfx_option_table\s*\{\{(.*?)\}\};", re.DOTALL
)
_TABLE_ROW_PATTERN = re.compile(r'\{\s*gltfx_gfx_option::(\w+)\s*,\s*"([^"]*)"')


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


def read_text(path):
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        return handle.read()


# Returns an ORDERED list of (name, value) pairs, in declaration order,
# never a dict (a dict would silently lose order information a set of
# duplicate names could hide behind).
def parse_enum_rows(text):
    block_match = _ENUM_BLOCK_PATTERN.search(text)
    if block_match is None:
        return None
    body = block_match.group(1)
    rows = []
    for match in _ENUM_ENUMERATOR_PATTERN.finditer(body):
        rows.append((match.group(1), int(match.group(2))))
    return rows


# Returns an ORDERED list of (identifier, data_name) pairs, in
# declaration order - identifier is the gltfx_gfx_option::<identifier>
# each row references, data_name is the quoted "dado" string.
def parse_table_rows(text):
    block_match = _TABLE_BLOCK_PATTERN.search(text)
    if block_match is None:
        return None
    body = block_match.group(1)
    rows = []
    for match in _TABLE_ROW_PATTERN.finditer(body):
        rows.append((match.group(1), match.group(2)))
    return rows


def require_nonempty_scan(count, what):
    if count == 0:
        print(
            f"{SCRIPT_NAME}: varredura vazia (0 {what} encontrado(s)) - GODS_LAWS.md L-40",
            file=sys.stderr,
        )
        return False
    return True


# The actual gate logic, factored out of real_main() so --selftest
# exercises the EXACT same function - not a reimplementation that
# could drift from production (check_layers.py's own precedent).
def check_gfx_option_ids(root):
    enum_path = os.path.join(root, ENUM_HEADER_RELATIVE_PATH)
    registry_path = os.path.join(root, REGISTRY_HEADER_RELATIVE_PATH)

    if not os.path.isfile(enum_path):
        print(f"{SCRIPT_NAME}: arquivo nao encontrado: {enum_path}", file=sys.stderr)
        return False
    if not os.path.isfile(registry_path):
        print(f"{SCRIPT_NAME}: arquivo nao encontrado: {registry_path}", file=sys.stderr)
        return False

    enum_rows = parse_enum_rows(read_text(enum_path))
    if enum_rows is None:
        print(
            f"{SCRIPT_NAME}: bloco 'enum class gltfx_gfx_option : std::uint16_t {{ ... }}' nao "
            f"encontrado em {enum_path}",
            file=sys.stderr,
        )
        return False

    if not require_nonempty_scan(len(enum_rows), "enumerador(es) de gltfx_gfx_option"):
        return False

    # Item 1: every enumerator already has an explicit value by
    # construction of the regex above (it only matches "name = N,");
    # an enumerator with no "= N" (implicit auto-increment) simply does
    # not match and is silently absent from enum_rows - which would
    # then disagree with the registry's own row count below, catching
    # it indirectly. Checked directly too, by re-scanning for any
    # comma-terminated identifier the value-carrying pattern above did
    # not consume.
    block_match = _ENUM_BLOCK_PATTERN.search(read_text(enum_path))
    body = block_match.group(1)
    bare_identifiers = re.findall(r"(?:\{|,)\s*(\w+)\s*,", "{" + body)
    explicit_names = {name for name, _value in enum_rows}
    implicit = [name for name in bare_identifiers if name not in explicit_names]
    if implicit:
        print(
            f"{SCRIPT_NAME}: enumerador(es) sem valor numerico explicito: {', '.join(implicit)}",
            file=sys.stderr,
        )
        return False

    # Item 2: strictly increasing in declaration order - append-only,
    # never reordered.
    for previous, current in zip(enum_rows, enum_rows[1:]):
        if current[1] <= previous[1]:
            print(
                f"{SCRIPT_NAME}: ids nao crescentes na ordem de declaracao: "
                f"{previous[0]}={previous[1]} seguido de {current[0]}={current[1]}",
                file=sys.stderr,
            )
            return False

    # Item 2, continued: no duplicate value (already implied by strict
    # increase above, checked again defensively in case a future edit
    # relaxes that rule to "non-decreasing").
    seen_values = {}
    for name, value in enum_rows:
        if value in seen_values:
            print(
                f"{SCRIPT_NAME}: id {value} repetido entre '{seen_values[value]}' e '{name}'",
                file=sys.stderr,
            )
            return False
        seen_values[value] = name

    # Item 3: no live id/name matches a retired one.
    live_names = {name for name, _value in enum_rows}
    for retired_value, retired_name in RETIRED_OPTION_IDS:
        if retired_value in seen_values:
            print(
                f"{SCRIPT_NAME}: id {retired_value} foi aposentado (era '{retired_name}') e "
                f"nunca pode ser reutilizado - agora usado por '{seen_values[retired_value]}'",
                file=sys.stderr,
            )
            return False
        if retired_name in live_names:
            print(
                f"{SCRIPT_NAME}: nome '{retired_name}' foi aposentado (id {retired_value}) e "
                "nunca pode ser reutilizado",
                file=sys.stderr,
            )
            return False

    # Item 4: the registry table.
    table_rows = parse_table_rows(read_text(registry_path))
    if table_rows is None:
        print(
            f"{SCRIPT_NAME}: bloco 'k_gfx_option_table{{{{ ... }}}}' nao encontrado em "
            f"{registry_path}",
            file=sys.stderr,
        )
        return False
    if not require_nonempty_scan(len(table_rows), "linha(s) de k_gfx_option_table"):
        return False

    enum_identifiers_in_order = [name for name, _value in enum_rows]
    table_identifiers_in_order = [identifier for identifier, _data_name in table_rows]
    if table_identifiers_in_order != enum_identifiers_in_order:
        print(
            f"{SCRIPT_NAME}: a ordem/conjunto de ids em {registry_path} diverge do enum: "
            f"enum={enum_identifiers_in_order} tabela={table_identifiers_in_order}",
            file=sys.stderr,
        )
        return False

    seen_data_names = {}
    for identifier, data_name in table_rows:
        if data_name in seen_data_names:
            print(
                f"{SCRIPT_NAME}: nome de dado '{data_name}' repetido entre '"
                f"{seen_data_names[data_name]}' e '{identifier}'",
                file=sys.stderr,
            )
            return False
        seen_data_names[data_name] = identifier

    print(
        f"{SCRIPT_NAME}: {len(enum_rows)} id(s) verificado(s) - explicitos, crescentes, sem "
        "buraco reutilizado, nome de dado unico"
    )
    return True


# --- real mode -------------------------------------------------------


def real_main(args):
    if len(args) != 1:
        fail("usage: check_gfx_option_ids.py <source-root-directory>")
    root = args[0]
    if not os.path.isdir(root):
        fail(f"directory not found: {root}")
    if not check_gfx_option_ids(root):
        fail("violacao de id de opcao grafica encontrada (GODS_LAWS.md L-26; ver mensagem acima)")


# --- fixtures and controls for --selftest -----------------------------


def make_scratch_workdir():
    return tempfile.mkdtemp(prefix="glintfx-gfx-option-ids-selftest-", dir=os.environ.get("TMPDIR"))


_FIXTURE_ENUM_TEMPLATE = """// fixture
namespace glintfx {{
enum class gltfx_gfx_option : std::uint16_t {{
{body}
}};
}}
"""

_FIXTURE_TABLE_TEMPLATE = """// fixture
namespace glintfx::platform {{
inline constexpr std::array<gfx_option_row, {count}> k_gfx_option_table{{{{
{body}
}}}};
}}
"""


def write_fixture(root, enum_body, table_body):
    enum_path = os.path.join(root, ENUM_HEADER_RELATIVE_PATH)
    registry_path = os.path.join(root, REGISTRY_HEADER_RELATIVE_PATH)
    os.makedirs(os.path.dirname(enum_path), exist_ok=True)
    os.makedirs(os.path.dirname(registry_path), exist_ok=True)
    with open(enum_path, "w", encoding="utf-8") as handle:
        handle.write(_FIXTURE_ENUM_TEMPLATE.format(body=enum_body))
    with open(registry_path, "w", encoding="utf-8") as handle:
        handle.write(_FIXTURE_TABLE_TEMPLATE.format(body=table_body, count=table_body.count("{")))


def _make_capture():
    import contextlib
    import io

    class _Captured:
        __slots__ = ("result", "text")

        def __init__(self, result, text):
            self.result = result
            self.text = text

    def capture(fn):
        buffer = io.StringIO()
        with contextlib.redirect_stdout(buffer), contextlib.redirect_stderr(buffer):
            result = fn()
        return _Captured(result, buffer.getvalue())

    return capture


_CLEAN_ENUM_BODY = "    vsync = 0,\n    frame_rate_cap = 1,\n    gpu_preference = 2,"
_CLEAN_TABLE_BODY = (
    '    {gltfx_gfx_option::vsync, "vsync"},\n'
    '    {gltfx_gfx_option::frame_rate_cap, "frame_rate_cap"},\n'
    '    {gltfx_gfx_option::gpu_preference, "gpu_preference"},'
)


# Positive control: a clean, three-row fixture, consistent between the
# two files. Expected: passes.
def selftest_positive_control(scratch, capture):
    root = os.path.join(scratch, "positive")
    write_fixture(root, _CLEAN_ENUM_BODY, _CLEAN_TABLE_BODY)

    outcome = capture(lambda: check_gfx_option_ids(root))
    if outcome.result:
        print("selftest: controle POSITIVO OK (fixture limpa aprovada)")
        return True
    print(
        "selftest: controle POSITIVO FALHOU (fixture limpa deveria ter sido aprovada)",
        file=sys.stderr,
    )
    print(outcome.text, file=sys.stderr)
    return False


# Negative control: id repetido - two enumerators share the numeric
# value 1. Expected: reproves, citing both names.
def selftest_negative_control_repeated_id(scratch, capture):
    root = os.path.join(scratch, "negative_repeated_id")
    dirty_enum_body = "    vsync = 0,\n    frame_rate_cap = 1,\n    gpu_preference = 1,"
    write_fixture(root, dirty_enum_body, _CLEAN_TABLE_BODY)

    outcome = capture(lambda: check_gfx_option_ids(root))
    if outcome.result:
        print(
            "selftest: controle NEGATIVO (id repetido) FALHOU (id duplicado nao foi pego)",
            file=sys.stderr,
        )
        return False
    if "repetido" not in outcome.text and "crescentes" not in outcome.text:
        print(
            "selftest: controle NEGATIVO (id repetido) FALHOU (reprovou, mas nao citou o motivo)",
            file=sys.stderr,
        )
        print(outcome.text, file=sys.stderr)
        return False
    print("selftest: controle NEGATIVO (id repetido) OK (id duplicado pego e citado)")
    return True


# Empty-scan floor: the enum block exists but has no enumerators at
# all. Expected: reproves with "varredura vazia" in the message.
def selftest_empty_scan_control(scratch, capture):
    root = os.path.join(scratch, "empty")
    write_fixture(root, "", _CLEAN_TABLE_BODY)

    outcome = capture(lambda: check_gfx_option_ids(root))
    if outcome.result:
        print(
            "selftest: controle de VARREDURA VAZIA FALHOU (enum sem enumeradores deveria ter "
            "sido recusado, mas passou)",
            file=sys.stderr,
        )
        print(outcome.text, file=sys.stderr)
        return False
    if "varredura vazia" not in outcome.text:
        print(
            "selftest: controle de VARREDURA VAZIA FALHOU (recusou, mas nao disse 'varredura "
            "vazia')",
            file=sys.stderr,
        )
        print(outcome.text, file=sys.stderr)
        return False
    print("selftest: controle de VARREDURA VAZIA OK (enum sem enumeradores recusado)")
    return True


def selftest_main():
    scratch = make_scratch_workdir()
    capture = _make_capture()
    try:
        controls = [
            selftest_positive_control(scratch, capture),
            selftest_negative_control_repeated_id(scratch, capture),
            selftest_empty_scan_control(scratch, capture),
        ]
        if not all(controls):
            print("check_gfx_option_ids.py --selftest: FALHOU (ver acima)", file=sys.stderr)
            sys.exit(1)
        print(f"check_gfx_option_ids.py --selftest: os {len(controls)} controles OK")
    finally:
        shutil.rmtree(scratch, ignore_errors=True)


def main():
    args = sys.argv[1:]
    if args == ["--selftest"]:
        selftest_main()
        return
    real_main(args)


if __name__ == "__main__":
    main()
