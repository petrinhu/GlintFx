#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_plan_scope_diff.py - PLAN-SCOPE-DIFF (achado do CTO, 06/09/2026,
# registrado em TODO.md sob "PLAN-SCOPE-DIFF"): a peneira de fechamento
# que teria pego a causa da noite inteira, não o sintoma. A fatia da
# fachada de janela (docs/plano-w6a-janela.md, linha da fatia 6, "W-D'
# headers públicos, fachada comum, concepts, `adapter()`") prometia DEZ
# caminhos como entregável e o commit que a fechou (`841e7ab`) só
# tinha CINCO - o fechamento declarou "compilação limpa, nenhum
# símbolo sem definição, N casos", tudo verdadeiro, e nada disso
# COMPARA o prometido com a árvore. Este script é essa comparação,
# feita mecanicamente em vez de por leitura.
#
# O QUE ELE FAZ: recebe o plano (um arquivo Markdown) e um MARCADOR -
# uma substring que aparece SÓ na linha da tabela da fatia em questão
# (nunca o número da linha sozinho: este mesmo documento tem outra
# tabela, a de riscos na seção 5, que também numera suas próprias
# linhas 1-8, e "a linha 6" sem mais contexto casaria com a errada -
# achado feito ao escrever este script, não hipotético). Extrai da
# COLUNA "Nasce / muda" dessa linha todo token entre crases que TEM
# CARA de caminho de arquivo (extensão reconhecida, sem parênteses,
# sem `::`, sem estar entre `<...>` como um header de sistema),
# expande a notação de chave `nome.{hpp,cpp}` em duas entradas, e
# confere cada uma contra a árvore: caminho completo (`a/b/c.hpp`)
# checa literal; nome solto (`c.hpp`, quando a prosa do plano some com
# o diretório por elipse - "e window_facade.cpp" depois de já ter dito
# o diretório da peça anterior) procura por NOME BASE em qualquer lugar
# da árvore rastreada - mais fraco que caminho completo, mas nunca
# falso-negativo por causa da elipse do português.
#
# GODS_LAWS.md L-40 (piso de varredura não-vazia): zero caminho
# EXTRAÍDO da linha é sempre sinal de o marcador não ter casado (linha
# errada, ou o marcador mudou de forma no documento) - nunca "esta
# fatia não promete nada", e reprova. Isso é diferente de "algum
# caminho promtido está ausente da árvore", que é o resultado real que
# decide se a onda fecha - impresso sempre, com a lista, nunca
# escondido atrás de um veredito único.
#
# ESCOPO DESTA FATIA (ordem explícita do time-lead, 06/09/2026): este
# script é o PASSO DO FECHAMENTO desta onda (w6a-janela), não um
# portão genérico para todo plano futuro - a generalização (aplicar a
# TODO plano, não só a uma linha citada à mão) é uma proposta do CTO
# que o líder ainda não decidiu, e expandir por conta própria aqui
# seria repetir exatamente o erro que abriu esta fatia (escopo que
# cresce sem ninguém notar).

import argparse
import re
import subprocess
import sys
from pathlib import Path

SCRIPT_NAME = "check_plan_scope_diff.py"

# Extensões que fazem um token entre crases "ter cara de caminho de
# arquivo" - deliberadamente CURTA e só de extensões reais deste
# projeto (GODS_LAWS.md stack), para não confundir `desc.logical_size`
# (não é extensão de arquivo nenhuma) com `display.hpp` (é).
FILE_EXTENSIONS = {"hpp", "cpp", "hxx", "cxx", "cc", "h", "py", "sh", "md", "txt", "yml", "in"}

BACKTICK_TOKEN = re.compile(r"`([^`]+)`")
BRACE_FORM = re.compile(r"^([\w./\-]+)\.\{([a-zA-Z,]+)\}$")
SIMPLE_FORM = re.compile(r"^([\w./\-]+)\.([a-zA-Z]+)$")


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


def find_row_line(plan_text, marker):
    """Returns the ONE line of `plan_text` that both starts a markdown
    table row (`| <digits> |`) and contains `marker` - never just "row
    number N", because this document has more than one table that
    numbers its own rows starting at 1 (this script's own header
    comment explains why that ambiguity is real, not hypothetical)."""
    matches = [
        line
        for line in plan_text.splitlines()
        if re.match(r"^\|\s*\d+\s*\|", line) and marker in line
    ]
    if len(matches) == 0:
        fail(f"nenhuma linha de tabela (\"| N | ...\") contendo o marcador {marker!r} encontrada")
    if len(matches) > 1:
        fail(
            f"{len(matches)} linhas de tabela contendo o marcador {marker!r} - marcador ambíguo, "
            "escolha uma substring que apareça em UMA só linha de tabela"
        )
    return matches[0]


def split_row_columns(row_line):
    """Splits a markdown table row on unescaped `|`, respecting `\\|`
    (the literal-pipe escape this same plan document already uses,
    e.g. fatia 1's own "...\\|windows\\|nenhum\\|SEM-PENDENCIA")."""
    placeholder = "\x00"
    protected = row_line.replace("\\|", placeholder)
    columns = [cell.strip() for cell in protected.split("|")]
    return [cell.replace(placeholder, "|") for cell in columns]


def extract_candidate_paths(column_text):
    """Returns the list of file-path-shaped tokens found in
    `column_text`'s own backtick spans - see this script's own header
    comment for the exact shape a token has to have."""
    candidates = []
    for token in BACKTICK_TOKEN.findall(column_text):
        if token.startswith("<") and token.endswith(">"):
            continue  # system header/URL form, e.g. `<windows.h>` - never ours to check.
        if "(" in token or ")" in token or "::" in token or " " in token:
            continue  # a call/method/template token, e.g. `adapter()`, `display_connection<A>::adapter()`.
        brace_match = BRACE_FORM.match(token)
        if brace_match:
            base, exts = brace_match.groups()
            for ext in exts.split(","):
                if ext in FILE_EXTENSIONS:
                    candidates.append(f"{base}.{ext}")
            continue
        simple_match = SIMPLE_FORM.match(token)
        if simple_match:
            _, ext = simple_match.groups()
            if ext in FILE_EXTENSIONS:
                candidates.append(token)
    return candidates


def path_exists_in_worktree(root, path):
    return (Path(root) / path).is_file()


def basename_exists_in_worktree(root, name):
    result = subprocess.run(
        ["git", "-C", str(root), "ls-files"], capture_output=True, text=True, check=True
    )
    return any(Path(line).name == name for line in result.stdout.splitlines())


def path_exists_in_ref(root, ref, path):
    result = subprocess.run(
        ["git", "-C", str(root), "cat-file", "-e", f"{ref}:{path}"],
        capture_output=True,
        check=False,
    )
    return result.returncode == 0


def basename_exists_in_ref(root, ref, name):
    result = subprocess.run(
        ["git", "-C", str(root), "ls-tree", "-r", "--name-only", ref],
        capture_output=True,
        text=True,
        check=True,
    )
    return any(Path(line).name == name for line in result.stdout.splitlines())


def check_existence(root, ref, candidates):
    """Returns (existing, missing) - `ref` of None means "the live
    working tree", matching this script's own --ref CLI default."""
    existing, missing = [], []
    for path in candidates:
        is_bare_name = "/" not in path
        if ref is None:
            found = (
                basename_exists_in_worktree(root, path)
                if is_bare_name
                else path_exists_in_worktree(root, path)
            )
        else:
            found = (
                basename_exists_in_ref(root, ref, path)
                if is_bare_name
                else path_exists_in_ref(root, ref, path)
            )
        (existing if found else missing).append(path)
    return existing, missing


def real_main(args):
    parser = argparse.ArgumentParser(prog=SCRIPT_NAME, add_help=False)
    parser.add_argument("plan", help="caminho do plano (Markdown)")
    parser.add_argument("--marker", required=True)
    parser.add_argument("--ref", default=None, help="SHA/branch a conferir - omitido = árvore de trabalho")
    parser.add_argument("--root", default=".")
    parsed = parser.parse_args(args)

    plan_text = Path(parsed.plan).read_text(encoding="utf-8")
    row_line = find_row_line(plan_text, parsed.marker)
    columns = split_row_columns(row_line)
    # split_row_columns() splits on EVERY `|`, including the leading
    # one before the first cell and the trailing one after the last -
    # columns[0] is always "" (before "| # |"), so column 4 (not 3) is
    # "Nasce / muda": [0]="", [1]="#", [2]="Fatia", [3]="Lado",
    # [4]="Nasce / muda" (docs/plano-w6a-janela.md's own table header,
    # section 2.2) - an off-by-one here silently reads "Lado" ("comum"/
    # "Wayland"/"Windows", never a file path) instead, which the
    # extraction floor below would ALSO have caught as zero candidates,
    # but proving the right cause here is cheaper than making the
    # reader guess (this exact bug was caught live while writing this
    # script, against the real plan file, not hypothetical).
    if len(columns) < 5:
        fail(
            f"linha da tabela tem {len(columns)} coluna(s), esperava pelo menos 5 "
            "(vazio | # | Fatia | Lado | Nasce/muda | ...)"
        )
    deliverable_column = columns[4]

    candidates = extract_candidate_paths(deliverable_column)
    print(f"{SCRIPT_NAME}: {len(candidates)} caminho(s) citado(s) na coluna de entregáveis")
    if len(candidates) == 0:
        fail(
            "0 caminho(s) citado(s) - varredura vazia (GODS_LAWS.md L-40): o marcador achou a "
            "linha errada, ou a coluna de entregáveis mudou de forma sem este script acompanhar"
        )

    existing, missing = check_existence(parsed.root, parsed.ref, candidates)
    ref_label = parsed.ref if parsed.ref is not None else "árvore de trabalho"
    print(f"{SCRIPT_NAME}: contra {ref_label} - {len(existing)} existe(m), {len(missing)} falta(m)")
    for path in candidates:
        marker_char = "OK " if path in existing else "FALTA"
        print(f"  [{marker_char}] {path}")

    if missing:
        fail(
            f"{len(missing)} caminho(s) prometido(s) pela fatia ausente(s) da árvore ({ref_label}): "
            + ", ".join(missing)
        )


# --- selftest -----------------------------------------------------

def selftest_extraction_matches_real_row():
    row_text = (
        '`include/glintfx/platform/window/display.hpp` e `window.hpp` (forma); '
        '`src/platform/port/display_backend_port.hpp` e `window_adapter_port.hpp` (concepts); '
        '`display_connection<A>::adapter()`; '
        '`src/platform/window/display_facade.cpp` e `window_facade.cpp` sobre '
        '`selected_display_backend`/`selected_window_adapter`; '
        '`src/platform/wayland/display_backend.{hpp,cpp}` (compõe); '
        '`tests/hostile_win32_macros_shim.hpp`; cobertura de higiene; '
        '`prepare_arch_ports_fixture.sh` estagia `src/platform/window/` e os `.cpp` novos. '
        'cada célula conferida em `learn.microsoft.com`, `<windows.h>`, `adapter()`, `desc.logical_size`'
    )
    candidates = extract_candidate_paths(row_text)
    expected = {
        "include/glintfx/platform/window/display.hpp",
        "window.hpp",
        "src/platform/port/display_backend_port.hpp",
        "window_adapter_port.hpp",
        "src/platform/window/display_facade.cpp",
        "window_facade.cpp",
        "src/platform/wayland/display_backend.hpp",
        "src/platform/wayland/display_backend.cpp",
        "tests/hostile_win32_macros_shim.hpp",
        "prepare_arch_ports_fixture.sh",
    }
    if set(candidates) != expected:
        print(
            f"selftest: extracao nao bateu com a linha real da fatia 6 - achou {sorted(candidates)}, "
            f"esperava {sorted(expected)}",
            file=sys.stderr,
        )
        return False
    print("selftest: extração bateu os 10 caminhos exatos da linha real da fatia 6 - ok")
    return True


def selftest_end_to_end_column_offset():
    """Exercises find_row_line() + split_row_columns() TOGETHER
    against a fake table shaped exactly like docs/plano-w6a-janela.md's
    own (# | Fatia | Lado | Nasce/muda | Prova Linux | Prova Windows |
    Par) - selftest_extraction_matches_real_row() above only calls
    extract_candidate_paths() directly and would NOT have caught the
    off-by-one this script actually shipped with once (column 3 read
    "Lado", never the deliverables column 4) - real_main() itself
    caught it, live, against the real plan file, which is exactly the
    gap this end-to-end control now closes."""
    import tempfile

    content = (
        "| # | Fatia | Lado | Nasce / muda | Prova Linux | Prova Windows | Par |\n"
        "|---|---|---|---|---|---|---|\n"
        "| 6 | **MARCADOR-E2E** | comum | `a/b/real_path.hpp` | prova | prova | par |\n"
    )
    with tempfile.NamedTemporaryFile(mode="w", suffix=".md", delete=False) as handle:
        handle.write(content)
        plan_path = handle.name
    root_dir = Path(plan_path).parent
    (root_dir / "a").mkdir(exist_ok=True)
    (root_dir / "a" / "b").mkdir(exist_ok=True)
    (root_dir / "a" / "b" / "real_path.hpp").write_text("", encoding="utf-8")
    try:
        try:
            real_main([plan_path, "--marker", "MARCADOR-E2E", "--root", str(root_dir)])
        except SystemExit as exc:
            print(f"selftest: pipeline completa reprovou inesperadamente (codigo {exc.code}) - "
                  "a coluna certa nao foi lida", file=sys.stderr)
            return False
        print("selftest: pipeline completa (linha->colunas->extracao->existencia) leu a coluna "
              "certa (4, 'Nasce/muda', nunca 3, 'Lado') - ok")
        return True
    finally:
        Path(plan_path).unlink(missing_ok=True)


def selftest_zero_candidates_reproves():
    import contextlib
    import io

    row_text = "sem caminho nenhum aqui, so' `adapter()` e `<windows.h>` e `desc.logical_size`"
    import tempfile

    with tempfile.NamedTemporaryFile(mode="w", suffix=".md", delete=False) as handle:
        handle.write(f"| 1 | **MARCADOR-TESTE** | comum | {row_text} | a | b | c |\n")
        plan_path = handle.name
    try:
        buffer = io.StringIO()
        with contextlib.redirect_stderr(buffer):
            try:
                real_main([plan_path, "--marker", "MARCADOR-TESTE"])
            except SystemExit as exc:
                if exc.code == 1 and "varredura vazia" in buffer.getvalue():
                    print("selftest: zero caminhos citados reprova (RED, piso L-40) - ok")
                    return True
                print(f"selftest: codigo/mensagem inesperados: {exc.code} / {buffer.getvalue()}",
                      file=sys.stderr)
                return False
        print("selftest: zero caminhos citados NAO reprovou - esperado exit 1", file=sys.stderr)
        return False
    finally:
        Path(plan_path).unlink(missing_ok=True)


def selftest_ambiguous_marker_reproves():
    """This document has TWO tables that both number their own rows
    starting at 1 (this script's own header comment) - a marker that
    accidentally matches a row in each of them must reprove, never
    silently pick the first one it finds."""
    import tempfile

    content = (
        "| 6 | **fatia A, MARCADOR-COMUM** | x | `a.hpp` | p | q | r |\n"
        "| 6 | **risco B, MARCADOR-COMUM** | y | `b.hpp` | p | q | r |\n"
    )
    with tempfile.NamedTemporaryFile(mode="w", suffix=".md", delete=False) as handle:
        handle.write(content)
        plan_path = handle.name
    try:
        try:
            real_main([plan_path, "--marker", "MARCADOR-COMUM"])
        except SystemExit as exc:
            if exc.code == 1:
                print("selftest: marcador que casa DUAS linhas reprova (nunca escolhe a primeira "
                      "em silencio) - ok")
                return True
            print(f"selftest: codigo inesperado {exc.code} para marcador ambiguo", file=sys.stderr)
            return False
        print("selftest: marcador ambiguo NAO reprovou - esperado exit 1", file=sys.stderr)
        return False
    finally:
        Path(plan_path).unlink(missing_ok=True)


def selftest_main():
    controls = [
        selftest_extraction_matches_real_row(),
        selftest_end_to_end_column_offset(),
        selftest_zero_candidates_reproves(),
        selftest_ambiguous_marker_reproves(),
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
            "usage: check_plan_scope_diff.py --compare <plano.md> --marker <substring> "
            "[--ref <sha>] [--root <dir>]  |  --selftest"
        )


if __name__ == "__main__":
    main()
