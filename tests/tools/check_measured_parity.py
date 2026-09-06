#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_measured_parity.py - the READER half of MEASURED-COLLECTOR
# (docs/plano-w6a-janela.md, achado do time-lead 06/09/2026). Lives in
# the `parity` job (.github/workflows/ci.yml, GODS_LAWS.md L-04) - the
# ONE place in this project's whole CI where a Linux artifact and a
# Windows artifact are ever downloaded side by side - because what
# decides whether a measured fact deserves to become a promise is the
# COMPARISON between the two systems, never a number sitting alone in
# one leg's own log. A number printed only on the Linux side, with no
# reader ever placing it next to the Windows side, is the exact defect
# this whole fatia exists to close, just moved one file over - see
# this script's own header comment in collect_measured.py for the
# fuller story.
#
# THREE BUCKETS, deliberately never fewer:
#   - iguais: the key was measured on BOTH systems and every value
#     either side reported for it is identical.
#   - divergentes: measured on both systems, but the two sides
#     disagree - window_parity_test.cpp's own logical_size() bug (the
#     one that opened this whole fatia) would have landed here, the
#     day it was still bug and not yet fix.
#   - so de um lado: measured on only ONE system - not automatically a
#     defect (registry_smoke.cpp's own global_count has no Windows
#     concept to compare against at all), but declared, never silently
#     dropped from the table.
#
# WHAT THIS SCRIPT DOES NOT DO, ON PURPOSE (the CTO's own item 4,
# "a regra que fecha o ciclo"): it never fails the `parity` job because
# a key is divergent or one-sided - only because the TOTAL sweep came
# back empty (GODS_LAWS.md L-40, the same floor collect_measured.py
# itself already enforces one layer down). Turning a "divergente"/"so
# de um lado" row into a closed promise, a declared exception, or a
# tracked pending item is a WAVE-CLOSING decision (this fatia's own
# item 4), never something a CI script decides unattended - the same
# separation of "measures" from "judges" this project's own parity_
# exceptions.txt/parity_aliases.txt already keep between check_test_
# parity.py's mechanical union and the human decision of what belongs
# in either file.

import argparse
import sys
from collections import defaultdict

SCRIPT_NAME = "check_measured_parity.py"

import re

MEASURED_LINE = re.compile(r"^MEASURED\s+([A-Za-z0-9_]+\.[A-Za-z0-9_]+)=(.*)$")


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


def parse_measured_files(paths):
    """Returns {key: set(values)} across every MEASURED line found in
    every file in `paths` - a key repeated with the SAME value inside
    one side is normal (several legs of the same matrix measuring the
    identical fact) and collapses into one set member; repeated with a
    DIFFERENT value is a real own-side disagreement, kept visible as a
    multi-member set rather than silently picking one."""
    values = defaultdict(set)
    for path in paths:
        with open(path, "r", encoding="utf-8", errors="replace") as handle:
            for raw_line in handle:
                line = raw_line.strip()
                match = MEASURED_LINE.match(line)
                if match:
                    key, value = match.group(1), match.group(2)
                    values[key].add(value)
    return dict(values)


def build_table(linux_values, windows_values):
    """Returns (iguais, divergentes, so_de_um_lado) as sorted lists of
    (key, linux_repr, windows_repr) triples - `windows_repr`/`linux_
    repr` is "-" when that side never measured the key at all."""
    all_keys = sorted(set(linux_values) | set(windows_values))
    iguais = []
    divergentes = []
    so_de_um_lado = []
    for key in all_keys:
        on_linux = key in linux_values
        on_windows = key in windows_values
        linux_repr = ",".join(sorted(linux_values.get(key, []))) if on_linux else "-"
        windows_repr = ",".join(sorted(windows_values.get(key, []))) if on_windows else "-"
        if on_linux and on_windows:
            if linux_values[key] == windows_values[key]:
                iguais.append((key, linux_repr, windows_repr))
            else:
                divergentes.append((key, linux_repr, windows_repr))
        else:
            so_de_um_lado.append((key, linux_repr, windows_repr))
    return iguais, divergentes, so_de_um_lado


def print_section(title, rows):
    print(f"## {title} ({len(rows)})")
    if not rows:
        print("(nenhuma)")
        return
    print("| chave | Linux | Windows |")
    print("|---|---|---|")
    for key, linux_repr, windows_repr in rows:
        print(f"| `{key}` | {linux_repr} | {windows_repr} |")


def real_main(args):
    parser = argparse.ArgumentParser(prog=SCRIPT_NAME, add_help=False)
    parser.add_argument("--linux", nargs="*", default=[])
    parser.add_argument("--windows", nargs="*", default=[])
    parsed = parser.parse_args(args)

    if not parsed.linux and not parsed.windows:
        fail("usage: check_measured_parity.py --compare --linux <f1> [<f2> ...] --windows <f1> [<f2> ...]")

    linux_values = parse_measured_files(parsed.linux)
    windows_values = parse_measured_files(parsed.windows)

    total_lines = sum(len(v) for v in linux_values.values()) + sum(
        len(v) for v in windows_values.values()
    )
    print(f"{SCRIPT_NAME}: {len(linux_values)} chave(s) do lado Linux, "
          f"{len(windows_values)} chave(s) do lado Windows, {total_lines} valor(es) no total")

    # GODS_LAWS.md L-40: zero chave dos DOIS lados e' sempre coleta
    # quebrada (a mesma logica de piso ja aplicada, um layer abaixo,
    # por collect_measured.py's proprio real_main()) - nunca reprova
    # por divergencia ou por "so de um lado" (este script's own header
    # comment, "O QUE ESTE SCRIPT NAO FAZ").
    if not linux_values and not windows_values:
        fail(
            "0 chave(s) MEASURED em QUALQUER dos dois lados - varredura vazia (GODS_LAWS.md "
            "L-40): a colheita de uma das pernas (ou das duas) esta quebrada, nunca "
            "'nada para comparar'"
        )

    iguais, divergentes, so_de_um_lado = build_table(linux_values, windows_values)
    print_section("MEASURED - iguais", iguais)
    print_section("MEASURED - divergentes", divergentes)
    print_section("MEASURED - so de um lado", so_de_um_lado)
    print(
        f"{SCRIPT_NAME}: {len(iguais)} igual(is), {len(divergentes)} divergente(s), "
        f"{len(so_de_um_lado)} so' de um lado"
    )


# --- selftest -----------------------------------------------------

def _write_temp(tmp_path, name, content):
    path = tmp_path / name
    path.write_text(content, encoding="utf-8")
    return str(path)


def selftest_empty_both_sides_reproves(tmp_path):
    linux_file = _write_temp(tmp_path, "linux_empty.txt", "")
    windows_file = _write_temp(tmp_path, "windows_empty.txt", "")
    try:
        real_main(["--linux", linux_file, "--windows", windows_file])
    except SystemExit as exc:
        if exc.code == 1:
            print("selftest: dois lados vazios reprova (RED) - ok")
            return True
        print(f"selftest: codigo inesperado {exc.code} para dois lados vazios", file=sys.stderr)
        return False
    print("selftest: dois lados vazios NAO reprovou - esperado exit 1", file=sys.stderr)
    return False


def selftest_three_buckets_classify_correctly(tmp_path):
    linux_file = _write_temp(
        tmp_path,
        "linux.txt",
        "MEASURED window_parity_test.active_after_open=0\n"
        "MEASURED window_parity_test.logical_width=800\n"
        "MEASURED registry_smoke.global_count=61\n",
    )
    windows_file = _write_temp(
        tmp_path,
        "windows.txt",
        "MEASURED window_parity_test.active_after_open=0\n"
        "MEASURED window_parity_test.logical_width=0\n",
    )
    import contextlib
    import io

    buffer = io.StringIO()
    try:
        with contextlib.redirect_stdout(buffer):
            real_main(["--linux", linux_file, "--windows", windows_file])
    except SystemExit as exc:
        print(f"selftest: caso de tres baldes reprovou inesperadamente (codigo {exc.code})",
              file=sys.stderr)
        return False
    output = buffer.getvalue()
    checks = [
        "active_after_open" in output.split("iguais")[1].split("divergentes")[0],
        "logical_width" in output.split("divergentes")[1].split("so de um lado")[0],
        "global_count" in output.split("so de um lado")[1],
    ]
    if not all(checks):
        print(f"selftest: classificacao dos tres baldes saiu errada:\n{output}", file=sys.stderr)
        return False
    print("selftest: iguais/divergentes/so-de-um-lado classificados corretamente - ok")
    return True


def selftest_one_side_empty_still_succeeds(tmp_path):
    """One side genuinely having zero MEASURED lines (a leg that never
    ran, or a snapshot that came back empty for a REAL reason on just
    that leg) must NOT reprove by itself - only both sides empty does
    (this script's own header comment, "O QUE ESTE SCRIPT NAO FAZ")."""
    linux_file = _write_temp(
        tmp_path, "linux_only.txt", "MEASURED seat_test.pointer=0\n"
    )
    windows_file = _write_temp(tmp_path, "windows_only_empty.txt", "")
    try:
        real_main(["--linux", linux_file, "--windows", windows_file])
    except SystemExit as exc:
        print(f"selftest: um lado vazio (o outro nao) reprovou inesperadamente (codigo {exc.code})",
              file=sys.stderr)
        return False
    print("selftest: um lado vazio, o outro nao, NAO reprova (so' os dois vazios reprovam) - ok")
    return True


def selftest_main():
    import tempfile
    from pathlib import Path

    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)
        controls = [
            selftest_empty_both_sides_reproves(tmp_path),
            selftest_three_buckets_classify_correctly(tmp_path),
            selftest_one_side_empty_still_succeeds(tmp_path),
        ]
    if not all(controls):
        print(f"{SCRIPT_NAME} --selftest: FALHOU (ver acima)", file=sys.stderr)
        sys.exit(1)
    print(f"{SCRIPT_NAME} --selftest: os {len(controls)} controles OK")


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
    elif args and args[0] == "--compare":
        real_main(args[1:])
    else:
        fail(
            "usage: check_measured_parity.py --compare --linux <f1> [<f2> ...] --windows "
            "<f1> [<f2> ...]  |  --selftest"
        )


if __name__ == "__main__":
    main()
