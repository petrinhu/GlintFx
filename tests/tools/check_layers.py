#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_layers.py - CI gate for GODS_LAWS.md L-19 ("a CI gate reproves
# the violation" instead of trusting the discipline of whoever writes
# the code). Verifies that the six pure layers (src/core, include/
# glintfx/core, src/gfss, src/gfui, include/glintfx/gfss, include/
# glintfx/gfui - see _PURE_LAYER_DIR_SPECS below, the single source of
# truth for which six directories this gate owns) never pull in a
# header from a layer above (glintfx/platform/) or from the operating
# system, and never hide a violation behind a form this scanner cannot
# see through. None of these six directories knows anything about the
# OS - only platform/ does (GODS_LAWS.md L-19), and render/ is a
# documented, deliberate exception of its own (L-31, L-07 EXCECAO No
# 1: the GL loader), so this gate does not scan either.
#
# LAYERS-GATE-GFSS-GFUI sub-fatia L-4 (TODO.md, GODS_LAWS.md L-19/
# L-40/L-67, plano em docs/plano-layers-l4.md): a auditoria de L-3
# achou que a POLITICA de nomes era uma lista de PROIBIDOS por
# subcadeia - busca dirigida, que a propria L-40 item 5 probe ("quando
# o espaco de verificacao e pequeno e enumeravel, enumere-o inteiro").
# Onze formas media contra os dois compiladores passavam limpas por
# essa lista (aspas, "./", "..", caminho absoluto, caminho versionado,
# caixa, cabecalho de SO que ninguem tinha listado). Esta fatia troca
# a lista de proibidos por uma LISTA DE PERMITIDOS FECHADA - a
# biblioteca padrao do C++23 (N4950, menos fstream/filesystem, banidos
# desde ASSET-LOAD), os cabecalhos publicos do proprio projeto que
# EXISTEM de verdade nas seis camadas puras, os dois cabecalhos
# gerados pelo CMake, e inclusao relativa entre aspas que resolve
# dentro das seis camadas - ver classify_header_argument() abaixo. A
# mesma auditoria achou que a tabela de macros guardava UM valor por
# nome (a ultima definicao textual vencia, mesmo quando o compilador
# real usaria outra, dependendo de qual ramo de #if estivesse ativo) -
# _collect_macro_multimap() abaixo troca isso por um MULTIMAPA com
# SEMANTICA DE UNIAO: toda definicao de todo ramo conta, e qualquer
# uma que nao resolva com seguranca reprova a diretiva inteira. A
# mesma auditoria achou tres buracos de prova menores (percurso nunca
# testado em gfss/gfui nem na propria raiz da camada; `#include_next`
# caindo no balde errado por acidente; atalho de arquivo QUEBRADO
# nunca reprovando) e duas lacunas do INBOX absorvidas nesta fatia
# (`PORTAO-DE-CAMADA-CEGO-A-UTF16`, `PORTAO-DE-CAMADA-AGULHA-SO-MORDE-
# ANGULO`) - ver walk_layer_directory() e _read_source_text() abaixo.
#
# O QUE SAIU (GODS_LAWS.md L-67, o que para de ser verdade e apagado,
# nunca comentado): `OS_HEADER_NEEDLES`, `UPPER_LAYER_NEEDLE`,
# `_FORBIDDEN_PATTERN` e a lista de proibidos que eles formavam;
# `_collect_object_macros` (um valor por nome); `_find_symlinked_
# directories`/`_walk_header_files`/`os.walk` (seguido por um percurso
# proprio via os.scandir+os.lstat que classifica cada entrada numa
# enumeracao fechada de quatro tipos: arquivo regular, pasta, ligacao -
# nunca seguida -, e "outro" - FIFO/dispositivo/soquete, nunca
# aberto); a nota UTF-16 "LIMITACAO DECLARADA"; e as duas linhas do
# INBOX no TODO.md.
#
# ORACULO DIFERENCIAL (GODS_LAWS.md projeto L-5, docs/plano-layers-l5.
# md): este arquivo NAO chama compilador nenhum - so oferece, pelo modo
# `--export-fixtures <pasta-vazia>` da linha de comando, as fixtures
# das proprias tabelas de _CASE_TABLES (abaixo) mais o veredito REAL de
# check_layers() pra cada uma, num manifest.json. Quem consome isso e'
# tests/tools/check_layers_oracle.py, um script SEPARADO que roda so'
# no CI do servidor (nunca nesta maquina - GODS_LAWS.md L-45): ele
# pergunta ao compilador de verdade que cabecalho cada arquivo de
# camada pura puxa de fato, e reprova se o compilador viu cabecalho
# proibido num caso que este portao deixou passar. `_CASE_TABLES` e' a
# fonte unica das duas pontas (autoteste E exportacao); a trava X1 (ver
# `selftest_case_table_registry_x1` abaixo) enumera o modulo inteiro e
# reprova tabela de Case esquecida fora do registro.
#
# O QUE AINDA FICA FORA DESTA FATIA (INBOX, ver docs/plano-layers-l4.md
# §7 e docs/plano-layers-l5.md §11): direcao de inclusao ENTRE as
# camadas puras (core nao inclui gfss, gfss nao inclui gfui);
# <cstdio>/<iostream> como agulha nova (decisao de produto do lider); a
# divisao deste arquivo em modulos por assunto; e a direcao 2 do
# oraculo (portao reprovou, compilador so' puxou permitido) cruzada
# ENTRE sistemas (ORACULO-CAMADAS-DIRECAO-2-ENTRE-SISTEMAS, TODO.md).
#
# ADENDO (docs/plano-layers-l4-adendo.md, GODS_LAWS.md L-40/L-42/L-67):
# a revisao independente achou que `import`/`export import` de unidade
# de cabecalho partido em varias linhas fisicas bypassava o portao
# inteiro (CRITICO-1) e quatro buracos de prova (IMPORTANTE-1..3,
# UTF-32 BE). Medindo contra g++ 16.2.1 e clang++ 22.1.8 nesta sessao,
# o defeito acabou sendo maior: ONZE formas de `import` de unidade de
# cabecalho escapavam (atributo, macro, colagem `##`, fora do inicio
# de linha), `import std;`/`module X;` puxavam camada proibida por
# DECISAO invertida agora, e o lexico da fase 3 (numero de pre-
# processamento, nome de cabecalho, literal que atravessa linha)
# dessincronizava do compilador em sete formas, escondendo um
# `#include <fstream>` comum atras de um comentario que so' o portao
# enxergava. A troca: os identificadores `import`, `module`,
# `_Pragma`, `__pragma`, `asm`, `__asm`, `__asm__`, `_asm` sao
# proibidos por TOKEN em qualquer posicao (D-A1, _FORBIDDEN_
# IDENTIFIERS abaixo); colagem `##`/`%:%:` que pode FORMAR um desses
# reprova pela cadeia inteira (D-A2, _paste_chain_violations());
# `#pragma` vira lista de PERMITIDOS - so' `once` passa (D-A3,
# _pragma_is_allowed()); e o lexico da fase 3 passa a reconhecer
# numero de pre-processamento e nome de cabecalho como a norma, com
# recusa fechada de literal nao terminado na linha (D-A4,
# _PhaseLexer abaixo). `_IMPORT_DIRECTIVE_PATTERN` e' a decisao
# INVERTIDA de L-3 (L-67): a diretiva de import de UMA linha nao e'
# mais reconhecida aqui - D-A1 cobre TODA forma, nao so' essa.
#
# ANCHOR-ON-DIRECTIVE/PHASE-2-3/RAW-STRING-NO-SPLICE (heranca de L-1..
# L-3): a agulha so e procurada dentro do ARGUMENTO de uma diretiva de
# inclusao reconhecida (nunca numa linha qualquer, comentario
# incluido); a segmentacao em linhas logicas modela as fases 2 (emenda
# de linha) e 3 (comentario vira espaco, cadeia bruta e' isenta da
# fase 2, numero de pre-processamento e nome de cabecalho sao lidos
# como a norma) da traducao ANTES de qualquer diretiva ser reconhecida
# - ver _PhaseLexer/_translate_phases_2_and_3() e _split_logical_
# lines() abaixo. O CONTRATO de saida (clean_text, clean_lines) e' o
# mesmo de L-1..L-3; o adendo so' ACRESCENTA a lista de violacoes
# lexicas e as faixas excluidas (string/char/nome de cabecalho), que
# D-A1/D-A2 usam pra nao contar identificador proibido dentro de
# literal, comentario ou nome de cabecalho.
#
# Usage:
#   check_layers.py <source-root-directory>
#   check_layers.py --selftest
#
# Each function below does one thing (GODS_LAWS.md L-17).

import collections
import json
import os
import re
import shutil
import stat
import sys
import tempfile

SCRIPT_NAME = "check_layers.py"

# --- the six pure layers: single source of truth (L-4, achado 2.2) ---
#
# Substitui core_source_dirs() + GFSS_GFUI_DIR_SPECS (duas fontes
# antes desta fatia) por UMA constante unica - o piso de varredura nao
# vazia, a recusa de ligacao simbolica e o percurso de arquivo agora
# tratam as seis pastas de forma identica, uma por uma, nunca em
# agregado (uma pasta que perdeu todo arquivo nao se esconde atras da
# contagem de outra - GODS_LAWS.md L-40).
_PURE_LAYER_DIR_SPECS = (
    ("src/core", ("src", "core")),
    ("include/glintfx/core", ("include", "glintfx", "core")),
    ("src/gfss", ("src", "gfss")),
    ("src/gfui", ("src", "gfui")),
    ("include/glintfx/gfss", ("include", "glintfx", "gfss")),
    ("include/glintfx/gfui", ("include", "glintfx", "gfui")),
)

_PROJECT_PURE_SUBLAYERS = ("core", "gfss", "gfui")

# --- N4950 (C++23) standard library headers, allow-list (D-1) --------
#
# Transcrito da norma, nao de memoria (GODS_LAWS.md L-4, criterio 6 do
# plano): Table 24 "C++ library headers" e Table 25 "C++ headers for C
# library facilities", https://timsong-cpp.github.io/cppwp/n4950/headers
# (conferido contra a fonte em 23/09/2026). C12/selftest_stdlib_table_
# count_control() abaixo imprime a contagem e compara com as duas
# tabelas escritas aqui - uma transcricao errada faz esse controle
# reprovar sozinho, sem depender de nenhum outro caso notar a lacuna.
_N4950_CXX_LIBRARY_HEADERS = (
    "algorithm", "any", "array", "atomic", "barrier", "bit", "bitset",
    "charconv", "chrono", "codecvt", "compare", "complex", "concepts",
    "condition_variable", "coroutine", "deque", "exception", "execution",
    "expected", "filesystem", "flat_map", "flat_set", "format",
    "forward_list", "fstream", "functional", "future", "generator",
    "initializer_list", "iomanip", "ios", "iosfwd", "iostream", "istream",
    "iterator", "latch", "limits", "list", "locale", "map", "mdspan",
    "memory", "memory_resource", "mutex", "new", "numbers", "numeric",
    "optional", "ostream", "print", "queue", "random", "ranges", "ratio",
    "regex", "scoped_allocator", "semaphore", "set", "shared_mutex",
    "source_location", "span", "spanstream", "sstream", "stack",
    "stacktrace", "stdexcept", "stdfloat", "stop_token", "streambuf",
    "string", "string_view", "strstream", "syncstream", "system_error",
    "thread", "tuple", "type_traits", "typeindex", "typeinfo",
    "unordered_map", "unordered_set", "utility", "valarray", "variant",
    "vector", "version",
)
_N4950_C_COMPAT_HEADERS = (
    "cassert", "cctype", "cerrno", "cfenv", "cfloat", "cinttypes",
    "climits", "clocale", "cmath", "csetjmp", "csignal", "cstdarg",
    "cstddef", "cstdint", "cstdio", "cstdlib", "cstring", "ctime",
    "cuchar", "cwchar", "cwctype",
)
assert len(_N4950_CXX_LIBRARY_HEADERS) == 86, "Table 24 tem 86 cabecalhos - contagem mudou?"
assert len(_N4950_C_COMPAT_HEADERS) == 21, "Table 25 tem 21 cabecalhos - contagem mudou?"

# ASSET-LOAD (28/08/2026, GODS_LAWS.md L-19/L-40): proibicao PRE-
# EXISTENTE a esta fatia, preservada literalmente - nenhuma proibicao
# nova e' inventada aqui (GODS_LAWS.md projeto L-5 do plano).
_STDLIB_BANNED_NAMES = frozenset({"fstream", "filesystem"})
_STDLIB_ALLOWED_NAMES = (
    frozenset(_N4950_CXX_LIBRARY_HEADERS) | frozenset(_N4950_C_COMPAT_HEADERS)
) - _STDLIB_BANNED_NAMES

# Cabecalhos GERADOS pelo CMake (nao existem na arvore rastreada - sao
# escritos no diretorio de build), lista fechada citada da fonte:
# cmake/GlintfxLibrary.cmake:131 (EXPORT_FILE_NAME) e :140 (configure_
# file do template version_macros.hpp.in).
_GENERATED_HEADERS = frozenset({"glintfx/export.hpp", "glintfx/version_macros.hpp"})

# FILE-EXTENSIONS-CPP23, alargada com a lista de sufixos de C++ do GCC
# (docs/plano-layers-l4.md §1.3, <https://gcc.gnu.org/onlinedocs/gcc/
# Overall-Options.html>) e comparada SEM distincao de caixa (D12).
_SOURCE_EXTENSIONS = (
    ".c", ".cc", ".cp", ".cxx", ".cpp", ".c++",
    ".h", ".hh", ".hp", ".hxx", ".hpp", ".h++",
    ".tcc", ".ipp", ".tpp", ".inl",
    ".cppm", ".ixx", ".mpp", ".ccm", ".cxxm",
)
_KNOWN_NON_SOURCE_FILENAMES = frozenset({"CMakeLists.txt"})

# --- diretivas: enumeracao fechada de nomes (familia B) ---------------
#
# Nunca `#include`/`import` isolados: TODO nome de diretiva e' julgado.
# Estas 14 nao afetam inclusao (permitidas, sem efeito); `include` e
# `include_next` tem tratamento proprio abaixo (resolvem cabecalho);
# qualquer OUTRO nome reprova, nomeado - cobre `#import` (COM/Obj-C),
# `#using` (C++/CLI), `#embed` (extensao Clang), `#assert`, `#ident` e
# o que ainda nao existe (docs/plano-layers-l4.md §2.3a).
_ALLOWED_OTHER_DIRECTIVE_NAMES = frozenset({
    "define", "undef", "if", "ifdef", "ifndef", "elif", "elifdef",
    "elifndef", "else", "endif", "line", "error", "warning", "pragma",
})
_INCLUSION_DIRECTIVE_NAMES = ("include", "include_next")

_DIRECTIVE_NAME_PATTERN = re.compile(r"^\s*(?:#|%:)\s*([A-Za-z_]\w*)?")

# --- D-A1: identificadores proibidos por TOKEN em qualquer posicao ---
#
# (docs/plano-layers-l4-adendo.md D-A1): `import`/`module` (importacao
# de unidade de cabecalho, de modulo nomeado e importacao implicita -
# nenhuma e' verificavel por texto, [cpp.import]/[module.unit]),
# `_Pragma`/`__pragma` (o operador de pragma, mesma superficie que
# `#pragma`, so' que vindo de dentro de uma macro), e `asm`/`__asm`/
# `__asm__`/`_asm` (a diretiva assembly puxa arquivo com `.incbin`/
# `.include`, medido contra g++ - objdump embutiu o arquivo na secao
# .text). `_INCLUDE_ALIAS_PATTERN`/`_IMPORT_DIRECTIVE_PATTERN` da L-3
# SAEM (L-67): a diretiva de import de UMA linha e a busca textual de
# `include_alias` nao cobrem as formas partidas/por macro/por colagem
# que a revisao achou - o token cobre TODA forma, em qualquer versao
# de compilador (2.2 do adendo: a diferenca GCC/clang e' de versao).
_FORBIDDEN_IDENTIFIERS = frozenset({
    "import", "module", "_Pragma", "__pragma",
    "asm", "__asm", "__asm__", "_asm",
})
# Numero LITERAL, independente de _FORBIDDEN_IDENTIFIERS - a mesma
# trava de C12 (memoria feedback_trava_tautologica.md): F18 gera um
# caso por identificador ITERANDO a propria constante, entao um nome
# apagado da constante encolhe a lista de casos JUNTO e o "trava:
# len(fixtures) == len(constante)" nunca nota. So' um numero escrito A
# PARTE, que nao deriva do que esta sendo conferido, pega isso.
_FORBIDDEN_IDENTIFIER_COUNT = 8
_IDENTIFIER_TOKEN_PATTERN = re.compile(r"[A-Za-z_]\w*")

# --- D-A4: contextos da norma que abrem NOME DE CABECALHO -----------
#
# [lex.pptoken]: so' nasce nome de cabecalho depois de `include`/
# `include_next`/`import`/`embed` numa diretiva reconhecida, depois de
# `import` (ou `export import`) no inicio da linha logica, ou dentro
# de `__has_include`/`__has_include_next`/`__has_embed` seguido de
# `(`. Fora desses contextos `<`/`"` sao operador/abertura de string
# comuns - a raiz de P12/Q01-Q08 (docs/plano-layers-l4-adendo.md §1.4)
# e' o portao NAO conhecer esses contextos e deixar `'`/`"`/`/*`/`//`
# dentro de `<...>` abrirem literal/comentario FALSOS.
_HEADER_NAME_INCLUDE_DIRECTIVES = frozenset({"include", "include_next", "import", "embed"})
_HEADER_NAME_HAS_OPERATORS = frozenset({"__has_include", "__has_include_next", "__has_embed"})

# [lex.header]: apostrofo, barra invertida, `/*` e `//` sao de suporte
# CONDICIONAL num nome de cabecalho (`"` tambem, so' na forma `<...>`)
# - cada implementacao decide o que fazer; este portao nunca da'
# beneficio da duvida, sempre reprova (docs/plano-layers-l4-adendo.md
# D-A4 item 2).
_HEADER_NAME_ODD_SUBSTRINGS = ("'", '"', "\\", "/*", "//")

# --- D-A3: `#pragma` vira lista de PERMITIDOS -------------------------
_PRAGMA_HEAD_PATTERN = re.compile(r"^\s*(?:#|%:)\s*pragma\b(.*)$")
_ALLOWED_PRAGMA_TOKENS = ("once",)

# --- macro multimapa (familia A) --------------------------------------
#
# `(\(?)`nao tem `\s*` entre o nome e o parenteses de proposito: macro
# de FUNCAO exige o `(` colado ao nome (a propria norma C++). Um
# espaco antes do `(` faz o corpo comecar com "(x) ..." como texto
# comum de macro de OBJETO - LITERAL_BODY_PATTERN abaixo so aceita
# corpo que e' EXATAMENTE um nome de cabecalho `<...>`/"...", entao
# esse caso cai em "other" do mesmo jeito, pelo motivo certo.
_DEFINE_HEAD_PATTERN = re.compile(r"^\s*(?:#|%:)\s*define\s+([A-Za-z_]\w*)(\(?)(.*)$")
_OBJECT_MACRO_LITERAL_BODY_PATTERN = re.compile(r'^(<[^>\n]*>|"[^"\n]*")$')
_LITERAL_ARGUMENT_PATTERN = re.compile(r'^\s*(<[^>\n]*>|"[^"\n]*")\s*$')
_BARE_IDENTIFIER_PATTERN = re.compile(r"^\s*([A-Za-z_]\w*)\s*$")


def _is_ident_char(ch):
    return ch.isalnum() or ch == "_"


def _pp_number_end(text, i, n):
    """[lex.ppnumber]: consome um numero de pre-processamento inteiro a
    partir de `i` (que ja' e' digito, ou '.' seguido de digito) e
    devolve o indice logo apos o fim. O `'` dentro dele NUNCA abre
    literal de caractere - e' a raiz de P12/Q04/Q07 (docs/plano-
    layers-l4-adendo.md D-A4 item 1): `pp-number ' digit` e `pp-number
    ' nondigit` sao producao da propria gramatica."""
    j = i + 1
    while j < n:
        c = text[j]
        if c in "eEpP" and j + 1 < n and text[j + 1] in "+-":
            j += 2
            continue
        if c == "'" and j + 1 < n and _is_ident_char(text[j + 1]):
            j += 2
            continue
        if _is_ident_char(c) or c == ".":
            j += 1
            continue
        break
    return j


def _trailing_code_tokens(out, count):
    """Varre `out` (lista de chars ja' emitidos) de tras pra frente e
    devolve ate' `count` tokens finais - identificador, ou pontuacao de
    um caractere (o digrafo `%:` conta como um so') - como (texto,
    indice_inicial) em ORDEM NORMAL. So' pra decidir CONTEXTO de nome
    de cabecalho (D-A4 item 2) - nao e' um tokenizador completo."""
    tokens = []
    j = len(out)
    while j > 0 and len(tokens) < count:
        while j > 0 and out[j - 1] in " \t\v\f\r\n":
            j -= 1
        if j == 0:
            break
        end = j
        ch = out[j - 1]
        if _is_ident_char(ch):
            while j > 0 and _is_ident_char(out[j - 1]):
                j -= 1
        elif ch == ":" and j >= 2 and out[j - 2] == "%":
            j -= 2
        else:
            j -= 1
        tokens.append(("".join(out[j:end]), j))
    tokens.reverse()
    return tokens


def _at_logical_line_start(out, index):
    j = index
    while j > 0 and out[j - 1] in " \t\v\f\r":
        j -= 1
    return j == 0 or out[j - 1] == "\n"


def _header_name_context(out):
    """D-A4 item 2, [lex.pptoken]: decide se a posicao logo apos `out`
    esta num dos tres contextos da norma que abrem nome de cabecalho.
    Nao consome nada - so' olha pra tras (_trailing_code_tokens)."""
    tail = _trailing_code_tokens(out, 2)
    if not tail:
        return False
    last_text, last_idx = tail[-1]
    prev_text, prev_idx = tail[-2] if len(tail) >= 2 else (None, None)
    if last_text == "import" and _at_logical_line_start(out, last_idx):
        return True
    if last_text == "import" and prev_text == "export" and _at_logical_line_start(out, prev_idx):
        return True
    if (
        prev_text in ("#", "%:")
        and last_text in _HEADER_NAME_INCLUDE_DIRECTIVES
        and _at_logical_line_start(out, prev_idx)
    ):
        return True
    if last_text == "(" and prev_text in _HEADER_NAME_HAS_OPERATORS:
        return True
    return False


def _scan_header_name_body(text, start, n, opener):
    """A partir de `start` (logo apos o abre `<` ou `"`), procura o
    fecho (`>` ou `"`) na MESMA linha logica (splice aplicado por
    cima), respeitando splices. Devolve (fim_exclusivo, corpo) se achar
    o fecho ANTES de uma quebra REAL, senao None - o lexico comum
    entao segue, exatamente como o compilador faz (D-A4 item 2)."""
    closer = ">" if opener == "<" else '"'
    body = []
    j = start
    while j < n:
        if text[j] == "\\" and j + 1 < n and text[j + 1] in ("\n", "\r"):
            j += 3 if text[j + 1] == "\r" and j + 2 < n and text[j + 2] == "\n" else 2
            continue
        if text[j] == closer:
            return j + 1, "".join(body)
        if text[j] == "\n":
            return None
        body.append(text[j])
        j += 1
    return None


def _header_body_has_odd_char(body):
    """[lex.header]: apostrofo, barra invertida, `/*`, `//` (e `"` na
    forma `<...>`) sao de suporte condicional - nunca da beneficio da
    duvida (D-A4 item 2)."""
    return any(needle in body for needle in _HEADER_NAME_ODD_SUBSTRINGS)


_RAW_STRING_PREFIXES = ("u8R", "uR", "UR", "LR", "R")
_RAW_STRING_MAX_DELIM = 16
_RAW_STRING_FORBIDDEN_DELIM_CHARS = frozenset(" \t\v\f\r\n()\\\"")


class _PhaseLexer:
    """Fases 2 (emenda de linha) e 3 (troca de comentario por espaco,
    reconhecimento de string/char/cadeia-bruta/numero-de-pre-
    processamento/nome-de-cabecalho) da traducao, num unico laco sobre
    o texto ORIGINAL. Um metodo por estado (GODS_LAWS.md L-17: a
    funcao herdada de L-1..L-3 tinha 160 linhas e nao podia crescer -
    docs/plano-layers-l4-adendo.md D-A4 acrescenta tres regras a ela,
    entao ela foi dividida no mesmo commit). O conteudo de uma cadeia
    bruta continua ISENTO da fase 2 (RAW-STRING-NO-SPLICE, herdado de
    L-2). Resultado em .out/.out_lines (clean_text/clean_lines, mesmo
    contrato de L-1..L-3), .violations (as tres regras novas) e
    .excluded (faixas de string/char/nome-de-cabecalho, pra D-A1/D-A2
    nao contarem identificador proibido dentro delas).
    """

    _CODE, _STRING, _CHAR, _BLOCK, _LINE, _RAW = range(6)

    def __init__(self, text):
        self.text = text
        self.n = len(text)
        self.i = 0
        self.line_no = 1
        self.state = self._CODE
        self.out = []
        self.out_lines = []
        self.violations = []
        self.excluded = []
        self.comment_start_line = None
        self.raw_closer = None
        self.raw_start_line = None
        self.token_start = None

    def run(self):
        while self.i < self.n:
            if self.state != self._RAW and self._consume_splice():
                continue
            ch = self.text[self.i]
            if self.state == self._CODE:
                self._step_code(ch)
            elif self.state in (self._STRING, self._CHAR):
                self._step_quoted(ch)
            elif self.state == self._BLOCK:
                self._step_block_comment(ch)
            elif self.state == self._LINE:
                self._step_line_comment(ch)
            else:
                self._step_raw_string()
        self._flush_open_state()

    def _consume_splice(self):
        if self.text[self.i] != "\\" or self.i + 1 >= self.n or self.text[self.i + 1] not in ("\n", "\r"):
            return False
        if self.text[self.i + 1] == "\r" and self.i + 2 < self.n and self.text[self.i + 2] == "\n":
            self.i += 3
        else:
            self.i += 2
        self.line_no += 1
        return True

    def _emit_code_char(self, ch):
        self.out.append(ch)
        self.out_lines.append(self.line_no)
        if ch == "\n":
            self.line_no += 1
        self.i += 1

    def _step_code(self, ch):
        nxt = self.text[self.i + 1] if self.i + 1 < self.n else ""
        if ch == "/" and nxt == "*":
            self._open_block_comment()
            return
        if ch == "/" and nxt == "/":
            self._open_line_comment()
            return
        if (ch.isdigit() or (ch == "." and nxt.isdigit())) and not (self.out and _is_ident_char(self.out[-1])):
            self._consume_number()
            return
        if ch in "<\"" and _header_name_context(self.out) and self._consume_header_name(ch):
            return
        if ch == '"':
            self._open_string_literal()
            return
        if ch == "'":
            self._open_char_literal()
            return
        self._emit_code_char(ch)

    def _open_block_comment(self):
        self.state = self._BLOCK
        self.comment_start_line = self.line_no
        self.i += 2

    def _open_line_comment(self):
        self.state = self._LINE
        self.comment_start_line = self.line_no
        self.i += 2

    def _consume_number(self):
        end = _pp_number_end(self.text, self.i, self.n)
        for k in range(self.i, end):
            self.out.append(self.text[k])
            self.out_lines.append(self.line_no)
        self.i = end

    def _consume_header_name(self, opener):
        scanned = _scan_header_name_body(self.text, self.i + 1, self.n, opener)
        if scanned is None:
            return False
        end, body = scanned
        if _header_body_has_odd_char(body):
            self.violations.append(("nome de cabecalho com caractere de suporte condicional", self.line_no))
        start_out = len(self.out)
        for k in range(self.i, end):
            self.out.append(self.text[k])
            self.out_lines.append(self.line_no)
        self.excluded.append((start_out, len(self.out)))
        self.i = end
        return True

    def _open_string_literal(self):
        prefix_length = self._raw_prefix_length()
        delimiter_result = self._raw_delimiter(self.i + 1) if prefix_length else None
        if delimiter_result is not None:
            delimiter, body_start = delimiter_result
            self.raw_closer = ")" + delimiter + '"'
            self.raw_start_line = self.line_no
            self.state = self._RAW
            self.i = body_start
            return
        self.token_start = len(self.out)
        self.state = self._STRING
        self._emit_code_char('"')

    def _open_char_literal(self):
        self.token_start = len(self.out)
        self.state = self._CHAR
        self._emit_code_char("'")

    def _raw_prefix_length(self):
        tail = "".join(self.out[-3:])
        for prefix in _RAW_STRING_PREFIXES:
            if not tail.endswith(prefix):
                continue
            before_index = len(self.out) - len(prefix) - 1
            if before_index >= 0 and _is_ident_char(self.out[before_index]):
                continue
            return len(prefix)
        return 0

    def _raw_delimiter(self, start):
        j = start
        delimiter_chars = []
        while j < self.n:
            ch2 = self.text[j]
            if ch2 == "(":
                return "".join(delimiter_chars), j + 1
            if ch2 in _RAW_STRING_FORBIDDEN_DELIM_CHARS:
                return None
            if len(delimiter_chars) >= _RAW_STRING_MAX_DELIM:
                return None
            delimiter_chars.append(ch2)
            j += 1
        return None

    def _step_quoted(self, ch):
        if ch == "\n":
            self.violations.append(("literal nao terminado", self.line_no))
            self._close_quoted_at_newline()
            return
        self.out.append(ch)
        self.out_lines.append(self.line_no)
        if ch == "\\" and self.i + 1 < self.n:
            self._consume_quoted_escape()
            return
        closer = '"' if self.state == self._STRING else "'"
        if ch == closer:
            self.excluded.append((self.token_start, len(self.out)))
            self.state = self._CODE
        self.i += 1

    def _consume_quoted_escape(self):
        escaped = self.text[self.i + 1]
        self.out.append(escaped)
        self.out_lines.append(self.line_no)
        if escaped == "\n":
            self.line_no += 1
        self.i += 2

    def _close_quoted_at_newline(self):
        self.excluded.append((self.token_start, len(self.out)))
        self.out.append("\n")
        self.out_lines.append(self.line_no)
        self.line_no += 1
        self.state = self._CODE
        self.i += 1

    def _step_block_comment(self, ch):
        nxt = self.text[self.i + 1] if self.i + 1 < self.n else ""
        if ch == "*" and nxt == "/":
            self.out.append(" ")
            self.out_lines.append(self.comment_start_line)
            self.state = self._CODE
            self.i += 2
            return
        if ch == "\n":
            self.line_no += 1
        self.i += 1

    def _step_line_comment(self, ch):
        if ch == "\n":
            self.out.append(" ")
            self.out_lines.append(self.comment_start_line)
            self.out.append(ch)
            self.out_lines.append(self.line_no)
            self.state = self._CODE
            self.line_no += 1
            self.i += 1
            return
        self.i += 1

    def _step_raw_string(self):
        ch = self.text[self.i]
        if self.text[self.i : self.i + len(self.raw_closer)] == self.raw_closer:
            self.out.append(" ")
            self.out_lines.append(self.raw_start_line)
            self.i += len(self.raw_closer)
            self.state = self._CODE
            self.raw_closer = None
            self.raw_start_line = None
            return
        if ch == "\n":
            self.line_no += 1
        self.i += 1

    def _flush_open_state(self):
        if self.state in (self._LINE, self._BLOCK):
            self.out.append(" ")
            self.out_lines.append(self.comment_start_line)
        elif self.state == self._RAW:
            self.out.append(" ")
            self.out_lines.append(self.raw_start_line)


# CONT-4C (docs/plano-layers-l4.md/plano-layers-l4-adendo.md, GODS_LAWS.md
# L-04/L-40 item 5): fase 1 da traducao ([lex.phases]/1: "any sequence
# intended to be a new-line is replaced by a new-line character")
# faltava por inteiro - _PhaseLexer so' reconhecia '\n' cru. Medido em
# 2026-09-23 contra g++ 16.2.1 e clang++ 22.1.8, agrupando TODAS as
# fixtures numa unica invocacao por compilador (GODS_LAWS.md L-11): so'
# tres formas sao quebra real - LF, CRLF (como UMA quebra) e CR sozinho
# (estilo Mac classico, como UMA quebra); VT (0x0B), FF (0x0C), FS/GS/RS
# (0x1C-0x1E) e os separadores Unicode NEL/LS/PS (U+0085/U+2028/U+2029)
# NAO sao - os dois compiladores os recusam (erro de token invalido) ou
# os tratam como espaco INTRA-linha, nunca como fim de linha; um arquivo
# com CR sozinho como unico separador (sem '\n' nenhum) escapava do
# portao antigo por inteiro: nenhuma linha logica era reconhecida, e um
# '#include <fstream>' assim escrito nunca era visto. A normalizacao
# roda ANTES da fase 2+3 (_translate_phases_2_and_3 abaixo), pra toda
# checagem de quebra de linha rio abaixo - inclusive a que
# _scan_header_name_body() faz sobre o texto CRU - herdar o mesmo '\n'
# unico sem precisar saber da CR.
_LINE_BREAK_SEQUENCE_PATTERN = re.compile(r"\r\n|\r")


def _normalize_newlines(text):
    """Fase 1 da traducao ([lex.phases]/1). Troca toda sequencia que os
    dois compiladores medidos reconhecem como quebra de linha fisica
    (CRLF como UMA, CR sozinho como UMA) por '\\n'; nao toca nos
    separadores medidos como NAO-quebra (ver bloco de comentario
    acima). Duas quebras cru consecutivas (ex.: LF seguido de CR solto,
    '\\n\\r') viram duas '\\n' - uma linha logica vazia entre elas, que
    _split_logical_lines() abaixo ja descarta por nao ter conteudo."""
    return _LINE_BREAK_SEQUENCE_PATTERN.sub("\n", text)


_PhaseResult = collections.namedtuple("_PhaseResult", "clean_text clean_lines violations excluded")


def _translate_phases_2_and_3(text):
    """Fases 2+3 da traducao - ver _PhaseLexer acima. Retorna
    _PhaseResult(clean_text, clean_lines, violations, excluded):
    mesmo contrato (clean_text, clean_lines) de L-1..L-3, ACRESCIDO
    (docs/plano-layers-l4-adendo.md D-A4) da lista de violacoes
    lexicas [(motivo, linha), ...] e das faixas excluidas [(inicio,
    fim), ...] em indices de clean_text (string/char/nome-de-
    cabecalho) que D-A1/D-A2 usam pra nao contar identificador
    proibido dentro de literal/nome-de-cabecalho."""
    lexer = _PhaseLexer(text)
    lexer.run()
    return _PhaseResult("".join(lexer.out), lexer.out_lines, lexer.violations, lexer.excluded)


def _split_logical_lines(clean_text, clean_lines):
    """Gera (linha_fisica, texto_logico) para toda linha logica NAO-VAZIA
    de `clean_text`/`clean_lines` (fase 2+3 ja' aplicadas por
    _translate_phases_2_and_3() acima)."""
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


def _range_excluded(index, excluded_ranges):
    return any(start <= index < end for start, end in excluded_ranges)


def _forbidden_token_violations(clean_text, clean_lines, excluded_ranges):
    """D-A1: cada ocorrencia de um identificador de _FORBIDDEN_
    IDENTIFIERS fora de comentario (ja' virou espaco na fase 3),
    literal (string/char, `excluded_ranges`) e nome de cabecalho
    (idem) reprova, nomeada (docs/plano-layers-l4-adendo.md D-A1)."""
    violations = []
    for match in _IDENTIFIER_TOKEN_PATTERN.finditer(clean_text):
        word = match.group(0)
        if word not in _FORBIDDEN_IDENTIFIERS:
            continue
        if _range_excluded(match.start(), excluded_ranges):
            continue
        violations.append((
            f"identificador '{word}' proibido em camada pura: a importacao de modulo e "
            "de unidade de cabecalho nao e verificavel por texto (GODS_LAWS.md L-40)",
            clean_lines[match.start()],
        ))
    return violations


_PASTE_TOKEN_PATTERN = re.compile(r"[A-Za-z_]\w*|[0-9][\w.]*|##|%:%:|\S")


def _function_macro_params(param_and_body_text):
    """`param_and_body_text` comeca logo apos o `(` colado ao nome (ja'
    confirmado macro de funcao pelo chamador). Devolve (params,
    body_text) - `__VA_ARGS__`/`__VA_OPT__` sempre contam como
    parametro (D-A2), mesmo sem `...` na lista."""
    depth = 1
    end = len(param_and_body_text)
    for idx, ch in enumerate(param_and_body_text):
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
            if depth == 0:
                end = idx
                break
    param_text = param_and_body_text[:end]
    body_text = param_and_body_text[end + 1 :]
    params = {p.strip() for p in param_text.split(",") if p.strip() and p.strip() != "..."}
    params.add("__VA_ARGS__")
    params.add("__VA_OPT__")
    return params, body_text


def _paste_chains(body_text):
    """D-A2: agrupa tokens ligados por `##`/`%:%:` em cadeias - a
    cadeia INTEIRA e' a unidade, nao cada operador isolado (G12,
    docs/plano-layers-l4-adendo.md D-A2)."""
    tokens = _PASTE_TOKEN_PATTERN.findall(body_text)
    chains = []
    i = 1
    n = len(tokens)
    while i < n - 1:
        if tokens[i] not in ("##", "%:%:"):
            i += 1
            continue
        chain = [tokens[i - 1], tokens[i + 1]]
        i += 2
        while i < n - 1 and tokens[i] in ("##", "%:%:"):
            chain.append(tokens[i + 1])
            i += 2
        chains.append(chain)
    return chains


def _is_fixed_ident_token(tok, params):
    return tok not in params and bool(tok) and (tok[0].isalpha() or tok[0] == "_")


def _paste_chain_is_safe(chain, params):
    """D-A2, as tres regras (qualquer uma basta): (1) o1 fixo e nenhum
    proibido COMECA com ele; (2) ok fixo e nenhum proibido TERMINA com
    ele; (3) algum oi fixo que NAO e' subcadeia de nenhum proibido, ou
    o1 e' numero (resultado comeca por digito, nunca identificador)."""
    o1, ok = chain[0], chain[-1]
    if o1[0].isdigit():
        return True
    if _is_fixed_ident_token(o1, params) and not any(f.startswith(o1) for f in _FORBIDDEN_IDENTIFIERS):
        return True
    if _is_fixed_ident_token(ok, params) and not any(f.endswith(ok) for f in _FORBIDDEN_IDENTIFIERS):
        return True
    for operand in chain:
        if _is_fixed_ident_token(operand, params) and not any(operand in f for f in _FORBIDDEN_IDENTIFIERS):
            return True
    return False


def _paste_chain_violations(segment, lineno):
    """D-A2: so' se aplica a linhas `#define` (funcao ou objeto); o
    resto do arquivo nao tem `##` fora de macro (a norma so' define
    colagem dentro de replacement-list)."""
    match = _DEFINE_HEAD_PATTERN.match(segment)
    if not match:
        return []
    is_function_like = match.group(2) == "("
    if is_function_like:
        params, body_text = _function_macro_params(match.group(3))
    else:
        params, body_text = {"__VA_ARGS__", "__VA_OPT__"}, match.group(3)
    violations = []
    for chain in _paste_chains(body_text):
        if not _paste_chain_is_safe(chain, params):
            violations.append(("colagem que pode formar identificador proibido (GODS_LAWS.md L-40)", lineno))
    return violations


def _pragma_is_allowed(segment):
    """D-A3: `#pragma` vira lista de PERMITIDOS - so' `once` passa
    (comentario ja' virou espaco na fase 3, entao dividir por espaco
    basta). Pragma vazio (`#pragma` sozinho) reprova - fechado por
    padrao (H7)."""
    match = _PRAGMA_HEAD_PATTERN.match(segment)
    if not match:
        return True
    return tuple(match.group(1).split()) == _ALLOWED_PRAGMA_TOKENS


# --- familia A: multimapa de macro, semantica de uniao ----------------


def _collect_macro_multimap(logical_lines):
    """Retorna {nome: [definicao, ...]}, uma entrada por `#define`
    encontrado em QUALQUER lugar do arquivo, ramo condicional incluido
    (`#undef` e' ignorado - honrar pela ordem do texto e' exatamente o
    que dava errado, ver docs/plano-layers-l4.md §2.1). Cada definicao
    e' `("literal", corpo)` quando e' macro de OBJETO cujo corpo e'
    EXATAMENTE um nome de cabecalho `<...>`/"..." , ou `("other", None)`
    para macro de FUNCAO ou corpo que nao e' esse literal exato - a
    resolucao abaixo trata QUALQUER "other" no mesmo nome como motivo
    pra nao resolver (computada), nunca ignora silenciosamente.
    """
    macros = {}
    for _lineno, segment in logical_lines:
        match = _DEFINE_HEAD_PATTERN.match(segment)
        if not match:
            continue
        name = match.group(1)
        is_function_like = match.group(2) == "("
        entries = macros.setdefault(name, [])
        if is_function_like:
            entries.append(("other", None))
            continue
        body = match.group(3).strip()
        literal_match = _OBJECT_MACRO_LITERAL_BODY_PATTERN.match(body)
        if literal_match:
            entries.append(("literal", literal_match.group(0)))
        else:
            entries.append(("other", None))
    return macros


def _strip_literal_delimiters(text):
    if text.startswith("<") and text.endswith(">"):
        return text[1:-1], False
    if text.startswith('"') and text.endswith('"'):
        return text[1:-1], True
    return text, False


def _macro_candidates(name, macros):
    """Devolve a lista de corpos (uniao de TODA definicao do nome, L-4
    §2.1) ou None quando o nome cai em COMPUTADA - sem definicao
    nenhuma, ou alguma definicao "other" (macro de funcao, ou corpo
    que nao e' literal exato) no mesmo nome: fechado por padrao, nunca
    tenta resolver so' os ramos limpos."""
    definitions = macros.get(name) if name else None
    if not definitions:
        return None
    if any(kind == "other" for kind, _body in definitions):
        return None
    return [body for _kind, body in definitions]


def _angle_body_is_contaminated(body, macros):
    """Um corpo `<...>` cujo interior contem um identificador que E'
    ELE MESMO nome de macro no arquivo cai em COMPUTADA - o compilador
    real EXPANDE esse identificador de novo, o portao nao modela
    substituicao, entao nao da' o beneficio da duvida (L-4 §2.1)."""
    if not body.startswith("<"):
        return False
    inner = body[1:-1]
    return any(ident.group(0) in macros for ident in re.finditer(r"[A-Za-z_]\w*", inner))


def _first_refused_candidate(candidates, root, including_dir):
    """True se algum candidato reprova pela politica de nome (D-1) -
    basta UM pra reprovar o nome inteiro (semantica de uniao)."""
    for body in candidates:
        literal, is_quoted = _strip_literal_delimiters(body)
        if not classify_header_argument(literal, is_quoted, root, including_dir):
            return True
    return False


def _evaluate_include_argument(argument_text, macros, root, including_dir):
    """Retorna "pass", "computed" ou "policy" para o ARGUMENTO (o texto
    apos `include`/`include_next`) de uma diretiva de inclusao ja
    reconhecida - COMPUTED-INCLUDE (L-3) + a semantica de uniao de L-4
    (§2.1), dividida em tres funcoes auxiliares acima (GODS_LAWS.md
    L-17: a original tinha 39 linhas, 4 parametros e aninhamento 5 -
    achado do revisor de L-4, docs/plano-layers-l4-adendo.md §6).
    """
    literal_match = _LITERAL_ARGUMENT_PATTERN.match(argument_text)
    if literal_match:
        candidates = [literal_match.group(1)]
    else:
        bare_match = _BARE_IDENTIFIER_PATTERN.match(argument_text)
        name = bare_match.group(1) if bare_match else None
        candidates = _macro_candidates(name, macros)
        if candidates is None:
            return "computed"
        if any(_angle_body_is_contaminated(body, macros) for body in candidates):
            return "computed"

    if _first_refused_candidate(candidates, root, including_dir):
        return "policy"
    return "pass"


# --- familia C: politica de nome (lista de permitidos fechada, D-1) --


def _split_header_path(name):
    """Divide `name` em componentes por '/', recusando barra invertida
    (C8: docs MS aceita, este portao nunca aceita - GODS_LAWS.md
    projeto L-4, mesmo veredito nos cinco sistemas) e qualquer
    componente vazio/'.'/'..' (C2, C4, C5, C15). Retorna None quando o
    nome nao e' resolvivel de jeito nenhum.
    """
    if "\\" in name:
        return None
    parts = name.split("/")
    for part in parts:
        if part in ("", ".", ".."):
            return None
    return parts


def _resolve_case_exact(base_dir, components):
    """Resolve `components` sob `base_dir`, um segmento por vez,
    comparando cada um EXATO (case-sensitive, mesmo em sistema de
    arquivos sem distincao de caixa - C17, conferido contra
    os.listdir(), nunca os.path.exists()). Nunca segue ligacao em
    nenhum passo - usa o MESMO classificador fechado de walk_layer_
    directory() (_classify_entry()/_kind_from_lstat(), D17: o
    os.path.islink() de antes era falso pra JUNCAO do Windows, CPython
    #67596 - o mesmo defeito que D11 consertou no percurso e esqueceu
    aqui, achado IMPORTANTE-2 do revisor). Retorna o caminho absoluto
    do ARQUIVO REGULAR final, ou None.
    """
    current = base_dir
    last = len(components) - 1
    for index, part in enumerate(components):
        try:
            entries = os.listdir(current)
        except OSError:
            return None
        if part not in entries:
            return None
        current = os.path.join(current, part)
        if _classify_entry(current) == "link":
            return None
        if index == last:
            if not os.path.isfile(current):
                return None
        else:
            if not os.path.isdir(current):
                return None
    return current


def _project_header_verdict(root, components):
    """`components[0]` e' sempre "glintfx" quando chamada. Resolve a
    excecao dos GERADOS (item 3) e a existencia real dentro de
    include/glintfx/<core|gfss|gfui>/... (item 2) - qualquer outra
    coisa sob glintfx/ (inclusive glintfx/platform/, C22) reprova: a
    regra que da nome ao portao passa a ser provada pela AUSENCIA na
    lista, nunca por uma agulha propria.
    """
    joined = "/".join(components)
    if joined in _GENERATED_HEADERS:
        return True
    if len(components) < 2 or components[1] not in _PROJECT_PURE_SUBLAYERS:
        return False
    rest = components[2:]
    if not rest:
        return False
    base = os.path.join(root, "include", "glintfx", components[1])
    return _resolve_case_exact(base, rest) is not None


def _relative_quote_verdict(root, including_dir, components):
    """Regra 4: `"x.hpp"`/`"sub/x.hpp"` resolvido primeiro contra a
    pasta do arquivo que inclui, depois contra as duas raizes de
    inclusao do projeto (src/, include/ - as mesmas -I do build real,
    src/gfui/CMakeLists.txt:63 e cmake/GlintfxLibrary.cmake:153). O
    arquivo resolvido precisa cair DENTRO de uma das seis camadas
    puras - senao uma inclusao relativa poderia escapar para
    platform/ e ainda passar.
    """
    search_roots = (
        including_dir,
        os.path.join(root, "src"),
        os.path.join(root, "include"),
    )
    pure_layer_paths = tuple(
        os.path.normpath(os.path.join(root, *parts)) for _label, parts in _PURE_LAYER_DIR_SPECS
    )
    for base in search_roots:
        resolved = _resolve_case_exact(base, components)
        if resolved is None:
            continue
        resolved_norm = os.path.normpath(resolved)
        if any(
            resolved_norm == layer or resolved_norm.startswith(layer + os.sep)
            for layer in pure_layer_paths
        ):
            return True
    return False


def classify_header_argument(argument_literal, is_quoted, root, including_dir):
    """D-1: lista de permitidos FECHADA, nomes comparados EXATOS e com
    distincao de caixa (mesmo veredito nos cinco sistemas). Retorna
    True (permitido) ou False (reprova) - nunca da beneficio da
    duvida a forma que nao bate com nenhuma das quatro regras.
    """
    if "/" not in argument_literal and argument_literal in _STDLIB_ALLOWED_NAMES:
        return True
    components = _split_header_path(argument_literal)
    if components is not None and components and components[0] == "glintfx":
        return _project_header_verdict(root, components)
    if is_quoted and components is not None:
        if _relative_quote_verdict(root, including_dir, components):
            return True
    return False


# --- familia D: entradas da arvore (percurso proprio) ------------------


def _kind_from_lstat(mode, file_attributes):
    """Parte PURA de _classify_entry() abaixo: decide "dir"/"file"/
    "link"/"other" a partir do MODO e dos atributos de arquivo do
    Windows ja' obtidos - nunca chama os.lstat() sozinha, quem chama
    decide de onde os bytes vem, reais ou SINTETICOS (D16, achado
    IMPORTANTE-3 do revisor: o bloco de juncao do Windows nunca tinha
    prova fora de um CI Windows real - com esta divisao, o mesmo
    julgamento roda em QUALQUER sistema com dois registros de teste,
    porque stat.FILE_ATTRIBUTE_REPARSE_POINT existe no Python do Linux
    tambem, so' nunca e' setado de verdade por os.lstat() la').
    """
    if stat.S_ISLNK(mode):
        return "link"
    reparse_point = getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0)
    if file_attributes is not None and reparse_point and (file_attributes & reparse_point):
        return "link"
    if stat.S_ISDIR(mode):
        return "dir"
    if stat.S_ISREG(mode):
        return "file"
    return "other"


def _classify_entry(path):
    """Enumeracao fechada de quatro tipos: "dir", "file", "link"
    (ligacao simbolica POSIX OU ponto de nova analise do Windows -
    juncao/reparse point) ou "other" (FIFO, dispositivo, soquete).
    "missing" quando a entrada nao existe (corrida rara entre listar e
    stat). Usa os.lstat() SEMPRE - nunca segue a propria entrada."""
    try:
        st = os.lstat(path)
    except OSError:
        return "missing"
    return _kind_from_lstat(st.st_mode, getattr(st, "st_file_attributes", None))


def _classify_source_filename(name):
    if name in _KNOWN_NON_SOURCE_FILENAMES:
        return "skip"
    if name.lower().endswith(_SOURCE_EXTENSIONS):
        return "source"
    return "unknown"


def _bucket_one_entry(result, stack, entry):
    """Classifica UMA entrada do scandir e a poe no balde certo (ou
    empilha, se for pasta a descer). Um passo do laco de
    walk_layer_directory() abaixo, fatorado pra manter as duas funcoes
    sob o teto de linhas (L-17)."""
    path = entry.path
    kind = _classify_entry(path)
    if kind == "dir":
        stack.append(path)
    elif kind == "link":
        result["link_paths"].append(path)
    elif kind == "file":
        verdict = _classify_source_filename(entry.name)
        if verdict == "skip":
            result["skipped_non_source"].append(path)
        elif verdict == "source":
            result["source_files"].append(path)
        else:
            result["unknown_type_files"].append(path)
    else:
        result["other_entry_paths"].append(path)


def walk_layer_directory(root_dir):
    """Percurso proprio de UMA camada pura (via os.scandir + os.lstat -
    NUNCA os.walk(), que segue juncao do Windows mesmo com
    followlinks=False - CPython #67596, aberto). NUNCA desce numa
    entrada classificada "link": laco e' impossivel por construcao, e
    o conteudo por tras dela fica invisivel de proposito (a decisao
    SYMLINK-DIR-REFUSED herdada de L-3: reprovar a PRESENCA, sem
    seguir). Retorna um dict com cinco listas: arquivos-fonte
    (varridos por violacao), pulados-conhecidos (CMakeLists.txt,
    contados mas nunca varridos), arquivos de tipo desconhecido
    (reprovam por presenca), ligacoes (reprovam por presenca) e
    "outras" entradas - FIFO/dispositivo/soquete (reprovam por
    presenca, NUNCA abertas: abrir um FIFO trava).
    """
    result = {
        "source_files": [],
        "skipped_non_source": [],
        "unknown_type_files": [],
        "link_paths": [],
        "other_entry_paths": [],
    }
    stack = [root_dir]
    while stack:
        current = stack.pop()
        try:
            entries = sorted(os.scandir(current), key=lambda dir_entry: dir_entry.name)
        except OSError:
            continue
        for entry in entries:
            _bucket_one_entry(result, stack, entry)
    return result


def scan_pure_layer_dirs(root):
    """Classifica as seis camadas puras, uma por uma (nunca em
    agregado). Retorna {label: (state, payload)}: state="missing"
    (nao existe, payload=None); state="root_is_link" (a PROPRIA raiz
    da camada e' uma ligacao/juncao, payload=caminho - D7); state=
    "root_not_a_dir" (existe mas nao e' pasta nem ligacao - FIFO/
    dispositivo na raiz, payload=caminho); ou state="ok" (payload=dict
    de walk_layer_directory()).
    """
    result = {}
    for label, parts in _PURE_LAYER_DIR_SPECS:
        path = os.path.join(root, *parts)
        kind = _classify_entry(path)
        if kind == "missing":
            result[label] = ("missing", None)
        elif kind == "link":
            result[label] = ("root_is_link", path)
        elif kind != "dir":
            result[label] = ("root_not_a_dir", path)
        else:
            result[label] = ("ok", walk_layer_directory(path))
    return result


# --- familia E: codificacao -------------------------------------------


def _decode_by_bom(raw):
    """Decodifica `raw` pela marca de ordem de bytes - o compilador da
    Microsoft aceita UTF-16 LE/BE (com ou sem marca) e UTF-8 (com
    marca), e este projeto nao passa /utf-8 nem /source-charset
    (docs/plano-layers-l4.md §1.2/§2.4) - g++/clang++ desta maquina
    recusam UTF-16, entao so' UTF-8/UTF-8-com-marca sao confirmaveis
    aqui; o resto vem de documentacao, nao medicao (criterio 6). UTF-32
    (que a Microsoft NAO documenta aceitar) reprova por si so', testado
    ANTES de UTF-16 (as marcas compartilham os dois primeiros bytes).
    Retorna (texto, None) no sucesso, ou (None, motivo) na reprovacao.
    """
    if raw[:4] in (b"\x00\x00\xfe\xff", b"\xff\xfe\x00\x00"):
        return None, "codificacao nao verificavel (UTF-32 - so UTF-16/UTF-8 sao aceitas)"
    if raw[:2] == b"\xff\xfe":
        try:
            return raw[2:].decode("utf-16-le", errors="strict"), None
        except UnicodeDecodeError as exc:
            return None, f"codificacao nao verificavel (UTF-16 LE invalido: {exc})"
    if raw[:2] == b"\xfe\xff":
        try:
            return raw[2:].decode("utf-16-be", errors="strict"), None
        except UnicodeDecodeError as exc:
            return None, f"codificacao nao verificavel (UTF-16 BE invalido: {exc})"
    return raw.decode("utf-8-sig", errors="replace"), None


def _read_source_text(path):
    """Le `path` em BYTES, decodifica via _decode_by_bom() e recusa
    caractere nulo depois de decodificar por QUALQUER caminho - e' o
    sinal de UTF-16 sem marca que a Microsoft aceitaria e este portao
    nao consegue decodificar sem ambiguidade. Retorna (texto, None) no
    sucesso, ou (None, motivo) na reprovacao.
    """
    try:
        with open(path, "rb") as handle:
            raw = handle.read()
    except OSError as exc:
        return None, f"open refused ({exc})"

    text, encoding_error = _decode_by_bom(raw)
    if encoding_error is not None:
        return None, encoding_error

    if "\x00" in text:
        return None, (
            "caractere nulo: possivel UTF-16 sem marca, que o compilador "
            "da Microsoft aceita (GODS_LAWS.md L-4, familia E)"
        )
    return text, None


# --- juncao dos escaneamentos por arquivo ------------------------------


# Agrupa os tres dados que TODA resolucao de cabecalho precisa (a
# tabela de macros do arquivo, a raiz do projeto, a pasta de quem
# inclui), pra nenhuma funcao de julgamento de linha passar de 4
# parametros (L-17) so' porque carrega esse contexto adiante.
_ScanContext = collections.namedtuple("_ScanContext", "macros root including_dir")


def _scan_one_logical_line(lineno, segment, ctx):
    """Julga UMA linha logica pela enumeracao fechada de nomes de
    diretiva (familia B): `pragma` tem verificacao propria de conteudo
    (D-A3); `include`/`include_next` resolvem cabecalho (familia A+C);
    as 13 outras sao permitidas sem efeito; qualquer outro nome
    reprova. O token `import` isolado NAO e' mais reconhecido aqui -
    D-A1 (escaneamento de token no arquivo INTEIRO, scan_file_
    directives() abaixo) cobre TODA forma de `import`/`module`/
    `_Pragma`/`__pragma`/`asm`, nao so' a diretiva de uma linha
    (GODS_LAWS.md projeto L-4/L-67: `_IMPORT_DIRECTIVE_PATTERN` e a
    busca textual de `include_alias` saem, o token e' quem cobre).
    Devolve (kind, linha, detail) ou None (linha limpa)."""
    directive_match = _DIRECTIVE_NAME_PATTERN.match(segment)
    if not directive_match:
        return None
    name = directive_match.group(1)
    if name is None:
        return None
    if name == "pragma":
        if _pragma_is_allowed(segment):
            return None
        return ("directive", lineno, "pragma nao permitido em camada pura")
    if name in _INCLUSION_DIRECTIVE_NAMES:
        argument_text = segment[directive_match.end() :].strip()
        verdict = _evaluate_include_argument(argument_text, ctx.macros, ctx.root, ctx.including_dir)
        if verdict != "pass":
            return (verdict, lineno, None)
        return None
    if name in _ALLOWED_OTHER_DIRECTIVE_NAMES:
        return None
    return ("directive", lineno, None)


def scan_file_directives(text, root, including_dir):
    """Retorna a lista de (kind, linha, detail) de um arquivo ja
    decodificado, kind em {"policy", "computed", "directive", "token",
    "lexical"}. Primeiro a fase 1 (_normalize_newlines(), CONT-4C: toda
    quebra de linha fisica medida vira '\\n' unico); depois as tres
    regras lexicas da fase 2+3 (D-A4, numero de pre-processamento/nome
    de cabecalho/literal nao terminado - _translate_phases_2_and_3());
    depois o escaneamento de TOKEN proibido no arquivo INTEIRO (D-A1,
    _forbidden_token_violations()); depois, por linha logica: a
    colagem que pode formar um token proibido (D-A2, so' em linhas
    `#define` - _paste_chain_violations()) e a enumeracao fechada de
    nomes de diretiva (familia B, _scan_one_logical_line() acima).
    """
    phase = _translate_phases_2_and_3(_normalize_newlines(text))
    logical = list(_split_logical_lines(phase.clean_text, phase.clean_lines))
    ctx = _ScanContext(macros=_collect_macro_multimap(logical), root=root, including_dir=including_dir)
    violations = [("lexical", lineno, reason) for reason, lineno in phase.violations]
    violations.extend(
        ("token", lineno, reason)
        for reason, lineno in _forbidden_token_violations(phase.clean_text, phase.clean_lines, phase.excluded)
    )
    for lineno, segment in logical:
        violations.extend(("token", ln, reason) for reason, ln in _paste_chain_violations(segment, lineno))
        verdict = _scan_one_logical_line(lineno, segment, ctx)
        if verdict is not None:
            violations.append(verdict)
    return violations


def violations_in_file(path, root):
    """Cada item de `violations` e' (kind, path, lineno, detail). kind
    em {"policy", "computed", "directive", "token", "lexical",
    "encoding"}. `detail` carrega texto para "encoding" (a mensagem
    ja' e' o motivo inteiro, nao ha "linha" significativa - lineno
    fica 1), "token" e "lexical" (sempre) e "directive" (so' quando o
    motivo generico "diretiva nao permitida" nao basta, ex.: pragma).
    """
    text, encoding_violation = _read_source_text(path)
    if encoding_violation is not None:
        return [("encoding", path, 1, encoding_violation)]
    including_dir = os.path.dirname(path)
    directive_violations = scan_file_directives(text, root, including_dir)
    return [(kind, path, lineno, detail) for kind, lineno, detail in directive_violations]


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


def _report_structural_state(scan):
    """Fase 1: toda camada presente e' de fato uma pasta, nunca uma
    ligacao (D7). Imprime cada falha e devolve True so' quando as seis
    estao estruturalmente sas."""
    ok = True
    for label, (state, payload) in scan.items():
        if state == "missing":
            print(
                f"{SCRIPT_NAME}: varredura vazia ({label} nao existe) - GODS_LAWS.md L-40",
                file=sys.stderr,
            )
            ok = False
        elif state in ("root_is_link", "root_not_a_dir"):
            print(
                f"{SCRIPT_NAME}: ligacao simbolica dentro de camada pura "
                f"(nao seguida): {payload} - GODS_LAWS.md L-02",
                file=sys.stderr,
            )
            ok = False
    return ok


def _report_nonempty_floor(scan):
    """Fase 2: toda camada presente tem pelo menos um arquivo-fonte
    (piso L-40, POR CAMADA, as seis, nunca agregado)."""
    ok = True
    for label, (_state, payload) in scan.items():
        if len(payload["source_files"]) == 0:
            print(
                f"{SCRIPT_NAME}: varredura vazia (0 arquivos em {label}) - GODS_LAWS.md L-40",
                file=sys.stderr,
            )
            ok = False
    return ok


def _collect_scan_violations(scan, root):
    """Fase 3: toda entrada problematica (ligacao, tipo desconhecido,
    "outro") E toda violacao de conteudo dos arquivos-fonte. Retorna
    (violations, scanned_count, skipped_count)."""
    violations = []
    skipped_count = 0
    scanned_count = 0
    entry_reasons = (
        ("link_paths", "ligacao simbolica dentro de camada pura (nao seguida)"),
        ("unknown_type_files", "arquivo de tipo desconhecido em camada pura"),
        ("other_entry_paths", "entrada que nao e arquivo regular nem pasta (nao aberta)"),
    )
    for _label, (_state, payload) in scan.items():
        skipped_count += len(payload["skipped_non_source"])
        for bucket, reason in entry_reasons:
            for path in payload[bucket]:
                violations.append(("entry", path, 0, reason))
        for source_path in payload["source_files"]:
            scanned_count += 1
            violations.extend(violations_in_file(source_path, root))
    return violations, scanned_count, skipped_count


def _print_violations(violations):
    print(f"{SCRIPT_NAME}: layer violations (GODS_LAWS.md L-19):", file=sys.stderr)
    for kind, path, lineno, detail in violations:
        if kind == "computed":
            print(
                f"{path}:{lineno}: inclusao computada nao verificavel (GODS_LAWS.md L-02)",
                file=sys.stderr,
            )
        elif kind == "directive":
            print(f"{path}:{lineno}: {detail or 'diretiva nao permitida em camada pura'}", file=sys.stderr)
        elif kind in ("token", "lexical"):
            print(f"{path}:{lineno}: {detail}", file=sys.stderr)
        elif kind in ("encoding", "entry"):
            print(f"{path}: {detail}", file=sys.stderr)
        else:  # "policy"
            print(f"{path}:{lineno}", file=sys.stderr)


def check_layers(root):
    """Logica real do portao, fatorada pra --selftest exercitar a
    MESMA funcao (nunca uma reimplementacao que pudesse divergir).
    Tres fases, cada uma podendo terminar cedo - ver as tres funcoes
    _report_*/_collect_scan_violations acima, uma por fase (L-17)."""
    scan = scan_pure_layer_dirs(root)

    if not _report_structural_state(scan):
        return False
    if not _report_nonempty_floor(scan):
        return False

    violations, scanned_count, skipped_count = _collect_scan_violations(scan, root)
    if violations:
        _print_violations(violations)
        return False

    print(f"{SCRIPT_NAME}: violations: 0 in {scanned_count} files scanned, {skipped_count} skipped")
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


# --- fixtures e executor de tabela para --selftest ---------------------


def make_scratch_workdir():
    return tempfile.mkdtemp(prefix="glintfx-layers-selftest-", dir=os.environ.get("TMPDIR"))


def _write_bytes(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as handle:
        handle.write(data)


def _write_clean_file(path):
    _write_bytes(path, b"#include <cstdint>\n// clean layer file, no OS or upper-layer header\n")


def make_clean_fixture(root):
    for _label, parts in _PURE_LAYER_DIR_SPECS:
        _write_clean_file(os.path.join(root, *parts, "clean.hpp"))


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


class Case:
    """Um caso de --selftest: `content` e' BYTES (E precisa de bytes
    crus, nao texto - GODS_LAWS.md projeto L-4 §3, "toda fixture e'
    gravada em binario"). `verdict` e' um de "passes", "reproves_
    policy", "reproves_computed", "reproves_directive", "reproves_
    encoding". `message` e' a substring que a mensagem de reprovacao
    tem de conter - confere o MOTIVO certo, nao so' "reprovou"
    (docs/plano-layers-l4.md §3: "um caso que reprova pelo motivo
    errado esconde o mutante").
    """

    __slots__ = (
        "name", "content", "verdict", "message", "message_absent",
        "plant_dir", "plant_name", "extra_files", "also_expect",
    )

    # Campos opcionais recebidos por **opts (L-17: alem de 4 parametros
    # a assinatura reprova) - so' os tres primeiros (name/content/
    # verdict) sao obrigatorios; o resto tem default aqui, um lugar so'.
    # `also_expect` (adendo, familia L/C): tupla de `linha` ou
    # `(linha, substring)` - exige uma SEGUNDA violacao citando
    # `target:linha` (com `substring` na mensagem, se dado). E' o que
    # prova as duas defesas juntas em C24/C25/D17 e "lexical + policy"
    # em L6 (docs/plano-layers-l4-adendo.md §5, "expectativa multipla").
    _OPTION_DEFAULTS = {
        "message": None,
        "message_absent": None,
        "plant_dir": ("src", "core"),
        "plant_name": None,
        "extra_files": None,
        "also_expect": None,
    }

    def __init__(self, name, content, verdict, **opts):
        unknown = opts.keys() - Case._OPTION_DEFAULTS.keys()
        if unknown:
            raise TypeError(f"Case: opcao desconhecida {sorted(unknown)}")
        merged = {**Case._OPTION_DEFAULTS, **opts}

        self.name = name
        self.content = content if isinstance(content, bytes) else content.encode("utf-8")
        self.verdict = verdict
        message = merged["message"]
        if message is None and verdict == "reproves_computed":
            # Distingue reprovacao por COMPUTADA da reprovacao por
            # POLITICA - as duas reprovam (outcome.result False), mas
            # so' a mensagem certa prova que a razao e' a certa (L-3,
            # herdado; achado pela propria bateria de mutacao de L-4:
            # M-A5 sobrevivia sem esta checagem, porque um #include
            # cujo nome nao resolve por macro E' rejeitado pela
            # politica de nome de qualquer jeito, escondendo a falta
            # da checagem de contaminacao por tras de uma reprovacao
            # com o motivo errado).
            message = "inclusao computada nao verificavel"
        self.message = message
        self.message_absent = merged["message_absent"]
        self.plant_dir = merged["plant_dir"]
        self.plant_name = merged["plant_name"] or f"{name}.cpp"
        self.extra_files = merged["extra_files"] or {}
        self.also_expect = merged["also_expect"] or ()


def _plant_case_fixture(scratch, family_label, case):
    """Constroi a fixture de UM caso (pasta limpa + arquivos extras +
    o arquivo plantado) e devolve o caminho do arquivo plantado."""
    root = os.path.join(scratch, f"{family_label}_{case.name}")
    make_clean_fixture(root)
    for rel_path, data in case.extra_files.items():
        _write_bytes(os.path.join(root, *rel_path.split("/")), data)
    target = os.path.join(root, *case.plant_dir, case.plant_name)
    _write_bytes(target, case.content)
    return root, target


def _judge_case_outcome(label, case, target, outcome):
    """Confere o veredito de UM caso contra a saida capturada de
    check_layers(). Devolve (ok, motivo_se_falhou_ou_None)."""
    reproved = not outcome.result
    should_reprove = case.verdict != "passes"

    if should_reprove and not reproved:
        return False, "deveria ter reprovado, mas passou"
    if not should_reprove and reproved:
        return False, "deveria ter passado, mas reprovou"
    if should_reprove and target not in outcome.text:
        return False, f"reprovou, mas nao citou {target}"
    if should_reprove and case.message and case.message not in outcome.text:
        return False, f"reprovou, mas sem a mensagem esperada {case.message!r}"
    if should_reprove and case.message_absent and case.message_absent in outcome.text:
        # O inverso do check acima: prova que reprovou pelo motivo
        # CERTO ao exigir a AUSENCIA do motivo de uma familia vizinha -
        # necessario quando o veredito esperado ("reproves_policy") nao
        # tem texto proprio distintivo (a linha e' so' "path:linha",
        # sem sufixo), entao so' a ausencia do texto de uma reprovacao
        # "entry"/"encoding" prova que NAO foi essa a rota tomada
        # (achado pela propria bateria de mutacao de L-4: M-D12-D13 e
        # M-E1 sobreviviam sem esta checagem).
        return False, f"reprovou pelo motivo errado - {case.message_absent!r} nao deveria aparecer"
    if should_reprove and case.also_expect:
        for entry in case.also_expect:
            lineno, needle = entry if isinstance(entry, tuple) else (entry, None)
            marker = f"{target}:{lineno}"
            if marker not in outcome.text:
                return False, f"reprovou, mas faltou a segunda violacao esperada em {marker!r}"
            if needle and needle not in outcome.text:
                return False, f"reprovou, mas faltou o texto also_expect {needle!r}"
    return True, None


def run_case_table(scratch, capture, family_label, cases):
    """Executor de tabela unico (GODS_LAWS.md projeto L-4 §3, "um
    executor de tabela substitui todas as sete copias do mesmo
    laco"). Retorna (ok, count)."""
    ok = True
    for case in cases:
        root, target = _plant_case_fixture(scratch, family_label, case)
        outcome = capture(lambda: check_layers(root))
        label = f"{family_label}:{case.name}"
        passed, reason = _judge_case_outcome(label, case, target, outcome)
        if not passed:
            print(f"selftest: {label} FALHOU ({reason})", file=sys.stderr)
            print(outcome.text, file=sys.stderr)
            ok = False
            continue
        verdict_word = "reprovado" if case.verdict != "passes" else "passou"
        print(f"selftest: {label} OK ({verdict_word})")
    return ok, len(cases)


# --- familia A: tabela de macros (achado critico) ----------------------

_FAMILY_A_CASES = (
    Case(
        "A1_if1_forbidden_first",
        "#if 1\n#define HDR <fstream>\n#else\n#define HDR <cstdint>\n#endif\n#include HDR\n",
        "reproves_policy",
    ),
    Case(
        "A2_if0_forbidden_second",
        "#if 0\n#define HDR <cstdint>\n#else\n#define HDR <fstream>\n#endif\n#include HDR\n",
        "reproves_policy",
    ),
    Case(
        "A3_redefine_after_use",
        "#define HDR <fstream>\n#include HDR\n#undef HDR\n#define HDR <cstdint>\n",
        "reproves_policy",
    ),
    Case(
        "A4_chain_in_other_branch",
        "#if 0\n#define HDR <cstdint>\n#else\n#define OTHER <fstream>\n#define HDR OTHER\n#endif\n"
        "#include HDR\n",
        "reproves_computed",
    ),
    Case(
        "A5_identifier_inside_angle_body",
        "#define F fstream\n#define HDR <F>\n#include HDR\n",
        "reproves_computed",
    ),
    Case(
        "A6_std_name_redefined",
        "#define cstdint fstream\n#define HDR <cstdint>\n#include HDR\n",
        "reproves_computed",
    ),
    Case(
        "A7_two_clean_branches",
        "#if 1\n#define HDR <cstdint>\n#else\n#define HDR <cstddef>\n#endif\n#include HDR\n",
        "passes",
    ),
    Case(
        "A8_function_like_same_name",
        "#if 0\n#define HDR(x) <fstream>\n#else\n#define HDR <cstdint>\n#endif\n#include HDR\n",
        "reproves_computed",
    ),
)

# case_a..h de L-3 (COMPUTED-INCLUDE) - mantidos, nao migrados de
# forma, so' passam pelo executor comum agora.
_FAMILY_A_LEGACY_CASES = (
    Case(
        "legacy_a_object_macro_forbidden_resolves_and_bites",
        "#define HDR <fstream>\n#include HDR\n",
        "reproves_policy",
    ),
    Case(
        "legacy_b_object_macro_clean_resolves_and_passes",
        "#define HDR <cstdint>\n#include HDR\n",
        "passes",
    ),
    Case(
        "legacy_c_object_macro_quoted_body_no_forbidden_form",
        '#define HDR "cstdint"\n#include HDR\n',
        "passes",
    ),
    Case(
        "legacy_d_digraph_plus_object_macro_resolves_and_bites",
        "#define HDR <fstream>\n%:include HDR\n",
        "reproves_policy",
    ),
    Case(
        "legacy_e_no_macro_defined_anywhere",
        "#include SOME_UNDEFINED_MACRO\n",
        "reproves_computed",
    ),
    Case(
        "legacy_f_function_like_macro_never_resolved",
        "#define HDR(x) <fstream>\n#include HDR(1)\n",
        "reproves_computed",
    ),
    Case(
        "legacy_g_macro_body_not_a_literal_header_never_resolved",
        "#define HDR 42\n#include HDR\n",
        "reproves_computed",
    ),
    Case(
        "legacy_h_named_module_import_reproves_by_token",
        "import glintfx_demo_module;\n",
        "reproves_token",
        message="identificador 'import' proibido",
    ),
)


# --- familia B: nomes de diretiva --------------------------------------
#
# CRITERIO 6 (docs/plano-layers-l4.md): B5 (`#using`) e' forma do
# COMPILADOR DA MICROSOFT (C++/CLI) - g++/clang++ desta maquina nao tem
# esse comportamento pra confirmar contra, entao o rotulo e' "fonte:
# documentacao da Microsoft (<https://learn.microsoft.com/cpp/
# preprocessor/pragma-directives-and-the-pragma-keyword>), NAO MEDIDO
# neste portao" - nunca "confirmado". B8/B9 (`include_alias`,
# `_Pragma`/`__pragma`) da L-4 MIGRARAM para as familias H e F do
# adendo (docs/plano-layers-l4-adendo.md §5, nota de rodape da familia
# H): H3 e' o B8 antigo, F14 e' o B9 antigo - `_Pragma`/`__pragma` sao
# TOKEN proibido agora (D-A1), nao busca textual por `include_alias`.
_FAMILY_B_CASES = (
    Case("B1_include_next_clean", "#include_next <cstdint>\n", "passes"),
    Case("B2_include_next_forbidden", "#include_next <fstream>\n", "reproves_policy"),
    Case("B3_import_directive", "#import <fstream>\n", "reproves_directive", message="diretiva nao permitida"),
    Case(
        "B4_import_clean_still_refused",
        "#import <cstdint>\n",
        "reproves_directive",
        message="diretiva nao permitida",
    ),
    Case("B5_using_directive", "#using <mscorlib.dll>\n", "reproves_directive", message="diretiva nao permitida"),
    Case("B6_embed_directive", '#embed "x"\n', "reproves_directive", message="diretiva nao permitida"),
    Case("B10_pragma_once_passes", "#pragma once\n", "passes"),
)

# B7: um trecho valido para CADA um dos 14 nomes permitidos-sem-efeito.
# "pragma" usa `#pragma once\n` (D-A3 tornou pragma restritivo - o
# antigo `#pragma B7_unknown_pragma\n` reprovaria agora; a familia H
# testa pragma de verdade).
_FAMILY_B_ALLOWED_NAME_FIXTURES = (
    ("define", "#define B7_DEFINE_TEST 1\n"),
    ("undef", "#undef B7_NEVER_DEFINED\n"),
    ("if", "#if 1\n"),
    ("ifdef", "#ifdef B7_X\n"),
    ("ifndef", "#ifndef B7_X\n"),
    ("elif", "#elif 1\n"),
    ("elifdef", "#elifdef B7_X\n"),
    ("elifndef", "#elifndef B7_X\n"),
    ("else", "#else\n"),
    ("endif", "#endif\n"),
    ("line", "#line 1\n"),
    ("error", "#error B7 msg\n"),
    ("warning", "#warning B7 msg\n"),
    ("pragma", "#pragma once\n"),
)
_FAMILY_B7_CASES = tuple(
    Case(f"B7_allowed_name_{name}", content, "passes") for name, content in _FAMILY_B_ALLOWED_NAME_FIXTURES
)


def selftest_family_b7(scratch, capture):
    ok, count = run_case_table(scratch, capture, "B7", _FAMILY_B7_CASES)
    if len(_FAMILY_B_ALLOWED_NAME_FIXTURES) != len(_ALLOWED_OTHER_DIRECTIVE_NAMES):
        print(
            "selftest: B7 FALHOU (a tabela de nomes permitidos tem "
            f"{len(_FAMILY_B_ALLOWED_NAME_FIXTURES)} entradas, mas "
            f"_ALLOWED_OTHER_DIRECTIVE_NAMES tem {len(_ALLOWED_OTHER_DIRECTIVE_NAMES)} - "
            "algum nome ficou sem prova (M-B7)",
            file=sys.stderr,
        )
        ok = False
    return ok, count


# --- familia C: politica de nome ----------------------------------------
#
# CRITERIO 6: C6/C7 (a caixa de <Windows.h>/<WinUser.h>/<gl/GL.h> tanto
# faz pra ESTE portao, que sempre recusa esses nomes de qualquer jeito
# - a comparacao exata E' medida contra o sistema de arquivos real, ver
# _resolve_case_exact()) e C8 (docs MS aceita `\` como separador - NAO
# MEDIDO aqui, so' citado da fonte: <https://learn.microsoft.com/cpp/
# build/reference/utf-8-set-source-and-executable-character-sets-to-
# utf-8>) sao os dois casos desta familia com componente MSVC nao
# medido - o resto (C1..C22) e' medido contra o sistema de arquivos
# real e/ou contra g++/clang++ (ver docs/plano-layers-l4.md §0 e as
# fixtures de /var/tmp/glintfx-L4-lab/probe/, todas ja confirmadas
# pelo planejador antes desta implementacao).

_FAMILY_C_CASES = (
    Case("C1_quoted_forbidden_stdlib", '#include "fstream"\n', "reproves_policy"),
    Case("C2_dot_slash", "#include <./fstream>\n", "reproves_policy"),
    Case("C3_versioned_path", "#include <c++/16/fstream>\n", "reproves_policy"),
    Case("C4_abs_path", '#include "/usr/include/unistd.h"\n', "reproves_policy"),
    Case("C5_dotdot", "#include <bits/../fstream>\n", "reproves_policy"),
    Case("C6_windows_h_case", "#include <Windows.h>\n", "reproves_policy"),
    Case("C7a_winuser_case", "#include <WinUser.h>\n", "reproves_policy"),
    Case("C7b_gl_case", "#include <gl/GL.h>\n", "reproves_policy"),
    Case("C8_backslash", "#include <sys\\stat.h>\n", "reproves_policy"),
    Case("C9a_linux_input", "#include <linux/input.h>\n", "reproves_policy"),
    Case("C9b_pthread", "#include <pthread.h>\n", "reproves_policy"),
    Case("C9c_dirent", "#include <dirent.h>\n", "reproves_policy"),
    Case("C9d_poll", "#include <poll.h>\n", "reproves_policy"),
    Case("C10a_fstream_upper", "#include <FSTREAM>\n", "reproves_policy"),
    Case("C10b_cstdint_upper", "#include <CSTDINT>\n", "reproves_policy"),
    Case("C11a_angle_cstdint", "#include <cstdint>\n", "passes"),
    Case("C11b_quoted_cstdint", '#include "cstdint"\n', "passes"),
    Case("C13a_fstream_banned", "#include <fstream>\n", "reproves_policy"),
    Case("C13b_filesystem_banned", "#include <filesystem>\n", "reproves_policy"),
    Case(
        "C14_own_header_exists",
        "#include <glintfx/core/vec2.hpp>\n",
        "passes",
        extra_files={"include/glintfx/core/vec2.hpp": b"#pragma once\n"},
    ),
    Case(
        "C15_own_header_dotdot",
        "#include <glintfx/core/../platform/window.hpp>\n",
        "reproves_policy",
        extra_files={"include/glintfx/platform/window.hpp": b"#pragma once\n"},
    ),
    Case("C16_own_header_missing", "#include <glintfx/core/nao_existe.hpp>\n", "reproves_policy"),
    Case(
        # A camada ("core") esta certa - so' o NOME DO ARQUIVO diverge
        # na caixa. Isto exercita _resolve_case_exact() de verdade
        # (C17 com a camada errada, como em versoes anteriores desta
        # fixture, nunca sai do crivo mais cedo de
        # _project_header_verdict() - components[1] not in
        # _PROJECT_PURE_SUBLAYERS ja recusa antes de chegar no
        # listdir(), o que deixa a comparacao case-exact sem prova).
        "C17_own_header_wrong_case",
        "#include <glintfx/core/VEC2.hpp>\n",
        "reproves_policy",
        extra_files={"include/glintfx/core/vec2.hpp": b"#pragma once\n"},
    ),
    Case(
        "C18_relative_quote_sibling",
        '#include "irmao.hpp"\n',
        "passes",
        extra_files={"src/core/irmao.hpp": b"#pragma once\n"},
    ),
    Case("C19_relative_quote_missing", '#include "faltando.hpp"\n', "reproves_policy"),
    Case(
        "C20_relative_quote_via_src_root",
        '#include "gfss/anb_c20.hpp"\n',
        "passes",
        plant_dir=("src", "gfui"),
        extra_files={"src/gfss/anb_c20.hpp": b"#pragma once\n"},
    ),
    Case("C21a_generated_export", "#include <glintfx/export.hpp>\n", "passes"),
    Case("C21b_generated_version_macros", "#include <glintfx/version_macros.hpp>\n", "passes"),
    Case(
        "C22_glintfx_platform_reserved",
        "#include <glintfx/platform/window.hpp>\n",
        "reproves_policy",
        extra_files={"include/glintfx/platform/window.hpp": b"#pragma once\n"},
    ),
    # C23/C23b (revisao independente, IMPORTANTE-1): a fronteira de
    # _relative_quote_verdict() - inclusao relativa que resolve mas cai
    # FORA das seis camadas puras - nunca tinha caso proprio; C20 so'
    # prova resolucao bem-sucedida DENTRO do limite.
    Case(
        "C23_relative_quote_escapes_via_src_root",
        '#include "platform/window.hpp"\n',
        "reproves_policy",
        plant_dir=("src", "gfui"),
        plant_name="escape.hpp",
        extra_files={"src/platform/window.hpp": b"#pragma once\n"},
    ),
    Case(
        "C23b_relative_quote_escapes_via_include_root",
        '#include "glintfx_extra/x.hpp"\n',
        "reproves_policy",
        extra_files={"include/glintfx_extra/x.hpp": b"#pragma once\n"},
    ),
    # C26 (revisao independente, MENOR): `<glintfx/<sublayer>>` sem
    # componente nenhum depois - a guarda `if not rest: return False`
    # de _project_header_verdict() ja' existia e ja' funcionava, so'
    # nunca tinha prova (MUT1 do revisor).
    Case("C26a_bare_sublayer_core", "#include <glintfx/core>\n", "reproves_policy"),
    Case("C26b_bare_sublayer_gfss", "#include <glintfx/gfss>\n", "reproves_policy"),
)

_FAMILY_C12_CASES = tuple(
    Case(f"C12_std_{name.replace('+', 'plus')}", f"#include <{name}>\n", "passes")
    for name in sorted(_STDLIB_ALLOWED_NAMES)
)


# Total LITERAL, independente das duas tuplas acima - 86 (Table 24) +
# 21 (Table 25) - 2 (banidos) = 105, conferido em 23/09/2026 contra
# <https://timsong-cpp.github.io/cppwp/n4950/headers>. Comparar
# `_STDLIB_ALLOWED_NAMES` so' contra um valor DERIVADO das mesmas
# tuplas e' uma trava tautologica (memoria feedback_trava_tautologica.
# md): se um nome sumir das duas tuplas ao mesmo tempo (ex.: um define
# apagado sem querer, ou o proprio assert do topo do arquivo editado
# junto por engano), a comparacao contra si mesma nunca nota - so' um
# numero ESCRITO A PARTE, que nao deriva do que esta sendo conferido,
# pega isso.
_N4950_ALLOWED_HEADER_COUNT = 105


def selftest_family_c12(scratch, capture):
    ok, count = run_case_table(scratch, capture, "C12", _FAMILY_C12_CASES)
    derived_expected = len(_N4950_CXX_LIBRARY_HEADERS) + len(_N4950_C_COMPAT_HEADERS) - len(_STDLIB_BANNED_NAMES)
    print(
        f"selftest: C12 tabela N4950 - {len(_STDLIB_ALLOWED_NAMES)} nomes permitidos "
        f"(derivado das tuplas: {derived_expected}; literal fixo: {_N4950_ALLOWED_HEADER_COUNT})"
    )
    if len(_STDLIB_ALLOWED_NAMES) != derived_expected:
        print(
            "selftest: C12 FALHOU (contagem da lista permitida diverge do calculo "
            "feito das proprias duas tabelas - M-C12)",
            file=sys.stderr,
        )
        ok = False
    if len(_STDLIB_ALLOWED_NAMES) != _N4950_ALLOWED_HEADER_COUNT:
        print(
            "selftest: C12 FALHOU (contagem final diverge do numero LITERAL fixo "
            f"{_N4950_ALLOWED_HEADER_COUNT} - as duas tuplas transcritas da norma "
            "encolheram ou cresceram - M-C12)",
            file=sys.stderr,
        )
        ok = False
    return ok, count


# --- familia D: entradas da arvore --------------------------------------


def _make_dirty_external_dir(root, suffix):
    external_dir = os.path.join(root, f"_external_dirty_{suffix}")
    _write_bytes(os.path.join(external_dir, "dirty.hpp"), b"#include <wayland-client.h>\n")
    return external_dir


def _judge_symlink_reproval(tag, link_path, outcome):
    """Julgamento comum a D1-D9: reprovou, citou o CAMINHO da ligacao,
    nunca citou o conteudo por tras dela, e a mensagem e' de PRESENCA
    ("ligacao simbolica"), nunca uma reprovacao de conteudo comum que
    por acaso citaria o mesmo caminho (mesmo path, motivo errado -
    achado pela propria bateria de mutacao: M-D8-D9 sobrevivia sem
    esta ultima checagem, porque abrir o alvo por engano tambem
    reprova). Devolve (ok, mensagem)."""
    if outcome.result:
        return False, f"{tag} FALHOU (ligacao deveria ter reprovado, mas passou)"
    if link_path not in outcome.text:
        return False, f"{tag} FALHOU (reprovou, mas nao citou {link_path})"
    if "dirty.hpp" in outcome.text or "wayland-client" in outcome.text:
        return False, f"{tag} FALHOU (citou o conteudo por tras da ligacao - seguiu em vez de recusar)"
    if "ligacao simbolica" not in outcome.text:
        return False, f"{tag} FALHOU (reprovou, mas sem a mensagem de presenca 'ligacao simbolica')"
    return True, f"{tag} OK (reprovado por presenca, sem seguir o conteudo)"


def selftest_family_d1_d6_symlinked_dirs(scratch, capture):
    """D1..D6: uma pasta-atalho apontando pra fora, plantada em CADA
    uma das seis camadas puras (fixture propria por pasta - um laco
    quebrado nao se esconde atras de outro que ainda funciona, o
    mutante do revisor de L-3 que apagava so' o segundo dos dois
    lacos antigos)."""
    ok = True
    count = 0
    for label, parts in _PURE_LAYER_DIR_SPECS:
        safe_label = label.replace("/", "_")
        root = os.path.join(scratch, f"D_dir_{safe_label}")
        make_clean_fixture(root)
        external_dir = _make_dirty_external_dir(root, safe_label)
        link_path = os.path.join(root, *parts, "linked")
        os.symlink(external_dir, link_path, target_is_directory=True)

        outcome = capture(lambda: check_layers(root))
        count += 1
        passed, message = _judge_symlink_reproval(f"D1-D6:{label}", link_path, outcome)
        print(f"selftest: {message}", file=sys.stderr if not passed else sys.stdout)
        ok = ok and passed
    return ok, count


def selftest_family_d7_root_symlink(scratch, capture):
    """D7: a PROPRIA raiz de uma camada (src/gfss, escolha arbitraria)
    e' ela mesma a ligacao."""
    root = os.path.join(scratch, "D7_root_is_link")
    make_clean_fixture(root)
    gfss_root = os.path.join(root, "src", "gfss")
    external_dir = _make_dirty_external_dir(root, "d7")
    shutil.rmtree(gfss_root)
    os.symlink(external_dir, gfss_root, target_is_directory=True)

    outcome = capture(lambda: check_layers(root))
    passed, message = _judge_symlink_reproval("D7", gfss_root, outcome)
    print(f"selftest: {message}", file=sys.stderr if not passed else sys.stdout)
    return passed, 1


_D8_D9_TARGET_MAKERS = (
    ("D8_valid_file_shortcut", lambda root: _make_dirty_external_dir(root, "d8") + "/dirty.hpp"),
    ("D9_broken_file_shortcut", lambda root: os.path.join(root, "_does_not_exist_d9.hpp")),
)


def selftest_family_d8_d9_file_shortcuts(scratch, capture):
    """D8: atalho de ARQUIVO valido apontando pra fora. D9: atalho de
    arquivo QUEBRADO (aponta pra alvo inexistente) - antes desta
    fatia isso caia no `except OSError` de violations_in_file() e
    devolvia lista vazia (silencioso); agora nem chega a abrir,
    porque a classificacao de entrada pega "link" antes de
    violations_in_file() ser chamada."""
    ok = True
    count = 0
    for name, make_target in _D8_D9_TARGET_MAKERS:
        root = os.path.join(scratch, name)
        make_clean_fixture(root)
        link_path = os.path.join(root, "src", "core", "shortcut.hpp")
        os.symlink(make_target(root), link_path)

        outcome = capture(lambda: check_layers(root))
        count += 1
        passed, message = _judge_symlink_reproval(name, link_path, outcome)
        print(f"selftest: {message}", file=sys.stderr if not passed else sys.stdout)
        ok = ok and passed
    return ok, count


def selftest_family_d10_fifo(scratch, capture):
    """D10: FIFO chamado x.hpp (POSIX). Nao existe no Windows (NTFS
    nao tem FIFO) - GODS_LAWS.md projeto L-4 §3, "ausencia declarada e
    contada, nunca pulo calado"."""
    if not hasattr(os, "mkfifo"):
        print("selftest: D10 nao aplicavel neste sistema (sem os.mkfifo)")
        return True, 1
    root = os.path.join(scratch, "D10_fifo")
    make_clean_fixture(root)
    fifo_path = os.path.join(root, "src", "core", "pipe.hpp")
    os.mkfifo(fifo_path)

    outcome = capture(lambda: check_layers(root))
    reproved = not outcome.result
    if not reproved:
        print("selftest: D10 FALHOU (FIFO deveria ter reprovado, mas passou)", file=sys.stderr)
        return False, 1
    if fifo_path not in outcome.text:
        print(f"selftest: D10 FALHOU (reprovou, mas nao citou {fifo_path})", file=sys.stderr)
        return False, 1
    print("selftest: D10 OK (FIFO reprovado por presenca, sem abrir)")
    return True, 1


def _mklink_junction(link_path, external_dir):
    """Unica chamada de processo permitida nesta fatia (docs/plano-
    layers-l4.md §5), com teto de tempo - compartilhada por D11/D17
    (L-17: extrair evita duplicar as mesmas 6 linhas duas vezes).
    Devolve None no sucesso, ou a mensagem de erro."""
    import subprocess

    completed = subprocess.run(
        ["cmd", "/c", "mklink", "/J", link_path, external_dir],
        capture_output=True,
        timeout=30,
        text=True,
    )
    if completed.returncode != 0:
        return f"mklink /J recusado: {completed.stderr.strip()}"
    return None


def selftest_family_d11_junction(scratch, capture):
    """D11: juncao do Windows apontando pra fora. Nao existe no
    Linux/macOS - declarado e contado, nunca pulo calado."""
    if sys.platform != "win32":
        print("selftest: D11 nao aplicavel neste sistema (juncao e' feature do Windows)")
        return True, 1
    root = os.path.join(scratch, "D11_junction")
    make_clean_fixture(root)
    external_dir = _make_dirty_external_dir(root, "d11")
    link_path = os.path.join(root, "src", "gfui", "junction")
    mklink_error = _mklink_junction(link_path, external_dir)
    if mklink_error is not None:
        print(f"selftest: D11 FALHOU ({mklink_error})", file=sys.stderr)
        return False, 1

    outcome = capture(lambda: check_layers(root))
    reproved = not outcome.result
    if not reproved:
        print("selftest: D11 FALHOU (juncao deveria ter reprovado, mas passou)", file=sys.stderr)
        return False, 1
    if link_path not in outcome.text:
        print(f"selftest: D11 FALHOU (reprovou, mas nao citou {link_path})", file=sys.stderr)
        return False, 1
    print("selftest: D11 OK (juncao reprovada por presenca, sem seguir)")
    return True, 1


def selftest_family_d16_kind_from_lstat(scratch, capture):
    """D16 (revisao independente, IMPORTANTE-3): prova _kind_from_
    lstat() DIRETO, com dois registros sinteticos (pasta com/sem
    FILE_ATTRIBUTE_REPARSE_POINT) - roda em TODO sistema, porque o
    atributo existe no Python do Linux tambem (0x400), so' nunca e'
    setado de verdade por os.lstat() la' - antes desta divisao, o
    bloco de juncao do Windows so' tinha prova rodando um CI Windows
    real."""
    del scratch, capture
    reparse = getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400)
    dir_mode = stat.S_IFDIR | 0o755
    ok = True
    with_reparse = _kind_from_lstat(dir_mode, reparse)
    if with_reparse != "link":
        print(
            f"selftest: D16 FALHOU (pasta com REPARSE_POINT deveria ser 'link', foi {with_reparse!r})",
            file=sys.stderr,
        )
        ok = False
    without_reparse = _kind_from_lstat(dir_mode, 0)
    if without_reparse != "dir":
        print(
            f"selftest: D16 FALHOU (pasta sem REPARSE_POINT deveria ser 'dir', foi {without_reparse!r})",
            file=sys.stderr,
        )
        ok = False
    if ok:
        print("selftest: D16 OK (_kind_from_lstat julga juncao pelo atributo, em qualquer sistema)")
    return ok, 1


def selftest_family_d17_junction_relative_include(scratch, capture):
    """D17 (revisao independente, IMPORTANTE-2, Windows apenas): juncao
    no MEIO do caminho de uma inclusao relativa - o mesmo defeito que
    D11 consertou no percurso e esqueceu na resolucao (_resolve_case_
    exact usava os.path.islink(), falso pra juncao - CPython #67596).
    Nao existe no Linux/macOS - declarado e contado."""
    if sys.platform != "win32":
        print("selftest: D17 nao aplicavel neste sistema (juncao e' feature do Windows)")
        return True, 1
    root = os.path.join(scratch, "D17_junction_relative")
    make_clean_fixture(root)
    external_dir = _make_dirty_external_dir(root, "d17")
    link_path = os.path.join(root, "src", "core", "sub")
    mklink_error = _mklink_junction(link_path, external_dir)
    if mklink_error is not None:
        print(f"selftest: D17 FALHOU ({mklink_error})", file=sys.stderr)
        return False, 1
    target = os.path.join(root, "src", "core", "a.cpp")
    _write_bytes(target, b'#include "sub/x.hpp"\n')

    outcome = capture(lambda: check_layers(root))
    passed, message = _judge_double_reproval("D17", link_path, target, outcome)
    print(f"selftest: {message}", file=sys.stdout if passed else sys.stderr)
    return passed, 1


def _judge_double_reproval(tag, link_path, target, outcome):
    """Julgamento comum a C24/C25: reprovou, citou a LIGACAO (entrada
    por presenca) E citou o alvo (politica na linha do #include) -
    achado IMPORTANTE-2 do revisor, as duas defesas juntas."""
    if outcome.result:
        return False, f"{tag} FALHOU (deveria ter reprovado, mas passou)"
    if link_path not in outcome.text:
        return False, f"{tag} FALHOU (nao citou a ligacao {link_path})"
    if target not in outcome.text:
        return False, f"{tag} FALHOU (nao citou {target})"
    return True, f"{tag} OK (ligacao E politica reprovadas juntas)"


def selftest_family_c24_relative_quote_via_symlink(scratch, capture):
    """C24 (revisao independente, IMPORTANTE-2): ligacao simbolica no
    MEIO do caminho de uma inclusao RELATIVA (aspas), apontando pra
    fora, com a pasta externa LIMPA de proposito (nenhuma outra
    violacao cobre a linha do #include - so' a ligacao E' a defesa).
    Prova as DUAS coisas juntas (also_expect nao serve aqui - precisa
    de os.symlink(), que Case/extra_files nao fazem)."""
    root = os.path.join(scratch, "C24_relative_quote_symlink")
    make_clean_fixture(root)
    external_dir = os.path.join(root, "_external_clean_c24")
    _write_bytes(os.path.join(external_dir, "x.hpp"), b"#pragma once\n")
    link_path = os.path.join(root, "src", "core", "sub")
    os.symlink(external_dir, link_path, target_is_directory=True)
    target = os.path.join(root, "src", "core", "a.cpp")
    _write_bytes(target, b'#include "sub/x.hpp"\n')
    outcome = capture(lambda: check_layers(root))
    passed, message = _judge_double_reproval("C24", link_path, target, outcome)
    print(f"selftest: {message}", file=sys.stdout if passed else sys.stderr)
    if not passed:
        print(outcome.text, file=sys.stderr)
    return passed, 1


def selftest_family_c25_project_header_via_symlink(scratch, capture):
    """C25 (revisao independente, IMPORTANTE-2): mesmo defeito de C24,
    pela raiz de PROJETO (`<glintfx/core/...>`) em vez de aspas."""
    root = os.path.join(scratch, "C25_project_header_symlink")
    make_clean_fixture(root)
    external_dir = os.path.join(root, "_external_clean_c25")
    _write_bytes(os.path.join(external_dir, "x.hpp"), b"#pragma once\n")
    link_path = os.path.join(root, "include", "glintfx", "core", "sub")
    os.symlink(external_dir, link_path, target_is_directory=True)
    target = os.path.join(root, "src", "core", "a.cpp")
    _write_bytes(target, b"#include <glintfx/core/sub/x.hpp>\n")
    outcome = capture(lambda: check_layers(root))
    passed, message = _judge_double_reproval("C25", link_path, target, outcome)
    print(f"selftest: {message}", file=sys.stdout if passed else sys.stderr)
    if not passed:
        print(outcome.text, file=sys.stderr)
    return passed, 1


_FAMILY_D_EXTENSION_CASES = (
    Case(
        "D12_uppercase_extension",
        b"#include <fstream>\n",
        "reproves_policy",
        message_absent="tipo desconhecido",
        plant_name="DIRTY.HPP",
    ),
    Case("D13a_ext_C_upper", b"#include <fstream>\n", "reproves_policy", message_absent="tipo desconhecido", plant_name="x.C"),
    Case("D13b_ext_hp", b"#include <fstream>\n", "reproves_policy", message_absent="tipo desconhecido", plant_name="x.hp"),
    Case("D13c_ext_h_plus_plus", b"#include <fstream>\n", "reproves_policy", message_absent="tipo desconhecido", plant_name="x.h++"),
    Case("D13d_ext_c_plus_plus", b"#include <fstream>\n", "reproves_policy", message_absent="tipo desconhecido", plant_name="x.c++"),
    Case("D13e_ext_tcc", b"#include <fstream>\n", "reproves_policy", message_absent="tipo desconhecido", plant_name="x.tcc"),
    Case("D13f_ext_c", b"#include <fstream>\n", "reproves_policy", message_absent="tipo desconhecido", plant_name="x.c"),
    Case(
        "D14a_unknown_extension",
        b"nao e' fonte C++\n",
        "reproves_directive",
        plant_name="notas.xyz",
        message="tipo desconhecido",
    ),
    Case(
        "D14b_no_extension",
        b"nao e' fonte C++\n",
        "reproves_directive",
        plant_name="impl",
        message="tipo desconhecido",
    ),
    Case(
        "D15_cmakelists_comment_not_scanned",
        b"# include <fstream>\n",
        "passes",
        plant_name="CMakeLists.txt",
    ),
)


def selftest_family_d_extensions(scratch, capture):
    return run_case_table(scratch, capture, "D", _FAMILY_D_EXTENSION_CASES)


# --- familia E: codificacao ----------------------------------------------


def _utf16(text, byteorder):
    return text.encode(f"utf-16-{byteorder}")


# CRITERIO 6: os SEIS casos abaixo sao medidos DE VERDADE - E1..E6
# testam o comportamento deste PORTAO (decodifica/recusa os bytes
# certos), que roda em Python puro e nao depende de compilador nenhum.
# O que NAO e' medido aqui, e fica citado so' da fonte (<https://learn.
# microsoft.com/cpp/build/reference/unicode-support-in-the-compiler-
# and-linker>): se o compilador da Microsoft de fato ACEITARIA cada um
# desses arquivos como fonte C++ valida - essa parte e' premissa do
# desenho (docs/plano-layers-l4.md §1.2), nunca "confirmado" neste
# arquivo.
_FAMILY_E_CASES = (
    Case(
        "E1_utf16le_bom_forbidden",
        b"\xff\xfe" + _utf16("#include <fstream>\n", "le"),
        "reproves_policy",
        message_absent="caractere nulo",
    ),
    Case(
        "E2_utf16be_bom_forbidden",
        b"\xfe\xff" + _utf16("#include <fstream>\n", "be"),
        "reproves_policy",
        message_absent="caractere nulo",
    ),
    Case(
        "E3_utf16le_bom_clean",
        b"\xff\xfe" + _utf16("#include <cstdint>\n", "le"),
        "passes",
    ),
    Case(
        "E4_utf16le_no_bom_forbidden",
        _utf16("#include <fstream>\n", "le"),
        "reproves_directive",
        message="caractere nulo",
    ),
    Case(
        "E5_utf32le_bom",
        b"\xff\xfe\x00\x00" + "#include <cstdint>\n".encode("utf-32-le"),
        "reproves_directive",
        message="UTF-32",
    ),
    Case(
        "E6_utf8_null_byte_mid",
        b"#include <cstdint>\n// x\x00y\n",
        "reproves_directive",
        message="caractere nulo",
    ),
    # E7 (revisao independente, COSMETICO/MUT8): UTF-32 BIG-ENDIAN sem
    # prova propria - so' o LE tinha caso; a mensagem "UTF-32" e' quem
    # distingue de uma reprovacao por "caractere nulo" (o mutante troca
    # a tupla de marcas por so' a LE, e o arquivo cairia na decodificacao
    # UTF-8 e reprovaria pelo nulo - motivo errado).
    Case(
        "E7_utf32be_bom",
        b"\x00\x00\xfe\xff" + "#include <cstdint>\n".encode("utf-32-be"),
        "reproves_directive",
        message="UTF-32",
    ),
)


# --- controles herdados (positivo/negativo/piso/estrutura) --------------


def selftest_positive_control(scratch, capture):
    root = os.path.join(scratch, "positive")
    make_clean_fixture(root)
    outcome = capture(lambda: check_layers(root))
    if outcome.result:
        print("selftest: controle POSITIVO OK (fixture limpa aprovada)")
        return True
    print("selftest: controle POSITIVO FALHOU (fixture limpa deveria ter sido aprovada)", file=sys.stderr)
    print(outcome.text, file=sys.stderr)
    return False


def selftest_negative_control(scratch, capture):
    root = os.path.join(scratch, "negative")
    make_clean_fixture(root)
    target = os.path.join(root, "include", "glintfx", "core", "dirty.hpp")
    _write_bytes(target, b"#include <wayland-client.h>\n")
    outcome = capture(lambda: check_layers(root))
    if outcome.result:
        print("selftest: controle NEGATIVO FALHOU (header do SO nao foi pego)", file=sys.stderr)
        return False
    if target not in outcome.text:
        print(f"selftest: controle NEGATIVO FALHOU (reprovou, mas nao citou {target})", file=sys.stderr)
        print(outcome.text, file=sys.stderr)
        return False
    print("selftest: controle NEGATIVO OK (header do SO pego e citado)")
    return True


def selftest_negative_control_file_header(scratch, capture):
    root = os.path.join(scratch, "negative_file_header")
    make_clean_fixture(root)
    target = os.path.join(root, "src", "core", "dirty_file_io.cpp")
    _write_bytes(target, b"#include <fstream>\n")
    outcome = capture(lambda: check_layers(root))
    if outcome.result:
        print("selftest: controle NEGATIVO (fstream) FALHOU (nao foi pego)", file=sys.stderr)
        return False
    if target not in outcome.text:
        print(f"selftest: controle NEGATIVO (fstream) FALHOU (nao citou {target})", file=sys.stderr)
        return False
    print("selftest: controle NEGATIVO (fstream) OK")
    return True


def selftest_empty_scan_control(scratch, capture):
    root = os.path.join(scratch, "empty")
    os.makedirs(root, exist_ok=True)
    outcome = capture(lambda: check_layers(root))
    if outcome.result:
        print("selftest: controle de VARREDURA VAZIA FALHOU (deveria ter sido recusada)", file=sys.stderr)
        return False
    if "varredura vazia" not in outcome.text:
        print("selftest: controle de VARREDURA VAZIA FALHOU (nao disse 'varredura vazia')", file=sys.stderr)
        return False
    print("selftest: controle de VARREDURA VAZIA OK")
    return True


def selftest_per_layer_negative_control(scratch, capture):
    ok = True
    for label, parts in _PURE_LAYER_DIR_SPECS:
        safe_label = label.replace("/", "_")
        root = os.path.join(scratch, f"negative_layer_{safe_label}")
        make_clean_fixture(root)
        target = os.path.join(root, *parts, "dirty.hpp")
        _write_bytes(target, b"#include <wayland-client.h>\n")
        outcome = capture(lambda: check_layers(root))
        if outcome.result or target not in outcome.text:
            print(f"selftest: controle NEGATIVO ({label}) FALHOU", file=sys.stderr)
            print(outcome.text, file=sys.stderr)
            ok = False
            continue
        print(f"selftest: controle NEGATIVO ({label}) OK")
    return ok


def selftest_per_layer_floor(scratch, capture):
    ok = True
    for label, parts in _PURE_LAYER_DIR_SPECS:
        safe_label = label.replace("/", "_")
        root = os.path.join(scratch, f"floor_layer_{safe_label}")
        make_clean_fixture(root)
        os.remove(os.path.join(root, *parts, "clean.hpp"))
        outcome = capture(lambda: check_layers(root))
        if outcome.result or f"0 arquivos em {label}" not in outcome.text:
            print(f"selftest: controle de PISO ({label}) FALHOU", file=sys.stderr)
            print(outcome.text, file=sys.stderr)
            ok = False
            continue
        print(f"selftest: controle de PISO ({label}) OK")
    return ok


# --- ANCHOR-ON-DIRECTIVE / PHASE-2-3 / RAW-STRING / CRLF / BOM --------
# (heranca de L-1..L-3, migradas pro executor comum sem mudar caso)

_ANCHOR_DIRECTIVE_CASES = (
    Case("anchor_0_plain_include", "#include <fstream>\n", "reproves_policy"),
    Case("anchor_1_space_after_hash", "#  include <GL/gl.h>\n", "reproves_policy"),
    Case("anchor_2_no_space", "#include<windows.h>\n", "reproves_policy"),
    Case(
        "anchor_3_import_angle",
        "import <fstream>;\n",
        "reproves_token",
        message="identificador 'import' proibido",
    ),
    Case(
        "anchor_4_export_import_angle",
        "export import <fstream>;\n",
        "reproves_token",
        message="identificador 'import' proibido",
    ),
    Case("anchor_5_prose_mentions_needle", "// a GL/WGL function name is a mouthful\n", "passes"),
    Case("anchor_6_commented_out_include", "// #include <windows.h>\n", "passes"),
    Case("anchor_7_digraph_glued", "%:include <fstream>\n", "reproves_policy"),
    Case("anchor_8_digraph_spaced", "%:  include <fstream>\n", "reproves_policy"),
    Case("anchor_9_digraph_split_not_recognized", "% : include <fstream>\n", "passes"),
)

_PHASE23_DIRECTIVE_CASES = (
    Case("phase23_a_splice_before_bracket", "#include \\\n<fstream>\n", "reproves_policy"),
    Case("phase23_b_block_comment_mid_directive", "#include /* nada */ <fstream>\n", "reproves_policy"),
    Case("phase23_c_directive_inside_block_comment", "/*\n#include <windows.h>\n*/\n", "passes"),
    Case(
        "phase23_d_quote_does_not_open_comment",
        'const char* s = "/*";\n#include <fstream>\n// */\n',
        "reproves_policy",
    ),
    Case(
        "phase23_e_line_comment_swallows_spliced_continuation",
        "// comentario \\\n#include <fstream>\n",
        "passes",
    ),
)

_RAW_STRING_CASES = (
    Case(
        "rawstring_a_fake_comment_hides_real_include",
        'const char* s = R"(foo " bar /* baz)";\n#include <fstream>\n',
        "reproves_policy",
    ),
    Case(
        "rawstring_b_include_inside_raw_string_is_inert",
        'const char* s = R"(\n#include <fstream>\n)";\n',
        "passes",
    ),
    Case(
        "rawstring_c_mismatched_delimiter_does_not_close",
        'const char* s = R"y(before )x" middle\n#include <fstream>\nafter )y";\n',
        "passes",
    ),
    Case(
        "rawstring_d_prefix_u8R",
        'const char8_t* s = u8R"(foo " bar /* baz)";\n#include <fstream>\n',
        "reproves_policy",
    ),
    Case(
        "rawstring_e_prefix_uR",
        'const char16_t* s = uR"(foo " bar /* baz)";\n#include <fstream>\n',
        "reproves_policy",
    ),
    Case(
        "rawstring_f_prefix_UR",
        'const char32_t* s = UR"(foo " bar /* baz)";\n#include <fstream>\n',
        "reproves_policy",
    ),
    Case(
        "rawstring_g_prefix_LR",
        'const wchar_t* s = LR"(foo " bar /* baz)";\n#include <fstream>\n',
        "reproves_policy",
    ),
    Case(
        "rawstring_h_delimiter_max_length_16",
        'const char* s = R"ABCDEFGHIJKLMNOP(foo " bar /* baz)ABCDEFGHIJKLMNOP";\n#include <fstream>\n',
        "reproves_policy",
    ),
    Case(
        "rawstring_i_backslash_newline_not_spliced_inside_raw_string",
        'const char* s = R"END(before\n)EN\\\nD"\n#include <fstream>\nafter\n)END";\n',
        "passes",
    ),
)

_CRLF_DIRECTIVE_CASES = (
    Case(
        "crlf_a_splice",
        b'const char* s = "x";\r\n#include \\\r\n<fstream>\r\nint main(){return 0;}\r\n',
        "reproves_policy",
    ),
    Case(
        "crlf_b_comment_swallows_splice",
        b"// comentario \\\r\n#include <fstream>\r\nint main(){return 0;}\r\n",
        "passes",
    ),
    Case(
        "crlf_c_raw_string_spans_lines",
        b'const char* s = R"(\r\n#include <fstream>\r\n)";\r\nint main(){return 0;}\r\n',
        "passes",
    ),
    Case(
        "crlf_d_fake_comment_hides_include",
        b'const char* s = R"(foo " bar /* baz)";\r\n#include <fstream>\r\nint main(){return 0;}\r\n',
        "reproves_policy",
    ),
)

_BOM_CASES = (
    Case("bom_a_start_then_real_include", "\ufeff#include <fstream>\n", "reproves_policy"),
    Case("bom_b_start_then_prose_only", "\ufeff// so prosa, sem diretiva\n", "passes"),
)


# --- familia F: token proibido (D-A1, docs/plano-layers-l4-adendo.md §5) -
#
# Onze formas de `import` de unidade de cabecalho (P01-P38), `import
# std`/`module X` (P08/P34/P35/P43), pragma-operador (P11) e `asm`
# (P39) - todas confirmadas contra g++ 16.2.1 e clang++ 22.1.8 na
# sessao do adendo (as sondas em /var/tmp/glintfx-L4-adendo-lab/probe/
# tem o mesmo conteudo byte a byte destes casos).
_ONE_CASE_MESSAGE = "identificador"
_FAMILY_F_CASES = (
    Case("F1_import_3_lines", "import\n<fstream>\n;\n", "reproves_token", message=_ONE_CASE_MESSAGE),
    Case("F2_import_attr_one_line", "import <fstream> [[]];\n", "reproves_token", message=_ONE_CASE_MESSAGE),
    Case(
        "F3_export_import_attr",
        "export import <fstream> [[deprecated]];\n",
        "reproves_token",
        message=_ONE_CASE_MESSAGE,
    ),
    Case("F4_import_quoted_attr", 'import "fstream" [[]];\n', "reproves_token", message=_ONE_CASE_MESSAGE),
    Case(
        "F5_import_macro_arg",
        "#define HDR <fstream>\nimport HDR;\n",
        "reproves_token",
        message=_ONE_CASE_MESSAGE,
    ),
    Case(
        "F6_import_semicolon_next_line",
        "import <fstream>\n;\n",
        "reproves_token",
        message=_ONE_CASE_MESSAGE,
    ),
    Case(
        "F7_macro_named_import",
        "#define IMP import\nIMP <fstream>;\n",
        "reproves_token",
        message=_ONE_CASE_MESSAGE,
    ),
    Case(
        "F8_import_not_line_start",
        "int x; import <fstream>;\n",
        "reproves_token",
        message=_ONE_CASE_MESSAGE,
    ),
    Case(
        "F9_import_inside_ifzero",
        "#if 0\nimport <fstream>;\n#endif\n",
        "reproves_token",
        message=_ONE_CASE_MESSAGE,
    ),
    Case("F10_import_std", "import std;\n", "reproves_token", message=_ONE_CASE_MESSAGE),
    Case("F11_import_std_multiline", "import\nstd;\n", "reproves_token", message=_ONE_CASE_MESSAGE),
    Case("F12_module_decl", "module glintfx_platform;\n", "reproves_token", message=_ONE_CASE_MESSAGE),
    Case(
        "F13_global_module_fragment",
        "module;\n#include <cstdint>\nexport module m;\n",
        "reproves_token",
        message=_ONE_CASE_MESSAGE,
    ),
    Case(
        "F14_pragma_operator_clang_module",
        '_Pragma("clang module import std")\n',
        "reproves_token",
        message=_ONE_CASE_MESSAGE,
    ),
    Case("F15_msvc_pragma_operator", "__pragma(once)\n", "reproves_token", message=_ONE_CASE_MESSAGE),
    Case(
        "F16_asm_incbin",
        'asm(".incbin \\"/etc/hostname\\"");\nint main(){}\n',
        "reproves_token",
        message=_ONE_CASE_MESSAGE,
    ),
    Case("F17a_gcc_asm_dunder", '__asm__("");\n', "reproves_token", message=_ONE_CASE_MESSAGE),
    Case("F17b_msvc_asm_dunder", "__asm {}\n", "reproves_token", message=_ONE_CASE_MESSAGE),
    Case("F17c_msvc_asm_single", "_asm {}\n", "reproves_token", message=_ONE_CASE_MESSAGE),
)

# F18: um caso por identificador da lista (8) - a TRAVA e' o numero de
# casos bater com o tamanho de _FORBIDDEN_IDENTIFIERS (M-F18 "nome
# esquecido").
_FAMILY_F18_FIXTURES = tuple((ident, f"{ident};\n") for ident in sorted(_FORBIDDEN_IDENTIFIERS))
_FAMILY_F18_CASES = tuple(
    Case(f"F18_{i}_{ident.replace('_', 'u')}", content, "reproves_token", message=_ONE_CASE_MESSAGE)
    for i, (ident, content) in enumerate(_FAMILY_F18_FIXTURES)
)


def selftest_family_f18(scratch, capture):
    ok, count = run_case_table(scratch, capture, "F18", _FAMILY_F18_CASES)
    if len(_FORBIDDEN_IDENTIFIERS) != _FORBIDDEN_IDENTIFIER_COUNT:
        print(
            "selftest: F18 FALHOU (_FORBIDDEN_IDENTIFIERS tem "
            f"{len(_FORBIDDEN_IDENTIFIERS)} nomes, mas o numero LITERAL fixo e' "
            f"{_FORBIDDEN_IDENTIFIER_COUNT} - a lista encolheu ou cresceu sem os "
            "casos F18 acompanharem, porque eles sao gerados DA PROPRIA constante "
            "(M-F18, trava tautologica)",
            file=sys.stderr,
        )
        ok = False
    if len(_FAMILY_F18_FIXTURES) != len(_FORBIDDEN_IDENTIFIERS):
        print(
            "selftest: F18 FALHOU (a tabela de fixtures tem "
            f"{len(_FAMILY_F18_FIXTURES)} entradas, mas _FORBIDDEN_IDENTIFIERS tem "
            f"{len(_FORBIDDEN_IDENTIFIERS)} - algum nome ficou sem prova (M-F18)",
            file=sys.stderr,
        )
        ok = False
    return ok, count


# F-neg: identificador PARECIDO nao e' token proibido; literal/
# comentario/cadeia-bruta/nome-de-cabecalho EXCLUEM o texto de dentro.
_FAMILY_F_NEG_CASES = (
    Case(
        "Fneg1_similar_identifiers_pass",
        "int important = 0; int modules = 0; int import_asset = 0; int asm_x = 0;\n",
        "passes",
    ),
    Case(
        "Fneg2_inside_string_or_char_literal",
        "const char* s = \"import <fstream>;\"; char c = 'm';\n",
        "passes",
    ),
    Case(
        "Fneg3_inside_raw_string",
        'const char* s = R"(\nmodule x;\nimport std;\n)";\n',
        "passes",
    ),
    Case(
        "Fneg4_inside_comments",
        "// import <fstream>;\n/* module x; */\n",
        "passes",
    ),
    Case(
        # `module` precisa aparecer como TOKEN ISOLADO (separado por
        # `/`) dentro do nome de cabecalho pra provar a EXCLUSAO de
        # verdade - um nome como `module_map.hpp` nao serve: `_` cola
        # "module" e "map" num so' identificador ("module_map"), que
        # nunca bateria com _FORBIDDEN_IDENTIFIERS de qualquer jeito,
        # exclusao ou nao (achado ao rodar o mutante Fneg5 nesta sessao).
        "Fneg5_module_inside_header_name_is_not_a_token",
        "#include <glintfx/core/module/x.hpp>\n",
        "passes",
        extra_files={"include/glintfx/core/module/x.hpp": b"#pragma once\n"},
    ),
)


# --- familia G: colagem que pode formar token proibido (D-A2) -----------

_FAMILY_G_CASES = (
    Case(
        "G1_paste_forms_import",
        "#define CAT(a,b) a##b\nCAT(im,port) <fstream>;\n",
        "reproves_token",
        message="colagem",
    ),
    Case(
        "G2_paste_forms_Pragma",
        '#define CAT(a,b) a##b\nCAT(_Pra,gma)("clang module import std")\n',
        "reproves_token",
        message="colagem",
    ),
    Case("G3_prefix_fixed_safe_real_tree_form", "#define X(n) k_expected_##n\n", "passes"),
    Case("G4_suffix_fixed_safe", "#define X(n) n##_suffix\n", "passes"),
    # G4b: isola a regra 2 (sufixo) da regra 3 (subcadeia). "_suffix"
    # nao e' subcadeia de nenhum proibido, entao G4 passa mesmo so' com
    # a regra 3 - nao prova a regra 2 sozinha. "por" E' subcadeia de
    # "import" (posicoes 2-4) mas NAO e' SUFIXO dele ("import" termina
    # em "ort") - so' a regra 2 (sufixo exato) aceita "por" como
    # seguro; a regra 3 (subcadeia) reprovaria sozinha. Sem a regra 2,
    # este caso reprova errado.
    Case("G4b_suffix_matches_substring_but_not_suffix", "#define X(n) n##por\n", "passes"),
    Case("G5_prefix_fixed_but_matches_import", "#define X(n) im##n\n", "reproves_token", message="colagem"),
    Case("G6_suffix_fixed_but_matches_import", "#define X(n) n##port\n", "reproves_token", message="colagem"),
    Case("G7_both_operands_params", "#define X(a,b) a##b\n", "reproves_token", message="colagem"),
    Case("G8_digraph_paste", "#define X(a,b) a %:%: b\n", "reproves_token", message="colagem"),
    Case("G9_number_operand_safe", "#define X(n) 1##n\n", "passes"),
    Case(
        "G10_va_args_treated_as_parameter",
        "#define X(...) __VA_ARGS__##t\n",
        "reproves_token",
        message="colagem",
    ),
    Case("G11_object_macro_two_fixed", "#define X im##port\n", "reproves_token", message="colagem"),
    Case("G12_three_operand_chain", "#define X(a) a##por##t\n", "reproves_token", message="colagem"),
)


# --- familia H: `#pragma` vira lista de permitidos (D-A3) ----------------

_PRAGMA_MESSAGE = "pragma nao permitido"
_FAMILY_H_CASES = (
    Case("H1a_pragma_once_hash", "#pragma once\n", "passes"),
    Case("H1b_pragma_once_digraph", "%:pragma once\n", "passes"),
    Case("H1c_pragma_once_spaced_comment", "#  pragma   once  // x\n", "passes"),
    Case(
        "H2_pragma_clang_module_import",
        "#pragma clang module import std\n",
        "reproves_directive",
        message=_PRAGMA_MESSAGE,
    ),
    Case(
        "H3_pragma_include_alias",
        "#pragma include_alias(<cstdint>, <windows.h>)\n#include <cstdint>\n",
        "reproves_directive",
        message=_PRAGMA_MESSAGE,
    ),
    Case(
        "H4_pragma_comment_lib",
        '#pragma comment(lib, "ws2_32")\n',
        "reproves_directive",
        message=_PRAGMA_MESSAGE,
    ),
    Case(
        "H5a_pragma_gcc_dependency",
        '#pragma GCC dependency "x.hpp"\n',
        "reproves_directive",
        message=_PRAGMA_MESSAGE,
    ),
    Case(
        "H5b_pragma_push_macro",
        '#pragma push_macro("X")\n',
        "reproves_directive",
        message=_PRAGMA_MESSAGE,
    ),
    Case(
        "H6_pragma_once_extra_tokens",
        "#pragma once extra\n",
        "reproves_directive",
        message=_PRAGMA_MESSAGE,
    ),
    Case("H7_pragma_empty", "#pragma\n", "reproves_directive", message=_PRAGMA_MESSAGE),
)


# --- familia L: lexico da fase 3 (D-A4) -----------------------------------

_LEXICAL_HEADER_MESSAGE = "nome de cabecalho com caractere de suporte condicional"
_LEXICAL_LITERAL_MESSAGE = "literal nao terminado"
_FAMILY_L_CASES = (
    Case(
        "L1_digitsep_no_longer_hides_include",
        "auto a = 1'0; auto s = \"'/*\";\n#include <fstream>\nauto t = \"*/\";\n",
        "reproves_policy",
        also_expect=(2,),
    ),
    Case(
        "L2_digitsep_before_letter_no_longer_hides_include",
        "auto a = 0x1'f; auto s = \"'/*\";\n#include <fstream>\nauto t = \"*/\";\n",
        "reproves_policy",
        also_expect=(2,),
    ),
    Case(
        "L3_udl_number_no_longer_hides_include",
        "auto a = 1'0_km; auto s = \"'/*\";\n#include <fstream>\nauto t = \"*/\";\n",
        "reproves_policy",
        also_expect=(2,),
    ),
    Case(
        "L4_real_tree_digit_separator_no_regression",
        "constexpr double k = 1'000'000'000.0;\n#include <cstdint>\n",
        "passes",
    ),
    Case(
        "L5_exponent_and_float_forms_no_regression",
        "double d = 1.5e+3; double e = 0x1p-3; float f = .5f;\n",
        "passes",
    ),
    Case(
        "L6_hasinclude_blockcomment_and_hidden_include",
        "#if __has_include(<x/*y>)\n#endif\n#include <fstream>\n// */\n",
        "reproves_lexical",
        message=_LEXICAL_HEADER_MESSAGE,
        also_expect=(3,),
    ),
    Case(
        "L7a_hasinclude_apostrophe",
        "#if __has_include(<x'y>)\n#endif\nauto s = \"'/*\";\n#include <fstream>\nauto t = \"*/\";\n",
        "reproves_lexical",
        message=_LEXICAL_HEADER_MESSAGE,
    ),
    Case(
        "L7b_hasinclude_quote_in_angle",
        '#if __has_include(<x"y>)\n#endif\nauto s = "/*";\n#include <fstream>\nauto t = "*/";\n',
        "reproves_lexical",
        message=_LEXICAL_HEADER_MESSAGE,
    ),
    Case(
        "L8_hasinclude_quoted_backslash",
        '#if __has_include("x\\") && __has_include("/*")\n#endif\n#include <fstream>\n// */\n',
        "reproves_lexical",
        message=_LEXICAL_HEADER_MESSAGE,
    ),
    # L8b: isola a barra invertida - L8 sozinho tem um SEGUNDO
    # __has_include("/*") na mesma linha, que ja' reprova pela odd
    # "/*"; um mutante que tira so' "\\" da lista de odd continuaria
    # reprovando L8 pelo "/*" do segundo argumento, escondendo a falta
    # da barra invertida. L8b so' tem o argumento com barra.
    Case(
        "L8b_hasinclude_quoted_backslash_isolated",
        '#if __has_include("x\\")\n#endif\n#include <fstream>\nauto t = "*";\n',
        "reproves_lexical",
        message=_LEXICAL_HEADER_MESSAGE,
    ),
    Case(
        "L9_include_own_directive_has_odd_header_name",
        "#include <x/*y>\n",
        "reproves_lexical",
        message=_LEXICAL_HEADER_MESSAGE,
    ),
    Case(
        "L10_hasinclude_clean_name_no_regression",
        "#if __has_include(<cstdint>)\n#endif\n",
        "passes",
    ),
    Case(
        "L11_lone_apostrophe_in_skipped_group",
        "#if 0\ndon't\n#endif\nauto s = \"'/*\";\n#include <fstream>\nauto t = \"*/\";\n",
        "reproves_lexical",
        message=_LEXICAL_LITERAL_MESSAGE,
        also_expect=(5,),
    ),
    Case(
        "L12_apostrophe_in_error_directive_skipped",
        "#if 0\n#error it's\n#endif\nauto s = \"'/*\";\n#include <fstream>\nauto t = \"*/\";\n",
        "reproves_lexical",
        message=_LEXICAL_LITERAL_MESSAGE,
        also_expect=(5,),
    ),
    Case(
        "L13_splice_inside_string_is_not_a_real_break",
        'const char* s = "ab\\\ncd";\n#include <cstdint>\n',
        "passes",
    ),
    Case(
        "L14_literal_prefixes_no_regression",
        'auto a = u8\'a\'; auto b = L"x"; auto c = U\'b\'; auto d = u"y";\n#include <cstdint>\n',
        "passes",
    ),
)


# --- familia M: fase 1 - normalizacao de quebra de linha (CONT-4C) ------
#
# Espaco FECHADO enumerado e medido contra g++ 16.2.1 e clang++ 22.1.8
# em 2026-09-23, numa unica invocacao por compilador (GODS_LAWS.md
# L-04/L-11/L-40 item 5) - ver _normalize_newlines() acima e o
# relatorio em /var/tmp/glintfx-plan/impl-layers-L4c.md. Quatro
# controles POSITIVOS (a quebra e' real - o `#include <fstream>` do
# outro lado dela tem de ser visto e reprovado) e oito de FALSO
# POSITIVO (a quebra NAO e' real - o portao nao pode inventar uma
# diretiva que o compilador nunca ve).
_FAMILY_M_CASES = (
    Case("M1_lf_is_a_real_break", "auto a = 1;\n#include <fstream>\n", "reproves_policy", also_expect=(2,)),
    Case("M2_crlf_is_a_real_break", b"auto a = 1;\r\n#include <fstream>\r\n", "reproves_policy", also_expect=(2,)),
    Case(
        "M3_lone_cr_is_a_real_break",
        b"auto a = 1;\r#include <fstream>\r",
        "reproves_policy",
        also_expect=(2,),
    ),
    Case(
        "M4_lf_then_cr_is_two_real_breaks",
        b"auto a = 1;\n\r#include <fstream>\n\r",
        "reproves_policy",
        # linha 3, nao 2: LF e' quebra por si (fecha a linha 1), e o CR
        # solto que vem logo depois e' OUTRA quebra por si (abre e fecha
        # uma linha 2 vazia) - mesmo modelo do str.splitlines() do
        # Python ('a\n\rb'.splitlines() == ['a', '', 'b']), que e' o que
        # o portao PRE-L4 usava e este fix preserva. Pino a linha aqui
        # porque um mutante que para de normalizar o CR que segue um LF
        # ainda reprova (a diretiva sobrevive por tolerancia de espaco
        # em branco do regex de nome de diretiva) mas na linha ERRADA -
        # so' o numero da linha mata esse mutante.
        also_expect=(3,),
    ),
    # M5/M6 testam VT/FF NO MEIO de uma linha ja' aberta (posicao (a) -
    # nao sao quebra ali, e o portao nao pode inventar diretiva). Achado
    # da revisao independente rodada 5, GODS_LAWS.md L-40 item 5 -
    # medido em invocacao agrupada (g++/clang++ -M): quando VT ou FF
    # abrem uma linha FISICA de verdade (a quebra real vem de outro
    # separador antes deles) e caem logo ANTES de `#include`, os dois
    # compiladores medidos os aceitam como espaco em branco LIDER de
    # diretiva - `\f#include <fstream>` compila igual a `#include
    # <fstream>`. O portao ja reprova certo nesse caso, mas por um
    # motivo INCIDENTAL: o `\s` do Python usado em
    # _DIRECTIVE_NAME_PATTERN ja casa com VT/FF/FS/GS/RS/NEL/LS/PS
    # (todos os oito), entao qualquer um deles como PRIMEIRO caractere
    # de uma linha logica real e' tolerado como espaco antes do `#` -
    # nao e' acaso que funciona, e' o `\s` fazendo o trabalho. Quem
    # tocar `_DIRECTIVE_NAME_PATTERN` no futuro precisa saber disso.
    Case("M5_vt_is_not_a_break", b"auto a = 1;\x0b#include <fstream>\x0b", "passes"),
    Case("M6_ff_is_not_a_break", b"auto a = 1;\x0c#include <fstream>\x0c", "passes"),
    Case("M7_fs_is_not_a_break", b"auto a = 1;\x1c#include <fstream>\x1c", "passes"),
    Case("M8_gs_is_not_a_break", b"auto a = 1;\x1d#include <fstream>\x1d", "passes"),
    Case("M9_rs_is_not_a_break", b"auto a = 1;\x1e#include <fstream>\x1e", "passes"),
    Case("M10_nel_is_not_a_break", "auto a = 1;\u0085#include <fstream>\u0085", "passes"),
    Case("M11_ls_is_not_a_break", "auto a = 1;\u2028#include <fstream>\u2028", "passes"),
    Case("M12_ps_is_not_a_break", "auto a = 1;\u2029#include <fstream>\u2029", "passes"),
    # M13-M16 (achado da revisao independente, rodada 5, GODS_LAWS.md
    # L-20/L-40): M1-M4 tem no MAXIMO duas quebras fisicas (uma no
    # meio, uma no fim do arquivo) - um mutante que normaliza so' a
    # PRIMEIRA quebra do arquivo inteiro (`.sub(..., count=1)`) passa
    # despercebido por elas, porque o fallback final de
    # _split_logical_lines() sempre devolve o que sobra
    # no fim do texto como ultima linha logica, mesmo sem quebra real
    # fechando ela - e nos casos M1-M4 a violacao sempre calha de estar
    # nessa ultima linha, entao ainda e' vista por acidente. Estes
    # quatro casos tem TRES quebras fisicas, com a violacao na ultima
    # linha logica, precedida por uma SEGUNDA linha de codigo comum -
    # se so' a primeira quebra normalizar, a segunda linha e a terceira
    # (a violacao) ficam GRUDADAS numa linha logica so' que comeca com
    # `int`, nao com `#`, e a diretiva nunca e' vista. Um por separador
    # real (LF, CRLF, CR sozinho) mais um misto (CR, CRLF e LF no mesmo
    # arquivo) - prova que a normalizacao nao depende de qual separador
    # aparece primeiro.
    Case(
        "M13_three_breaks_lf",
        b"int x = 1;\nint y = 2;\n#include <fstream>\n",
        "reproves_policy",
        also_expect=(3,),
    ),
    Case(
        "M14_three_breaks_crlf",
        b"int x = 1;\r\nint y = 2;\r\n#include <fstream>\r\n",
        "reproves_policy",
        also_expect=(3,),
    ),
    Case(
        "M15_three_breaks_lone_cr",
        b"int x = 1;\rint y = 2;\r#include <fstream>\r",
        "reproves_policy",
        also_expect=(3,),
    ),
    Case(
        "M16_three_breaks_mixed_separators",
        b"int x = 1;\rint y = 2;\r\n#include <fstream>\n",
        "reproves_policy",
        also_expect=(3,),
    ),
)


# Achado das revisoes rodada 6/7 (GODS_LAWS.md L-40 item 5): M13-M16
# sao EXEMPLOS (tres quebras fisicas) - um mutante `count=k` fixo so'
# morre se alguma fixture tiver MAIS de k quebras, entao qualquer
# exemplo novo so' empurra a fronteira de novo (k+1 sobrevive ao
# exemplo de k quebras). "Mais uma fixture com N quebras nao fecha
# isso; so' empurra de novo" (ordem do revisor). Fecha-se pela REGRA
# que _normalize_newlines() promete, chamando a funcao DIRETO (sem
# plantar arquivo nem rodar o portao inteiro).
#
# Achado da revisao rodada 8, sobre o proprio controle de regra: o
# `count` do `.sub()` so' consome CASAMENTOS do regex
# (`_LINE_BREAK_SEQUENCE_PATTERN`, que casa `\r\n` OU `\r` sozinho -
# NUNCA `\n` puro, que ja e' o alvo da normalizacao). O texto da rodada
# 7 ciclava por TRES formas (`\n`, `\r\n`, `\r`), e so' duas delas
# contam como casamento - "mata count=k para todo k menor que 200_000"
# (comentario da rodada 7) e "mata QUALQUER count=k finito" (comentario
# desta secao na rodada 6) eram os DOIS FALSOS: o numero real de
# casamentos ficava perto de 2/3 dos 200_000 ciclos, e um `count=150000`
# sobrevivia. **O que esta funcao PROVA, com exatidao, e nada alem
# disso: mata `count=k` para todo `k` menor que o numero de CASAMENTOS
# do texto - N=_NORMALIZE_PROPERTY_MIN_MATCHES=200_000 - nao "todo k
# finito" e nao "todo k menor que 200_000 ciclos".**
_FAKE_LINE_BREAK_SEPARATORS = ("\x0b", "\x0c", "\x1c", "\x1d", "\x1e", "\u0085", "\u2028", "\u2029")

# As DUAS UNICAS formas que _LINE_BREAK_SEQUENCE_PATTERN casa - ver o
# comentario acima. `\n` puro fica de fora de proposito: e' o alvo da
# normalizacao, entao NUNCA e' um casamento do regex, so' um caractere
# que sobrevive intacto (e conta na saida de qualquer jeito).
_NORMALIZE_PROPERTY_MATCH_FORMS = ("\r\n", "\r")
_NORMALIZE_PROPERTY_PLAIN_LF_STRIDE = 7  # um '\n' puro a cada N ciclos - nao conta como casamento


def _build_mixed_real_break_text(match_count):
    """Texto com EXATAMENTE `match_count` separadores que
    _LINE_BREAK_SEQUENCE_PATTERN CASA (`\r\n` e `\r` sozinho,
    alternando - as duas UNICAS formas que o regex reconhece), cada um
    precedido por um trecho de conteudo de tamanho VARIADO (1 a 11
    caracteres, ciclando por `i % 11` - "posicoes variadas", nao um
    passo fixo que um mutante por coincidencia poderia acertar), mais
    um `\n` PURO (que o regex NAO casa - ja e' o alvo) intercalado a
    cada _NORMALIZE_PROPERTY_PLAIN_LF_STRIDE ciclos, provando que LF
    puro convive sem inflar nem esconder a contagem de casamentos.
    Devolve (texto, casamentos_esperados, quebras_esperadas_apos_
    normalizar) - cada casamento vira EXATAMENTE um '\n' na saida
    (mesmo o CRLF, duas bytes, uma quebra - GODS_LAWS.md L-04, tabela
    medida em impl-layers-L4c.md secao 2), e cada '\n' puro ja
    inserido continua contando, sozinho, um a um."""
    chunks = []
    plain_lf_count = 0
    for i in range(match_count):
        chunks.append("x" * ((i % 11) + 1))
        chunks.append(_NORMALIZE_PROPERTY_MATCH_FORMS[i % len(_NORMALIZE_PROPERTY_MATCH_FORMS)])
        if i % _NORMALIZE_PROPERTY_PLAIN_LF_STRIDE == 0:
            # "z" separa o '\n' puro do casamento que acabou de ser
            # anexado - sem isso, quando o casamento anterior for um
            # `\r` sozinho, o '\n' colaria nele e formaria um `\r\n`
            # que o regex casaria como UM casamento so' (a substring
            # concatenada nao distingue "\r" + "\n" separados de um
            # "\r\n" de proposito) - um '\n' puro "comido" pelo
            # casamento vizinho, contado errado. Achado medindo
            # DEPOIS de escrever esta funcao pela primeira vez: a
            # contagem batia (200_000 casamentos), mas o numero de
            # quebras na saida vinha ~14 mil abaixo do esperado.
            chunks.append("z")
            chunks.append("\n")
            plain_lf_count += 1
    text = "".join(chunks)
    return text, match_count, match_count + plain_lf_count


def _build_fake_break_text(repeats):
    """Texto com os oito separadores FALSOS (nenhum e' quebra real -
    tabela medida) ciclando, cada um precedido por conteudo de tamanho
    variado - usado para provar identidade byte a byte."""
    chunks = []
    for i in range(repeats):
        chunks.append("y" * ((i % 5) + 1))
        chunks.append(_FAKE_LINE_BREAK_SEPARATORS[i % len(_FAKE_LINE_BREAK_SEPARATORS)])
    return "".join(chunks)


def _judge_normalize_no_cr_survives(normalized):
    label = "M:normalize_property_no_cr_survives"
    if "\r" in normalized:
        print(f"selftest: {label} FALHOU (sobrou \\r na saida - count=k finito nao normaliza tudo)", file=sys.stderr)
        return False
    print(f"selftest: {label} OK (passou)")
    return True


def _judge_normalize_break_count(normalized, expected_breaks):
    label = "M:normalize_property_break_count"
    actual = normalized.count("\n")
    if actual != expected_breaks:
        print(f"selftest: {label} FALHOU (esperava {expected_breaks} quebras, saiu {actual})", file=sys.stderr)
        return False
    print(f"selftest: {label} OK (passou)")
    return True


def _judge_normalize_fake_untouched(original, normalized):
    label = "M:normalize_property_fake_untouched"
    if normalized != original:
        print(f"selftest: {label} FALHOU (separador falso foi tocado pela normalizacao)", file=sys.stderr)
        return False
    print(f"selftest: {label} OK (passou)")
    return True


def _judge_normalize_match_floor(actual_matches, expected_minimum):
    """Achado da revisao rodada 8: sem esta checagem, a fixture podia
    ter MENOS casamentos reais do que o nome da constante prometia (foi
    exatamente o defeito - `_build_mixed_real_break_text` ciclava por
    tres formas, so' duas casavam, e a fixture de "200_000" tinha uns
    133_334 casamentos de verdade). Conta os casamentos pelo MESMO
    regex que _normalize_newlines() usa - nunca por suposicao sobre
    quantos ciclos deveriam virar casamento."""
    label = "M:normalize_property_match_floor"
    if actual_matches < expected_minimum:
        print(
            f"selftest: {label} FALHOU ({actual_matches} casamentos reais, esperava >= {expected_minimum})",
            file=sys.stderr,
        )
        return False
    print(f"selftest: {label} OK (passou)")
    return True


# Achado da revisao rodada 7: teste finito de REGRA ainda tem um teto -
# um `count=k` mutante sobrevive a qualquer k maior que o numero de
# CASAMENTOS do texto do controle (ver o comentario grande acima de
# _FAKE_LINE_BREAK_SEPARATORS - rodada 8 corrigiu "ciclos" para
# "casamentos"). O limite abaixo prova a regra para todo `count=k` com
# k < 200_000 CASAMENTOS - o maior arquivo real medido nas seis camadas
# puras hoje (`wc -l` em src/core, include/glintfx/core, src/gfss,
# src/gfui, include/glintfx/gfss, include/glintfx/gfui, 2026-09-23) e'
# src/gfss/selector_parse.cpp, com 962 linhas: 200_000 e' mais de
# DUZENTAS VEZES esse tamanho, e nenhum arquivo das camadas puras chega
# perto. Nao e' o teto teorico do bug (`count=k` sempre tem um k+1 que
# sobrevive a um teste de k casamentos) - e' um teto PRATICO, registrado
# aqui explicitamente para quem reler nao presumir que "200 mil" e'
# magico: e' 200x a maior fixture real conhecida, nao infinito - e
# _judge_normalize_match_floor() acima PROVA que o texto realmente tem
# esse tanto de casamentos, nunca so' supoe pelo numero de ciclos.
_NORMALIZE_PROPERTY_MIN_MATCHES = 200_000
_NORMALIZE_PROPERTY_FAKE_BREAK_CYCLES = 20_000


def selftest_normalize_newlines_property(scratch, capture):
    """Controle de REGRA para _normalize_newlines() (nao de exemplo -
    ver comentario acima de _FAKE_LINE_BREAK_SEPARATORS e de
    _NORMALIZE_PROPERTY_MIN_MATCHES). Chama a funcao DIRETO, fora do
    pipeline do portao (`scratch`/`capture` nao plantam nada aqui, so'
    mantem a assinatura comum de _run_selftest_group()). Quatro
    asserções: (0) o texto de verdade tem pelo menos
    _NORMALIZE_PROPERTY_MIN_MATCHES CASAMENTOS do regex de quebra (nao
    so' ciclos - rodada 8); (1) nao pode sobrar NENHUM `\\r` na saida;
    (2) o numero de `\\n` da saida bate com casamentos + LFs puros
    inseridos; (3) um texto so' com os oito separadores falsos sai
    IDENTICO byte a byte - nenhum deles pode ser tocado."""
    del scratch, capture
    real_text, expected_matches, expected_breaks = _build_mixed_real_break_text(_NORMALIZE_PROPERTY_MIN_MATCHES)
    actual_matches = len(_LINE_BREAK_SEQUENCE_PATTERN.findall(real_text))
    normalized_real = _normalize_newlines(real_text)
    fake_text = _build_fake_break_text(_NORMALIZE_PROPERTY_FAKE_BREAK_CYCLES)
    normalized_fake = _normalize_newlines(fake_text)

    checks = (
        _judge_normalize_match_floor(actual_matches, expected_matches),
        _judge_normalize_no_cr_survives(normalized_real),
        _judge_normalize_break_count(normalized_real, expected_breaks),
        _judge_normalize_fake_untouched(fake_text, normalized_fake),
    )
    return all(checks), len(checks)


# --- registro unico das tabelas de Case, e exportacao pro oraculo ------
#
# docs/plano-layers-l5.md §4.1: fonte UNICA para o autoteste E para
# `--export-fixtures` - o oraculo (tests/tools/check_layers_oracle.py)
# nunca importa este modulo nem copia fixture, so' fala com ele pela
# linha de comando e por um manifest.json (L-4 §5, "o oraculo nao
# substitui o portao e nao pode reusar a logica que existe pra
# vigiar"). `modo_oraculo` e' "compilar" pra toda tabela, exceto C12
# ("calibracao"): o conteudo de cada caso de C12 e' `#include <nome>\n`
# - exatamente o que a calibracao do oraculo ja pre-processa de uma vez
# so' (secao 7.1 do plano) - compilar os 105 separados custaria 105
# processos pra repetir a MESMA pergunta que a calibracao ja responde.
_CASE_TABLES = (
    ("anchor", _ANCHOR_DIRECTIVE_CASES, "compilar"),
    ("phase23", _PHASE23_DIRECTIVE_CASES, "compilar"),
    ("rawstring", _RAW_STRING_CASES, "compilar"),
    ("crlf", _CRLF_DIRECTIVE_CASES, "compilar"),
    ("bom", _BOM_CASES, "compilar"),
    ("A", _FAMILY_A_CASES, "compilar"),
    ("A_legacy", _FAMILY_A_LEGACY_CASES, "compilar"),
    ("B", _FAMILY_B_CASES, "compilar"),
    ("B7", _FAMILY_B7_CASES, "compilar"),
    ("C", _FAMILY_C_CASES, "compilar"),
    ("C12", _FAMILY_C12_CASES, "calibracao"),
    ("D", _FAMILY_D_EXTENSION_CASES, "compilar"),
    ("E", _FAMILY_E_CASES, "compilar"),
    ("F", _FAMILY_F_CASES, "compilar"),
    ("F18", _FAMILY_F18_CASES, "compilar"),
    ("Fneg", _FAMILY_F_NEG_CASES, "compilar"),
    ("G", _FAMILY_G_CASES, "compilar"),
    ("H", _FAMILY_H_CASES, "compilar"),
    ("L", _FAMILY_L_CASES, "compilar"),
    ("M", _FAMILY_M_CASES, "compilar"),
)


def _iter_module_case_tables():
    """Enumeracao FECHADA do modulo (GODS_LAWS.md L-40 item 5): toda
    tupla NAO VAZIA no escopo do modulo cujos elementos sao todos
    `Case`. So' o nivel do modulo - uma tabela PRIVADA, vivendo dentro
    de uma funcao (como a de X3 abaixo, de proposito), nunca aparece
    aqui. Usada pela trava X1 - nunca uma lista mantida a mao, que uma
    tabela nova esqueceria de atualizar."""
    found = {}
    for name, value in globals().items():
        if not isinstance(value, tuple) or not value:
            continue
        if all(isinstance(item, Case) for item in value):
            found[name] = value
    return found


def selftest_case_table_registry_x1(scratch, capture):
    """X1 (docs/plano-layers-l5.md §4.1 item 2): toda tupla de `Case`
    do modulo tem de estar em `_CASE_TABLES` (por IDENTIDADE, nao por
    igualdade de conteudo - duas tabelas com o mesmo conteudo por
    acidente continuam sendo tabelas DIFERENTES), o registro nunca cita
    tabela que nao existe, e nenhum rotulo se repete."""
    del scratch, capture  # X1 so' inspeciona o proprio modulo, nao roda check_layers()
    ok = True
    discovered = _iter_module_case_tables()
    discovered_ids = {id(table) for table in discovered.values()}
    registered_ids = {id(table) for _label, table, _mode in _CASE_TABLES}
    registered_labels = [label for label, _table, _mode in _CASE_TABLES]

    missing = sorted(name for name, table in discovered.items() if id(table) not in registered_ids)
    if missing:
        print(f"selftest: X1 FALHOU (tabela(s) de Case fora do registro: {missing})", file=sys.stderr)
        ok = False

    dangling = sorted(label for label, table, _mode in _CASE_TABLES if id(table) not in discovered_ids)
    if dangling:
        print(f"selftest: X1 FALHOU (registro cita tabela que nao existe no modulo: {dangling})", file=sys.stderr)
        ok = False

    duplicate_labels = sorted({label for label in registered_labels if registered_labels.count(label) > 1})
    if duplicate_labels:
        print(f"selftest: X1 FALHOU (rotulo repetido em _CASE_TABLES: {duplicate_labels})", file=sys.stderr)
        ok = False

    if ok:
        print(f"selftest: X1 OK ({len(discovered)} tabelas descobertas, {len(_CASE_TABLES)} registradas)")
    return ok, 1


def _classify_target_kind(filename):
    """Traduz o veredito de `_classify_source_filename()` (fonte/skip/
    desconhecido) pro vocabulario do manifesto do oraculo (docs/plano-
    layers-l5.md §4.1) - uma fonte so', nunca uma segunda lista mantida
    a parte."""
    verdict = _classify_source_filename(filename)
    return {"skip": "nao-fonte-conhecido", "source": "fonte", "unknown": "desconhecido"}[verdict]


def _relpath_under_pure_layer(root, abs_path):
    """Caminho de `abs_path` relativo a `root`, em POSIX, SE e somente
    se cair dentro de uma das seis camadas puras (_PURE_LAYER_DIR_
    SPECS) - None quando fica fora delas (o oraculo so' examina o que
    o proprio portao tambem examinaria)."""
    rel = os.path.relpath(abs_path, root).replace(os.sep, "/")
    for _label, parts in _PURE_LAYER_DIR_SPECS:
        prefix = "/".join(parts) + "/"
        if rel.startswith(prefix):
            return rel
    return None


def _case_export_targets(root, target, case):
    """Alvos de UM caso: o arquivo plantado, mais cada `extra_files`
    que caia numa das seis camadas puras (docs/plano-layers-l5.md
    §4.1 item 3)."""
    targets = []
    main_rel = _relpath_under_pure_layer(root, target)
    if main_rel is not None:
        targets.append(main_rel)
    for rel_path in case.extra_files:
        abs_extra = os.path.join(root, *rel_path.split("/"))
        extra_rel = _relpath_under_pure_layer(root, abs_extra)
        if extra_rel is not None and extra_rel not in targets:
            targets.append(extra_rel)
    return targets


# Achado da revisao independente (23/09/2026): _export_one_case() tinha
# 5 parametros soltos (teto de L-17 e' 4) - (label, case, mode) sao os
# TRES que ja vem juntos de cada linha do registro _CASE_TABLES, entao
# agrupa-los numa tupla nomeada e' o mesmo remedio que RealMainInputs
# ja usa em check_test_parity.py, sem introduzir estado nenhum.
_CaseExportJob = collections.namedtuple("_CaseExportJob", ("label", "case", "mode"))


def _export_one_case(dest_dir, job, capture):
    """Exporta UM caso: mesma `_plant_case_fixture()` do autoteste
    (fonte unica, R-9 do plano), mesma `check_layers()` sob captura -
    o oraculo compara com o VEREDITO REAL aqui capturado, nunca com o
    declarado na tabela (docs/plano-layers-l5.md §5.2)."""
    root, target = _plant_case_fixture(dest_dir, job.label, job.case)
    outcome = capture(lambda: check_layers(root))
    return {
        "tabela": job.label,
        "caso": job.case.name,
        "modo_oraculo": job.mode,
        "raiz": os.path.relpath(root, dest_dir).replace(os.sep, "/"),
        "alvos": [
            {"caminho": rel, "tipo": _classify_target_kind(rel.rsplit("/", 1)[-1])}
            for rel in _case_export_targets(root, target, job.case)
        ],
        "veredito_declarado": job.case.verdict,
        "veredito_real": "passou" if outcome.result else "reprovou",
        "saida_real": outcome.text,
    }


def _export_case_tables(dest_dir, registry):
    """Fonte UNICA de exportacao - `--export-fixtures` real E os
    controles X2/X3 do autoteste chamam esta mesma funcao (docs/plano-
    layers-l5.md §4.1: 'os dois chamam a mesma _plant_case_fixture()',
    X2 confere byte a byte). `dest_dir` precisa existir e estar vazio -
    responsabilidade do chamador (nunca mistura exportacao velha com
    nova)."""
    capture = _make_capture()
    cases_out = [
        _export_one_case(dest_dir, _CaseExportJob(label, case, mode), capture)
        for label, table, mode in registry
        for case in table
    ]
    manifest = {
        "stdlib_permitidos": sorted(_STDLIB_ALLOWED_NAMES),
        "stdlib_banidos": sorted(_STDLIB_BANNED_NAMES),
        "gerados": sorted(_GENERATED_HEADERS),
        "camadas_puras": [
            {"rotulo": label, "partes": list(parts)} for label, parts in _PURE_LAYER_DIR_SPECS
        ],
        "total_casos": len(cases_out),
        "casos": cases_out,
    }
    with open(os.path.join(dest_dir, "manifest.json"), "w", encoding="utf-8") as handle:
        json.dump(manifest, handle, indent=2, sort_keys=True)
        handle.write("\n")
    return manifest


def export_fixtures_main(args):
    """`check_layers.py --export-fixtures <pasta-vazia>` (docs/plano-
    layers-l5.md §4.1 item 3). Nunca roda oraculo nenhum aqui - so'
    planta fixture e chama a MESMA check_layers() do modo real."""
    if len(args) != 1:
        fail("usage: check_layers.py --export-fixtures <pasta-vazia>")
    dest_dir = args[0]
    if os.path.isdir(dest_dir) and os.listdir(dest_dir):
        fail(f"pasta de exportacao nao esta vazia: {dest_dir}")
    os.makedirs(dest_dir, exist_ok=True)
    manifest = _export_case_tables(dest_dir, _CASE_TABLES)
    print(f"{SCRIPT_NAME}: --export-fixtures: {manifest['total_casos']} casos exportados em {dest_dir}")


def selftest_case_table_registry_x2(scratch, capture):
    """X2 (docs/plano-layers-l5.md §4.1 item 4): ida e volta - exporta
    pro scratch e confere que `total_casos` bate com a soma do
    registro, e que os bytes de cada alvo plantado sao IDENTICOS a
    `case.content` (nunca lidos como texto - L-4 §3, 'toda fixture e
    gravada em binario')."""
    del capture  # X2 usa o proprio capture de _export_case_tables(), nao o do chamador
    dest_dir = os.path.join(scratch, "x2_export")
    manifest = _export_case_tables(dest_dir, _CASE_TABLES)
    expected_total = sum(len(table) for _label, table, _mode in _CASE_TABLES)
    ok = True
    if manifest["total_casos"] != expected_total:
        print(
            f"selftest: X2 FALHOU (total_casos={manifest['total_casos']}, esperado {expected_total})",
            file=sys.stderr,
        )
        ok = False
    for label, table, _mode in _CASE_TABLES:
        for case in table:
            planted_path = os.path.join(dest_dir, f"{label}_{case.name}", *case.plant_dir, case.plant_name)
            with open(planted_path, "rb") as handle:
                actual_bytes = handle.read()
            if actual_bytes != case.content:
                print(f"selftest: X2 FALHOU (bytes divergem em {label}:{case.name})", file=sys.stderr)
                ok = False
    if ok:
        print(f"selftest: X2 OK ({expected_total} casos, ida e volta byte a byte)")
    return ok, 1


def selftest_case_table_registry_x3(scratch, capture):
    """X3 (docs/plano-layers-l5.md §4.1 item 5): o manifesto grava o
    veredito REAL, nunca o declarado. Tabela PRIVADA de proposito - vive
    DENTRO desta funcao, nunca no nivel do modulo, entao X1 (que so'
    enxerga o nivel do modulo) nunca a enumera."""
    del capture
    private_table = (
        Case(
            "X3_declared_passes_real_reproves",
            b"#include <fstream>\n",
            "passes",  # DECLARADO errado de proposito - o real tem de reprovar
        ),
    )
    private_registry = (("X3", private_table, "compilar"),)
    dest_dir = os.path.join(scratch, "x3_export")
    manifest = _export_case_tables(dest_dir, private_registry)
    ok = True
    entry = manifest["casos"][0]
    if entry["veredito_declarado"] != "passes":
        print("selftest: X3 FALHOU (veredito_declarado nao capturado)", file=sys.stderr)
        ok = False
    if entry["veredito_real"] != "reprovou":
        print(
            f"selftest: X3 FALHOU (veredito_real={entry['veredito_real']!r}, esperado 'reprovou' - "
            "o manifesto gravou o declarado no lugar do real)",
            file=sys.stderr,
        )
        ok = False
    if ok:
        print("selftest: X3 OK (manifesto grava o veredito REAL, nao o declarado)")
    return ok, 1


# Cada entrada e' (funcao, *args-extra-alem-de-scratch/capture) - a
# lista declarativa que selftest_main() abaixo so' percorre, no lugar
# de uma chamada por linha (L-17: a antiga tinha 50 linhas so' de
# chamadas repetitivas).
_SELFTEST_GROUPS = (
    (selftest_positive_control,),
    (selftest_negative_control,),
    (selftest_negative_control_file_header,),
    (selftest_empty_scan_control,),
    (selftest_per_layer_negative_control,),
    (selftest_per_layer_floor,),
    (run_case_table, "anchor", _ANCHOR_DIRECTIVE_CASES),
    (run_case_table, "phase23", _PHASE23_DIRECTIVE_CASES),
    (run_case_table, "rawstring", _RAW_STRING_CASES),
    (run_case_table, "crlf", _CRLF_DIRECTIVE_CASES),
    (run_case_table, "bom", _BOM_CASES),
    (run_case_table, "A", _FAMILY_A_CASES),
    (run_case_table, "A_legacy", _FAMILY_A_LEGACY_CASES),
    (run_case_table, "B", _FAMILY_B_CASES),
    (selftest_family_b7,),
    (run_case_table, "C", _FAMILY_C_CASES),
    (selftest_family_c12,),
    (selftest_family_d1_d6_symlinked_dirs,),
    (selftest_family_d7_root_symlink,),
    (selftest_family_d8_d9_file_shortcuts,),
    (selftest_family_d10_fifo,),
    (selftest_family_d11_junction,),
    (selftest_family_d16_kind_from_lstat,),
    (selftest_family_d17_junction_relative_include,),
    (selftest_family_c24_relative_quote_via_symlink,),
    (selftest_family_c25_project_header_via_symlink,),
    (selftest_family_d_extensions,),
    (run_case_table, "E", _FAMILY_E_CASES),
    (run_case_table, "F", _FAMILY_F_CASES),
    (selftest_family_f18,),
    (run_case_table, "Fneg", _FAMILY_F_NEG_CASES),
    (run_case_table, "G", _FAMILY_G_CASES),
    (run_case_table, "H", _FAMILY_H_CASES),
    (run_case_table, "L", _FAMILY_L_CASES),
    (run_case_table, "M", _FAMILY_M_CASES),
    (selftest_normalize_newlines_property,),
    (selftest_case_table_registry_x1,),
    (selftest_case_table_registry_x2,),
    (selftest_case_table_registry_x3,),
)


def _run_selftest_group(scratch, capture, group):
    fn, *extra_args = group
    outcome = fn(scratch, capture, *extra_args)
    return outcome if isinstance(outcome, tuple) else (outcome, 1)


def selftest_main():
    scratch = make_scratch_workdir()
    capture = _make_capture()
    try:
        outcomes = [_run_selftest_group(scratch, capture, group) for group in _SELFTEST_GROUPS]
        results = [ok for ok, _count in outcomes]
        total_cases = sum(count for _ok, count in outcomes)
        if not all(results):
            print("check_layers.py --selftest: FALHOU (ver acima)", file=sys.stderr)
            sys.exit(1)
        print(
            f"check_layers.py --selftest: {len(results)} grupos de controle OK "
            f"({total_cases} casos no total)"
        )
    finally:
        shutil.rmtree(scratch, ignore_errors=True)


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
    elif args and args[0] == "--export-fixtures":
        export_fixtures_main(args[1:])
    else:
        real_main(args)


if __name__ == "__main__":
    main()
