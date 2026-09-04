#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_macro_balance.py - CI gate for TODO.md's HDR-MACRO-BALANCE
# ("Nenhum portao detecta header publico que esqueca um #undef, deixando
# macro vazar para o pre-processador de todo consumidor").
#
# ORIGIN (TODO.md, HDR-MACRO-BALANCE, achado da revisao adversarial de
# 27/08/2026): o revisor removeu o `#undef GLINTFX_GFSS_TOKEN_KIND_LIST`
# de include/glintfx/gfss/token.hpp e o build completo com -Werror saiu
# exit 0, com a suite inteira em 47/47. check_public_name_collision.py
# (docs/api-conventions.md R6) NAO cobre o caso por duas razoes
# independentes: seu extrator so pega enum class/class/struct/funcao/
# membro de dado - NUNCA #define -, e mesmo se pegasse, ele checa a
# direcao "o sistema define um nome nosso", nunca "nosso header vaza
# macro para o consumidor" - a direcao OPOSTA.
#
# DUAS ABORDAGENS MEDIDAS CONTRA O MESMO FIXTURE ANTES DE ESCOLHER
# (ordem do orquestrador, GODS_LAWS.md L-01: "meca as duas... escolha
# com dado"), as duas propostas pelo proprio item do TODO.md:
#
#   A) `cpp -dM` antes/depois de incluir cada header publico (diff do
#      conjunto de macros ativas). MEDIDO contra uma copia de
#      token.hpp fora da arvore (L-27): pega o controle negativo
#      corretamente (GLINTFX_GFSS_TOKEN_KIND_LIST aparece no diff
#      quando o #undef e removido, ausente quando presente). MAS tem
#      um teto de paridade que a abordagem B nao tem: MSVC nao tem
#      NENHUMA flag documentada equivalente a `-dM` (pesquisado por
#      check_public_name_collision.py's own NAMES-PARITY-WIN, mesmo
#      cabecalho deste diretorio - "MSVC nao documenta nenhuma flag
#      equivalente a -dM"). check_public_name_collision.py contorna
#      isso perguntando "este NOME especifico esta ativo?" via `#ifdef`
#      por candidato - mas essa tecnica exige, primeiro, uma lista de
#      NOMES CANDIDATOS, que so vem de uma varredura textual dos
#      #define do proprio header (ou seja, reintroduz a abordagem B
#      como pre-requisito, so que pagando o custo extra de compilar).
#      Sem essa lista candidata, MSVC nao tem como responder "qual e o
#      CONJUNTO INTEIRO de macros ativas" - so pode responder pergunta
#      pontual por nome. GATE-TREE-PARITY (GODS_LAWS.md L-04, decisao
#      do lider: "O comportamento deve ser igual em qualquer OS") exige
#      MESMO MECANISMO, comportamento igual, nas cinco plataformas - e
#      a abordagem A nao tem caminho documentado para chegar la sem
#      reinventar a abordagem B por baixo.
#
#   B) Varredura textual de balanco #define/#undef restrita a
#      include/ - MESMO mecanismo que
#      check_public_name_collision.py's own name_is_undef_in_same_file()
#      ja usa para sua propria pergunta ("undef no MESMO arquivo
#      neutraliza"), aqui generalizado para TODO #define do header,
#      nao so os candidatos a colisao de sistema. MEDIDO contra a
#      MESMA copia: pega o controle negativo identico ao da abordagem A
#      (leaked=['GLINTFX_GFSS_TOKEN_KIND_LIST'] quando o #undef falta,
#      leaked=[] quando presente), sem precisar de compilador nenhum -
#      texto puro, identico nas cinco plataformas por construcao, sem
#      escape hatch de "nao aplicavel no Windows" nenhum.
#
# VEREDITO, com dado: abordagem B. Mesma capacidade de deteccao medida
# (ambas pegam o controle negativo do MESMO fixture), custo em
# processos menor (zero subprocessos, contra 2 invocacoes de compilador
# por header na abordagem A), e SEM o teto de paridade MSVC que a
# abordagem A carrega estruturalmente. A abordagem A e descartada por
# essa razao estrutural, nao por preguica de medir - ela FOI medida e
# FUNCIONOU no lado GCC, so nao tem como funcionar igual no lado MSVC
# sem reimplementar B por dentro.
#
# LIMITACAO DECLARADA (mesma limitacao que o proprio TODO.md aceita
# como aceitavel para esta fatia, e a MESMA que
# check_public_name_collision.py's own name_is_undef_in_same_file()
# ja carrega): balanco textual, nao rastreamento de `#ifdef`/`#elif`.
# Um `#define` textualmente dentro de um bloco condicional nunca
# ativo NAO e distinguido de um `#define` incondicional - ambos contam
# igual para o balanco. Isto e seguro pelo lado da SUB-deteccao (nunca
# aprova em silencio um `#define` condicional desbalanceado: ele ainda
# teria de ter um `#undef` em algum lugar do MESMO arquivo para nao
# reprovar), e o unico jeito de errar seria um FALSO POSITIVO sobre um
# `#define` condicional cujo par `#undef` mora em OUTRO arquivo -
# padrao que nenhum header hoje usa (medido: todos os 11 headers
# publicos rastreados usam `#pragma once`, zero usa guarda `#ifndef`,
# e as 3 macros de cada grupo X-macro sao definidas e desfeitas dentro
# do MESMO header - ver enumerate_public_headers() mais abaixo rodada
# ao vivo). Se esse padrao aparecer no futuro, a saida FALHA NOMEANDO
# o arquivo/macro, nunca aprova calada - e o autor daquela fatia decide
# se e falso positivo (entra na allowlist fechada abaixo, com
# justificativa) ou defeito real.
#
# O QUE CONTA COMO "MACRO LEGITIMA" (a lista fechada exigida pelo
# orquestrador, GODS_LAWS.md L-40: "nao escreva uma lista de excecoes
# frouxa que esvazia o portao") - _LEGITIMATE_SURVIVING_MACROS abaixo,
# HOJE VAZIA, POR MEDICAO, NAO POR PRESUNCAO:
#
#   1. Guarda de inclusao (`#ifndef HEADER_H` / `#define HEADER_H` sem
#      `#undef`, por desenho - e' esse o proposito de um header guard).
#      NAO SE APLICA hoje: os 11 headers rastreados usam `#pragma once`
#      (medido: `grep -L "#pragma once" $(find include -name "*.hpp")`
#      devolve vazio - zero excecao), entao esta categoria tem ZERO
#      membro real hoje.
#   2. Macro de exportacao gerada (GLINTFX_API e as demais de
#      export.hpp, GLINTFX_VERSION_* de version_macros.hpp). NAO SE
#      APLICA: essas duas cabecalhos sao GERADOS em tempo de build
#      para "${CMAKE_BINARY_DIR}/generated/include/glintfx/" (ver
#      cmake/GlintfxLibrary.cmake's own glintfx_generate_export_header()
#      e o configure_file() do version_macros.hpp.in logo abaixo dele)
#      - NUNCA moram sob a arvore RASTREADA `include/` que este script
#      varre (medido: `find include -name export.hpp -o -name
#      version_macros.hpp` nao acha nada; os dois so existem sob
#      `build/generated/include/`, arquivo de build, nao rastreado).
#      Esta gate varre PROJECT_SOURCE_DIR/include, o MESMO diretorio
#      que check_public_name_collision.py's own enumerate_public_headers()
#      varre - os dois headers gerados estao fora do escopo por
#      construcao, nao por omissao.
#
# CONTAGEM: 0 entradas legitimas conhecidas hoje (as duas categorias
# acima existem em teoria, mas NENHUMA tem membro real na arvore
# rastreada agora). Todo #define encontrado sob include/ tem, portanto,
# de ter seu #undef no MESMO arquivo, sem excecao - exatamente a
# convencao que os 3 headers com macro hoje (token.hpp, value.hpp,
# node_view.hpp) ja seguem.
#
# Each function below does one thing (GODS_LAWS.md L-17).

import os
import re
import sys
import tempfile
from collections import Counter

SCRIPT_NAME = "check_macro_balance.py"

_HEADER_EXTENSIONS = (".hpp", ".h", ".hh", ".hxx")

# The closed, counted, justified allowlist - see this file's own header
# comment above for why it is empty TODAY, and what would earn an
# entry (a citation of WHICH real header needs it and WHY, the same
# bar GODS_LAWS.md L-40 sets for every other exception list in this
# project).
_LEGITIMATE_SURVIVING_MACROS = frozenset()


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


# --- header enumeration and text prep (same shape as the sibling gate,
# check_public_name_collision.py's own enumerate_public_headers()/
# strip_line_comments(), deliberately reused rather than reinvented) --


def enumerate_public_headers(include_dir):
    files = []
    for dirpath, _dirnames, filenames in os.walk(include_dir):
        for name in filenames:
            if name.endswith(_HEADER_EXTENSIONS):
                files.append(os.path.join(dirpath, name))
    return sorted(files)


def strip_line_comments(text):
    """Strips a trailing "// ..." comment from every line before macro
    extraction - a prose comment that happens to contain the words
    "#define"/"#undef" (this file's OWN header above does, repeatedly)
    must never be mistaken for a real preprocessor directive. No /* */
    block comments exist in this project's public headers (same
    declared limitation check_public_name_collision.py's own sibling
    function carries).
    """
    return "\n".join(re.sub(r"//.*", "", line) for line in text.splitlines())


_DEFINE_RE = re.compile(r"^\s*#\s*define\s+([A-Za-z_][A-Za-z0-9_]*)")
_UNDEF_RE = re.compile(r"^\s*#\s*undef\s+([A-Za-z_][A-Za-z0-9_]*)")


def scan_defines_and_undefs(text):
    """One pass over a (comment-stripped) header's text, returning two
    parallel lists: every (lineno, name) a "#define NAME..." directive
    introduces (object-like or function-like, the macro's own
    parameter list is irrelevant to balance), and every (lineno, name)
    an "#undef NAME" directive retires. Line numbers are 1-based, over
    the SAME stripped text passed in (comment-stripping never changes
    line count - re.sub never removes a newline).
    """
    defines, undefs = [], []
    for lineno, line in enumerate(text.splitlines(), start=1):
        m = _DEFINE_RE.match(line)
        if m:
            defines.append((lineno, m.group(1)))
            continue
        m = _UNDEF_RE.match(line)
        if m:
            undefs.append((lineno, m.group(1)))
    return defines, undefs


def classify_header(path):
    """Balances one header's own #define/#undef directives, name by
    name. Returns a list of dicts, one per name that LEAKS (more
    #define than #undef occurrences, and not in the closed
    allowlist) - status is always "LEAK" here, the caller decides what
    an empty list means. `line` is the LAST unmatched #define's own
    line (the most actionable one: "this is the definition missing
    its #undef").
    """
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as handle:
            text = handle.read()
    except OSError as exc:
        print(f"{SCRIPT_NAME}: {path}: open refused ({exc})", file=sys.stderr)
        return [], 0

    defines, undefs = scan_defines_and_undefs(strip_line_comments(text))
    define_names = [name for _lineno, name in defines]
    undef_counts = Counter(name for _lineno, name in undefs)

    last_define_line = {}
    define_counts = Counter()
    for lineno, name in defines:
        define_counts[name] += 1
        last_define_line[name] = lineno  # last occurrence wins, on purpose

    leaks = []
    for name, count in define_counts.items():
        if name in _LEGITIMATE_SURVIVING_MACROS:
            continue
        if undef_counts.get(name, 0) < count:
            leaks.append({"file": path, "line": last_define_line[name], "name": name})
    return leaks, len(define_names)


# --- L-40 floor ------------------------------------------------------


def require_nonempty_scan(value, what):
    if not value:
        print(f"{SCRIPT_NAME}: varredura vazia ({what})", file=sys.stderr)
        return False
    return True


# --- the check itself --------------------------------------------------


def check_macro_balance(include_dir):
    headers = enumerate_public_headers(include_dir)
    if not require_nonempty_scan(headers, f"0 headers publicos encontrados em {include_dir}"):
        return False

    all_leaks = []
    macro_count = 0
    for header in headers:
        leaks, defines_in_file = classify_header(header)
        macro_count += defines_in_file
        all_leaks.extend(leaks)

    if all_leaks:
        print(f"{SCRIPT_NAME}: MACRO VAZADA (TODO.md HDR-MACRO-BALANCE):", file=sys.stderr)
        for leak in all_leaks:
            print(f"  {leak['file']}:{leak['line']}:{leak['name']}", file=sys.stderr)
        return False

    print(
        f"{SCRIPT_NAME}: {macro_count} #define(s) verificado(s) em {len(headers)} header(s) "
        f"publico(s) sob {include_dir}, 0 vazamento real "
        f"({len(_LEGITIMATE_SURVIVING_MACROS)} entrada(s) na allowlist fechada)"
    )
    return True


# --- real mode -----------------------------------------------------------


def real_main(args):
    if len(args) != 1:
        fail("usage: check_macro_balance.py <include_dir>")
    (include_dir,) = args
    if not os.path.isdir(include_dir):
        fail(f"include dir not found: {include_dir}")
    if not check_macro_balance(include_dir):
        fail("macro vazada de header publico encontrada (ver mensagem acima)")


# --- selftest fixtures and controls -------------------------------------


def make_scratch_workdir():
    return tempfile.mkdtemp(prefix="glintfx-macro-balance-selftest-", dir=os.environ.get("TMPDIR"))


def make_fixture_include_dir(scratch, label):
    d = os.path.join(scratch, label, "include", "pkg")
    os.makedirs(d, exist_ok=True)
    return d


def _write(path, text):
    with open(path, "w", encoding="utf-8") as handle:
        handle.write(text)


# Positive control: the REAL shape token.hpp uses (X-macro list,
# enumerator, count-one, all three defined AND undef'd). Expected:
# passes, zero leaks, 3 macros counted.
def selftest_positive_control(scratch):
    include_dir = make_fixture_include_dir(scratch, "positive")
    _write(
        os.path.join(include_dir, "widget.hpp"),
        "#pragma once\n"
        "#define WIDGET_KIND_LIST(X) X(a) X(b)\n"
        "enum class kind {\n"
        "#define WIDGET_KIND_ENUMERATOR(name) name,\n"
        "    WIDGET_KIND_LIST(WIDGET_KIND_ENUMERATOR)\n"
        "#undef WIDGET_KIND_ENUMERATOR\n"
        "};\n"
        "#undef WIDGET_KIND_LIST\n",
    )
    leaks, defines = classify_header(os.path.join(include_dir, "widget.hpp"))
    if leaks:
        print(f"selftest: controle POSITIVO FALHOU (esperava zero vazamentos, achou {leaks})", file=sys.stderr)
        return False
    if defines != 2:
        print(f"selftest: controle POSITIVO FALHOU (esperava 2 #define, contou {defines})", file=sys.stderr)
        return False
    print("selftest: controle POSITIVO OK (header no molde X-macro real, balanceado)")
    return True


# Negative control: the EXACT experiment TODO.md's own HDR-MACRO-BALANCE
# item cites (27/08/2026 adversarial review) - the outer #undef removed.
# Expected: REAL leak, citing file, line and the exact name.
def selftest_negative_control(scratch):
    include_dir = make_fixture_include_dir(scratch, "negative")
    path = os.path.join(include_dir, "widget.hpp")
    _write(
        path,
        "#pragma once\n"
        "#define WIDGET_KIND_LIST(X) X(a) X(b)\n"
        "enum class kind {\n"
        "#define WIDGET_KIND_ENUMERATOR(name) name,\n"
        "    WIDGET_KIND_LIST(WIDGET_KIND_ENUMERATOR)\n"
        "#undef WIDGET_KIND_ENUMERATOR\n"
        "};\n"
        "// #undef WIDGET_KIND_LIST -- deliberately missing, same shape as the\n"
        "// real 27/08/2026 experiment against include/glintfx/gfss/token.hpp\n",
    )
    leaks, _defines = classify_header(path)
    if not leaks:
        print("selftest: controle NEGATIVO FALHOU (esperava vazamento de WIDGET_KIND_LIST, nao achou nada)", file=sys.stderr)
        return False
    if not any(leak["name"] == "WIDGET_KIND_LIST" and leak["file"] == path for leak in leaks):
        print(f"selftest: controle NEGATIVO FALHOU (vazamento errado: {leaks})", file=sys.stderr)
        return False
    if len(leaks) != 1:
        print(f"selftest: controle NEGATIVO FALHOU (esperava exatamente 1 vazamento, achou {leaks})", file=sys.stderr)
        return False
    print(
        f"selftest: controle NEGATIVO OK (WIDGET_KIND_LIST reprovado, citando "
        f"{leaks[0]['file']}:{leaks[0]['line']})"
    )
    return True


# Empty-scan floor control: an include_dir with zero headers must be
# REFUSED, never presumed clean (GODS_LAWS.md L-40).
def selftest_empty_scan_control(scratch):
    include_dir = os.path.join(scratch, "empty_scan", "include")
    os.makedirs(include_dir, exist_ok=True)

    import contextlib
    import io

    buffer = io.StringIO()
    with contextlib.redirect_stdout(buffer), contextlib.redirect_stderr(buffer):
        result = check_macro_balance(include_dir)
    text = buffer.getvalue()

    if result:
        print("selftest: controle de VARREDURA VAZIA FALHOU (deveria recusar include_dir sem headers, mas passou)", file=sys.stderr)
        return False
    if "varredura vazia" not in text:
        print("selftest: controle de VARREDURA VAZIA FALHOU (recusou, mas nao disse 'varredura vazia')", file=sys.stderr)
        print(text, file=sys.stderr)
        return False
    print("selftest: controle de VARREDURA VAZIA OK (include_dir sem headers recusado)")
    return True


# Inverse-of-the-obvious control (the orquestrador's own warning: "nem
# toda macro em cabecalho publico e vazamento"): a header with NO
# #define at all must pass cleanly, contributing 0 to the macro count,
# never mistaken for an empty scan (there IS a header, it just declares
# no macro - a real, common shape: 8 of the 11 real tracked headers
# today have zero #define, measured live by check_macro_balance() itself
# against PROJECT_SOURCE_DIR/include in the real run this selftest does
# not touch).
def selftest_header_without_macros_control(scratch):
    include_dir = make_fixture_include_dir(scratch, "no_macros")
    _write(
        os.path.join(include_dir, "plain.hpp"),
        "#pragma once\n#include <cstdint>\n\nstruct plain {\n    std::uint32_t value = 0;\n};\n",
    )
    leaks, defines = classify_header(os.path.join(include_dir, "plain.hpp"))
    if leaks or defines != 0:
        print(f"selftest: controle SEM MACRO FALHOU (esperava 0 defines/0 leaks, achou defines={defines} leaks={leaks})", file=sys.stderr)
        return False
    print("selftest: controle SEM MACRO OK (header sem nenhum #define nao e' confundido com vazamento nem com varredura vazia)")
    return True


def _run_all_controls(scratch):
    return [
        selftest_positive_control(scratch),
        selftest_negative_control(scratch),
        selftest_empty_scan_control(scratch),
        selftest_header_without_macros_control(scratch),
    ]


def selftest_main():
    scratch = make_scratch_workdir()
    try:
        controls = _run_all_controls(scratch)
        if not all(controls):
            print("check_macro_balance.py --selftest: FALHOU (ver acima)", file=sys.stderr)
            sys.exit(1)
        print(f"check_macro_balance.py --selftest: os {len(controls)} controles OK")
    finally:
        import shutil

        shutil.rmtree(scratch, ignore_errors=True)


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
    else:
        real_main(args)


if __name__ == "__main__":
    main()
