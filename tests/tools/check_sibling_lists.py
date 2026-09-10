#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_sibling_lists.py - TODO.md item GATE-SIBLING-LIST (GODS_LAWS.md
# L-36/L-40).
#
# Generalizes the anchor-pair mechanism check_dup_laws.py already
# proved in this house (GODS_LAWS.md/ESCOPO.md, "<!-- DUP-BLOCK:ID:
# START/END -->", byte-for-byte equality) to TWO respects that file's
# own design does not cover: (1) any number of pairs, discovered by
# GROUP NAME across the whole tracked tree, not two hardcoded file
# paths; (2) TOKEN-SET equality, not byte equality - a sibling list
# living once in POSIX sh (`readonly VAR="a b c"`) and once in
# PowerShell (`$VAR = @("a", "b", "c")`) is never byte-identical even
# when it is semantically the exact same closed set, and this project
# has real pairs shaped exactly that way (see below).
#
# THE REAL DEFECT THIS CLOSES (measured 05-06/09/2026, git blame on
# tests/tools/check_port_privacy.sh and tools/ci/check-port-privacy-
# win.ps1): the two files each carry a CLOSED list of adapter/port
# class names (KNOWN_ADAPTER_CLASSES/KNOWN_PORT_NAMES) that the file's
# own comments say, in prose, "MUST match ... verbatim" / "MUST stay
# identical to the ones below, by hand, in the same commit that ever
# changes either". Six separate times across five days, a name was
# added to ONE side's list and the SAME commit's own comment names the
# other side as still owing the update ("tools/ci/check-port-privacy-
# win.ps1's own KNOWN_ADAPTER_CLASSES is OUT OF THIS FATIA'S declared
# file scope ... it still needs the SAME update ... flagged for the
# orchestrator rather than edited here" - check_port_privacy.sh, the
# wayland_window_adapter comment). The author warned IN WRITING, every
# time, and nothing mechanical ever collected on the warning - exactly
# the GATE-SIBLING-LIST item's own "na segunda, o proprio autor avisou
# por escrito que faltava, sem que houvesse mecanismo para cobrar."
#
# THE OTHER REAL INCIDENT THE ITEM NAMES ("uma lista de biblioteca do
# sistema tinha a variante de entrega e nao a de depuracao",
# DEBUG-CRT-NAMES, tools/ci/check-dep-zero-win.ps1) has NO sibling
# list anywhere else in this tree (grep for the four CRT DLL base
# names across every .cmake/.ps1/.py/.yml file in this repository
# finds exactly one file) - it was a single list failing to cover a
# SECOND CONSUMER (the windows-debug job) of itself, not two lists
# drifting apart. That shape is NOT what this gate closes; marking it
# here would be a false MUST-MATCH pair with nothing on the other end
# (GODS_LAWS.md L-40: this gate reproves an orphan marker precisely so
# a well-meaning-but-wrong marking like that gets caught, not
# invented as silent coverage of a defect this design cannot see).
#
# OPT-IN BY EXPLICIT MARKER, NEVER BY SIMILARITY (the item's own
# design constraint, verbatim: "O desenho e OPT-IN POR MARCADOR
# EXPLICITO, nunca por semelhanca"): this gate NEVER compares two
# lists because they look alike. It compares ONLY the text bracketed
# by a matching pair of anchors naming the SAME group id:
#
#   # GLINTFX-SIBLING-LIST:<group-id>:START
#   ...quoted string literals...
#   # GLINTFX-SIBLING-LIST:<group-id>:END
#
# tests/tools/check_dep_zero.py's own system-library allowlist and
# tools/ci/check-dep-zero-win.ps1's own IMPORT_ALLOWLIST_EXACT are a
# real pair of lists that LOOK like siblings (both "which system
# library may this binary link") and LEGITIMATELY diverge - Linux
# .so names and Windows .dll names are never the same strings, by
# construction, for every platform this project targets. Left
# unmarked on purpose: a similarity-based design would either flag
# that pair constantly (noise, disabled on day one, GODS_LAWS.md L-36's
# own "viraria ruido e seria desligada no primeiro dia") or need a
# growing exceptions list nobody reviews - the same trap check_test_
# parity.py's own header already rejected for test-name parity.
#
# WHAT THIS GATE DOES NOT CATCH (declared, GODS_LAWS.md L-40 applied to
# the next reader instead of only to the leader - a portao sold as
# stronger than it is is worse than no portao, GODS_LAWS.md L-36):
#
#   * A list that SHOULD be marked and never was. The marker is the
#     only entry point this gate has - it has no way to notice a new
#     closed list that repeats itself across two files if nobody wraps
#     it. Cited as the item's own first NAO PEGA line.
#   * A list whose FORM changes so much that no quoted-string token
#     survives inside the marked block (e.g. someone rewrites the
#     list as a numeric enum with no string literal anywhere). This is
#     NOT swallowed silently - EMPTY-EXTRACTION reproves exactly like
#     the empty-scan floor does (see require_nonempty_extraction()
#     below) - but it is worth naming here because the failure mode a
#     careless reader might expect ("the gate just stops checking
#     that pair") is the opposite of what actually happens.
#   * Semantic equivalence under a different spelling (a renamed but
#     equivalent entry on one side, e.g. an alias). This gate proves
#     SET EQUALITY of literal tokens, nothing about meaning - the item
#     says as much for the general shape: "Nao valida semantica, so
#     igualdade."
#
# Usage:
#   check_sibling_lists.py --check <repo-root-directory>
#   check_sibling_lists.py --selftest
#
# Each function below does one thing (GODS_LAWS.md L-17).

import re
import subprocess
import sys

SCRIPT_NAME = "check_sibling_lists.py"

_START_RE = re.compile(r"#\s*GLINTFX-SIBLING-LIST:([A-Za-z0-9_-]+):START\s*$")
_END_RE = re.compile(r"#\s*GLINTFX-SIBLING-LIST:([A-Za-z0-9_-]+):END\s*$")
_QUOTED_STRING_RE = re.compile(r'"([^"]*)"')


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


# --- extraction ----------------------------------------------------------


# One (group_id, path, token_set, line_count) per well-formed block, PLUS
# a separate list of structural errors (never swallowed - an
# unterminated or mismatched marker pair is reproved by name, not
# skipped as though the file had no marker at all). Mirrors check_dup_
# laws.py's own block_text() in spirit (line-based, anchors excluded
# from the extracted text) but walks every file ONCE with an explicit
# stack, because a single file may carry more than one marked group
# (check_port_privacy.sh's own KNOWN_ADAPTER_CLASSES/KNOWN_PORT_NAMES
# pair lives inside one shared block today, but nothing in this design
# assumes only one block per file).
def scan_file_for_blocks(path, text):
    blocks = []
    errors = []
    open_group = None
    open_start_line = None
    open_lines = []
    for line_no, line in enumerate(text.splitlines(), start=1):
        start_m = _START_RE.search(line)
        end_m = _END_RE.search(line)
        if start_m:
            if open_group is not None:
                errors.append(
                    f"{path}:{open_start_line}: bloco '{open_group}' nunca fechou antes de "
                    f"'{start_m.group(1)}' abrir na linha {line_no} (START aninhado nao suportado)"
                )
                open_group = None
            open_group = start_m.group(1)
            open_start_line = line_no
            open_lines = []
            continue
        if end_m:
            if open_group is None:
                errors.append(
                    f"{path}:{line_no}: END '{end_m.group(1)}' sem START correspondente"
                )
                continue
            if end_m.group(1) != open_group:
                errors.append(
                    f"{path}:{line_no}: END '{end_m.group(1)}' nao casa com o START aberto "
                    f"'{open_group}' (linha {open_start_line})"
                )
                open_group = None
                continue
            blocks.append((open_group, path, "\n".join(open_lines), open_start_line, line_no))
            open_group = None
            continue
        if open_group is not None:
            open_lines.append(line)
    if open_group is not None:
        errors.append(
            f"{path}:{open_start_line}: bloco '{open_group}' nunca fechou (sem END ate o fim do arquivo)"
        )
    return blocks, errors


# Turns a block's raw text into the set of tokens it declares. A quoted
# string with internal whitespace (the POSIX sh shape, one big
# space-separated string) is split into words; a quoted string with no
# whitespace (one element of a PowerShell `@("a", "b")` array literal)
# is kept whole. Both syntaxes converge on the SAME token set for the
# SAME logical list - proven by selftest_positive_control_mixed_syntax
# below against the real shapes this project's own two files use.
def tokenize_block(block_text_value):
    tokens = set()
    for match in _QUOTED_STRING_RE.finditer(block_text_value):
        value = match.group(1)
        if not value:
            continue
        if any(ch.isspace() for ch in value):
            tokens.update(value.split())
        else:
            tokens.add(value)
    return tokens


# --- comparison ------------------------------------------------------------


# groups: group_id -> list of (path, token_set, line_count)
def group_blocks_by_id(all_blocks):
    groups = {}
    for group_id, path, raw_text, _start_line, _end_line in all_blocks:
        groups.setdefault(group_id, []).append((path, raw_text))
    return groups


# The whole verdict, as a list of error strings - empty means the gate
# passes. Reproduces, in one place, every control --selftest exercises
# in red: structural errors (always fatal, never optional), top-level
# empty scan (GODS_LAWS.md L-40), a group with something other than
# exactly two marked files (orphan marker, or an ambiguous third), a
# marked block with zero extractable tokens (extraction floor, also
# L-40), and a genuine token-set mismatch between the two files of a
# well-formed pair.
def evaluate(all_blocks, structural_errors):
    errors = list(structural_errors)

    if not all_blocks:
        errors.append(
            "varredura vazia: nenhum bloco GLINTFX-SIBLING-LIST encontrado em arquivo algum - "
            "GODS_LAWS.md L-40, isto e sinal de coleta quebrada (ou de marcador renomeado/"
            "apagado), nunca 'nada para comparar'"
        )
        return errors

    groups = group_blocks_by_id(all_blocks)

    for group_id in sorted(groups):
        entries = groups[group_id]
        paths = [path for path, _raw in entries]
        if len(entries) != 2:
            errors.append(
                f"grupo '{group_id}': marcado em {len(entries)} arquivo(s) ({', '.join(paths)}), "
                "esperava exatamente 2 - um marcador orfao (a lista irma nao foi marcada, ou foi "
                "apagada) ou um terceiro marcador ambiguo nao tem par unico para comparar"
            )
            continue

        (path_a, raw_a), (path_b, raw_b) = entries
        tokens_a = tokenize_block(raw_a)
        tokens_b = tokenize_block(raw_b)

        empty_sides = [p for p, t in ((path_a, tokens_a), (path_b, tokens_b)) if not t]
        if empty_sides:
            errors.append(
                f"grupo '{group_id}': extracao vazia em {', '.join(empty_sides)} - o bloco marcado "
                "nao contem nenhum literal de string entre aspas (GODS_LAWS.md L-40: forma mudou e "
                "escapou a extracao, tratado como reprovacao, nunca pulo calado)"
            )
            continue

        only_a = sorted(tokens_a - tokens_b)
        only_b = sorted(tokens_b - tokens_a)
        if only_a or only_b:
            msg = [f"grupo '{group_id}': DIVERGE entre '{path_a}' e '{path_b}':"]
            if only_a:
                msg.append(f"  so em {path_a}: {', '.join(only_a)}")
            if only_b:
                msg.append(f"  so em {path_b}: {', '.join(only_b)}")
            errors.append("\n".join(msg))

    return errors


# --- real mode -------------------------------------------------------------


def git_ls_files(root):
    proc = subprocess.run(
        ["git", "-C", root, "ls-files", "-z"],
        capture_output=True,
        check=False,
    )
    if proc.returncode != 0:
        fail(
            f"'git -C {root} ls-files' saiu com codigo {proc.returncode}: "
            f"{proc.stderr.decode('utf-8', errors='replace').strip()}"
        )
    raw = proc.stdout.split(b"\x00")
    return [p.decode("utf-8", errors="surrogateescape") for p in raw if p]


def read_text_lenient(path):
    try:
        with open(path, "r", encoding="utf-8") as handle:
            return handle.read()
    except (OSError, UnicodeDecodeError):
        return None


def real_main(args):
    if len(args) != 1:
        fail("usage: check_sibling_lists.py --check <repo-root-directory>")
    root = args[0]

    all_blocks = []
    structural_errors = []
    files_with_markers = 0
    for rel_path in git_ls_files(root):
        import os

        abs_path = os.path.join(root, rel_path)
        text = read_text_lenient(abs_path)
        if text is None or "GLINTFX-SIBLING-LIST:" not in text:
            continue
        files_with_markers += 1
        blocks, errors = scan_file_for_blocks(rel_path, text)
        all_blocks.extend(blocks)
        structural_errors.extend(errors)

    group_count = len({b[0] for b in all_blocks})
    print(
        f"{SCRIPT_NAME}: {files_with_markers} arquivo(s) com marcador, {len(all_blocks)} "
        f"bloco(s) bem formado(s), {group_count} grupo(s) distinto(s), "
        f"{len(structural_errors)} erro(s) estrutural(is)"
    )

    errors = evaluate(all_blocks, structural_errors)
    if errors:
        print(f"{SCRIPT_NAME}: REPROVADO ({len(errors)} problema(s)):", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        sys.exit(1)

    print(f"{SCRIPT_NAME}: {group_count} grupo(s) de lista irma comparado(s), 0 divergencias")


# --- fixtures and controls for --selftest -----------------------------


def _sh_block(group_id, value_string):
    return (
        f'# GLINTFX-SIBLING-LIST:{group_id}:START\n'
        f'readonly KNOWN_ADAPTER_CLASSES="{value_string}"\n'
        f'# GLINTFX-SIBLING-LIST:{group_id}:END\n'
    )


def _ps1_block(group_id, *quoted_items):
    items = ", ".join(f'"{i}"' for i in quoted_items)
    return (
        f'# GLINTFX-SIBLING-LIST:{group_id}:START\n'
        f'$KNOWN_ADAPTER_CLASSES = @({items})\n'
        f'# GLINTFX-SIBLING-LIST:{group_id}:END\n'
    )


def _run(file_texts):
    """file_texts: dict[path] -> text. Returns (all_blocks, structural_errors)."""
    all_blocks = []
    structural_errors = []
    for path, text in file_texts.items():
        blocks, errors = scan_file_for_blocks(path, text)
        all_blocks.extend(blocks)
        structural_errors.extend(errors)
    return all_blocks, structural_errors


# Positive control: the REAL shape this project's own pair uses - one
# sh-style space-separated quoted string, one PowerShell array literal
# with one quoted item per entry - same logical set. Expected: passes,
# zero errors.
def selftest_positive_control_mixed_syntax():
    files = {
        "a.sh": _sh_block("adapters", "wayland_display_adapter win32_display_adapter fake_display_adapter"),
        "b.ps1": _ps1_block("adapters", "wayland_display_adapter", "win32_display_adapter", "fake_display_adapter"),
    }
    blocks, structural = _run(files)
    errors = evaluate(blocks, structural)
    if errors:
        print(f"selftest: controle POSITIVO FALHOU (esperava zero erros, veio {errors})", file=sys.stderr)
        return False
    print("selftest: controle POSITIVO OK (sh space-separated e ps1 array convergem no mesmo conjunto)")
    return True


# VERMELHO #1 - o defeito real do produto: um nome entra de um lado e
# nao do outro (a forma exata de wayland_window_adapter ter faltado em
# tools/ci/check-port-privacy-win.ps1 por uma fatia inteira).
def selftest_divergent_token_reproves():
    files = {
        "a.sh": _sh_block("adapters", "wayland_display_adapter wayland_window_adapter"),
        "b.ps1": _ps1_block("adapters", "wayland_display_adapter"),
    }
    blocks, structural = _run(files)
    errors = evaluate(blocks, structural)
    if not errors:
        print("selftest: VERMELHO#1 FALHOU (nome so-de-um-lado deveria ter reprovado)", file=sys.stderr)
        return False
    if not any("wayland_window_adapter" in e for e in errors):
        print(f"selftest: VERMELHO#1 FALHOU (reprovou, mas nao citou o nome): {errors}", file=sys.stderr)
        return False
    print(f"selftest: VERMELHO#1 OK (nome so-de-um-lado pego): {errors}")
    return True


# VERMELHO #2 - marcador orfao: o grupo foi aberto em UM arquivo so, o
# outro lado nunca foi marcado (ou o marcador foi apagado por engano).
def selftest_orphan_marker_reproves():
    files = {"only_a.sh": _sh_block("solo_group", "alpha beta")}
    blocks, structural = _run(files)
    errors = evaluate(blocks, structural)
    if not errors:
        print("selftest: VERMELHO#2 FALHOU (marcador orfao deveria ter reprovado)", file=sys.stderr)
        return False
    if not any("solo_group" in e and "1 arquivo" in e for e in errors):
        print(f"selftest: VERMELHO#2 FALHOU (reprovou, mas nao descreveu o orfao): {errors}", file=sys.stderr)
        return False
    print(f"selftest: VERMELHO#2 OK (marcador orfao pego): {errors}")
    return True


# Controle: um TERCEIRO arquivo marca o mesmo grupo - ambiguo, nao tem
# par unico, reprova mesmo que dois dos tres batam entre si.
def selftest_triple_marker_reproves():
    files = {
        "a.sh": _sh_block("triplo", "x y"),
        "b.ps1": _ps1_block("triplo", "x", "y"),
        "c.sh": _sh_block("triplo", "x y"),
    }
    blocks, structural = _run(files)
    errors = evaluate(blocks, structural)
    if not errors:
        print("selftest: controle TRIPLO-MARCADOR FALHOU (terceiro marcador deveria ter reprovado)", file=sys.stderr)
        return False
    if not any("triplo" in e and "3 arquivo" in e for e in errors):
        print(f"selftest: controle TRIPLO-MARCADOR FALHOU (nao descreveu a ambiguidade): {errors}", file=sys.stderr)
        return False
    print(f"selftest: controle TRIPLO-MARCADOR OK: {errors}")
    return True


# VERMELHO #3 (piso de varredura vazia, GODS_LAWS.md L-40): zero
# blocos em arquivo algum.
def selftest_top_level_empty_scan_reproves():
    errors = evaluate([], [])
    if not errors:
        print("selftest: VERMELHO#3 FALHOU (zero blocos deveria ter reprovado)", file=sys.stderr)
        return False
    if not any("varredura vazia" in e for e in errors):
        print(f"selftest: VERMELHO#3 FALHOU (sem 'varredura vazia'): {errors}", file=sys.stderr)
        return False
    print(f"selftest: VERMELHO#3 OK: {errors}")
    return True


# VERMELHO #4 - extracao vazia: o marcador existe dos dois lados, mas
# um deles teve a forma trocada (nenhum literal de string sobrevive
# dentro do bloco) - GODS_LAWS.md L-40 aplicado a leitura, nunca um
# "sem lista, sem problema" silencioso.
def selftest_empty_extraction_reproves():
    files = {
        "a.sh": _sh_block("forma_trocada", "alpha beta"),
        "b.ps1": (
            "# GLINTFX-SIBLING-LIST:forma_trocada:START\n"
            "$KNOWN_ADAPTER_CLASSES = @(1, 2)\n"  # sem aspas - forma trocada, nada para extrair
            "# GLINTFX-SIBLING-LIST:forma_trocada:END\n"
        ),
    }
    blocks, structural = _run(files)
    errors = evaluate(blocks, structural)
    if not errors:
        print("selftest: VERMELHO#4 FALHOU (extracao vazia deveria ter reprovado)", file=sys.stderr)
        return False
    if not any("extracao vazia" in e for e in errors):
        print(f"selftest: VERMELHO#4 FALHOU (sem 'extracao vazia'): {errors}", file=sys.stderr)
        return False
    print(f"selftest: VERMELHO#4 OK: {errors}")
    return True


# VERMELHO #5 - bloco nunca fechado (START sem END ate o fim do
# arquivo): nunca pode virar "grupo ignorado em silencio".
def selftest_unterminated_block_reproves():
    text = '# GLINTFX-SIBLING-LIST:sem_fim:START\nreadonly X="a b"\n'
    blocks, structural = _run({"a.sh": text})
    if blocks:
        print(f"selftest: VERMELHO#5 FALHOU (bloco sem END nao deveria virar bloco valido): {blocks}", file=sys.stderr)
        return False
    if not any("nunca fechou" in e for e in structural):
        print(f"selftest: VERMELHO#5 FALHOU (erro estrutural nao citou 'nunca fechou'): {structural}", file=sys.stderr)
        return False
    errors = evaluate(blocks, structural)
    if not errors:
        print("selftest: VERMELHO#5 FALHOU (evaluate() deveria propagar o erro estrutural)", file=sys.stderr)
        return False
    print(f"selftest: VERMELHO#5 OK (bloco sem END pego, nunca ignorado): {structural}")
    return True


# Controle: END sem START correspondente - mesma familia do vermelho
# #5, o outro lado do mesmo defeito de forma.
def selftest_end_without_start_reproves():
    text = '# GLINTFX-SIBLING-LIST:orfao:END\n'
    blocks, structural = _run({"a.sh": text})
    if not any("sem START correspondente" in e for e in structural):
        print(f"selftest: controle END-SEM-START FALHOU: {structural}", file=sys.stderr)
        return False
    print(f"selftest: controle END-SEM-START OK: {structural}")
    return True


# Controle NUNCA-POR-SEMELHANCA - a garantia central do desenho (item
# GATE-SIBLING-LIST, verbatim: "O desenho e OPT-IN POR MARCADOR
# EXPLICITO, nunca por semelhanca"): duas listas NAO marcadas, de nomes
# parecidos e conteudo DIVERGENTE, nunca geram erro nenhum - porque
# scan_file_for_blocks() so olha para texto dentro de um par de
# marcadores, e este arquivo nao tem nenhum.
def selftest_unmarked_divergence_never_flagged():
    files = {
        "check_dep_zero.py": 'ALLOWLIST = ["libwayland-client.so.0", "libEGL.so.1", "libGL.so.1"]\n',
        "check-dep-zero-win.ps1": '$ALLOWLIST = @("KERNEL32.dll", "USER32.dll", "VCRUNTIME140.dll")\n',
    }
    blocks, structural = _run(files)
    if blocks or structural:
        print(
            f"selftest: controle NUNCA-POR-SEMELHANCA FALHOU (texto sem marcador nao deveria "
            f"produzir bloco nem erro): blocks={blocks} structural={structural}",
            file=sys.stderr,
        )
        return False
    errors = evaluate(blocks, structural)
    # zero blocos SEM nenhum arquivo ter marcador algum e um caso
    # diferente do "varredura vazia" do mundo real (onde se espera
    # achar marcadores e nao se acha nenhum) - aqui o proprio real_main
    # so soma ao total quando "GLINTFX-SIBLING-LIST:" aparece no texto,
    # entao este par nunca entraria no scan de producao; o controle
    # prova apenas que scan_file_for_blocks() em si nao inventa bloco
    # a partir de conteudo parecido.
    print("selftest: controle NUNCA-POR-SEMELHANCA OK (listas parecidas e divergentes, sem marcador, ignoradas)")
    return True


def selftest_main():
    controls = [
        selftest_positive_control_mixed_syntax(),
        selftest_divergent_token_reproves(),
        selftest_orphan_marker_reproves(),
        selftest_triple_marker_reproves(),
        selftest_top_level_empty_scan_reproves(),
        selftest_empty_extraction_reproves(),
        selftest_unterminated_block_reproves(),
        selftest_end_without_start_reproves(),
        selftest_unmarked_divergence_never_flagged(),
    ]
    if not all(controls):
        print(f"{SCRIPT_NAME} --selftest: FALHOU (ver acima)", file=sys.stderr)
        sys.exit(1)
    print(f"{SCRIPT_NAME} --selftest: os {len(controls)} controles OK")


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
    elif args and args[0] == "--check":
        real_main(args[1:])
    else:
        fail("usage: check_sibling_lists.py --check <repo-root-directory>  |  --selftest")


if __name__ == "__main__":
    main()
