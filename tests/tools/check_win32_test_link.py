#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_win32_test_link.py - CORE-LOG-CI fatia 4 (plano em
# /var/tmp/glintfx-plan/ci-corelog-tres-defeitos.md sec. 4.3): fecha o
# mesmo tipo de lacuna que check_container_fixture_link.py ja fecha do
# lado do Containerfile Linux, agora do lado dos alvos win32_* de
# tests/CMakeLists.txt. TRES vezes em tres dias (07/09, 08/09, 09/09/
# 2026, ver o comentario da linha ~1526 de tests/CMakeLists.txt) um
# alvo win32_* ficou sem um .cpp que ele precisa em tempo de LINK -
# nunca de compilacao (nenhum #include quebrou, entao um portao de
# fechamento de #include - GODS_LAWS.md L-17 - nunca veria nada de
# errado). So o `link.exe` real prova isso.
#
# WHAT THIS SCRIPT DOES, MESMA FILOSOFIA DE check_container_fixture_
# link.py's own "WHAT THIS SCRIPT DOES": ele NAO modela "que simbolo o
# arquivo X chama do arquivo Y" - isso e exatamente o tipo de inventario
# curado a mao que esta familia de defeito e sobre (GODS_LAWS.md L-17:
# um portao que mantem a propria lista curada so muda o defeito de
# lugar). Ele LE a receita real - os proprios target_sources()/target_
# link_libraries() de tests/CMakeLists.txt e dos src/**/CMakeLists.txt -
# e RODA o cl.exe/link.exe REAL da Microsoft (glintfx-msvc:latest,
# tools/msvc-container/README.md) contra ela, num diretorio descartavel,
# nunca dentro da arvore real. Qualquer razao para um alvo nao ligar -
# um atomo faltando, uma violacao real de ODR, um typo de flag - surge
# como o erro REAL do `link.exe`, nunca uma aproximacao heuristica dele.
#
# GODS_LAWS.md L-11 (um processo por ALVO, nunca por arquivo varrido):
# esta e a razao de existir de build_library_dll() ser UMA chamada
# docker (todas as fontes da biblioteca de uma vez, `cl /LD`) e link_
# one_test() ser UMA chamada docker por alvo win32_* - nunca um
# subprocesso por arquivo-fonte dentro de um alvo. run_link_check()
# gasta exatamente 1 (a DLL) + N (os N alvos) invocacoes de `docker
# run`, nunca mais.
#
# GODS_LAWS.md L-40 (piso de varredura nao-vazia): zero alvos win32_*
# encontrados reprova, zero fontes de glintfx_library encontradas
# reprova, e o resumo final imprime as quatro contagens SEMPRE, com
# ligados+falharam+ambiente == encontrados verificado a cada execucao.
#
# GODS_LAWS.md L-49 (ferramenta morrendo != codigo reprovando): timeout
# de uma invocacao `docker run` ou um codigo de saida >=128 (a propria
# ferramenta caindo) e contado como "ambiente", nunca como "falhou" - as
# duas correcoes sao opostas e classify_link_result() as separa, nunca
# so pelo codigo de saida sozinho.
#
# Usage:
#   check_win32_test_link.py --exec <repo-root> [--image <docker-image>]
#                             [--timeout-seconds <n>]
#   check_win32_test_link.py --selftest
#
# CADA FUNCAO ABAIXO FAZ UMA COISA (GODS_LAWS.md L-17):
#   extract_win32_test_targets()   - le tests/CMakeLists.txt, devolve os
#                                     alvos win32_* com suas fontes/libs
#   collect_win32_library_layout() - le os src/**/CMakeLists.txt, segue
#                                     add_subdirectory() simulando WIN32,
#                                     devolve as fontes/libs da propria
#                                     glintfx_library nessa configuracao
#   write_msvc_export_header()     - escreve o par dllexport/dllimport
#                                     de verdade (nao o STATIC_DEFINE que
#                                     o README usa para /c isolado - aqui
#                                     o defeito QUE ESTA SENDO PROVADO SO
#                                     existe no modo compartilhado real)
#   build_library_dll()            - UMA chamada `cl /LD`: glintfx.dll +
#                                     glintfx.lib, a partir do layout
#                                     acima
#   link_one_test()                - UMA chamada `cl ... /link glintfx.
#                                     lib ...` por alvo win32_*
#   run_link_check()                - orquestra as cinco acima, laco
#                                     item-a-item (nunca um subprocesso
#                                     por arquivo dentro do laco)
#   print_summary()                - imprime as quatro contagens SEMPRE

import os
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
import time

SCRIPT_NAME = "check_win32_test_link.py"
DEFAULT_IMAGE = "glintfx-msvc:latest"
DEFAULT_TIMEOUT_SECONDS = 240
STDERR_TAIL_LINES = 25

# Mesmo valor/mesma semantica de check_container_fixture_link.py's own
# GATE_SKIP_RETURN_CODE: "declarado NAO APLICAVEL" - nao e PASS nem
# FAIL. Este script nunca e registrado como ctest (secao 4.3 do plano:
# "Nao e add_test"), mas tools/preci.sh's own stage_win32_link le este
# codigo para distinguir os dois casos, mesma convencao.
GATE_SKIP_RETURN_CODE = 77


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


def read_file(path):
    try:
        with open(path, "r", encoding="utf-8") as handle:
            return handle.read()
    except OSError as exc:
        fail(f"arquivo nao encontrado: {path} ({exc})")


def stderr_tail(text, n=STDERR_TAIL_LINES):
    lines = (text or "").splitlines()
    if len(lines) <= n:
        return "\n".join(lines)
    return "\n".join(["...(truncado)..."] + lines[-n:])


# --- tokenizacao compartilhada dos dois lados de CMake (tests/ e src/) ----


def _strip_cmake_comment(line):
    # "#" inicia comentario em CMake. Nenhuma linha de target_sources()/
    # target_link_libraries() deste projeto carrega um "#" dentro de um
    # caminho ou nome de lib (confirmado por leitura de todo tests/
    # CMakeLists.txt e src/**/CMakeLists.txt antes de escrever esta
    # funcao) - um corte simples por indice e correto aqui, sem
    # precisar entender aspas.
    idx = line.find("#")
    return line if idx == -1 else line[:idx]


def _strip_cmake_comments_from_text(text):
    """Remove todo comentario ANTES de qualquer regex baseada em
    parenteses rodar sobre o texto - achado real ao testar contra
    tests/CMakeLists.txt: os comentarios explicativos deste projeto sao
    prosa completa, e prosa completa cita parenteses de verdade (ex.:
    "win32_window_adapter_route_message() (window_message_route.hpp)"
    dentro do PROPRIO comentario de um bloco target_sources(...)). Uma
    captura nao-gulosa "(.*?)\\)" para no primeiro ")" que encontrar -
    inclusive um dentro de um comentario - e devolvia so a PRIMEIRA
    fonte da lista, calada, sem erro nenhum. Comentario removido por
    inteiro, nunca só uma tentativa de "pular" o parentese dele."""
    return "\n".join(_strip_cmake_comment(line) for line in text.splitlines())


def _tokenize_cmake_args(raw_text):
    """Tokens de uma lista de argumentos CMake (o meio de um target_
    sources(...)/target_link_libraries(...) capturado por regex),
    aceitando tanto tokens entre aspas ("${PROJECT_SOURCE_DIR}/src/...")
    quanto tokens nus (version.cpp, user32) - as duas formas coexistem
    entre tests/CMakeLists.txt e src/**/CMakeLists.txt."""
    tokens = []
    for line in raw_text.splitlines():
        stripped = _strip_cmake_comment(line).strip()
        if not stripped:
            continue
        for tok in shlex.split(stripped):
            tokens.append(tok.strip('"'))
    return tokens


_PROJECT_SOURCE_DIR_PREFIX = "${PROJECT_SOURCE_DIR}/"


def _resolve_project_source_dir_token(token):
    """tests/CMakeLists.txt sempre referencia arquivo de src/ por
    caminho absoluto-estilo "${PROJECT_SOURCE_DIR}/src/..." (nunca
    relativo a tests/, ao contrario de src/**/CMakeLists.txt - ver
    collect_win32_library_layout()'s own header comment). Devolve o
    caminho relativo a raiz do repositorio."""
    if token.startswith(_PROJECT_SOURCE_DIR_PREFIX):
        return token[len(_PROJECT_SOURCE_DIR_PREFIX) :]
    return token


def _extract_call_args(text, func_name, target_name):
    """O corpo de `func_name(target_name PRIVATE <isto>)` dentro de
    `text`, ja tokenizado. Nao-guloso e DOTALL: os blocos deste projeto
    nunca tem um "(" a mais dentro da lista de argumentos (nenhum
    caminho carrega parenteses) - confirmado antes de escrever esta
    funcao, mesma garantia que check_container_fixture_link.py's own
    _COPY_RE documenta para o Containerfile."""
    pattern = re.compile(
        rf"{re.escape(func_name)}\(\s*{re.escape(target_name)}\s+PRIVATE\s*(.*?)\)",
        re.DOTALL,
    )
    match = pattern.search(text)
    if not match:
        return []
    return _tokenize_cmake_args(match.group(1))


# --- 1. extract_win32_test_targets: tests/CMakeLists.txt -----------------


# So "if(" abre um NIVEL novo de aninhamento - "elseif("/"else()" sao o
# MESMO nivel do "if(" que os precede (um ramo alternativo, nao um
# bloco aninhado), e um "endif()" fecha exatamente o nivel do "if("
# mais recente. Contar "elseif(" como abertura (erro corrigido antes de
# rodar contra a arvore real, achado em teste manual: o bloco se(WIN32)/
# elseif(BUILD_SHARED_LIBS)/else()/endif() de tests/CMakeLists.txt
# nunca fechava, porque cada "elseif(" empurrava a profundidade de novo
# e o UNICO endif() do bloco so tirava um nivel) faria _find_matching_
# endif() nunca encontrar o endif() certo em qualquer bloco if/elseif/
# else real.
_IF_OPEN_RE = re.compile(r"^\s*if\s*\(")
_ENDIF_RE = re.compile(r"^\s*endif\s*\(\s*\)\s*$")
_WIN32_IF_RE = re.compile(r"^\s*if\s*\(\s*WIN32\s*\)\s*$")
_ADD_TEST_RE = re.compile(r"glintfx_add_test\(\s*(\w+)\s*\)")


def _find_matching_endif(lines, if_line_idx):
    depth = 1
    i = if_line_idx + 1
    while i < len(lines):
        if _IF_OPEN_RE.match(lines[i]):
            depth += 1
        elif _ENDIF_RE.match(lines[i]):
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return None


def extract_win32_test_targets(cmake_text):
    """Cada glintfx_add_test(<nome>) dentro de um bloco if(WIN32) de
    tests/CMakeLists.txt, com as fontes extras (target_sources) e libs
    extras (target_link_libraries) que o MESMO bloco declara para ele -
    a "receita" real que cmake/GlintfxTest.cmake usa para montar cada
    executavel de teste. Zero alvos e piso vazio (GODS_LAWS.md L-40),
    verificado pelo chamador (run_link_check), nao aqui - esta funcao so
    reporta o que encontrou."""
    lines = _strip_cmake_comments_from_text(cmake_text).splitlines()
    targets = []
    i = 0
    while i < len(lines):
        if _WIN32_IF_RE.match(lines[i]):
            end = _find_matching_endif(lines, i)
            if end is None:
                fail(
                    f"if(WIN32) sem endif() correspondente em tests/CMakeLists.txt, "
                    f"linha {i + 1} - arvore malformada, gate recusado antes de tentar ligar nada"
                )
            block_text = "\n".join(lines[i + 1 : end])
            for match in _ADD_TEST_RE.finditer(block_text):
                name = match.group(1)
                raw_sources = _extract_call_args(block_text, "target_sources", name)
                targets.append(
                    {
                        "name": name,
                        "sources": [_resolve_project_source_dir_token(s) for s in raw_sources],
                        "libs": _extract_call_args(block_text, "target_link_libraries", name),
                    }
                )
            i = end + 1
        else:
            i += 1
    return targets


# --- 2. collect_win32_library_layout: src/**/CMakeLists.txt --------------


_ADD_SUBDIR_RE = re.compile(r"add_subdirectory\(\s*(\S+)\s*\)")
_IF_RE = re.compile(r"^\s*if\s*\((.*)\)\s*$")
_ELSEIF_RE = re.compile(r"^\s*elseif\s*\((.*)\)\s*$")
_ELSE_RE = re.compile(r"^\s*else\s*\(\s*\)\s*$")
_GENERATED_GL_LOADER_CALL = "glintfx_add_generated_gl_loader(glintfx_library)"


def _win32_condition_true(condition_text):
    """Avaliador MINIMO: so WIN32/UNIX (e as formas NOT) condicionam
    algum add_subdirectory() em toda a arvore src/**/CMakeLists.txt
    deste projeto hoje - confirmado por
    `grep -rn "^if(\\|^elseif(\\|add_subdirectory("` antes de escrever
    esta funcao (o unico bloco assim e src/platform/CMakeLists.txt's
    own if(UNIX)/elseif(WIN32)/else()). Qualquer OUTRA condicao (ex.:
    o if(NOT ...STREQUAL...) de src/render/CMakeLists.txt, que nao
    envolve add_subdirectory nenhum) nunca precisa ser avaliada de
    verdade para decidir QUAIS arquivos entram no layout WIN32 - conta
    como verdadeira (o ramo e visitado) para nao esconder um
    add_subdirectory futuro atras de uma condicao este avaliador nao
    reconhece; documentado aqui, nao um valor sorteado."""
    normalized = condition_text.strip()
    if normalized == "WIN32":
        return True
    if normalized == "UNIX":
        return False
    if normalized == "NOT WIN32":
        return False
    if normalized == "NOT UNIX":
        return True
    return True


def _add_subdirectory_calls_for_win32(cmake_text):
    """add_subdirectory(<nome>) que uma configuracao WIN32 realmente
    visita, na ordem em que aparecem - avalia if/elseif/else/endif com
    _win32_condition_true(), com curto-circuito igual ao CMake real
    (uma vez que um ramo do if/elseif/else e tomado, os seguintes do
    MESMO grupo nunca sao)."""
    calls = []
    cond_stack = []
    for line in cmake_text.splitlines():
        if_match = _IF_RE.match(line)
        elseif_match = _ELSEIF_RE.match(line)
        else_match = _ELSE_RE.match(line)
        endif_match = _ENDIF_RE.match(line)
        if if_match:
            truth = _win32_condition_true(if_match.group(1))
            cond_stack.append({"any_taken": truth, "current": truth})
        elif elseif_match:
            if not cond_stack:
                fail("elseif() sem if() correspondente ao percorrer src/**/CMakeLists.txt")
            top = cond_stack[-1]
            truth = (not top["any_taken"]) and _win32_condition_true(elseif_match.group(1))
            top["current"] = truth
            top["any_taken"] = top["any_taken"] or truth
        elif else_match:
            if not cond_stack:
                fail("else() sem if() correspondente ao percorrer src/**/CMakeLists.txt")
            top = cond_stack[-1]
            truth = not top["any_taken"]
            top["current"] = truth
            top["any_taken"] = True
        elif endif_match:
            if not cond_stack:
                fail("endif() sem if() correspondente ao percorrer src/**/CMakeLists.txt")
            cond_stack.pop()
        else:
            active = all(c["current"] for c in cond_stack)
            match = _ADD_SUBDIR_RE.search(line)
            if match and active:
                calls.append(match.group(1))
    return calls


def collect_win32_library_layout(repo_root):
    """Segue add_subdirectory() a partir de src/CMakeLists.txt,
    simulando uma configuracao WIN32 (a mesma selecao que src/platform/
    CMakeLists.txt's own if(UNIX)/elseif(WIN32) faz de verdade),
    coletando: (a) toda fonte de target_sources(glintfx_library
    PRIVATE ...) - sempre um caminho NU, relativo ao proprio diretorio
    do CMakeLists.txt que o declara (confirmado: nenhum src/**/
    CMakeLists.txt usa ${PROJECT_SOURCE_DIR}, so tests/CMakeLists.txt
    usa, porque so ele referencia arquivo FORA do proprio diretorio);
    (b) toda lib de target_link_libraries(glintfx_library PRIVATE
    ...); (c) se algum arquivo visitado chama glintfx_add_generated_gl_
    loader(glintfx_library) - o UNICO lugar onde uma fonte da biblioteca
    e GERADA em tempo de build (src/render/CMakeLists.txt), nao listada
    literalmente - ver discover_generated_gl_source() abaixo para como
    esse caso e resolvido sem reconstruir o codegen aqui.

    Devolve (sources, libs, needs_generated_gl_loader, visited_files).
    """
    sources = []
    libs = []
    visited_files = []
    needs_generated_gl_loader = False

    def walk(rel_dir):
        nonlocal needs_generated_gl_loader
        cmake_path = os.path.join(repo_root, rel_dir, "CMakeLists.txt")
        if not os.path.isfile(cmake_path):
            fail(f"CMakeLists.txt ausente em {rel_dir} (add_subdirectory apontou para la)")
        text = _strip_cmake_comments_from_text(read_file(cmake_path))
        visited_files.append(os.path.join(rel_dir, "CMakeLists.txt"))

        for match in re.finditer(
            r"target_sources\(\s*glintfx_library\s+PRIVATE\s*(.*?)\)", text, re.DOTALL
        ):
            for token in _tokenize_cmake_args(match.group(1)):
                sources.append(os.path.normpath(os.path.join(rel_dir, token)))

        for match in re.finditer(
            r"target_link_libraries\(\s*glintfx_library\s+PRIVATE\s*(.*?)\)", text, re.DOTALL
        ):
            libs.extend(_tokenize_cmake_args(match.group(1)))

        if _GENERATED_GL_LOADER_CALL in text:
            needs_generated_gl_loader = True

        for subdir in _add_subdirectory_calls_for_win32(text):
            walk(os.path.join(rel_dir, subdir))

    walk("src")

    # GEMEO (L-17) de check_container_fixture_link.py's own colisao-de-
    # nome-de-arquivo: se dois layers algum dia listarem o MESMO
    # basename (ex.: dois "display_adapter.cpp"), um /Fo por diretorio
    # unico faltaria e o segundo .obj pisaria no primeiro no MESMO
    # comando `cl /LD` - silenciosamente, sem erro de link nenhum
    # (apenas um simbolo do arquivo perdido some). Detectado aqui,
    # nunca deixado para o `cl` explicar mal.
    basenames = {}
    for src in sources:
        base = os.path.basename(src)
        basenames.setdefault(base, []).append(src)
    collisions = {b: paths for b, paths in basenames.items() if len(paths) > 1}
    if collisions:
        details = "; ".join(f"{b}: {paths}" for b, paths in collisions.items())
        fail(
            "colisao de nome-base entre fontes de glintfx_library na configuracao WIN32 "
            f"(cada uma perderia a outra no MESMO /Fo de diretorio unico): {details}"
        )

    return sources, libs, needs_generated_gl_loader, visited_files


def discover_generated_gl_source(candidate_build_roots):
    """gl_functions.cpp (o loader GL 3.3 core, GL-LOADER) e GERADO em
    tempo de build por gl_registry_codegen a partir do gl.xml
    vendorizado (src/render/CMakeLists.txt) - nao existe como arquivo
    rastreado para este script simplesmente ler. Reconstruir o host
    tool inteiro dentro do container MSVC so para este portao seria
    trabalho pesado fora de escopo desta fatia (o gerador e um binario
    Linux, o conteudo gerado e C++ puro, sem nada especifico de
    plataforma - ver src/render/CMakeLists.txt's own "UNLIKE the
    Wayland binding" comment): este portao reaproveita o arquivo que UM
    configure normal deste repositorio (Linux, o unico que os alvos
    deste projeto rodam) ja gerou, de qualquer diretorio de build
    existente, na ordem dada - o mesmo principio do README's own
    "export.hpp gerado em build-preci/generated/include" (reusar
    geracao Linux para um cl.exe que so recusa a parte que E
    especifica de plataforma, a visibility attribute). Devolve o
    primeiro caminho que existir, ou None (o chamador declara a
    ausencia, nunca finge que gerou)."""
    for root in candidate_build_roots:
        candidate = os.path.join(root, "generated", "render", "gl_functions.cpp")
        sibling_header = os.path.join(root, "generated", "render", "gl_functions.hpp")
        if os.path.isfile(candidate) and os.path.isfile(sibling_header):
            return candidate
    return None


def discover_generated_include_dir(candidate_build_roots):
    """version_macros.hpp e o outro gerado que os headers publicos
    precisam (glintfx_configure_version_header(), src/CMakeLists.txt) -
    mesma razao/mesma descoberta de discover_generated_gl_source()
    acima."""
    for root in candidate_build_roots:
        candidate = os.path.join(root, "generated", "include", "glintfx")
        if os.path.isdir(candidate) and os.path.isfile(
            os.path.join(candidate, "version_macros.hpp")
        ):
            return candidate
    return None


def default_generated_build_roots(repo_root):
    # Preferencia por frescor: build-preci/ e o que tools/preci.sh's
    # own stage_configure produz e mantem atualizado a cada push local;
    # build/ e o mais generico (usado direto por um `cmake --build
    # build` manual); build-preci-sanitize/build-preci-debug sao os
    # ultimos recursos, gerados por estagios que tambem configuram a
    # arvore inteira.
    here = os.path.dirname(os.path.abspath(__file__))
    # tests/tools/ -> raiz do proprio repositorio ONDE ESTE SCRIPT VIVE,
    # nao necessariamente repo_root: repo_root pode ser um worktree
    # (GODS_LAWS.md L-55) sem build/ proprio nenhum - o gerado, sendo
    # OS-agnostico, vem do checkout principal quando o worktree nao tem
    # o seu.
    own_repo_root = os.path.normpath(os.path.join(here, "..", ".."))
    roots = []
    for base in (repo_root, own_repo_root):
        for name in ("build-preci", "build", "build-preci-sanitize", "build-preci-debug"):
            candidate = os.path.join(base, name)
            if candidate not in roots:
                roots.append(candidate)
    return roots


# --- 3. write_msvc_export_header ------------------------------------------


# GLINTFX_API real (__declspec), NAO a variante GLINTFX_LIBRARY_STATIC_
# DEFINE que o README usa para /c isolado (tools/msvc-container/
# README.md linha ~121-128): aquela existe para contornar um problema
# de FERRAMENTA (o export.hpp gerado por um `cmake` configurado com GCC
# carrega __attribute__((visibility)), que o cl.exe real nao entende) e
# colapsa GLINTFX_API para nada - o que apagaria justamente a diferenca
# exportado/nao-exportado que o DEFEITO 1 desta fatia existe para
# provar (secao 0 do plano: "o defeito e exclusivo do modo
# compartilhado"). Aqui o par dllexport/dllimport tem de ser real.
_MSVC_EXPORT_HEADER = """// SPDX-License-Identifier: AGPL-3.0-or-later
// Gerado por check_win32_test_link.py - par dllexport/dllimport REAL
// para o cl.exe da Microsoft (o export.hpp que um configure CMake+GCC
// gera carrega __attribute__((visibility)), que o cl.exe nao entende -
// ver README.md deste diretorio, secao do GLINTFX_LIBRARY_STATIC_
// DEFINE). GLINTFX_LIBRARY_STATIC_DEFINE NUNCA e definido por este
// gate: colapsar GLINTFX_API para nada apagaria a distincao
// exportado/nao-exportado que o DEFEITO 1 (CORE-LOG-CI) existe para
// provar.
#ifndef GLINTFX_API_H
#define GLINTFX_API_H

#ifndef GLINTFX_API
#  ifdef glintfx_library_EXPORTS
#    define GLINTFX_API __declspec(dllexport)
#  else
#    define GLINTFX_API __declspec(dllimport)
#  endif
#endif

#ifndef GLINTFX_LIBRARY_NO_EXPORT
#  define GLINTFX_LIBRARY_NO_EXPORT
#endif

#ifndef GLINTFX_LIBRARY_DEPRECATED
#  define GLINTFX_LIBRARY_DEPRECATED __declspec(deprecated)
#endif

#ifndef GLINTFX_LIBRARY_DEPRECATED_EXPORT
#  define GLINTFX_LIBRARY_DEPRECATED_EXPORT GLINTFX_API GLINTFX_LIBRARY_DEPRECATED
#endif

#ifndef GLINTFX_LIBRARY_DEPRECATED_NO_EXPORT
#  define GLINTFX_LIBRARY_DEPRECATED_NO_EXPORT GLINTFX_LIBRARY_NO_EXPORT GLINTFX_LIBRARY_DEPRECATED
#endif

#endif /* GLINTFX_API_H */
"""


def write_msvc_export_header(scratch, generated_include_src_dir):
    dest_dir = os.path.join(scratch, "generated_include", "glintfx")
    os.makedirs(dest_dir, exist_ok=True)
    with open(os.path.join(dest_dir, "export.hpp"), "w", encoding="utf-8") as handle:
        handle.write(_MSVC_EXPORT_HEADER)
    if generated_include_src_dir is not None:
        shutil.copyfile(
            os.path.join(generated_include_src_dir, "version_macros.hpp"),
            os.path.join(dest_dir, "version_macros.hpp"),
        )
    return dest_dir


# --- execucao via docker ---------------------------------------------------


def _run_docker(image, repo_root, scratch, shell_command, timeout_seconds):
    """UM `docker run` = um processo neste host por chamada (GODS_LAWS.
    md L-11) - tudo que o comando shell precisa (harness, fontes,
    include dirs) ja mora dentro de /src (repo_root, so leitura) ou
    /build (scratch, leitura e escrita). Sufixo :ro,Z / :Z de
    reetiquetagem SELinux, GODS_LAWS.md L-60 e a nota do CLAUDE.md deste
    projeto - nunca semodule/audit2allow."""
    args = [
        "docker",
        "run",
        "--rm",
        "-v",
        f"{repo_root}:/src:ro,Z",
        "-v",
        f"{scratch}:/build:Z",
        image,
        "bash",
        "-c",
        f"cd /build && {shell_command}",
    ]
    start = time.monotonic()
    try:
        result = subprocess.run(args, capture_output=True, text=True, timeout=timeout_seconds)
        elapsed = time.monotonic() - start
        return result.returncode, result.stdout, result.stderr, elapsed, False
    except subprocess.TimeoutExpired as exc:
        elapsed = time.monotonic() - start
        stdout = exc.stdout.decode("utf-8", "replace") if isinstance(exc.stdout, bytes) else (exc.stdout or "")
        stderr = exc.stderr.decode("utf-8", "replace") if isinstance(exc.stderr, bytes) else (exc.stderr or "")
        return None, stdout, stderr, elapsed, True


# --- 4. build_library_dll --------------------------------------------------


def build_library_dll(image, repo_root, scratch, sources, libs, generated_gl_source, timeout_seconds):
    """UMA chamada `cl /LD`: toda fonte de glintfx_library (mais o
    loader GL gerado, se a configuracao precisar dele) num unico
    glintfx.dll + glintfx.lib. Modo COMPARTILHADO de proposito (secao 0
    do plano): e o unico modo em que o DEFEITO 1 existe - no modo
    estatico o .lib conteria gpu_kind_report.obj de qualquer jeito e o
    alvo win32_iconic_present_test ligaria mesmo com o defeito."""
    container_sources = [f"/src/{src}" for src in sources]
    extra_include_flag = ""
    if generated_gl_source is not None:
        # gl_functions.hpp e gl_functions.cpp sao gerados JUNTOS
        # (tools/gl_registry_codegen), e gl_functions.cpp inclui o
        # ".hpp" IRMAO por nome nu ("gl_functions.hpp") - os dois
        # precisam estar no MESMO diretorio dentro do container, nao so
        # o .cpp. gl_functions.hpp por sua vez inclui "gl_abi.hpp"/
        # "gl_proc_address.hpp" por nome nu (os hand-written de
        # src/render/) - dai o /I extra em src/render, o MESMO par que
        # glintfx_add_generated_gl_loader() (src/render/CMakeLists.txt)
        # passa como PRIVATE include dir de verdade.
        generated_dir = os.path.dirname(generated_gl_source)
        dest_dir = os.path.join(scratch, "generated_render")
        os.makedirs(dest_dir, exist_ok=True)
        shutil.copyfile(generated_gl_source, os.path.join(dest_dir, "gl_functions.cpp"))
        shutil.copyfile(
            os.path.join(generated_dir, "gl_functions.hpp"),
            os.path.join(dest_dir, "gl_functions.hpp"),
        )
        container_sources.append("/build/generated_render/gl_functions.cpp")
        extra_include_flag = "/I /build/generated_render /I /src/src/render "

    objs_dir = os.path.join(scratch, "objs_lib")
    os.makedirs(objs_dir, exist_ok=True)

    lib_flags = " ".join(f"{lib}.lib" for lib in dict.fromkeys(libs))
    source_args = " ".join(shlex.quote(s) for s in container_sources)
    link_clause = f"/link {lib_flags}" if lib_flags else ""

    command = (
        "cl /nologo /std:c++latest /Zc:__cplusplus /EHsc /W4 /WX /LD "
        "/I /src/include /I /build/generated_include /I /src/src "
        f"{extra_include_flag}"
        "/D_WIN32=1 /DWIN32=1 /D_WIN32_WINNT=0x0A00 /Dglintfx_library_EXPORTS "
        '/Fo"/build/objs_lib/" /Fe"/build/glintfx.dll" '
        f"{source_args} {link_clause}"
    )
    returncode, stdout, stderr, elapsed, timed_out = _run_docker(
        image, repo_root, scratch, command, timeout_seconds
    )
    ok = (
        not timed_out
        and returncode == 0
        and os.path.isfile(os.path.join(scratch, "glintfx.lib"))
    )
    return {
        "ok": ok,
        "returncode": returncode,
        "stdout": stdout,
        "stderr": stderr,
        "elapsed": elapsed,
        "timed_out": timed_out,
    }


# --- 5. link_one_test -------------------------------------------------------


def link_one_test(image, repo_root, scratch, target, timeout_seconds):
    """UMA chamada `cl ... /link glintfx.lib <libs>` por alvo - o
    harness (tests/harness/*.cpp) mais o proprio <nome>.cpp mais as
    fontes extras que tests/CMakeLists.txt lista para ESTE alvo
    (cmake/GlintfxTest.cmake's own glintfx_add_test(): todo teste linca
    glintfx::glintfx inteiro E o proprio <nome>.cpp - o target_sources
    extra e sempre uma segunda compilacao de arquivo(s) de src/, nunca
    substitui esses dois)."""
    name = target["name"]
    harness_sources = [
        "/src/tests/harness/harness_main.cpp",
        "/src/tests/harness/check.cpp",
        "/src/tests/harness/test_registry.cpp",
    ]
    test_source = f"/src/tests/{name}.cpp"
    extra_sources = [f"/src/{src}" for src in target["sources"]]
    all_sources = harness_sources + [test_source] + extra_sources

    objs_dir = os.path.join(scratch, f"objs_{name}")
    os.makedirs(objs_dir, exist_ok=True)

    lib_flags = " ".join(f"{lib}.lib" for lib in dict.fromkeys(target["libs"]))
    source_args = " ".join(shlex.quote(s) for s in all_sources)
    link_clause = f"/link /LIBPATH:/build glintfx.lib {lib_flags}".rstrip()

    command = (
        "cl /nologo /std:c++latest /Zc:__cplusplus /EHsc /W4 /WX "
        "/I /src/include /I /build/generated_include /I /src/src "
        "/D_WIN32=1 /DWIN32=1 /D_WIN32_WINNT=0x0A00 "
        f'/Fo"/build/objs_{name}/" /Fe"/build/{name}.exe" '
        f"{source_args} {link_clause}"
    )
    returncode, stdout, stderr, elapsed, timed_out = _run_docker(
        image, repo_root, scratch, command, timeout_seconds
    )
    return {
        "name": name,
        "returncode": returncode,
        "stdout": stdout,
        "stderr": stderr,
        "elapsed": elapsed,
        "timed_out": timed_out,
    }


# GODS_LAWS.md L-49: distingue ferramenta morrendo (timeout, codigo de
# saida >=128 - um sinal matando o processo) de reprovacao LEGITIMA do
# `link.exe` (LNK2019/LNK1120, codigo de saida pequeno e positivo - o
# cl.exe real usa 2, nao 1, medido ao vivo nesta maquina antes de
# escrever esta funcao). Uma reprovacao sem NENHUMA linha "LNK" no
# texto tambem cai em "ambiente": o `link.exe` nao chegou a rodar (erro
# de flag, arquivo ausente etc.), o que e um problema desta ferramenta/
# receita, nunca do codigo sob teste.
def classify_link_result(result):
    if result["timed_out"]:
        return "ambiente", "timeout da invocacao docker (GODS_LAWS.md L-49: ferramenta, nao codigo)"
    if result["returncode"] is None:
        return "ambiente", "docker nao devolveu codigo de saida"
    if result["returncode"] == 0:
        return "ligou", None
    if result["returncode"] >= 128:
        return (
            "ambiente",
            f"codigo de saida {result['returncode']} (>=128, ferramenta morrendo - GODS_LAWS.md L-49)",
        )
    text = (result["stdout"] or "") + (result["stderr"] or "")
    if "LNK" in text:
        return "falhou", None
    return (
        "ambiente",
        f"codigo de saida {result['returncode']} sem nenhuma linha LNK - link.exe nao chegou a rodar",
    )


# --- 6. run_link_check ------------------------------------------------------


def new_summary():
    return {
        "alvos_encontrados": 0,
        "fontes_biblioteca_encontradas": 0,
        "ligados": 0,
        "falharam": 0,
        "ambiente": 0,
        "tempo_total_s": 0.0,
    }


def run_link_check(repo_root, image, timeout_seconds):
    cmake_text = read_file(os.path.join(repo_root, "tests", "CMakeLists.txt"))
    targets = extract_win32_test_targets(cmake_text)

    summary = new_summary()
    summary["alvos_encontrados"] = len(targets)
    errors = []
    detalhes_lnk = []

    if len(targets) == 0:
        errors.append(
            "varredura vazia: nenhum glintfx_add_test() dentro de um bloco if(WIN32) em "
            "tests/CMakeLists.txt - GODS_LAWS.md L-40, isto e sinal de coleta quebrada, nunca "
            "de 'nenhum teste win32 existe'"
        )
        return summary, errors

    sources, libs, needs_gl_loader, visited_files = collect_win32_library_layout(repo_root)
    summary["fontes_biblioteca_encontradas"] = len(sources)
    if len(sources) == 0:
        errors.append(
            "varredura vazia: nenhuma fonte de glintfx_library encontrada para a configuracao "
            f"WIN32 (arquivos CMake visitados: {visited_files}) - GODS_LAWS.md L-40"
        )
        return summary, errors

    build_roots = default_generated_build_roots(repo_root)
    generated_gl_source = discover_generated_gl_source(build_roots) if needs_gl_loader else None
    if needs_gl_loader and generated_gl_source is None:
        errors.append(
            "src/render/CMakeLists.txt chama glintfx_add_generated_gl_loader(glintfx_library), mas "
            f"nenhum generated/render/gl_functions.cpp foi achado em {build_roots} - configure a "
            "arvore (Linux) ao menos uma vez antes de rodar este portao (o loader e OS-agnostico, "
            "gerado uma vez e reaproveitado, ver discover_generated_gl_source() no cabecalho deste "
            "arquivo)"
        )
        return summary, errors
    generated_include_dir = discover_generated_include_dir(build_roots)

    scratch = tempfile.mkdtemp(prefix="glintfx-win32-link-", dir=os.environ.get("TMPDIR"))
    try:
        write_msvc_export_header(scratch, generated_include_dir)

        dll_result = build_library_dll(
            image, repo_root, scratch, sources, libs, generated_gl_source, timeout_seconds
        )
        summary["tempo_total_s"] += dll_result["elapsed"]

        if not dll_result["ok"]:
            # A DLL e pre-requisito de TODO alvo win32_* - nenhum deles
            # chega a ser tentado, e cada um e contado como "ambiente"
            # (nao "falhou": o link.exe de cada teste individual nunca
            # rodou) para que ligados+falharam+ambiente==encontrados
            # continue batendo (GODS_LAWS.md L-36/L-40).
            summary["ambiente"] = len(targets)
            reason = "timeout" if dll_result["timed_out"] else f"rc={dll_result['returncode']}"
            errors.append(
                f"glintfx.dll (pre-requisito de todo alvo win32_*) nao compilou/ligou ({reason}):\n"
                f"{stderr_tail(dll_result['stdout'] + dll_result['stderr'])}"
            )
            return summary, errors

        for target in targets:
            result = link_one_test(image, repo_root, scratch, target, timeout_seconds)
            summary["tempo_total_s"] += result["elapsed"]
            classification, note = classify_link_result(result)
            if classification == "ligou":
                summary["ligados"] += 1
            elif classification == "falhou":
                summary["falharam"] += 1
                detalhes_lnk.append(
                    f"{result['name']}: nao ligou -> {stderr_tail(result['stdout'] + result['stderr'])}"
                )
            else:
                summary["ambiente"] += 1
                detalhes_lnk.append(f"{result['name']}: ambiente -> {note}")
    finally:
        shutil.rmtree(scratch, ignore_errors=True)

    total_classificado = summary["ligados"] + summary["falharam"] + summary["ambiente"]
    if total_classificado != summary["alvos_encontrados"]:
        errors.append(
            "reconciliacao falhou (GODS_LAWS.md L-36): "
            f"ligados({summary['ligados']}) + falharam({summary['falharam']}) + "
            f"ambiente({summary['ambiente']}) = {total_classificado} != "
            f"alvos_encontrados({summary['alvos_encontrados']})"
        )
    if summary["falharam"] > 0 or summary["ambiente"] > 0:
        errors.extend(detalhes_lnk)

    return summary, errors


# --- 7. print_summary -------------------------------------------------------


def print_summary(summary):
    print(
        f"{SCRIPT_NAME}: alvos encontrados={summary['alvos_encontrados']} | "
        f"fontes biblioteca encontradas={summary['fontes_biblioteca_encontradas']} | "
        f"ligados={summary['ligados']} | falharam={summary['falharam']} | "
        f"ambiente={summary['ambiente']} | tempo total (s)={summary['tempo_total_s']:.1f}"
    )


# --- modo real ---------------------------------------------------------------


def real_main(repo_root, image, timeout_seconds):
    if not os.path.isdir(repo_root):
        fail(f"repo-root nao existe: {repo_root}")

    if shutil.which("docker") is None:
        print(
            f"{SCRIPT_NAME}: declarado NAO APLICAVEL: docker ausente neste host, 0 de 1 exercido "
            "(este portao roda cl.exe/link.exe reais dentro de glintfx-msvc:latest, ver tools/"
            "msvc-container/README.md)"
        )
        sys.exit(GATE_SKIP_RETURN_CODE)

    probe = subprocess.run(
        ["docker", "image", "inspect", image], capture_output=True, text=True
    )
    if probe.returncode != 0:
        print(
            f"{SCRIPT_NAME}: declarado NAO APLICAVEL: imagem '{image}' fora do cache local, "
            "0 de 1 exercido - construa-a com tools/msvc-container/README.md antes (este portao "
            "nunca baixa/constroi a imagem sozinho, GODS_LAWS.md L-14)"
        )
        sys.exit(GATE_SKIP_RETURN_CODE)

    summary, errors = run_link_check(os.path.abspath(repo_root), image, timeout_seconds)
    print_summary(summary)

    if errors:
        print(f"{SCRIPT_NAME}: REPROVADO ({len(errors)} problema(s)):", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        sys.exit(1)

    print(f"{SCRIPT_NAME}: OK - todo alvo win32_* de tests/CMakeLists.txt ligou contra o glintfx.dll real")


# --- --selftest --------------------------------------------------------------
#
# GODS_LAWS.md L-36: tres controles - positivo (parsing correto de um
# bloco if(WIN32) real), negativo/vazio (zero glintfx_add_test dentro
# do bloco reprova, e zero fontes reprova), e o VERMELHO real do
# `link.exe` (o motivo deste portao existir) contra um par de fixtures
# sinteticas, no MESMO desenho de check_container_fixture_link.py's
# own selftest_missing_atom_reproves()/selftest_positive_control().


def _selftest_parsing_positive():
    cmake_text = """
if(WIN32)
    glintfx_add_test(fake_alpha_test)
    target_include_directories(fake_alpha_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
    target_sources(fake_alpha_test PRIVATE
        "${PROJECT_SOURCE_DIR}/src/platform/win32/alpha.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/win32/beta.cpp"
    )
    target_link_libraries(fake_alpha_test PRIVATE user32 gdi32)
endif()
"""
    targets = extract_win32_test_targets(cmake_text)
    ok = (
        len(targets) == 1
        and targets[0]["name"] == "fake_alpha_test"
        and targets[0]["sources"] == [
            "src/platform/win32/alpha.cpp",
            "src/platform/win32/beta.cpp",
        ]
        and targets[0]["libs"] == ["user32", "gdi32"]
    )
    if not ok:
        print(f"selftest: PARSING-POSITIVO FALHOU: {targets}", file=sys.stderr)
        return False
    print(f"selftest: PARSING-POSITIVO OK: {targets}")
    return True


def _selftest_parsing_empty_block():
    cmake_text = """
if(WIN32)
    # bloco real, mas sem nenhum glintfx_add_test dentro - o piso de
    # run_link_check() e quem reprova isto, esta funcao so confirma que
    # a extracao devolve lista vazia (nao quebra, nao inventa alvo).
    message(STATUS "nada aqui")
endif()
"""
    targets = extract_win32_test_targets(cmake_text)
    if targets != []:
        print(f"selftest: PARSING-VAZIO FALHOU (esperava lista vazia): {targets}", file=sys.stderr)
        return False
    print("selftest: PARSING-VAZIO OK (bloco if(WIN32) sem glintfx_add_test, lista vazia)")
    return True


def _selftest_layout_respects_win32_branch(scratch):
    """src/ sintetico com o MESMO desenho if(UNIX)/elseif(WIN32) que
    src/platform/CMakeLists.txt usa de verdade: uma fonte incondicional,
    uma so-Linux, uma so-Windows - collect_win32_library_layout() tem
    de trazer a incondicional e a so-Windows, e recusar a so-Linux."""
    root = os.path.join(scratch, "layout")
    os.makedirs(root, exist_ok=True)
    _write(
        os.path.join(root, "src", "CMakeLists.txt"),
        "add_library(glintfx_library)\n"
        "add_subdirectory(common)\n"
        "add_subdirectory(platform)\n",
    )
    _write(
        os.path.join(root, "src", "common", "CMakeLists.txt"),
        "target_sources(glintfx_library PRIVATE common_atom.cpp)\n",
    )
    _write(os.path.join(root, "src", "common", "common_atom.cpp"), "int common_atom() { return 1; }\n")
    _write(
        os.path.join(root, "src", "platform", "CMakeLists.txt"),
        "if(UNIX)\n"
        "    add_subdirectory(linux_only)\n"
        "elseif(WIN32)\n"
        "    add_subdirectory(win_only)\n"
        "endif()\n",
    )
    _write(
        os.path.join(root, "src", "platform", "linux_only", "CMakeLists.txt"),
        "target_sources(glintfx_library PRIVATE linux_atom.cpp)\n",
    )
    _write(os.path.join(root, "src", "platform", "linux_only", "linux_atom.cpp"), "int linux_atom() { return 2; }\n")
    _write(
        os.path.join(root, "src", "platform", "win_only", "CMakeLists.txt"),
        "target_sources(glintfx_library PRIVATE win_atom.cpp)\n"
        "target_link_libraries(glintfx_library PRIVATE user32)\n",
    )
    _write(os.path.join(root, "src", "platform", "win_only", "win_atom.cpp"), "int win_atom() { return 3; }\n")

    sources, libs, needs_gl_loader, _visited = collect_win32_library_layout(root)
    ok = (
        sorted(sources)
        == sorted(
            [
                os.path.normpath("src/common/common_atom.cpp"),
                os.path.normpath("src/platform/win_only/win_atom.cpp"),
            ]
        )
        and libs == ["user32"]
        and needs_gl_loader is False
    )
    if not ok:
        print(f"selftest: LAYOUT-WIN32 FALHOU: sources={sources} libs={libs}", file=sys.stderr)
        return False
    print(f"selftest: LAYOUT-WIN32 OK (so-Linux excluido, so-Windows incluido): sources={sources} libs={libs}")
    return True


def _write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as handle:
        handle.write(text)


# VERMELHO real (a razao deste portao existir): um "repo" sintetico com
# glintfx_library exportando uma funcao que chama para dentro de um
# atomo NAO exportado (mesma forma exata do DEFEITO 1 real -
# gpu_kind_report.cpp/resolve_gpu_kind_report(), secao 4.1 do plano) -
# um alvo que lista o atomo em target_sources liga limpo, um que nao
# lista reprova com LNK2019 citando o simbolo real.
def _build_link_fixture(scratch):
    root = os.path.join(scratch, "linkfix")
    os.makedirs(root, exist_ok=True)
    _write(os.path.join(root, "include", "glintfx", "fixture_public.hpp"), "#pragma once\n")
    _write(
        os.path.join(root, "src", "atom_internal.cpp"),
        "// Simbolo NAO exportado - mesma forma do gpu_kind_report.cpp real.\n"
        "int fixture_internal_helper() { return 41; }\n",
    )
    _write(
        os.path.join(root, "src", "exported.cpp"),
        '#include "glintfx/export.hpp"\n'
        "extern int fixture_internal_helper();\n"
        "extern \"C\" GLINTFX_API int fixture_exported_value() { return fixture_internal_helper() + 1; }\n",
    )
    # main() mora SO no harness (mesma forma real: tests/*.cpp deste
    # projeto nunca define main(), GLINTFX_TEST() so registra - achado
    # ao rodar este selftest pela primeira vez, LNK2005 "main already
    # defined" quando o .cpp de teste tambem definia um: sintoma real
    # de ferramenta/fixture, nunca do link.exe reprovando o que este
    # portao existe para provar).
    _write(
        os.path.join(root, "tests", "harness", "harness_main.cpp"),
        "extern int fixture_call_under_test();\n"
        "int main() { return fixture_call_under_test(); }\n",
    )
    _write(os.path.join(root, "tests", "harness", "check.cpp"), "")
    _write(os.path.join(root, "tests", "harness", "test_registry.cpp"), "")
    # O "teste" REPROVADOR: nunca lista atom_internal.cpp no proprio
    # target_sources - so o "teste" VERDE lista.
    _write(
        os.path.join(root, "tests", "fixture_missing_atom_test.cpp"),
        "extern int fixture_internal_helper();\n"
        "int fixture_call_under_test() { return fixture_internal_helper(); }\n",
    )
    _write(
        os.path.join(root, "tests", "fixture_linked_atom_test.cpp"),
        "extern int fixture_internal_helper();\n"
        "int fixture_call_under_test() { return fixture_internal_helper(); }\n",
    )
    return root


def _selftest_real_toolchain(scratch, image, timeout_seconds):
    fixture_root = _build_link_fixture(scratch)
    build_scratch = os.path.join(scratch, "linkfix_build")
    os.makedirs(build_scratch, exist_ok=True)
    write_msvc_export_header(build_scratch, None)

    dll_result = build_library_dll(
        image,
        fixture_root,
        build_scratch,
        ["src/exported.cpp", "src/atom_internal.cpp"],
        [],
        None,
        timeout_seconds,
    )
    if not dll_result["ok"]:
        print(
            f"selftest: REAL-TOOLCHAIN FALHOU (DLL de fixture nao ligou): "
            f"{stderr_tail(dll_result['stdout'] + dll_result['stderr'])}",
            file=sys.stderr,
        )
        return False

    missing_target = {"name": "fixture_missing_atom_test", "sources": [], "libs": []}
    missing_result = link_one_test(image, fixture_root, build_scratch, missing_target, timeout_seconds)
    missing_class, _note = classify_link_result(missing_result)
    if missing_class != "falhou":
        print(
            f"selftest: REAL-TOOLCHAIN FALHOU (vermelho esperado nao veio): {missing_class} "
            f"rc={missing_result['returncode']}\n{stderr_tail(missing_result['stdout'] + missing_result['stderr'])}",
            file=sys.stderr,
        )
        return False
    combined = (missing_result["stdout"] or "") + (missing_result["stderr"] or "")
    if "fixture_internal_helper" not in combined or "LNK2019" not in combined:
        print(
            f"selftest: REAL-TOOLCHAIN FALHOU (vermelho nao cita o simbolo/LNK2019 real): {combined}",
            file=sys.stderr,
        )
        return False
    print(f"selftest: REAL-TOOLCHAIN VERMELHO OK (LNK2019 real citando fixture_internal_helper)")

    linked_target = {
        "name": "fixture_linked_atom_test",
        "sources": ["src/atom_internal.cpp"],
        "libs": [],
    }
    linked_result = link_one_test(image, fixture_root, build_scratch, linked_target, timeout_seconds)
    linked_class, note = classify_link_result(linked_result)
    if linked_class != "ligou":
        print(
            f"selftest: REAL-TOOLCHAIN FALHOU (verde esperado nao veio): {linked_class} ({note}) "
            f"rc={linked_result['returncode']}\n{stderr_tail(linked_result['stdout'] + linked_result['stderr'])}",
            file=sys.stderr,
        )
        return False
    print("selftest: REAL-TOOLCHAIN VERDE OK (mesmo atomo, listado desta vez, liga limpo)")
    return True


def selftest_main(image, timeout_seconds):
    parsing_results = [
        ("parsing-positivo", _selftest_parsing_positive()),
        ("parsing-vazio", _selftest_parsing_empty_block()),
    ]

    scratch = tempfile.mkdtemp(prefix="glintfx-win32-link-selftest-", dir=os.environ.get("TMPDIR"))
    try:
        layout_ok = _selftest_layout_respects_win32_branch(scratch)
        parsing_results.append(("layout-win32", layout_ok))

        docker_available = shutil.which("docker") is not None
        if docker_available:
            probe = subprocess.run(["docker", "image", "inspect", image], capture_output=True)
            docker_available = probe.returncode == 0

        if docker_available:
            toolchain_ok = _selftest_real_toolchain(scratch, image, timeout_seconds)
            parsing_results.append(("real-toolchain", toolchain_ok))
        else:
            print(
                f"{SCRIPT_NAME} --selftest: docker/imagem '{image}' ausentes - controle "
                "real-toolchain PULADO, contado e declarado (GODS_LAWS.md L-40), nunca escondido",
                file=sys.stderr,
            )
            parsing_results.append(("real-toolchain", None))
    finally:
        shutil.rmtree(scratch, ignore_errors=True)

    ran = [ok for _name, ok in parsing_results if ok is not None]
    skipped = [name for name, ok in parsing_results if ok is None]
    print(
        f"{SCRIPT_NAME} --selftest: controles executados: {len(ran)}/{len(parsing_results)} | "
        f"pulados: {len(skipped)} ({', '.join(skipped) if skipped else 'nenhum'})"
    )

    if not all(ran):
        print(f"{SCRIPT_NAME} --selftest: FALHOU (ver acima)", file=sys.stderr)
        sys.exit(1)
    if skipped:
        sys.exit(GATE_SKIP_RETURN_CODE)
    print(f"{SCRIPT_NAME} --selftest: os {len(ran)} controles OK")


# --- main --------------------------------------------------------------------


_USAGE = (
    "usage: check_win32_test_link.py --exec <repo-root> [--image <docker-image>] "
    "[--timeout-seconds <n>]  |  --selftest [--image <docker-image>] [--timeout-seconds <n>]"
)


# Parsing manual de argv, nao argparse (mesma escolha de check_
# container_fixture_link.py's own main()): um positional que comeca com
# "--" (o proprio modo, "--exec"/"--selftest") confunde o parser de
# opcoes do argparse, que tenta casa-lo contra uma flag registrada antes
# de considerar posicional.
def main():
    args = sys.argv[1:]
    if not args:
        fail(_USAGE)

    mode = args[0]
    rest = args[1:]
    image = DEFAULT_IMAGE
    timeout_seconds = DEFAULT_TIMEOUT_SECONDS
    repo_root = None

    i = 0
    while i < len(rest):
        token = rest[i]
        if token == "--image":
            i += 1
            if i >= len(rest):
                fail("--image exige um valor")
            image = rest[i]
        elif token == "--timeout-seconds":
            i += 1
            if i >= len(rest):
                fail("--timeout-seconds exige um valor")
            try:
                timeout_seconds = int(rest[i])
            except ValueError:
                fail(f"--timeout-seconds precisa de um inteiro, recebi '{rest[i]}'")
        elif repo_root is None and not token.startswith("--"):
            repo_root = token
        else:
            fail(f"argumento nao reconhecido: {token}\n{_USAGE}")
        i += 1

    if mode == "--selftest":
        selftest_main(image, timeout_seconds)
        return
    if mode == "--exec":
        if not repo_root:
            fail(_USAGE)
        real_main(repo_root, image, timeout_seconds)
        return
    fail(_USAGE)


if __name__ == "__main__":
    main()
