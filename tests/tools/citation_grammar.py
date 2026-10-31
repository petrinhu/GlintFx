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
# `docs/api-conventions.md` exigir crase num nome de teste feito so de
# letras minusculas); uma data
# sozinha (nao e' um identificador de execucao - toda linha datada
# deste projeto tem data, e' o assunto do arquivo, nao prova de nada).
#
# FORMA "TESTE CITADO" (CLAIM-CITATIONS E2-E4, docs/plano-w7c.md sec.
# 3.E; CITATION-COMMON-WORD, TODO.md): uma alegacao de medicao/
# igualdade-entre-sistemas num comentario e' citada quando uma
# frase-gatilho do bloco e' SEGUIDA, NA MESMA FRASE, pelo nome de um
# teste que EXISTE no inventario fechado (tests/tools/test_name_
# inventory.py, E1). Tres regras, cada uma com o proprio vermelho no
# --selftest:
#
#   1. FRASES-GATILHO (fechadas): "see"; e "proved"/"proven" seguido
#      de "by", com ate QUATRO palavras no meio, separadas por espaco
#      ou virgula ("Proved by:", "proven by", "proved live by", "Proved
#      at compile time by", "proven, not promised, by" - as formas que
#      a arvore real escreve, medidas em 27/10/2026). Cinco palavras ou
#      mais ja nao sao a mesma locucao.
#   2. ESCOPO DE FRASE: o nome tem de vir DEPOIS da frase-gatilho e
#      ANTES do fim da frase dela. Fim de frase: ".", "!" ou "?",
#      seguidos (depois de fechamentos opcionais ")", "]" ou "'") do
#      comeco da frase seguinte - letra MAIUSCULA de A a Z, crase, "("
#      ou aspas duplas, com ou sem espaco antes (a juncao sem
#      separador cola as linhas) - ou do fim do texto. As aspas duplas
#      logo depois do terminador SEMPRE abrem a frase seguinte, mesmo
#      com minuscula depois delas: por isso elas nao estao entre os
#      fechamentos, onde seriam letra morta (o lookahead ja as aceita
#      como abertura; a retirada foi provada equivalente por
#      enumeracao na CITATION-COMMON-WORD-fix2). Cada caractere das
#      tres classes tem o proprio vermelho no --selftest. Por isso o
#      ponto de "math2d_test.cpp", de "GODS_LAWS.md" ou de "e.g. the"
#      nao fecha a frase (minuscula depois). Antes desta regra, qualquer
#      nome do inventario em QUALQUER lugar do bloco contava se uma
#      frase-gatilho existisse em qualquer outro lugar dele: medido na
#      pre-execucao da S6 (D-W8-1086), o exemplo `shapes` entrou no
#      inventario e a prosa "three parallel {magnitude, unit} shapes"
#      de include/glintfx/gfss/value.hpp passou a contar como citacao,
#      por causa de um "see" de outro paragrafo.
#   3. FORMA DE IDENTIFICADOR: um nome feito SO de letras minusculas
#      ("shapes", "textures") se le como palavra comum, e so conta
#      ENTRE CRASES, uma colada antes e outra colada depois (`shapes`).
#      Uma crase de um lado so e' caractere comum, como no CommonMark
#      ("When a backtick string is not closed by a matching backtick
#      string, we just have literal backticks"). Nome com qualquer
#      outro caractere ("_", digito, maiuscula) e' identificador por
#      si. E' a regra do
#      Doxygen para link automatico ("words that contain at least one
#      non-lower case character", e "\ref" para nome todo minusculo):
#      sem ela, "Proved by: the shapes it draws" na MESMA frase seria
#      citacao falsa.
#
# O NOME CONTA INTEIRO, nas tres regras: o nome do inventario so casa
# entre fronteiras de palavra (\b) dos dois lados, e "_" e digito sao
# parte da palavra. "wire_relay" (nome real) nao casa dentro de
# "wire_relay_inventado_test" (que nao existe), nem "relay_test" no fim
# de "wire_relay_test" - a mesma familia do link automatico que pegava
# parte de uma palavra (Trac, ticket 322: "AlaBamA" linkava "AlaBam").
# O "." e o "-" sao fronteira: "math2d_test.cpp" cita math2d_test, a
# forma que a arvore escreve; pela mesma regra "wire_relay-x" citaria
# wire_relay (perda declarada, L-40: hifen nao e' forma de nome de
# teste deste projeto, que e' snake_case pela L-21).
#
# has_cited_test_reference() recebe DUAS juncoes do mesmo bloco -
# separadas por espaco (o caso comum: uma frase que o reflow do
# comentario quebrou numa fronteira de palavra de verdade) e
# concatenadas sem separador (GODS_LAWS.md memoria feedback_referencia_
# quebrada_pela_linha: um identificador longo, sem espaco nenhum no
# ponto onde o reflow quebrou, como "window_desc_" + "\n" +
# "validation_test.cpp's own" - so a juncao SEM separador reconstroi
# "window_desc_validation_test") - porque o chamador (extrator de
# bloco) nao sabe, so olhando o texto, qual dos dois casos aconteceu
# em cada quebra de linha. As tres regras valem nas duas juncoes.
#
# O QUE ISTO NAO VE (GODS_LAWS.md L-40, declarado): um nome de teste
# sem NENHUMA frase-gatilho antes dele na mesma frase ("Exercised by
# `x`", "`x` proves this") nao conta - escreva a citacao numa das
# formas acima; o fim de frase e' uma regra de texto, nao gramatica:
# uma frase que comeca por minuscula depois de ".", "!" ou "?"
# continua a anterior (o escopo fica MAIS LARGO; depois de "!" e de
# "?", mais largo tambem que o da regra das frases do Unicode, UAX
# #29, que so poupa o ponto antes de minuscula, SB8, e quebra depois
# dos outros dois, SB11; medido em 31/10/2026 nas duas juncoes de
# todo bloco dos cabecalhos publicos: nenhum "!" ou "?" seguido de
# minuscula), "e.g. `x`", "e.g. (x" e
# 'e.g. "x"' fecham a frase antes do x (o escopo fica mais curto:
# escreva "e.g. the `x`" ou tire o "e.g."); a aspa dupla logo depois
# do terminador abre a frase seguinte mesmo quando fecha uma citacao
# ('("did the call succeed?") before', de core/err.hpp, fecha a frase
# no "?"; o escopo fica mais curto, e a Punkt do NLTK, ao contrario,
# le essa aspa como fechamento); aspas tipograficas e
# maiuscula fora de A a Z nao abrem nem fecham frase (medido em
# 30/10/2026: nenhum cabecalho publico tem uma depois de terminador);
# os controles provam que cada caractere das tres classes faz o seu
# papel, nao que um caractere de fora fica de fora (so a minuscula
# depois do ponto tem controle, PONTO-DE-EXTENSAO); os fechamentos e
# a aspa dupla so tem controle depois do ponto: depois de "!" e de
# "?" eles valem porque os tres terminadores dividem uma classe so do
# regex, e um mutante que separe os terminadores passa em todos os
# controles (revisao N1 da CITATION-COMMON-WORD-fix2, I-1 e I-2;
# limite declarado pela camada N3 da L-12 do projeto; a tabela
# inteira do fim de frase e' o item CITATION-SENTENCE-TABLE do
# TODO.md); um paragrafo sem
# ponto final emenda no seguinte, porque o extrator une as linhas sem
# guardar a linha em branco; e a juncao
# sem separador perde a frase-gatilho que o reflow partiu ao meio
# ("Proved" no fim de uma linha, "by:" no comeco da outra) - nesse
# caso so a juncao com espaco ve a frase-gatilho.
# PLAN-SCOPE-COLUMNS (F, fora desta fatia) reusa este modulo.
#
# Usage:
#   citation_grammar.py --selftest

import functools
import re
import string
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


# --- forma "teste citado" (CLAIM-CITATIONS E2-E4, CITATION-COMMON-WORD) --

# Regra 1: "proved"/"proven", ate quatro palavras (letras e hifen)
# separadas por espaco ou virgula, "by", e o ":" opcional de "Proved
# by:". O {0,4}? preguicoso pega o "by" mais proximo. Com os nomes
# que um teste pode ter aqui, preguicoso e guloso dao o mesmo
# resultado: so um nome feito so de letras caberia entre dois "by"
# (o "_", o digito e a crase interrompem a palavra), o nome so de
# minusculas precisa de crase, e a L-21 do projeto da' nome snake_case
# a todo teste. Um nome de letras com maiuscula mudaria isso, e o
# guloso passaria a pular o nome que esta entre dois "by" (revisao N1
# da CITATION-COMMON-WORD, cosmetico C-5).
_PROVED_BY_RE = re.compile(r"\bprov(?:ed|en)(?:[ ,]+[A-Za-z][A-Za-z-]*){0,4}?[ ,]+by\b:?", re.IGNORECASE)
_SEE_RE = re.compile(r"\bsee\b", re.IGNORECASE)

# Regra 2: o fim de frase, ver o comentario do topo.
_SENTENCE_END_RE = re.compile(r"[.!?][)\]']*(?=\s*[A-Z`(\"]|\s*$)")

# Regra 3: nome feito so de letras minusculas.
_LOWERCASE_WORD_RE = re.compile(r"[a-z]+")


@functools.lru_cache(maxsize=4)
def _names_pattern(frozen_names):
    """Uma expressao so com todos os nomes, com a mesma fronteira de
    palavra (\\b) dos dois lados que a busca nome a nome usava. Com a
    fronteira, a ordem das alternativas nao muda o resultado (um nome
    que e' prefixo de outro so casa onde o outro nao cabe); a ordem
    fixa so faz o padrao compilado ser o mesmo em toda execucao."""
    alternatives = "|".join(re.escape(name) for name in sorted(frozen_names))
    return re.compile(r"\b(?:" + alternatives + r")\b")


def _sentence_scopes(text):
    """Para cada frase-gatilho de `text`, o par (inicio, fim): do fim
    da frase-gatilho ao fim da frase dela (regra 2), ou ao fim do texto
    quando a frase nao fecha."""
    scopes = []
    for trigger in list(_PROVED_BY_RE.finditer(text)) + list(_SEE_RE.finditer(text)):
        end = _SENTENCE_END_RE.search(text, trigger.end())
        scopes.append((trigger.end(), end.start() if end else len(text)))
    return sorted(scopes)


def _is_enveloped(text, match):
    """True quando o nome tem uma crase colada antes E outra colada
    depois (regra 3); uma crase de um lado so nao e' envelope."""
    return text[match.start() - 1 : match.start()] == "`" and text[match.end() : match.end() + 1] == "`"


def _cited_name_in(text, pattern):
    """O primeiro nome do inventario (pela posicao) que aparece dentro
    do escopo de alguma frase-gatilho de `text` e passa a regra 3, ou
    None."""
    for start, end in _sentence_scopes(text):
        for match in pattern.finditer(text, start, end):
            name = match.group(0)
            if _LOWERCASE_WORD_RE.fullmatch(name) and not _is_enveloped(text, match):
                continue
            return name
    return None


def has_cited_test_reference(spaced_text, tight_text, known_test_names):
    """True quando o bloco (as duas juncoes do MESMO texto - ver o
    comentario do topo deste modulo para o porque das duas) tem uma
    frase-gatilho SEGUIDA, na mesma frase, pelo nome de um teste que
    EXISTE em `known_test_names`, na forma de identificador (as tres
    regras do comentario do topo). Devolve o nome citado (o primeiro
    pela posicao, na juncao com espaco antes da juncao sem separador),
    ou None quando nenhuma frase-gatilho e' seguida por um nome valido
    na frase dela (a mesma "citacao de teste que nao existe reprova"
    da tabela E3 - devolver None aqui e' o que faz o chamador tratar
    como NAO citado, nunca aceitar uma citacao fantasma)."""
    if not known_test_names:
        return None
    pattern = _names_pattern(frozenset(known_test_names))
    return _cited_name_in(spaced_text, pattern) or _cited_name_in(tight_text, pattern)


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


# VERMELHO da regra 2 (escopo de frase): a frase-gatilho e o nome real
# estao no MESMO bloco, mas em frases diferentes - o nome nao e' a
# citacao daquela frase-gatilho. Mutante que mata: escopo ate o fim do
# texto (sem _SENTENCE_END_RE).
def _selftest_name_in_another_sentence_rejected():
    known = {"window_parity_test"}
    spaced = "The refusal is identical on every platform (see the table above). Unrelated: window_parity_test opens a window."
    got = has_cited_test_reference(spaced, spaced, known)
    if got is not None:
        print(f"selftest: OUTRA-FRASE FALHOU (nome de outra frase foi aceito como citacao): {got!r}", file=sys.stderr)
        return False
    print("selftest: OUTRA-FRASE OK (nome fora da frase da frase-gatilho nunca e citacao)")
    return True


# VERDE da regra 2: o ponto de uma extensao de arquivo e o de uma
# abreviatura nao fecham a frase (minuscula depois deles), entao o
# nome depois de "tests/math2d_test.cpp's" ainda e' da frase do
# gatilho. Mutante que mata: qualquer ponto fecha a frase.
def _selftest_extension_dot_does_not_end_the_sentence():
    known = {"lerp_endpoint_case"}
    spaced = "Measured live: see tests/math2d_test.cpp, e.g. the case lerp_endpoint_case, which reproduces it."
    got = has_cited_test_reference(spaced, spaced, known)
    if got != "lerp_endpoint_case":
        print(f"selftest: PONTO-DE-EXTENSAO FALHOU (o ponto de '.cpp'/'e.g.' fechou a frase): {got!r}", file=sys.stderr)
        return False
    print("selftest: PONTO-DE-EXTENSAO OK (ponto seguido de minuscula nao fecha a frase)")
    return True


# VERMELHO da regra 2: uma frase nova que comeca por crase (o nome de
# um identificador, comum nos cabecalhos: "`noexcept`: proven ...")
# fecha a anterior. Mutante que mata: tirar a crase do comeco de frase
# em _SENTENCE_END_RE.
def _selftest_sentence_starting_with_backtick_ends_the_previous():
    known = {"window_parity_test"}
    spaced = "The refusal is identical on every platform (see the table above). `window_parity_test` opens a window."
    got = has_cited_test_reference(spaced, spaced, known)
    if got is not None:
        print(f"selftest: FRASE-COM-CRASE FALHOU (o nome da frase seguinte, aberta por crase, foi aceito): {got!r}", file=sys.stderr)
        return False
    print("selftest: FRASE-COM-CRASE OK (frase aberta por crase depois de um ponto e frase nova)")
    return True


# VERMELHO da regra 3 (forma de identificador): na MESMA frase do
# gatilho, um nome do inventario feito so de minusculas, sem crase, e'
# palavra comum. Mutante que mata: tirar a conferencia de
# _LOWERCASE_WORD_RE.
def _selftest_bare_lowercase_word_rejected():
    known = {"shapes"}
    spaced = "Measured on both platforms. Proved by: the shapes it draws, compared pixel by pixel."
    got = has_cited_test_reference(spaced, spaced, known)
    if got is not None:
        print(f"selftest: PALAVRA-COMUM FALHOU (palavra comum sem crase foi aceita como citacao): {got!r}", file=sys.stderr)
        return False
    print("selftest: PALAVRA-COMUM OK (nome so de minusculas, sem crase, nunca e citacao)")
    return True


# VERDE da regra 3: o MESMO nome, entre crases, e' citacao. Mutante que
# mata: exigir "_" no nome em vez de aceitar o envelope.
def _selftest_enveloped_lowercase_name_accepted():
    known = {"shapes"}
    spaced = "Measured on both platforms. Proved by: `shapes`, compared pixel by pixel."
    got = has_cited_test_reference(spaced, spaced, known)
    if got != "shapes":
        print(f"selftest: PALAVRA-ENTRE-CRASES FALHOU (nome entre crases deveria ser citacao): {got!r}", file=sys.stderr)
        return False
    print("selftest: PALAVRA-ENTRE-CRASES OK (nome so de minusculas entre crases e citacao)")
    return True


# VERDE da regra 1: "proved" + ate quatro palavras + "by", as formas
# que a arvore real escreve. Mutante que mata: voltar a "proved by"
# colado.
def _selftest_widened_trigger_accepted():
    known = {"loop_bind_test"}
    for spaced in (
        "Measured: proved live by `loop_bind_test`'s own case.",
        "Measured: proven, not promised, by loop_bind_test.",
        "Measured: Proved at compile time by loop_bind_test (both directions).",
    ):
        got = has_cited_test_reference(spaced, spaced, known)
        if got != "loop_bind_test":
            print(f"selftest: GATILHO-LARGO FALHOU (deveria aceitar): {spaced!r} -> {got!r}", file=sys.stderr)
            return False
    print("selftest: GATILHO-LARGO OK ('proved'/'proven' + ate 4 palavras + 'by' e frase-gatilho)")
    return True


# VERMELHO da regra 1: cinco palavras entre "proved" e "by" ja nao sao
# a locucao. Mutante que mata: tirar o limite {0,4}.
def _selftest_five_words_between_rejected():
    known = {"loop_bind_test"}
    spaced = "Measured: proved over many long years and by loop_bind_test too."
    got = has_cited_test_reference(spaced, spaced, known)
    if got is not None:
        print(f"selftest: GATILHO-LONGE FALHOU (cinco palavras entre 'proved' e 'by' foi aceito): {got!r}", file=sys.stderr)
        return False
    print("selftest: GATILHO-LONGE OK (cinco palavras ou mais entre 'proved' e 'by' nao e frase-gatilho)")
    return True


# VERMELHO de regressao (S6, D-W8-1086): o bloco de include/glintfx/
# gfss/value.hpp com a palavra "shapes" de volta, reduzido as duas
# frases que importam - o "see" de um paragrafo e a prosa do outro.
# Com o codigo de antes desta regra, o nome "shapes" do inventario
# virava citacao; agora as regras 2 e 3 recusam, cada uma sozinha.
def _selftest_s6_common_word_in_prose_rejected():
    known = {"shapes", "draw2d_shapes_parity_test"}
    lines = [
        "a six-member time-unit enum (see gltfx_gfss_time_unit's own comment",
        "below for the semantics the leader defined for it).",
        "The cost of three parallel {magnitude, unit} shapes instead of one",
        "shared one is three enums, on all five platforms laid out identically.",
    ]
    got = has_cited_test_reference(" ".join(lines), "".join(lines), known)
    if got is not None:
        print(f"selftest: S6-PALAVRA-COMUM FALHOU (a prosa de value.hpp virou citacao): {got!r}", file=sys.stderr)
        return False
    print("selftest: S6-PALAVRA-COMUM OK (a palavra 'shapes' da prosa nao e citacao)")
    return True


# VERMELHO da regra 3, o envelope de DOIS lados (CITATION-COMMON-WORD-
# fix1, revisao N1, I-1): uma crase so de um lado nao e' envelope, e a
# palavra comum continua palavra comum. docs/api-conventions.md promete
# a crase colada antes E depois. Mutantes que matam: _is_enveloped sem
# a conferencia da crase da direita (MRV1 da revisao) ou da esquerda.
def _selftest_one_sided_backtick_rejected():
    known = {"shapes"}
    for spaced in (
        "Measured on both platforms. Proved by: `shapes it draws, compared pixel by pixel.",
        "Measured on both platforms. Proved by: the shapes` it draws, compared pixel by pixel.",
    ):
        got = has_cited_test_reference(spaced, spaced, known)
        if got is not None:
            print(f"selftest: CRASE-DE-UM-LADO FALHOU (crase de um lado so foi aceita como envelope): {spaced!r} -> {got!r}", file=sys.stderr)
            return False
    print("selftest: CRASE-DE-UM-LADO OK (nome so de minusculas com crase de um lado so nunca e citacao)")
    return True


# VERMELHO da tabela E3 com um nome que CONTEM um nome real (revisao N1,
# I-2): o nome conta inteiro. "wire_relay_inventado_test" nao existe, e
# o nome real "wire_relay" no comeco dele nao vira citacao; o espelho e'
# o nome real "relay_test" no fim de "wire_relay_test". Mutantes que
# matam: _names_pattern sem a fronteira \b da direita (MRV2 da revisao)
# ou da esquerda.
def _selftest_name_inside_longer_name_rejected():
    for known, spaced in (
        ({"wire_relay"}, "Measured live: proved by wire_relay_inventado_test on both platforms."),
        ({"relay_test"}, "Measured live: proved by wire_relay_test on both platforms."),
    ):
        got = has_cited_test_reference(spaced, spaced, known)
        if got is not None:
            print(f"selftest: NOME-DENTRO-DE-OUTRO FALHOU (parte de um nome mais longo foi aceita como citacao): {spaced!r} -> {got!r}", file=sys.stderr)
            return False
    print("selftest: NOME-DENTRO-DE-OUTRO OK (so o nome inteiro conta; comeco ou fim de outro nome nunca e citacao)")
    return True


# VERMELHO da regra 2, o fechamento DEPOIS do ponto (revisao N1, C-4; a
# regra das frases do Unicode, UAX #29, SB9 a SB11: o terminador, os
# fechamentos, e so entao o espaco e a frase seguinte): em "(see the
# table above.)" o ")" ainda e' da frase que o ponto fechou. Mutante que
# mata: _SENTENCE_END_RE sem o grupo de fechamentos (MRV3 da revisao).
def _selftest_closer_after_period_ends_the_sentence():
    known = {"window_parity_test"}
    spaced = "The refusal is identical on every platform (see the table above.) Unrelated: window_parity_test opens a window."
    got = has_cited_test_reference(spaced, spaced, known)
    if got is not None:
        print(f"selftest: FECHAMENTO-DEPOIS-DO-PONTO FALHOU (o nome da frase seguinte foi aceito): {got!r}", file=sys.stderr)
        return False
    print("selftest: FECHAMENTO-DEPOIS-DO-PONTO OK (ponto seguido de fechamento e de frase nova fecha a frase)")
    return True


# VERMELHO da regra 2, os TRES terminadores (CITATION-COMMON-WORD-fix2,
# revisao N1 da fix1, I-A): docs/api-conventions.md promete ".", "!" e
# "?". Mutantes que matam: tirar "!" ou "?" da classe (MREV-PONTO da
# revisao tira os dois).
def _selftest_every_terminator_ends_the_sentence():
    known = {"window_parity_test"}
    for terminator in ".!?":
        spaced = f"The refusal is identical on every platform (see the table above){terminator} Unrelated: window_parity_test opens a window."
        got = has_cited_test_reference(spaced, spaced, known)
        if got is not None:
            print(f"selftest: TERMINADORES FALHOU (o terminador {terminator!r} nao fechou a frase): {got!r}", file=sys.stderr)
            return False
    print("selftest: TERMINADORES OK ('.', '!' e '?' fecham a frase)")
    return True


# VERMELHO da regra 2, os fechamentos "]" e "'" e mais de um seguido
# (revisao N1 da fix1, I-B; o ")" sozinho e' o de FECHAMENTO-DEPOIS-DO-
# PONTO), e o VERDE do lado oposto: um fechamento seguido de minuscula
# continua a frase. Mutantes que matam: tirar "]" ou "'" da classe
# (MREV-FECHA da revisao tira os dois), ou aceitar um fechamento so.
def _selftest_every_closer_after_the_terminator_ends_the_sentence():
    known = {"window_parity_test"}
    for closers in ("]", "'", "])"):
        spaced = f"The refusal is identical on every platform [see the table above.{closers} Unrelated: window_parity_test opens a window."
        got = has_cited_test_reference(spaced, spaced, known)
        if got is not None:
            print(f"selftest: FECHAMENTOS FALHOU (o fechamento {closers!r} nao fechou a frase): {got!r}", file=sys.stderr)
            return False
    spaced = "Measured live: see the 'frame.' window_parity_test reproduces it."
    if has_cited_test_reference(spaced, spaced, known) != "window_parity_test":
        print(f"selftest: FECHAMENTOS FALHOU (fechamento seguido de minuscula fechou a frase): {spaced!r}", file=sys.stderr)
        return False
    print("selftest: FECHAMENTOS OK (']', \"'\" e varios seguidos ficam na frase que o ponto fechou)")
    return True


# VERMELHO da regra 2, as aberturas (revisao N1 da fix1, I-C e I-D):
# toda maiuscula de A a Z, "(" e aspas duplas abrem a frase seguinte
# (a crase e' a de FRASE-COM-CRASE); e aspas duplas logo depois do
# ponto abrem a frase mesmo com minuscula depois (o lado oposto do
# VERDE de FECHAMENTOS). Mutantes que matam: tirar "(" (MREV-PARABERT)
# ou as aspas (MREV-ASPAS) da classe, ou estreitar a faixa A-Z.
def _selftest_every_opener_starts_a_new_sentence():
    known = {"window_parity_test"}
    starts = [letter + ":" for letter in string.ascii_uppercase] + ["(Unrelated:", '"Unrelated:']
    texts = [f"The refusal is identical on every platform (see the table above). {start} window_parity_test opens a window." for start in starts]
    texts.append('Measured live: see the "frame." window_parity_test reproduces it.')
    for spaced in texts:
        got = has_cited_test_reference(spaced, spaced, known)
        if got is not None:
            print(f"selftest: ABERTURAS FALHOU (o nome da frase seguinte foi aceito): {spaced!r} -> {got!r}", file=sys.stderr)
            return False
    print("selftest: ABERTURAS OK (maiuscula de A a Z, '(' e aspas duplas abrem a frase seguinte)")
    return True


# VERMELHO da regra 2 na juncao sem separador: o fim de frase vale sem
# espaco nenhum entre o terminador e a frase seguinte (o reflow cola as
# linhas). Mutante que mata: exigir espaco antes da abertura.
def _selftest_sentence_end_without_space_in_tight_join():
    known = {"window_parity_test"}
    lines = [
        "The refusal is identical on every platform (see the table above).",
        "Unrelated: window_parity_test opens a window.",
    ]
    got = has_cited_test_reference(" ".join(lines), "".join(lines), known)
    if got is not None:
        print(f"selftest: JUNCAO-SEM-ESPACO FALHOU (o fim de frase sem espaco nao fechou a frase): {got!r}", file=sys.stderr)
        return False
    print("selftest: JUNCAO-SEM-ESPACO OK (terminador colado na frase seguinte fecha a frase)")
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
        _selftest_name_in_another_sentence_rejected(),
        _selftest_extension_dot_does_not_end_the_sentence(),
        _selftest_sentence_starting_with_backtick_ends_the_previous(),
        _selftest_bare_lowercase_word_rejected(),
        _selftest_enveloped_lowercase_name_accepted(),
        _selftest_widened_trigger_accepted(),
        _selftest_five_words_between_rejected(),
        _selftest_s6_common_word_in_prose_rejected(),
        _selftest_one_sided_backtick_rejected(),
        _selftest_name_inside_longer_name_rejected(),
        _selftest_closer_after_period_ends_the_sentence(),
        _selftest_every_terminator_ends_the_sentence(),
        _selftest_every_closer_after_the_terminator_ends_the_sentence(),
        _selftest_every_opener_starts_a_new_sentence(),
        _selftest_sentence_end_without_space_in_tight_join(),
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
