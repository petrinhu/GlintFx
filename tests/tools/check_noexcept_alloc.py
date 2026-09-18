#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_noexcept_alloc.py - TODO.md item NOEXCEPT-ALLOC-B8 fatia F6
# (GODS_LAWS.md L-22 do projeto: "nenhuma excecao cruza a API publica";
# docs/api-conventions.md R3; ESCOPO.md Decisao 8/9/13: "Devolve erro; o
# aplicativo decide", nunca std::terminate() no processo do consumidor).
#
# O DEFEITO ESTE PORTAO EXISTE PARA IMPEDIR DE VOLTAR: alocar dentro de
# uma funcao `noexcept` sem `try` eficaz ao redor. Sob falta de memoria,
# `std::bad_alloc` escapa da funcao e `[except.terminate]` MATA O
# PROCESSO DE QUEM USA A BIBLIOTECA - nao um erro que o consumidor pode
# tratar. As fatias F1 a F5 (17-18/09/2026) consertaram os oito pontos
# reais que essa familia produzia hoje (B1 a B8, ver a auditoria em
# /var/tmp/builds/claude-1000/varredura-noexcept/RELATORIO.md); este
# arquivo e o portao que garante que um nono ponto nao entra amanha sem
# ninguem notar.
#
# DUAS FAMILIAS, NOMENCLATURA DOS ARTEFATOS (troca de letra aqui inverte
# o escopo inteiro - ver /var/tmp/glintfx-f6-plano/plano-f6-portao.md
# sec. 0): familia A = construtor padrao/de movimento de contentor da
# STL (`std::vector`/`std::string`...), que a linguagem OBRIGA a ser
# `noexcept` mas que a STL da Microsoft aloca por dentro quando o modo
# de depuracao de iterador esta ligado (`_ITERATOR_DEBUG_LEVEL != 0`,
# padrao em build Debug) - so mata no Windows em depuracao, `try` nao
# salva porque a fronteira e o proprio construtor chamado, nao a funcao
# que envolve; ~140 sitios medidos, CATRACA (nao conserto) nesta fatia,
# por ordem do lider (ESCOPO.md Decisao 13, 18/09/2026). Familia B =
# overload que ALOCA e NAO e `noexcept` (`push_back`, `reserve`,
# `std::string(view)`, `new`...) dentro de uma funcao `noexcept`, direta
# ou por chamada a funcao do projeto que aloca sem `try` eficaz - mata
# nos CINCO sistemas, Release incluido; BLOQUEANTE nesta fatia, sem
# tolerancia (a promessa do lider nao tem faixa aceitavel).
#
# POR QUE O `clang-tidy` (bugprone-exception-escape, JA LIGADO neste
# projeto) NAO PEGA ISTO (pesquisa obrigatoria ANTES do desenho, L-22
# global/L-43 do projeto - texto completo da pesquisa no plano desta
# fatia): a analise so prova lancamento ANDANDO POR CORPOS DE FUNCAO
# VISIVEIS. Na `libstdc++` (o compilador deste repositorio no Linux) o
# lancador de `std::bad_alloc` mora FORA dos cabecalhos
# (`__throw_bad_alloc()` sem corpo visivel) - "chamada sem corpo" e
# tratada como "desconhecida", nunca reportada, entao `push_back()`
# dentro de `noexcept` produz ZERO avisos (medido, `clang-tidy 22.1.8`
# contra este `.clang-tidy`). Na STL da Microsoft o mesmo lancador e
# INLINE (`xmemory:107 _Throw_bad_array_new_length()`), e e por isso que
# o job `Windows - Lint` real ja pegou um caso desta familia
# (`WIN-NOEXCEPT-ESCAPE`, TODO.md) - mas aquele job so varre
# `src/platform/win32/`, entao B7/B8 (codigo Wayland) nunca passariam
# por ele. Familia A e invisivel ao check POR DESENHO: `vector()` e
# `noexcept` por assinatura, entao "nada escapa" da NOSSA funcao aos
# olhos do analisador - o processo morre dentro do construtor chamado,
# nao na nossa fronteira. `-Wterminate`/cppcheck's own
# `throwInNoexceptFunction` tambem descartados pela mesma pesquisa: os
# dois so veem um `throw` ESCRITO no texto - esta familia inteira nao
# tem um `throw` escrito, tem um `push_back`.
#
# ESTE SCRIPT NAO E UM PARSER DE C++ - e analise de texto com casamento
# de chaves, herdada e promovida de
# /var/tmp/builds/claude-1000/varredura-noexcept/varre_noexcept.py (a
# regua ja calibrada contra `c08d0ed`/`603db6f`, 17/09/2026). O QUE ESTE
# METODO NAO VE, declarado aqui e reimpresso em TODA execucao real
# (nunca so em rodape, GODS_LAWS.md L-43 - "zero de varredura estreita
# falando sozinho" e o defeito nomeado que isto evita): concatenacao de
# string com `+`/`+=` fora do padrao reconhecido; `std::optional<contentor>
# ::emplace`; construcao de `std::variant` fora da forma
# `::ok(std::move(...))`; sobrecargas de mesmo NOME colapsadas na
# propagacao transitiva (o fecho e por nome, nao por assinatura); corpo
# de lambda anonima cuja alocacao mora fora do texto analisado por esta
# chamada.
#
# COPIA DE CONTENTOR POR ATRIBUICAO (`destino = origem;`) E INVISIVEL
# INTEIRA, NAS DUAS FAMILIAS - NAO SO NA FAMILIA A (achado do time-lead,
# 18/09/2026, medido contra `src/platform/gl/gl_context_facade.cpp:329`
# ANTES do conserto do commit `7a34bb2`): nem a copia de uma struct
# inteira que carrega vetor (`dest = other;`), nem a ATRIBUICAO direta
# de um `std::vector` comum para dentro de um MEMBRO ja existente -
# inclusive para dentro de um `std::optional<std::vector<...>>`
# (`holder->campo = valor;`) - produz qualquer token
# `std::vector`/`std::optional` no texto da propria atribuicao, entao
# `container_hits()` nunca casa ali. Plantado de volta e rodado contra a
# arvore real: `familia_B_achados=0`, veredito APROVADO, com o defeito
# no lugar. **ESTE CASO E FAMILIA B: MATA O PROCESSO DO CONSUMIDOR EM
# QUALQUER UM DOS CINCO SISTEMAS, EM RELEASE - NAO SO no modo de
# depuracao da Microsoft que o proximo paragrafo descreve.** Fixture
# que documenta esta lacuna especifica (nao consertada, so registrada):
# `tests/tools/fixtures/noexcept_alloc/gap_optional_vector_assignment.cpp`,
# ligada a `--selftest` como marcador de lacuna, nunca como controle de
# acerto.
#
# SO PARA A FAMILIA A AUTOMATICA (a que DEPENDE do modo de depuracao de
# iterador da Microsoft, `_ITERATOR_DEBUG_LEVEL != 0`): apenas
# construcao DIRETA de `std::vector`/`std::string` e contada - um tipo
# do PROJETO que carrega um `std::vector`/`std::string` por MEMBRO
# (COMPOSICAO, nao construcao direta) nao e rastreado sitio a sitio por
# este motor automatico; a baseline da familia A cobre so os sitios que
# o motor efetivamente enxerga.
#
# ESTA LACUNA FOI POSTA DIANTE DO LIDER E ACEITA POR ELE, NAO E
# OMISSAO HERDADA (decisao dele, 18/09/2026, por `AskUserQuestion`,
# medido e registrado em `ESCOPO.md`): o universo real de familia A e
# de ordem de ~140 sitios (RELATORIO.md secao 3 - "40 sitios diretos"
# mais "26 tipos do projeto que carregam vector/string, com 33
# declaracoes, 70 temporarios e 38 return{...}"); esta baseline
# automatica cobre 60/61 - o SUBCONJUNTO de construcao direta que o
# motor de texto enxerga, nunca uma remedicao mais precisa do mesmo
# conjunto. O lider recusou explicitamente a alternativa de parar
# esta fatia para ensinar o motor a rastrear composicao de tipos
# (exigiria um motor com noção de TIPO, nao so texto, com volume de
# falso positivo nao medido) - ele congelou os 60 e aceitou por
# escrito que os ~80 sitios restantes podem crescer SEM QUE PORTAO
# NENHUM PERCEBA; so leitura humana os pegaria. Ver ESCOPO.md, Decisao
# 13, para o texto completo da decisao. **A COPIA POR ATRIBUICAO,
# acima, e uma lacuna DIFERENTE e MAIS AMPLA que esta - cobre familia B
# tambem, mata em qualquer sistema, e ainda nao tem numero de sitios
# medido; corrigido o que o portao DECLARA nesta fatia, o motor em si
# continua sem alargar, por decisao do lider, fatia propria futura.**
#
# USO:
#   check_noexcept_alloc.py <raiz-do-repo> [--json <arquivo>]
#   check_noexcept_alloc.py --selftest
#
# Identificadores e comentarios em ingles, snake_case (GODS_LAWS.md
# L-21 do projeto); mensagens impressas em pt-br, como os demais
# checadores desta casa (check_test_parity.py, check_spdx.py). Apenas
# biblioteca padrao do Python 3 (GODS_LAWS.md L-07 do projeto -
# dependencia zero: nenhum `pip install`).

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

# GATE-ENV-SWEEP (mesmo remedio de check_test_parity.py/
# check_measured_parity.py, GODS_LAWS.md L-40): TODO.md's own status
# column and this script's own veredito line print non-Latin-1
# characters ("✅", "⏳") - a Windows console's default legacy code page
# has no slot for either, medido em outros portoes desta casa contra o
# mesmo runner.
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="backslashreplace")
if hasattr(sys.stderr, "reconfigure"):
    sys.stderr.reconfigure(encoding="utf-8", errors="backslashreplace")

SCRIPT_NAME = "check_noexcept_alloc.py"
SEM_PENDENCIA = "SEM-PENDENCIA"
FIXTURES_DIR_NAME = "fixtures/noexcept_alloc"


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


# ============================================================
# MOTOR DE ANALISE DE TEXTO (portado de varre_noexcept.py, calibrado
# contra c08d0ed/603db6f em 17/09/2026 - ver RELATORIO.md secao 1).
# Cada funcao aqui responde exatamente a UMA pergunta do motor
# (GODS_LAWS.md L-17).
# ============================================================

KEYWORDS = {
    "if", "for", "while", "switch", "catch", "return", "sizeof", "decltype",
    "alignof", "static_assert", "noexcept", "requires", "__attribute__",
    "else", "do", "try", "co_return", "co_await", "throw", "new", "delete",
    "case", "default", "typeid", "static_cast", "reinterpret_cast",
    "const_cast", "dynamic_cast", "constexpr", "explicit", "and", "or", "not",
    "defined", "__has_include", "assert",
}

# Contentores cujo construtor PADRAO e de MOVIMENTO sao noexcept e
# alocam proxy sob IDL != 0 (medido em /opt/msvc/.../include: vector:
# 672,761; xstring:758,1077 - RELATORIO.md secao 1).
PROXY_NOEXCEPT = {"vector", "string", "wstring", "basic_string", "u8string", "u16string", "u32string"}
# Contentores cujo construtor padrao ALOCA (sentinela/proxy) na STL da
# Microsoft mas NAO e noexcept (xtree:915, list:812, deque:643,
# unordered_map:98): capturavel por try comum, porem diverge da
# libstdc++ (que nao aloca ali).
ALLOC_NOT_NOEXCEPT_DEFAULT = {"map", "multimap", "set", "multiset", "list", "deque",
                              "unordered_map", "unordered_set", "unordered_multimap",
                              "unordered_multiset"}
CONTAINERS = PROXY_NOEXCEPT | ALLOC_NOT_NOEXCEPT_DEFAULT | {"function"}

ALLOC_METHODS = {
    "push_back", "emplace_back", "emplace", "insert", "resize", "reserve",
    "append", "assign", "substr", "replace", "push_front", "emplace_front",
    "try_emplace", "insert_or_assign", "emplace_hint", "shrink_to_fit",
}
ALLOC_FREE = {"std::to_string", "std::format", "std::vformat", "std::make_unique",
              "std::make_shared", "std::to_wstring"}

# Nomes que colidem com metodos comuns da STL (size(), value(), ...) e
# NUNCA propagam alocacao pela cadeia transitiva por nome - a regua e
# por NOME, e esses nomes aparecem em toda chamada de contentor.
STL_LIKE = {"size", "empty", "data", "begin", "end", "at", "count", "find", "insert", "erase",
            "clear", "swap", "get", "value", "reset", "release", "open", "close", "index",
            "front", "back", "length", "capacity", "max_size", "has_value", "error", "emplace"}


def strip_comments_and_literals(text):
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if text.startswith("//", i):
            j = text.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
        elif text.startswith("/*", i):
            j = text.find("*/", i)
            j = n if j < 0 else j + 2
            out.append(re.sub(r"[^\n]", " ", text[i:j]))
            i = j
        elif c == '"':
            j = i + 1
            while j < n and text[j] != '"':
                if text[j] == "\\":
                    j += 1
                if text[j] == "\n":
                    break
                j += 1
            j += 1
            out.append('"' + " " * max(0, j - i - 2) + '"')
            i = j
        elif c == "'":
            j = i + 1
            while j < n and text[j] != "'":
                if text[j] == "\\":
                    j += 1
                if text[j] == "\n":
                    break
                j += 1
            j += 1
            out.append("'" + " " * max(0, j - i - 2) + "'")
            i = j
        else:
            out.append(c)
            i += 1
    joined = "".join(out)
    return re.sub(r"(?m)^[ \t]*#[^\n]*", lambda m: " " * len(m.group(0)), joined)


def match_forward(s, i, open_c, close_c):
    depth = 0
    n = len(s)
    while i < n:
        if s[i] == open_c:
            depth += 1
        elif s[i] == close_c:
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return -1


def match_backward(s, i, open_c, close_c):
    depth = 0
    while i >= 0:
        if s[i] == close_c:
            depth += 1
        elif s[i] == open_c:
            depth -= 1
            if depth == 0:
                return i
        i -= 1
    return -1


def skip_ws(s, i):
    n = len(s)
    while i < n and s[i].isspace():
        i += 1
    return i


def skip_template_args(s, i):
    depth = 0
    n = len(s)
    while i < n:
        c = s[i]
        if c == "<":
            depth += 1
        elif c == ">":
            depth -= 1
            if depth == 0:
                return i + 1
        elif c in ";{}":
            return -1
        i += 1
    return -1


def line_of(s, i):
    return s.count("\n", 0, i) + 1


IDENT_BEFORE = re.compile(r"(operator\s*\(\s*\)|operator\s*[^\s\w(]{1,3}|[A-Za-z_][\w:]*)\s*$")


def find_functions(s, path):
    """Localiza definicoes de funcao (com corpo). Devolve lista de dicts."""
    funcs = []
    n = len(s)
    i = 0
    while i < n:
        j = s.find("{", i)
        if j < 0:
            break
        k = j - 1
        while k >= 0 and s[k].isspace():
            k -= 1
        is_try_block = False
        if s[max(0, k - 2):k + 1] == "try" and (k - 3 < 0 or not (s[k - 3].isalnum() or s[k - 3] == "_")):
            is_try_block = True
            k -= 3
            while k >= 0 and s[k].isspace():
                k -= 1
        head_end = k + 1
        pos = k
        found_params = -1
        scan = pos
        safety = 0
        seen_comma = False
        seen_init_colon = False
        while scan >= 0 and safety < 4000:
            safety += 1
            c = s[scan]
            if c.isspace():
                scan -= 1
                continue
            if c == ",":
                seen_comma = True
                scan -= 1
                continue
            if c == ":" and not (scan > 0 and s[scan - 1] == ":") and not (scan + 1 < n and s[scan + 1] == ":"):
                seen_init_colon = True
                scan -= 1
                continue
            if c in ")":
                op = match_backward(s, scan, "(", ")")
                if op < 0:
                    break
                if seen_comma and not seen_init_colon:
                    break
                pre = s[max(0, op - 64):op]
                m = IDENT_BEFORE.search(pre)
                name = m.group(1) if m else ""
                pre_stripped = pre.rstrip()
                prev_char = pre_stripped[-1] if pre_stripped else ""
                if name == "noexcept":
                    scan = op - 1
                    continue
                if prev_char == "]":
                    found_params = op
                    # CONSERTO SOBRE O MOTOR ORIGINAL (achado real,
                    # medido contra a arvore inteira do projeto): o
                    # nome literal "<lambda>" era IDENTICO para toda
                    # lambda anonima do repositorio inteiro - allocs_B/
                    # inherited sao indexados por NOME, entao QUALQUER
                    # lambda noexcept do projeto colidia com QUALQUER
                    # OUTRA lambda que aloca, em qualquer arquivo,
                    # medido: 4 lambdas sem relacao nenhuma (anb_parse.
                    # cpp, color_parse.cpp, egl_context_adapter.cpp,
                    # wgl_context_adapter.cpp) todas "acusadas" pelo
                    # mesmo achado de declaration_value_check.cpp:316.
                    # Chave unica por arquivo:linha - a MESMA garantia
                    # que ja vale para toda funcao nomeada (duas
                    # funcoes nomeadas iguais em arquivos diferentes ja
                    # tem essa mesma fraqueza de colisao por nome,
                    # documentada no cabecalho deste script; lambda e o
                    # unico caso onde o proprio motor original ja
                    # tinha ZERO unicidade por desenho, entao o
                    # conserto aqui e local a essa forma).
                    lname = f"<lambda:{path}:{line_of(s, op)}>"
                    break
                if name and name.split("::")[-1] not in KEYWORDS and name not in ("decltype",):
                    found_params = op
                    lname = name
                    break
                if name in ("decltype",):
                    scan = op - 1
                    continue
                break
            if c in "}":
                op = match_backward(s, scan, "{", "}")
                if op < 0:
                    break
                scan = op - 1
                continue
            if c == ">" and scan > 0 and s[scan - 1] == "-":
                # CONSERTO SOBRE O MOTOR ORIGINAL (achado real, medido
                # contra src/gfss/declaration_value_check.cpp: uma
                # lambda com trailing-return-type, "[&](args) -> T {",
                # tinha a seta "->" confundida com fechamento de
                # template ">" pelo ramo logo abaixo - match_backward()
                # entao procurava um "<" casado que nao existia ali,
                # atravessava o arquivo inteiro e prendia a busca a um
                # "(" de OUTRA chamada qualquer, produzindo uma "funcao
                # fantasma" com o nome errado (aqui, colidiu com
                # "size()" de duas funcoes noexcept reais e nao
                # relacionadas em outros arquivos, via allocs_B
                # indexado por nome). Trata a seta como DOIS
                # caracteres de tipo de retorno, nunca como fechamento
                # de generico - o "isalnum() or c in '_:&*[]-'" logo
                # abaixo ja cobre o resto do tipo de retorno (nome,
                # espaco, '*', '&', '::').
                scan -= 2
                continue
            if c == ">":
                op = match_backward(s, scan, "<", ">")
                if op < 0:
                    scan -= 1
                    continue
                scan = op - 1
                continue
            if c.isalnum() or c in "_:&*[]-":
                scan -= 1
                continue
            if c == ";" or c == "{":
                break
            scan -= 1
        if found_params < 0:
            i = j + 1
            continue
        close = match_forward(s, found_params, "(", ")")
        if close < 0 or close > j:
            i = j + 1
            continue
        head = s[close + 1:head_end]
        body_end = match_forward(s, j, "{", "}")
        if body_end < 0:
            i = j + 1
            continue
        noexcept_m = re.search(r"\bnoexcept\b(\s*\(([^()]*(\([^()]*\))*[^()]*)\))?", head)
        is_noexcept = False
        noexcept_kind = ""
        if noexcept_m:
            arg = noexcept_m.group(2)
            if arg is None:
                is_noexcept, noexcept_kind = True, "noexcept"
            elif arg.strip() == "false":
                is_noexcept, noexcept_kind = False, "noexcept(false)"
            elif arg.strip() == "true":
                is_noexcept, noexcept_kind = True, "noexcept(true)"
            else:
                is_noexcept, noexcept_kind = True, "noexcept(" + arg.strip()[:40] + ")"
        pre_start = max(s.rfind(";", 0, found_params), s.rfind("}", 0, found_params), s.rfind("{", 0, found_params))
        ret_text = s[pre_start + 1:found_params].strip()
        funcs.append({
            "file": path,
            "name": lname,
            "line": line_of(s, found_params),
            "head": head.strip(),
            "ret": ret_text[-160:],
            "noexcept": is_noexcept,
            "noexcept_kind": noexcept_kind,
            "start": j,
            "end": body_end,
            "init_start": close + 1,
            "try_block": is_try_block,
        })
        i = j + 1
    return funcs


def try_ranges(s, start, end):
    out = []
    for m in re.finditer(r"\btry\s*\{", s[start:end]):
        o = start + m.end() - 1
        c = match_forward(s, o, "{", "}")
        if c < 0:
            continue
        types = []
        k = skip_ws(s, c + 1)
        while s.startswith("catch", k):
            p = s.find("(", k)
            q = match_forward(s, p, "(", ")")
            types.append(s[p + 1:q].strip())
            b = s.find("{", q)
            bc = match_forward(s, b, "{", "}")
            k = skip_ws(s, bc + 1)
        out.append((o, c, types))
    return out


def effective_catch(types):
    for t in types:
        t2 = t.replace(" ", "")
        if t2 == "..." or "bad_alloc" in t2 or "std::exception" in t2:
            return True
    return False


def container_hits(s, start, end):
    hits = []
    for m in re.finditer(r"\bstd::(" + "|".join(sorted(CONTAINERS)) + r")\b", s[start:end]):
        typ = m.group(1)
        pos = start + m.start()
        k = start + m.end()
        k = skip_ws(s, k)
        if k < end and s[k] == "<":
            k2 = skip_template_args(s, k)
            if k2 < 0:
                continue
            k = skip_ws(s, k2)
        if k >= end:
            continue
        c = s[k]
        if c in "&*" or s.startswith("::", k):
            continue
        kind = None
        if c == "{":
            cl = match_forward(s, k, "{", "}")
            inner = s[k + 1:cl].strip()
            kind = "default-temp" if inner == "" else ("move-temp" if inner.startswith("std::move(") else "args-temp")
        elif c == "(":
            cl = match_forward(s, k, "(", ")")
            inner = s[k + 1:cl].strip()
            kind = "default-temp" if inner == "" else ("move-temp" if inner.startswith("std::move(") else "args-temp")
        elif c.isalpha() or c == "_":
            mm = re.match(r"[A-Za-z_]\w*", s[k:])
            name = mm.group(0)
            k3 = skip_ws(s, k + len(name))
            if k3 >= end:
                continue
            c3 = s[k3]
            if c3 == ";":
                kind = "default-decl"
            elif c3 == "{":
                cl = match_forward(s, k3, "{", "}")
                inner = s[k3 + 1:cl].strip()
                kind = "default-decl" if inner == "" else ("move-decl" if inner.startswith("std::move(") else "args-decl")
            elif c3 == "(":
                cl = match_forward(s, k3, "(", ")")
                inner = s[k3 + 1:cl].strip()
                kind = "default-decl" if inner == "" else ("move-decl" if inner.startswith("std::move(") else "args-decl")
            elif c3 == "=":
                rest = s[k3 + 1:s.find(";", k3)].strip()
                if rest in ("{}", "{ }"):
                    kind = "default-decl"
                elif rest.startswith("std::move("):
                    kind = "move-decl"
                else:
                    kind = "args-decl"
            elif c3 in ",)":
                continue
            else:
                continue
        else:
            continue
        hits.append({"pos": pos, "line": line_of(s, pos), "type": typ, "kind": kind,
                     "text": re.sub(r"\s+", " ", s[pos:min(end, pos + 90)]).strip()})
    return hits


def return_hits(s, f):
    out = []
    ret = f["ret"]
    rt = None
    for t in PROXY_NOEXCEPT:
        if re.search(r"\bstd::" + t + r"\b", ret):
            rt = t
    if rt is None:
        return out
    for m in re.finditer(r"\breturn\b\s*([^;]*);", s[f["start"]:f["end"]]):
        expr = m.group(1).strip()
        pos = f["start"] + m.start()
        if expr in ("{}", "{ }"):
            kind = "return-default"
        elif expr.startswith("std::move("):
            kind = "return-move"
        elif expr.startswith('"'):
            kind = "return-literal(args)"
        elif re.fullmatch(r"[A-Za-z_]\w*", expr):
            kind = "return-local(move-se-sem-NRVO)"
        elif expr.startswith("{"):
            kind = "return-braced(args)"
        elif expr.startswith("std::nullopt") or expr.startswith("std::unexpected"):
            continue
        elif "::err(" in expr:
            continue
        elif "::ok(std::move(" in expr or "::ok(" in expr:
            kind = "return-move"
        else:
            kind = "return-expr"
        out.append({"pos": pos, "line": line_of(s, pos), "type": rt, "kind": kind,
                    "text": "return " + expr[:80]})
    return out


def method_hits(s, start, end):
    out = []
    for m in re.finditer(r"(?:\.|->)\s*(" + "|".join(sorted(ALLOC_METHODS)) + r")\s*\(", s[start:end]):
        pos = start + m.start()
        rm = re.search(r"([A-Za-z_]\w*)\s*(?:\(\s*\))?\s*$", s[max(start, pos - 80):pos])
        out.append({"pos": pos, "line": line_of(s, pos), "type": "metodo", "kind": m.group(1),
                    "recv": rm.group(1) if rm else "",
                    "text": re.sub(r"\s+", " ", s[max(start, pos - 40):min(end, pos + 50)]).strip()})
    # NOTHROW-NEW (conserto sobre o motor original, GODS_LAWS.md L-43:
    # varre_noexcept.py/RELATORIO.md secao 4 registrava "new (std::
    # nothrow) T" como falso positivo DESCARTADO POR LEITURA (err.cpp:
    # 43, display_facade.cpp:67, window_facade.cpp:162, gl_context_
    # facade.cpp:257, gfx_open_only_fixation.cpp:79 ja usam essa forma
    # em producao, precisamente porque ela NAO lanca - allocation-form
    # nothrow), nunca filtrado pela regra automatica - um portao que
    # reprova codigo ja correto por essa lacuna e o mesmo defeito que
    # este arquivo existe para fechar (GODS_LAWS.md L-40). Casada
    # ANTES da regra geral de "new", pela MESMA posicao inicial (as
    # duas regex casam a partir do "new" literal), para a regra geral
    # nunca ver esse sitio.
    nothrow_new_positions = {m.start() for m in re.finditer(r"\bnew\s*\(\s*std::nothrow\s*\)", s[start:end])}
    for m in re.finditer(r"\b(std::to_string|std::format|std::vformat|std::make_unique|std::make_shared|std::to_wstring)\b|\bnew\s+[A-Za-z_(]", s[start:end]):
        if m.start() in nothrow_new_positions:
            continue
        pos = start + m.start()
        out.append({"pos": pos, "line": line_of(s, pos), "type": "livre", "kind": m.group(0).strip(),
                    "text": re.sub(r"\s+", " ", s[max(start, pos - 30):min(end, pos + 60)]).strip()})
    for m in re.finditer(r"\+=", s[start:end]):
        pos = start + m.start()
        seg = s[max(start, pos - 60):pos]
        if re.search(r"(str|text|name|out|buf|msg|path|line|result|json|src|source)\w*\s*$", seg, re.I):
            out.append({"pos": pos, "line": line_of(s, pos), "type": "string?", "kind": "+=",
                        "text": re.sub(r"\s+", " ", s[max(start, pos - 40):min(end, pos + 40)]).strip()})
    return out


CALL_RE = re.compile(r"(?<![\w.>])([A-Za-z_][\w:]*)\s*\(")


def called_names(s, start, end):
    names = set()
    for m in CALL_RE.finditer(s[start:end]):
        nm = m.group(1)
        base = nm.split("::")[-1]
        if base in KEYWORDS or nm.startswith("std::") or nm.startswith("GLINTFX_"):
            continue
        names.add(base)
    for m in re.finditer(r"(?:\.|->)\s*([A-Za-z_]\w*)\s*\(", s[start:end]):
        names.add(m.group(1))
    return names


def analyze_source(text, rel_path):
    """Roda o motor inteiro contra UM texto de fonte ja lido. Devolve
    (funcs, hits) - a mesma forma que analyze_tree() agrega por
    arquivo, exposta a parte para a calibracao poder rodar contra um
    unico arquivo de fixture sem tocar disco duas vezes."""
    s = strip_comments_and_literals(text)
    funcs = find_functions(s, rel_path)
    for f in funcs:
        f["tries"] = try_ranges(s, f["start"], f["end"])
        f["calls"] = called_names(s, f["init_start"], f["end"])
    hits = container_hits(s, 0, len(s)) + method_hits(s, 0, len(s))
    for f in funcs:
        hits += return_hits(s, f)
    for h in hits:
        h["file"] = rel_path
        inner = None
        for f in funcs:
            if f["init_start"] <= h["pos"] <= f["end"]:
                if inner is None or f["start"] > inner["start"]:
                    inner = f
        h["func"] = inner["name"] if inner else "<fora de funcao>"
        h["func_line"] = inner["line"] if inner else 0
        h["func_noexcept"] = inner["noexcept"] if inner else False
        prot = False
        if inner:
            for (o, c, types) in inner["tries"]:
                if o <= h["pos"] <= c and effective_catch(types):
                    prot = True
        h["protected"] = prot
        k = h["kind"]
        t = h["type"]
        if t in PROXY_NOEXCEPT and k in ("default-decl", "default-temp", "move-decl", "move-temp",
                                          "return-default", "return-move"):
            fam = "A"
        elif t in PROXY_NOEXCEPT and k == "return-local(move-se-sem-NRVO)":
            fam = "A?"
        elif t in ALLOC_NOT_NOEXCEPT_DEFAULT and k.startswith("default"):
            fam = "B-msvc-only"
        elif t == "function":
            fam = "B?" if k.startswith("args") else "-"
        elif k == "substr":
            fam = "B-ambiguo-substr"
        else:
            fam = "B"
        h["family"] = fam
        h["site"] = (h["file"], h["func"], f"{t}:{k}")
    return funcs, hits


def resolve_string_view_substr(all_files_text):
    """Segunda passada (a mesma que varre_noexcept.py faz): separa
    substr() de std::string_view (nao aloca) de substr() de
    std::string (aloca), por NOME declarado - texto, nao tipo (ver o
    cabecalho deste script, 'o que esta regua NAO ve')."""
    sv_names, str_names = set(), set()
    for t in all_files_text.values():
        for m in re.finditer(r"std::(?:w)?string_view\s+(?:const\s+)?&?\s*([A-Za-z_]\w*)", t):
            sv_names.add(m.group(1))
        for m in re.finditer(r"std::(?:w)?string\s+(?:const\s+)?&?\s*([A-Za-z_]\w*)", t):
            str_names.add(m.group(1))
    return sv_names, str_names


def analyze_tree(files):
    """files: lista de (rel_path, texto_bruto). Devolve dict com
    all_funcs, all_hits, allocs_B (funcoes com alocacao B direta e
    desprotegida) e inherited (propagacao transitiva por nome)."""
    all_files_text = {rel: strip_comments_and_literals(text) for rel, text in files}
    sv_names, str_names = resolve_string_view_substr(all_files_text)

    all_funcs, all_hits = [], []
    for rel, text in files:
        funcs, hits = analyze_source(text, rel)
        for h in hits:
            if h["family"] == "B-ambiguo-substr":
                if h.get("recv") in sv_names and h.get("recv") not in str_names:
                    h["family"] = "-"
                elif h.get("recv") in sv_names and h.get("recv") in str_names:
                    h["family"] = "B-ambiguo"
                else:
                    h["family"] = "B"
        all_funcs.extend(funcs)
        all_hits.extend(hits)

    by_name = {}
    for f in all_funcs:
        by_name.setdefault(f["name"], []).append(f)

    allocs_B = {}
    for h in all_hits:
        if h["family"] in ("B", "B-msvc-only") and not h["protected"] and h["func"] != "<fora de funcao>":
            allocs_B.setdefault(h["func"], set()).add(f"{h['file']}:{h['line']}:{h['kind']}")

    src_cache = all_files_text
    changed = True
    inherited = {}
    rounds = 0
    while changed and rounds < 12:
        changed = False
        rounds += 1
        for f in all_funcs:
            s = src_cache.get(f["file"], "")
            for c in f["calls"]:
                if c in STL_LIKE or c == f["name"]:
                    continue
                if c in allocs_B or c in inherited:
                    unprotected_call = False
                    for m in re.finditer(r"(?<![\w.>])" + re.escape(c) + r"\s*\(", s[f["init_start"]:f["end"]]):
                        pos = f["init_start"] + m.start()
                        covered = any(o <= pos <= cl and effective_catch(types) for (o, cl, types) in f["tries"])
                        if not covered:
                            unprotected_call = True
                            break
                    if not unprotected_call:
                        continue
                    key = f["name"]
                    tag = f"via {c}"
                    if tag not in inherited.get(key, set()):
                        inherited.setdefault(key, set()).add(tag)
                        changed = True

    return {"funcs": all_funcs, "hits": all_hits, "allocs_B": allocs_B, "inherited": inherited}


# ============================================================
# ENUMERACAO (git ls-files -z, escopo src/ include/)
# ============================================================

def enumerate_tracked_files(root):
    try:
        proc = subprocess.run(
            ["git", "ls-files", "-z", "--",
             "src/*.cpp", "src/*.hpp", "src/*.h",
             "include/*.hpp", "include/*.h"],
            cwd=str(root), capture_output=True, check=True,
        )
    except (OSError, subprocess.CalledProcessError) as exc:
        fail(
            f"'git ls-files' falhou em '{root}' - varredura recusada, nunca presumida vazia "
            f"(GODS_LAWS.md L-40): {exc}"
        )
    names = [p for p in proc.stdout.decode("utf-8", errors="replace").split("\0") if p]
    return sorted(names)


# ============================================================
# ARQUIVO DE EXCECOES DE FAMILIA B (tests/noexcept_alloc_exceptions.txt)
# ============================================================

VALID_EXCEPTION_TYPES_PREFIX = "DECISAO-LIDER:"
VALID_EXCEPTION_TYPE_TOOL = "FALSO-POSITIVO-DA-REGUA"


def parse_b_exceptions(text, source_label="tests/noexcept_alloc_exceptions.txt"):
    exceptions = {}
    errors = []
    for raw_line in text.splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        parts = [p.strip() for p in line.split("|")]
        if len(parts) != 5:
            errors.append(
                f"{source_label}: linha malformada (esperava 5 campos separados por '|', "
                f"achou {len(parts)}): {line!r}"
            )
            continue
        caminho, funcao, tipo, razao, item = parts
        if tipo != VALID_EXCEPTION_TYPE_TOOL and not tipo.startswith(VALID_EXCEPTION_TYPES_PREFIX):
            errors.append(
                f"{source_label}: tipo invalido {tipo!r} para {caminho}|{funcao} (esperado "
                f"{VALID_EXCEPTION_TYPE_TOOL!r} ou {VALID_EXCEPTION_TYPES_PREFIX!r}DD/MM/AAAA)"
            )
            continue
        exceptions[(caminho, funcao)] = {
            "tipo": tipo, "razao": razao, "item": item, "raw": line,
        }
    return exceptions, errors


# ============================================================
# BASELINE DE FAMILIA A (tests/noexcept_alloc_family_a_baseline.txt)
# ============================================================

def parse_family_a_baseline(text, source_label="tests/noexcept_alloc_family_a_baseline.txt"):
    sites = set()
    errors = []
    for raw_line in text.splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        parts = [p.strip() for p in line.split("|")]
        if len(parts) != 3:
            errors.append(
                f"{source_label}: linha malformada (esperava 3 campos 'caminho|funcao|forma', "
                f"achou {len(parts)}): {line!r}"
            )
            continue
        sites.add(tuple(parts))
    return sites, errors


# ============================================================
# TODO.md - so para a regra de morte de item (mesmo parse de
# check_test_parity.py/check_measured_parity.py)
# ============================================================

_TODO_ROW_RE = re.compile(r"^\|.*\|$")


def parse_todo_status(text):
    status_by_item = {}
    for line in text.splitlines():
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


def validate_item_deaths(items_by_key, todo_status, todo_text, label):
    """items_by_key: {chave: item_id}, chave so para a mensagem. Regra
    de morte (mesma de check_test_parity.py/check_measured_parity.py):
    item concluido em TODO.md, ou item que nao existe em lugar nenhum
    (nem tabela nem prosa entre crases), reprova - exceto SEM-PENDENCIA."""
    errors = []
    for key, item in items_by_key.items():
        if item == SEM_PENDENCIA:
            continue
        status = todo_status.get(item)
        if status is not None:
            if status["concluded"]:
                errors.append(
                    f"{label} {key}: aponta para o item {item!r}, marcado como CONCLUIDO "
                    f"({status['status_text']}) em TODO.md - regra 'concluido sem par': "
                    "apague esta linha"
                )
            continue
        if todo_text is not None and f"`{item}`" in todo_text:
            continue
        errors.append(
            f"{label} {key}: cita item {item!r}, que nao existe em TODO.md (nem na tabela "
            "nem como `id` entre crases em prosa) - ponteiro para o nada"
        )
    return errors


# ============================================================
# "O QUE ESTA REGUA NAO VE" - impresso SEMPRE, junto do zero
# (GODS_LAWS.md L-43, defeito 3 da secao 8 do plano da fatia)
# ============================================================

BLIND_SPOTS_TEXT = (
    "o que esta regua NAO ve (declarado, nunca implicito): concatenacao com + / +=,\n"
    "  std::optional<contentor>::emplace, construcao de std::variant fora da forma\n"
    "  ::ok(std::move(...)), sobrecargas colapsadas por nome, lambda anonima cujo\n"
    "  corpo aloca fora do texto\n"
    "COPIA DE CONTENTOR POR ATRIBUICAO (destino = origem;) e INVISIVEL INTEIRA, nas\n"
    "  DUAS familias, NAO SO na A: struct inteira com vetor por dest = other, ou\n"
    "  atribuicao direta pra dentro de um MEMBRO ja existente (inclusive pra dentro\n"
    "  de std::optional<std::vector<...>>) - medido contra gl_context_facade.cpp:329\n"
    "  (consertado em 7a34bb2): plantado de volta, o motor devolveu\n"
    "  familia_B_achados=0 - ESTE CASO MATA EM QUALQUER UM DOS CINCO SISTEMAS, em\n"
    "  Release, nao so no modo de depuracao da Microsoft; marcado por fixture em\n"
    "  fixtures/noexcept_alloc/gap_optional_vector_assignment.cpp (--selftest)\n"
    "familia_A_linha_de_base cobre SO construcao DIRETA de vector/string - tipo do\n"
    "  projeto que carrega vector/string por MEMBRO (composicao) fica de fora, cerca\n"
    "  de 80 sitios adicionais medidos a mao (RELATORIO.md), que podem crescer SEM\n"
    "  este portao perceber; decisao do lider, 18/09/2026, ESCOPO.md Decisao 13"
)


# ============================================================
# RELATORIO
# ============================================================

def run_gate(root, exceptions_text, baseline_text, todo_text, calibration_dir,
             enumerate_fn=enumerate_tracked_files, read_fn=None):
    """O portao inteiro, como uma funcao pura o bastante para
    --selftest poder chamar com fixtures em vez de tocar o disco real
    quando faz sentido. Devolve (report_lines, ok, findings) -
    report_lines e a saida de duas camadas; ok e o veredicto; findings
    e um dict com o detalhe nomeado dos achados, para os controles do
    --selftest inspecionarem sem re-parsear texto."""
    if read_fn is None:
        def read_fn(path):
            return path.read_text(encoding="utf-8", errors="replace")

    lines = []
    reasons = []

    # ---- DEGRAU 1: arquivos_encontrados == 0 reprova ----
    rel_paths = enumerate_fn(root)
    arquivos_encontrados = len(rel_paths)

    files = []
    arquivos_analisados = 0
    arquivos_pulados = 0
    for rel in rel_paths:
        try:
            text = read_fn(root / rel)
        except OSError as exc:
            arquivos_pulados += 1
            reasons.append(f"arquivo pulado (erro de leitura): {rel} ({exc})")
            arquivos_analisados += 1
            continue
        files.append((rel, text))
        arquivos_analisados += 1

    # ---- DEGRAU 2: analisados != encontrados reprova (armadilha do lote) ----
    if arquivos_analisados != arquivos_encontrados:
        reasons.append(
            f"cobertura perdida: encontrados={arquivos_encontrados}, "
            f"analisados={arquivos_analisados} - o lote parou no meio (GODS_LAWS.md L-36)"
        )

    tree = analyze_tree(files)
    funcoes = len(tree["funcs"])
    funcoes_noexcept = sum(1 for f in tree["funcs"] if f["noexcept"])

    # ---- DEGRAU 3: funcoes_noexcept == 0 reprova ----
    if funcoes_noexcept == 0:
        reasons.append(
            "funcoes_noexcept=0 - uma regua que deixou de casar chaves acha zero funcao e "
            "imprime zero achado, o que parece limpeza mas e varredura quebrada (GODS_LAWS.md L-40)"
        )

    # ---- DEGRAU 4: calibracao roda em TODA execucao ----
    # LE SEMPRE DO DISCO REAL, nunca via `read_fn` (que --selftest pode
    # injetar como um duble apontando para uma arvore sintetica
    # completamente diferente) - a calibracao prova a REGUA contra as
    # duas fixtures fixas do repositorio, nunca contra o que quer que
    # o chamador tenha simulado como "arvore".
    calib_dirty_path = calibration_dir / "calib_dirty_complex_match.cpp"
    calib_clean_path = calibration_dir / "calib_clean_complex_match.cpp"
    calib_ok = True
    calib_detail = "nao rodada"
    try:
        dirty_text = calib_dirty_path.read_text(encoding="utf-8")
        clean_text = calib_clean_path.read_text(encoding="utf-8")
        dirty_tree = analyze_tree([("calib_dirty_complex_match.cpp", dirty_text)])
        clean_tree = analyze_tree([("calib_clean_complex_match.cpp", clean_text)])
        dirty_reproved = _tree_has_any_finding(dirty_tree)
        clean_reproved = _tree_has_any_finding(clean_tree)
        if dirty_reproved and not clean_reproved:
            acc_line = next(iter(dirty_reproved))
            calib_detail = f"fixture_suja=ACUSADA(linha {acc_line}) fixture_limpa=ABSOLVIDA"
        else:
            calib_ok = False
            calib_detail = (
                f"FALHOU: fixture_suja acusada={bool(dirty_reproved)}, "
                f"fixture_limpa acusada={bool(clean_reproved)} (esperava suja=True, limpa=False)"
            )
            reasons.append(
                f"calibracao reprovou: {calib_detail} - a regua nao esta calibrada, nada "
                "abaixo dela pode ser confiado (GODS_LAWS.md L-43)"
            )
    except OSError as exc:
        calib_ok = False
        calib_detail = f"FALHOU: fixture de calibracao ilegivel ({exc})"
        reasons.append(f"calibracao nao rodou: {calib_detail}")

    # ---- FAMILIA B: achados bloqueantes ----
    b_exceptions, b_exc_errors = parse_b_exceptions(exceptions_text)
    reasons.extend(f"tests/noexcept_alloc_exceptions.txt: {e}" for e in b_exc_errors)
    todo_status = parse_todo_status(todo_text) if todo_text is not None else {}
    b_items_by_key = {f"{c}|{fn}": exc["item"] for (c, fn), exc in b_exceptions.items()}
    reasons.extend(
        validate_item_deaths(b_items_by_key, todo_status, todo_text, "tests/noexcept_alloc_exceptions.txt")
    )

    # SITIO-UNICO (conserto sobre defeito medido em 18/09/2026, achado F6:
    # a mesma funcao noexcept alcancada por MAIS DE UM caminho de
    # propagacao transitiva produzia DUAS entradas em tree["funcs"] para o
    # MESMO sitio real - mesmo arquivo, mesma linha, mesmo nome (a chamada
    # recursiva da propria funcao dentro do proprio corpo confunde
    # find_functions(), que reabre um "cabecalho" fantasma logo apos o
    # primeiro). Sem agrupar aqui, familia_B_achados publicava contagem de
    # LINHA EMITIDA, nunca de SITIO ENCONTRADO - o numero publicado nao
    # era o que aparentava (GODS_LAWS.md L-43: o entregavel e o RELATORIO,
    # nunca decoracao do motor). Agrupa por (arquivo, linha, nome) ANTES
    # de contar e ANTES de imprimir; funde direto/herdado por UNIAO -
    # NUNCA descarta o segundo caminho, so a CONTAGEM duplicada dele.
    grouped_sites = {}
    site_order = []
    for f in tree["funcs"]:
        if not f["noexcept"]:
            continue
        direct = tree["allocs_B"].get(f["name"])
        indirect = tree["inherited"].get(f["name"])
        if not direct and not indirect:
            continue
        site_key = (f["file"], f["line"], f["name"])
        if site_key not in grouped_sites:
            grouped_sites[site_key] = {"direct": set(), "indirect": set()}
            site_order.append(site_key)
        if direct:
            grouped_sites[site_key]["direct"].update(direct)
        if indirect:
            grouped_sites[site_key]["indirect"].update(indirect)

    b_findings = []
    for site_key in site_order:
        site_file, site_line, site_func = site_key
        merged = grouped_sites[site_key]
        exc = b_exceptions.get((site_file, site_func))
        finding = {
            "file": site_file, "func": site_func, "line": site_line,
            "direct": sorted(merged["direct"]),
            "indirect": sorted(merged["indirect"]),
            "covered": exc is not None,
        }
        b_findings.append(finding)
        if exc is None:
            reasons.append(
                f"familia B: {site_file}:{site_line} funcao {site_func}() e noexcept e aloca "
                f"sem try eficaz (direto={finding['direct']}, herdado={finding['indirect']}) - "
                "sem excecao em tests/noexcept_alloc_exceptions.txt (GODS_LAWS.md L-22)"
            )

    familia_B_achados = sum(1 for x in b_findings if not x["covered"])
    familia_B_excecoes = sum(1 for x in b_findings if x["covered"])

    # ---- FAMILIA A: catraca ----
    baseline_sites, baseline_errors = parse_family_a_baseline(baseline_text)
    reasons.extend(f"tests/noexcept_alloc_family_a_baseline.txt: {e}" for e in baseline_errors)

    current_sites = set()
    for h in tree["hits"]:
        if h["family"] in ("A", "A?"):
            current_sites.add(h["site"])

    familia_A_novos = sorted(current_sites - baseline_sites)
    familia_A_orfaos = sorted(baseline_sites - current_sites)
    for site in familia_A_novos:
        reasons.append(
            f"familia A: sitio novo fora da linha de base: {site[0]}|{site[1]}|{site[2]} - "
            "acrescente a tests/noexcept_alloc_family_a_baseline.txt (ESCOPO.md Decisao 13) "
            "ou conserte (degrau 1/2/3 da escada, docs/api-conventions.md R3)"
        )
    for site in familia_A_orfaos:
        reasons.append(
            f"familia A: linha da baseline sem sitio correspondente (orfa): "
            f"{site[0]}|{site[1]}|{site[2]} - o sitio sumiu; remova a linha de "
            "tests/noexcept_alloc_family_a_baseline.txt"
        )

    ok = not reasons

    # ---- RELATORIO, DUAS CAMADAS ----
    lines.append("[camada 1 - numeros crus]")
    lines.append("escopo: src/ include/ (arquivos rastreados por git ls-files)")
    lines.append(
        f"arquivos_encontrados={arquivos_encontrados}  arquivos_analisados={arquivos_analisados}  "
        f"arquivos_pulados={arquivos_pulados}"
    )
    lines.append(f"funcoes={funcoes}  funcoes_noexcept={funcoes_noexcept}")
    lines.append(f"calibracao: {calib_detail}")
    lines.append(f"familia_B_achados={familia_B_achados}  familia_B_excecoes_declaradas={familia_B_excecoes}")
    lines.append(
        f"familia_A_sitios={len(current_sites)}  familia_A_linha_de_base={len(baseline_sites)}  "
        f"familia_A_novos={len(familia_A_novos)}  familia_A_orfaos={len(familia_A_orfaos)}"
    )
    lines.append("[camada 2 - criterio aplicado]")
    lines.append(BLIND_SPOTS_TEXT)
    if ok:
        lines.append("veredito: APROVADO")
    else:
        lines.append(f"veredito: REPROVADO ({len(reasons)} motivo(s)):")
        for r in reasons:
            lines.append(f"  - {r}")

    findings = {
        "b_findings": b_findings,
        "familia_A_novos": familia_A_novos,
        "familia_A_orfaos": familia_A_orfaos,
        "calib_ok": calib_ok,
        "arquivos_encontrados": arquivos_encontrados,
        "arquivos_analisados": arquivos_analisados,
        "funcoes_noexcept": funcoes_noexcept,
        "reasons": reasons,
    }
    return lines, ok, findings


def _tree_has_any_finding(tree):
    """Usado pela calibracao E pelos controles de fixture do
    --selftest: devolve o conjunto de linhas de achado (familia A OU
    familia B - direta OU HERDADA por propagacao transitiva -
    desprotegida, sem olhar excecao nenhuma - testa a REGUA, nunca o
    arquivo de excecoes) num unico arquivo pequeno. Vazio == absolvido.
    A metade herdada (tree["inherited"]) e a mesma logica que
    run_gate() usa para os achados reais (B4/B5 da familia B so
    aparecem por essa via) - uma versao anterior deste controle so
    olhava tree["allocs_B"] (achado DIRETO) e deixava bad_transitive.
    cpp passar batido, exatamente a classe de bug que este portao
    existe para pegar."""
    lines = set()
    for h in tree["hits"]:
        if h["family"] in ("A", "A?"):
            lines.add(h["line"])
    for f in tree["funcs"]:
        if not f["noexcept"]:
            continue
        if f["name"] in tree["allocs_B"]:
            for site in tree["allocs_B"][f["name"]]:
                lines.add(int(site.split(":")[1]))
        if f["name"] in tree["inherited"]:
            lines.add(f["line"])
    return lines


# ============================================================
# MODO REAL
# ============================================================

def real_main(args):
    parser = argparse.ArgumentParser(prog=SCRIPT_NAME, add_help=False)
    parser.add_argument("root")
    parser.add_argument("--json", default=None)
    parsed = parser.parse_args(args)

    root = Path(parsed.root)
    exceptions_path = root / "tests" / "noexcept_alloc_exceptions.txt"
    baseline_path = root / "tests" / "noexcept_alloc_family_a_baseline.txt"
    todo_path = root / "TODO.md"
    calibration_dir = Path(__file__).resolve().parent / FIXTURES_DIR_NAME

    try:
        exceptions_text = exceptions_path.read_text(encoding="utf-8")
    except OSError as exc:
        fail(f"nao consegui ler {exceptions_path}: {exc}")
    try:
        baseline_text = baseline_path.read_text(encoding="utf-8")
    except OSError as exc:
        fail(f"nao consegui ler {baseline_path}: {exc}")
    todo_text = None
    if todo_path.exists():
        todo_text = todo_path.read_text(encoding="utf-8", errors="replace")

    lines, ok, findings = run_gate(root, exceptions_text, baseline_text, todo_text, calibration_dir)
    for line in lines:
        print(f"{SCRIPT_NAME}: {line}" if not line.startswith(" ") else line)

    if parsed.json is not None:
        Path(parsed.json).write_text(json.dumps(findings, indent=1, ensure_ascii=False, default=list))

    if not ok:
        sys.exit(1)


# ============================================================
# --selftest
# ============================================================

def _fixture_path(name):
    return Path(__file__).resolve().parent / FIXTURES_DIR_NAME / name


DIRTY_FIXTURES = [
    "bad_push_back.cpp",
    "bad_string_from_view.cpp",
    "bad_transitive.cpp",
    "bad_new_array.cpp",
    "bad_duplicate_site_merges.cpp",
    "calib_dirty_complex_match.cpp",
]
CLEAN_FIXTURES = [
    "good_fixed_buffer.cpp",
    "good_try_catch.cpp",
    "good_not_noexcept.cpp",
    "good_nothrow_new.cpp",
    "good_string_view_substr.cpp",
    "calib_clean_complex_match.cpp",
]


def selftest_dirty_fixtures_are_accused():
    ok = True
    for name in DIRTY_FIXTURES:
        path = _fixture_path(name)
        if not path.exists():
            print(f"selftest: fixture suja ausente: {path}", file=sys.stderr)
            ok = False
            continue
        text = path.read_text(encoding="utf-8")
        tree = analyze_tree([(name, text)])
        found = _tree_has_any_finding(tree)
        if not found:
            print(f"selftest: fixture suja NAO acusada: {name}", file=sys.stderr)
            ok = False
        else:
            print(f"selftest: {name} acusada (linha(s) {sorted(found)}) - ok")
    return ok


def selftest_clean_fixtures_are_absolved():
    ok = True
    for name in CLEAN_FIXTURES:
        path = _fixture_path(name)
        if not path.exists():
            print(f"selftest: fixture limpa ausente: {path}", file=sys.stderr)
            ok = False
            continue
        text = path.read_text(encoding="utf-8")
        tree = analyze_tree([(name, text)])
        found = _tree_has_any_finding(tree)
        if found:
            print(f"selftest: fixture limpa ACUSADA por engano: {name} (linha(s) {sorted(found)})", file=sys.stderr)
            ok = False
        else:
            print(f"selftest: {name} absolvida - ok")
    return ok


def selftest_empty_scan_reproves():
    def empty_enumerate(_root):
        return []

    def fake_read(_path):
        return ""

    lines, ok, _ = run_gate(
        Path("/nonexistent"), "# nenhuma excecao\n", "# nenhum sitio na linha de base\n",
        None, _fixture_path("."), enumerate_fn=empty_enumerate, read_fn=fake_read,
    )
    if ok:
        print("selftest: DEGRAU-1 FALHOU (varredura vazia deveria reprovar)", file=sys.stderr)
        return False
    if not any("arquivos_encontrados=0" in ln for ln in lines):
        print(f"selftest: DEGRAU-1 FALHOU (nao imprimiu arquivos_encontrados=0): {lines}", file=sys.stderr)
        return False
    print("selftest: DEGRAU-1 OK (arquivos_encontrados=0 reprova)")
    return True


def selftest_batch_loss_reproves():
    files = {"a.cpp": "void f() noexcept {}\n"}

    def enumerate_two(_root):
        return ["a.cpp", "b.cpp"]

    def read_only_a(path):
        name = Path(path).name
        if name not in files:
            raise OSError("arquivo nao encontrado (simulado)")
        return files[name]

    lines, ok, findings = run_gate(
        Path("/nonexistent"), "# nenhuma excecao\n", "# nenhum sitio na linha de base\n",
        None, _fixture_path("."), enumerate_fn=enumerate_two, read_fn=read_only_a,
    )
    _ = lines, ok  # inspecionados via findings["reasons"] abaixo
    reasons = findings["reasons"]
    if not any("pulado" in r for r in reasons):
        print(f"selftest: LOTE-PARCIAL FALHOU (nao registrou o arquivo pulado): {reasons}", file=sys.stderr)
        return False
    print("selftest: LOTE-PARCIAL OK (arquivo que falha a leitura e registrado, nunca some em silencio)")
    return True


def selftest_calibration_wired_into_real_gate():
    """Prova que run_gate() de fato roda a calibracao (nao so os
    testes dedicados acima, que chamam analyze_tree() direto) - contra
    um repo VAZIO (zero arquivo em src/include), a calibracao ainda
    roda e ainda precisa passar."""
    def empty_enumerate(_root):
        return []

    lines, _ok, findings = run_gate(
        Path("/nonexistent"), "# nenhuma excecao\n", "# nenhum sitio na linha de base\n",
        None, _fixture_path("."), enumerate_fn=empty_enumerate,
    )
    if not findings["calib_ok"]:
        print(f"selftest: CALIBRACAO-LIGADA FALHOU (calibracao nao passou mesmo em repo vazio): {lines}", file=sys.stderr)
        return False
    if not any("calibracao: fixture_suja=ACUSADA" in ln for ln in lines):
        print(f"selftest: CALIBRACAO-LIGADA FALHOU (linha de calibracao ausente do relatorio): {lines}", file=sys.stderr)
        return False
    print("selftest: CALIBRACAO-LIGADA OK (roda em toda execucao, inclusive repo vazio)")
    return True


def selftest_family_a_ratchet_new_site_reproves():
    files = {"src/probe.cpp": "std::vector<int> f() noexcept { std::vector<int> v; return v; }\n"}

    def enumerate_one(_root):
        return ["src/probe.cpp"]

    def read_one(_path):
        return files["src/probe.cpp"]

    lines, ok, findings = run_gate(
        Path("/nonexistent"), "# nenhuma excecao\n",
        "# nenhum sitio na linha de base\n",  # baseline vazia - o sitio acima e sempre "novo"
        None, _fixture_path("."), enumerate_fn=enumerate_one, read_fn=read_one,
    )
    if ok:
        print("selftest: CATRACA-A-NOVO FALHOU (sitio de familia A fora da baseline deveria reprovar)", file=sys.stderr)
        return False
    if not findings["familia_A_novos"]:
        print(f"selftest: CATRACA-A-NOVO FALHOU (nao listou sitio novo): {lines}", file=sys.stderr)
        return False
    print(f"selftest: CATRACA-A-NOVO OK (sitio fora da baseline reprova): {findings['familia_A_novos']}")
    return True


def selftest_family_a_ratchet_orphan_reproves():
    files = {"src/probe.cpp": "void f() noexcept {}\n"}  # nenhum sitio real

    def enumerate_one(_root):
        return ["src/probe.cpp"]

    def read_one(path):
        return files["src/probe.cpp"]

    baseline = "src/probe.cpp|f|vector:default-decl\n"  # linha que nao existe mais na fonte
    lines, ok, findings = run_gate(
        Path("/nonexistent"), "# nenhuma excecao\n", baseline,
        None, _fixture_path("."), enumerate_fn=enumerate_one, read_fn=read_one,
    )
    if ok:
        print("selftest: CATRACA-A-ORFAO FALHOU (linha da baseline sem sitio deveria reprovar)", file=sys.stderr)
        return False
    if not findings["familia_A_orfaos"]:
        print(f"selftest: CATRACA-A-ORFAO FALHOU (nao listou orfao): {lines}", file=sys.stderr)
        return False
    print(f"selftest: CATRACA-A-ORFAO OK (sitio que sumiu da fonte reprova): {findings['familia_A_orfaos']}")
    return True


def selftest_family_a_ratchet_matching_baseline_passes():
    files = {"src/probe.cpp": "void f() noexcept { std::vector<int> v; (void)v; }\n"}

    def enumerate_one(_root):
        return ["src/probe.cpp"]

    def read_one(path):
        return files["src/probe.cpp"]

    baseline = "src/probe.cpp|f|vector:default-decl\n"
    lines, ok, findings = run_gate(
        Path("/nonexistent"), "# nenhuma excecao\n", baseline,
        None, _fixture_path("."), enumerate_fn=enumerate_one, read_fn=read_one,
    )
    if not ok:
        print(f"selftest: CATRACA-A-ESTAVEL FALHOU (sitio que bate com a baseline nao deveria reprovar): {lines}", file=sys.stderr)
        return False
    print("selftest: CATRACA-A-ESTAVEL OK (sitio igual a baseline nao reprova)")
    return True


def selftest_family_b_exception_accepted():
    files = {"src/probe.cpp": "void f() noexcept { std::vector<int> v; v.push_back(1); }\n"}

    def enumerate_one(_root):
        return ["src/probe.cpp"]

    def read_one(path):
        return files["src/probe.cpp"]

    exceptions = "src/probe.cpp|f|FALSO-POSITIVO-DA-REGUA|explicacao qualquer|SEM-PENDENCIA\n"
    lines, ok, findings = run_gate(
        Path("/nonexistent"), exceptions, "# nenhum sitio na linha de base\n",
        None, _fixture_path("."), enumerate_fn=enumerate_one, read_fn=read_one,
    )
    b_reproved = any("familia B" in r for r in findings["reasons"])
    if b_reproved:
        print(f"selftest: EXCECAO-B-ACEITA FALHOU (achado coberto por excecao valida reprovou): {lines}", file=sys.stderr)
        return False
    print("selftest: EXCECAO-B-ACEITA OK (excecao com 5 campos validos absolve o achado)")
    return True


def selftest_family_b_exception_with_concluded_item_reproves():
    files = {"src/probe.cpp": "void f() noexcept { std::vector<int> v; v.push_back(1); }\n"}

    def enumerate_one(_root):
        return ["src/probe.cpp"]

    def read_one(path):
        return files["src/probe.cpp"]

    exceptions = "src/probe.cpp|f|FALSO-POSITIVO-DA-REGUA|razao|ITEM-CONCLUIDO\n"
    todo_text = (
        "| WSJF | ID | Onda | Grupo | Descricao | Prioridade | Pre-requisito | Dificuldade | Status | Estado |\n"
        "|---|---|---|---|---|---|---|---|---|---|\n"
        "| 1.0 | ITEM-CONCLUIDO | W1 | X | y | Alta | - | Media | ✅ Concluído | - |\n"
    )
    lines, ok, findings = run_gate(
        Path("/nonexistent"), exceptions, "# nenhum sitio na linha de base\n",
        todo_text, _fixture_path("."), enumerate_fn=enumerate_one, read_fn=read_one,
    )
    if ok:
        print("selftest: EXCECAO-B-ITEM-MORTO FALHOU (item concluido deveria reprovar - regra de morte)", file=sys.stderr)
        return False
    print("selftest: EXCECAO-B-ITEM-MORTO OK (excecao cujo item ja fechou reprova)")
    return True


def selftest_family_b_duplicate_site_merges_into_one_line():
    """Prova o conserto F6 (achado do time-lead, 18/09/2026): um sitio
    (arquivo, linha, nome) que find_functions() acerta encontrar DUAS
    vezes - aqui, por um `struct` de namespace logo apos a funcao
    noexcept, a mesma forma que expos o defeito real em
    src/gfss/selector_parse.cpp - produz UMA SO linha de achado, com os
    DOIS caminhos de heranca (via helper_a/via helper_b) FUNDIDOS no
    mesmo "herdado", nunca duas linhas identicas e nunca um caminho
    descartado."""
    path = _fixture_path("bad_duplicate_site_merges.cpp")
    if not path.exists():
        print(f"selftest: fixture de fusao ausente: {path}", file=sys.stderr)
        return False
    text = path.read_text(encoding="utf-8")

    def enumerate_one(_root):
        return ["probe.cpp"]

    def read_one(_path):
        return text

    lines, ok, findings = run_gate(
        Path("/nonexistent"), "# nenhuma excecao\n", "# nenhum sitio na linha de base\n",
        None, _fixture_path("."), enumerate_fn=enumerate_one, read_fn=read_one,
    )
    if ok:
        print("selftest: FUSAO-DE-SITIO FALHOU (fixture suja deveria reprovar)", file=sys.stderr)
        return False
    target_findings = [f for f in findings["b_findings"] if f["func"] == "target"]
    if len(target_findings) != 1:
        print(
            f"selftest: FUSAO-DE-SITIO FALHOU (esperava 1 achado para target(), achou "
            f"{len(target_findings)}): {lines}",
            file=sys.stderr,
        )
        return False
    indirect = set(target_findings[0]["indirect"])
    if indirect != {"via helper_a", "via helper_b"}:
        print(
            f"selftest: FUSAO-DE-SITIO FALHOU (esperava os DOIS caminhos fundidos, achou "
            f"{sorted(indirect)}): {lines}",
            file=sys.stderr,
        )
        return False
    target_reasons = [r for r in findings["reasons"] if "funcao target()" in r]
    if len(target_reasons) != 1:
        print(
            f"selftest: FUSAO-DE-SITIO FALHOU (esperava 1 motivo de reprovacao para target(), "
            f"achou {len(target_reasons)}): {target_reasons}",
            file=sys.stderr,
        )
        return False
    print(
        "selftest: FUSAO-DE-SITIO OK (sitio duplicado vira 1 linha, "
        f"caminhos fundidos: {sorted(indirect)})"
    )
    return True


def selftest_known_gap_optional_vector_assignment_currently_absolved():
    """MARCADOR DE LACUNA, NAO CONTROLE DE ACERTO (GODS_LAWS.md L-43,
    ESCOPO.md Decisao 13; achado do time-lead em 18/09/2026 contra
    src/platform/gl/gl_context_facade.cpp:329, consertado no codigo
    real pelo commit 7a34bb2). Este controle prova que o motor ATUAL
    ainda ABSOLVE uma copia de std::vector por ATRIBUICAO para dentro
    de um std::optional<std::vector<...>> ja existente, dentro de
    funcao noexcept - a MESMA forma que matava o processo do
    consumidor em QUALQUER sistema (familia B, nao familia A: nao
    depende do modo de depuracao da Microsoft). O veredito esperado
    AQUI E ABSOLVIDA - e o proprio ponto do controle, nao um bug dele.

    SE ISTO COMECAR A FALHAR: nao conserte o controle nem a fixture.
    E SINAL DE ACERTO - o motor foi alargado para rastrear atribuicao
    de contentor (fatia futura, fora do escopo desta). Nesse dia, mova
    tests/tools/fixtures/noexcept_alloc/gap_optional_vector_assignment.cpp
    para DIRTY_FIXTURES e apague este controle-marcador (GODS_LAWS.md
    L-67: o que se revoga se apaga, nunca se arquiva)."""
    name = "gap_optional_vector_assignment.cpp"
    path = _fixture_path(name)
    if not path.exists():
        print(f"selftest: fixture de lacuna ausente: {path}", file=sys.stderr)
        return False
    text = path.read_text(encoding="utf-8")
    tree = analyze_tree([(name, text)])
    found = _tree_has_any_finding(tree)
    if found:
        print(
            f"selftest: LACUNA-ATRIBUICAO-OPTIONAL: o motor passou a acusar "
            f"(linha(s) {sorted(found)}) - isto E SINAL DE ACERTO (a lacuna foi "
            "fechada), NAO uma regressao a consertar: mova a fixture para "
            "DIRTY_FIXTURES e apague este controle-marcador",
            file=sys.stderr,
        )
        return False
    print(
        "selftest: LACUNA-ATRIBUICAO-OPTIONAL OK (marcador de lacuna: o motor ATUAL "
        "ainda absolve atribuicao de vector para dentro de optional<vector>, "
        "ESCOPO.md Decisao 13 - gl_context_facade.cpp:329, consertado em 7a34bb2)"
    )
    return True


def selftest_main():
    controls = [
        selftest_dirty_fixtures_are_accused(),
        selftest_clean_fixtures_are_absolved(),
        selftest_empty_scan_reproves(),
        selftest_batch_loss_reproves(),
        selftest_calibration_wired_into_real_gate(),
        selftest_family_a_ratchet_new_site_reproves(),
        selftest_family_a_ratchet_orphan_reproves(),
        selftest_family_a_ratchet_matching_baseline_passes(),
        selftest_family_b_exception_accepted(),
        selftest_family_b_exception_with_concluded_item_reproves(),
        selftest_family_b_duplicate_site_merges_into_one_line(),
        selftest_known_gap_optional_vector_assignment_currently_absolved(),
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
        real_main(args)


if __name__ == "__main__":
    main()
