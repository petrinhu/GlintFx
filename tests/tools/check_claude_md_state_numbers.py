#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_claude_md_state_numbers.py - GODS_LAWS.md L-36/L-40 gate.
#
# INBOX (item novo do lider, 06/09/2026 - docs/auditoria-decisoes-
# autonomas.md F2): "não criar portão mecânico para número volátil em
# documento" (decisão de 25/08/2026) já custou o mesmo defeito QUATRO
# vezes em README.md, sempre depois de ser consertado uma vez - a
# lider mediu, no proprio registro da auditoria, quatro reincidencias
# em duas semanas. check_readme_volatile_numbers.py fechou essa classe
# para README.md; "o CLAUDE.md continua sem portao" (mesma auditoria,
# verbatim) - o proprio CLAUDE.md deste projeto documenta a REGRA
# ("todo numero que muda por fatia entra como o comando que o mede,
# nunca como o valor medido - comando nao apodrece, numero escrito
# sim", secao "Estado atual do repositorio") mas nunca teve nada
# mecanico vigiando-a - so' o "proximo editor tropeça nela", que e'
# exatamente a classe de controle humano a lei do lider sobre portoes
# existe para substituir.
#
# ESCOPO DELIBERADAMENTE ESTREITO (nao o documento inteiro): medido
# antes de escrever este gate (GODS_LAWS.md L-43, criterio fixado antes
# do dado) - um escaneamento de digito cru no CLAUDE.md INTEIRO produz
# dezenas de falsos positivos legitimos que o README nunca teve (itens
# de lista numerada, "C++23", labels de onda "W1"/"W2", nomes de item
# como "GFSS-*"/"R2D-*") - o README nao tem NADA disso porque o gate
# la' e' ancorado a dois paragrafos de narrativa especificos, nao ao
# arquivo inteiro. A secao "## Estado atual do repositorio" e'
# DIFERENTE: ela mesma se declara, no proprio texto, "esta secao nao
# carrega contador" - e' exatamente o unico lugar deste documento onde
# QUALQUER digito residual (fora das isencoes abaixo) e' uma violacao
# da propria regra que a secao anuncia, nao uma leitura falsa.
#
# ISENCOES (vocabulario fechado - o MESMO que check_readme_volatile_
# numbers.py ja provou zero falso positivo, mais duas novas, medidas
# contra a arvore real desta secao antes deste gate nascer):
#   1. Span entre crases (`...`) - codigo, comando, nome de arquivo.
#   2. Citacao de lei `L-NN`.
#   3. Data `DD/MM/AAAA` ou `DD/MM/AA` - fato calendario, nunca conta
#      derivada do repositorio.
#   4. Numero de versao `vA.B(.C(.D))` ou `A.B(.C(.D))` - identificador
#      congelado, nao contagem.
#   5. Hash de commit hexadecimal (7 a 40 caracteres hex) fora de crase
#      (a mesma prosa as vezes cita um SHA sem formatar como codigo).
#   6. Referencia de secao `§N` de outro documento (ex.: `ESCOPO.md
#      §1`) - identificador de estrutura daquele documento, nao uma
#      contagem derivada deste repositorio.
#
# Usage:
#   check_claude_md_state_numbers.py <claude-md-path>
#   check_claude_md_state_numbers.py --selftest

import re
import sys
from pathlib import Path

SCRIPT_NAME = "check_claude_md_state_numbers.py"

SECTION_HEADING = "## Estado atual do repositório"

_BACKTICK_SPAN_RE = re.compile(r"`[^`]*`")
_LAW_CITATION_RE = re.compile(r"\bL-\d+\b")
_DATE_RE = re.compile(r"\b\d{1,2}/\d{1,2}/\d{2,4}\b")
_VERSION_RE = re.compile(r"\bv?\d+\.\d+(?:\.\d+)*\b")
_HEX_SHA_RE = re.compile(r"\b[0-9a-f]{7,40}\b")
_SECTION_REF_RE = re.compile(r"§\s*\d+")
# \b on both sides - a BARE number token, never a digit embedded inside
# an identifier that also has a letter (measured false positive this
# fixes: "Catch2"/"GoogleTest" in this section's own harness bullet -
# \b requires a transition between a \w and a non-\w character, and
# both 'h' and '2' in "Catch2" are \w, so there is no boundary between
# them for a bare \d to exploit; a standalone "56 commits" still
# matches in full).
_DIGIT_RE = re.compile(r"\b\d+\b")


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


def extract_state_section(text, heading=SECTION_HEADING):
    """Returns the text from `heading` (exclusive of the heading line
    itself) up to the next `## ` heading, or None if `heading` is not
    found at all - a missing section is a FACT to report (GODS_LAWS.md
    L-40), never silently skipped as if it passed."""
    start = text.find(heading)
    if start == -1:
        return None
    body_start = start + len(heading)
    rest = text[body_start:]
    match = re.search(r"\n## ", rest)
    end = body_start + match.start() if match else len(text)
    return text[body_start:end]


def strip_exemptions(section_text):
    stripped = _BACKTICK_SPAN_RE.sub("", section_text)
    stripped = _LAW_CITATION_RE.sub("", stripped)
    stripped = _DATE_RE.sub("", stripped)
    stripped = _VERSION_RE.sub("", stripped)
    stripped = _HEX_SHA_RE.sub("", stripped)
    stripped = _SECTION_REF_RE.sub("", stripped)
    return stripped


def find_violations(section_text):
    """Returns a list of (line_number, line_text) pairs, 1-indexed
    RELATIVE TO THE SECTION, for every line that still carries a digit
    after every exemption above is stripped."""
    stripped = strip_exemptions(section_text)
    violations = []
    for line_no, line in enumerate(stripped.splitlines(), start=1):
        if _DIGIT_RE.search(line):
            violations.append((line_no, line.strip()))
    return violations


def check_claude_md_state_numbers(claude_md_path):
    try:
        text = Path(claude_md_path).read_text(encoding="utf-8")
    except OSError as exc:
        print(f"{SCRIPT_NAME}: nao foi possivel ler {claude_md_path} ({exc})", file=sys.stderr)
        return False

    section = extract_state_section(text)
    if section is None:
        print(
            f"{SCRIPT_NAME}: secao {SECTION_HEADING!r} nao encontrada em {claude_md_path} "
            f"(GODS_LAWS.md L-40: ausencia e' falha, nunca sucesso silencioso)",
            file=sys.stderr,
        )
        return False

    violations = find_violations(section)
    if violations:
        print(
            f"{SCRIPT_NAME}: {len(violations)} linha(s) da secao {SECTION_HEADING!r} carregam "
            f"digito fora das isencoes (numero volatil escrito em vez do comando que o mede):",
            file=sys.stderr,
        )
        for line_no, line in violations:
            print(f"  secao:linha {line_no}: {line[:160]}", file=sys.stderr)
        return False

    print(
        f"{SCRIPT_NAME}: 0 digito(s) fora das isencoes em {SECTION_HEADING!r} "
        f"({len(section.splitlines())} linha(s) varrida(s))"
    )
    return True


# --- selftest ------------------------------------------------------------


def selftest_clean_section_passes():
    text = (
        f"# Doc\n\n{SECTION_HEADING}\n\n"
        "Regra: nenhum numero solto aqui. Data valida: 21/08/2026. Lei: L-04. "
        "Versao: `v0.1.0.0`. Comando: `git rev-list --count HEAD`. SHA: `a1b2c3d`.\n\n"
        "## Outra secao\n\nConteudo com 42 numeros aqui - fora do escopo, ignorado.\n"
    )
    section = extract_state_section(text)
    if section is None:
        print("selftest: controle CLEAN-SECTION FALHOU (secao nao encontrada)", file=sys.stderr)
        return False
    violations = find_violations(section)
    if violations:
        print(f"selftest: controle CLEAN-SECTION FALHOU (achou {violations})", file=sys.stderr)
        return False
    print("selftest: controle CLEAN-SECTION OK (secao limpa nao reprova)")
    return True


def selftest_bare_digit_fails():
    text = (
        f"# Doc\n\n{SECTION_HEADING}\n\n"
        "Hoje o repositorio tem 56 commits e 9 casos de teste.\n\n"
        "## Outra secao\n\nignorado\n"
    )
    section = extract_state_section(text)
    violations = find_violations(section)
    if not violations:
        print(
            "selftest: controle BARE-DIGIT FALHOU (deveria reprovar '56 commits'/'9 casos')",
            file=sys.stderr,
        )
        return False
    print(f"selftest: controle BARE-DIGIT OK (reprovou {len(violations)} linha(s))")
    return True


def selftest_outside_section_ignored():
    text = (
        f"# Doc\n\n{SECTION_HEADING}\n\nlimpo aqui\n\n"
        "## Outra secao\n\n56 numeros soltos aqui, fora do escopo deste gate\n"
    )
    section = extract_state_section(text)
    violations = find_violations(section)
    if violations:
        print(
            "selftest: controle OUTSIDE-SECTION FALHOU (numero fora da secao nao deveria contar)",
            file=sys.stderr,
        )
        return False
    print("selftest: controle OUTSIDE-SECTION OK (numero fora da secao alvo e' ignorado)")
    return True


def selftest_missing_section_fails(tmp_path):
    text = "# Doc\n\nSem a secao alvo aqui.\n"
    tmp_path.write_text(text, encoding="utf-8")
    ok = check_claude_md_state_numbers(tmp_path)
    if ok:
        print("selftest: controle MISSING-SECTION FALHOU (deveria reprovar)", file=sys.stderr)
        return False
    print("selftest: controle MISSING-SECTION OK (secao ausente reprova, nunca passa em silencio)")
    return True


def selftest_real_tree_passes(claude_md_path):
    ok = check_claude_md_state_numbers(claude_md_path)
    if not ok:
        print("selftest: controle REAL-TREE FALHOU (o CLAUDE.md real deveria passar hoje)", file=sys.stderr)
        return False
    print("selftest: controle REAL-TREE OK (o CLAUDE.md real deste repositorio passa)")
    return True


def selftest_main():
    import tempfile

    with tempfile.TemporaryDirectory(prefix="glintfx-claude-md-state-numbers-selftest-") as tmp_dir:
        tmp_path = Path(tmp_dir) / "CLAUDE.md"
        real_claude_md = Path(__file__).resolve().parents[2] / "CLAUDE.md"
        controls = [
            selftest_clean_section_passes(),
            selftest_bare_digit_fails(),
            selftest_outside_section_ignored(),
            selftest_missing_section_fails(tmp_path),
            selftest_real_tree_passes(real_claude_md),
        ]
    if not all(controls):
        print(f"{SCRIPT_NAME} --selftest: FALHOU (ver acima)", file=sys.stderr)
        sys.exit(1)
    print(f"{SCRIPT_NAME} --selftest: os {len(controls)} controles OK")


def real_main(args):
    if len(args) != 1:
        fail("usage: check_claude_md_state_numbers.py <claude-md-path>")
    path = args[0]
    if not Path(path).is_file():
        fail(f"file not found: {path}")
    if not check_claude_md_state_numbers(path):
        fail("numero volatil fora das isencoes na secao 'Estado atual do repositorio' (ver mensagem acima)")


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
    else:
        real_main(args)


if __name__ == "__main__":
    main()
