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
#
# FORMA "TESTE CITADO" (CLAIM-CITATIONS E2-E4, docs/plano-w7c.md sec.
# 3.E, docs/plano-w7c-adendo-revalidacao.md sec. 3.E): uma alegacao de
# medicao/igualdade-entre-sistemas num comentario e' citada quando o
# MESMO bloco de comentario tem uma frase-gatilho ("Proved by:",
# "proven by", "see") E o nome de um teste que EXISTE no inventario
# fechado (tests/tools/test_name_inventory.py, E1). has_cited_test_
# reference() recebe DUAS juncoes do mesmo bloco - separadas por
# espaco (o caso comum: uma frase que o reflow do comentario quebrou
# numa fronteira de palavra de verdade) e concatenadas sem separador
# (GODS_LAWS.md memoria feedback_referencia_quebrada_pela_linha: um
# identificador longo, sem espaco nenhum no ponto onde o reflow
# quebrou, como "window_desc_" + "\n" + "validation_test.cpp's own" -
# so a juncao SEM separador reconstroi "window_desc_validation_test")
# - porque o chamador (extrator de bloco) nao sabe, so olhando o
# texto, qual dos dois casos aconteceu em cada quebra de linha.
#
# O QUE ISTO NAO VE (GODS_LAWS.md L-40): um nome de teste citado sem
# NENHUMA das tres frases-gatilho por perto (prosa que so MENCIONA um
# teste, sem alegar que ele PROVA a alegacao corrente, nao conta -
# nome sozinho no bloco nunca e citacao); a busca e por SUBSTRING com
# fronteira de palavra (`\bnome\b`) no bloco inteiro, nunca ancorada a
# uma distancia fixa da frase-gatilho - um bloco muito longo com um
# nome de teste em outro paragrafo, sem relacao com a frase-gatilho
# deste, pode aceitar por coincidencia; o risco e aceito porque os
# nomes de teste deste projeto sao longos e descritivos (varias
# palavras coladas por "_"), tornando colisao por acaso rara na
# pratica, e o cabeçalho do portao que consome esta funcao
# (check_claim_citations.py) declara este limite tambem.
# PLAN-SCOPE-COLUMNS (F, fora desta fatia) reusa este modulo.
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


# --- forma "teste citado" (CLAIM-CITATIONS E2-E4) -----------------------

_PROVED_BY_RE = re.compile(r"\bproved\s+by\b\s*:?", re.IGNORECASE)
_PROVEN_BY_RE = re.compile(r"\bproven\s+by\b", re.IGNORECASE)
_SEE_RE = re.compile(r"\bsee\b", re.IGNORECASE)


def has_citation_trigger_phrase(spaced_text):
    """True quando `spaced_text` contem alguma das tres frases-gatilho
    de citacao de teste ("Proved by:"/"proven by"/"see"). Funcao
    separada de has_cited_test_reference() porque um chamador (por
    exemplo, uma exceptions.txt) pode precisar saber que HAVIA
    intencao de citar, mesmo quando o nome citado nao existe."""
    return bool(_PROVED_BY_RE.search(spaced_text) or _PROVEN_BY_RE.search(spaced_text) or _SEE_RE.search(spaced_text))


def has_cited_test_reference(spaced_text, tight_text, known_test_names):
    """True quando o bloco (as duas juncoes do MESMO texto - ver o
    comentario do topo deste modulo para o porque das duas) tem uma
    frase-gatilho E o nome de um teste que EXISTE em
    `known_test_names` em qualquer uma das duas juncoes. Devolve o
    nome citado (o primeiro achado, ordem de `known_test_names`), ou
    None quando nao ha frase-gatilho, ou o nome citado nao existe no
    inventario (a mesma "citacao de teste que nao existe reprova" da
    tabela E3 - devolver None aqui e' o que faz o chamador tratar como
    NAO citado, nunca aceitar uma citacao fantasma)."""
    if not has_citation_trigger_phrase(spaced_text):
        return None
    for name in known_test_names:
        pattern = re.compile(r"\b" + re.escape(name) + r"\b")
        if pattern.search(spaced_text) or pattern.search(tight_text):
            return name
    return None


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


# VERMELHO: frase-gatilho + nome real do inventario, juntos no bloco -
# tem que aceitar, e devolver o nome exato.
def _selftest_cited_test_reference_accepted():
    known = {"window_parity_test", "rslt_test"}
    spaced = "None of these values is cached. Proved by: window_parity_test (both platforms, same name)."
    got = has_cited_test_reference(spaced, spaced, known)
    if got != "window_parity_test":
        print(f"selftest: TESTE-CITADO FALHOU (deveria aceitar e devolver o nome): {got!r}", file=sys.stderr)
        return False
    print("selftest: TESTE-CITADO OK ('Proved by:' + nome real aceito, nome devolvido)")
    return True


# VERMELHO da tabela E3: "citacao de teste que nao existe reprova" -
# frase-gatilho presente, mas o nome citado NAO esta no inventario.
def _selftest_cited_test_reference_rejected_unknown_name():
    known = {"window_parity_test"}
    spaced = "Refusal proved by nome_inventado_test, four cases, no container."
    got = has_cited_test_reference(spaced, spaced, known)
    if got is not None:
        print(f"selftest: TESTE-INVENTADO FALHOU (nome que nao existe no inventario foi aceito): {got!r}", file=sys.stderr)
        return False
    print("selftest: TESTE-INVENTADO OK (nome fora do inventario nunca e citacao)")
    return True


# VERMELHO: nome real do inventario mencionado no bloco, mas SEM
# nenhuma frase-gatilho por perto - mencao sozinha nunca e citacao
# (o modulo nao pode aceitar so por o nome aparecer no texto).
def _selftest_cited_test_reference_requires_trigger_phrase():
    known = {"window_parity_test"}
    spaced = "window_parity_test also exercises the same refusal case, unrelated to this claim."
    got = has_cited_test_reference(spaced, spaced, known)
    if got is not None:
        print(f"selftest: SEM-GATILHO FALHOU (nome sem frase-gatilho foi aceito como citacao): {got!r}", file=sys.stderr)
        return False
    print("selftest: SEM-GATILHO OK (nome de teste mencionado sem 'Proved by:'/'proven by'/'see' nunca e citacao)")
    return True


# VERMELHO: identificador quebrado pelo reflow do comentario sem
# espaco no ponto da quebra (GODS_LAWS.md memoria feedback_
# referencia_quebrada_pela_linha) - so a juncao SEM separador
# (tight_text) reconstroi o nome; a juncao COM espaco (spaced_text)
# quebra o nome em dois tokens e nao acha. Mutante que mata: chamar
# has_cited_test_reference() so com spaced_text nos dois parametros.
def _selftest_cited_test_reference_reconstructs_wrapped_identifier():
    known = {"window_desc_validation_test"}
    lines = [
        "Refusal proven by window_desc_",
        "validation_test.cpp's own both_dimensions_zero_is_rejected.",
    ]
    spaced_text = " ".join(lines)
    tight_text = "".join(lines)
    if has_cited_test_reference(spaced_text, spaced_text, known) is not None:
        print("selftest: NOME-QUEBRADO FALHOU (spaced_text sozinho nao deveria reconstruir o nome partido)", file=sys.stderr)
        return False
    got = has_cited_test_reference(spaced_text, tight_text, known)
    if got != "window_desc_validation_test":
        print(f"selftest: NOME-QUEBRADO FALHOU (tight_text deveria reconstruir o nome partido): {got!r}", file=sys.stderr)
        return False
    print("selftest: NOME-QUEBRADO OK (tight_text reconstroi identificador quebrado pelo reflow, spaced_text sozinho nao)")
    return True


def selftest_main():
    controls = [
        _selftest_run_citation_accepted(),
        _selftest_sha_citation_accepted(),
        _selftest_run_without_number_rejected(),
        _selftest_sha_outside_backticks_rejected(),
        _selftest_bare_date_rejected(),
        _selftest_empty_text_no_citation(),
        _selftest_cited_test_reference_accepted(),
        _selftest_cited_test_reference_rejected_unknown_name(),
        _selftest_cited_test_reference_requires_trigger_phrase(),
        _selftest_cited_test_reference_reconstructs_wrapped_identifier(),
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
