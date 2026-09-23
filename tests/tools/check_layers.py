#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_layers.py - CI gate for GODS_LAWS.md L-19 ("a CI gate reproves
# the violation" instead of trusting the discipline of whoever writes
# the code).
#
# PORT of the former tests/tools/check_layers.sh (POSIX sh), retired
# in the same fatia that wrote this file (GATE-TREE-PARITY, GODS_LAWS.md
# L-04, decisao do lider: "O comportamento deve ser igual em qualquer
# OS") - the sh version was if(UNIX)-guarded in tests/CMakeLists.txt,
# so nothing checked layer discipline on the Windows CI job at all).
# Registered here, unguarded, as an ordinary ctest case - the same
# shape check_spdx.py and check_hygiene_coverage.py already proved
# works on all five platforms.
#
# Verifies that the pure layers (src/core/, include/glintfx/core/,
# src/gfss/, include/glintfx/gfss/, src/gfui/, include/glintfx/gfui/)
# do not include (a) a header from a layer above (glintfx/platform/),
# nor (b) an operating system header. None of these layers knows
# anything about the OS - only platform/ does (GODS_LAWS.md L-19), and
# render/ is a documented, deliberate exception of its own (GODS_LAWS.md
# L-31, L-07 EXCECAO No 1: the GL loader), so this gate does not scan it.
#
# LAYERS-GATE-GFSS-GFUI (TODO.md, GODS_LAWS.md L-19/L-40/L-67): gfss/
# and gfui/ used to be a KNOWN, DOCUMENTED gap - src/gfss/CMakeLists.txt
# and src/gfui/CMakeLists.txt's own header comments said so in prose,
# while a system header planted in either directory went unseen by
# this gate. This fatia closes the gap: both layers are enumerated
# below, with their OWN per-directory non-empty floor (see
# GFSS_GFUI_DIR_SPECS's own comment for why aggregate is not enough).
# The two CMakeLists.txt comments that documented the gap are deleted
# in the same commit (GODS_LAWS.md L-67: what stops being true is
# removed, not archived).
#
# ANCHOR-ON-DIRECTIVE (achado real, 22/09/2026, ordem do lider por
# AskUserQuestion: "Olhar so diretivas de inclusao"): alargar o
# escopo acima descobriu que a agulha era procurada em QUALQUER linha,
# comentario incluido - src/gfss/anb_parse.cpp:159's own prose ("a
# GL/WGL function name is") casava com a agulha "GL/" sem nunca ter
# sido um #include. A mesma fragilidade sempre existiu em src/core/,
# so nunca se manifestou porque nenhum comentario de core/ citava uma
# dessas substrings fora de contexto de include. O lider decidiu
# corrigir a semantica do portao para TODAS as camadas que ele varre,
# core/ inclusive: a agulha so e procurada dentro de uma linha que SEJA
# uma diretiva de inclusao - #include (com ou sem espaco depois do
# '#', com ou sem espaco antes do '<'/'"') ou, por este projeto ser
# C++23, a forma de unidade de cabecalho `import <...>;`/`import
# "...";` (com ou sem `export` na frente). Ancorar so em #include
# teria trocado o falso positivo por um falso NEGATIVO pior - um
# `import <fstream>;` real passaria em silencio -, entao as duas
# formas sao reconhecidas. Ver _directive_argument() e
# _ANCHOR_DIRECTIVE_CASES abaixo.
#
# PHASE-2-3 (re-verificacao do lider, 22/09/2026, apos o ANCHOR-ON-
# DIRECTIVE acima): a ancora por LINHA FISICA sozinha trocou o falso
# positivo original por DOIS falsos negativos, medidos contra
# src/core/ - `#include \` + quebra + `<fstream>` e `#include /* nada
# */ <fstream>` (a diretiva partida entre duas linhas fisicas, ou com
# um comentario no meio) nao eram mais vistos. A causa e que o
# pre-processador real nao le linha fisica: a fase 2 da traducao
# emenda barra-invertida-e-nova-linha ANTES de qualquer diretiva ser
# reconhecida, e a fase 3 troca cada comentario por um espaco,
# respeitando literais de cadeia e de caractere. _translate_phases_2_
# and_3() modela as duas fases num unico laco; _logical_lines() a
# chama e so entao aplica _directive_argument() por linha LOGICA
# resultante. Ver o comentario PHASE-2-3 mais abaixo, no topo dessa
# funcao, para o detalhe e os cinco casos confirmados contra `g++
# -std=c++23 -fsyntax-only`.
#
# RAW-STRING-NO-SPLICE (LAYERS-GATE-GFSS-GFUI, sub-fatia L-2,
# 22/09/2026, absorve o item de INBOX PORTAO-DE-CAMADA-NAO-CONHECE-
# CADEIA-BRUTA - o mesmo defeito que o proprio varredor do Clang teve,
# PR #139504): a cadeia bruta do C++ (`R"delim(...)delim"`, com ou sem
# prefixo de codificacao `u8R`/`uR`/`UR`/`LR`) agora e um estado
# proprio do automato (state_raw_string), nao mais texto comum. Antes
# desta fatia, uma aspas ou um `/*` DENTRO do conteudo de uma cadeia
# bruta podia abrir um comentario ou uma string falsos que escondiam
# uma diretiva de inclusao real na linha seguinte - falso negativo, o
# lado perigoso. A funcao que faz o trabalho, _translate_phases_2_and_
# 3(), fundiu o que antes eram duas passadas separadas (_splice_lines()
# e _strip_comments_and_literals(), agora retiradas) numa unica: o
# conteudo de uma cadeia bruta e ISENTO da emenda de linha da fase 2
# (confirmado contra g++ 16.2.1 e clang++ 22.1.8, ver o docstring de
# _translate_phases_2_and_3() para o fixture que prova a diferenca), e
# isso so pode ser modelado processando as duas fases juntas, nunca
# como dois passes sequenciais sobre o texto inteiro.
#
# Usage:
#   check_layers.py <source-root-directory>
#   check_layers.py --selftest
#
# --selftest runs the four original GODS_LAWS.md L-40 controls (positive,
# negative, a SECOND negative specific to the file-I/O headers added by
# the ASSET-LOAD conserto of 28/08/2026, empty-scan) PLUS the four
# controls LAYERS-GATE-GFSS-GFUI adds (a violation control and a
# per-directory floor control, each looped over the four gfss/gfui
# directories; the ANCHOR-ON-DIRECTIVE control table; the PHASE-2-3
# control table; the RAW-STRING control table, nine cases covering
# every prefix form, the 16-character delimiter ceiling, mismatched-
# delimiter non-closure and the no-splice-inside-raw-string trap; and
# the CRLF control table, the same splice/comment/raw-string shapes
# rewritten byte-for-byte with \r\n, proving Windows-checkout line
# endings behave identically to \n - GATE-ENV-SWEEP's LINE_ENDING
# category; and the BOM control table, a leading UTF-8 byte-order mark
# before a real directive vs. before prose only) against disposable
# fixtures under a scratch directory, never against the real tracked
# tree.
#
# Each function below does one thing (GODS_LAWS.md L-17).

import os
import re
import shutil
import sys
import tempfile

SCRIPT_NAME = "check_layers.py"

# Layer above the core (FUND-2's own note: not created yet at the time
# the sh version was written, but the pattern stays ready for when it
# is born).
UPPER_LAYER_NEEDLE = "glintfx/platform/"

# OS headers covered by this gate: Wayland, Win32, GL/EGL, the most
# common low-level POSIX calls, and (ASSET-LOAD conserto, 28/08/2026,
# GODS_LAWS.md L-19/L-40) file I/O - <filesystem> and <fstream>. Ported
# verbatim from OS_HEADER_PATTERN in the sh version - see that file's
# own history (its own --selftest's selftest_negative_control_file_
# header) for why <filesystem>/<fstream> are here: a scratch core file
# that #includes <fstream> passed the PREVIOUS pattern clean before
# that fix.
OS_HEADER_NEEDLES = (
    "wayland",
    "windows.h",
    "winuser",
    "GL/",
    "EGL/",
    "<dlfcn",
    "<unistd",
    "<sys/",
    "<fcntl",
    "<filesystem",
    "<fstream",
)

# FILE-EXTENSIONS-CPP23 (LAYERS-GATE-GFSS-GFUI, sub-fatia L-3, achado do
# revisor de 23/09/2026: "extensoes de C++ fora da lista - .ipp e
# unidades de modulo como .cppm/.ixx - relevantes agora que o portao
# reconhece import"). Alarga a lista original com as extensoes de
# implementacao inline reais (.ipp/.tpp/.inl - corpo de template incluido
# de volta num .hpp, nunca compilado sozinho, mas pode conter uma
# diretiva de inclusao tanto quanto qualquer .hpp) e as quatro extensoes
# de unidade de modulo C++20/23 que os principais toolchains usam
# (.cppm - Clang/CMake; .ixx - MSVC; .mpp e .ccm/.cxxm - vistas noutros
# projetos/toolchains; nenhuma delas e' normativa da ISO, que so define a
# SINTAXE de unidade de modulo, nunca uma extensao de arquivo). Medido
# em 23/09/2026 (GODS_LAWS.md L-44, fato antes de suposicao): nenhum
# arquivo com qualquer uma destas oito extensoes existe hoje na arvore
# real (find sem resultado nos seis diretorios cobertos), entao alargar
# a lista e' seguro - nao ha arquivo novo pra varredura real descobrir e
# quebrar a paridade do piso nao-vazio.
_HEADER_EXTENSIONS = (
    ".hpp",
    ".cpp",
    ".h",
    ".hh",
    ".hxx",
    ".cc",
    ".cxx",
    ".ipp",
    ".tpp",
    ".inl",
    ".cppm",
    ".ixx",
    ".mpp",
    ".ccm",
    ".cxxm",
)

_FORBIDDEN_PATTERN = re.compile(
    "|".join(re.escape(needle) for needle in (UPPER_LAYER_NEEDLE,) + OS_HEADER_NEEDLES)
)

# ANCHOR-ON-DIRECTIVE (see this file's own top-of-file comment): the
# needle is only ever searched inside the ARGUMENT of one of these two
# directive forms, never in a bare line. `^\s*` only tolerates leading
# WHITESPACE before the keyword - a "//" comment prefix, or any other
# non-whitespace text, rules the line out before the keyword is even
# reached, so a commented-out include ("// #include <windows.h>") does
# not match either pattern.
#
# DIGRAPH (LAYERS-GATE-GFSS-GFUI, sub-fatia L-3, regressao (b) do
# revisor de 23/09/2026: "%:include <fstream> - forma alternativa
# valida de # no C++23, puxa o cabecalho, e a ancora so reconhece #").
# `%:` e' um dos seis TOKENS ALTERNATIVOS da norma (junto com `<%` `%>`
# `<:` `:>` `%:%:`), spelling puramente lexico do token `#` - qualquer
# lugar onde `#` introduz uma diretiva, `%:` funciona identico.
# Confirmado contra g++ 16.2.1 e clang++ 22.1.8 (GODS_LAWS.md L-22, o
# compilador antes da minha tabela) em TRES formas: `%:include
# <fstream>` (colado), `%:  include <fstream>` (espaco depois do `:`,
# antes de `include`) e `%:define HDR <fstream>` - as tres puxam
# <fstream> de verdade nos dois compiladores, igual a `#include
# <fstream>`. UMA forma NAO e reconhecida por nenhum dos dois: `% :
# include <fstream>` (espaco ENTRE `%` e `:`) - os dois compiladores
# ecoam a linha como texto comum, sem processar diretiva nenhuma, porque
# o digrafo e' um token de DOIS CARACTERES ADJACENTES, nao duas
# pontuacoes soltas que a tokenizacao junta com espaco no meio. Por
# isso o regex casa `%:` colado (sem `\s*` entre os dois caracteres),
# com `\s*` so' ENTRE o digrafo/`#` e a palavra `include` - selftest_
# anchor_directive_control() abaixo tem os quatro casos (as tres formas
# que reprovam mais a que nao e digrafo e deve passar limpa).
_INCLUDE_DIRECTIVE_PATTERN = re.compile(r'^\s*(?:#|%:)\s*include\s*(<[^>\n]*>|"[^"\n]*")')
_IMPORT_DIRECTIVE_PATTERN = re.compile(r'^\s*(?:export\s+)?import\s*(<[^>\n]*>|"[^"\n]*")\s*;')


def _directive_argument(line):
    """Returns the bracketed/quoted header-name argument of a LOGICAL
    line (post phase-2/phase-3, see _scan_directives() below) that is
    a #include/%:include or a C++23 header-unit import directive with a
    LITERAL header-name argument, or None when the line is not that
    (prose, a comment, plain code, a module import with no header-name
    form, OR a #include/%:include whose argument is not literal - see
    COMPUTED-INCLUDE below for that last case, which this function
    deliberately leaves to its caller). Only the captured argument -
    never the rest of the line - is searched for the forbidden needles,
    so a real include followed by an unrelated trailing comment on the
    same logical line ("#include <good.hpp> // GL/ stuff") does not
    false-positive on the comment half either.
    """
    match = _INCLUDE_DIRECTIVE_PATTERN.match(line) or _IMPORT_DIRECTIVE_PATTERN.match(line)
    return match.group(1) if match else None


# COMPUTED-INCLUDE (LAYERS-GATE-GFSS-GFUI, sub-fatia L-3, regressao (a)
# do revisor de 23/09/2026: "#define X <fstream> seguido de #include X
# puxa o cabecalho de verdade e passa limpo, porque o portao nao modela
# a fase 4 da traducao e nao declara isso"). _directive_argument() so'
# reconhece a forma LITERAL (`<...>`/`"..."` direto depois de
# include/import) - um `#include X` com X um identificador bruto nunca
# batia em `_INCLUDE_DIRECTIVE_PATTERN` (o grupo capturado exige
# `<...>`/`"..."`), entao a linha inteira era invisivel pra
# _directive_lines() (nem contava como diretiva, nem entrava na lista
# de violacoes) - o portao simplesmente nao via a diretiva, muito menos
# o cabecalho que ela puxava de verdade.
#
# Confirmado contra g++ 16.2.1 e clang++ 22.1.8 (GODS_LAWS.md L-22)
# ANTES de desenhar o conserto: `#define HDR <fstream>` + `#include
# HDR` PUXA <fstream> de verdade nos dois compiladores - com corpo em
# aspas (`#define HDR "fstream"`, a forma "..." tambem resolve, cai no
# caminho de busca do <...> quando nao acha local), combinado com o
# digrafo (`%:include HDR`), e ATE com macro de FUNCAO (`#define
# HDR(x) <fstream>` + `#include HDR(1)`) - as quatro formas funcionam
# de verdade no preprocessador real.
#
# Desenho FECHADO POR PADRAO (GODS_LAWS.md L-02, "e' biblioteca, nao
# aplicacao" - mas o principio vale aqui: o portao nunca da o beneficio
# da duvida): toda diretiva #include/%:include cujo argumento NAO e' um
# nome de cabecalho literal reprova com mensagem propria ("inclusao
# computada nao verificavel"), EXCETO quando o argumento e' um
# identificador BRUTO (nenhum outro token na linha) que casa com uma
# macro de OBJETO (sem parametros - `#define IDENT <literal>`, nunca
# `#define IDENT(x) ...`) definida no MESMO ARQUIVO cujo corpo e'
# EXATAMENTE um nome de cabecalho literal - so' entao o literal
# resolvido e' buscado contra as agulhas, exatamente como o argumento
# direto sempre foi.
#
# Por que macro de FUNCAO fica de fora por desenho, mesmo funcionando
# no compilador real (confirmado acima): resolver o valor de uma
# invocacao de macro de funcao exigiria modelar substituicao de
# parametro, o que este portao NUNCA fez pra nenhuma outra forma de
# macro - e o desenho fechado-por-padrao ja cobre o caso com seguranca
# SEM resolver: `#include HDR(1)` cai no ramo "identificador bruto" so'
# se a linha inteira apos "include" for UM identificador e mais nada
# (`_BARE_MACRO_IDENTIFIER_PATTERN`); "HDR(1)" tem `(1)` sobrando, entao
# nunca casa esse padrao, e a macro de objeto nunca e' encontrada pra
# esse nome (mesmo que exista uma FUNCAO com esse nome) - cai direto em
# "computada nao verificavel", o veredito seguro. Nenhum #include/
# #define computado existe hoje na arvore real (medido em 23/09/2026,
# grep vazio nos seis diretorios cobertos) - fechar isso nao quebra a
# varredura real.
#
# `import`: NENHUMA resolucao por macro se aplica - `import IDENT;` com
# IDENT bruto (nao literal `<...>`/"...") fica como estava ANTES desta
# fatia, inteiramente ignorado pelo portao, tanto quando IDENT e' macro
# de objeto de cabecalho literal quanto quando nao e' - DECISAO DE
# ESCOPO DECLARADA, nao lacuna escondida: import de MODULO NOMEADO
# (`import std;`, `import foo.bar;`) e' sintaxe legitima do C++23 sem
# relacao nenhuma com cabecalho ou SO, e e' INDISTINGUIVEL de um import
# de header-unit computado por macro sem analise mais funda (a norma so
# decide "e' header-unit" ou "e' modulo" depois de tentar expandir
# macro e ver se o resultado tem forma de nome-de-cabecalho - este
# portao nao faz esse nivel de analise pra nenhuma forma de import).
# Resolver SO' os casos "macro reconhecida" e reprovar so' os "nao
# reconhecida" criaria uma REGRA ASSIMETRICA dificil de justificar (por
# que so' import ganha macro-resolucao, e so' quando a macro existe?) -
# a opcao mais simples e mais segura e' nao mexer no comportamento de
# `import` nenhum, e deixar a lacuna estreita que sobra declarada: um
# import de header-unit computado por macro (que nunca apareceu neste
# projeto ate hoje - projeto nao usa modulo nomeado nenhum ainda) fica
# fora do escopo desta fatia, tanto pra resolver quanto pra reprovar.
# `#include`/`%:include`, ao contrario, NUNCA sao ambiguos com outra
# sintaxe legitima - todo #include tem de resolver pra um cabecalho ou
# reprovar, entao o fechado-por-padrao se aplica ali sem esse risco.
_INCLUDE_KEYWORD_PATTERN = re.compile(r'^\s*(?:#|%:)\s*include\s*(.*)$')
_BARE_MACRO_IDENTIFIER_PATTERN = re.compile(r'^([A-Za-z_]\w*)\s*$')
_OBJECT_MACRO_DEFINE_PATTERN = re.compile(
    r'^\s*(?:#|%:)\s*define\s+([A-Za-z_]\w*)\s+(<[^>\n]*>|"[^"\n]*")\s*$'
)


# PHASE-2-3 (achado real, 22/09/2026, re-verificacao do lider apos o
# ANCHOR-ON-DIRECTIVE acima): _directive_argument() sozinho ancora
# certo NA LINHA FISICA, mas o pre-processador real NAO le linha
# fisica. Medido em copia fora da arvore, nos DOIS sentidos, contra
# src/core/ (a camada que ja era protegida antes desta fatia):
#   - `#include \` + quebra + `<fstream>` - o portao ancorado por linha
#     fisica NAO via (falso negativo: cada metade da diretiva cai em
#     uma linha fisica diferente, nenhuma delas casa sozinha).
#   - `#include /* nada */ <fstream>` - idem (o comentario de bloco no
#     meio da diretiva impede o regex de casar `#\s*include\s*<...>`
#     como uma unica sequencia).
# A causa: a norma C++ especifica duas fases de traducao ANTES de
# qualquer diretiva ser reconhecida - fase 2 emenda toda linha
# terminada em barra invertida com a seguinte (ANTES de qualquer
# comentario ou literal ser identificado, entao a emenda acontece
# mesmo "dentro" do que vira comentario/string), e fase 3 troca cada
# comentario por um unico espaco, respeitando literais de cadeia e de
# caractere (um '/*' ou uma '"' dentro de aspas nao abre comentario
# nem fecha a string) - EXCETO dentro de uma cadeia bruta, onde nem a
# fase 2 se aplica (RAW-STRING-NO-SPLICE, ver o docstring da funcao
# abaixo). _translate_phases_2_and_3() modela as duas fases NUM SO
# LACO (nao duas passadas sequenciais - a cadeia bruta so pode ser
# isenta da fase 2 se a decisao "estou numa cadeia bruta?" for tomada
# ANTES da emenda de linha rodar sobre aquele trecho, nao depois);
# _logical_lines() a chama e SO ENTAO _scan_directives() aplica _directive_argument()
# por linha LOGICA resultante, preservando o numero da linha FISICA
# original (a do primeiro caractere da linha logica) para a mensagem
# de reprovacao continuar citando arquivo:linha certo. Cada caso deste
# comentario foi confirmado contra o compilador real (`g++ -std=c++23
# -fsyntax-only`) antes de virar controle de --selftest - GODS_LAWS.md
# L-22: "a pesquisa vem antes do planejamento", aqui aplicada como "o
# compilador vem antes da minha tabela".
#
# FIM DE LINHA \r\n (GATE-ENV-SWEEP, categoria LINE_ENDING, TODO.md):
# o `\r` no frozenset logo abaixo e' so' MAIS UM caractere proibido de
# delimitador de cadeia bruta (junto com espaco/tab/parenteses/barra
# invertida/aspas), nao tratamento especial de fim de linha - mas o
# projeto roda nos cinco alvos (GODS_LAWS.md L-04, "comportamento
# igual em todo sistema, provado em cada um"), inclusive Windows, onde
# o checkout entrega `\r\n` (`core.autocrlf`, o mesmo fato que ja
# mordeu `check_dep_zero.py::_read_lines_latin1()`, commit 9288916) -
# entao a pergunta de produto e' real: um arquivo `\r\n`-terminado
# passa pelas TRES peças novas desta fatia (emenda de linha, fim de
# comentario de linha, fecho de cadeia bruta) do MESMO jeito que um
# `\n`-terminado? MEDIDO, nao suposto (GODS_LAWS.md L-04): sim, nos
# tres, confirmado contra g++ 16.2.1 e clang++ 22.1.8 e contra
# _logical_lines()/_scan_directives() desta funcao, com fixtures `\r\n`-puros -
# selftest_crlf_control() mais abaixo, quatro casos. As razoes, uma
# por peça: (1) a emenda de linha (fase 2) JA tratava `\r\n` desde
# antes desta fatia - o ramo `text[i + 1] == "\r" and ... == "\n"`
# em _translate_phases_2_and_3() e' herdado sem mudanca do antigo
# _splice_lines(); (2) o fim de um comentario de linha e o fecho de
# uma cadeia bruta so' testam `ch == "\n"`, nunca `ch == "\r"` - um
# `\r` que sobra antes do `\n` e' apenas mais um caractere comum
# (engolido dentro do comentario, ou dentro do corpo da cadeia bruta,
# igual a qualquer outro), entao a fronteira real (o `\n`) chega no
# mesmo lugar com ou sem o `\r` na frente; (3) um `\r` residual que
# sobrevive em `out` (por exemplo, no FIM de uma linha logica comum)
# nunca atrapalha _directive_argument(), porque a ancora e' so' no
# INICIO da linha (`^\s*#...`) e a captura do argumento para antes de
# qualquer `\r`/`\n`.
_RAW_STRING_PREFIXES = ("u8R", "uR", "UR", "LR", "R")
_RAW_STRING_MAX_DELIMITER_LENGTH = 16
_RAW_STRING_FORBIDDEN_DELIMITER_CHARS = frozenset(" \t\v\f\r\n()\\\"")


def _raw_string_prefix_length(out):
    """`out` e a lista de caracteres ja emitidos (a view JA passada
    pela fase 2, ate a posicao atual). Retorna o comprimento do
    prefixo de cadeia bruta (`R`, `u8R`, `uR`, `UR` ou `LR`) que
    termina exatamente no fim de `out`, ou 0 quando nenhuma das cinco
    formas casa ali - inclusive quando a forma casa mas o caractere
    imediatamente anterior a ela e alfanumerico ou `_`, o que
    significa que faz parte de um identificador MAIOR (`fooR"(...)"` e
    o identificador `fooR` seguido de uma string comum, nunca uma
    cadeia bruta - GODS_LAWS.md L-17, enumerar o espaco fechado das
    cinco formas inteiro, em vez de so casar a substring).
    """
    tail = "".join(out[-3:])
    for prefix in _RAW_STRING_PREFIXES:
        if not tail.endswith(prefix):
            continue
        before_index = len(out) - len(prefix) - 1
        if before_index >= 0:
            before_char = out[before_index]
            if before_char.isalnum() or before_char == "_":
                continue
        return len(prefix)
    return 0


def _raw_string_delimiter(text, start):
    """`start` e o indice logo apos a aspas de abertura de uma cadeia
    bruta ja confirmada por _raw_string_prefix_length(). Le o
    delimitador (0 a 16 caracteres, nenhum deles espaco/tab/nova-linha/
    parenteses/barra-invertida/aspas - GODS_LAWS.md projeto L-32
    emenda 22/09/2026, confirmado contra g++ 16.2.1: um delimitador de
    17 caracteres e erro de compilacao real, "delimitador de string
    nao tratada (raw) maior do que 16 caracteres"). Retorna (delimiter,
    body_start) quando a sequencia e valida e termina em '(' -
    body_start e o indice do primeiro caractere DEPOIS desse '(' -, ou
    None quando nao e uma abertura de cadeia bruta valida (a aspas
    volta a ser tratada como string comum pelo chamador).
    """
    i = start
    n = len(text)
    delimiter_chars = []
    while i < n:
        ch = text[i]
        if ch == "(":
            return "".join(delimiter_chars), i + 1
        if ch in _RAW_STRING_FORBIDDEN_DELIMITER_CHARS:
            return None
        if len(delimiter_chars) >= _RAW_STRING_MAX_DELIMITER_LENGTH:
            return None
        delimiter_chars.append(ch)
        i += 1
    return None


def _translate_phases_2_and_3(text):
    """Fases 2 (emenda de linha) e 3 (troca de comentario por espaco,
    reconhecimento de string/char/cadeia-bruta) da traducao, FUNDIDAS
    num unico laco sobre o texto ORIGINAL - substitui as antigas
    _splice_lines()/_strip_comments_and_literals() (duas passadas
    sequenciais, retiradas nesta fatia), porque o conteudo de uma
    cadeia bruta e ISENTO da fase 2 (RAW-STRING-NO-SPLICE): uma barra
    invertida imediatamente seguida de nova linha, DENTRO de
    `R"delim(...)delim"`, fica como dois caracteres literais em vez de
    ser apagada como em qualquer outro lugar do arquivo. Confirmado
    contra g++ 16.2.1 e clang++ 22.1.8 (`-fsyntax-only` e `-E`, ambos
    concordam): um programa com
        R"END(before
        )EN\\
        D"
        #include <fstream>
        after
        )END";
    NAO inclui `<fstream>` em nenhum dos dois compiladores - a barra
    invertida e a nova linha entre "EN" e "D\"" permanecem literais, o
    que significa que `)END"` (a sequencia de fecho real, delimitador
    "END") so aparece na ULTIMA linha, e tudo antes dela - inclusive o
    `#include <fstream>` no meio - e conteudo inerte da cadeia. Se a
    fase 2 rodasse ANTES do reconhecimento de cadeia bruta (como numa
    passada separada faria), essa barra-invertida-e-nova-linha seria
    apagada, formando um `)END"` PREMATURO logo apos "before\\n)" - a
    cadeia fecharia cedo demais e o `#include <fstream>` sobrevivente
    passaria a ser codigo de verdade, reprovando por um motivo que o
    compilador real nao reprova. selftest_raw_string_control() tem o
    caso `case_backslash_newline_not_spliced_inside_raw_string` que
    planta exatamente este fixture.

    Comentarios (state_block/state_line, inalterados) e corpo de
    cadeia bruta (state_raw_string, novo) colapsam para um UNICO
    espaco na saida, mesmo tratamento e mesmo motivo: o unico
    consumidor desta funcao, _logical_lines() abaixo, separa linhas logicas que _scan_directives() acha diretivas
    casando um regex POR LINHA LOGICA depois de separar clean_text em
    '\\n' - qualquer coisa que nao pode nunca ser confundida com texto
    de #include/import tem de SUMIR de clean_text, nao so ser marcada,
    senao uma cadeia bruta multi-linha cujo conteudo inclui uma linha
    que le "#include <fstream>" como DADO inerte
    (selftest_raw_string_control()'s case_b) sobreviveria a separacao
    e daria falso positivo.

    Retorna (clean_text, clean_lines) com clean_lines[i] a linha
    FISICA original (1-based) de clean_text[i] - mesmo contrato que as
    duas funcoes retiradas ofereciam juntas.
    """
    (
        state_code,
        state_string,
        state_char,
        state_block,
        state_line,
        state_raw_string,
    ) = range(6)
    state = state_code
    out = []
    out_lines = []
    line_no = 1
    comment_start_line = None
    raw_closer = None
    raw_start_line = None
    n = len(text)
    i = 0
    while i < n:
        # Fase 2: emenda barra-invertida-e-nova-linha em TODO estado,
        # exceto dentro do corpo de uma cadeia bruta (RAW-STRING-NO-
        # SPLICE, ver o docstring acima) - inclusive dentro do que vai
        # virar comentario ou string comum, onde a emenda ja se
        # aplicava antes desta fatia (PHASE-2-3 acima, caso "e").
        if state != state_raw_string:
            if text[i] == "\\" and i + 1 < n and text[i + 1] in ("\n", "\r"):
                if text[i + 1] == "\r" and i + 2 < n and text[i + 2] == "\n":
                    i += 3
                else:
                    i += 2
                line_no += 1
                continue

        ch = text[i]
        nxt = text[i + 1] if i + 1 < n else ""

        if state == state_code:
            if ch == "/" and nxt == "*":
                state = state_block
                comment_start_line = line_no
                i += 2
                continue
            if ch == "/" and nxt == "/":
                state = state_line
                comment_start_line = line_no
                i += 2
                continue
            if ch == '"':
                prefix_length = _raw_string_prefix_length(out)
                delimiter_result = _raw_string_delimiter(text, i + 1) if prefix_length else None
                if delimiter_result is not None:
                    delimiter, body_start = delimiter_result
                    raw_closer = ")" + delimiter + '"'
                    raw_start_line = line_no
                    state = state_raw_string
                    i = body_start
                    continue
                state = state_string
            elif ch == "'":
                state = state_char
            out.append(ch)
            out_lines.append(line_no)
            if ch == "\n":
                line_no += 1
            i += 1
            continue
        if state in (state_string, state_char):
            out.append(ch)
            out_lines.append(line_no)
            if ch == "\n":
                line_no += 1
            if ch == "\\" and i + 1 < n:
                out.append(text[i + 1])
                out_lines.append(line_no)
                if text[i + 1] == "\n":
                    line_no += 1
                i += 2
                continue
            if (state == state_string and ch == '"') or (state == state_char and ch == "'"):
                state = state_code
            i += 1
            continue
        if state == state_block:
            if ch == "*" and nxt == "/":
                out.append(" ")
                out_lines.append(comment_start_line)
                state = state_code
                i += 2
                continue
            if ch == "\n":
                line_no += 1
            i += 1
            continue
        if state == state_line:
            if ch == "\n":
                out.append(" ")
                out_lines.append(comment_start_line)
                out.append(ch)
                out_lines.append(line_no)
                state = state_code
                line_no += 1
                i += 1
                continue
            i += 1
            continue
        if state == state_raw_string:
            if text[i : i + len(raw_closer)] == raw_closer:
                out.append(" ")
                out_lines.append(raw_start_line)
                i += len(raw_closer)
                state = state_code
                raw_closer = None
                raw_start_line = None
                continue
            if ch == "\n":
                line_no += 1
            i += 1
            continue
    if state in (state_line, state_block):
        # Comentario nao fechado ate o fim do arquivo (arquivo mal
        # formado) - emite o espaco mesmo assim, em vez de descartar
        # o restante em silencio; um '*/' faltando ja seria erro de
        # compilacao real antes disso.
        out.append(" ")
        out_lines.append(comment_start_line)
    elif state == state_raw_string:
        # Mesma logica para uma cadeia bruta nunca fechada.
        out.append(" ")
        out_lines.append(raw_start_line)
    return "".join(out), out_lines


def _logical_lines(text):
    """Gera (linha_fisica, texto_logico) para toda linha logica NAO-VAZIA
    de `text`, apos modelar as fases 2 e 3 da traducao (ver o comentario
    PHASE-2-3 acima). `linha_fisica` e a linha do PRIMEIRO caractere da
    linha logica correspondente - onde ela de fato comeca no arquivo
    real, mesmo quando foi emendada ou teve um comentario no meio. Era o
    laco interno de _directive_lines() (retirada nesta fatia, virou
    _scan_directives() abaixo) - fatorada porque COMPUTED-INCLUDE
    precisa de DUAS passadas pelas mesmas linhas logicas (uma pra
    coletar macro de objeto via #define, outra pra resolver diretivas),
    e as duas tem de ver a MESMA segmentacao, nao duas independentes que
    pudessem divergir.
    """
    clean_text, clean_lines = _translate_phases_2_and_3(text)

    start = 0
    n = len(clean_text)
    for idx, ch in enumerate(clean_text):
        if ch != "\n":
            continue
        segment = clean_text[start:idx]
        if segment.strip():
            yield clean_lines[start], segment
        start = idx + 1
    if start < n:
        segment = clean_text[start:n]
        if segment.strip():
            yield clean_lines[start], segment


def _collect_object_macros(logical_lines):
    """Retorna {nome: literal} para toda macro de OBJETO (sem parametro,
    ver COMPUTED-INCLUDE acima) cujo corpo e' EXATAMENTE um nome de
    cabecalho literal, definida em qualquer lugar do arquivo. NAO
    exige que o #define apareca ANTES do #include que a usa (o
    preprocessador real exigiria - um #include de macro ainda nao
    definida e' erro de compilacao de verdade) - simplificacao
    DECLARADA (GODS_LAWS.md L-44, nao escondida): um arquivo que usa a
    macro fora de ordem nao compila de qualquer forma, entao o
    veredito deste portao pra esse caso e' irrelevante pro produto -
    nunca esconde uma diretiva que compilaria de verdade.
    """
    object_macros = {}
    for _lineno, segment in logical_lines:
        match = _OBJECT_MACRO_DEFINE_PATTERN.match(segment)
        if match:
            object_macros[match.group(1)] = match.group(2)
    return object_macros


def _scan_directives(text):
    """Substitui _directive_lines() (retirada nesta fatia). Retorna
    (resolved, unresolved): `resolved` e uma lista de (linha_fisica,
    argumento_literal) - toda diretiva #include/%:include/import cujo
    argumento e' um nome de cabecalho literal, direto ou resolvido por
    macro de objeto do MESMO arquivo (ver COMPUTED-INCLUDE acima) -, a
    buscar contra as agulhas exatamente como antes desta fatia;
    `unresolved` e a lista de linha_fisica de toda diretiva #include/
    %:include cujo argumento NAO e' literal e NAO resolve por macro
    local - reprova por si so' (fechado por padrao), sem precisar bater
    com nenhuma agulha, porque o portao nao consegue provar que o
    cabecalho puxado e' seguro.
    """
    logical_lines = list(_logical_lines(text))
    object_macros = _collect_object_macros(logical_lines)

    resolved = []
    unresolved = []
    for lineno, segment in logical_lines:
        argument = _directive_argument(segment)
        if argument is not None:
            resolved.append((lineno, argument))
            continue

        include_match = _INCLUDE_KEYWORD_PATTERN.match(segment)
        if include_match is None:
            continue  # nao e #include/%:include nem import - codigo comum, ja limpo de comentario/string pela fase 2/3.

        bare_match = _BARE_MACRO_IDENTIFIER_PATTERN.match(include_match.group(1))
        macro_value = object_macros.get(bare_match.group(1)) if bare_match else None
        if macro_value is not None:
            resolved.append((lineno, macro_value))
        else:
            unresolved.append(lineno)
    return resolved, unresolved


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


def core_source_dirs(root):
    for candidate in (
        os.path.join(root, "src", "core"),
        os.path.join(root, "include", "glintfx", "core"),
    ):
        if os.path.isdir(candidate):
            yield candidate


def _walk_header_files(directory):
    files = []
    for dirpath, _dirnames, filenames in os.walk(directory):
        for name in filenames:
            if name.endswith(_HEADER_EXTENSIONS):
                files.append(os.path.join(dirpath, name))
    return files


def core_source_files(root):
    """One os.walk() per candidate directory - directory entries come
    back as discrete strings from the filesystem, never a newline-
    joined text stream a hostile filename could split (the same
    GATE-TREE-PARITY-NEWLINE reasoning check_vendor_purity.py's own
    header documents).
    """
    files = []
    for source_dir in core_source_dirs(root):
        files.extend(_walk_header_files(source_dir))
    return files


# BOM-STRIP (achado real, 23/09/2026, re-verificacao do orquestrador,
# familia DIFERENTE da minha - codificacao, nao sintaxe): uma marca de
# ordem de bytes UTF-8 (`EF BB BF`, decodifica pra `U+FEFF`) no INICIO
# do arquivo e' descartada pelo compilador ANTES da fase 1 - g++
# 16.2.1 e clang++ 22.1.8 confirmados, os dois aceitam `<BOM>#include
# <fstream>` e puxam `<fstream>` normalmente. `encoding="utf-8"` puro
# NAO faz isso: o Python devolve a marca como o primeiro caractere da
# string (`\ufeff`), que nao casa `\s` (`'\ufeff'.isspace()` e' False),
# entao `^\s*#` de _INCLUDE_DIRECTIVE_PATTERN falhava bem no inicio do
# arquivo - falso negativo, o mesmo lado perigoso das duas familias
# anteriores. `encoding="utf-8-sig"` e' o codec padrao do Python pra
# exatamente isso: descarta a marca se ela existir no INICIO da
# decodificacao, e e' identico a `"utf-8"` quando ela nao existe -
# nunca precisa de `if` proprio. **Uma marca no MEIO do arquivo (nao
# no inicio) e' erro de compilacao de verdade nos dois compiladores**
# ("stray '#' in program" / "unexpected character <U+FEFF>"), entao
# so' a do inicio precisa de tratamento - modelar mais que isso seria
# consertar codigo que ja nao compila.
#
# LIMITACAO DECLARADA, medida em 23/09/2026 e NAO consertada aqui
# (fora do escopo desta fatia, orientacao do orquestrador): um arquivo
# em UTF-16 (com ou sem marca) e' INVISIVEL tanto pro portao antigo
# (substring, commit 87a778f) quanto pro novo - `encoding="utf-8"`/
# `"utf-8-sig"` decodificam UTF-16 como uma sequencia de caracteres
# intercalados com NUL (`#\x00i\x00n\x00c\x00l\x00u\x00d\x00e\x00...`),
# que nao casa nenhum padrao deste arquivo. Nao e' regressao desta
# fatia - e' lacuna pre-existente nas duas versoes, registrada aqui
# para o orquestrador abrir item proprio se decidir que vale consertar
# (o compilador da Microsoft aceita fonte em UTF-16; g++/clang++ desta
# maquina nao, sem opcao extra - nao ha como confirmar contra os dois
# compiladores do resto deste arquivo).
def violations_in_file(path):
    """Cada item de `violations` e (kind, path, lineno). kind="needle" e
    o caso original (o argumento resolvido bate com uma agulha
    proibida); kind="computed" e' o caso novo do COMPUTED-INCLUDE acima
    (a diretiva #include/%:include nao pode ser verificada e reprova
    por si so', sem precisar bater com agulha nenhuma).
    """
    violations = []
    try:
        with open(path, "r", encoding="utf-8-sig", errors="replace") as handle:
            text = handle.read()
    except OSError as exc:
        print(f"{SCRIPT_NAME}: {path}: open refused ({exc})", file=sys.stderr)
        return violations
    resolved, unresolved = _scan_directives(text)
    for lineno, argument in resolved:
        if _FORBIDDEN_PATTERN.search(argument):
            violations.append(("needle", path, lineno))
    for lineno in unresolved:
        violations.append(("computed", path, lineno))
    return violations


# GODS_LAWS.md L-40 (piso de varredura nao-vazia): zero files found
# under src/core/ or include/glintfx/core/ is not "nothing to report",
# and reproves.
def require_nonempty_scan(file_count):
    if file_count == 0:
        print(
            f"{SCRIPT_NAME}: varredura vazia (0 arquivos em src/core ou "
            "include/glintfx/core) - GODS_LAWS.md L-40",
            file=sys.stderr,
        )
        return False
    return True


# --- gfss/gfui additions (LAYERS-GATE-GFSS-GFUI) ----------------------
#
# Unlike core_source_dirs()/require_nonempty_scan() above, whose floor
# is on the AGGREGATE file count across src/core/ and
# include/glintfx/core/ combined, each of these four directories gets
# its OWN floor, checked separately: a layer split across
# src/<name>/ and include/glintfx/<name>/ can have its files
# concentrated almost entirely in one of the two (measured against the
# real tree on 22/09/2026: include/glintfx/gfui/ has exactly ONE
# public header, node_view.hpp, against dozens under src/gfui/) - an
# AGGREGATE check would let a directory that silently lost every file
# hide behind the other directory's count. GODS_LAWS.md L-40: absence
# is declared and counted, never a quiet skip - so a missing directory
# and an existing-but-empty one are both failures, reported by name.
GFSS_GFUI_DIR_SPECS = (
    ("src/gfss", ("src", "gfss")),
    ("src/gfui", ("src", "gfui")),
    ("include/glintfx/gfss", ("include", "glintfx", "gfss")),
    ("include/glintfx/gfui", ("include", "glintfx", "gfui")),
)


def gfss_gfui_dir_files(root):
    """Returns {label: (files, exists)} for each of the four directories
    in GFSS_GFUI_DIR_SPECS. `exists` is False when the directory itself
    is missing (distinct from existing-but-empty; both fail the floor
    below, but the message says which case it is)."""
    result = {}
    for label, parts in GFSS_GFUI_DIR_SPECS:
        path = os.path.join(root, *parts)
        if not os.path.isdir(path):
            result[label] = ([], False)
            continue
        result[label] = (_walk_header_files(path), True)
    return result


def require_nonempty_gfss_gfui_dirs(dir_files):
    ok = True
    for label, (files, exists) in dir_files.items():
        if not exists:
            print(
                f"{SCRIPT_NAME}: varredura vazia ({label} nao existe) - "
                "GODS_LAWS.md L-40",
                file=sys.stderr,
            )
            ok = False
        elif len(files) == 0:
            print(
                f"{SCRIPT_NAME}: varredura vazia (0 arquivos em {label}) - "
                "GODS_LAWS.md L-40",
                file=sys.stderr,
            )
            ok = False
    return ok


# SYMLINK-DIR-REFUSED (LAYERS-GATE-GFSS-GFUI, sub-fatia L-3, lacuna
# antiga declarada pelo revisor de 23/09/2026: "diretorio alcancado por
# ligacao simbolica, que a varredura nao segue"). `os.walk()`, no modo
# padrao (`followlinks=False`, o que `_walk_header_files()` sempre
# usou), lista o NOME de um subdiretorio-ligacao em `dirnames` mas
# NUNCA entra nele como um `dirpath` de uma proxima iteracao - o
# conteudo por tras da ligacao fica INVISIVEL pra `_walk_header_files()`
# hoje: nem os arquivos de la contam pro piso nao-vazio, nem sao
# varridos por violacao. Um cabecalho de SO plantado atras de uma
# ligacao simbolica dentro de uma camada "pura" passaria como se a
# camada estivesse limpa - falso negativo silencioso, sem nenhum aviso.
#
# Duas saidas possiveis (o time pediu pra escolher a mais completa e
# justificar): (a) SEGUIR a ligacao (`followlinks=True`) - varreria o
# conteudo de verdade, mas troca um blind spot por dois riscos novos:
# uma ligacao apontando pra um ANCESTRAL do proprio diretorio cria um
# CICLO que faz `os.walk()` nunca terminar (GODS_LAWS.md L-11, proibido
# desenho que gasta tempo/processo sem teto por item varrido), e uma
# ligacao pode apontar pra QUALQUER lugar do sistema de arquivos,
# inclusive fora da arvore deste projeto inteiramente - a promessa "so
# varro estes seis diretorios" deixaria de valer sem ninguem decidir
# isso explicitamente. (b) REPROVAR a MERA PRESENCA da ligacao, nomeada,
# sem seguir. Escolhida (b): GODS_LAWS.md L-02 (fechado por padrao) - a
# lacuna de hoje e' invisibilidade TOTAL (0 violacoes, sem aviso nenhum
# nem pro operador saber que uma ligacao existe ali); uma camada que
# este portao promete pura nao ganha uma saida nao-declarada por
# ligacao simbolica. Reprovar e' estritamente MAIS protetor que o
# comportamento de hoje (silencio) mesmo nao sendo mais "completo" no
# sentido de escanear o conteudo de la - a decisao de abrir excecao pra
# um caso real fica com o lider (GODS_LAWS.md L-01), nao com este
# portao sozinho.
def _find_symlinked_directories(root_dir):
    """Retorna a lista ordenada de subdiretorios de `root_dir` (o
    proprio `root_dir` incluido) alcancados por ligacao simbolica.
    `root_dir` inexistente devolve lista vazia (quem chama ja trata
    ausencia via require_nonempty_scan()/require_nonempty_gfss_gfui_
    dirs() antes desta funcao rodar)."""
    if not os.path.isdir(root_dir):
        return []
    if os.path.islink(root_dir):
        return [root_dir]
    found = []
    for dirpath, dirnames, _filenames in os.walk(root_dir):
        for name in dirnames:
            candidate = os.path.join(dirpath, name)
            if os.path.islink(candidate):
                found.append(candidate)
    return sorted(found)


def require_no_symlinked_layer_dirs(root):
    ok = True
    for source_dir in core_source_dirs(root):
        for linked in _find_symlinked_directories(source_dir):
            print(
                f"{SCRIPT_NAME}: ligacao simbolica dentro de camada pura "
                f"(nao seguida): {linked} - GODS_LAWS.md L-02",
                file=sys.stderr,
            )
            ok = False
    for _label, parts in GFSS_GFUI_DIR_SPECS:
        path = os.path.join(root, *parts)
        for linked in _find_symlinked_directories(path):
            print(
                f"{SCRIPT_NAME}: ligacao simbolica dentro de camada pura "
                f"(nao seguida): {linked} - GODS_LAWS.md L-02",
                file=sys.stderr,
            )
            ok = False
    return ok


# The actual gate logic, factored out of real_main() so --selftest
# exercises the EXACT same function - not a reimplementation that
# could drift from production.
def check_layers(root):
    core_files = core_source_files(root)
    if not require_nonempty_scan(len(core_files)):
        return False

    gfss_gfui_files = gfss_gfui_dir_files(root)
    if not require_nonempty_gfss_gfui_dirs(gfss_gfui_files):
        return False

    if not require_no_symlinked_layer_dirs(root):
        return False

    files = list(core_files)
    for dir_file_list, _exists in gfss_gfui_files.values():
        files.extend(dir_file_list)
    file_count = len(files)

    violations = []
    for f in files:
        violations.extend(violations_in_file(f))

    if violations:
        print(f"{SCRIPT_NAME}: layer violations (GODS_LAWS.md L-19):", file=sys.stderr)
        for kind, path, lineno in violations:
            if kind == "computed":
                print(
                    f"{path}:{lineno}: inclusao computada nao verificavel "
                    "(GODS_LAWS.md L-02)",
                    file=sys.stderr,
                )
            else:
                print(f"{path}:{lineno}", file=sys.stderr)
        return False

    print(f"{SCRIPT_NAME}: violations: 0 in {file_count} files scanned")
    return True


# --- real mode -------------------------------------------------------


def real_main(args):
    if len(args) != 1:
        fail("usage: check_layers.py <source-root-directory>")
    root = args[0]
    if not os.path.isdir(root):
        fail(f"directory not found: {root}")
    if not check_layers(root):
        fail("layer violation found (GODS_LAWS.md L-19; see message above)")


# --- fixtures and controls for --selftest -----------------------------

# FIXTURE-WRITE-CRLF-DOUBLING (LAYERS-GATE-GFSS-GFUI, sub-fatia L-3,
# 23/09/2026, vermelho do servidor real - run 35845851878, os tres
# trabalhos Windows do CI reprovaram em selftest_crlf_control() com
# "case_splice_crlf deveria ter reprovado, mas passou" e o gemeo do
# outro caso invertido): TODA gravacao de fixture abaixo abria com
# `open(path, "w", encoding="utf-8")` - modo texto SEM `newline=""`.
# O modulo `io` do Python, em modo texto de escrita com `newline=None`
# (o default), traduz cada caractere `\n` da string para `os.linesep`
# ANTES de gravar, e faz isso incondicionalmente - nao verifica se ja
# havia um `\r` logo antes. No Windows, `os.linesep` e `\r\n`; um
# conteudo que ja carrega `\r\n` literal (as quatro fixtures de
# _CRLF_DIRECTIVE_CASES, unicas com isso) virava `\r` (inalterado) +
# `\r\n` (o `\n` traduzido) = `\r\r\n` em disco - TRIPLICADO, nao o
# `\r\n` unico que um checkout git real produz. Reproduzido com o
# CODIGO DE PRODUCAO desta mesma funcao (nao uma reimplementacao),
# gravando os bytes que uma escrita em modo texto do Windows produziria
# e chamando check_layers() puro: o controle inverte exatamente como no
# servidor (`check_layers() == True` quando deveria reprovar).
#
# Por que a duplicacao quebra justo o caso do SPLICE: a leitura de
# violations_in_file() usa modo texto universal (`newline=None` no
# `open("r", ...)`, ver o comentario dali) - toda sequencia `\r\n`,
# `\r` ou `\n` vira `\n` ANTES do texto chegar em
# _translate_phases_2_and_3(). Um `\r\r\n` em disco e' lido como `\r`
# sozinho (nao seguido de `\n` de imediato) -> um `\n`, e depois o
# `\r\n` seguinte -> outro `\n`: DOIS `\n` para cada UM que devia
# existir. No caso_splice_crlf isso insere uma linha logica extra
# exatamente entre `#include \` e `<fstream>` depois da fase 2 emendar
# a barra invertida com o primeiro `\n` - o `<fstream>` sobrevivente
# fica isolado na SUA PROPRIA linha logica, sem `#include` na frente, e
# o portao nao acha diretiva nenhuma para examinar: falso negativo.
#
# Conserto: `newline=""` em TODAS as nove gravacoes de fixture deste
# arquivo (L-17, "isolado ou padrao" - a duplicacao so se MANIFESTA nos
# quatro casos com `\r\n` literal, mas a causa e generica a qualquer
# `open(..., "w", encoding="utf-8")` sem `newline`, entao o conserto e
# generico tambem, nao so nas quatro gravacoes de CRLF). `newline=""`
# desliga a traducao de fim de linha por completo - contrato documentado
# do modulo `io`, nao comportamento medido so nesta maquina -, e por
# isso o byte gravado e sempre `conteudo.encode("utf-8")` exato,
# QUALQUER que seja o SO: confirmado abaixo (nao so citado da doc) que,
# com o conserto, o arquivo em disco bate byte a byte com
# `content.encode("utf-8")`, sem nenhuma substituicao dependente de
# `os.linesep` - o mesmo resultado, byte a byte, que uma gravacao real
# no Windows produziria.
#
# Lado da LEITURA, conferido e DEIXADO como esta (nao e o defeito): um
# `\r` isolado (nao seguido de `\n`) DENTRO de uma diretiva de inclusao
# foi testado contra g++ 16.2.1 e clang++ 22.1.8 nesta fatia
# (`#include\r<fstream>\n`, arquivo `lonecr_in_directive.cpp` de
# verificacao) - os DOIS compiladores tratam o `\r` isolado como
# terminador de linha, identico a `\n`: `#include` sozinho reprova por
# faltar argumento, e `<fstream>` sobra intacto na proxima linha, nunca
# expandido. Isso e EXATAMENTE o que o modo texto universal do Python
# ja faz na leitura (`\r` isolado -> `\n`), entao `violations_in_file()`
# concorda com os dois compiladores sem precisar de `newline=""` do
# lado de leitura - so o lado de ESCRITA das fixtures tinha o defeito.
def make_scratch_workdir():
    # A hand written Unix path does not exist on every platform (Windows
    # has no /tmp). dir=os.environ.get("TMPDIR") without a hardcoded
    # fallback lets tempfile.mkdtemp fall through to gettempdir(), which
    # already checks TMPDIR/TEMP/TMP and then the platform default.
    return tempfile.mkdtemp(prefix="glintfx-layers-selftest-", dir=os.environ.get("TMPDIR"))


def _write_clean_file(path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="") as handle:
        handle.write("#include <cstdint>\n// clean layer file, no OS or upper-layer header\n")


def make_gfss_gfui_clean_fixture(root):
    for _label, parts in GFSS_GFUI_DIR_SPECS:
        _write_clean_file(os.path.join(root, *parts, "clean.hpp"))


def make_clean_fixture(root):
    _write_clean_file(os.path.join(root, "src", "core", "clean.cpp"))
    make_gfss_gfui_clean_fixture(root)


def _make_capture():
    import contextlib
    import io

    class _Captured:
        __slots__ = ("result", "text")

        def __init__(self, result, text):
            self.result = result
            self.text = text

    def capture(fn):
        buffer = io.StringIO()
        with contextlib.redirect_stdout(buffer), contextlib.redirect_stderr(buffer):
            result = fn()
        return _Captured(result, buffer.getvalue())

    return capture


# Positive control: clean fixture. Expected: passes.
def selftest_positive_control(scratch, capture):
    root = os.path.join(scratch, "positive")
    make_clean_fixture(root)

    outcome = capture(lambda: check_layers(root))
    if outcome.result:
        print("selftest: controle POSITIVO OK (fixture limpa aprovada)")
        return True
    print(
        "selftest: controle POSITIVO FALHOU (fixture limpa deveria ter sido aprovada)",
        file=sys.stderr,
    )
    print(outcome.text, file=sys.stderr)
    return False


# Negative control: plants a forbidden OS header inside
# include/glintfx/core/. Expected: reproves and cites the planted file.
def selftest_negative_control(scratch, capture):
    root = os.path.join(scratch, "negative")
    make_clean_fixture(root)
    target_dir = os.path.join(root, "include", "glintfx", "core")
    os.makedirs(target_dir, exist_ok=True)
    target = os.path.join(target_dir, "dirty.hpp")
    with open(target, "w", encoding="utf-8", newline="") as handle:
        handle.write("#include <wayland-client.h>\n")

    outcome = capture(lambda: check_layers(root))
    if outcome.result:
        print(
            "selftest: controle NEGATIVO FALHOU (header do SO em "
            "include/glintfx/core/ nao foi pego)",
            file=sys.stderr,
        )
        return False
    if target not in outcome.text:
        print(
            f"selftest: controle NEGATIVO FALHOU (reprovou, mas nao citou {target})",
            file=sys.stderr,
        )
        print(outcome.text, file=sys.stderr)
        return False
    print("selftest: controle NEGATIVO OK (header do SO em include/glintfx/core/ pego e citado)")
    return True


# Second negative control (ASSET-LOAD conserto, 28/08/2026): plants
# <fstream> - a file-I/O header, not a Wayland/GL/POSIX one - inside
# src/core/. Expected: reproves and cites the planted file. Separate
# function, separate fixture, on purpose (GODS_LAWS.md L-40 "enumeracao
# fechada por construcao"): the ORIGINAL negative control above only
# ever exercises the wayland-client.h branch of OS_HEADER_NEEDLES, so a
# regression that broke JUST the <filesystem>/<fstream> entries would
# pass every other control silently - this is the control that closes
# that gap specifically.
def selftest_negative_control_file_header(scratch, capture):
    root = os.path.join(scratch, "negative_file_header")
    make_clean_fixture(root)
    target = os.path.join(root, "src", "core", "dirty_file_io.cpp")
    with open(target, "w", encoding="utf-8", newline="") as handle:
        handle.write("#include <fstream>\n")

    outcome = capture(lambda: check_layers(root))
    if outcome.result:
        print(
            "selftest: controle NEGATIVO (header de arquivo) FALHOU "
            "(<fstream> em src/core/ nao foi pego)",
            file=sys.stderr,
        )
        return False
    if target not in outcome.text:
        print(
            "selftest: controle NEGATIVO (header de arquivo) FALHOU "
            f"(reprovou, mas nao citou {target})",
            file=sys.stderr,
        )
        print(outcome.text, file=sys.stderr)
        return False
    print(
        "selftest: controle NEGATIVO (header de arquivo) OK "
        "(<fstream> em src/core/ pego e citado)"
    )
    return True


# Empty-scan floor: neither src/core/ nor include/glintfx/core/ exists.
# Expected: reproves with "varredura vazia" in the message.
def selftest_empty_scan_control(scratch, capture):
    root = os.path.join(scratch, "empty")
    os.makedirs(root, exist_ok=True)

    outcome = capture(lambda: check_layers(root))
    if outcome.result:
        print(
            "selftest: controle de VARREDURA VAZIA FALHOU (raiz sem "
            "src/core nem include/glintfx/core deveria ter sido "
            "recusada, mas passou)",
            file=sys.stderr,
        )
        print(outcome.text, file=sys.stderr)
        return False
    if "varredura vazia" not in outcome.text:
        print(
            "selftest: controle de VARREDURA VAZIA FALHOU (recusou, mas "
            "nao disse 'varredura vazia')",
            file=sys.stderr,
        )
        print(outcome.text, file=sys.stderr)
        return False
    print("selftest: controle de VARREDURA VAZIA OK (raiz sem diretorio de core recusada)")
    return True


# LAYERS-GATE-GFSS-GFUI control 1: a forbidden OS header planted in
# EACH of the four gfss/gfui directories in turn (own fixture per
# directory, so one broken branch cannot hide behind another passing).
# Expected: reproves and cites the planted file, for all four.
def selftest_negative_control_gfss_gfui(scratch, capture):
    ok = True
    for label, parts in GFSS_GFUI_DIR_SPECS:
        safe_label = label.replace("/", "_")
        root = os.path.join(scratch, f"negative_gfss_gfui_{safe_label}")
        make_clean_fixture(root)
        target = os.path.join(root, *parts, "dirty.hpp")
        with open(target, "w", encoding="utf-8", newline="") as handle:
            handle.write("#include <wayland-client.h>\n")

        outcome = capture(lambda: check_layers(root))
        if outcome.result:
            print(
                f"selftest: controle NEGATIVO ({label}) FALHOU (header do SO "
                "nao foi pego)",
                file=sys.stderr,
            )
            ok = False
            continue
        if target not in outcome.text:
            print(
                f"selftest: controle NEGATIVO ({label}) FALHOU (reprovou, mas "
                f"nao citou {target})",
                file=sys.stderr,
            )
            print(outcome.text, file=sys.stderr)
            ok = False
            continue
        print(f"selftest: controle NEGATIVO ({label}) OK (header do SO pego e citado)")
    return ok


# LAYERS-GATE-GFSS-GFUI control 2: the PER-DIRECTORY floor. For each of
# the four directories in turn, build a fixture where the OTHER three
# are populated and clean, and THIS one is present but empty (zero
# matching files). Expected: reproves, naming exactly this directory -
# proving the floor is per-directory, not an aggregate that the other
# three's files could mask.
def selftest_gfss_gfui_per_directory_floor(scratch, capture):
    ok = True
    for label, parts in GFSS_GFUI_DIR_SPECS:
        safe_label = label.replace("/", "_")
        root = os.path.join(scratch, f"floor_gfss_gfui_{safe_label}")
        make_clean_fixture(root)
        empty_dir = os.path.join(root, *parts)
        # empty it back out: make_clean_fixture already wrote clean.hpp
        # into it above, remove just that one file so the directory
        # exists but is empty, while the other three keep theirs.
        os.remove(os.path.join(empty_dir, "clean.hpp"))

        outcome = capture(lambda: check_layers(root))
        if outcome.result:
            print(
                f"selftest: controle de PISO POR DIRETORIO ({label}) FALHOU "
                "(diretorio vazio deveria ter sido recusado, mas passou)",
                file=sys.stderr,
            )
            print(outcome.text, file=sys.stderr)
            ok = False
            continue
        if f"0 arquivos em {label}" not in outcome.text:
            print(
                f"selftest: controle de PISO POR DIRETORIO ({label}) FALHOU "
                f"(recusou, mas nao citou '0 arquivos em {label}')",
                file=sys.stderr,
            )
            print(outcome.text, file=sys.stderr)
            ok = False
            continue
        print(
            f"selftest: controle de PISO POR DIRETORIO ({label}) OK "
            "(diretorio vazio recusado nominalmente, sem mascaramento pelos outros tres)"
        )
    return ok


# ANCHOR-ON-DIRECTIVE control (ordem do lider, 22/09/2026: "Olhar so
# diretivas de inclusao"): the needle search only fires inside a line
# that IS an #include or C++23 header-unit import directive - never
# inside a comment that merely mentions one of the needle substrings
# in prose (the false positive that started this: anb_parse.cpp:159's
# own "a GL/WGL function name is"), and never missed just because the
# real include uses C++23 `import <header>;` instead of `#include
# <header>` (a naive #include-only anchor would have opened exactly
# that hole - a real forbidden import passing silently, worse than the
# false positive it fixes). Each case below is planted, alone, into an
# otherwise-clean fixture; `True` means the gate must reprove and cite
# the planted file, `False` means it must pass clean.
_ANCHOR_DIRECTIVE_CASES = (
    ("#include <fstream>\n", True),
    ("#  include <GL/gl.h>\n", True),
    ("#include<windows.h>\n", True),
    ("import <fstream>;\n", True),
    ("export import <fstream>;\n", True),
    ("// a GL/WGL function name is a mouthful\n", False),
    ("// #include <windows.h>\n", False),
    # DIGRAPH (LAYERS-GATE-GFSS-GFUI, sub-fatia L-3, regressao (b) - ver
    # o comentario DIGRAPH acima de _INCLUDE_DIRECTIVE_PATTERN): as tres
    # formas confirmadas contra g++ 16.2.1 e clang++ 22.1.8, mais a
    # forma com espaco DENTRO do digrafo que os dois compiladores NAO
    # reconhecem como diretiva nenhuma (ecoada como texto comum).
    ("%:include <fstream>\n", True),
    ("%:  include <fstream>\n", True),
    ("% : include <fstream>\n", False),
)


def selftest_anchor_directive_control(scratch, capture):
    ok = True
    for index, (planted_line, should_reprove) in enumerate(_ANCHOR_DIRECTIVE_CASES):
        root = os.path.join(scratch, f"anchor_{index}")
        make_clean_fixture(root)
        target = os.path.join(root, "src", "core", f"anchor_case_{index}.cpp")
        with open(target, "w", encoding="utf-8", newline="") as handle:
            handle.write(planted_line)

        outcome = capture(lambda: check_layers(root))
        reproved = not outcome.result
        label = repr(planted_line.rstrip("\n"))

        if should_reprove and not reproved:
            print(
                f"selftest: controle de ANCORA FALHOU ({label} deveria ter "
                "reprovado, mas passou)",
                file=sys.stderr,
            )
            ok = False
            continue
        if should_reprove and target not in outcome.text:
            print(
                f"selftest: controle de ANCORA FALHOU ({label} reprovou, mas "
                f"nao citou {target})",
                file=sys.stderr,
            )
            print(outcome.text, file=sys.stderr)
            ok = False
            continue
        if not should_reprove and reproved:
            print(
                f"selftest: controle de ANCORA FALHOU ({label} deveria ter "
                "passado (nao e diretiva de inclusao), mas reprovou)",
                file=sys.stderr,
            )
            print(outcome.text, file=sys.stderr)
            ok = False
            continue

        verdict = "reprovado" if should_reprove else "passou"
        print(f"selftest: controle de ANCORA OK ({label} {verdict} como esperado)")
    return ok


# PHASE-2-3 control (re-verificacao do lider, 22/09/2026): os cinco
# casos abaixo, cada um confirmado contra `g++ -std=c++23
# -fsyntax-only` antes de virar controle (ver o comentario PHASE-2-3
# no topo do arquivo). Cada entrada e o CONTEUDO MULTI-LINHA completo
# plantado num arquivo novo de src/core/ - nao uma unica linha, porque
# o proprio fenomeno sob teste (emenda de linha, comentario
# multi-linha) so existe atravessando mais de uma linha fisica.
_PHASE23_DIRECTIVE_CASES = (
    ("case_a_splice_before_bracket", "#include \\\n<fstream>\n", True),
    ("case_b_block_comment_mid_directive", "#include /* nada */ <fstream>\n", True),
    (
        "case_c_directive_inside_block_comment",
        "/*\n#include <windows.h>\n*/\n",
        False,
    ),
    (
        "case_d_quote_does_not_open_comment",
        'const char* s = "/*";\n#include <fstream>\n// */\n',
        True,
    ),
    (
        "case_e_line_comment_swallows_spliced_continuation",
        "// comentario \\\n#include <fstream>\n",
        False,
    ),
)


def selftest_phase23_directive_control(scratch, capture):
    ok = True
    for name, content, should_reprove in _PHASE23_DIRECTIVE_CASES:
        root = os.path.join(scratch, f"phase23_{name}")
        make_clean_fixture(root)
        target = os.path.join(root, "src", "core", f"{name}.cpp")
        with open(target, "w", encoding="utf-8", newline="") as handle:
            handle.write(content)

        outcome = capture(lambda: check_layers(root))
        reproved = not outcome.result

        if should_reprove and not reproved:
            print(
                f"selftest: controle FASE-2-3 FALHOU ({name} deveria ter "
                "reprovado, mas passou)",
                file=sys.stderr,
            )
            ok = False
            continue
        if should_reprove and target not in outcome.text:
            print(
                f"selftest: controle FASE-2-3 FALHOU ({name} reprovou, mas "
                f"nao citou {target})",
                file=sys.stderr,
            )
            print(outcome.text, file=sys.stderr)
            ok = False
            continue
        if not should_reprove and reproved:
            print(
                f"selftest: controle FASE-2-3 FALHOU ({name} deveria ter "
                "passado, mas reprovou)",
                file=sys.stderr,
            )
            print(outcome.text, file=sys.stderr)
            ok = False
            continue

        verdict = "reprovado" if should_reprove else "passou"
        print(f"selftest: controle FASE-2-3 OK ({name} {verdict} como esperado)")
    return ok


# RAW-STRING control (LAYERS-GATE-GFSS-GFUI, sub-fatia L-2, 22/09/2026,
# absorve o item de INBOX PORTAO-DE-CAMADA-NAO-CONHECE-CADEIA-BRUTA):
# nove casos, cada um confirmado contra `g++ -std=c++23 -fsyntax-only`
# (g++ 16.2.1) E `clang++ -std=c++23 -fsyntax-only` (clang++ 22.1.8)
# antes de virar controle - GODS_LAWS.md L-22, "o compilador vem antes
# da minha tabela", igual ao PHASE-2-3 acima:
#   a. `R"(foo " bar /* baz)";` seguido de `#include <fstream>` numa
#      linha propria: a aspas e o `/*` DENTRO da cadeia bruta nao abrem
#      string nem comentario nenhum - a cadeia fecha em `)"` mesmo, e o
#      #include depois dela E uma diretiva de verdade. Os dois
#      compiladores incluem <fstream> (`grep -c basic_fstream` no `-E`
#      > 0); e e exatamente o falso negativo que este item promete
#      fechar (o portao de HOJE, sem esta fatia, passa limpo aqui -
#      prova de estreia em selftest_raw_string_control() mais abaixo).
#   b. `#include <fstream>` DENTRO do conteudo de uma cadeia bruta
#      multi-linha: nao e diretiva nenhuma, e' dado. <fstream> nao
#      aparece no `-E` dos dois compiladores.
#   c. delimitador diferente no fecho: abre com `R"y(`, tem um `)x"`
#      no meio (nao fecha, delimitador errado) e um `#include
#      <fstream>` depois dele, fechando de verdade so em `)y"`.
#      <fstream> nao aparece - o #include fica dentro da cadeia bruta
#      inteira, nunca vira codigo.
#   d-g. os quatro prefixos de codificacao (`u8R`, `uR`, `UR`, `LR`) -
#      cada um, com o mesmo fixture do caso (a), tambem inclui
#      <fstream> nos dois compiladores (o erro de conversao de tipo
#      que `u8R`/`uR`/`UR`/`LR` produzem contra `const char*` e so
#      sobre o TIPO da string - a cadeia bruta em si foi reconhecida
#      igual a forma sem prefixo, confirmado porque o #include depois
#      dela aparece no `-E` de qualquer forma).
#   h. delimitador de 16 caracteres (o teto - um de 17 e erro real de
#      compilacao, "delimitador de string nao tratada (raw) maior do
#      que 16 caracteres", g++ 16.2.1): mesmo fixture do caso (a), com
#      `R"ABCDEFGHIJKLMNOP(...)ABCDEFGHIJKLMNOP"` - <fstream> aparece.
#   i. `case_backslash_newline_not_spliced_inside_raw_string`: ver o
#      docstring de _translate_phases_2_and_3() acima para o fixture
#      completo e por que ele so passa quando a fase 2 e' de fato
#      isenta dentro do corpo da cadeia bruta.
_RAW_STRING_CASES = (
    (
        "case_a_fake_comment_hides_real_include",
        'const char* s = R"(foo " bar /* baz)";\n#include <fstream>\n',
        True,
    ),
    (
        "case_b_include_inside_raw_string_is_inert",
        'const char* s = R"(\n#include <fstream>\n)";\n',
        False,
    ),
    (
        "case_c_mismatched_delimiter_does_not_close",
        'const char* s = R"y(before )x" middle\n#include <fstream>\nafter )y";\n',
        False,
    ),
    (
        "case_d_prefix_u8R",
        'const char8_t* s = u8R"(foo " bar /* baz)";\n#include <fstream>\n',
        True,
    ),
    (
        "case_e_prefix_uR",
        'const char16_t* s = uR"(foo " bar /* baz)";\n#include <fstream>\n',
        True,
    ),
    (
        "case_f_prefix_UR",
        'const char32_t* s = UR"(foo " bar /* baz)";\n#include <fstream>\n',
        True,
    ),
    (
        "case_g_prefix_LR",
        'const wchar_t* s = LR"(foo " bar /* baz)";\n#include <fstream>\n',
        True,
    ),
    (
        "case_h_delimiter_max_length_16",
        'const char* s = R"ABCDEFGHIJKLMNOP(foo " bar /* baz)ABCDEFGHIJKLMNOP";\n'
        "#include <fstream>\n",
        True,
    ),
    (
        "case_i_backslash_newline_not_spliced_inside_raw_string",
        'const char* s = R"END(before\n)EN\\\nD"\n#include <fstream>\nafter\n)END";\n',
        False,
    ),
)


def selftest_raw_string_control(scratch, capture):
    ok = True
    for name, content, should_reprove in _RAW_STRING_CASES:
        root = os.path.join(scratch, f"rawstring_{name}")
        make_clean_fixture(root)
        target = os.path.join(root, "src", "core", f"{name}.cpp")
        with open(target, "w", encoding="utf-8", newline="") as handle:
            handle.write(content)

        outcome = capture(lambda: check_layers(root))
        reproved = not outcome.result

        if should_reprove and not reproved:
            print(
                f"selftest: controle de CADEIA BRUTA FALHOU ({name} deveria ter "
                "reprovado, mas passou)",
                file=sys.stderr,
            )
            ok = False
            continue
        if should_reprove and target not in outcome.text:
            print(
                f"selftest: controle de CADEIA BRUTA FALHOU ({name} reprovou, "
                f"mas nao citou {target})",
                file=sys.stderr,
            )
            print(outcome.text, file=sys.stderr)
            ok = False
            continue
        if not should_reprove and reproved:
            print(
                f"selftest: controle de CADEIA BRUTA FALHOU ({name} deveria ter "
                "passado, mas reprovou)",
                file=sys.stderr,
            )
            print(outcome.text, file=sys.stderr)
            ok = False
            continue

        verdict = "reprovado" if should_reprove else "passou"
        print(f"selftest: controle de CADEIA BRUTA OK ({name} {verdict} como esperado)")
    return ok


# CRLF control (GATE-ENV-SWEEP, categoria LINE_ENDING - ver a
# declaracao por perto de _RAW_STRING_FORBIDDEN_DELIMITER_CHARS acima
# para a razao de cada caso): os MESMOS quatro fixtures que provaram
# splice/comentario/cadeia-bruta com `\n` acima, reescritos byte a
# byte com `\r\n` (nunca so' o rotulo "CRLF" - o conteudo de fato
# muda), confirmados contra g++ 16.2.1 e clang++ 22.1.8 antes de virar
# controle (GODS_LAWS.md L-22).
_CRLF_DIRECTIVE_CASES = (
    (
        "case_splice_crlf",
        'const char* s = "x";\r\n#include \\\r\n<fstream>\r\nint main(){return 0;}\r\n',
        True,
    ),
    (
        "case_comment_swallows_crlf_splice",
        "// comentario \\\r\n#include <fstream>\r\nint main(){return 0;}\r\n",
        False,
    ),
    (
        "case_raw_string_spans_crlf_lines",
        'const char* s = R"(\r\n#include <fstream>\r\n)";\r\nint main(){return 0;}\r\n',
        False,
    ),
    (
        "case_fake_comment_hides_include_crlf",
        'const char* s = R"(foo " bar /* baz)";\r\n#include <fstream>\r\nint main(){return 0;}\r\n',
        True,
    ),
)


def selftest_crlf_control(scratch, capture):
    ok = True
    for name, content, should_reprove in _CRLF_DIRECTIVE_CASES:
        root = os.path.join(scratch, f"crlf_{name}")
        make_clean_fixture(root)
        target = os.path.join(root, "src", "core", f"{name}.cpp")
        with open(target, "w", encoding="utf-8", newline="") as handle:
            handle.write(content)

        outcome = capture(lambda: check_layers(root))
        reproved = not outcome.result

        if should_reprove and not reproved:
            print(
                f"selftest: controle de FIM DE LINHA FALHOU ({name} deveria ter "
                "reprovado, mas passou)",
                file=sys.stderr,
            )
            ok = False
            continue
        if should_reprove and target not in outcome.text:
            print(
                f"selftest: controle de FIM DE LINHA FALHOU ({name} reprovou, "
                f"mas nao citou {target})",
                file=sys.stderr,
            )
            print(outcome.text, file=sys.stderr)
            ok = False
            continue
        if not should_reprove and reproved:
            print(
                f"selftest: controle de FIM DE LINHA FALHOU ({name} deveria ter "
                "passado, mas reprovou)",
                file=sys.stderr,
            )
            print(outcome.text, file=sys.stderr)
            ok = False
            continue

        verdict = "reprovado" if should_reprove else "passou"
        print(f"selftest: controle de FIM DE LINHA OK ({name} {verdict} como esperado)")
    return ok


# BOM control (BOM-STRIP - ver a declaracao por perto de
# violations_in_file() acima para a razao de cada caso). Os dois casos
# que o orquestrador pediu, confirmados contra g++ 16.2.1 e clang++
# 22.1.8 antes de virarem controle (GODS_LAWS.md L-22): marca no
# inicio do arquivo, seguida de um #include de verdade, reprova; marca
# no inicio seguida so' de prosa passa. `\ufeff` no INICIO da string
# de conteudo vira, ao escrever com `encoding="utf-8"`, os tres bytes
# `EF BB BF` no inicio do arquivo real - a mesma marca que o Visual
# Studio grava.
_BOM_CASES = (
    ("case_bom_start_then_real_include", "\ufeff#include <fstream>\n", True),
    ("case_bom_start_then_prose_only", "\ufeff// so prosa, sem diretiva\n", False),
)


def selftest_bom_control(scratch, capture):
    ok = True
    for name, content, should_reprove in _BOM_CASES:
        root = os.path.join(scratch, f"bom_{name}")
        make_clean_fixture(root)
        target = os.path.join(root, "src", "core", f"{name}.cpp")
        with open(target, "w", encoding="utf-8", newline="") as handle:
            handle.write(content)

        outcome = capture(lambda: check_layers(root))
        reproved = not outcome.result

        if should_reprove and not reproved:
            print(
                f"selftest: controle de MARCA UTF-8 FALHOU ({name} deveria ter "
                "reprovado, mas passou)",
                file=sys.stderr,
            )
            ok = False
            continue
        if should_reprove and target not in outcome.text:
            print(
                f"selftest: controle de MARCA UTF-8 FALHOU ({name} reprovou, "
                f"mas nao citou {target})",
                file=sys.stderr,
            )
            print(outcome.text, file=sys.stderr)
            ok = False
            continue
        if not should_reprove and reproved:
            print(
                f"selftest: controle de MARCA UTF-8 FALHOU ({name} deveria ter "
                "passado, mas reprovou)",
                file=sys.stderr,
            )
            print(outcome.text, file=sys.stderr)
            ok = False
            continue

        verdict = "reprovado" if should_reprove else "passou"
        print(f"selftest: controle de MARCA UTF-8 OK ({name} {verdict} como esperado)")
    return ok


# COMPUTED-INCLUDE control (LAYERS-GATE-GFSS-GFUI, sub-fatia L-3, ver o
# comentario COMPUTED-INCLUDE acima de _INCLUDE_KEYWORD_PATTERN). Cada
# caso confirmado contra g++ 16.2.1 e clang++ 22.1.8 antes de virar
# controle (GODS_LAWS.md L-22), EXCETO os dois marcados "nunca compila"
# abaixo, que sao erro de compilacao de verdade nos dois - o portao
# reprova mesmo assim (fechado por padrao: nao resolver e' o veredito
# seguro, nao uma tentativa de simular o compilador nesses dois).
# `expects_message` distingue reprovacao por AGULHA (a de sempre,
# "path:lineno") de reprovacao por COMPUTADA NAO VERIFICAVEL - as duas
# reprovam (outcome.result False), mas por motivos diferentes, e o
# controle confere qual das duas mensagens saiu.
_COMPUTED_INCLUDE_CASES = (
    (
        "case_a_object_macro_forbidden_resolves_and_bites",
        "#define HDR <fstream>\n#include HDR\n",
        True,
        "needle",
    ),
    (
        "case_b_object_macro_clean_resolves_and_passes",
        "#define HDR <cstdint>\n#include HDR\n",
        False,
        None,
    ),
    (
        # Corpo em aspas (nao so' <...>) - GODS_LAWS.md L-22 confirmou
        # contra g++ que `#define HDR "fstream"` + `#include HDR`
        # tambem resolve de verdade (cai na busca do sistema quando nao
        # acha local). Usa a agulha "wayland" (sem "<" embutido na
        # propria agulha), NAO "<fstream" - achado a parte, fora do
        # escopo desta fatia, registrado no relato ao time: seis das
        # onze agulhas de OS_HEADER_NEEDLES ("<fstream", "<filesystem",
        # "<dlfcn", "<unistd", "<sys/", "<fcntl") tem o `<` GRAVADO na
        # propria agulha, entao nunca mordem a forma "..." (aspas) do
        # mesmo cabecalho, computada ou direta - pre-existe a esta
        # fatia (nao criada por ela), e nao e uma das quatro frentes.
        "case_c_object_macro_quoted_body_resolves_and_bites",
        '#define HDR "wayland-client.h"\n#include HDR\n',
        True,
        "needle",
    ),
    (
        "case_d_digraph_plus_object_macro_resolves_and_bites",
        "#define HDR <fstream>\n%:include HDR\n",
        True,
        "needle",
    ),
    (
        "case_e_no_macro_defined_anywhere",
        "#include SOME_UNDEFINED_MACRO\n",
        True,
        "computed",
    ),
    (
        # Macro de FUNCAO: funciona no compilador real (confirmado),
        # mas este portao deliberadamente NUNCA resolve macro de
        # funcao - "HDR(1)" nao casa _BARE_MACRO_IDENTIFIER_PATTERN
        # (sobra "(1)"), entao cai direto em "computada", o veredito
        # seguro sem precisar modelar substituicao de parametro.
        "case_f_function_like_macro_never_resolved",
        "#define HDR(x) <fstream>\n#include HDR(1)\n",
        True,
        "computed",
    ),
    (
        # Corpo da macro NAO e' um cabecalho literal (nunca compilaria
        # de verdade tambem) - _OBJECT_MACRO_DEFINE_PATTERN exige que o
        # grupo 2 seja `<...>`/"..."; "42" nao bate, entao HDR nunca
        # entra na tabela de macros e o #include fica sem resolucao.
        "case_g_macro_body_not_a_literal_header_never_resolved",
        "#define HDR 42\n#include HDR\n",
        True,
        "computed",
    ),
    (
        # Import de MODULO NOMEADO, nao de header-unit - nao e' macro
        # nenhuma envolvida. Tem de passar limpo (decisao de escopo do
        # comentario `import` acima) - reprovar aqui seria falso
        # positivo contra sintaxe C++23 legitima.
        "case_h_named_module_import_is_not_computed_include",
        "import glintfx_demo_module;\n",
        False,
        None,
    ),
)


def selftest_computed_include_control(scratch, capture):
    ok = True
    for name, content, should_reprove, expected_kind in _COMPUTED_INCLUDE_CASES:
        root = os.path.join(scratch, f"computed_{name}")
        make_clean_fixture(root)
        target = os.path.join(root, "src", "core", f"{name}.cpp")
        with open(target, "w", encoding="utf-8", newline="") as handle:
            handle.write(content)

        outcome = capture(lambda: check_layers(root))
        reproved = not outcome.result

        if should_reprove and not reproved:
            print(
                f"selftest: controle de INCLUSAO COMPUTADA FALHOU ({name} "
                "deveria ter reprovado, mas passou)",
                file=sys.stderr,
            )
            ok = False
            continue
        if should_reprove and target not in outcome.text:
            print(
                f"selftest: controle de INCLUSAO COMPUTADA FALHOU ({name} "
                f"reprovou, mas nao citou {target})",
                file=sys.stderr,
            )
            print(outcome.text, file=sys.stderr)
            ok = False
            continue
        if should_reprove and expected_kind == "computed" and "computada" not in outcome.text:
            print(
                f"selftest: controle de INCLUSAO COMPUTADA FALHOU ({name} "
                "reprovou, mas nao pela mensagem de inclusao computada "
                "nao verificavel)",
                file=sys.stderr,
            )
            print(outcome.text, file=sys.stderr)
            ok = False
            continue
        if should_reprove and expected_kind == "needle" and "computada" in outcome.text:
            print(
                f"selftest: controle de INCLUSAO COMPUTADA FALHOU ({name} "
                "reprovou pela mensagem de computada, mas deveria ter sido "
                "pela agulha (macro resolvida)",
                file=sys.stderr,
            )
            print(outcome.text, file=sys.stderr)
            ok = False
            continue
        if not should_reprove and reproved:
            print(
                f"selftest: controle de INCLUSAO COMPUTADA FALHOU ({name} "
                "deveria ter passado, mas reprovou)",
                file=sys.stderr,
            )
            print(outcome.text, file=sys.stderr)
            ok = False
            continue

        verdict = "reprovado" if should_reprove else "passou"
        print(f"selftest: controle de INCLUSAO COMPUTADA OK ({name} {verdict} como esperado)")
    return ok


# NEEDLE-BITES control (LAYERS-GATE-GFSS-GFUI, sub-fatia L-3, buraco de
# prova do revisor de 23/09/2026: "das 12 palavras que o portao
# procura, 8 sobrevivem a remocao sem o --selftest notar - inclusive
# glintfx/platform/, a regra que da nome ao portao"). Antes desta
# fatia, so' quatro das doze agulhas (UPPER_LAYER_NEEDLE + as onze de
# OS_HEADER_NEEDLES) tinham algum controle que as plantava sozinhas e
# conferia a reprovacao: "wayland" (via wayland-client.h nos controles
# NEGATIVO e NEGATIVO-gfss/gfui), "GL/" e "windows.h" (via
# _ANCHOR_DIRECTIVE_CASES) e "<fstream" (onipresente). As outras oito -
# "glintfx/platform/", "winuser", "EGL/", "<dlfcn", "<unistd", "<sys/",
# "<fcntl", "<filesystem" - nunca apareciam plantadas em NENHUM
# fixture, entao remove-las de OS_HEADER_NEEDLES/UPPER_LAYER_NEEDLE nao
# fazia NENHUM controle reprovar - a logica funcionava, a prova nao
# existia. Este controle planta, isolado, um #include cujo argumento
# contem EXATAMENTE cada uma das doze agulhas (uma forma real de
# cabecalho, nao a substring sozinha) e confere reprovacao citando o
# arquivo - se uma agulha for removida de _FORBIDDEN_PATTERN, o caso
# correspondente para de bater e este controle FALHA, provando a
# mordida. Estreia vermelha verificada manualmente (L-36): comentar
# UPPER_LAYER_NEEDLE numa copia fora da arvore e rodar --selftest faz
# exatamente este controle (o caso "glintfx/platform/") reprovar - os
# outros dez continuam OK, confirmando que cada caso testa SO' a sua
# propria agulha, nenhum mascara o outro.
_NEEDLE_BITES_CASES = (
    ("upper_layer_needle_glintfx_platform", '#include "glintfx/platform/window.hpp"\n'),
    ("wayland", "#include <wayland-client.h>\n"),
    ("windows_h", "#include <windows.h>\n"),
    ("winuser", "#include <winuser.h>\n"),
    ("gl_slash", "#include <GL/gl.h>\n"),
    ("egl_slash", "#include <EGL/egl.h>\n"),
    ("dlfcn", "#include <dlfcn.h>\n"),
    ("unistd", "#include <unistd.h>\n"),
    ("sys_slash", "#include <sys/stat.h>\n"),
    ("fcntl", "#include <fcntl.h>\n"),
    ("filesystem", "#include <filesystem>\n"),
    ("fstream", "#include <fstream>\n"),
)


def selftest_needle_bites_control(scratch, capture):
    ok = True
    seen = set()
    for name, content in _NEEDLE_BITES_CASES:
        root = os.path.join(scratch, f"needle_{name}")
        make_clean_fixture(root)
        target = os.path.join(root, "src", "core", f"{name}.cpp")
        with open(target, "w", encoding="utf-8", newline="") as handle:
            handle.write(content)

        outcome = capture(lambda: check_layers(root))
        if outcome.result:
            print(
                f"selftest: controle de AGULHA MORDE ({name}) FALHOU "
                "(deveria ter reprovado, mas passou - agulha nao morde)",
                file=sys.stderr,
            )
            ok = False
            continue
        if target not in outcome.text:
            print(
                f"selftest: controle de AGULHA MORDE ({name}) FALHOU "
                f"(reprovou, mas nao citou {target})",
                file=sys.stderr,
            )
            print(outcome.text, file=sys.stderr)
            ok = False
            continue
        seen.add(name)
        print(f"selftest: controle de AGULHA MORDE ({name}) OK (agulha morde e cita o arquivo)")
    # GODS_LAWS.md L-40 (piso de varredura nao-vazia, aplicado a ESTE
    # controle tambem): _NEEDLE_BITES_CASES tem de cobrir as DOZE
    # agulhas reais, nunca um subconjunto que uma edicao futura
    # encolheu em silencio.
    expected_needle_count = len((UPPER_LAYER_NEEDLE,) + OS_HEADER_NEEDLES)
    if len(_NEEDLE_BITES_CASES) != expected_needle_count:
        print(
            "selftest: controle de AGULHA MORDE FALHOU (tabela de casos tem "
            f"{len(_NEEDLE_BITES_CASES)} entradas, mas ha {expected_needle_count} "
            "agulhas reais - alguma ficou sem prova)",
            file=sys.stderr,
        )
        ok = False
    return ok


# SYMLINK-DIR-REFUSED control (LAYERS-GATE-GFSS-GFUI, sub-fatia L-3, ver
# o comentario SYMLINK-DIR-REFUSED acima de _find_symlinked_directories()).
def selftest_symlinked_layer_dir_control(scratch, capture):
    root = os.path.join(scratch, "symlinked_layer_dir")
    make_clean_fixture(root)

    # Diretorio de verdade FORA das seis camadas cobertas, com um
    # cabecalho de SO plantado dentro - o mesmo conteudo que, se
    # alcancado, seria uma violacao normal de agulha.
    external_dir = os.path.join(root, "_external_not_a_layer")
    os.makedirs(external_dir, exist_ok=True)
    with open(
        os.path.join(external_dir, "dirty.hpp"), "w", encoding="utf-8", newline=""
    ) as handle:
        handle.write("#include <wayland-client.h>\n")

    # Ligacao simbolica DENTRO de src/core/ apontando pro diretorio
    # externo acima - o caso que os.walk(followlinks=False) listaria em
    # dirnames mas nunca visitaria como dirpath.
    link_path = os.path.join(root, "src", "core", "linked")
    os.symlink(external_dir, link_path, target_is_directory=True)

    outcome = capture(lambda: check_layers(root))
    if outcome.result:
        print(
            "selftest: controle de LIGACAO SIMBOLICA FALHOU (ligacao dentro "
            "de src/core/ deveria ter sido recusada, mas passou)",
            file=sys.stderr,
        )
        print(outcome.text, file=sys.stderr)
        return False
    if link_path not in outcome.text:
        print(
            "selftest: controle de LIGACAO SIMBOLICA FALHOU (recusou, mas "
            f"nao citou {link_path})",
            file=sys.stderr,
        )
        print(outcome.text, file=sys.stderr)
        return False
    # A parte mais importante da prova: o conteudo por TRAS da ligacao
    # (dirty.hpp, fora da arvore de camadas) nunca deveria ter sido
    # varrido - se aparecesse citado, o portao estaria SEGUINDO a
    # ligacao, nao recusando a presenca dela (o desenho ESCOLHIDO,
    # ver o comentario SYMLINK-DIR-REFUSED).
    if "dirty.hpp" in outcome.text:
        print(
            "selftest: controle de LIGACAO SIMBOLICA FALHOU (citou "
            "dirty.hpp - o portao seguiu a ligacao em vez de so recusar a "
            "presenca dela)",
            file=sys.stderr,
        )
        print(outcome.text, file=sys.stderr)
        return False
    print(
        "selftest: controle de LIGACAO SIMBOLICA OK (presenca recusada, "
        "nomeada, sem seguir o conteudo por tras dela)"
    )
    return True


def selftest_main():
    scratch = make_scratch_workdir()
    capture = _make_capture()
    try:
        controls = [
            selftest_positive_control(scratch, capture),
            selftest_negative_control(scratch, capture),
            selftest_negative_control_file_header(scratch, capture),
            selftest_empty_scan_control(scratch, capture),
            selftest_negative_control_gfss_gfui(scratch, capture),
            selftest_gfss_gfui_per_directory_floor(scratch, capture),
            selftest_anchor_directive_control(scratch, capture),
            selftest_phase23_directive_control(scratch, capture),
            selftest_raw_string_control(scratch, capture),
            selftest_crlf_control(scratch, capture),
            selftest_bom_control(scratch, capture),
            selftest_computed_include_control(scratch, capture),
            selftest_needle_bites_control(scratch, capture),
            selftest_symlinked_layer_dir_control(scratch, capture),
        ]
        if not all(controls):
            print("check_layers.py --selftest: FALHOU (ver acima)", file=sys.stderr)
            sys.exit(1)
        print(f"check_layers.py --selftest: os {len(controls)} controles OK")
    finally:
        shutil.rmtree(scratch, ignore_errors=True)


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
    else:
        real_main(args)


if __name__ == "__main__":
    main()
