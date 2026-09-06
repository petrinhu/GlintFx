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
# QUATRO SEÇÕES, desde 06/09/2026 (achado do time-lead, item 3 da
# fatia de fechamento - a versão anterior deste script tinha só tres,
# "iguais/divergentes/so de um lado", e a terceira escondia um problema
# real: 20 das 46 chaves so' de um lado da primeira rodada real ja
# tinham par TESTADO em tests/parity_aliases.txt/exceptions.txt, so' a
# CHAVE especifica que nao tinha sido escrita ainda dos dois lados -
# "so de um lado" tratava isso IGUAL a uma chave de dono nunca visto em
# lugar nenhum, escondendo qual das duas situacoes era qual):
#   - iguais: medido nos dois sistemas, mesmo valor.
#   - divergentes: medido nos dois, valores diferentes -
#     window_parity_test.cpp's own logical_size() bug (o que abriu esta
#     fatia inteira) teria caido aqui, no dia em que ainda era bug.
#   - herdada: so' de um lado, mas o DONO (o texto antes do primeiro
#     ponto na chave, ex. "seat_test" em "seat_test.pointer") ja
#     aparece em tests/parity_exceptions.txt - a lacuna e' CONHECIDA e
#     tem item (ou e' permanente, SEM-PENDENCIA), so' nao tinha sido
#     contada aqui antes. Nunca revalida a regra de morte daquele
#     arquivo (check_test_parity.py's own validate_exceptions() ja faz
#     isso, um portao acima) - so' herda a classificacao.
#   - obrigatoria: so' de um lado, dono SEM entrada em parity_
#     exceptions.txt - a secao que tem de chegar a ZERO no fechamento
#     da onda (este script's own "O QUE ESTE SCRIPT NAO FAZ" abaixo
#     explica por que "obrigatoria != reprova"). Duas formas, ambas
#     impressas: "por par existente" (o dono tem entrada em parity_
#     aliases.txt - o TESTE ja e' comparavel entre sistemas, so' esta
#     chave especifica ainda nao foi medida do outro lado) e "por dono
#     desconhecido" (nem exceptions nem aliases conhecem este dono -
#     achado genuinamente novo).
#
# SCANCOUNT NUNCA ENTRA NESTAS QUATRO SEÇÕES (mesmo achado, "dois
# ajustes de forma no coletor"): uma contagem de varredura exaustiva
# (gfui_*_test's own L-40 counts) e' identica nos cinco sistemas POR
# CONSTRUÇÃO - nenhuma chamada de SO por perto, so' logica C++23 pura -
# entao compara-la nunca responde uma pergunta real de paridade, so'
# reafirma que codigo deterministico e' deterministico. Contada por
# perna, impressa numa linha propria, nunca comparada.
#
# WHAT THIS SCRIPT DOES NOT DO, ON PURPOSE (the CTO's own item 4,
# "a regra que fecha o ciclo"): it never fails the `parity` job because
# a key is divergent or unilateral - only because the TOTAL sweep came
# back empty (GODS_LAWS.md L-40, the same floor collect_measured.py
# itself already enforces one layer down), EVEN for the new "obrigatoria"
# section - a nonzero obrigatoria count is the METRIC the wave-closing
# checklist reads (this fatia's own item 3: "a metrica muda: nao e' a
# tabela encolhendo - e' a secao obrigatoria em zero no fechamento"),
# never something this script blocks a push over by itself. Turning an
# "obrigatoria" row into a closed promise, a declared exception, or a
# tracked pending item is a WAVE-CLOSING decision, never something a CI
# script decides unattended - the same separation of "measures" from
# "judges" this project's own parity_exceptions.txt/parity_aliases.txt
# already keep between check_test_parity.py's mechanical union and the
# human decision of what belongs in either file.

import argparse
import re
import sys
from collections import defaultdict

SCRIPT_NAME = "check_measured_parity.py"

MEASURED_TOKEN_LINE = re.compile(r"^(MEASURED|SCANCOUNT)\s+([A-Za-z0-9_]+)\.([A-Za-z0-9_]+)=(.*)$")

SEM_PENDENCIA = "SEM-PENDENCIA"


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


def parse_measured_files(paths):
    """Returns (measured_values, scancount_totals) - `measured_values`
    is {key: set(values)} for every MEASURED line (key = "owner.key",
    matching collect_measured.py's own token shape); `scancount_totals`
    is a simple per-side line count for every SCANCOUNT line, never
    broken down by key (this script's own header comment, "SCANCOUNT
    NUNCA ENTRA"). A key repeated with the SAME value inside one side
    is normal (several legs of the same matrix measuring the identical
    fact) and collapses into one set member; repeated with a DIFFERENT
    value is a real own-side disagreement, kept visible as a multi-
    member set rather than silently picking one."""
    measured_values = defaultdict(set)
    scancount_total = 0
    for path in paths:
        with open(path, "r", encoding="utf-8", errors="replace") as handle:
            for raw_line in handle:
                line = raw_line.strip()
                match = MEASURED_TOKEN_LINE.match(line)
                if not match:
                    continue
                token, owner, key, value = match.groups()
                if token == "SCANCOUNT":
                    scancount_total += 1
                else:
                    measured_values[f"{owner}.{key}"].add(value)
    return dict(measured_values), scancount_total


def parse_exception_owners(text):
    """Returns {test_name: (item, is_permanent)} from tests/parity_
    exceptions.txt's own `name|platform|reason|item` lines - reads the
    SAME file check_test_parity.py already validates against TODO.md
    one gate over, never re-validating the death rule here (this
    script's own header comment: 'nunca revalida... so' herda')."""
    owners = {}
    for raw_line in text.splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split("|")
        if len(parts) != 4:
            continue
        test_name, _platform, _reason, item = (part.strip() for part in parts)
        owners[test_name] = (item, item == SEM_PENDENCIA)
    return owners


def parse_alias_owners(text):
    """Returns the set of test names appearing on EITHER side of
    tests/parity_aliases.txt's own `linux_name|windows_name` lines -
    the TEST itself has a declared cross-system pair, even when a
    specific MEASURED key from it does not yet."""
    owners = set()
    for raw_line in text.splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split("|")
        if len(parts) != 2:
            continue
        linux_name, windows_name = (part.strip() for part in parts)
        owners.add(linux_name)
        owners.add(windows_name)
    return owners


def classify_unilateral(key, exception_owners, alias_owners):
    """Returns (section, detail) for a key present on exactly one
    side - `section` is "herdada" or "obrigatoria", `detail` is the
    human-readable reason this script's own header comment names.

    Checks the FULL key ("owner.key") against tests/parity_exceptions.
    txt BEFORE falling back to the bare owner - this is what lets a
    SINGLE measured fact (never the whole test) be declared permanent,
    e.g. win32_window_close_request_test.wm_size_during_create's own
    sentinel value (src/platform/win32/display_adapter.cpp's own
    header comment): the TEST itself already has a declared pair
    (tests/parity_aliases.txt), but this ONE fact - a Windows message-
    pump timing detail with no Wayland equivalent even in principle -
    is what actually needs the permanent declaration, not the test."""
    if key in exception_owners:
        item, is_permanent = exception_owners[key]
        if is_permanent:
            return "herdada", "permanente (SEM-PENDENCIA)"
        return "herdada", f"item {item}"
    owner = key.split(".", 1)[0]
    if owner in exception_owners:
        item, is_permanent = exception_owners[owner]
        if is_permanent:
            return "herdada", "permanente (SEM-PENDENCIA)"
        return "herdada", f"item {item}"
    if owner in alias_owners:
        return "obrigatoria", "por par existente (teste tem par, esta chave ainda nao)"
    return "obrigatoria", "por dono desconhecido"


def build_table(linux_values, windows_values, exception_owners, alias_owners):
    """Returns (iguais, divergentes, herdada, obrigatoria) as sorted
    lists of (key, linux_repr, windows_repr[, detail]) - `windows_repr`/
    `linux_repr` is "-" when that side never measured the key at all."""
    all_keys = sorted(set(linux_values) | set(windows_values))
    iguais, divergentes, herdada, obrigatoria = [], [], [], []
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
            continue
        section, detail = classify_unilateral(key, exception_owners, alias_owners)
        row = (key, linux_repr, windows_repr, detail)
        (herdada if section == "herdada" else obrigatoria).append(row)
    return iguais, divergentes, herdada, obrigatoria


def print_section(title, rows, with_detail=False):
    print(f"## {title} ({len(rows)})")
    if not rows:
        print("(nenhuma)")
        return
    if with_detail:
        print("| chave | Linux | Windows | motivo |")
        print("|---|---|---|---|")
        for key, linux_repr, windows_repr, detail in rows:
            print(f"| `{key}` | {linux_repr} | {windows_repr} | {detail} |")
    else:
        print("| chave | Linux | Windows |")
        print("|---|---|---|")
        for key, linux_repr, windows_repr in rows:
            print(f"| `{key}` | {linux_repr} | {windows_repr} |")


def real_main(args):
    parser = argparse.ArgumentParser(prog=SCRIPT_NAME, add_help=False)
    parser.add_argument("--linux", nargs="*", default=[])
    parser.add_argument("--windows", nargs="*", default=[])
    parser.add_argument("--exceptions", default=None, help="tests/parity_exceptions.txt")
    parser.add_argument("--aliases", default=None, help="tests/parity_aliases.txt")
    parsed = parser.parse_args(args)

    if not parsed.linux and not parsed.windows:
        fail("usage: check_measured_parity.py --compare --linux <f1> [<f2> ...] --windows <f1> [<f2> ...] [--exceptions <f>] [--aliases <f>]")

    linux_values, linux_scancount = parse_measured_files(parsed.linux)
    windows_values, windows_scancount = parse_measured_files(parsed.windows)

    exception_owners = {}
    if parsed.exceptions is not None:
        with open(parsed.exceptions, "r", encoding="utf-8") as handle:
            exception_owners = parse_exception_owners(handle.read())
    alias_owners = set()
    if parsed.aliases is not None:
        with open(parsed.aliases, "r", encoding="utf-8") as handle:
            alias_owners = parse_alias_owners(handle.read())

    total_lines = sum(len(v) for v in linux_values.values()) + sum(
        len(v) for v in windows_values.values()
    )
    print(f"{SCRIPT_NAME}: {len(linux_values)} chave(s) MEASURED do lado Linux, "
          f"{len(windows_values)} chave(s) MEASURED do lado Windows, {total_lines} valor(es) no total")
    print(f"{SCRIPT_NAME}: {linux_scancount} linha(s) SCANCOUNT do lado Linux, "
          f"{windows_scancount} linha(s) SCANCOUNT do lado Windows (colhidas, nunca comparadas)")

    # GODS_LAWS.md L-40: zero chave MEASURED dos DOIS lados e' sempre
    # coleta quebrada (a mesma logica de piso ja aplicada, um layer
    # abaixo, por collect_measured.py's proprio real_main()) - nunca
    # reprova por divergencia, por "herdada" ou por "obrigatoria" (este
    # script's own header comment, "O QUE ESTE SCRIPT NAO FAZ").
    if not linux_values and not windows_values:
        fail(
            "0 chave(s) MEASURED em QUALQUER dos dois lados - varredura vazia (GODS_LAWS.md "
            "L-40): a colheita de uma das pernas (ou das duas) esta quebrada, nunca "
            "'nada para comparar'"
        )

    iguais, divergentes, herdada, obrigatoria = build_table(
        linux_values, windows_values, exception_owners, alias_owners
    )
    print_section("MEASURED - iguais", iguais)
    print_section("MEASURED - divergentes", divergentes)
    print_section("MEASURED - herdada", herdada, with_detail=True)
    print_section("MEASURED - obrigatoria", obrigatoria, with_detail=True)
    print(
        f"{SCRIPT_NAME}: {len(iguais)} igual(is), {len(divergentes)} divergente(s), "
        f"{len(herdada)} herdada(s), {len(obrigatoria)} obrigatoria(s)"
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


def selftest_four_sections_classify_correctly(tmp_path):
    """The four cases this fatia's own briefing names, one row each:
    iguais/divergentes (unchanged from before), herdada com item,
    herdada permanente, obrigatoria por par existente, obrigatoria por
    dono desconhecido."""
    linux_file = _write_temp(
        tmp_path,
        "linux4.txt",
        "MEASURED window_parity_test.active_after_open=0\n"
        "MEASURED window_parity_test.logical_width=800\n"
        "MEASURED tem_item_test.some_key=1\n"
        "MEASURED tem_permanente_test.other_key=2\n"
        "MEASURED tem_par_test.paired_key=3\n"
        "MEASURED dono_desconhecido_test.mystery_key=4\n",
    )
    windows_file = _write_temp(
        tmp_path,
        "windows4.txt",
        "MEASURED window_parity_test.active_after_open=0\n"
        "MEASURED window_parity_test.logical_width=0\n",
    )
    exceptions_file = _write_temp(
        tmp_path,
        "exceptions4.txt",
        "tem_item_test|linux|razao qualquer|ITEM-ABERTO\n"
        "tem_permanente_test|linux|razao permanente|SEM-PENDENCIA\n",
    )
    aliases_file = _write_temp(tmp_path, "aliases4.txt", "tem_par_test|tem_par_test_win\n")
    import contextlib
    import io

    buffer = io.StringIO()
    try:
        with contextlib.redirect_stdout(buffer):
            real_main(
                [
                    "--linux",
                    linux_file,
                    "--windows",
                    windows_file,
                    "--exceptions",
                    exceptions_file,
                    "--aliases",
                    aliases_file,
                ]
            )
    except SystemExit as exc:
        print(f"selftest: caso dos quatro baldes reprovou inesperadamente (codigo {exc.code})",
              file=sys.stderr)
        return False
    output = buffer.getvalue()
    try:
        iguais_block = output.split("iguais")[1].split("divergentes")[0]
        divergentes_block = output.split("divergentes")[1].split("herdada")[0]
        herdada_block = output.split("- herdada")[1].split("obrigatoria")[0]
        obrigatoria_block = output.split("- obrigatoria")[1]
    except IndexError:
        print(f"selftest: nao achei as quatro secoes na saida:\n{output}", file=sys.stderr)
        return False
    checks = [
        "active_after_open" in iguais_block,
        "logical_width" in divergentes_block,
        "tem_item_test" in herdada_block and "ITEM-ABERTO" in herdada_block,
        "tem_permanente_test" in herdada_block and "permanente" in herdada_block,
        "tem_par_test" in obrigatoria_block and "par existente" in obrigatoria_block,
        "dono_desconhecido_test" in obrigatoria_block and "dono desconhecido" in obrigatoria_block,
    ]
    if not all(checks):
        print(f"selftest: classificacao dos quatro baldes saiu errada ({checks}):\n{output}",
              file=sys.stderr)
        return False
    print("selftest: iguais/divergentes/herdada/obrigatoria classificados corretamente, os "
          "quatro casos nomeados na fatia - ok")
    return True


def selftest_scancount_never_compared(tmp_path):
    """A SCANCOUNT line collected on both sides is counted, never
    turned into a comparison row - this script's own header comment,
    'SCANCOUNT NUNCA ENTRA NESTAS QUATRO SEÇÕES'."""
    linux_file = _write_temp(
        tmp_path,
        "linux_scan.txt",
        "MEASURED seat_test.pointer=0\n"
        "SCANCOUNT gfui_node_state_test.node_state_count=5\n",
    )
    windows_file = _write_temp(
        tmp_path, "windows_scan.txt", "SCANCOUNT gfui_node_state_test.node_state_count=5\n"
    )
    import contextlib
    import io

    buffer = io.StringIO()
    try:
        with contextlib.redirect_stdout(buffer):
            real_main(["--linux", linux_file, "--windows", windows_file])
    except SystemExit as exc:
        print(f"selftest: caso SCANCOUNT reprovou inesperadamente (codigo {exc.code})",
              file=sys.stderr)
        return False
    output = buffer.getvalue()
    if "node_state_count" in output.split("linha(s) SCANCOUNT")[0]:
        print(f"selftest: chave SCANCOUNT vazou para a tabela MEASURED:\n{output}", file=sys.stderr)
        return False
    if "1 linha(s) SCANCOUNT do lado Linux, 1 linha(s) SCANCOUNT do lado Windows" not in output:
        print(f"selftest: contagem SCANCOUNT por perna saiu errada:\n{output}", file=sys.stderr)
        return False
    print("selftest: SCANCOUNT e' contado por perna e nunca vira linha de comparacao - ok")
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
            selftest_four_sections_classify_correctly(tmp_path),
            selftest_scancount_never_compared(tmp_path),
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
