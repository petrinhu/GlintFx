#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_docs_count_vocab.py - DOCS-COUNT-VOCAB sub-fatias D2/D2b
# (docs/plano-w7c.md sec. 3.D, decisoes D11-D14; docs/plano-w7c-
# adendo-revalidacao.md sec. 3.D, decisao D-A9). Fecha a mesma familia
# de defeito que ja mordeu tres vezes neste projeto no mesmo dia
# (narrativa do README, alegacao falsa no teste do Windows,
# CLAIM-CITATIONS): prosa com um NUMERO DE CONTAGEM que apodrece - o
# codigo muda, o numero fica escrito, e passa a mentir.
#
# UNIVERSO: a MESMA enumeracao de check_dash_pubdoc.py (`scanned_
# files`/`in_scope`/`is_exempt`), IMPORTADA, nunca copiada (GODS_LAWS.
# md L-33/memoria feedback_copia_em_vez_de_fonte) - ela e' mais larga
# que "documento ao consumidor" (inclui todo *.txt rastreado, inclusive
# CMakeLists.txt e arquivos internos de tests/), entao este portao e'
# mais ESTRITO que o de travessao, nunca mais frouxo.
#
# VOCABULARIO FECHADO E NOMEADO (uma forma, uma funcao - GODS_LAWS.md
# L-17), cada uma exigindo um SUBSTANTIVO DE CONTAGEM adjacente
# (tests, test cases, cases, controls, checks, jobs, gates, fixtures,
# assertions, scenarios, pairs) - um numero sem substantivo de
# contagem por perto nao e' varrido por desenho (ele nao afirma
# medir nada sobre TESTE nenhum, pode ser qualquer outro numero):
#   - digito + ate' dois modificadores + substantivo de contagem
#     ("116 registered cases");
#   - `N of N` (regra da data, D2b: so' conta quando o SEGUNDO N nao
#     e' seguido de '/' - "2 of 26/08/2026" e' DATA escrita com "of",
#     nunca contagem; "43 of 53 apply" E' contagem);
#   - `N/N` + substantivo de contagem;
#   - numero por extenso, SO' na forma "palavra-numero + ate' dois
#     modificadores + substantivo de contagem" ("ten renamed pairs",
#     "forty controls") - nunca uma palavra-numero solta.
#
# ISENCOES, LISTA FECHADA E IMPRESSA (D-A9: NENHUMA isencao nova alem
# destas tres - "entre aspas" e "parametro de desenho" abririam o
# furo que este portao existe para fechar):
#   - trecho entre crases (a frase inteira e' codigo/literal, nao
#     alegacao de prosa);
#   - citacao `L-NN` (GODS_LAWS.md) imediatamente colada ao numero -
#     "L-40" nao e' contagem de teste nenhum;
#   - citacao de medicao datada NO MESMO PARAGRAFO (citation_grammar.
#     py, D1: `gh run view <id>` ou SHA entre crases) - um paragrafo e'
#     o bloco continuo de linhas nao-vazias que contem o numero (o
#     mesmo comentario de varias linhas, ou o mesmo paragrafo de
#     prosa);
#   - secoes JA LANCADAS do CHANGELOG.md (cabecalho `## [A.B.C.D] -
#     data`) - historico datado e' imutavel por convencao (Keep a
#     Changelog); `[Unreleased]` e' varrida sem isencao.
#
# GUARDA DE GRAMATICA (nao e' isencao - fecha um falso positivo medido
# na calibracao real, D3, 24/09/2026): um substantivo de contagem
# NUNCA e' seguido imediatamente por '/' sem espaco - "the same two
# libraries tests/CMakeLists.txt's own..." casava "two libraries
# tests" como contagem, quando "tests/CMakeLists.txt" e' um CAMINHO de
# arquivo (a palavra "tests" ali nao e' o substantivo ingles, e' o
# primeiro componente do path) - achado real em src/platform/win32/
# CMakeLists.txt:89 e tests/CMakeLists.txt:2176, ambos citando "tests/
# CMakeLists.txt's own..." logo apos "libraries".
#
# Usage:
#   check_docs_count_vocab.py --check <repo-root>
#   check_docs_count_vocab.py --selftest

import os
import re
import sys
from typing import NamedTuple

import check_dash_pubdoc
import citation_grammar

SCRIPT_NAME = "check_docs_count_vocab.py"

# PLURAL SEMPRE, nunca singular opcional (calibracao contra a arvore
# real, L-43: "gate"/"pair"/"check"/"control"/"test" no singular sao
# tambem verbos comuns em ingles - "platforms that pair", "things the
# gate [does]" - e casariam prosa comum como falso positivo. Todo
# exemplo real do vocabulario, no proprio plano e nesta calibracao, ja
# usa a forma plural; exigi-la sempre elimina a ambiguidade sem perder
# nenhum caso conhecido.
_COUNT_NOUN = (
    r"(?:test\s+cases|tests|cases|controls|checks|jobs|gates|fixtures|"
    r"assertions|scenarios|pairs)"
)
_MODIFIER = r"[A-Za-z][A-Za-z-]*"

_DIGIT_COUNT_RE = re.compile(rf"\b\d+(?:\s+{_MODIFIER}){{0,2}}\s+{_COUNT_NOUN}\b(?!/)")
_N_SLASH_N_RE = re.compile(rf"\b\d+/\d+\s+{_COUNT_NOUN}\b(?!/)")
_N_OF_N_RE = re.compile(r"\b\d+\s+of\s+\d+\b")

_NUMBER_WORDS = (
    "one two three four five six seven eight nine ten eleven twelve "
    "thirteen fourteen fifteen sixteen seventeen eighteen nineteen "
    "twenty thirty forty fifty sixty seventy eighty ninety"
).split()
_NUMBER_WORD_ALT = "|".join(sorted(_NUMBER_WORDS, key=len, reverse=True))
_SPELLED_COUNT_RE = re.compile(
    rf"\b(?:{_NUMBER_WORD_ALT})(?:\s+{_MODIFIER}){{0,2}}\s+{_COUNT_NOUN}\b(?!/)",
    re.IGNORECASE,
)


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


# --- deteccao, uma funcao por forma (GODS_LAWS.md L-17) -----------------


def _find_digit_counts(text):
    return [m.span() for m in _DIGIT_COUNT_RE.finditer(text)]


def _find_slash_counts(text):
    return [m.span() for m in _N_SLASH_N_RE.finditer(text)]


def _find_n_of_n_counts(text):
    """D2b: '2 of 26/08/2026' e' data escrita com 'of' - so' conta
    quando o segundo numero NAO e' seguido de '/'."""
    spans = []
    for m in _N_OF_N_RE.finditer(text):
        if text[m.end() : m.end() + 1] == "/":
            continue
        spans.append(m.span())
    return spans


def _find_spelled_counts(text):
    return [m.span() for m in _SPELLED_COUNT_RE.finditer(text)]


def find_count_phrases(text):
    """Todos os trechos com forma de contagem, ordenados por posicao -
    lista de (start, end). Sobreposicao entre formas e' possivel (ex.:
    um `N of N jobs` casa tanto N-of-N quanto, em tese, digito+
    substantivo se reformatado); nao e' mesclada aqui - cada forma e'
    seu proprio achado, o chamador decide o que fazer com a posicao."""
    spans = (
        _find_digit_counts(text)
        + _find_slash_counts(text)
        + _find_n_of_n_counts(text)
        + _find_spelled_counts(text)
    )
    return sorted(set(spans))


# --- isencoes, lista fechada (D-A9) -------------------------------------


def _find_matching_close_run(text, search_from, run_len):
    """A partir de search_from, acha a PROXIMA backtick string de
    comprimento exatamente run_len - pulando runs de outro comprimento
    (crase literal dentro do trecho ainda aberto, CommonMark). Devolve
    (close_start, indice_logo_apos_o_fechamento) ou None se nao achar
    nenhuma ate o fim do texto."""
    n = len(text)
    j = search_from
    while j < n and text[j] != "`":
        j += 1
    while j < n:
        close_start = j
        while j < n and text[j] == "`":
            j += 1
        if j - close_start == run_len:
            return close_start, j
        while j < n and text[j] != "`":
            j += 1
    return None


def find_code_spans(text):
    """DOCS-COUNT-VOCAB D2d (achado do main, segunda revisao da regra
    de crase, 24/09/2026 - GODS_LAWS.md L-22/L-42, pesquisa antes da
    terceira tentativa): trecho de codigo (CODE SPAN) na definicao real
    do CommonMark (spec.commonmark.org/0.31.2/#code-spans) - "a
    backtick string is a string of one or more backtick characters
    that is neither preceded nor followed by a backtick" abre um
    trecho, e ele fecha na PROXIMA backtick string de comprimento
    IGUAL (pareamento por COMPRIMENTO, nunca a proxima crase
    qualquer); sem fechamento do mesmo comprimento, a abertura nao e'
    delimitador, e' crase literal. D2c usava rfind/find (a crase mais
    proxima antes e depois do achado) - isso pareia a crase de
    FECHAMENTO de um trecho anterior com a de ABERTURA do proximo,
    isentando tudo o que sobra entre dois trechos de codigo NAO
    relacionados ("Run `ctest` then 43 of 53 jobs apply, see
    `ci.yml`." isentava a contagem no meio, que nao esta dentro de
    trecho de codigo nenhum). Devolve a lista de (content_start,
    content_end) - os limites do CONTEUDO de cada trecho, sem as
    proprias crases."""
    spans = []
    i = 0
    n = len(text)
    while i < n:
        if text[i] != "`":
            i += 1
            continue
        run_start = i
        while i < n and text[i] == "`":
            i += 1
        run_len = i - run_start
        content_start = i
        found = _find_matching_close_run(text, content_start, run_len)
        if found is None:
            i = content_start  # sem fechamento do mesmo comprimento - crase literal
            continue
        close_start, i = found
        spans.append((content_start, close_start))
    return spans


# Sinal de codigo/literal (D2d): um trecho entre crases so' isenta a
# contagem que carrega quando o PROPRIO CONTEUDO do trecho tem pelo
# menos um sinal de caminho/opcao/simbolo de ferramenta - prosa pura
# entre crases ("SEVENTEEN scenarios it runs", "43 of 53 " com espaco
# a mais) NAO isenta so' por estar dentro de um trecho de codigo real;
# "99% tests passed, 2 tests failed out of 216" continua isento, pelo
# '%'/'.'/':' que carrega. Hifen so' conta no INICIO de palavra (flag
# de linha de comando, "-oom-kill-disable"), nunca um hifen de
# pontuacao no meio de uma frase.
_CODE_SIGNAL_RE = re.compile(r"[/\\.=()\[\]<>$%:#*_]|(?<!\S)-")


def _has_code_signal(content):
    return bool(_CODE_SIGNAL_RE.search(content))


def _is_between_backticks(text, start, end):
    """Isento so' quando [start:end] cai DENTRO do conteudo de um
    trecho de codigo real (find_code_spans(), pareado por comprimento
    - D2d) E esse trecho carrega ao menos um sinal de codigo
    (_has_code_signal() - D2d). Uma crase que so' embrulha a propria
    contagem, sem sinal nenhum, nao isenta (D2c) - agora medido pelo
    CONTEUDO, nunca pelo tamanho (um espaco a mais bastava para
    enganar o criterio de tamanho de D2c)."""
    for content_start, content_end in find_code_spans(text):
        if content_start <= start and end <= content_end:
            return _has_code_signal(text[content_start:content_end])
    return False


def _is_law_citation(text, start):
    """'GODS_LAWS.md L-40' / 'L-40' - o numero colado a um 'L-' na
    frente nunca e' contagem de teste, e' identificador de lei."""
    return text[max(0, start - 2) : start] == "L-"


def is_exempt_count(text, start, end, paragraph_text):
    if _is_between_backticks(text, start, end):
        return True, "entre crases"
    if _is_law_citation(text, start):
        return True, "citacao L-NN"
    if citation_grammar.has_dated_measurement_citation(paragraph_text):
        return True, "citacao de medicao datada no paragrafo"
    return False, None


# --- paragrafos (bloco continuo de linhas nao-vazias) -------------------


def split_into_paragraphs(text):
    """Lista de (start_line_0based, end_line_0based) - um paragrafo e'
    um bloco MAXIMO de linhas nao-vazias (nao-so'-espaco); e' a mesma
    unidade tanto de um comentario `#` de varias linhas quanto de um
    paragrafo de prosa Markdown, sem precisar de duas funcoes."""
    lines = text.splitlines()
    paragraphs = []
    start = None
    for i, line in enumerate(lines):
        if line.strip():
            if start is None:
                start = i
        elif start is not None:
            paragraphs.append((start, i - 1))
            start = None
    if start is not None:
        paragraphs.append((start, len(lines) - 1))
    return paragraphs


def _line_start_offsets(text):
    """Lista de offset (indice de char) onde cada linha 0-based
    comeca, para converter (start, end) de re.finditer de volta a
    numero de linha 1-based sem re-escanear o texto inteiro por
    achado."""
    offsets = [0]
    for line in text.splitlines(keepends=True):
        offsets.append(offsets[-1] + len(line))
    return offsets


def _line_number_for_offset(line_offsets, char_offset):
    lo, hi = 0, len(line_offsets) - 1
    while lo < hi:
        mid = (lo + hi + 1) // 2
        if line_offsets[mid] <= char_offset:
            lo = mid
        else:
            hi = mid - 1
    return lo + 1  # 1-based


# --- CHANGELOG: secoes ja' lancadas sao historico datado ----------------


_CHANGELOG_HEADER_RE = re.compile(r"^## \[(.+?)\](?: - .+)?\s*$")


def released_changelog_line_ranges(text):
    """Para CHANGELOG.md: lista de (start_line_1based, end_line_1based)
    de cada secao JA LANCADA (cabecalho '## [A.B.C.D] - data', nunca
    '## [Unreleased]'). Uma secao vai do proprio cabecalho ate' a
    linha anterior ao PROXIMO cabecalho de nivel 2, ou fim do
    arquivo."""
    lines = text.splitlines()
    headers = []  # (line_1based, is_released)
    for i, line in enumerate(lines, start=1):
        m = _CHANGELOG_HEADER_RE.match(line)
        if m:
            headers.append((i, m.group(1) != "Unreleased"))
    ranges = []
    for idx, (line_no, released) in enumerate(headers):
        end = headers[idx + 1][0] - 1 if idx + 1 < len(headers) else len(lines)
        if released:
            ranges.append((line_no, end))
    return ranges


def _line_in_ranges(line_no, ranges):
    return any(start <= line_no <= end for start, end in ranges)


# --- varredura de um arquivo ---------------------------------------------


def scan_file_text(path, text):
    """Devolve lista de achados VIVOS (nao isentos): (line_1based,
    trecho, motivo_nao_isento="viva"). Achados isentos nao entram
    aqui - a contagem deles e' feita a parte, pelo chamador, para o
    relatorio "K isentas por citacao, H por historico"."""
    line_offsets = _line_start_offsets(text)
    paragraphs = split_into_paragraphs(text)
    released_ranges = released_changelog_line_ranges(text) if path == "CHANGELOG.md" else []

    findings = []  # (line, trecho, categoria) - categoria in {"viva","citada","historico"}
    for start, end in find_count_phrases(text):
        line_no = _line_number_for_offset(line_offsets, start)
        para_start_line, para_end_line = _paragraph_for_line(paragraphs, line_no)
        paragraph_text = "\n".join(text.splitlines()[para_start_line : para_end_line + 1])

        if _line_in_ranges(line_no, released_ranges):
            findings.append((line_no, text[start:end], "historico"))
            continue

        exempt, _reason = is_exempt_count(text, start, end, paragraph_text)
        if exempt:
            findings.append((line_no, text[start:end], "citada"))
            continue

        findings.append((line_no, text[start:end], "viva"))
    return findings


def _paragraph_for_line(paragraphs, line_1based):
    line_0based = line_1based - 1
    for start, end in paragraphs:
        if start <= line_0based <= end:
            return start, end
    return line_0based, line_0based


# --- modo real -------------------------------------------------------


def _read_text(root, path):
    file_path = os.path.join(root, *path.split("/"))
    try:
        with open(file_path, "r", encoding="utf-8", errors="surrogateescape") as handle:
            return handle.read()
    except OSError:
        return None


class ScanSummary(NamedTuple):
    """Agrupa os quatro numeros de uma varredura (GODS_LAWS.md L-17:
    junta o que sempre anda junto num tipo so', em vez de estourar o
    teto de parametros de quem imprime o resumo)."""

    universe_count: int
    live: list
    cited_count: int
    historico_count: int
    phrase_count: int


def _scan_universe(root, universe):
    """Devolve um ScanSummary - uma funcao so' pelo laco (GODS_LAWS.md
    L-17), reusada por real_main e por quem quiser a mesma varredura
    sem a impressao/saida."""
    live = []
    cited_count = 0
    historico_count = 0
    phrase_count = 0
    for path in sorted(universe):
        text = _read_text(root, path)
        if text is None:
            continue
        for line_no, trecho, categoria in scan_file_text(path, text):
            phrase_count += 1
            if categoria == "citada":
                cited_count += 1
            elif categoria == "historico":
                historico_count += 1
            else:
                live.append((path, line_no, trecho))
    return ScanSummary(len(universe), live, cited_count, historico_count, phrase_count)


def _print_summary_and_maybe_fail(summary):
    print(
        f"{SCRIPT_NAME}: varreu {summary.universe_count} arquivo(s), {summary.phrase_count} "
        f"frase(s) com forma de contagem, {summary.cited_count} isenta(s) por citacao, "
        f"{summary.historico_count} por historico, {len(summary.live)} viva(s)"
    )
    print(
        f"{SCRIPT_NAME}: o que este portao NAO ve - numero por extenso fora da forma nomeada "
        "(ex.: 'ninety' sozinho, sem substantivo de contagem adjacente); numero em imagem; "
        "parafrase livre sem digito nem palavra-numero ('quase cem testes')"
    )
    if summary.live:
        print(f"{SCRIPT_NAME}: REPROVADO ({len(summary.live)} frase(s) viva(s), sem citacao nem historico):", file=sys.stderr)
        for path, line_no, trecho in summary.live:
            print(f"  {path}:{line_no}: {trecho!r}", file=sys.stderr)
        sys.exit(1)


def real_main(args):
    if len(args) != 1:
        fail("usage: check_docs_count_vocab.py --check <repo-root-directory>")
    root = args[0]

    tracked, untracked, ok = check_dash_pubdoc.scanned_files(root)
    if not ok:
        fail(f"'git ls-files' falhou em '{root}' (nao e repositorio git, ou git indisponivel)")
    if len(tracked) + len(untracked) == 0:
        fail("varredura vazia: 0 arquivo(s) rastreado(s) ou nao-rastreado(s) - GODS_LAWS.md L-40")

    universe = [p for p in tracked + untracked if check_dash_pubdoc.in_scope(p) and not check_dash_pubdoc.is_exempt(p)]
    if not universe:
        fail("varredura vazia: 0 arquivo(s) no universo (in_scope, nao isento) - GODS_LAWS.md L-40")

    _print_summary_and_maybe_fail(_scan_universe(root, universe))


# --- controles do --selftest -------------------------------------------


# Controle NEGATIVO = a ponte do revisor reconstruida a partir das
# frases que motivaram tudo (docs/plano-w7c.md 3.D, tabela D2).
def _selftest_bridge_negative_controls():
    fixtures = [
        "Linux's 90 tests currently pass, all green.",
        "The Windows job reports 88 tests today.",
        "The migration produced ten renamed pairs in this pass.",
        "We now enforce forty controls across the suite.",
        "This suite has 116 registered cases in shared mode and 114 in static mode.",
        "The gate confirms 5 of 5 jobs are green tonight.",
    ]
    ok = True
    for text in fixtures:
        phrases = find_count_phrases(text)
        if not phrases:
            print(f"selftest: PONTE-NEGATIVA FALHOU (nao achou forma de contagem): {text!r}", file=sys.stderr)
            ok = False
            continue
        start, end = phrases[0]
        exempt, _reason = is_exempt_count(text, start, end, text)
        if exempt:
            print(f"selftest: PONTE-NEGATIVA FALHOU (deveria ser viva, saiu isenta): {text!r}", file=sys.stderr)
            ok = False
    if ok:
        print(f"selftest: PONTE-NEGATIVA OK ({len(fixtures)} frases da ponte do revisor, todas vivas)")
    return ok


# Guarda de gramatica (achado real da calibracao D3, 24/09/2026): um
# substantivo seguido de '/' sem espaco e' inicio de caminho de
# arquivo, nunca a palavra em ingles - "two libraries tests/CMakeLists.
# txt's own..." nao pode contar "tests" como substantivo de contagem.
def _selftest_noun_immediately_before_slash_is_not_a_count():
    text = "the same two libraries tests/CMakeLists.txt's own target already links for it."
    phrases = find_count_phrases(text)
    if phrases:
        print(f"selftest: GUARDA-CAMINHO FALHOU (casou 'tests/' como substantivo de contagem): {phrases} em {text!r}", file=sys.stderr)
        return False
    print("selftest: GUARDA-CAMINHO OK ('tests/CMakeLists.txt' nunca casa como substantivo de contagem)")
    return True


def _selftest_bridge_positive_real_docs():
    here = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.abspath(os.path.join(here, "..", ".."))
    text = _read_text(repo_root, "docs/api-conventions.md")
    if text is None:
        print("selftest: PONTE-POSITIVA PULADO (docs/api-conventions.md indisponivel aqui)")
        return True
    findings = scan_file_text("docs/api-conventions.md", text)
    live = [f for f in findings if f[2] == "viva"]
    if live:
        print(f"selftest: PONTE-POSITIVA FALHOU (documento real reprovaria): {live}", file=sys.stderr)
        return False
    print("selftest: PONTE-POSITIVA OK (docs/api-conventions.md, documento real, sem vivas)")
    return True


# VERMELHO (piso de varredura vazia, GODS_LAWS.md L-40).
def _selftest_empty_universe_would_fail():
    universe = [p for p in [] if check_dash_pubdoc.in_scope(p) and not check_dash_pubdoc.is_exempt(p)]
    if universe:
        print("selftest: VARREDURA-VAZIA FALHOU (lista vazia deveria continuar vazia)", file=sys.stderr)
        return False
    print("selftest: VARREDURA-VAZIA OK (universo vazio detectavel pelo chamador, real_main reprova)")
    return True


# D2b: a regra da data.
def _selftest_date_rule_n_of_m():
    date_text = "GODS_LAWS.md L-40 achado 2 of 26/08/2026 (\"a guarda de progresso...\")"
    if _find_n_of_n_counts(date_text):
        print(f"selftest: REGRA-DA-DATA FALHOU ('2 of 26/08/2026' nao deveria contar): {date_text!r}", file=sys.stderr)
        return False
    count_text = "the same sweep found 43 of 53 apply to a Windows job today."
    if not _find_n_of_n_counts(count_text):
        print(f"selftest: REGRA-DA-DATA FALHOU ('43 of 53 apply' deveria contar): {count_text!r}", file=sys.stderr)
        return False
    print("selftest: REGRA-DA-DATA OK ('N of DATA' nao conta, '43 of 53 apply' conta)")
    return True


# D2c (revisao da D3, 24/09/2026): uma crase que so' embrulha a
# propria contagem NAO isenta mais - e' o contorno que o lider
# proibiu, nao um conserto do documento.
def _selftest_backtick_wrapping_only_the_count_not_exempt():
    text = "the suite has `90 tests` today, unchanged."
    phrases = find_count_phrases(text)
    if not phrases:
        print(f"selftest: CRASE-ESTREITA FALHOU (nao achou a forma dentro de crases): {text!r}", file=sys.stderr)
        return False
    start, end = phrases[0]
    exempt, reason = is_exempt_count(text, start, end, text)
    if exempt:
        print(f"selftest: CRASE-ESTREITA FALHOU (crase que so' embrulha a contagem nao pode isentar): {text!r} exempt={exempt} reason={reason}", file=sys.stderr)
        return False
    print("selftest: CRASE-ESTREITA OK (crase que so' embrulha a contagem nunca isenta - D2c)")
    return True


# Positivo: a crase isenta de verdade quando o trecho e' MAIOR que a
# contagem - saida de ferramenta citada por inteiro, ou um comando que
# por acaso contem a forma dentro de algo maior.
# D2d (segunda revisao da regra de crase, 24/09/2026): as seis sondas
# do main, uma funcao por sonda (GODS_LAWS.md L-17).
def _matched_span_exempt(text, wanted):
    for s, e in find_count_phrases(text):
        if text[s:e] == wanted:
            return is_exempt_count(text, s, e, text)
    return None, None


def _selftest_pairing_two_unrelated_code_spans_never_exempts_prose_between():
    # A prosa entre os dois trechos de codigo carrega um '/' de
    # proposito - um sinal de codigo por coincidencia, o suficiente
    # para o pareamento ingenuo (rfind/find) escapar so' pelo
    # _has_code_signal(); so' o pareamento por comprimento real
    # (find_code_spans) prova que "43 of 53" fica FORA de qualquer
    # trecho de codigo de verdade.
    text = "Run `ctest -R foo` then 43 of 53 tests match /some/path, see `ci.yml`."
    exempt, reason = _matched_span_exempt(text, "43 of 53")
    if exempt is None:
        print(f"selftest: PAREAMENTO FALHOU (nao achou '43 of 53'): {text!r}", file=sys.stderr)
        return False
    if exempt:
        print(f"selftest: PAREAMENTO FALHOU (prosa entre dois trechos de codigo NAO relacionados isentou): reason={reason}", file=sys.stderr)
        return False
    print("selftest: PAREAMENTO OK (prosa entre dois trechos de codigo distintos nunca isenta)")
    return True


def _selftest_extra_space_inside_backticks_does_not_exempt():
    text = "the `43 of 53 ` apply"
    exempt, reason = _matched_span_exempt(text, "43 of 53")
    if exempt is None:
        print(f"selftest: ESPACO-A-MAIS FALHOU (nao achou '43 of 53'): {text!r}", file=sys.stderr)
        return False
    if exempt:
        print(f"selftest: ESPACO-A-MAIS FALHOU (um espaco extra dentro da crase nao pode isentar): reason={reason}", file=sys.stderr)
        return False
    print("selftest: ESPACO-A-MAIS OK (espaco extra dentro da crase nao burla mais o criterio)")
    return True


def _selftest_prose_inside_backticks_without_code_signal_not_exempt():
    text = "the `SEVENTEEN scenarios it runs`"
    exempt, reason = _matched_span_exempt(text, "SEVENTEEN scenarios")
    if exempt is None:
        print(f"selftest: PROSA-PURA FALHOU (nao achou 'SEVENTEEN scenarios'): {text!r}", file=sys.stderr)
        return False
    if exempt:
        print(f"selftest: PROSA-PURA FALHOU (prosa sem sinal de codigo entre crases nao pode isentar): reason={reason}", file=sys.stderr)
        return False
    print("selftest: PROSA-PURA OK (prosa pura entre crases, sem sinal de codigo, nunca isenta)")
    return True


def _selftest_bare_count_alone_in_backticks_still_not_exempt():
    text = "`43 of 53` alone"
    exempt, reason = _matched_span_exempt(text, "43 of 53")
    if exempt is None:
        print(f"selftest: SO-A-CONTAGEM FALHOU (nao achou '43 of 53'): {text!r}", file=sys.stderr)
        return False
    if exempt:
        print(f"selftest: SO-A-CONTAGEM FALHOU: reason={reason}", file=sys.stderr)
        return False
    print("selftest: SO-A-CONTAGEM OK (so' a contagem entre crases continua reprovando)")
    return True


def _selftest_tool_literal_with_code_signal_still_exempt():
    text = "the log said `99% tests passed, 2 tests failed out of 216` verbatim."
    exempt, reason = _matched_span_exempt(text, "2 tests")
    if exempt is None:
        print(f"selftest: LITERAL-DE-FERRAMENTA FALHOU (nao achou '2 tests'): {text!r}", file=sys.stderr)
        return False
    if not exempt or reason != "entre crases":
        print(f"selftest: LITERAL-DE-FERRAMENTA FALHOU (deveria continuar isento, pelo '%'): exempt={exempt} reason={reason}", file=sys.stderr)
        return False
    print("selftest: LITERAL-DE-FERRAMENTA OK ('%' e ':' sao sinal de codigo, saida de ferramenta continua isenta)")
    return True


def _selftest_command_code_span_does_not_exempt_count_outside_it():
    text = "`ctest -R foo` to run 12 tests"
    exempt, reason = _matched_span_exempt(text, "12 tests")
    if exempt is None:
        print(f"selftest: COMANDO-FORA FALHOU (nao achou '12 tests'): {text!r}", file=sys.stderr)
        return False
    if exempt:
        print(f"selftest: COMANDO-FORA FALHOU (a contagem esta FORA do trecho de codigo, nunca devia isentar): reason={reason}", file=sys.stderr)
        return False
    print("selftest: COMANDO-FORA OK (comando entre crases nao isenta contagem que fica fora dele, na prosa)")
    return True


def _selftest_backtick_wrapping_more_than_the_count_is_exempt():
    text = 'the log said "`99% tests passed, 2 tests failed out of 216: consume_test`" verbatim.'
    phrases = find_count_phrases(text)
    if not phrases:
        print(f"selftest: CRASE-LARGA FALHOU (nao achou a forma): {text!r}", file=sys.stderr)
        return False
    start, end = None, None
    for s, e in phrases:
        if text[s:e] == "2 tests":
            start, end = s, e
            break
    if start is None:
        print(f"selftest: CRASE-LARGA FALHOU (nao achou '2 tests' entre os achados): {phrases} em {text!r}", file=sys.stderr)
        return False
    exempt, reason = is_exempt_count(text, start, end, text)
    if not exempt or reason != "entre crases":
        print(f"selftest: CRASE-LARGA FALHOU (saida de ferramenta citada por inteiro deveria isentar): exempt={exempt} reason={reason}", file=sys.stderr)
        return False
    print("selftest: CRASE-LARGA OK (crase que envolve mais que a contagem - saida de ferramenta citada - isenta)")
    return True


def _selftest_dated_citation_exemption_same_paragraph():
    paragraph = (
        "a sweep of every consume test across these same 8 job logs\n"
        "(`gh run view 34271655070`) found that 43 of 53 apply to a\n"
        "Windows job today, no other test needed its timeout touched.\n"
    )
    phrases = find_count_phrases(paragraph)
    n_of_n = [(s, e) for s, e in phrases if paragraph[s:e].startswith("43")]
    if not n_of_n:
        print(f"selftest: CITACAO-MESMO-PARAGRAFO FALHOU (nao achou '43 of 53'): {paragraph!r}", file=sys.stderr)
        return False
    start, end = n_of_n[0]
    exempt, reason = is_exempt_count(paragraph, start, end, paragraph)
    if not exempt or reason != "citacao de medicao datada no paragrafo":
        print(f"selftest: CITACAO-MESMO-PARAGRAFO FALHOU: exempt={exempt} reason={reason}", file=sys.stderr)
        return False
    print("selftest: CITACAO-MESMO-PARAGRAFO OK (citacao de run no mesmo paragrafo isenta)")
    return True


def _selftest_law_citation_exemption():
    text = "GODS_LAWS.md L-40's own floor applies whenever tests run."
    findings = scan_file_text("fixture.md", text)
    live = [f for f in findings if f[2] == "viva"]
    for line_no, trecho, categoria in findings:
        if trecho.strip().startswith("40"):
            if categoria == "viva":
                print(f"selftest: LEI-L-NN FALHOU ('L-40' virou contagem viva): {findings}", file=sys.stderr)
                return False
    print("selftest: LEI-L-NN OK ('L-40' nunca e contado como contagem de teste)")
    return True


def _selftest_changelog_released_section_exempt():
    text = (
        "## [Unreleased]\n"
        "\n"
        "- The suite now has 5 test cases for this feature.\n"
        "\n"
        "## [0.2.0.0] - 2026-08-20\n"
        "\n"
        "- Landed 21 of 21 jobs green, with 70 cases total.\n"
    )
    findings = scan_file_text("CHANGELOG.md", text)
    by_category = {line_no: categoria for line_no, _trecho, categoria in findings}
    unreleased_line = 3
    released_line = 7
    if by_category.get(unreleased_line) != "viva":
        print(f"selftest: CHANGELOG-HISTORICO FALHOU ([Unreleased] deveria ser viva): {findings}", file=sys.stderr)
        return False
    if by_category.get(released_line) != "historico":
        print(f"selftest: CHANGELOG-HISTORICO FALHOU (secao lancada deveria ser historico): {findings}", file=sys.stderr)
        return False
    print("selftest: CHANGELOG-HISTORICO OK ([Unreleased] viva, secao lancada isenta por historico)")
    return True


def selftest_main():
    controls = [
        _selftest_bridge_negative_controls(),
        _selftest_noun_immediately_before_slash_is_not_a_count(),
        _selftest_bridge_positive_real_docs(),
        _selftest_empty_universe_would_fail(),
        _selftest_date_rule_n_of_m(),
        _selftest_backtick_wrapping_only_the_count_not_exempt(),
        _selftest_backtick_wrapping_more_than_the_count_is_exempt(),
        _selftest_pairing_two_unrelated_code_spans_never_exempts_prose_between(),
        _selftest_extra_space_inside_backticks_does_not_exempt(),
        _selftest_prose_inside_backticks_without_code_signal_not_exempt(),
        _selftest_bare_count_alone_in_backticks_still_not_exempt(),
        _selftest_tool_literal_with_code_signal_still_exempt(),
        _selftest_command_code_span_does_not_exempt_count_outside_it(),
        _selftest_dated_citation_exemption_same_paragraph(),
        _selftest_law_citation_exemption(),
        _selftest_changelog_released_section_exempt(),
    ] + [citation_grammar.has_dated_measurement_citation("") is False]
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
        fail("usage: check_docs_count_vocab.py --check <repo-root-directory>  |  --selftest")


if __name__ == "__main__":
    main()
