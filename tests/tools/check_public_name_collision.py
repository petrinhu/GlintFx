#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_public_name_collision.py - CI gate for docs/api-conventions.md
# R6 ("Nome publico nunca colide com macro de sistema nem com nome ja
# usado pela biblioteca padrao"). Scans two real things: the public
# headers this build actually ships (include/), and the SYSTEM headers
# the compiler actually searches - not a curated /usr/include grep.
#
# PORT of the former tests/tools/check_public_name_collision.sh (POSIX
# sh + tests/tools/enumerate_names.awk), done in the same fatia that
# finished GATE-TREE-PARITY (GODS_LAWS.md L-04, decisao do lider: "O
# comportamento deve ser igual em qualquer OS"). enumerate_names.awk is
# DELETED in this same fatia: its five extraction rules are
# reimplemented natively in Python below (enumerate_names_in_text()),
# so there is no separate scratch file to write and keep in sync with
# this script - one fewer moving part than the sh version had.
#
# ORIGIN (TODO.md, "Desvios", 25/08/2026): the CORE-ERROR adversarial
# review swept /usr/include for real and found a collision candidate
# ("value", a p11-kit PKCS11 macro) a hand-typed list never had a
# chance to name. "o valor do item nao e aquele nome, e o metodo: lista
# curada envelhece e depende de alguem lembrar, enquanto varredura real
# do sistema enumera" (GODS_LAWS.md L-40).
#
# WHAT "OUR NAMES" MEANS HERE, DECLARED (policy decision 1 of 3): every
# *.hpp/*.h/*.hh/*.hxx under <include_dir> is enumerated - not the .cpp
# implementation files. Only include/ is text the CONSUMER's
# preprocessor ever sees. Four categories are extracted MECHANICALLY:
# type names (class/struct/enum class), enumerator names, function/
# method names (the FIRST identifier immediately before an opening
# paren on a line that looks like a declaration - GLINTFX_API,
# noexcept, [[nodiscard]], inline, static or explicit somewhere on it -
# deliberately the FIRST match only, so a constructor's member-init-
# list or a one-line getter's body call are never mistaken for a name
# THIS library declared), and data-member names (a bare "TYPE name;"
# line with no parens). Declared limitation, not silently dropped:
# function PARAMETER names are NOT scanned.
#
# WHY std::-INHERITED METHOD NAMES ARE DELIBERATELY OUT OF SCOPE
# (policy decision 1, continued - measured live, not assumed): an
# earlier version of the sh sibling's own enumerate_names.awk captured
# EVERY identifier before a paren on a declaration line, not just the
# first, and it found a REAL system collision this way -
# std::variant::index(), called inside gltfx_rslt's one-line
# has_value()/has_error() getters, collides with a `#define index(s,c)
# ...` in the legacy X-Window system's own "Xos dot h" header (spelled
# out in prose here, not as a literal path string, on purpose -
# tests/tools/check_no_x11.py greps this whole repository for that
# system's header paths, GODS_LAWS.md L-05, and a literal mention here
# would trip a false positive). That name is not ours to rename: this
# project does not choose std::variant's method names. The FIRST-
# match-only rule closes this cleanly.
#
# WHAT COUNTS AS A COLLISION, DECLARED (policy decision 2): a system
# header #define-ing one of our names is a FAILURE, UNLESS one of THREE
# neutralizing shapes applies. First: that same file also #undef's the
# same name later. Second: the #define is textually nested inside
# "#ifdef GUARD"/"#if defined(GUARD)" for a symbol nothing in a NORMAL
# include chain ever defines, so the macro never becomes active in
# default preprocessing - proved by asking the SAME real compiler
# (macro_active_under_default_preprocessing_gcc/_msvc below), never a
# hand-rolled nested-#ifdef parser. Third: the matched FILE itself is
# not valid C/C++ preprocessor text at all (PCP's own /usr/include/pcp/
# builddefs, a Makefile fragment sharing a directory with real headers)
# - proved the same way, by asking the compiler. None of the three
# shapes is silently dropped - classify_matches() below tags EACH
# neutralized line with WHICH of the three reasons applied (GODS_
# LAWS.md L-40's "a contagem aparece na saida, mesmo quando passa").
#
# WHERE THIS SCANS, DECLARED (policy decision 3): the compiler's OWN
# documented system header search path - never a hardcoded /usr/include
# or a hardcoded Program Files path. Two different, frontend-specific
# ways to ask that same question, both below (discover_system_include_
# dirs_gcc/_msvc): GNU/Clang expose it only by asking the compiler
# process itself (`${cxx} -E -Wp,-v -xc++ -`); MSVC exposes it directly
# as the INCLUDE environment variable (see NAMES-PARITY-WIN below) - no
# compiler invocation needed to read it. If discovery yields ZERO
# directories - compiler missing, INCLUDE unset, or a broken toolchain
# - this is a HARD FAILURE (require_nonempty_scan below), never a
# silent skip.
#
# GATE-TREE-PARITY (GODS_LAWS.md L-04, revisao de 04/09/2026 - o
# registro anterior desta fatia escondia o MSVC atras de
# `if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")` no CMake, com um
# `message(STATUS ...)` que so aparece no log de configuracao - nenhum
# inventario de teste (`ctest -N`) o enxergava. Corrigido registrando o
# teste INCONDICIONALMENTE nas cinco plataformas, com o script decidindo
# em tempo de execucao (commit ed14ba2's own precedent, tests/tools/
# check_spdx.py: "ausencia declarada e CONTADA, nunca pulo silencioso").
# A PRIMEIRA versao dessa correcao (commit bb181ce) ainda deixava o
# MSVC como "declarado NAO APLICAVEL": o mecanismo GCC-frontend
# (`-E`/`-dM`/`-Wp,-v`/`-xc++`) genuinamente nao tem equivalente
# documentado no MSVC, mas isso nao significava que o PORTAO em si era
# inviavel no MSVC - so que a TECNICA usada do lado GNU/Clang era.
#
# NAMES-PARITY-WIN (GODS_LAWS.md L-04, tarefa do lider, 04/09/2026):
# "O portao de colisao de nome publico nunca varre os cabecalhos do
# sistema do Windows... Par: varrer o SDK na perna Windows." Pesquisado
# (WebSearch, mesma data) antes de tentar, por forca da L-22:
#
#   1. DESCOBERTA DO CAMINHO DE BUSCA (equivalente a `-Wp,-v -xc++ -`):
#      cl.exe documenta e LE diretamente a variavel de ambiente
#      INCLUDE para resolver "#include <...>" - Microsoft Learn, "CL
#      environment variables" (learn.microsoft.com/en-us/cpp/build/
#      reference/cl-environment-variables). Mais direto ainda que o
#      lado GCC: nao precisa nem invocar o compilador para descobrir -
#      so ler a variavel, ja populada pelo `vcvarsall.bat x64` que
#      .github/workflows/ci.yml's own "Preparar ambiente do compilador
#      (MSVC x64)" (CI-WIN-ENV) roda ANTES de qualquer `cmake -S/-B` do
#      job Windows, exportando cada variavel resultante (INCLUDE
#      incluida) via $env:GITHUB_ENV para todos os passos seguintes -
#      logo tambem para o passo `ctest` que executa este proprio
#      script. discover_system_include_dirs_msvc() abaixo so le
#      os.environ["INCLUDE"].
#
#   2. "ESTE NOME ESTA #define'D ATIVO" (equivalente a `-E -dM -xc++`):
#      MSVC nao documenta nenhuma flag equivalente a `-dM` (pesquisado:
#      nao existe "/PD" nem qualquer outra opcao de despejo de macros
#      documentada para cl.exe). Mas a PERGUNTA que classify_matches()
#      faz nao e "liste todas as macros" - e' "esta ESTE nome
#      #define'd, sim ou nao, apos incluir este arquivo" - e essa
#      pergunta tem resposta direta e padrao em qualquer preprocessador
#      C/C++, incluindo o de cl.exe: a propria diretiva `#ifdef NAME`.
#      classify_matches_msvc_batched() abaixo escreve (via
#      _write_msvc_batch_probe()) um arquivo-sonda descartavel que
#      inclui o cabecalho suspeito e testa `#ifdef NAME`, preprocessado
#      com `/E /TP` (ambas opcoes
#      documentadas e comuns do cl.exe - "/E", "preprocess to stdout";
#      "/TP", "compile all files as .cpp"), procurando por um dos dois
#      marcadores-sentinela na saida. Isto pergunta EXATAMENTE a mesma
#      coisa que o despejo `-dM` do lado GCC (uma macro conta como
#      "ativa" seja ela object-like ou function-like, mesma decisao de
#      politica 2 acima) - e e ainda MAIS direto que uma abordagem por
#      substituicao textual, que erraria uma macro function-like nunca
#      seguida de "(" nesta sonda; `#ifdef` nao tem esse problema em
#      NENHUM dos dois compiladores.
#
#   3. "ESTE ARQUIVO NAO E' CABECALHO C/C++ VALIDO" (equivalente ao
#      "invalid preprocessing directive" do GCC): MSVC nomeia o MESMO
#      defeito - uma linha "#palavra" que nao e' uma diretiva
#      reconhecida - como erro fatal C1021, "invalid preprocessor
#      command '<palavra>'" (Microsoft Learn, "Fatal Error C1021",
#      learn.microsoft.com/en-us/cpp/error-messages/compiler-errors-1/
#      fatal-error-c1021). classify_matches_msvc_batched() abaixo
#      procura pelo CODIGO "C1021" na saida, atribuido a' sonda exata
#      que o emitiu (nunca ao texto ao redor dele, que e' traduzido por
#      locale) - mais robusto ainda que o `LC_ALL=C` que o lado GCC
#      precisa forcar para o mesmo motivo.
#
# VEREDITO: VIAVEL, com fonte. Os tres mecanismos acima sao usados por
# discover_system_include_dirs_msvc() e classify_matches_msvc_batched()
# abaixo (que decide "ativo" e "nao e' cabecalho" para todo o lote), e
# public_name_collision_test PASSA A SER EXERCIDO de verdade no MSVC,
# nao mais declarado nao aplicavel - cxx_frontend() abaixo so retorna
# "unavailable" quando o escape hatch de teste (ver logo adiante) esta
# forcado; nenhuma id de compilador real cai la por conta propria mais,
# ja que .github/workflows/ci.yml's own matrix so produz "GNU" (Fedora/
# Ubuntu/Arch/CachyOS) e "MSVC" (Windows), e os dois sao suportados
# agora.
#
# Escape hatch para EXERCITAR o caminho de ausencia declarada fora de
# um cenario real (mesmo raciocinio do
# GLINTFX_SPDX_SELFTEST_FORCE_WINDOWS_HOSTILE_SKIP de check_spdx.py):
# GLINTFX_PNC_FORCE_GCC_FRONTEND_UNAVAILABLE=1 forca cxx_frontend() a
# responder "unavailable" mesmo com um cxx_id real e suportado - o
# UNICO jeito de alcancar esse caminho agora que GNU/Clang e MSVC sao
# ambos suportados por conta propria; existe apenas para provar que o
# codigo de ausencia declarada continua correto, nunca porque alguma
# plataforma real precise dele.
#
# BATCH-PARITY-WIN (GODS_LAWS.md L-04/L-11/L-36, achado real de
# 05/09/2026): NAMES-PARITY-WIN acima tornou este portao EXERCIDO de
# verdade no MSVC - e a primeira rodada real dele no servidor (run
# 33949568634) estourou o teto de 120s do teste em UMA das tres pernas
# Windows (56.19s na outra, mesma varredura - variancia grande demais
# para ser coincidencia). A causa: classify_matches() original fazia
# ATE DUAS invocacoes NOVAS de cl.exe POR CANDIDATO textual (uma para
# file_is_not_c_or_cpp_header_msvc(), outra para
# macro_active_under_default_preprocessing_msvc()) - no Linux isso e
# barato (3 candidatos medidos, ~0.01s por chamada de gcc), mas no SDK
# do Windows o numero de candidatos textuais e maior E cada novo
# processo cl.exe custa ordens de grandeza mais (antivirus do executor
# escaneia cada .exe novo, GODS_LAWS.md L-49's own "log de dentro bate
# o de fora" nao se aplica aqui, mas o principio de medir antes de
# consertar sim).
#
# O REMEDIO OBVIO - uma invocacao so, em lote, com todos os candidatos -
# resolve GODS_LAWS.md L-11 (nunca um processo por item varrido) mas
# ABRE um risco NOVO sob GODS_LAWS.md L-36: se aquela UNICA invocacao
# morrer no meio (um candidato cujo #include produz algo que o
# preprocessador nao sobrevive), os candidatos QUE VIRIAM DEPOIS dele
# no mesmo lote perderiam veredito - EM SILENCIO, se ninguem conferir.
# classify_matches_msvc_batched() abaixo concilia as duas leis, nunca
# escolhendo uma contra a outra:
#
#   1. Cada arquivo candidato vira UMA sonda .cpp propria (um #include
#      so, mais um "#ifdef NOME" por nome candidato daquele arquivo,
#      cada um com um marcador GLOBALMENTE unico - GLINTFX_PNC_BATCH_
#      <indice>_ACTIVE/_INACTIVE). Varios arquivos, cada um sua PROPRIA
#      sonda, viram argumentos de linha de comando SEPARADOS de UMA SO
#      chamada de cl.exe (`cl /E /TP sonda1.cpp sonda2.cpp ...`) - cada
#      argumento de fonte e uma translation unit independente para o
#      compilador, entao um erro fatal preprocessando a sonda 2 nao
#      corrompe a saida da sonda 1 nem impede que a saida da sonda 3
#      apareca (isto E' o mesmo principio por tras de "cl a.cpp b.cpp
#      c.cpp" processar os tres mesmo se um deles falhar - cada fonte e
#      seu proprio compile, so o DRIVER e compartilhado). Arquivos sao
#      agrupados em lotes de ate _MSVC_BATCH_CHUNK_SIZE por chamada,
#      so' para limitar o raio de um cenario que este portao nunca
#      observou de verdade e nao pode provar que nao acontece: cl.exe
#      abortando a invocacao inteira em vez de seguir para o proximo
#      argumento de fonte.
#   2. A reconciliacao, que e' a parte que fecha L-36: TODO candidato
#      enviado tem que terminar com um veredito (REAL ou NEUTRALIZADO)
#      OU aparecer, nomeado, na lista `missing` que
#      classify_matches_msvc_batched() devolve. check_public_name_
#      collision() abaixo trata QUALQUER `missing` nao-vazio como falha
#      dura, imprimindo cada candidato faltante - nunca um lote que
#      morreu no meio vira aprovacao silenciosa, e nunca vira uma
#      "colisao real" fantasma tambem (sao categorias distintas na
#      saida).
#
# O QUE ISSO NAO FAZ: nao tenta recuperar os candidatos de `missing`
# reprocessando-os um a um como fallback - a falha e reportada alta e
# nomeada, por pedido explicito de quem escreveu este briefing (a
# alternativa de recuperacao automatica existe e teria zero perda de
# cobertura tambem, mas esconderia que o lote e' fragil justamente
# quando ele o for).
#
# Usage:
#   check_public_name_collision.py <include_dir> <cxx-compiler> <cxx-compiler-id>
#   check_public_name_collision.py --selftest [cxx-compiler] [cxx-compiler-id]
#
# --selftest roda os dez controles (GODS_LAWS.md L-40: positivo,
# negativo, vazio - mais sete especificos deste portao, entre eles o
# controle de ATRIBUICAO-VS-DECLARACAO, achado real de 04/09/2026, e o
# de RECONCILIACAO DE LOTE, achado real de 05/09/2026, BATCH-PARITY-
# WIN) usando o MECANISMO do frontend detectado por cxx_frontend(cxx_id)
# - GCC/Clang (classify_matches) ou MSVC batched (classify_matches_
# msvc_batched) - sempre contra fixtures descartaveis sob
# tempfile.mkdtemp, nunca contra o include_dir real nem os cabecalhos
# de sistema reais da maquina. Quando o escape hatch esta ativo,
# nenhum dos dez e' exercitavel - o autoteste declara isso, conta 1/1
# como nao aplicavel, e passa.
#
# Each function below does one thing (GODS_LAWS.md L-17).

import os
import re
import shutil
import subprocess
import sys
import tempfile

SCRIPT_NAME = "check_public_name_collision.py"

_HEADER_EXTENSIONS = (".hpp", ".h", ".hh", ".hxx")

_FORCE_GCC_FRONTEND_UNAVAILABLE_ENV = "GLINTFX_PNC_FORCE_GCC_FRONTEND_UNAVAILABLE"

_UNAVAILABLE_REASON = (
    "o escape hatch de teste GLINTFX_PNC_FORCE_GCC_FRONTEND_UNAVAILABLE=1 esta ativo, "
    "forcando esta execucao a se comportar como se nenhum mecanismo de introspeccao de "
    "macro estivesse disponivel - nenhuma id de compilador real cai mais aqui por conta "
    "propria: GNU/Clang (via '-E'/'-dM'/'-Wp,-v'/'-xc++') e MSVC (via a variavel de "
    "ambiente INCLUDE mais uma sonda '#ifdef' preprocessada com '/E'/'/TP') sao ambos "
    "suportados agora (GODS_LAWS.md L-04, NAMES-PARITY-WIN, ver o cabecalho deste arquivo)"
)

_UNAVAILABLE_COVERAGE = (
    "condicao sintetica, so para provar que o caminho de ausencia declarada continua "
    "correto - nao ha lacuna de cobertura real por tras dela em nenhuma plataforma que "
    ".github/workflows/ci.yml's own matrix produz hoje"
)


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


def cxx_frontend(cxx_id):
    """Which preprocessor-introspection technique this gate uses for the
    compiler identified by `cxx_id` (CMAKE_CXX_COMPILER_ID). "gcc"
    covers GNU and Clang, AND any unrecognized future id (fail toward
    EXERCISING the check, never toward silently skipping it, GODS_
    LAWS.md L-40); "msvc" is MSVC (cl.exe); "unavailable" is reachable
    ONLY through the env-var escape hatch above - no compiler id
    answers it on its own anymore, now that both ids .github/workflows/
    ci.yml's own matrix produces are supported (NAMES-PARITY-WIN, see
    this file's own header).
    """
    if os.environ.get(_FORCE_GCC_FRONTEND_UNAVAILABLE_ENV) == "1":
        return "unavailable"
    if cxx_id == "MSVC":
        return "msvc"
    return "gcc"


def print_frontend_unavailable(prefix):
    print(
        f"{prefix}: 1 caso, 0 exercido(s) aqui, 1 declarado NAO APLICAVEL nesta "
        f"execucao: {_UNAVAILABLE_REASON}. {_UNAVAILABLE_COVERAGE}."
    )


# --- our names: enumeration from include_dir -------------------------


def enumerate_public_headers(include_dir):
    files = []
    for dirpath, _dirnames, filenames in os.walk(include_dir):
        for name in filenames:
            if name.endswith(_HEADER_EXTENSIONS):
                files.append(os.path.join(dirpath, name))
    return sorted(files)


def strip_line_comments(text):
    """Strips a trailing "// ..." comment from every line before name
    extraction - without this, a NOLINT comment's own prose could
    contain a word matching an extraction pattern by accident. No
    /* */ block comments exist in this project's public headers (same
    declared limitation the sh sibling's own header carried).
    """
    return "\n".join(re.sub(r"//.*", "", line) for line in text.splitlines())


_ENUM_CLASS_RE = re.compile(r"^\s*enum\s+class\s+([A-Za-z_][A-Za-z0-9_]*)")
_ENUM_CLOSE_RE = re.compile(r"^\s*\}")
_ENUMERATOR_RE = re.compile(r"^([A-Za-z_][A-Za-z0-9_]*)")
_CLASS_STRUCT_RE = re.compile(r"(?:^|\s)(?:class|struct)\s+(?:\[\[nodiscard\]\]\s+)?([A-Za-z_][A-Za-z0-9_]*)")
_FUNC_NAME_RE = re.compile(r"([A-Za-z_][A-Za-z0-9_]*)\s*\(")
_DECL_WORD_MARKERS = ("inline", "static", "explicit")
_DECL_TEXT_MARKERS = ("noexcept", "GLINTFX_API", "[[nodiscard]]")
_FUNC_NAME_EXCLUDED = frozenset({"if", "noexcept", "sizeof", "static_assert", "explicit"})
_MEMBER_TAIL_RE = re.compile(r"([A-Za-z_][A-Za-z0-9_]*)\s*;\s*$")
_MEMBER_EXCLUDED_PREFIX_RE = re.compile(
    r"^\s*(using|typedef|namespace|template|return|break|continue|if|for|while|else)(\s|;|\*)"
)


def _line_is_declaration(line):
    if any(marker in line for marker in _DECL_TEXT_MARKERS):
        return True
    return any(re.search(rf"(^|\s){word}(\s|$)", line) for word in _DECL_WORD_MARKERS)


def enumerate_names_in_text(text):
    """Reimplements, natively in Python, the five extraction rules the
    sh sibling's own enumerate_names.awk used to encode as a separate
    scratch file: enum-class enumerators, class/struct/enum-class type
    names, function/method declarations (first identifier before an
    opening paren on a line that LOOKS like a declaration), and bare
    data-member declarations - never a plain assignment to a name that
    already exists elsewhere ("errno = 0;" is not a declaration of
    "errno", see the comment at the member-tail check below). Line-by-
    line, same control flow as the awk program (an "in_enum" state
    machine, blank lines skipped entirely, at most one BODY rule
    applied per non-enum line).
    """
    names = []
    in_enum = False
    for raw_line in text.splitlines():
        if raw_line.strip() == "":
            continue

        enum_match = _ENUM_CLASS_RE.match(raw_line)
        if enum_match:
            names.append(enum_match.group(1))
            if "{" in raw_line:
                in_enum = True
            continue

        if in_enum and _ENUM_CLOSE_RE.match(raw_line):
            in_enum = False
            continue

        if in_enum:
            enumerator_match = _ENUMERATOR_RE.match(raw_line.lstrip())
            if enumerator_match:
                names.append(enumerator_match.group(1))
            continue

        class_match = _CLASS_STRUCT_RE.search(raw_line)
        if class_match:
            names.append(class_match.group(1))

        if "(" in raw_line:
            if _line_is_declaration(raw_line):
                func_match = _FUNC_NAME_RE.search(raw_line)
                if func_match and func_match.group(1) not in _FUNC_NAME_EXCLUDED:
                    names.append(func_match.group(1))
        elif raw_line.rstrip().endswith(";"):
            without_default = re.sub(r"=.*;", ";", raw_line)
            if not _MEMBER_EXCLUDED_PREFIX_RE.match(without_default):
                member_match = _MEMBER_TAIL_RE.search(without_default)
                # ASSIGNMENT-VS-DECLARATION (TODO.md, achado real de
                # 04/09/2026): stripping "= value" from a plain
                # assignment like "errno = 0;" leaves "errno;" - the
                # EXACT same shape as the tail of a genuine bare
                # declaration once its own initializer is stripped, so
                # _MEMBER_TAIL_RE alone cannot tell them apart. The
                # difference is what comes BEFORE the name: a real
                # declaration always has a TYPE there ("int contents;",
                # "const int read_errno = errno;" -> "const int
                # read_errno;"); a bare assignment to a name that
                # already exists (a system macro like errno, a member
                # set in a later statement) has NOTHING before it. The
                # `.strip()` check below requires that non-empty prefix
                # - it does NOT touch the declaration path: this file's
                # own "LOCAL VARIABLE NAMED contents" comment in
                # include/glintfx/platform/asset/file.hpp documents that
                # a bare "TYPE name;" line inside an INLINE function
                # body must still be caught, and it still is, since
                # "contents"/"read_errno" both keep a type prefix.
                if member_match and without_default[: member_match.start()].strip():
                    names.append(member_match.group(1))

    return names


def enumerate_our_names(include_dir):
    names = set()
    for header in enumerate_public_headers(include_dir):
        try:
            with open(header, "r", encoding="utf-8", errors="replace") as handle:
                text = handle.read()
        except OSError as exc:
            print(f"{SCRIPT_NAME}: {header}: open refused ({exc})", file=sys.stderr)
            continue
        names.update(enumerate_names_in_text(strip_line_comments(text)))
    return sorted(names)


# --- system side: where to look (frontend-specific discovery) ---------


def discover_system_include_dirs_gcc(cxx):
    """GCC-frontend convention (see this script's own header, policy
    decision 3): the search path a real compile would use, never a
    hardcoded /usr/include.
    """
    try:
        proc = subprocess.run(
            [cxx, "-E", "-Wp,-v", "-xc++", "-"],
            input="",
            capture_output=True,
            text=True,
            check=False,
        )
    except OSError:
        return []
    combined = proc.stdout + proc.stderr
    dirs = []
    on = False
    for line in combined.splitlines():
        if "#include <...> search starts here:" in line:
            on = True
            continue
        if "End of search list." in line:
            on = False
            continue
        if on:
            dirs.append(line.lstrip(" "))
    return dirs


def discover_system_include_dirs_msvc():
    """MSVC-frontend equivalent (NAMES-PARITY-WIN, see this file's own
    header): the compiler's OWN documented, exported search path for
    angle-bracket "#include <...>" resolution is the INCLUDE
    environment variable (Microsoft Learn, "CL environment variables")
    - cl.exe reads it directly, and this project's own .github/
    workflows/ci.yml populates it for every step of the windows job via
    vcvarsall.bat x64 (see that file's own "Preparar ambiente do
    compilador (MSVC x64)" / CI-WIN-ENV step). No compiler invocation
    needed to read it, unlike discover_system_include_dirs_gcc() above.
    Semicolon-separated (Windows PATH-list convention); an entry that
    does not exist on disk is dropped here rather than failing -
    require_nonempty_scan() at the call site is what turns a
    genuinely empty/missing INCLUDE into a hard failure, never a silent
    skip.
    """
    raw = os.environ.get("INCLUDE", "")
    return [d for d in raw.split(";") if d and os.path.isdir(d)]


def discover_system_include_dirs(frontend, cxx):
    if frontend == "msvc":
        return discover_system_include_dirs_msvc()
    return discover_system_include_dirs_gcc(cxx)


def list_files_under_dirs(dirs):
    files = []
    for d in dirs:
        if d and os.path.isdir(d):
            for dirpath, _dirnames, filenames in os.walk(d):
                for name in filenames:
                    files.append(os.path.join(dirpath, name))
    return files


def scan_defines_in_files(files, names):
    """Every "#define NAME..." (object-like or function-like) in any
    of <files>, for any of <names>. Returns a list of (file, lineno,
    name) - already CLASSIFIED as a candidate match, not yet REAL vs
    NEUTRALIZED (classify_matches() below does that). Frontend-
    agnostic: a plain text scan, identical on every platform.
    """
    if not names:
        return []
    alternation = "|".join(re.escape(n) for n in names)
    pattern = re.compile(rf"^\s*#\s*define\s+({alternation})\b")
    matches = []
    for path in files:
        try:
            with open(path, "r", encoding="utf-8", errors="replace") as handle:
                for lineno, line in enumerate(handle, start=1):
                    m = pattern.match(line)
                    if m:
                        matches.append((path, lineno, m.group(1)))
        except OSError:
            continue
    return matches


# A #define is NEUTRALIZED (policy decision 2) if the SAME file also
# #undef's the SAME name, anywhere in the file. Frontend-agnostic: a
# plain text scan.
def name_is_undef_in_same_file(file_path, name):
    pattern = re.compile(rf"^\s*#\s*undef\s+{re.escape(name)}\b")
    try:
        with open(file_path, "r", encoding="utf-8", errors="replace") as handle:
            return any(pattern.match(line) for line in handle)
    except OSError:
        return False


# --- GCC-frontend neutralization mechanics (asks the compiler) --------


# A #define is NEUTRALIZED under a SECOND, independent reason (policy
# decision 2, second shape): the line is textually nested inside
# "#ifdef GUARD"/"#if defined(GUARD)" for a symbol nothing in a NORMAL
# include chain ever defines, so the macro never becomes active. This
# does NOT hand-parse nested #ifdef/#elif/#endif - it asks the SAME
# compiler this whole gate already trusts for the search path to
# preprocess that ONE file standalone (`-E -dM`, dumping every macro
# alive at end-of-file) and checks whether NAME is in that dump. If the
# compiler cannot even preprocess the file standalone, the function
# answers "active" (GODS_LAWS.md L-40: "recusar alto e melhor que
# aprovar em silencio" - an unprovable case must never quietly turn
# into a neutralization).
def macro_active_under_default_preprocessing_gcc(file_path, name, cxx):
    try:
        proc = subprocess.run(
            [cxx, "-E", "-dM", "-xc++", file_path],
            capture_output=True,
            text=True,
            check=False,
        )
    except OSError:
        return True
    if proc.returncode != 0 or not proc.stdout:
        return True
    pattern = re.compile(rf"^#define\s+{re.escape(name)}\b")
    return any(pattern.match(line) for line in proc.stdout.splitlines())


# THIRD neutralizing reason (policy decision 2, third shape): the
# matched FILE itself is not valid C/C++ preprocessor text at all - a
# Makefile fragment (PCP's own builddefs) that happens to live inside a
# directory the compiler's own search path also uses for real headers,
# whose comment lines are BY COINCIDENCE syntactically legal #define
# directives. The mechanical, compiler-verified signal: a real header -
# even one this gate cannot fully preprocess standalone - NEVER
# contains a line starting with "#" that is not a recognised directive
# keyword; only a file using "#" as an ordinary prose/comment marker
# does, and the compiler calls that out BY NAME ("invalid preprocessing
# directive"). LC_ALL=C forces English so this does not depend on the
# machine's locale - the same reason this whole gate asks the REAL
# compiler instead of curating a list.
def file_is_not_c_or_cpp_header_gcc(file_path, cxx):
    env = dict(os.environ)
    env["LC_ALL"] = "C"
    try:
        proc = subprocess.run(
            [cxx, "-E", "-dM", "-xc++", file_path],
            capture_output=True,
            text=True,
            check=False,
            env=env,
        )
    except OSError:
        return False
    return "invalid preprocessing directive" in proc.stderr


# --- MSVC-frontend neutralization mechanics (NAMES-PARITY-WIN) --------

def _write(path, text):
    with open(path, "w", encoding="utf-8") as handle:
        handle.write(text)


def _msvc_probe_scratch_dir():
    return tempfile.mkdtemp(prefix="glintfx-pnc-msvc-probe-", dir=os.environ.get("TMPDIR"))


_MSVC_BATCH_CHUNK_SIZE = 40
_MSVC_BATCH_MARKER_PREFIX = "GLINTFX_PNC_BATCH"


def _msvc_batch_marker(idx):
    return f"{_MSVC_BATCH_MARKER_PREFIX}_{idx}"


def _write_msvc_batch_probe(probe_path, file_path, entries):
    """Writes ONE probe .cpp for `file_path`, testing every (name, idx)
    pair in `entries` (idx is a running counter UNIQUE ACROSS THE WHOLE
    BATCH, not just this file - see classify_matches_msvc_batched()
    below, and this file's own header, BATCH-PARITY-WIN). ONE #include
    (forward slashes - see this function's own predecessor's note,
    still true: backslashes inside a "#include \"...\"" string are, in
    practice, treated as literal separators by every mainstream
    compiler, but forward slashes sidestep the ambiguity entirely),
    then one "#ifdef NAME" per entry, each with its own marker. If the
    #include itself is not valid C/C++ preprocessor text, NONE of this
    probe's markers ever appear in the compiler's output - the caller
    distinguishes that case by asking the compiler DIRECTLY which probe
    file it names in a C1021 diagnostic (attributable, unlike the old
    per-candidate mechanism's fail-closed "active" default for this
    same defect - GODS_LAWS.md L-40 still applies when NEITHER a marker
    NOR an attributable C1021 shows up for an entry: that entry is left
    unresolved, never silently guessed).
    """
    include_path = file_path.replace("\\", "/")
    lines = [f'#include "{include_path}"\n']
    for name, idx in entries:
        marker = _msvc_batch_marker(idx)
        lines.append(f"#ifdef {name}\n{marker}_ACTIVE\n#else\n{marker}_INACTIVE\n#endif\n")
    _write(probe_path, "".join(lines))


# Attribution helper for the fatal-error case (achado real de 05/09/2026,
# ver a nota "ATTRIBUTION FIX" no docstring de classify_matches_msvc_
# batched() logo abaixo). O C1021 de uma diretiva invalida DENTRO do
# arquivo incluido e' reportado pelo cl.exe contra ESSE arquivo - a
# mesma string usada no "#include" da sonda (_write_msvc_batch_probe()
# acima escreve sempre com "/", mas o compilador pode ecoar de volta
# com o separador nativo do SO em algum diagnostico) - nunca contra o
# nome do .cpp wrapper que fez o include. As duas grafias sao checadas
# porque este portao nao pode provar, sem um cl.exe real nesta maquina,
# qual delas aquele compilador de fato usa para um arquivo ANINHADO
# (mesma limitacao ja registrada no docstring de classify_matches_msvc_
# batched() para "o compilador continua processando os demais arquivos
# apos um erro fatal num deles" - inferencia de arquitetura, nunca fato
# provado por fonte primaria aqui).
def _c1021_names_included_file(file_path, combined_output):
    forward_slash = file_path.replace("\\", "/")
    back_slash = file_path.replace("/", "\\")
    for spelling in {forward_slash, back_slash}:
        if re.search(rf"{re.escape(spelling)}\(\d+\)[^\n]*C1021", combined_output):
            return True
    return False


def classify_matches_msvc_batched(matches, cxx, probe_scratch):
    """MSVC-frontend batched classification (this file's own header,
    BATCH-PARITY-WIN, 05/09/2026): replaces up to TWO fresh cl.exe
    PROCESSES per textual candidate with as few invocations as this
    gate can make, while proving no candidate's verdict silently
    vanished (GODS_LAWS.md L-36).

    Matches already neutralized by name_is_undef_in_same_file() (a pure
    text check, no compiler needed) are classified immediately, same as
    classify_matches() above. Everything else is grouped by FILE - one
    probe per unique file, covering every name matched in it - and
    every file's probe becomes its OWN command-line source argument to
    ONE cl.exe invocation (`cl /E /TP probe1.cpp probe2.cpp ...`): each
    source argument is its own independent translation unit to the
    compiler, so a fatal error preprocessing one probe does not corrupt
    another probe's own output in the SAME invocation - the same reason
    "cl a.cpp b.cpp c.cpp" processes all three even if one of them
    fails to compile. Files are chunked at `_MSVC_BATCH_CHUNK_SIZE` per
    invocation only to bound the blast radius of a scenario this gate
    has never observed for real and cannot prove will not happen: the
    WHOLE invocation aborting instead of moving on to the next source
    argument.

    Returns (classified, missing): `classified` has the same shape
    classify_matches() above returns. `missing` is a list of
    (file, lineno, name) tuples that received NEITHER a marker NOR an
    attributable C1021 - GODS_LAWS.md L-36's own reconciliation: the
    caller (check_public_name_collision() below) treats ANY non-empty
    `missing` as a hard, named failure, never a silent pass-through and
    never a false "REAL collision" either.

    ATTRIBUTION FIX (achado real de 05/09/2026, servidor run 33951517070,
    o unico autoteste que quebrou depois de BATCH-PARITY-WIN): a
    verificacao original procurava o PROBE WRAPPER's own basename
    (`batch_0_0.cpp`) perto de "C1021" na saida combinada. Isso nunca
    casava, porque um C1021 disparado por uma diretiva invalida DENTRO
    do arquivo INCLUIDO e' reportado pelo cl.exe contra o arquivo
    incluido (a string exata do "#include" que o trouxe, ver
    _write_msvc_batch_probe() acima), nunca contra o .cpp wrapper que
    fez o include - o mesmo jeito que o GCC nomeia o arquivo de verdade
    em "invalid preprocessing directive", nao o argumento de linha de
    comando. Com o wrapper nunca citado, `blamed` ficava sempre vazio, o
    candidato do arquivo hostil nao recebia marcador nem C1021
    atribuivel, e caia em `missing` - reprovando um caso que devia ser
    NEUTRALIZADO. Corrigido comparando pela identidade do ARQUIVO
    INCLUIDO (`_c1021_names_included_file()` abaixo), nas duas grafias
    de separador possiveis, nunca pelo nome do wrapper.
    """
    classified = []
    to_batch = []
    idx_counter = 0
    for file_path, lineno, name in matches:
        if name_is_undef_in_same_file(file_path, name):
            classified.append({
                "status": "NEUTRALIZED", "file": file_path, "line": lineno,
                "name": name, "reason": "undef-mesmo-arquivo",
            })
            continue
        to_batch.append((file_path, lineno, name, idx_counter))
        idx_counter += 1

    by_file = {}
    for file_path, lineno, name, idx in to_batch:
        by_file.setdefault(file_path, []).append((lineno, name, idx))

    resolved = {}
    files = sorted(by_file)
    for chunk_start in range(0, len(files), _MSVC_BATCH_CHUNK_SIZE):
        chunk_files = files[chunk_start: chunk_start + _MSVC_BATCH_CHUNK_SIZE]
        probe_paths = []
        probe_basename_to_file = {}
        for i, file_path in enumerate(chunk_files):
            probe_path = os.path.join(probe_scratch, f"batch_{chunk_start}_{i}.cpp")
            entries = [(name, idx) for (_lineno, name, idx) in by_file[file_path]]
            _write_msvc_batch_probe(probe_path, file_path, entries)
            probe_paths.append(probe_path)
            probe_basename_to_file[os.path.basename(probe_path)] = file_path

        try:
            proc = subprocess.run(
                [cxx, "/nologo", "/E", "/TP"] + probe_paths,
                capture_output=True,
                text=True,
                check=False,
            )
            combined = proc.stdout + proc.stderr
        except OSError:
            combined = ""

        blamed = {
            file_path
            for file_path in chunk_files
            if _c1021_names_included_file(file_path, combined)
        }

        for probe_basename, file_path in probe_basename_to_file.items():
            entries = by_file[file_path]
            if file_path in blamed:
                for (_lineno, name, idx) in entries:
                    resolved[idx] = ("NEUTRALIZED", "arquivo-nao-e-cabecalho-c")
                continue
            for (_lineno, name, idx) in entries:
                marker = _msvc_batch_marker(idx)
                if f"{marker}_ACTIVE" in combined:
                    resolved[idx] = ("REAL", None)
                elif f"{marker}_INACTIVE" in combined:
                    resolved[idx] = ("NEUTRALIZED", "guarda-inativa-por-padrao")
                # else: no marker, no blamed C1021 for this probe's own
                # file - stays unresolved, caught by the reconciliation
                # loop below (never silently guessed either way).

    missing = []
    for file_path, lineno, name, idx in to_batch:
        outcome = resolved.get(idx)
        if outcome is None:
            missing.append((file_path, lineno, name))
            continue
        status, reason = outcome
        entry = {"status": status, "file": file_path, "line": lineno, "name": name}
        if reason:
            entry["reason"] = reason
        classified.append(entry)

    return classified, missing


# --- classification dispatch (frontend-agnostic caller) -----------------


def classify_matches(matches, cxx, frontend):
    """GCC-frontend classification of raw (file, lineno, name) matches
    into REAL and NEUTRALIZED, returning a list of dicts. NEUTRALIZED
    entries carry the reason (GODS_LAWS.md L-40 "contagem nunca
    escondida" applies to WHY, not just to the count). Each candidate
    gets its OWN, isolated compiler call here - cheap on this frontend
    (measured: ~0.01s/call), so there is no batching and no "missing"
    concept to reconcile (classify_all_matches() below always pairs
    this function with an empty `missing` list, by construction, never
    something proven at runtime). The MSVC frontend's own classifier -
    classify_matches_msvc_batched() above - exists BECAUSE the same
    per-candidate shape is NOT cheap there (this file's own header,
    BATCH-PARITY-WIN); `frontend` here is accepted only to fail loud on
    an unrecognized value, never silently treated as GCC.
    """
    if frontend == "msvc":
        raise ValueError("classify_matches() e' o caminho GCC - MSVC usa classify_matches_msvc_batched()")
    classified = []
    for file_path, lineno, name in matches:
        if name_is_undef_in_same_file(file_path, name):
            classified.append({
                "status": "NEUTRALIZED", "file": file_path, "line": lineno,
                "name": name, "reason": "undef-mesmo-arquivo",
            })
            continue

        if file_is_not_c_or_cpp_header_gcc(file_path, cxx):
            classified.append({
                "status": "NEUTRALIZED", "file": file_path, "line": lineno,
                "name": name, "reason": "arquivo-nao-e-cabecalho-c",
            })
            continue

        if not macro_active_under_default_preprocessing_gcc(file_path, name, cxx):
            classified.append({
                "status": "NEUTRALIZED", "file": file_path, "line": lineno,
                "name": name, "reason": "guarda-inativa-por-padrao",
            })
        else:
            classified.append({"status": "REAL", "file": file_path, "line": lineno, "name": name})
    return classified


def classify_all_matches(matches, cxx, frontend, probe_scratch=None):
    """Frontend dispatcher, always returning (classified, missing).
    GCC delegates to classify_matches() above - `missing` is always []
    there, since every candidate gets its own isolated compiler call.
    MSVC delegates to classify_matches_msvc_batched() above, whose
    whole reason to exist is that `missing` is NOT trivially empty -
    it is the GODS_LAWS.md L-36 reconciliation signal a batched
    invocation needs (this file's own header, BATCH-PARITY-WIN).
    """
    if frontend == "msvc":
        return classify_matches_msvc_batched(matches, cxx, probe_scratch)
    return classify_matches(matches, cxx, frontend), []


def real_collisions(classified):
    return [c for c in classified if c["status"] == "REAL"]


def neutralized_collisions(classified):
    return [c for c in classified if c["status"] == "NEUTRALIZED"]


# --- L-40 floor --------------------------------------------------------


def require_nonempty_scan(value, what):
    if not value:
        print(f"{SCRIPT_NAME}: varredura vazia ({what})", file=sys.stderr)
        return False
    return True


# --- the check itself ----------------------------------------------------


def check_public_name_collision(include_dir, cxx, cxx_id):
    frontend = cxx_frontend(cxx_id)
    if frontend == "unavailable":
        print_frontend_unavailable(SCRIPT_NAME)
        return True

    names = enumerate_our_names(include_dir)
    if not require_nonempty_scan(names, f"0 nomes publicos enumerados em {include_dir}"):
        return False
    name_count = len(names)

    sys_dirs = discover_system_include_dirs(frontend, cxx)
    discovery_desc = "variavel de ambiente INCLUDE" if frontend == "msvc" else f"'{cxx} -E -Wp,-v -xc++ -'"
    if not require_nonempty_scan(
        sys_dirs,
        f"0 diretorios de sistema descobertos via {discovery_desc} "
        "(compilador ausente, INCLUDE vazia, ou toolchain quebrada)",
    ):
        return False
    dir_count = len(sys_dirs)

    sys_files = list_files_under_dirs(sys_dirs)
    if not require_nonempty_scan(
        sys_files, f"0 arquivos de sistema varridos sob {dir_count} diretorio(s) descoberto(s)"
    ):
        return False
    file_count = len(sys_files)

    matches = scan_defines_in_files(sys_files, names)

    probe_scratch = _msvc_probe_scratch_dir() if frontend == "msvc" else None
    try:
        classified, missing = classify_all_matches(matches, cxx, frontend, probe_scratch)
    finally:
        if probe_scratch is not None:
            shutil.rmtree(probe_scratch, ignore_errors=True)

    # GODS_LAWS.md L-36 reconciliation (this file's own header,
    # BATCH-PARITY-WIN): on MSVC, `missing` names every candidate a
    # batched cl.exe invocation never gave a verdict to - checked and
    # failed BEFORE looking at REAL/NEUTRALIZED at all, so a lost-
    # coverage scenario is never mistaken for either "0 colisao real"
    # or a genuine collision. Always [] on GCC (classify_all_matches()
    # above), so this never fires there.
    if missing:
        print(
            f"{SCRIPT_NAME}: LOTE MSVC PERDEU COBERTURA de {len(missing)} candidato(s) - nem marcador "
            "nem C1021 atribuivel apareceu para eles (GODS_LAWS.md L-36: contagem de analisados nao "
            "bateu com a de encontrados, reprovado em vez de aprovado em silencio):",
            file=sys.stderr,
        )
        for file_path, lineno, name in missing:
            print(f"  {file_path}:{lineno}:{name}", file=sys.stderr)
        return False

    real = real_collisions(classified)
    neutralized = neutralized_collisions(classified)

    if neutralized:
        print(
            f"{SCRIPT_NAME}: {len(neutralized)} colisao(oes) NEUTRALIZADA(S) (define+undef no mesmo "
            "arquivo, ou define ativo so sob guarda de simbolo que a inclusao normal nao define, ou o "
            "proprio arquivo nao e C/C++ valido segundo o compilador - motivo por linha abaixo, "
            "GODS_LAWS.md L-40 nao esconde a contagem nem o motivo):"
        )
        for c in neutralized:
            print(f"  {c['file']}:{c['line']}:{c['name']}:{c['reason']}")

    if real:
        print(f"{SCRIPT_NAME}: COLISAO (docs/api-conventions.md R6):", file=sys.stderr)
        for c in real:
            print(f"  {c['file']}:{c['line']}:{c['name']}", file=sys.stderr)
        return False

    print(
        f"{SCRIPT_NAME}: {name_count} nome(s) publico(s) verificados contra {file_count} arquivo(s) "
        f"de sistema ({dir_count} diretorio(s)), 0 colisao real (frontend: {frontend})"
    )
    return True


# --- real mode -----------------------------------------------------------


def real_main(args):
    if len(args) != 3:
        fail("usage: check_public_name_collision.py <include_dir> <cxx-compiler> <cxx-compiler-id>")
    include_dir, cxx, cxx_id = args
    if not os.path.isdir(include_dir):
        fail(f"include dir not found: {include_dir}")
    # The compiler-existence check only makes sense on the path that is
    # actually going to invoke it - on the declared-absence path (the
    # escape hatch) check_public_name_collision() above returns before
    # ever touching `cxx`, so failing here first would turn a
    # legitimate declared absence into a spurious hard error.
    if cxx_frontend(cxx_id) != "unavailable" and shutil.which(cxx) is None and not os.path.isfile(cxx):
        fail(f"compiler not found in PATH: {cxx}")
    if not check_public_name_collision(include_dir, cxx, cxx_id):
        fail("verificacao de colisao de nome publico falhou (ver mensagem acima)")


# --- selftest fixtures and controls (frontend-agnostic bodies) ------------


def make_scratch_workdir():
    # A hand written Unix path does not exist on every platform (Windows
    # has no /tmp). dir=os.environ.get("TMPDIR") without a hardcoded
    # fallback lets tempfile.mkdtemp fall through to gettempdir(), which
    # already checks TMPDIR/TEMP/TMP and then the platform default.
    return tempfile.mkdtemp(prefix="glintfx-name-collision-selftest-", dir=os.environ.get("TMPDIR"))


def make_fixture_include_dir(scratch, label):
    d = os.path.join(scratch, label, "include", "pkg")
    os.makedirs(d, exist_ok=True)
    return d


def make_fixture_system_dir(scratch, label):
    d = os.path.join(scratch, label, "system")
    os.makedirs(d, exist_ok=True)
    return d


def _classify(matches, cxx, frontend, scratch, label):
    """Selftest helper: routes through classify_all_matches() (this
    file's own header, BATCH-PARITY-WIN) so every control below
    exercises the SAME dispatcher check_public_name_collision() uses in
    real mode - never a parallel, only-tested-in-selftest path. Gives
    it a fresh probe scratch dir per control when frontend == "msvc"
    (each control's fixtures must not collide with another control's
    throwaway probe files); None (unused) on the GCC path. A non-empty
    `missing` here is always a BUG (every control below plants exactly
    one candidate that should resolve) - surfaced on stderr so the
    calling control's own assertion fails with a clear pointer to why,
    instead of a confusing "candidate never appeared" message alone.
    """
    if frontend != "msvc":
        classified, missing = classify_all_matches(matches, cxx, frontend)
    else:
        probe_scratch = os.path.join(scratch, label, "msvc-probes")
        os.makedirs(probe_scratch, exist_ok=True)
        classified, missing = classify_all_matches(matches, cxx, frontend, probe_scratch)
    if missing:
        print(
            f"selftest: {label}: classify_all_matches() perdeu {len(missing)} candidato(s) - "
            f"{missing} - nunca deveria acontecer num controle com um unico candidato plantado",
            file=sys.stderr,
        )
    return classified


# Positive control: a clean public header (one type, one method), and a
# system header that defines something unrelated. Expected: passes,
# zero collisions.
def selftest_positive_control(scratch):
    include_dir = make_fixture_include_dir(scratch, "positive")
    _write(os.path.join(include_dir, "widget.hpp"),
           "class widget {\n  public:\n    [[nodiscard]] int size() const noexcept;\n};\n")
    sys_dir = make_fixture_system_dir(scratch, "positive")
    _write(os.path.join(sys_dir, "unrelated.h"), "#define UNRELATED_MACRO 1\n")

    names = enumerate_our_names(include_dir)
    matches = scan_defines_in_files(list_files_under_dirs([sys_dir]), names)
    if not matches:
        print("selftest: controle POSITIVO OK (header limpo, sem colisao)")
        return True
    print("selftest: controle POSITIVO FALHOU (esperava zero colisoes, achou algo)", file=sys.stderr)
    print(matches, file=sys.stderr)
    return False


# Negative control: a public header declaring `planted_collision_name`,
# and a system header defining exactly that, WITHOUT undef. Expected:
# classified as REAL, citing both file and name.
def selftest_negative_control(scratch, cxx, frontend):
    include_dir = make_fixture_include_dir(scratch, "negative")
    _write(os.path.join(include_dir, "widget.hpp"),
           "class widget {\n  public:\n    [[nodiscard]] int planted_collision_name() const noexcept;\n};\n")
    sys_dir = make_fixture_system_dir(scratch, "negative")
    hostile = os.path.join(sys_dir, "hostile.h")
    _write(hostile, "#define planted_collision_name 1\n")

    names = enumerate_our_names(include_dir)
    matches = scan_defines_in_files(list_files_under_dirs([sys_dir]), names)
    classified = _classify(matches, cxx, frontend, scratch, "negative")
    real = real_collisions(classified)
    if not real:
        print(f"selftest: controle NEGATIVO FALHOU (nao achou planted_collision_name em {hostile})", file=sys.stderr)
        return False
    if not any(c["file"] == hostile for c in real):
        print(f"selftest: controle NEGATIVO FALHOU (achou colisao, mas nao citou {hostile})", file=sys.stderr)
        return False
    if not any(c["name"] == "planted_collision_name" for c in real):
        print("selftest: controle NEGATIVO FALHOU (achou colisao, mas nao citou o nome)", file=sys.stderr)
        return False
    print("selftest: controle NEGATIVO OK (planted_collision_name reprovado, citando arquivo e nome)")
    return True


# Specific to policy decision 2's first neutralizing shape: same name,
# same fixture, but the hostile header ALSO #undef's it before EOF.
# Expected: NEUTRALIZED, not REAL.
def selftest_undef_neutralizes_control(scratch, cxx, frontend):
    include_dir = make_fixture_include_dir(scratch, "neutralize")
    _write(os.path.join(include_dir, "widget.hpp"),
           "class widget {\n  public:\n    [[nodiscard]] int planted_collision_name() const noexcept;\n};\n")
    sys_dir = make_fixture_system_dir(scratch, "neutralize")
    hostile = os.path.join(sys_dir, "hostile.h")
    _write(hostile, "#define planted_collision_name 1\n/* uses it here */\n#undef planted_collision_name\n")

    names = enumerate_our_names(include_dir)
    matches = scan_defines_in_files(list_files_under_dirs([sys_dir]), names)
    classified = _classify(matches, cxx, frontend, scratch, "neutralize")
    real = real_collisions(classified)
    neutralized = neutralized_collisions(classified)

    if real:
        print(
            "selftest: controle de NEUTRALIZACAO FALHOU (define+undef no mesmo arquivo deveria ser "
            "NEUTRALIZADO, apareceu como REAL)",
            file=sys.stderr,
        )
        return False
    if not any(c["name"] == "planted_collision_name" for c in neutralized):
        print(
            "selftest: controle de NEUTRALIZACAO FALHOU (nao apareceu na lista NEUTRALIZADA, ou nao "
            "citou o nome)",
            file=sys.stderr,
        )
        return False
    print("selftest: controle de NEUTRALIZACAO OK (define+undef no mesmo arquivo nao reprova, mas aparece na contagem)")
    return True


# Guard-inactive control (policy decision 2, second shape): a #define
# textually nested inside "#ifdef GUARD_NEVER_DEFINED", never active in
# a normal include chain. Expected: NEUTRALIZED, never REAL.
def selftest_guard_inactive_control(scratch, cxx, frontend):
    include_dir = make_fixture_include_dir(scratch, "guard_inactive")
    _write(os.path.join(include_dir, "widget.hpp"),
           "class widget {\n  public:\n    [[nodiscard]] int planted_collision_name() const noexcept;\n};\n")
    sys_dir = make_fixture_system_dir(scratch, "guard_inactive")
    hostile = os.path.join(sys_dir, "hostile.h")
    _write(hostile, "#ifdef GUARD_NEVER_DEFINED\n#define planted_collision_name 1\n#endif\n")

    names = enumerate_our_names(include_dir)
    matches = scan_defines_in_files(list_files_under_dirs([sys_dir]), names)
    classified = _classify(matches, cxx, frontend, scratch, "guard_inactive")
    real = real_collisions(classified)
    neutralized = neutralized_collisions(classified)

    if real:
        print(
            "selftest: controle de GUARDA INATIVA FALHOU (macro so existe sob #ifdef de simbolo nunca "
            "definido deveria ser NEUTRALIZADA, apareceu como REAL)",
            file=sys.stderr,
        )
        return False
    if not any(c["name"] == "planted_collision_name" for c in neutralized):
        print(
            "selftest: controle de GUARDA INATIVA FALHOU (nao apareceu na lista NEUTRALIZADA, ou nao "
            "citou o nome)",
            file=sys.stderr,
        )
        return False
    print("selftest: controle de GUARDA INATIVA OK (macro sob #ifdef de simbolo nunca definido nao reprova)")
    return True


# The middle case: same shape, but the guard symbol IS defined earlier
# in the same fixture header - so a real translation unit including it
# WOULD see the macro active. Expected: still REAL - the regression
# guard against loosening the check too far.
def selftest_guard_active_control(scratch, cxx, frontend):
    include_dir = make_fixture_include_dir(scratch, "guard_active")
    _write(os.path.join(include_dir, "widget.hpp"),
           "class widget {\n  public:\n    [[nodiscard]] int planted_collision_name() const noexcept;\n};\n")
    sys_dir = make_fixture_system_dir(scratch, "guard_active")
    hostile = os.path.join(sys_dir, "hostile.h")
    _write(
        hostile,
        "#define GUARD_ALWAYS_DEFINED_HERE 1\n#ifdef GUARD_ALWAYS_DEFINED_HERE\n"
        "#define planted_collision_name 1\n#endif\n",
    )

    names = enumerate_our_names(include_dir)
    matches = scan_defines_in_files(list_files_under_dirs([sys_dir]), names)
    classified = _classify(matches, cxx, frontend, scratch, "guard_active")
    real = real_collisions(classified)
    if not any(c["name"] == "planted_collision_name" for c in real):
        print(
            "selftest: controle de GUARDA ATIVA FALHOU (macro cujo simbolo-guarda ESTA definido no "
            "proprio arquivo deveria reprovar como REAL, nao reprovou)",
            file=sys.stderr,
        )
        return False
    print("selftest: controle de GUARDA ATIVA OK (macro cujo simbolo-guarda esta definido continua REAL)")
    return True


# Specific to policy decision 2's third neutralizing shape: the matched
# "system header" is not C/C++ at all (a Makefile fragment whose
# English-prose comment coincidentally contains a syntactically legal
# #define). Expected: NEUTRALIZED under "arquivo-nao-e-cabecalho-c".
def selftest_not_a_header_control(scratch, cxx, frontend):
    include_dir = make_fixture_include_dir(scratch, "not_a_header")
    _write(os.path.join(include_dir, "widget.hpp"),
           "class widget {\n  public:\n    [[nodiscard]] int planted_collision_name() const noexcept;\n};\n")
    sys_dir = make_fixture_system_dir(scratch, "not_a_header")
    hostile = os.path.join(sys_dir, "builddefs")
    _write(
        hostile,
        "# Copyright nobody, this is a Makefile fragment, not a header.\n"
        "# define planted_collision_name a coincidental directive-shaped line.\n",
    )

    names = enumerate_our_names(include_dir)
    matches = scan_defines_in_files(list_files_under_dirs([sys_dir]), names)
    classified = _classify(matches, cxx, frontend, scratch, "not_a_header")
    real = real_collisions(classified)
    neutralized = neutralized_collisions(classified)

    if real:
        print(
            "selftest: controle de ARQUIVO-NAO-CABECALHO FALHOU (Makefile com #define coincidente "
            "deveria ser NEUTRALIZADO, apareceu como REAL)",
            file=sys.stderr,
        )
        return False
    if not any(
        c["name"] == "planted_collision_name" and c["reason"] == "arquivo-nao-e-cabecalho-c"
        for c in neutralized
    ):
        print(
            "selftest: controle de ARQUIVO-NAO-CABECALHO FALHOU (nao apareceu na lista NEUTRALIZADA "
            "com o motivo certo)",
            file=sys.stderr,
        )
        return False
    print("selftest: controle de ARQUIVO-NAO-CABECALHO OK (Makefile cujo comentario colide por coincidencia nao reprova)")
    return True


# ASSIGNMENT-VS-DECLARATION control (achado real de 04/09/2026, TODO.md
# "Desvios": commit c6fac1c introduziu "errno = 0;" dentro de
# read_stream_bytes() em include/glintfx/platform/asset/file.hpp, e a
# extracao antiga tratava aquilo como se fosse uma declaracao de
# "errno" - errno e' macro real da libc, entao o portao reprovava algo
# que nao era nosso nome nenhum). Frontend-agnostic e sem precisar do
# compilador (so testa enumerate_our_names(), nao classify_matches()):
# uma atribuicao a um nome que ja existe ("errno = 0;", nada antes do
# "=") nao deve virar nome publico, enquanto uma declaracao bare
# ("int contents;") e uma declaracao com inicializador
# ("const int read_errno = errno;"), as duas dentro do corpo de uma
# funcao inline, continuam sendo extraidas normalmente (GODS_LAWS.md
# L-40's own "recusar alto e melhor que aprovar em silencio" corre nos
# dois sentidos: nao pode aprovar em silencio "errno", e nao pode
# deixar de reprovar "contents"/"read_errno" se algum dia voltarem a
# colidir).
def selftest_assignment_not_declaration_control(scratch):
    include_dir = make_fixture_include_dir(scratch, "assignment_not_declaration")
    _write(
        os.path.join(include_dir, "widget.hpp"),
        "inline void f() {\n"
        "    int contents;\n"
        "    errno = 0;\n"
        "    const int read_errno = errno;\n"
        "}\n",
    )
    names = enumerate_our_names(include_dir)
    if "errno" in names:
        print(
            "selftest: controle de ATRIBUICAO-NAO-E-DECLARACAO FALHOU (\"errno = 0;\" foi tratado "
            "como se declarasse errno)",
            file=sys.stderr,
        )
        return False
    if "contents" not in names:
        print(
            "selftest: controle de ATRIBUICAO-NAO-E-DECLARACAO FALHOU (declaracao bare dentro de "
            "funcao inline deixou de ser extraida)",
            file=sys.stderr,
        )
        return False
    if "read_errno" not in names:
        print(
            "selftest: controle de ATRIBUICAO-NAO-E-DECLARACAO FALHOU (declaracao com inicializador "
            "deixou de ser extraida)",
            file=sys.stderr,
        )
        return False
    print(
        "selftest: controle de ATRIBUICAO-NAO-E-DECLARACAO OK (atribuicao a nome existente nao vira "
        "nome publico; declaracao bare e com inicializador continuam)"
    )
    return True


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


# Empty-scan floor, side 1: zero public headers under include_dir.
def selftest_empty_our_names_control(scratch, cxx, cxx_id):
    include_dir = os.path.join(scratch, "empty_our_names", "include")
    os.makedirs(include_dir, exist_ok=True)

    outcome = _make_capture()(lambda: check_public_name_collision(include_dir, cxx, cxx_id))
    if outcome.result:
        print(
            "selftest: controle de VARREDURA VAZIA (nomes) FALHOU (deveria recusar include_dir sem "
            "headers, mas passou)",
            file=sys.stderr,
        )
        return False
    if "varredura vazia" not in outcome.text:
        print(
            "selftest: controle de VARREDURA VAZIA (nomes) FALHOU (recusou, mas nao disse "
            "'varredura vazia')",
            file=sys.stderr,
        )
        print(outcome.text, file=sys.stderr)
        return False
    print("selftest: controle de VARREDURA VAZIA (nomes) OK (include_dir sem headers recusado)")
    return True


# Empty-scan floor, side 2: the system-header side - the same shape a
# broken or not-yet-installed toolchain would produce, without needing
# to actually break this machine's compiler to prove it.
def selftest_empty_system_dirs_control():
    files = list_files_under_dirs(["/does/not/exist/glintfx-selftest"])
    if not require_nonempty_scan(files, "0 arquivos de sistema varridos"):
        print("selftest: controle de VARREDURA VAZIA (sistema) OK (diretorio de sistema inexistente recusado)")
        return True
    print(
        "selftest: controle de VARREDURA VAZIA (sistema) FALHOU (deveria recusar diretorio de sistema "
        "inexistente, mas passou)",
        file=sys.stderr,
    )
    return False


# Batch reconciliation control (this file's own header, BATCH-PARITY-
# WIN, GODS_LAWS.md L-36, achado real de 05/09/2026): classify_all_
# matches() must NEVER let a candidate silently vanish. On GCC the
# assertion is trivial - each candidate gets its own isolated compiler
# call, so `missing` is always empty by construction. On MSVC it is the
# whole point of the batched design: forcing the compiler binary itself
# to be unresolvable makes the WHOLE batch produce zero output, and the
# planted candidate must come back named in `missing`, never silently
# dropped and never misreported as a false REAL/NEUTRALIZED verdict.
def selftest_batch_reconciliation_control(scratch, cxx, frontend):
    include_dir = make_fixture_include_dir(scratch, "batch_reconciliation")
    _write(os.path.join(include_dir, "widget.hpp"),
           "class widget {\n  public:\n    [[nodiscard]] int planted_collision_name() const noexcept;\n};\n")
    sys_dir = make_fixture_system_dir(scratch, "batch_reconciliation")
    hostile = os.path.join(sys_dir, "hostile.h")
    _write(hostile, "#define planted_collision_name 1\n")

    names = enumerate_our_names(include_dir)
    matches = scan_defines_in_files(list_files_under_dirs([sys_dir]), names)
    if not matches:
        print(
            "selftest: controle de RECONCILIACAO DE LOTE FALHOU (fixture nao gerou candidato algum)",
            file=sys.stderr,
        )
        return False

    if frontend != "msvc":
        _classified, missing = classify_all_matches(matches, cxx, frontend)
        if missing:
            print(
                "selftest: controle de RECONCILIACAO DE LOTE FALHOU (GCC nunca deveria reportar "
                f"`missing`, e reportou: {missing})",
                file=sys.stderr,
            )
            return False
        print(
            "selftest: controle de RECONCILIACAO DE LOTE OK (GCC: cada candidato tem chamada "
            "propria, `missing` sempre vazio por construcao)"
        )
        return True

    probe_scratch = os.path.join(scratch, "batch_reconciliation", "msvc-probes")
    os.makedirs(probe_scratch, exist_ok=True)
    broken_cxx = os.path.join(scratch, "glintfx-selftest-definitely-not-a-real-compiler-binary")
    _classified, missing = classify_matches_msvc_batched(matches, broken_cxx, probe_scratch)
    if not any(name == "planted_collision_name" for (_file, _line, name) in missing):
        print(
            "selftest: controle de RECONCILIACAO DE LOTE FALHOU (compilador inexistente deveria "
            f"deixar planted_collision_name em `missing`, achou {missing})",
            file=sys.stderr,
        )
        return False
    print(
        "selftest: controle de RECONCILIACAO DE LOTE OK (MSVC: lote que nao roda nada e' detectado, "
        "nomeando o candidato que ficou sem veredito - nunca aprovado em silencio, nunca uma REAL/"
        "NEUTRALIZADA fantasma)"
    )
    return True


def _run_all_controls(scratch, cxx, cxx_id, frontend):
    return [
        selftest_positive_control(scratch),
        selftest_negative_control(scratch, cxx, frontend),
        selftest_undef_neutralizes_control(scratch, cxx, frontend),
        selftest_guard_inactive_control(scratch, cxx, frontend),
        selftest_guard_active_control(scratch, cxx, frontend),
        selftest_not_a_header_control(scratch, cxx, frontend),
        selftest_assignment_not_declaration_control(scratch),
        selftest_empty_our_names_control(scratch, cxx, cxx_id),
        selftest_empty_system_dirs_control(),
        selftest_batch_reconciliation_control(scratch, cxx, frontend),
    ]


def selftest_main(args):
    cxx = args[0] if args else os.environ.get("CXX_FOR_SELFTEST", "c++")
    cxx_id = args[1] if len(args) > 1 else os.environ.get("CXX_ID_FOR_SELFTEST", "GNU")

    frontend = cxx_frontend(cxx_id)

    # Nine of the ten controls below need a real compiler in some form:
    # GCC/Clang via `-E -dM` (see this file's own header, policy
    # decision 2), MSVC via `#ifdef` probes preprocessed with `/E /TP`
    # (NAMES-PARITY-WIN), or - the new RECONCILIACAO DE LOTE control's
    # own MSVC half - a DELIBERATELY BROKEN compiler path, to prove the
    # loud-failure mechanism itself works (BATCH-PARITY-WIN). The
    # exception, selftest_assignment_not_declaration_control, only
    # exercises enumerate_our_names() and needs no compiler at all - it
    # still runs behind this SAME declared-absence gate, on purpose:
    # when the escape hatch forces "unavailable", the WHOLE selftest is
    # one declared case here, not ten and not "nine plus one" - the
    # real gate's own single-case shape (check_public_name_collision()
    # above), not check_spdx.py's per-case list (that gate's cases are
    # independent hostile filenames; this gate's ten controls all share
    # the one unavailable mechanism, even the one that does not, on its
    # own, need it).
    if frontend == "unavailable":
        print_frontend_unavailable("check_public_name_collision.py --selftest")
        return

    if shutil.which(cxx) is None and not os.path.isfile(cxx):
        fail(f"selftest precisa de um compilador C++ em PATH ({cxx}), nao encontrado")

    scratch = make_scratch_workdir()
    try:
        controls = _run_all_controls(scratch, cxx, cxx_id, frontend)
        if not all(controls):
            print("check_public_name_collision.py --selftest: FALHOU (ver acima)", file=sys.stderr)
            sys.exit(1)
        print(f"check_public_name_collision.py --selftest: os {len(controls)} controles OK (frontend: {frontend})")
    finally:
        shutil.rmtree(scratch, ignore_errors=True)


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main(args[1:])
    else:
        real_main(args)


if __name__ == "__main__":
    main()
