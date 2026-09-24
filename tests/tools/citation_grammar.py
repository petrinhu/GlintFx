#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# citation_grammar.py - DOCS-COUNT-VOCAB sub-fatia D1 (docs/plano-w7c.md
# sec. 3.D, decisao D14). Modulo unico da forma "citacao de medicao
# datada": um numero de contagem escrito em prosa e' isento do portao
# de vocabulario (check_docs_count_vocab.py, D2) quando a MESMA frase
# carrega uma citacao permanente que aponta para o evento que mediu
# aquele numero - nunca o numero em si (que apodrece), mas um
# identificador que "devolve o mesmo evento amanha" (docs/gl-loop-
# portability-matrix.md:38, a propria frase que legitimou esta regra:
# "an execution identifier is a different kind of thing... it returns
# the identical run tomorrow, so it never rots the way a number does").
#
# DUAS FORMAS FECHADAS, cada uma com o proprio autoteste:
#   - Execucao de CI: `gh run view <id>` inteiro entre crases (o
#     comando real que reproduz a consulta, nao so' o numero).
#   - Commit: um SHA hexadecimal (7 a 40 caracteres) entre crases.
#
# O QUE NAO E' CITACAO (GODS_LAWS.md L-40, declarado): a palavra "run"
# sozinha, sem numero (nao aponta pra evento nenhum); um SHA-formato
# fora de crase (indistinguivel de prosa comum sem o envelope que
# marca "isto e' um identificador tecnico", a mesma razao que faz
# `docs/api-conventions.md` exigir crase em nome de teste); uma data
# sozinha (nao e' um identificador de execucao - toda linha datada
# deste projeto tem data, e' o assunto do arquivo, nao prova de nada).
# CLAIM-CITATIONS (E2-E4, fora desta fatia) estende este modulo com a
# forma "teste citado"; PLAN-SCOPE-COLUMNS (F, fora desta fatia) reusa.
#
# Usage:
#   citation_grammar.py --selftest

import re
import sys

SCRIPT_NAME = "citation_grammar.py"

# `gh run view <digitos>` inteiro entre crases - o comando real, nao
# so' o numero (um numero solto entre crases seria indistinguivel de
# qualquer outro numero de codigo citado).
_RUN_CITATION_RE = re.compile(r"`gh run view (\d{4,})`")

# SHA de commit entre crases - hexadecimal, 7 a 40 caracteres (o
# intervalo real de abreviacao do git, curto a completo). Minusculo
# por convencao deste repositorio (todo SHA citado em TODO.md/planos
# e' minusculo); nao aceita maiusculo para nao casar um token
# hexadecimal-por-acaso de outra natureza (ex.: uma constante em
# CAIXA-ALTA).
_SHA_CITATION_RE = re.compile(r"`([0-9a-f]{7,40})`")


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


def has_dated_measurement_citation(text):
    """True quando `text` contem ao menos uma citacao de medicao
    datada - `gh run view <id>` inteiro entre crases, ou um SHA entre
    crases. Falso para as tres formas que NAO contam (run sem numero,
    SHA fora de crase, data sozinha) - nenhuma delas casa os dois
    regex acima, por desenho."""
    if _RUN_CITATION_RE.search(text):
        return True
    if _SHA_CITATION_RE.search(text):
        return True
    return False


# --- controles do --selftest -------------------------------------------


def _selftest_run_citation_accepted():
    text = "a prova roda no servidor (`gh run view 34975391524`), sem duvida."
    if not has_dated_measurement_citation(text):
        print(f"selftest: RUN-CITACAO FALHOU (deveria aceitar): {text!r}", file=sys.stderr)
        return False
    print("selftest: RUN-CITACAO OK (`gh run view <id>` entre crases e' citacao)")
    return True


def _selftest_sha_citation_accepted():
    text = "o conserto entrou em `14427bc`, verde nos dois sistemas."
    if not has_dated_measurement_citation(text):
        print(f"selftest: SHA-CITACAO FALHOU (deveria aceitar): {text!r}", file=sys.stderr)
        return False
    print("selftest: SHA-CITACAO OK (SHA entre crases e' citacao)")
    return True


# VERMELHO da tabela D1: "run sem numero" nao e' citacao - mesmo com o
# comando entre crases, sem o ID ele nao aponta pra evento nenhum.
def _selftest_run_without_number_rejected():
    text = "a prova roda no servidor (`gh run view`, sem citar qual), sem duvida."
    if has_dated_measurement_citation(text):
        print(f"selftest: RUN-SEM-NUMERO FALHOU (nao deveria aceitar): {text!r}", file=sys.stderr)
        return False
    print("selftest: RUN-SEM-NUMERO OK (mencao de 'run' sem numero nunca e' citacao)")
    return True


# VERMELHO da tabela D1: "SHA fora de crase" nao e' citacao.
def _selftest_sha_outside_backticks_rejected():
    text = "o conserto entrou em 14427bc, verde nos dois sistemas."
    if has_dated_measurement_citation(text):
        print(f"selftest: SHA-FORA-DE-CRASE FALHOU (nao deveria aceitar): {text!r}", file=sys.stderr)
        return False
    print("selftest: SHA-FORA-DE-CRASE OK (SHA sem crase nunca e' citacao)")
    return True


# VERMELHO da tabela D1: "data sozinha" nao e' citacao.
def _selftest_bare_date_rejected():
    text = "medido em 24/09/2026, sem outra prova."
    if has_dated_measurement_citation(text):
        print(f"selftest: DATA-SOZINHA FALHOU (nao deveria aceitar): {text!r}", file=sys.stderr)
        return False
    print("selftest: DATA-SOZINHA OK (data sozinha nunca e' citacao)")
    return True


def _selftest_empty_text_no_citation():
    if has_dated_measurement_citation(""):
        print("selftest: TEXTO-VAZIO FALHOU (nao deveria achar citacao em texto vazio)", file=sys.stderr)
        return False
    print("selftest: TEXTO-VAZIO OK (texto vazio nunca contem citacao)")
    return True


def selftest_main():
    controls = [
        _selftest_run_citation_accepted(),
        _selftest_sha_citation_accepted(),
        _selftest_run_without_number_rejected(),
        _selftest_sha_outside_backticks_rejected(),
        _selftest_bare_date_rejected(),
        _selftest_empty_text_no_citation(),
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
        fail("usage: citation_grammar.py --selftest")


if __name__ == "__main__":
    main()
