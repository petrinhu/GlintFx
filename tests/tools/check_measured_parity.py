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
# CINCO SEÇÕES, desde 06/09/2026 (achado do time-lead E do CTO, item 3
# da fatia de fechamento, em duas rodadas - a versão anterior deste
# script tinha quatro, "iguais/divergentes/herdada/obrigatoria", e a
# segunda rodada achou que "divergentes em zero" era regra IMPOSSIVEL
# de cumprir: uma divergencia de AMBIENTE (seat_test.pointer's own
# 0-vs-1, o compositor headless nao anuncia ponteiro, a VM Windows tem
# mouse) e' permanente por natureza - regra impossivel e' contornada na
# primeira pressa, entao "divergentes" virou duas secoes):
#   - iguais: medido nos dois sistemas, mesmo valor.
#   - divergentes nao declaradas: medido nos dois, valores diferentes,
#     SEM linha em tests/measured_exceptions.txt com lado="ambos" -
#     window_parity_test.cpp's own logical_size() bug (o que abriu esta
#     fatia inteira) teria caido aqui, no dia em que ainda era bug. A
#     secao que tem de chegar a ZERO no fechamento, junto com
#     "obrigatoria" abaixo.
#   - divergentes declaradas: medido nos dois, valores diferentes, COM
#     linha em tests/measured_exceptions.txt (lado="ambos") explicando
#     (a) por que o ambiente difere e (b) qual teste prova o
#     comportamento da BIBLIOTECA no lugar - sem (b) a linha seria
#     carimbo, nao sentinela (ver a condicao em classify_divergent()).
#     Listada com contagem, nunca escondida, mas NUNCA bloqueia.
#   - herdada: so' de um lado, mas o DONO (o texto antes do primeiro
#     ponto na chave, ex. "seat_test" em "seat_test.pointer") ja
#     aparece em tests/parity_exceptions.txt, OU a propria chave tem
#     linha em tests/measured_exceptions.txt (lado="linux"/"windows") -
#     a lacuna e' CONHECIDA e tem item (ou e' permanente, SEM-
#     PENDENCIA). Nunca revalida a regra de morte daquele arquivo
#     (check_test_parity.py's own validate_exceptions() ja faz isso,
#     um portao acima) - so' herda a classificacao.
#   - obrigatoria: so' de um lado, dono SEM entrada em parity_
#     exceptions.txt e chave SEM entrada em measured_exceptions.txt -
#     a OUTRA secao que tem de chegar a ZERO no fechamento da onda
#     (este script's own "O QUE ESTE SCRIPT NAO FAZ" abaixo explica
#     por que "obrigatoria != reprova"). Duas formas, ambas impressas:
#     "por par existente" (o dono tem entrada em parity_aliases.txt -
#     o TESTE ja e' comparavel entre sistemas, so' esta chave
#     especifica ainda nao foi medida do outro lado) e "por dono
#     desconhecido" (nem exceptions nem aliases conhecem este dono -
#     achado genuinamente novo).
#
# SCANCOUNT NUNCA ENTRA NESTAS CINCO SEÇÕES (mesmo achado, "dois
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
# itself already enforces one layer down), OR because "divergentes nao
# declaradas" plus "obrigatoria" together are nonzero (achado do CTO,
# segunda rodada: THIS is the real closing metric, never "divergentes
# em zero" - a declared, permanent environment divergence never fecha,
# and demanding it would get contorted around the first deadline). Turning an "obrigatoria" or "divergente nao declarada" row
# into a closed promise, a declared exception, or a tracked pending
# item is a WAVE-CLOSING decision, never something a CI script decides
# unattended - the same separation of "measures" from "judges" this
# project's own parity_exceptions.txt/parity_aliases.txt already keep
# between check_test_parity.py's mechanical union and the human
# decision of what belongs in either file.
#
# THE ONE THING THIS SCRIPT DOES VALIDATE (conserto do mesmo dia, run
# 34023787584): a per-KEY exception (tests/measured_exceptions.txt)
# whose own item is already CONCLUDED in TODO.md reproves - the same
# "concluido sem par" death rule tests/parity_exceptions.txt's own
# validate_exceptions() already enforces one file over, applied here
# because THIS file has no sibling gate to enforce it for - nobody
# else ever reads tests/measured_exceptions.txt.

import argparse
import re
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ci_systems  # noqa: E402

# GATE-ENV-SWEEP, categoria OUTPUT_ENCODING (TODO.md, mesmo remedio de
# tests/tools/check_test_parity.py, arquivo inteiro por declaracao -
# aquele script's own header comment explica por que): este script
# printa `status['status_text']` (validate_key_exception_deaths()) e
# valores MEASURED arbitrarios (build_table()/print_section()), que
# podem conter "✅" ou qualquer outro caractere fora de Latin-1 - print()
# em modo texto estrito quebra com UnicodeEncodeError num console
# Windows de codepage restrita sem este reconfigure.
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="backslashreplace")
if hasattr(sys.stderr, "reconfigure"):
    sys.stderr.reconfigure(encoding="utf-8", errors="backslashreplace")

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


_TODO_ROW_RE = re.compile(r"^\|.*\|$")


def parse_todo_status(todo_text):
    """Same 12-column parse as check_test_parity.py's own parse_todo_
    status_text() (ID at index 2, Status at index 9) - copied, not
    imported, the same WET choice tests/tools/check_plan_scope_diff.py
    already made one file over (GODS_LAWS.md L-17 "regra de 3": this
    is the second copy, not yet the third that would trigger
    extraction)."""
    status_by_item = {}
    for line in todo_text.splitlines():
        line = line.rstrip("\n")
        if not _TODO_ROW_RE.match(line.strip()):
            continue
        parts = line.split("|")
        if len(parts) != 12:
            continue
        item_id = parts[2].strip()
        status_text = parts[9].strip()
        if not item_id or item_id in ("ID", "---") or set(item_id) <= {"-"}:
            continue
        status_by_item[item_id] = {
            "status_text": status_text,
            "concluded": status_text.startswith("✅"),
        }
    return status_by_item


def validate_key_exception_deaths(key_exceptions, todo_status, todo_text=None):
    """Returns the list of death-rule error strings - a key exception
    whose OWN item is already CONCLUDED in TODO.md (never SEM-
    PENDENCIA, which by definition has no item to conclude) is the
    exact 'concluido sem par' shape tests/parity_exceptions.txt's own
    validate_exceptions() already catches one file over, applied here
    to a MEASURED key instead of a ctest name.

    A SECOND death, found by auditoria independente (GODS_LAWS.md
    L-36 do projeto, "portao que aceita ponteiro para o nada"): an
    item that does not exist AT ALL in TODO.md used to fall through
    this same `if status is not None` check without ever being
    flagged - `todo_status.get(item)` returned None, the concluded
    check never ran, and the exception was silently accepted as if it
    pointed somewhere real (LOOP-CAP30-KWIN-VIRTUAL, zero occurrences
    anywhere in TODO.md, is the case the audit measured).

    `todo_status` alone is NOT the full truth of "exists in TODO.md":
    it only indexes the pipe-table rows (parse_todo_status()'s own
    12-column parse), and this project's own TODO.md also carries a
    whole INBOX/"ausencia declarada" section of prose bullets - real,
    intentionally undecided items, referenced everywhere else in this
    file as a backtick-quoted id (`GL-GPU-KIND-HW-EVIDENCE`, `SONDA-
    OCULTA-PUNE-JANELA-VISIVEL`), never as a table row. Treating "not
    in the table" as "does not exist" would be a NEW false positive on
    every such item - measured live against this very file's own
    tests/measured_exceptions.txt while writing this fix (three keys
    citing `GL-GPU-KIND-HW-EVIDENCE`, a real, still-open prose item).
    So a key exception's item only dies here when it is absent from
    the table AND its backtick-quoted form is absent from the raw
    TODO.md text too - genuinely nowhere, not just outside the table.

    The sibling gate one file over (check_test_parity.py's own
    validate_exceptions()) already catches the table-only shape of
    this same death ('excecao cita item ..., que nao existe em
    TODO.md'); this function now catches the full shape, for a
    MEASURED key instead of a ctest name."""
    errors = []
    for key, (_lado, item, is_permanent) in key_exceptions.items():
        if is_permanent:
            continue
        status = todo_status.get(item)
        if status is not None:
            if status["concluded"]:
                errors.append(
                    f"{key}: excecao por chave aponta para o item {item!r}, marcado como "
                    f"CONCLUIDO ({status['status_text']}) em TODO.md - regra 'concluido sem "
                    "par': apague esta linha de tests/measured_exceptions.txt, o par que ela "
                    "promete ja deveria existir"
                )
            continue
        if todo_text is not None and f"`{item}`" in todo_text:
            # Real item, declared only in prose (INBOX or a similar
            # bullet section) - not a dangling pointer, and this
            # script has no "concluded" signal for prose, so it is
            # left alone rather than guessed at.
            continue
        errors.append(
            f"{key}: excecao por chave cita item {item!r}, que nao existe em TODO.md (nem na "
            "tabela nem como `id` entre crases em prosa) - ponteiro para o nada: corrija o "
            "item para o dono real ou marque item=SEM-PENDENCIA se a ausencia for permanente"
        )
    return errors


def parse_key_exceptions(text):
    """Returns {key: (lado, item, is_permanent)} from tests/measured_
    exceptions.txt's own `chave|lado|razao|item` lines - a THIRD file,
    distinct from tests/parity_exceptions.txt (whose `nome|plataforma|
    razao|item` shape is check_test_parity.py's own contract: item=
    SEM-PENDENCIA there REQUIRES the par field to read "nenhum", never
    a prose reason - reusing that file for a per-KEY exception is
    exactly what reproved check_test_parity.py on run 34023787584).
    `lado` is "linux"/"windows" (the side this key is expected ABSENT
    from) or "ambos" (achado do CTO, 06/09/2026: the key IS measured
    on both sides, and the exception declares the DIVERGENCE itself as
    an environment fact, never a library defect - seat_test.pointer's
    own case, one compositor announcing a pointer the other does not).
    This file's own four columns carry a MEASURED key, never a ctest
    name, and only this script ever reads it."""
    exceptions = {}
    for raw_line in text.splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split("|")
        if len(parts) != 4:
            continue
        key, lado, _reason, item = (part.strip() for part in parts)
        exceptions[key] = (lado, item, item == SEM_PENDENCIA)
    return exceptions


def classify_unilateral(key, key_exceptions, exception_owners, alias_owners):
    """Returns (section, detail) for a key present on exactly one
    side - `section` is "herdada" or "obrigatoria", `detail` is the
    human-readable reason this script's own header comment names.

    Checks `key_exceptions` (tests/measured_exceptions.txt, a specific
    MEASURED key) BEFORE falling back to `exception_owners` (tests/
    parity_exceptions.txt, a whole test) - this is what lets a SINGLE
    measured fact (never the whole test) be declared permanent, e.g.
    win32_window_close_request_test.wm_size_during_create's own
    sentinel value (src/platform/win32/display_adapter.cpp's own
    header comment): the TEST itself already has a declared pair
    (tests/parity_aliases.txt), but this ONE fact - a Windows message-
    pump timing detail with no Wayland equivalent even in principle -
    is what actually needs the permanent declaration, not the test."""
    if key in key_exceptions:
        _lado, item, is_permanent = key_exceptions[key]
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


def classify_divergent(key, key_exceptions):
    """Returns (is_declared, detail) for a key measured on BOTH sides
    with DIFFERENT values - declared only when tests/measured_
    exceptions.txt names this exact key with lado="ambos" (achado do
    CTO, 06/09/2026: a divergence declared for any OTHER lado value
    would be a contradiction - "linux"/"windows" mean "absent from
    that side", not "present and different" - so only "ambos" ever
    grants this classification)."""
    if key in key_exceptions:
        lado, item, is_permanent = key_exceptions[key]
        if lado == "ambos":
            if is_permanent:
                return True, "permanente (SEM-PENDENCIA)"
            return True, f"item {item}"
    return False, ""


def build_table(linux_values, windows_values, key_exceptions, exception_owners, alias_owners):
    """Returns (iguais, divergentes_nao_declaradas, divergentes_
    declaradas, herdada, obrigatoria) as sorted lists of (key,
    linux_repr, windows_repr[, detail]) - `windows_repr`/`linux_repr`
    is "-" when that side never measured the key at all.

    CINCO SEÇÕES, nao quatro (achado do CTO, 06/09/2026, item 3 do
    conserto - "divergentes em zero" era regra impossivel de cumprir:
    uma divergencia de AMBIENTE, como seat_test.pointer's own 0-vs-1,
    e' permanente por natureza, entao a metrica de fechamento passa a
    contar divergentes NAO DECLARADAS, nunca toda divergencia)."""
    all_keys = sorted(set(linux_values) | set(windows_values))
    iguais, divergentes_nao_declaradas, divergentes_declaradas = [], [], []
    herdada, obrigatoria = [], []
    for key in all_keys:
        on_linux = key in linux_values
        on_windows = key in windows_values
        linux_repr = ",".join(sorted(linux_values.get(key, []))) if on_linux else "-"
        windows_repr = ",".join(sorted(windows_values.get(key, []))) if on_windows else "-"
        if on_linux and on_windows:
            if linux_values[key] == windows_values[key]:
                iguais.append((key, linux_repr, windows_repr))
                continue
            is_declared, detail = classify_divergent(key, key_exceptions)
            if is_declared:
                divergentes_declaradas.append((key, linux_repr, windows_repr, detail))
            else:
                divergentes_nao_declaradas.append((key, linux_repr, windows_repr))
            continue
        section, detail = classify_unilateral(key, key_exceptions, exception_owners, alias_owners)
        row = (key, linux_repr, windows_repr, detail)
        (herdada if section == "herdada" else obrigatoria).append(row)
    return iguais, divergentes_nao_declaradas, divergentes_declaradas, herdada, obrigatoria


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
    parser.add_argument("--measured-exceptions", default=None, help="tests/measured_exceptions.txt")
    parser.add_argument("--todo", default=None, help="TODO.md - exigido junto de --measured-exceptions, para a regra de morte")
    parsed = parser.parse_args(args)

    if not parsed.linux and not parsed.windows:
        fail("usage: check_measured_parity.py --compare --linux <f1> [<f2> ...] --windows <f1> [<f2> ...] [--exceptions <f>] [--aliases <f>] [--measured-exceptions <f>]")

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
    key_exceptions = {}
    if parsed.measured_exceptions is not None:
        with open(parsed.measured_exceptions, "r", encoding="utf-8") as handle:
            key_exceptions = parse_key_exceptions(handle.read())
        if parsed.todo is not None:
            with open(parsed.todo, "r", encoding="utf-8") as handle:
                todo_text = handle.read()
            todo_status = parse_todo_status(todo_text)
            death_errors = validate_key_exception_deaths(key_exceptions, todo_status, todo_text)
            if death_errors:
                fail(
                    f"{len(death_errors)} excecao(oes) por chave morta(s) (tests/measured_"
                    "exceptions.txt):\n  " + "\n  ".join(death_errors)
                )

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

    iguais, divergentes_nao_declaradas, divergentes_declaradas, herdada, obrigatoria = build_table(
        linux_values, windows_values, key_exceptions, exception_owners, alias_owners
    )
    print_section("MEASURED - iguais", iguais)
    print_section("MEASURED - divergentes nao declaradas", divergentes_nao_declaradas)
    print_section("MEASURED - divergentes declaradas", divergentes_declaradas, with_detail=True)
    print_section("MEASURED - herdada", herdada, with_detail=True)
    print_section("MEASURED - obrigatoria", obrigatoria, with_detail=True)
    # A metrica de fechamento (achado do CTO, 06/09/2026, item 3 do
    # conserto): "divergentes em zero" era regra impossivel - uma
    # divergencia de ambiente e' permanente por natureza, nunca fecha.
    # A metrica certa e' divergentes NAO DECLARADAS mais unilaterais
    # obrigatorias, as duas juntas em zero no fechamento da onda.
    print(
        f"{SCRIPT_NAME}: {len(iguais)} igual(is), {len(divergentes_nao_declaradas)} "
        f"divergente(s) nao declarada(s), {len(divergentes_declaradas)} divergente(s) "
        f"declarada(s), {len(herdada)} herdada(s), {len(obrigatoria)} obrigatoria(s)"
    )


# --- textual mode (PARITY-LOCAL-MIRROR I2, TODO.md W7-C, GODS_LAWS.md ---
# L-04/L-17/L-40) -----------------------------------------------------
#
# O gemeo de check_test_parity.py's own textual_main() (mesma familia
# de defeito, achado N4 do adendo, docs/plano-w7c-adendo-revalidacao.md
# SS3.I): a regra "excecao por chave aponta para item concluido/
# inexistente reprova" (validate_key_exception_deaths() acima) so'
# mordia dentro de real_main(), e SO quando chamado com --linux/
# --windows tambem (real_main() exige pelo menos um dos dois antes de
# sequer olhar --measured-exceptions/--todo) - nada local chamava isto
# contra dado real, do mesmo jeito que N3 ja tinha medido para
# tests/tools/check_test_parity.py um arquivo antes.
#
# --textual roda SO a parte que nao depende de MEASURED nenhum: a
# FORMA de cada linha de tests/measured_exceptions.txt
# (parse_key_exceptions(), reusada sem copiar) e o item de cada
# excecao por chave contra TODO.md - tabela E prosa, a checagem MAIS
# forte que este arquivo ja tinha (validate_key_exception_deaths()'s
# own docstring acima: pega tambem o ponteiro para o nada que
# check_test_parity.py's own validate_exceptions() nao cobre).
def _parse_textual_main_args(args):
    if len(args) != 2:
        fail("usage: check_measured_parity.py --textual <measured_exceptions.txt> <TODO.md>")
    return args


def textual_main(args):
    exceptions_path, todo_path = _parse_textual_main_args(args)
    with open(exceptions_path, "r", encoding="utf-8") as handle:
        key_exceptions = parse_key_exceptions(handle.read())
    with open(todo_path, "r", encoding="utf-8") as handle:
        todo_text = handle.read()
    todo_status = parse_todo_status(todo_text)

    print(
        f"{SCRIPT_NAME} --textual: {len(key_exceptions)} excecao(oes) por chave, "
        f"{len(todo_status)} item(ns) lido(s) de TODO.md"
    )

    # GODS_LAWS.md L-40, o mesmo piso que o gemeo em check_test_parity.py
    # aplica: TODO.md tem centenas de linhas de item em toda execucao
    # real deste portao - ler zero e sinal de leitura quebrada, nunca de
    # tabela genuinamente vazia. tests/measured_exceptions.txt, ao
    # contrario, pode legitimamente ter zero excecoes - so o TODO.md
    # entra no piso.
    if not todo_status:
        fail(
            "varredura vazia: TODO.md nao tem item de tabela nenhum reconhecido - "
            "GODS_LAWS.md L-40, isto e sinal de leitura quebrada, nunca de tabela vazia"
        )

    death_errors = validate_key_exception_deaths(key_exceptions, todo_status, todo_text)
    if death_errors:
        fail(
            f"{len(death_errors)} excecao(oes) por chave morta(s) (tests/measured_"
            "exceptions.txt):\n  " + "\n  ".join(death_errors)
        )
    print(
        f"{SCRIPT_NAME}: modo --textual OK - nenhuma excecao por chave aponta para item "
        "concluido ou inexistente (lacuna de MEASURED nao conferida aqui, so no servidor)"
    )


# --- per-system mode (CI-SPLIT-PER-OS A1, docs/plano-ci-split-per-os.md ---
# secao 4.3: "check_measured_parity.py (idem)") ---------------------------
#
# "Idem" aqui e' a mesma ideia de check_test_parity.py's own --per-
# system, ESCOPO REDUZIDO ao que generaliza sem desenho novo: --compare
# (real_main() acima) so' conhece DOIS lados (Linux uniao, Windows), e
# uma chave MEASURED que falta so' numa distro fica encoberta pela
# uniao Linux, do mesmo jeito que check_test_parity.py's --compare
# encobria uma lacuna so'-no-CachyOS antes da A1.
#
# LIMITACAO DECLARADA (GODS_LAWS.md L-40, mesmo principio do "WHAT THIS
# SCRIPT DOES NOT DO" no topo do arquivo): --per-system SO classifica
# PRESENCA por sistema (herdada/obrigatoria, reusando classify_
# unilateral() sem copiar nem alterar - a funcao ja e' agnostica de
# "qual lado", so' recebe uma chave e as duas tabelas de dono). Ele NAO
# faz a comparacao de VALOR (iguais/divergentes) entre N sistemas: essa
# generalizacao (o que "divergente" significa quando 4 de 5 sistemas
# concordam e um diverge) e' desenho que este item nao especificou, e
# escreve-la aqui seria inventar critério sem CTO - a mesma classe de
# risco que GODS_LAWS.md L-01/L-34 pede que va ao planejamento antes.
# --measured-exceptions/--todo (a regra de morte por chave) tambem
# ficam de fora deste modo por hoje, pela mesma razao. Igual ao modo
# --compare, --per-system e' um RELATORIO (nunca falha por conteudo
# divergente ou unilateral) - so' falha pelo piso de varredura vazia
# (GODS_LAWS.md L-40), generalizado por sistema.
#
# CI-SPLIT-PER-OS A3a (D-A12): fonte UNICA de sistemas, tools/ci/
# systems.txt via ci_systems.py - esta era uma copia INDEPENDENTE,
# redigitada a mao, da mesma lista que check_test_parity.py ja tinha
# (SISTEMAS_PER_SYSTEM_ESPERADOS) - nenhuma das duas lia a outra.
#
# D1/D2 (achados do main, mesmo conserto de check_test_parity.py):
# carga LAZY (nunca no import - D1, prende --selftest ao arquivo real)
# e resolvida a partir de Path(__file__) (nunca do cwd - D2, o ctest
# roda a partir do diretorio de build). set_ci_systems_override() troca
# a fonte por fixture, usado so' por --selftest.
_CI_SYSTEMS_CACHE = None
_CI_SYSTEMS_OVERRIDE = None


def set_ci_systems_override(systems):
    global _CI_SYSTEMS_OVERRIDE
    _CI_SYSTEMS_OVERRIDE = systems


def _load_real_ci_systems():
    global _CI_SYSTEMS_CACHE
    if _CI_SYSTEMS_CACHE is None:
        repo_root = Path(__file__).resolve().parents[2]
        systems_path = repo_root / "tools" / "ci" / "systems.txt"
        _CI_SYSTEMS_CACHE = ci_systems.load_systems(str(systems_path))
    return _CI_SYSTEMS_CACHE


def _current_ci_systems():
    if _CI_SYSTEMS_OVERRIDE is not None:
        return _CI_SYSTEMS_OVERRIDE
    return _load_real_ci_systems()


def _per_system_slugs_esperados():
    return ci_systems.all_slugs(_current_ci_systems())


def _parse_per_system_args(args):
    parser = argparse.ArgumentParser(prog=f"{SCRIPT_NAME} --per-system", add_help=False)
    parser.add_argument("systems", nargs="*")
    parser.add_argument("--exceptions", default=None, help="tests/parity_exceptions.txt")
    parser.add_argument("--aliases", default=None, help="tests/parity_aliases.txt")
    parser.add_argument("--measured-exceptions", default=None, help="tests/measured_exceptions.txt")
    parser.add_argument("--todo", default=None, help="TODO.md - exigido junto de --measured-exceptions")
    parsed = parser.parse_args(args)
    if not parsed.systems:
        fail(
            "usage: check_measured_parity.py --per-system <slug>=<arquivo> "
            "[<slug>=<arquivo> ...] [--exceptions <f>] [--aliases <f>] "
            "[--measured-exceptions <f>] [--todo <f>]"
        )
    system_paths = defaultdict(list)
    for entry in parsed.systems:
        if "=" not in entry:
            fail(f"--per-system: argumento malformado (esperava 'slug=arquivo'): {entry!r}")
        slug, path = entry.split("=", 1)
        if slug not in _per_system_slugs_esperados():
            fail(
                f"--per-system: slug de sistema invalido {slug!r} (validos: "
                f"{_per_system_slugs_esperados()})"
            )
        system_paths[slug].append(path)
    return dict(system_paths), parsed


def _load_per_system_owners(exceptions_path, aliases_path):
    exception_owners = {}
    if exceptions_path is not None:
        with open(exceptions_path, "r", encoding="utf-8") as handle:
            exception_owners = parse_exception_owners(handle.read())
    alias_owners = set()
    if aliases_path is not None:
        with open(aliases_path, "r", encoding="utf-8") as handle:
            alias_owners = parse_alias_owners(handle.read())
    return exception_owners, alias_owners


# GODS_LAWS.md L-04 item 2 + L-40 (piso de sistemas, gemeo do mesmo
# controle acrescentado a check_test_parity.py's own --per-system na
# mesma fatia): um sistema esperado que nunca recebeu slug=arquivo
# nenhum e' coleta quebrada silenciosa, nao "nada a reportar dele".
def _per_system_piso_errors(system_values):
    recebidos = set(system_values)
    esperados = set(_per_system_slugs_esperados())
    faltando = sorted(esperados - recebidos)
    errors = []
    if faltando:
        errors.append(
            "piso de sistemas (GODS_LAWS.md L-40): sistema(s) da lista de alvos sem "
            f"arquivo MEASURED nenhum recebido, nunca pulado(s) em silencio: {faltando}"
        )
    inesperados = sorted(recebidos - esperados)
    if inesperados:
        errors.append(
            f"sistema(s) fora da lista de alvos deste portao ({_per_system_slugs_esperados()}): "
            f"{inesperados} - amplie tools/ci/systems.txt antes de usar um slug novo"
        )
    if not any(system_values.values()):
        errors.append(
            "0 chave(s) MEASURED em QUALQUER sistema recebido - varredura vazia "
            "(GODS_LAWS.md L-40): a colheita de uma perna (ou de todas) esta quebrada, "
            "nunca 'nada para comparar'"
        )
    return errors


def compute_per_system_unilateral(system_values, exception_owners, alias_owners):
    """Retorna (uniao_de_chaves, classificacao) - `classificacao` e'
    {slug: [(chave, secao, motivo), ...]} para toda chave que falta
    NESSE sistema mas existe em algum outro - `secao` e' "herdada" ou
    "obrigatoria" (classify_unilateral(), reusada sem copiar)."""
    uniao_chaves = set()
    for valores in system_values.values():
        uniao_chaves |= set(valores)
    classificacao = {}
    for slug, valores in system_values.items():
        faltando = sorted(uniao_chaves - set(valores))
        classificacao[slug] = [
            (chave, *classify_unilateral(chave, {}, exception_owners, alias_owners)) for chave in faltando
        ]
    return uniao_chaves, classificacao


def _print_per_system_report(system_values, classificacao):
    for slug in sorted(system_values):
        print(f"{SCRIPT_NAME} --per-system: sistema {slug!r} = {len(system_values[slug])} chave(s) MEASURED")
    print(f"## MEASURED --per-system - obrigatoria/herdada por sistema")
    for slug in sorted(classificacao):
        linhas = classificacao[slug]
        obrigatoria = [row for row in linhas if row[1] == "obrigatoria"]
        herdada = [row for row in linhas if row[1] == "herdada"]
        print(
            f"### {slug}: {len(linhas)} chave(s) ausente(s) ({len(obrigatoria)} obrigatoria(s), "
            f"{len(herdada)} herdada(s))"
        )
        for chave, secao, motivo in linhas:
            print(f"  [{secao.upper()}] {chave} ({motivo})")


# --- CI-SPLIT-PER-OS A1b (D-A10): valor MEASURED entre N sistemas, --------
# por UNANIMIDADE, sem arbitro nem maioria --------------------------------
#
# D-A10 (docs/plano-ci-split-per-os.md secao 4.3, decisao do CTO,
# 24/09/2026): uma chave e' "igual" so' se TODOS os sistemas que a medem
# dao o MESMO valor; qualquer diferenca e' divergencia; nao ha sistema
# de referencia (nem Fedora, nem maioria) - a razao completa (por que
# maioria e por que Fedora foram recusados) mora no proprio plano, nao
# se repete aqui.
#
# LADO ganha vocabulario fechado (troca "ambos"): "todos" (o valor pode
# diferir entre QUAISQUER sistemas E dentro de um sistema - metrica do
# executor); "familias" (os sistemas Linux tem de concordar ENTRE SI; so'
# Linux x Windows pode diferir; NAO cobre instabilidade dentro de um
# sistema - so' "todos" cobre isso); "linux"/"windows"/slug/lista de
# slugs (auséncia esperada, forma que ja' existia para o caso unilateral -
# nao e' consumida por classify_unilateral(), que so' olha presenca no
# dict, entao nao muda). "ambos" reprova como formato invalido.
LADO_DIVERGENCE_KEYWORDS = ("todos", "familias")

# CI-SPLIT-PER-OS A3a (D-A12): derivado de _current_ci_systems(),
# nunca mais redigitado a mao (era a segunda copia independente da
# mesma regra que _familia_por_slug()["linux"] ja calcula em
# check_test_parity.py). Funcao, nunca constante - mesmo motivo D1 de
# _per_system_slugs_esperados() acima.
def _linux_family_slugs():
    return ci_systems.slugs_of_family(_current_ci_systems(), "linux")


def is_valid_lado_format(lado):
    """Fechado: "todos", "familias", ou uma lista separada por virgula
    de slugs/"linux"/"windows" - nunca "ambos" (M4: a migracao inteira
    deste item existe para isso reprovar)."""
    if lado in LADO_DIVERGENCE_KEYWORDS:
        return True
    tokens = [t.strip() for t in lado.split(",")]
    valid_refs = set(ci_systems.missing_on_vocabulary(_current_ci_systems()))
    return bool(tokens) and all(t and t in valid_refs for t in tokens)


def _format_lado_errors(key_exceptions):
    return [
        f"{key}: lado {lado!r} fora do vocabulario fechado (tests/measured_exceptions.txt) - "
        f"validos: {LADO_DIVERGENCE_KEYWORDS} ou lista separada por virgula de "
        f"{sorted(ci_systems.missing_on_vocabulary(_current_ci_systems()))} - 'ambos' nao existe mais (D-A10)"
        for key, (lado, _item, _perm) in key_exceptions.items()
        if not is_valid_lado_format(lado)
    ]


def compute_value_partition(key, system_values):
    """Retorna (particao, instaveis) - particao = {valor: [slugs]} SO'
    com sistemas cujo conjunto de valores para `key` tem EXATAMENTE 1
    elemento (M2: nunca escolhe um sistema de referencia, cada sistema
    entra por conta propria); instaveis = slugs cujo conjunto tem 2+
    valores DISTINTOS - a mesma chave, pernas diferentes do MESMO
    sistema, discordando entre si (M5: nunca colapsada escolhendo uma)."""
    particao = defaultdict(list)
    instaveis = []
    for slug, valores_por_chave in system_values.items():
        valores = valores_por_chave.get(key, set())
        if len(valores) >= 2:
            instaveis.append(slug)
        elif len(valores) == 1:
            particao[next(iter(valores))].append(slug)
    return dict(particao), sorted(instaveis)


def is_value_divergence_declared(lado, particao):
    """M1 (voto de maioria proibido): nao ha contagem de votos aqui -
    so' checa SE ha mais de um grupo, nunca QUANTOS slugs cada grupo
    tem. M3 (familias != todos): com "familias", dois grupos que
    incluem slug linux CADA UM (ubuntu num grupo, fedora noutro, por
    exemplo) sao NAO declarados - so' Linux x Windows pode divergir."""
    if lado == "todos":
        return True
    if lado == "familias":
        linux_groups = [valor for valor, slugs in particao.items() if any(s in _linux_family_slugs() for s in slugs)]
        return len(linux_groups) <= 1
    return False


def is_instability_declared(lado):
    """So' "familias" bullet 2 do plano: "todos" cobre divergencia
    "dentro de um sistema" explicitamente; "familias" nao menciona
    instabilidade nenhuma - fica sempre nao declarada."""
    return lado == "todos"


def classify_key_value(key, system_values, key_exceptions):
    """Retorna um dict com a classificacao completa desta chave -
    'categoria' e' iguais/divergentes/instaveis_apenas (chave so' com
    sistema(s) instavel(is), sem par estavel pra comparar), 'particao'
    e 'instaveis' vem de compute_value_partition(), 'lado'/'declarada'
    dizem se a divergencia (quando existir) esta coberta."""
    particao, instaveis = compute_value_partition(key, system_values)
    lado = key_exceptions.get(key, (None, None, None))[0]
    if len(particao) <= 1:
        categoria = "iguais" if particao else "instaveis_apenas"
    else:
        categoria = "divergentes"
    return {
        "categoria": categoria,
        "particao": particao,
        "instaveis": instaveis,
        "lado": lado,
        "divergencia_declarada": is_value_divergence_declared(lado, particao) if categoria == "divergentes" else None,
        "instabilidade_declarada": is_instability_declared(lado) if instaveis else None,
    }


def compute_per_system_value_comparison(uniao_chaves, system_values, key_exceptions):
    """Chaves presentes em MENOS de 2 sistemas nao entram aqui - nao ha
    o que comparar (ja cobertas inteiramente por herdada/obrigatoria)."""
    resultado = {}
    for chave in sorted(uniao_chaves):
        presencas = sum(1 for valores in system_values.values() if chave in valores)
        if presencas < 2:
            continue
        resultado[chave] = classify_key_value(chave, system_values, key_exceptions)
    return resultado


def _group_value_comparison(comparacao):
    """Retorna os cinco grupos que a secao imprime - separados aqui de
    _print_per_system_value_report() (que so' imprime) pela mesma regra
    de GODS_LAWS.md L-17 que ja divide o resto deste arquivo."""
    iguais = {k: c for k, c in comparacao.items() if c["categoria"] == "iguais"}
    divergentes = {k: c for k, c in comparacao.items() if c["categoria"] == "divergentes"}
    div_decl = {k: c for k, c in divergentes.items() if c["divergencia_declarada"]}
    div_nao = {k: c for k, c in divergentes.items() if not c["divergencia_declarada"]}
    inst_decl, inst_nao = [], []
    for chave, c in comparacao.items():
        alvo_par = inst_decl if c["instabilidade_declarada"] else inst_nao
        for slug in c["instaveis"]:
            alvo_par.append((chave, slug))
    return {
        "iguais": iguais,
        "divergentes_declaradas": div_decl,
        "divergentes_nao_declaradas": div_nao,
        "instaveis_declaradas": inst_decl,
        "instaveis_nao_declaradas": inst_nao,
    }


def _format_particao(particao):
    return " | ".join(f"{valor}: {','.join(sorted(slugs))}" for valor, slugs in sorted(particao.items()))


def _print_per_system_value_report(grupos):
    print("## MEASURED --per-system - valor por unanimidade (D-A10)")
    print(f"### iguais ({len(grupos['iguais'])})")
    print(f"### divergentes declaradas ({len(grupos['divergentes_declaradas'])})")
    for chave, c in sorted(grupos["divergentes_declaradas"].items()):
        print(f"  {chave}: {_format_particao(c['particao'])} (lado={c['lado']})")
    print(f"### divergentes NAO declaradas ({len(grupos['divergentes_nao_declaradas'])})")
    for chave, c in sorted(grupos["divergentes_nao_declaradas"].items()):
        print(f"  {chave}: {_format_particao(c['particao'])}")
    print(f"### instaveis declaradas ({len(grupos['instaveis_declaradas'])})")
    for chave, slug in sorted(grupos["instaveis_declaradas"]):
        print(f"  instavel no sistema {slug}: {chave}")
    print(f"### instaveis NAO declaradas ({len(grupos['instaveis_nao_declaradas'])})")
    for chave, slug in sorted(grupos["instaveis_nao_declaradas"]):
        print(f"  instavel no sistema {slug}: {chave}")


# GODS_LAWS.md L-40: linha de escopo SEMPRE impressa, mesmo com tudo
# zerado - "chaves: N" e' o tamanho da uniao inteira (nao so' as >=2
# sistemas que entram na comparacao de valor), "sistemas por chave" e'
# o min/max de cobertura entre TODAS as chaves da uniao.
def _print_value_scope_line(uniao_chaves, system_values, grupos, classificacao_presenca):
    contagens = [sum(1 for valores in system_values.values() if chave in valores) for chave in uniao_chaves]
    minimo = min(contagens) if contagens else 0
    maximo = max(contagens) if contagens else 0
    total_obrigatoria = sum(1 for linhas in classificacao_presenca.values() for row in linhas if row[1] == "obrigatoria")
    total_herdada = sum(1 for linhas in classificacao_presenca.values() for row in linhas if row[1] == "herdada")
    total_divergentes = len(grupos["divergentes_declaradas"]) + len(grupos["divergentes_nao_declaradas"])
    total_instaveis = len(grupos["instaveis_declaradas"]) + len(grupos["instaveis_nao_declaradas"])
    print(
        f"{SCRIPT_NAME} --per-system: chaves: {len(uniao_chaves)}; sistemas por chave: "
        f"{minimo}..{maximo}; iguais/divergentes/instaveis/herdada/obrigatoria: "
        f"{len(grupos['iguais'])}/{total_divergentes}/{total_instaveis}/{total_herdada}/{total_obrigatoria}"
    )


def _load_per_system_measured_exceptions(measured_exceptions_path, todo_path):
    """None quando --measured-exceptions nao foi dado - o modo por
    valor fica desligado (so' presenca), nunca calado sobre o motivo:
    per_system_main() avisa explicitamente quando isso acontece."""
    if measured_exceptions_path is None:
        return None, {}, ""
    with open(measured_exceptions_path, "r", encoding="utf-8") as handle:
        key_exceptions = parse_key_exceptions(handle.read())
    todo_status, todo_text = {}, ""
    if todo_path is not None:
        with open(todo_path, "r", encoding="utf-8") as handle:
            todo_text = handle.read()
        todo_status = parse_todo_status(todo_text)
    return key_exceptions, todo_status, todo_text


def _run_value_mode(uniao, system_values, classificacao_presenca, parsed):
    """A metade de per_system_main() que so' roda quando --measured-
    exceptions foi dado - extraida so' para caber no teto de 40 linhas
    de GODS_LAWS.md L-17, nao por responsabilidade nova."""
    key_exceptions, todo_status, todo_text = _load_per_system_measured_exceptions(
        parsed.measured_exceptions, parsed.todo
    )
    lado_errors = _format_lado_errors(key_exceptions)
    if lado_errors:
        fail(f"{len(lado_errors)} lado(s) invalido(s) em tests/measured_exceptions.txt:\n  " + "\n  ".join(lado_errors))
    death_errors = validate_key_exception_deaths(key_exceptions, todo_status, todo_text)
    if death_errors:
        fail(
            f"{len(death_errors)} excecao(oes) por chave morta(s) (tests/measured_"
            "exceptions.txt):\n  " + "\n  ".join(death_errors)
        )
    comparacao = compute_per_system_value_comparison(uniao, system_values, key_exceptions)
    grupos = _group_value_comparison(comparacao)
    _print_per_system_value_report(grupos)
    _print_value_scope_line(uniao, system_values, grupos, classificacao_presenca)
    print(
        f"{SCRIPT_NAME} --per-system: relatorio, nunca falha por divergencia/instabilidade/"
        "obrigatoria/herdada (so' pelo piso de varredura vazia e pela regra de morte por chave) "
        "- fechar e' decisao da onda (D-A10)"
    )


def per_system_main(args):
    system_paths, parsed = _parse_per_system_args(args)
    system_values = {slug: parse_measured_files(paths)[0] for slug, paths in system_paths.items()}
    exception_owners, alias_owners = _load_per_system_owners(parsed.exceptions, parsed.aliases)

    errors = _per_system_piso_errors(system_values)
    if errors:
        fail(f"{len(errors)} problema(s) de piso (--per-system):\n  " + "\n  ".join(errors))

    uniao, classificacao_presenca = compute_per_system_unilateral(system_values, exception_owners, alias_owners)
    _print_per_system_report(system_values, classificacao_presenca)

    if parsed.measured_exceptions is None:
        print(
            f"{SCRIPT_NAME} --per-system: --measured-exceptions nao foi dado - secao de VALOR "
            "(iguais/divergentes/instaveis, D-A10) desligada, so' presenca (herdada/obrigatoria) acima"
        )
        return
    _run_value_mode(uniao, system_values, classificacao_presenca, parsed)


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


def selftest_five_sections_classify_correctly(tmp_path):
    """The cases this fatia's own briefing names, one row each: iguais,
    divergente NAO declarada, divergente DECLARADA (lado="ambos"),
    herdada com item, herdada permanente, obrigatoria por par
    existente, obrigatoria por dono desconhecido."""
    linux_file = _write_temp(
        tmp_path,
        "linux5.txt",
        "MEASURED window_parity_test.active_after_open=0\n"
        "MEASURED window_parity_test.logical_width=800\n"
        "MEASURED seat_test.pointer=0\n"
        "MEASURED tem_item_test.some_key=1\n"
        "MEASURED tem_permanente_test.other_key=2\n"
        "MEASURED tem_par_test.paired_key=3\n"
        "MEASURED dono_desconhecido_test.mystery_key=4\n",
    )
    windows_file = _write_temp(
        tmp_path,
        "windows5.txt",
        "MEASURED window_parity_test.active_after_open=0\n"
        "MEASURED window_parity_test.logical_width=0\n"
        "MEASURED seat_test.pointer=1\n",
    )
    exceptions_file = _write_temp(
        tmp_path,
        "exceptions5.txt",
        "tem_item_test|linux|razao qualquer|ITEM-ABERTO\n"
        "tem_permanente_test|linux|razao permanente|SEM-PENDENCIA\n",
    )
    aliases_file = _write_temp(tmp_path, "aliases5.txt", "tem_par_test|tem_par_test_win\n")
    key_exceptions_file = _write_temp(
        tmp_path,
        "measured_exc5.txt",
        "seat_test.pointer|ambos|presenca de dispositivo do executor, provado por outro teste|SEM-PENDENCIA\n",
    )
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
                    "--measured-exceptions",
                    key_exceptions_file,
                ]
            )
    except SystemExit as exc:
        print(f"selftest: caso dos cinco baldes reprovou inesperadamente (codigo {exc.code})",
              file=sys.stderr)
        return False
    output = buffer.getvalue()
    try:
        iguais_block = output.split("MEASURED - iguais")[1].split("MEASURED - divergentes nao")[0]
        nao_declaradas_block = output.split("MEASURED - divergentes nao declaradas")[1].split(
            "MEASURED - divergentes declaradas"
        )[0]
        declaradas_block = output.split("MEASURED - divergentes declaradas")[1].split(
            "MEASURED - herdada"
        )[0]
        herdada_block = output.split("MEASURED - herdada")[1].split("MEASURED - obrigatoria")[0]
        obrigatoria_block = output.split("MEASURED - obrigatoria")[1]
    except IndexError:
        print(f"selftest: nao achei as cinco secoes na saida:\n{output}", file=sys.stderr)
        return False
    checks = [
        "active_after_open" in iguais_block,
        "logical_width" in nao_declaradas_block,
        "seat_test.pointer" in declaradas_block and "permanente" in declaradas_block,
        "tem_item_test" in herdada_block and "ITEM-ABERTO" in herdada_block,
        "tem_permanente_test" in herdada_block and "permanente" in herdada_block,
        "tem_par_test" in obrigatoria_block and "par existente" in obrigatoria_block,
        "dono_desconhecido_test" in obrigatoria_block and "dono desconhecido" in obrigatoria_block,
    ]
    if not all(checks):
        print(f"selftest: classificacao dos cinco baldes saiu errada ({checks}):\n{output}",
              file=sys.stderr)
        return False
    print("selftest: iguais/divergentes-nao-declaradas/divergentes-declaradas/herdada/"
          "obrigatoria classificados corretamente - ok")
    return True


def selftest_declared_divergence_with_concluded_item_reproves(tmp_path):
    """The death rule, applied to a DECLARED DIVERGENCE (lado="ambos"):
    a key exception whose own item is already CONCLUDED in TODO.md
    reproves, even when it declares a divergence rather than a
    unilateral absence - validate_key_exception_deaths() does not care
    which section a key would land in, only whether its item is done."""
    linux_file = _write_temp(tmp_path, "linux_divdead.txt", "MEASURED seat_test.pointer=0\n")
    windows_file = _write_temp(tmp_path, "windows_divdead.txt", "MEASURED seat_test.pointer=1\n")
    key_exceptions_file = _write_temp(
        tmp_path,
        "measured_exc_divdead.txt",
        "seat_test.pointer|ambos|razao qualquer|ITEM-CONCLUIDO\n",
    )
    todo_file = _write_temp(
        tmp_path,
        "todo_divdead.md",
        "| WSJF | ID | Onda | Grupo | Descricao | Prioridade | Pre-requisito | Dificuldade | Status | Estado |\n"
        "|---|---|---|---|---|---|---|---|---|---|\n"
        "| 1.0 | ITEM-CONCLUIDO | W1 | X | y | Alta | - | Media | ✅ Concluído | - |\n",
    )
    try:
        real_main(
            [
                "--linux",
                linux_file,
                "--windows",
                windows_file,
                "--measured-exceptions",
                key_exceptions_file,
                "--todo",
                todo_file,
            ]
        )
    except SystemExit as exc:
        if exc.code == 1:
            print("selftest: divergencia declarada com item CONCLUIDO reprova (regra de morte) "
                  "- ok")
            return True
        print(f"selftest: codigo inesperado {exc.code} para divergencia declarada morta",
              file=sys.stderr)
        return False
    print("selftest: divergencia declarada com item concluido NAO reprovou - esperado exit 1",
          file=sys.stderr)
    return False


def selftest_scancount_never_compared(tmp_path):
    """A SCANCOUNT line collected on both sides is counted, never
    turned into a comparison row - this script's own header comment,
    'SCANCOUNT NUNCA ENTRA NESTAS CINCO SEÇÕES'."""
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


def selftest_key_exception_accepted(tmp_path):
    """A key declared in tests/measured_exceptions.txt (never tests/
    parity_exceptions.txt - that file's own contract requires par=
    'nenhum' for SEM-PENDENCIA, not a prose reason, the exact shape
    that reproved check_test_parity.py on run 34023787584 when this
    fatia's own key exceptions were written into the WRONG file) lands
    in 'herdada', never 'obrigatoria'."""
    linux_file = _write_temp(tmp_path, "linux_keyexc.txt", "MEASURED some_test.lonely_key=1\n")
    windows_file = _write_temp(tmp_path, "windows_keyexc_empty.txt", "")
    key_exceptions_file = _write_temp(
        tmp_path, "measured_exc.txt", "some_test.lonely_key|windows|razao qualquer|SEM-PENDENCIA\n"
    )
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
                    "--measured-exceptions",
                    key_exceptions_file,
                ]
            )
    except SystemExit as exc:
        print(f"selftest: excecao por chave reprovou inesperadamente (codigo {exc.code})",
              file=sys.stderr)
        return False
    output = buffer.getvalue()
    if "lonely_key" not in output.split("- herdada")[1].split("obrigatoria")[0]:
        print(f"selftest: chave com excecao por chave nao caiu em herdada:\n{output}",
              file=sys.stderr)
        return False
    print("selftest: excecao por chave (tests/measured_exceptions.txt) aceita, cai em herdada - ok")
    return True


def selftest_key_exception_with_concluded_item_reproves(tmp_path):
    """The death rule, applied to a PER-KEY exception: a key exception
    whose own item is already CONCLUDED in TODO.md reproves - the
    exact case this fatia's own briefing names ('exceção por chave com
    item concluído reprovando'), mirroring tests/parity_exceptions.
    txt's own validate_exceptions() one file over."""
    linux_file = _write_temp(tmp_path, "linux_keyexc2.txt", "MEASURED some_test.other_key=1\n")
    windows_file = _write_temp(tmp_path, "windows_keyexc2_empty.txt", "")
    key_exceptions_file = _write_temp(
        tmp_path, "measured_exc2.txt", "some_test.other_key|windows|razao qualquer|ITEM-CONCLUIDO\n"
    )
    todo_file = _write_temp(
        tmp_path,
        "todo_keyexc2.md",
        "| WSJF | ID | Onda | Grupo | Descricao | Prioridade | Pre-requisito | Dificuldade | Status | Estado |\n"
        "|---|---|---|---|---|---|---|---|---|---|\n"
        "| 1.0 | ITEM-CONCLUIDO | W1 | X | y | Alta | - | Media | ✅ Concluído | - |\n",
    )
    try:
        real_main(
            [
                "--linux",
                linux_file,
                "--windows",
                windows_file,
                "--measured-exceptions",
                key_exceptions_file,
                "--todo",
                todo_file,
            ]
        )
    except SystemExit as exc:
        if exc.code == 1:
            print("selftest: excecao por chave com item CONCLUIDO reprova (regra de morte) - ok")
            return True
        print(f"selftest: codigo inesperado {exc.code} para excecao por chave morta",
              file=sys.stderr)
        return False
    print("selftest: excecao por chave com item concluido NAO reprovou - esperado exit 1",
          file=sys.stderr)
    return False


def selftest_key_exception_with_nonexistent_item_reproves(tmp_path):
    """The gap the auditoria found live, against this project's own
    real files: a key exception whose item has ZERO occurrences
    anywhere in TODO.md (not in the table, not as a backtick-quoted
    id in prose) used to return `status = None` from `todo_status.
    get(item)` and fall straight through the old `if status is not
    None and status["concluded"]` guard, never flagged - exactly
    LOOP-CAP30-KWIN-VIRTUAL, measured absent from TODO.md entirely
    before this fix landed. A ponteiro para o nada must reprove."""
    linux_file = _write_temp(tmp_path, "linux_ghost.txt", "MEASURED some_test.ghost_key=1\n")
    windows_file = _write_temp(tmp_path, "windows_ghost_empty.txt", "")
    key_exceptions_file = _write_temp(
        tmp_path, "measured_exc_ghost.txt", "some_test.ghost_key|windows|razao qualquer|ITEM-FANTASMA\n"
    )
    todo_file = _write_temp(
        tmp_path,
        "todo_ghost.md",
        "| WSJF | ID | Onda | Grupo | Descricao | Prioridade | Pre-requisito | Dificuldade | Status | Estado |\n"
        "|---|---|---|---|---|---|---|---|---|---|\n"
        "| 1.0 | ITEM-REAL | W1 | X | y | Alta | - | Media | ⏳ Pendente | - |\n"
        "\n## INBOX\n\n- **`OUTRO-ITEM-REAL`: prosa qualquer, sem relacao com o fantasma.**\n",
    )
    try:
        real_main(
            [
                "--linux",
                linux_file,
                "--windows",
                windows_file,
                "--measured-exceptions",
                key_exceptions_file,
                "--todo",
                todo_file,
            ]
        )
    except SystemExit as exc:
        if exc.code == 1:
            print("selftest: excecao por chave com item INEXISTENTE (fora da tabela e fora da "
                  "prosa) reprova - ok")
            return True
        print(f"selftest: codigo inesperado {exc.code} para excecao por chave fantasma",
              file=sys.stderr)
        return False
    print("selftest: excecao por chave com item inexistente NAO reprovou - esperado exit 1 "
          "(este e o bug que a auditoria mediu: ponteiro para o nada aceito em silencio)",
          file=sys.stderr)
    return False


def selftest_key_exception_with_prose_only_item_accepted(tmp_path):
    """The false positive this exact fix would have introduced if it
    only checked the pipe-table: an item declared for real, but ONLY
    as a backtick-quoted id in a prose bullet (TODO.md's own INBOX
    section, or a similar 'ausencia declarada' block) - never a table
    row. Measured live while writing this fix: `GL-GPU-KIND-HW-
    EVIDENCE` is exactly this shape in the real TODO.md, cited by
    three real key exceptions. This item is NOT a dangling pointer and
    must not reprove, even though `parse_todo_status()` never indexes
    it."""
    linux_file = _write_temp(tmp_path, "linux_prose.txt", "MEASURED some_test.prose_key=1\n")
    windows_file = _write_temp(tmp_path, "windows_prose_empty.txt", "")
    key_exceptions_file = _write_temp(
        tmp_path, "measured_exc_prose.txt", "some_test.prose_key|windows|razao qualquer|ITEM-SO-EM-PROSA\n"
    )
    todo_file = _write_temp(
        tmp_path,
        "todo_prose.md",
        "| WSJF | ID | Onda | Grupo | Descricao | Prioridade | Pre-requisito | Dificuldade | Status | Estado |\n"
        "|---|---|---|---|---|---|---|---|---|---|\n"
        "| 1.0 | ITEM-REAL | W1 | X | y | Alta | - | Media | ⏳ Pendente | - |\n"
        "\n## INBOX\n\n- **`ITEM-SO-EM-PROSA`: achado real, ainda sem linha na tabela.**\n",
    )
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
                    "--measured-exceptions",
                    key_exceptions_file,
                    "--todo",
                    todo_file,
                ]
            )
    except SystemExit as exc:
        print(f"selftest: excecao por chave so-em-prosa reprovou por engano (codigo {exc.code}) "
              "- falso positivo, o item existe em TODO.md fora da tabela", file=sys.stderr)
        return False
    print("selftest: excecao por chave citando item real declarado so em prosa (INBOX) e' "
          "aceita, nunca tratada como ponteiro para o nada - ok")
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


def _todo_textual_fixture(item_id, status_text):
    return (
        "| WSJF | ID | Onda | Grupo | Descricao | Prioridade | Pre-requisito | Dificuldade | "
        "Status | Estado |\n"
        "|---|---|---|---|---|---|---|---|---|---|\n"
        f"| 1.0 | {item_id} | W1 | X | y | Alta | - | Media | {status_text} | - |\n"
    )


def _run_textual_main_capturing(tmp_path, exceptions_text, todo_text):
    import contextlib
    import io

    exceptions_file = _write_temp(tmp_path, "textual_measured_exceptions.txt", exceptions_text)
    todo_file = _write_temp(tmp_path, "textual_TODO.md", todo_text)
    buffer = io.StringIO()
    exit_code = None
    with contextlib.redirect_stdout(buffer), contextlib.redirect_stderr(buffer):
        try:
            textual_main([exceptions_file, todo_file])
        except SystemExit as exc:
            exit_code = exc.code
    return exit_code, buffer.getvalue()


# Controle POSITIVO: excecao por chave apontando para item PENDENTE -
# passa, e o piso L-40 imprime as duas contagens.
def selftest_textual_positive_control(tmp_path):
    exit_code, output = _run_textual_main_capturing(
        tmp_path,
        "algum_teste.alguma_chave|windows|motivo qualquer aqui|ITEM-A\n",
        _todo_textual_fixture("ITEM-A", "⏳ Pendente"),
    )
    if exit_code not in (None, 0):
        print(
            f"selftest: TEXTUAL-POSITIVO FALHOU (esperava sucesso, saiu com codigo {exit_code!r}): "
            f"{output}",
            file=sys.stderr,
        )
        return False
    if "1 excecao(oes) por chave, 1 item(ns) lido(s) de TODO.md" not in output:
        print(
            f"selftest: TEXTUAL-POSITIVO FALHOU (piso L-40 nao imprimiu as duas contagens): "
            f"{output!r}",
            file=sys.stderr,
        )
        return False
    print("selftest: TEXTUAL-POSITIVO OK (modo --textual passa com excecao por chave pendente)")
    return True


# O VERMELHO central desta sub-fatia (o gemeo de I1): excecao por
# chave apontando para item ja CONCLUIDO reprova no modo --textual.
# Mutante que mata: pular a chamada a validate_key_exception_deaths()
# dentro de textual_main() - sem ela este controle passaria calado.
def selftest_textual_concluded_item_reproves(tmp_path):
    exit_code, output = _run_textual_main_capturing(
        tmp_path,
        "algum_teste.alguma_chave|windows|motivo qualquer aqui|ITEM-CONCLUIDO\n",
        _todo_textual_fixture("ITEM-CONCLUIDO", "✅ Concluído"),
    )
    if exit_code != 1:
        print(
            f"selftest: TEXTUAL-VERMELHO FALHOU (esperava reprovar com codigo 1, veio "
            f"{exit_code!r}): {output}",
            file=sys.stderr,
        )
        return False
    if "algum_teste.alguma_chave" not in output or "ITEM-CONCLUIDO" not in output:
        print(
            f"selftest: TEXTUAL-VERMELHO FALHOU (reprovou, mas nao citou a chave/item): "
            f"{output!r}",
            file=sys.stderr,
        )
        return False
    print(
        "selftest: TEXTUAL-VERMELHO OK (excecao por chave apontando para item concluido "
        "reprova no modo --textual)"
    )
    return True


# O gemeo do ponteiro-para-o-nada (validate_key_exception_deaths()'s
# own SECOND death, mais forte que a de check_test_parity.py): item
# que nao existe NEM na tabela NEM em prosa reprova no modo --textual.
def selftest_textual_nonexistent_item_reproves(tmp_path):
    exit_code, output = _run_textual_main_capturing(
        tmp_path,
        "algum_teste.alguma_chave|windows|motivo qualquer aqui|ITEM-FANTASMA\n",
        _todo_textual_fixture("OUTRO-ITEM", "⏳ Pendente"),
    )
    if exit_code != 1:
        print(
            f"selftest: TEXTUAL-PONTEIRO-PARA-O-NADA FALHOU (esperava reprovar com codigo 1, "
            f"veio {exit_code!r}): {output}",
            file=sys.stderr,
        )
        return False
    print(
        "selftest: TEXTUAL-PONTEIRO-PARA-O-NADA OK (item que nao existe nem na tabela nem em "
        "prosa reprova no modo --textual)"
    )
    return True


# Item real, declarado so' em prosa (INBOX), NAO reprova - a mesma
# aceitacao que validate_key_exception_deaths() ja prova por chamada
# direta, exercitada aqui pela entrada --textual real.
def selftest_textual_prose_only_item_accepted(tmp_path):
    exit_code, output = _run_textual_main_capturing(
        tmp_path,
        "algum_teste.alguma_chave|windows|motivo qualquer aqui|ITEM-EM-PROSA\n",
        _todo_textual_fixture("OUTRO-ITEM", "⏳ Pendente")
        + "\n- `ITEM-EM-PROSA`: bullet de INBOX, real, ainda sem linha de tabela.\n",
    )
    if exit_code not in (None, 0):
        print(
            f"selftest: TEXTUAL-PROSA FALHOU (esperava sucesso, item em prosa e real, saiu com "
            f"codigo {exit_code!r}): {output}",
            file=sys.stderr,
        )
        return False
    print("selftest: TEXTUAL-PROSA OK (item real, so' em prosa, aceito no modo --textual)")
    return True


# Piso L-40 do proprio modo --textual: TODO.md sem nenhuma linha de
# tabela reconhecida reprova como varredura vazia.
def selftest_textual_empty_todo_reproves(tmp_path):
    exit_code, output = _run_textual_main_capturing(tmp_path, "", "so prosa aqui, nenhuma linha de tabela\n")
    if exit_code != 1:
        print(
            f"selftest: TEXTUAL-TODO-VAZIO FALHOU (esperava reprovar com codigo 1, veio "
            f"{exit_code!r}): {output}",
            file=sys.stderr,
        )
        return False
    if "varredura vazia" not in output:
        print(
            f"selftest: TEXTUAL-TODO-VAZIO FALHOU (reprovou, mas nao com a mensagem de "
            f"varredura vazia - GODS_LAWS.md L-40): {output!r}",
            file=sys.stderr,
        )
        return False
    print("selftest: TEXTUAL-TODO-VAZIO OK (TODO.md sem item nenhum reconhecido reprova)")
    return True


# --- CI-SPLIT-PER-OS A1: controles do modo --per-system -------------------


def _run_per_system_main_capturing(argv):
    import contextlib
    import io

    buffer = io.StringIO()
    exit_code = None
    with contextlib.redirect_stdout(buffer), contextlib.redirect_stderr(buffer):
        try:
            per_system_main(argv)
        except SystemExit as exc:
            exit_code = exc.code
    return exit_code, buffer.getvalue()


def selftest_per_system_positive_control(tmp_path):
    caminho = _write_temp(tmp_path, "ps_pos.txt", "MEASURED a_test.k=1\n")
    argv = [f"{slug}={caminho}" for slug in _per_system_slugs_esperados()]
    exit_code, output = _run_per_system_main_capturing(argv)
    if exit_code not in (None, 0):
        print(f"selftest: PER-SYSTEM-POSITIVO FALHOU (codigo {exit_code!r}): {output}", file=sys.stderr)
        return False
    if "0 obrigatoria(s), 0 herdada(s)" not in output:
        print(f"selftest: PER-SYSTEM-POSITIVO FALHOU (esperava 0/0): {output!r}", file=sys.stderr)
        return False
    print("selftest: PER-SYSTEM-POSITIVO OK (cinco sistemas com a mesma chave, nada obrigatorio/herdado)")
    return True


# C2-gemeo (docs/plano-ci-split-per-os.md secao 4.3, "idem"): uma chave
# que falta so' no cachyos, sem dono conhecido em parity_exceptions.txt
# nem em parity_aliases.txt, tem de aparecer como "obrigatoria" SO' na
# lista do cachyos - o defeito que --compare (dois lados so') nao pode
# nem enxergar, porque nao existe "cachyos" nele.
def selftest_per_system_gap_classified_by_system(tmp_path):
    cheio = _write_temp(tmp_path, "ps_cheio.txt", "MEASURED a_test.k=1\nMEASURED b_test.k=2\n")
    so_a = _write_temp(tmp_path, "ps_so_a.txt", "MEASURED a_test.k=1\n")
    argv = [
        f"windows={cheio}",
        f"fedora={cheio}",
        f"ubuntu={cheio}",
        f"arch={cheio}",
        f"cachyos={so_a}",
    ]
    exit_code, output = _run_per_system_main_capturing(argv)
    if exit_code not in (None, 0):
        print(f"selftest: PER-SYSTEM-LACUNA-POR-SISTEMA FALHOU (codigo {exit_code!r}): {output}", file=sys.stderr)
        return False
    if "### cachyos: 1 chave(s) ausente(s) (1 obrigatoria(s), 0 herdada(s))" not in output:
        print(
            f"selftest: PER-SYSTEM-LACUNA-POR-SISTEMA FALHOU (cachyos nao apareceu com 1 "
            f"obrigatoria): {output!r}",
            file=sys.stderr,
        )
        return False
    for outro in ("windows", "fedora", "ubuntu", "arch"):
        if f"### {outro}: 0 chave(s) ausente(s)" not in output:
            print(
                f"selftest: PER-SYSTEM-LACUNA-POR-SISTEMA FALHOU ({outro} deveria ter 0 "
                f"chaves ausentes, so' cachyos perde b_test.k): {output!r}",
                file=sys.stderr,
            )
            return False
    print("selftest: PER-SYSTEM-LACUNA-POR-SISTEMA OK (chave ausente so' no cachyos classificada so' nele)")
    return True


def selftest_per_system_gap_herdada_when_owner_known(tmp_path):
    cheio = _write_temp(tmp_path, "ps_cheio2.txt", "MEASURED a_test.k=1\nMEASURED tem_item_test.k=2\n")
    so_a = _write_temp(tmp_path, "ps_so_a2.txt", "MEASURED a_test.k=1\n")
    exceptions_file = _write_temp(tmp_path, "ps_exceptions.txt", "tem_item_test|linux|motivo qualquer|ITEM-ABERTO\n")
    argv = [
        f"windows={cheio}",
        f"fedora={cheio}",
        f"ubuntu={cheio}",
        f"arch={cheio}",
        f"cachyos={so_a}",
        "--exceptions",
        exceptions_file,
    ]
    exit_code, output = _run_per_system_main_capturing(argv)
    if exit_code not in (None, 0):
        print(f"selftest: PER-SYSTEM-HERDADA FALHOU (codigo {exit_code!r}): {output}", file=sys.stderr)
        return False
    if "[HERDADA] tem_item_test.k" not in output:
        print(f"selftest: PER-SYSTEM-HERDADA FALHOU (dono conhecido devia virar herdada): {output!r}", file=sys.stderr)
        return False
    print("selftest: PER-SYSTEM-HERDADA OK (dono com excecao em parity_exceptions.txt classifica como herdada)")
    return True


def selftest_per_system_missing_system_reproves(tmp_path):
    caminho = _write_temp(tmp_path, "ps_faltando.txt", "MEASURED a_test.k=1\n")
    argv = [f"{slug}={caminho}" for slug in _per_system_slugs_esperados() if slug != "windows"]
    exit_code, output = _run_per_system_main_capturing(argv)
    if exit_code != 1:
        print(f"selftest: PER-SYSTEM-PISO-AUSENTE FALHOU (esperava exit 1, veio {exit_code!r}): {output}", file=sys.stderr)
        return False
    if "windows" not in output:
        print(f"selftest: PER-SYSTEM-PISO-AUSENTE FALHOU (mensagem nao citou 'windows'): {output!r}", file=sys.stderr)
        return False
    print("selftest: PER-SYSTEM-PISO-AUSENTE OK (sistema esperado sem arquivo nenhum reprova)")
    return True


def selftest_per_system_empty_all_reproves(tmp_path):
    vazio = _write_temp(tmp_path, "ps_vazio.txt", "")
    argv = [f"{slug}={vazio}" for slug in _per_system_slugs_esperados()]
    exit_code, output = _run_per_system_main_capturing(argv)
    if exit_code != 1:
        print(f"selftest: PER-SYSTEM-VARREDURA-VAZIA FALHOU (esperava exit 1, veio {exit_code!r}): {output}", file=sys.stderr)
        return False
    if "varredura vazia" not in output:
        print(f"selftest: PER-SYSTEM-VARREDURA-VAZIA FALHOU (mensagem sem 'varredura vazia'): {output!r}", file=sys.stderr)
        return False
    print("selftest: PER-SYSTEM-VARREDURA-VAZIA OK (todo sistema com 0 chaves reprova)")
    return True


def selftest_per_system_unexpected_slug_rejected():
    exit_code, output = _run_per_system_main_capturing(["bogus-slug=/tmp/x"])
    if exit_code != 1:
        print(f"selftest: PER-SYSTEM-SLUG-INVALIDO FALHOU (esperava exit 1, veio {exit_code!r}): {output}", file=sys.stderr)
        return False
    print("selftest: PER-SYSTEM-SLUG-INVALIDO OK (slug fora da lista de alvos e' recusado)")
    return True


# --- CI-SPLIT-PER-OS A1b (D-A10): controles do modo --per-system, valor --


def _write_five_systems(tmp_path, prefix, values_by_slug):
    """values_by_slug: {slug: [valor, ...]} - mais de um valor por slug
    simula pernas discordando (compartilhado/estatico) dentro do MESMO
    sistema, o cenario M5. Slugs ausentes do dict ficam de fora (usado
    por M2, fedora sem medir a chave nenhuma). Chave fixa "some_test.k" -
    nenhum controle ainda precisou de outra (GODS_LAWS.md L-17, regra
    de 3 - extrai-se um parametro so' na terceira ocorrencia real)."""
    argv = []
    for slug, valores in values_by_slug.items():
        linhas = "\n".join(f"MEASURED some_test.k={v}" for v in valores) + "\n"
        caminho = _write_temp(tmp_path, f"{prefix}_{slug}.txt", linhas)
        argv.append(f"{slug}={caminho}")
    return argv


def selftest_per_system_value_unanimity_iguais_control(tmp_path):
    argv = _write_five_systems(tmp_path, "unanime", {s: ["1"] for s in _per_system_slugs_esperados()})
    exceptions_file = _write_temp(tmp_path, "exc_vazio.txt", "")
    todo_file = _write_temp(tmp_path, "TODO_vazio.md", "")
    exit_code, output = _run_per_system_main_capturing(
        argv + ["--measured-exceptions", exceptions_file, "--todo", todo_file]
    )
    if exit_code not in (None, 0):
        print(f"selftest: PER-SYSTEM-VALOR-IGUAIS FALHOU (codigo {exit_code!r}): {output}", file=sys.stderr)
        return False
    if "### iguais (1)" not in output:
        print(f"selftest: PER-SYSTEM-VALOR-IGUAIS FALHOU (esperava 1 igual): {output!r}", file=sys.stderr)
        return False
    print("selftest: PER-SYSTEM-VALOR-IGUAIS OK (cinco sistemas com o mesmo valor - iguais)")
    return True


# C2-gemeo de valor (docs/plano-ci-split-per-os.md secao 7, A1b): "chave
# k = 1 em fedora, ubuntu, arch e windows, e 0 em cachyos, sem excecao" -
# o TESTE VERMELHO DE ESTREIA da A1b. Antes desta fatia (ae5dd7e), o modo
# por sistema nao dizia NADA do valor (so' presenca) - vermelho genuino.
# Tambem mata M1 (voto de maioria): 4 sistemas concordam em "1" e 1 diverge
# - um mutante de maioria classificaria "iguais" (4 > 1), este controle
# exige "divergentes".
def selftest_per_system_value_divergent_undeclared_reproves(tmp_path):
    argv = _write_five_systems(
        tmp_path, "diverge",
        {"fedora": ["1"], "ubuntu": ["1"], "arch": ["1"], "windows": ["1"], "cachyos": ["0"]},
    )
    exceptions_file = _write_temp(tmp_path, "exc_vazio2.txt", "")
    todo_file = _write_temp(tmp_path, "TODO_vazio2.md", "")
    exit_code, output = _run_per_system_main_capturing(
        argv + ["--measured-exceptions", exceptions_file, "--todo", todo_file]
    )
    if exit_code not in (None, 0):
        print(f"selftest: PER-SYSTEM-VALOR-DIVERGENTE FALHOU (codigo {exit_code!r}): {output}", file=sys.stderr)
        return False
    if "### divergentes NAO declaradas (1)" not in output:
        print(f"selftest: PER-SYSTEM-VALOR-DIVERGENTE FALHOU (esperava 1 divergente nao declarada): {output!r}", file=sys.stderr)
        return False
    if "0: cachyos | 1: arch,fedora,ubuntu,windows" not in output:
        print(f"selftest: PER-SYSTEM-VALOR-DIVERGENTE FALHOU (particao nao impressa como esperado): {output!r}", file=sys.stderr)
        return False
    print("selftest: PER-SYSTEM-VALOR-DIVERGENTE OK (4x1 nao vira iguais - unanimidade, nunca maioria)")
    return True


def selftest_per_system_value_divergent_todos_declared(tmp_path):
    argv = _write_five_systems(
        tmp_path, "declara",
        {"fedora": ["1"], "ubuntu": ["1"], "arch": ["1"], "windows": ["1"], "cachyos": ["0"]},
    )
    exceptions_file = _write_temp(tmp_path, "exc_todos.txt", "some_test.k|todos|metrica do executor|SEM-PENDENCIA\n")
    todo_file = _write_temp(tmp_path, "TODO_todos.md", "")
    exit_code, output = _run_per_system_main_capturing(
        argv + ["--measured-exceptions", exceptions_file, "--todo", todo_file]
    )
    if exit_code not in (None, 0):
        print(f"selftest: PER-SYSTEM-VALOR-TODOS-DECLARA FALHOU (codigo {exit_code!r}): {output}", file=sys.stderr)
        return False
    if "### divergentes declaradas (1)" not in output:
        print(f"selftest: PER-SYSTEM-VALOR-TODOS-DECLARA FALHOU (esperava declarada com lado=todos): {output!r}", file=sys.stderr)
        return False
    print("selftest: PER-SYSTEM-VALOR-TODOS-DECLARA OK (lado=todos declara a mesma divergencia)")
    return True


# M2: "Fedora como referencia" proibido - fedora NUNCA mede a chave
# (ausente do dict, nao so' com valor diferente), e ubuntu≠arch. Um
# mutante que comparasse tudo contra fedora nao teria contra o que
# comparar e deixaria a chave de fora (silenciosa) - aqui ela TEM de
# aparecer como divergente.
def selftest_per_system_value_without_reference_system_still_compares(tmp_path):
    argv = _write_five_systems(
        tmp_path, "semfedora",
        {"ubuntu": ["1"], "arch": ["0"], "windows": ["1"], "cachyos": ["1"]},
    )
    exceptions_file = _write_temp(tmp_path, "exc_semfedora.txt", "")
    todo_file = _write_temp(tmp_path, "TODO_semfedora.md", "")
    exit_code, output = _run_per_system_main_capturing(
        argv + ["fedora=" + _write_temp(tmp_path, "semfedora_fedora.txt", "")]
        + ["--measured-exceptions", exceptions_file, "--todo", todo_file]
    )
    if exit_code not in (None, 0):
        print(f"selftest: PER-SYSTEM-VALOR-SEM-FEDORA FALHOU (codigo {exit_code!r}): {output}", file=sys.stderr)
        return False
    if "### divergentes NAO declaradas (1)" not in output:
        print(f"selftest: PER-SYSTEM-VALOR-SEM-FEDORA FALHOU (fedora ausente nao pode impedir a comparacao): {output!r}", file=sys.stderr)
        return False
    print("selftest: PER-SYSTEM-VALOR-SEM-FEDORA OK (ubuntu x arch diverge mesmo sem fedora medir nada)")
    return True


# M3: "familias" tratado como "todos" e' proibido - ubuntu != fedora
# (dois sistemas LINUX discordando entre si) tem de sair NAO declarada
# mesmo com lado=familias, porque familias so' cobre Linux x Windows.
def selftest_per_system_value_familias_linux_vs_linux_undeclared(tmp_path):
    argv = _write_five_systems(
        tmp_path, "familias_ll",
        {"fedora": ["1"], "ubuntu": ["0"], "arch": ["1"], "windows": ["1"], "cachyos": ["1"]},
    )
    exceptions_file = _write_temp(tmp_path, "exc_familias_ll.txt", "some_test.k|familias|mecanismo Wayland x Win32|SEM-PENDENCIA\n")
    todo_file = _write_temp(tmp_path, "TODO_familias_ll.md", "")
    exit_code, output = _run_per_system_main_capturing(
        argv + ["--measured-exceptions", exceptions_file, "--todo", todo_file]
    )
    if exit_code not in (None, 0):
        print(f"selftest: PER-SYSTEM-VALOR-FAMILIAS-LL FALHOU (codigo {exit_code!r}): {output}", file=sys.stderr)
        return False
    if "### divergentes NAO declaradas (1)" not in output:
        print(f"selftest: PER-SYSTEM-VALOR-FAMILIAS-LL FALHOU (familias nao cobre linux x linux): {output!r}", file=sys.stderr)
        return False
    print("selftest: PER-SYSTEM-VALOR-FAMILIAS-LL OK (familias nao declara divergencia entre duas distros)")
    return True


def selftest_per_system_value_familias_linux_vs_windows_declared(tmp_path):
    argv = _write_five_systems(
        tmp_path, "familias_lw",
        {"fedora": ["1"], "ubuntu": ["1"], "arch": ["1"], "cachyos": ["1"], "windows": ["0"]},
    )
    exceptions_file = _write_temp(tmp_path, "exc_familias_lw.txt", "some_test.k|familias|mecanismo Wayland x Win32|SEM-PENDENCIA\n")
    todo_file = _write_temp(tmp_path, "TODO_familias_lw.md", "")
    exit_code, output = _run_per_system_main_capturing(
        argv + ["--measured-exceptions", exceptions_file, "--todo", todo_file]
    )
    if exit_code not in (None, 0):
        print(f"selftest: PER-SYSTEM-VALOR-FAMILIAS-LW FALHOU (codigo {exit_code!r}): {output}", file=sys.stderr)
        return False
    if "### divergentes declaradas (1)" not in output:
        print(f"selftest: PER-SYSTEM-VALOR-FAMILIAS-LW FALHOU (linux unanime x windows tem de declarar): {output!r}", file=sys.stderr)
        return False
    print("selftest: PER-SYSTEM-VALOR-FAMILIAS-LW OK (linux unanime, so' windows diverge - familias declara)")
    return True


# M4: "ambos" nao existe mais - migracao completa, formato invalido.
def selftest_per_system_value_ambos_format_rejected(tmp_path):
    argv = _write_five_systems(tmp_path, "ambos", {s: ["1"] for s in _per_system_slugs_esperados()})
    exceptions_file = _write_temp(tmp_path, "exc_ambos.txt", "some_test.k|ambos|forma antiga|SEM-PENDENCIA\n")
    todo_file = _write_temp(tmp_path, "TODO_ambos.md", "")
    exit_code, output = _run_per_system_main_capturing(
        argv + ["--measured-exceptions", exceptions_file, "--todo", todo_file]
    )
    if exit_code != 1:
        print(f"selftest: PER-SYSTEM-VALOR-AMBOS-REJEITADO FALHOU (esperava exit 1, veio {exit_code!r}): {output}", file=sys.stderr)
        return False
    if "formato invalido" not in output and "fora do vocabulario fechado" not in output:
        print(f"selftest: PER-SYSTEM-VALOR-AMBOS-REJEITADO FALHOU (mensagem nao citou o motivo): {output!r}", file=sys.stderr)
        return False
    print("selftest: PER-SYSTEM-VALOR-AMBOS-REJEITADO OK ('ambos' reprova como formato invalido, D-A10)")
    return True


# M5: instabilidade DENTRO do mesmo sistema (fedora shared=1/static=0)
# nunca pode ser colapsada escolhendo um valor - tem de sair em secao
# propria, "instavel no sistema fedora", NAO declarada sem lado=todos.
def selftest_per_system_value_instability_within_system_reproves(tmp_path):
    argv = _write_five_systems(
        tmp_path, "instavel",
        {"fedora": ["1", "0"], "ubuntu": ["1"], "arch": ["1"], "windows": ["1"], "cachyos": ["1"]},
    )
    exceptions_file = _write_temp(tmp_path, "exc_instavel.txt", "")
    todo_file = _write_temp(tmp_path, "TODO_instavel.md", "")
    exit_code, output = _run_per_system_main_capturing(
        argv + ["--measured-exceptions", exceptions_file, "--todo", todo_file]
    )
    if exit_code not in (None, 0):
        print(f"selftest: PER-SYSTEM-VALOR-INSTAVEL FALHOU (codigo {exit_code!r}): {output}", file=sys.stderr)
        return False
    if "### instaveis NAO declaradas (1)" not in output or "instavel no sistema fedora: some_test.k" not in output:
        print(f"selftest: PER-SYSTEM-VALOR-INSTAVEL FALHOU (instabilidade do fedora nao apareceu como esperado): {output!r}", file=sys.stderr)
        return False
    print("selftest: PER-SYSTEM-VALOR-INSTAVEL OK (fedora shared!=static aparece como instavel, nunca colapsado)")
    return True


def selftest_per_system_value_instability_declared_with_todos(tmp_path):
    argv = _write_five_systems(
        tmp_path, "instaveltodos",
        {"fedora": ["1", "0"], "ubuntu": ["1"], "arch": ["1"], "windows": ["1"], "cachyos": ["1"]},
    )
    exceptions_file = _write_temp(tmp_path, "exc_instaveltodos.txt", "some_test.k|todos|tempo/contagem do executor|SEM-PENDENCIA\n")
    todo_file = _write_temp(tmp_path, "TODO_instaveltodos.md", "")
    exit_code, output = _run_per_system_main_capturing(
        argv + ["--measured-exceptions", exceptions_file, "--todo", todo_file]
    )
    if exit_code not in (None, 0):
        print(f"selftest: PER-SYSTEM-VALOR-INSTAVEL-TODOS FALHOU (codigo {exit_code!r}): {output}", file=sys.stderr)
        return False
    if "### instaveis declaradas (1)" not in output:
        print(f"selftest: PER-SYSTEM-VALOR-INSTAVEL-TODOS FALHOU (lado=todos tem de declarar a instabilidade): {output!r}", file=sys.stderr)
        return False
    print("selftest: PER-SYSTEM-VALOR-INSTAVEL-TODOS OK (lado=todos cobre instabilidade dentro de um sistema)")
    return True


def selftest_per_system_value_mode_optional_when_no_measured_exceptions(tmp_path):
    argv = _write_five_systems(tmp_path, "semvalor", {s: ["1"] for s in _per_system_slugs_esperados()})
    exit_code, output = _run_per_system_main_capturing(argv)
    if exit_code not in (None, 0):
        print(f"selftest: PER-SYSTEM-VALOR-MODO-OPCIONAL FALHOU (codigo {exit_code!r}): {output}", file=sys.stderr)
        return False
    if "secao de VALOR" not in output or "desligada" not in output:
        print(f"selftest: PER-SYSTEM-VALOR-MODO-OPCIONAL FALHOU (aviso de modo desligado nao apareceu): {output!r}", file=sys.stderr)
        return False
    print("selftest: PER-SYSTEM-VALOR-MODO-OPCIONAL OK (sem --measured-exceptions, so' presenca, avisado)")
    return True


def selftest_per_system_value_exception_pointing_to_concluded_item_reproves(tmp_path):
    argv = _write_five_systems(tmp_path, "concluido", {s: ["1"] for s in _per_system_slugs_esperados()})
    exceptions_file = _write_temp(tmp_path, "exc_concluido.txt", "some_test.k|todos|motivo qualquer|ITEM-FECHADO\n")
    todo_file = _write_temp(
        tmp_path, "TODO_concluido.md",
        "| WSJF | ID | Onda | Grupo | Descricao | Prioridade | Pre-requisito | Dificuldade | Status | Estado |\n"
        "|---|---|---|---|---|---|---|---|---|---|\n"
        "| 1.0 | ITEM-FECHADO | W1 | X | y | Alta | - | Media | ✅ Concluido | - |\n",
    )
    exit_code, output = _run_per_system_main_capturing(
        argv + ["--measured-exceptions", exceptions_file, "--todo", todo_file]
    )
    if exit_code != 1:
        print(f"selftest: PER-SYSTEM-VALOR-EXCECAO-CONCLUIDA FALHOU (esperava exit 1, veio {exit_code!r}): {output}", file=sys.stderr)
        return False
    if "CONCLUIDO" not in output:
        print(f"selftest: PER-SYSTEM-VALOR-EXCECAO-CONCLUIDA FALHOU (mensagem nao citou CONCLUIDO): {output!r}", file=sys.stderr)
        return False
    print("selftest: PER-SYSTEM-VALOR-EXCECAO-CONCLUIDA OK (regra de morte por chave vale no modo por sistema)")
    return True


def _per_system_value_controls():
    return [
        selftest_per_system_value_unanimity_iguais_control,
        selftest_per_system_value_divergent_undeclared_reproves,
        selftest_per_system_value_divergent_todos_declared,
        selftest_per_system_value_without_reference_system_still_compares,
        selftest_per_system_value_familias_linux_vs_linux_undeclared,
        selftest_per_system_value_familias_linux_vs_windows_declared,
        selftest_per_system_value_ambos_format_rejected,
        selftest_per_system_value_instability_within_system_reproves,
        selftest_per_system_value_instability_declared_with_todos,
        selftest_per_system_value_mode_optional_when_no_measured_exceptions,
        selftest_per_system_value_exception_pointing_to_concluded_item_reproves,
    ]


# CI-SPLIT-PER-OS A3a (D-A12, D1 - achado do main): mesma fixture de
# check_test_parity.py - nenhum controle deste arquivo toca tools/ci/
# systems.txt, exceto o controle dedicado que testa o arquivo real.
_SELFTEST_CI_SYSTEMS = {
    "fedora": "linux",
    "ubuntu": "linux",
    "arch": "linux",
    "cachyos": "linux",
    "windows": "windows",
}


# D2 (achado do main): prova que _load_real_ci_systems() resolve o
# caminho a partir de Path(__file__), nunca do cwd - so' a FORMA
# (familias linux/windows presentes), nunca o conjunto exato de slugs
# (D1: a A7 acrescentando uma distro tem que deixar isto intacto).
def selftest_real_ci_systems_loads_from_script_location():
    global _CI_SYSTEMS_CACHE
    _CI_SYSTEMS_CACHE = None
    try:
        systems = _load_real_ci_systems()
    except (OSError, ci_systems.CiSystemsError) as exc:
        print(f"selftest: CI-SYSTEMS-CAMINHO-REAL FALHOU (tools/ci/systems.txt nao carregou): {exc}", file=sys.stderr)
        return False
    familias_presentes = set(systems.values())
    if familias_presentes != {"linux", "windows"}:
        print(f"selftest: CI-SYSTEMS-CAMINHO-REAL FALHOU (esperava as familias linux e windows, veio {familias_presentes})", file=sys.stderr)
        return False
    print(
        "selftest: CI-SYSTEMS-CAMINHO-REAL OK (tools/ci/systems.txt real carrega via "
        "Path(__file__), nao depende do cwd - D2)"
    )
    return True


def selftest_main():
    import tempfile

    set_ci_systems_override(_SELFTEST_CI_SYSTEMS)
    try:
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            controls = [
                selftest_empty_both_sides_reproves(tmp_path),
                selftest_five_sections_classify_correctly(tmp_path),
                selftest_scancount_never_compared(tmp_path),
                selftest_key_exception_accepted(tmp_path),
                selftest_key_exception_with_concluded_item_reproves(tmp_path),
                selftest_key_exception_with_nonexistent_item_reproves(tmp_path),
                selftest_key_exception_with_prose_only_item_accepted(tmp_path),
                selftest_declared_divergence_with_concluded_item_reproves(tmp_path),
                selftest_one_side_empty_still_succeeds(tmp_path),
                selftest_textual_positive_control(tmp_path),
                selftest_textual_concluded_item_reproves(tmp_path),
                selftest_textual_nonexistent_item_reproves(tmp_path),
                selftest_textual_prose_only_item_accepted(tmp_path),
                selftest_textual_empty_todo_reproves(tmp_path),
                selftest_per_system_positive_control(tmp_path),
                selftest_per_system_gap_classified_by_system(tmp_path),
                selftest_per_system_gap_herdada_when_owner_known(tmp_path),
                selftest_per_system_missing_system_reproves(tmp_path),
                selftest_per_system_empty_all_reproves(tmp_path),
                selftest_per_system_unexpected_slug_rejected(),
                *(control(tmp_path) for control in _per_system_value_controls()),
                selftest_real_ci_systems_loads_from_script_location(),
            ]
    finally:
        set_ci_systems_override(None)
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
    elif args and args[0] == "--textual":
        textual_main(args[1:])
    elif args and args[0] == "--per-system":
        per_system_main(args[1:])
    else:
        fail(
            "usage: check_measured_parity.py --compare --linux <f1> [<f2> ...] --windows "
            "<f1> [<f2> ...]  |  --textual <measured_exceptions.txt> <TODO.md>  |  "
            "--per-system <slug>=<arquivo> [...]  |  --selftest"
        )


if __name__ == "__main__":
    main()
