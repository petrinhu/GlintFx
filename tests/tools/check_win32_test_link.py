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
#                                     alvos APLICAVEIS ao Windows (WIN-
#                                     CROSS-STAGE S6: incondicional +
#                                     if(WIN32), nunca if(UNIX)/if(NOT
#                                     WIN32)) com suas fontes/libs, mais
#                                     a contagem de exclusao por motivo
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
    sources(...)/target_link_libraries(...)/target_compile_definitions(
    ...) capturado por regex), aceitando tanto tokens entre aspas
    ("${PROJECT_SOURCE_DIR}/src/...") quanto tokens nus (version.cpp,
    user32) - as duas formas coexistem entre tests/CMakeLists.txt e
    src/**/CMakeLists.txt. `shlex.split()` (modo POSIX, default) ja
    remove a aspa dupla que ENVOLVE um token inteiro - achado ao
    escrever WIN-CROSS-STAGE S6 (10/09/2026, ao extrair target_compile_
    definitions pela primeira vez neste arquivo): o `.strip('"')` extra
    que costumava vir depois disto era redundante no caso comum e
    ATIVAMENTE ERRADO no caso de um token cujo VALOR de verdade termina
    em aspa literal (`GLTFX_X_SOURCE="...arquivo.hpp\\""` vira, depois
    de shlex.split, `GLTFX_X_SOURCE="...arquivo.hpp"` - com a aspa
    final fazendo parte do DADO, nao um envelope; `.strip('"')` comia
    essa aspa de dado por engano, tarde demais para reconstruir).
    Removido - shlex.split() sozinho ja e o parser certo aqui."""
    tokens = []
    for line in raw_text.splitlines():
        stripped = _strip_cmake_comment(line).strip()
        if not stripped:
            continue
        tokens.extend(shlex.split(stripped))
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


# WIN-CROSS-STAGE S6 (D-4, plano em /var/tmp/glintfx-plan/win-cross-
# stage.md): antes desta fatia, so' os glintfx_add_test() dentro de um
# if(WIN32) contavam (15 hoje) - os 65 incondicionais (fora de
# qualquer if) NUNCA passavam pelo cl.exe local, e foi exatamente ai
# que 5edd6bf escondeu um defeito de teste que so' o servidor viu.
# Passa a devolver TODO glintfx_add_test() aplicavel ao Windows:
# incondicional OU dentro de if(WIN32); EXCLUI so' o que esta
# EXPLICITAMENTE sob if(UNIX)/if(NOT WIN32) - as unicas duas formas
# que algum dia citam UNIX/WIN32 ao redor de um glintfx_add_test() em
# toda a arvore de tests/CMakeLists.txt hoje (medido: `if(elseif(else`
# ao redor de add_test so' usa "WIN32", "UNIX" ou nenhuma - nunca "NOT
# WIN32" na pratica, mas a forma e reconhecida do mesmo jeito, para o
# dia em que alguem escrever uma).
#
# Reusa o MESMO walker se/elseif/else/endif por profundidade que
# collect_win32_library_layout() ja usa mais abaixo (_IF_RE/_ELSEIF_RE/
# _ELSE_RE/_ENDIF_RE, definidos logo depois desta funcao neste mesmo
# modulo - Python resolve nomes de modulo em tempo de CHAMADA, nao de
# definicao, entao a ordem textual nao importa aqui).
_ADD_TEST_ANY_RE = re.compile(r"glintfx_add_test\(\s*(\w+)\s*\)")
# Reaproveitado por collect_win32_library_layout() mais abaixo tambem
# (_add_subdirectory_calls_for_win32) - UM so' padrao de "endif()" para
# o arquivo inteiro, nunca dois padroes divergentes por acidente.
_ENDIF_RE = re.compile(r"^\s*endif\s*\(\s*\)\s*$")


def _excludes_windows(condition_text):
    """True so' para as duas formas literais que EXCLUEM Windows
    (GODS_LAWS.md L-40: reconhecer de MENOS e' o lado seguro aqui - um
    alvo que deveria ser excluido e nao foi e' descoberto na hora de
    ligar (rc de link real), um alvo que sumiu por engano nunca seria
    descoberto por ninguem)."""
    return condition_text.strip() in ("UNIX", "NOT WIN32")


def extract_win32_test_targets(cmake_text):
    """(targets, exclusion_counts) - targets e' TODO glintfx_add_test()
    aplicavel ao Windows (incondicional + if(WIN32)), cada um com as
    fontes extras (target_sources) e libs extras (target_link_
    libraries) que o MESMO nome de alvo declara EM QUALQUER LUGAR do
    arquivo (busca global por nome, nao mais so' dentro do bloco
    if(WIN32) - convencao real deste projeto: target_sources/target_
    link_libraries de um alvo sempre citam o MESMO nome que o
    glintfx_add_test() dele, nunca um nome diferente). exclusion_counts
    e' um dict {condicao_textual: quantidade} dos alvos EXCLUIDOS por
    if(UNIX)/if(NOT WIN32) - sempre presente, mesmo vazio (piso de
    FORMATO, GODS_LAWS.md L-40: o chamador confere sources+excluidos ==
    total bruto). Zero alvos aplicaveis e' piso vazio, verificado pelo
    chamador (run_link_check), nao aqui - esta funcao so reporta o que
    encontrou."""
    stripped_text = _strip_cmake_comments_from_text(cmake_text)
    lines = stripped_text.splitlines()
    targets = []
    exclusion_counts = {}
    stack = []

    i = 0
    while i < len(lines):
        line = lines[i]
        if_match = _IF_RE.match(line)
        elseif_match = _ELSEIF_RE.match(line)
        else_match = _ELSE_RE.match(line)
        endif_match = _ENDIF_RE.match(line)
        if if_match:
            stack.append(if_match.group(1).strip())
        elif elseif_match:
            if not stack:
                fail(f"elseif() sem if() correspondente em tests/CMakeLists.txt, linha {i + 1}")
            stack[-1] = elseif_match.group(1).strip()
        elif else_match:
            if not stack:
                fail(f"else() sem if() correspondente em tests/CMakeLists.txt, linha {i + 1}")
            stack[-1] = "__else__"
        elif endif_match:
            if not stack:
                fail(f"endif() sem if() correspondente em tests/CMakeLists.txt, linha {i + 1}")
            stack.pop()
        else:
            for match in _ADD_TEST_ANY_RE.finditer(line):
                name = match.group(1)
                excluding = [c for c in stack if _excludes_windows(c)]
                if excluding:
                    reason = excluding[-1]
                    exclusion_counts[reason] = exclusion_counts.get(reason, 0) + 1
                    continue
                raw_sources = _extract_call_args(stripped_text, "target_sources", name)
                raw_defines = _extract_call_args(stripped_text, "target_compile_definitions", name)
                targets.append(
                    {
                        "name": name,
                        "sources": [_resolve_project_source_dir_token(s) for s in raw_sources],
                        "libs": _extract_call_args(stripped_text, "target_link_libraries", name),
                        # target_compile_definitions - achado ao rodar S6 contra a
                        # arvore real (10/09/2026): dois alvos "oracle" (log_
                        # field_test, gfss_declaration_registry_doc_oracle_test)
                        # citam macro GLTFX_..._SOURCE apontando para o proprio
                        # arquivo-fonte deles, via ${PROJECT_SOURCE_DIR} - resolvido
                        # para /src (o mount do container), igual sources/libs.
                        # O VALOR nunca precisa ser um caminho Windows valido: este
                        # estagio nunca EXECUTA o binario (S5), so compila - a
                        # macro so precisa ser uma string literal valida em tempo
                        # de compilacao.
                        "defines": [
                            d.replace("${PROJECT_SOURCE_DIR}", "/src") for d in raw_defines
                        ],
                    }
                )
        i += 1

    if stack:
        fail(f"tests/CMakeLists.txt termina com {len(stack)} bloco(s) if() sem endif() correspondente")

    return targets, exclusion_counts


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


# --- 4b. build_harness_objects (WIN-CROSS-STAGE S6, otimizacao condicionada) -
#
# ANTES desta fatia, link_one_test() recompilava as MESMAS 3 fontes do
# harness (tests/harness/*.cpp) em TODO alvo - 15 vezes hoje, dezenas
# depois que extract_win32_test_targets() passou a devolver todo alvo
# aplicavel (nao so os win32_*). Condicao medida em 10/09/2026 (nao
# adivinhada, GODS_LAWS.md L-43: criterio antes do dado): o tempo TOTAL
# do estagio win32-link ja era MAIOR que o do stage_sanitizer da mesma
# rodada ANTES desta fatia expandir a contagem de alvos - `tools/
# preci.sh --win32-link-only` mediu 110.7s com 15 alvos; `time tools/
# preci.sh --sanitizer-only` mediu 83.3s. Por isso a otimizacao entra
# JUNTO com a expansao de escopo desta fatia, nao como passo separado
# condicionado a uma comparacao em tempo de execucao (a condicao que o
# plano descreve ja estava satisfeita pelo medido, e so fica mais
# verdadeira com mais alvos).
_HARNESS_SOURCES = (
    "/src/tests/harness/harness_main.cpp",
    "/src/tests/harness/check.cpp",
    "/src/tests/harness/test_registry.cpp",
)
_HARNESS_OBJ_NAMES = ("harness_main.obj", "check.obj", "test_registry.obj")


def build_harness_objects(image, repo_root, scratch, timeout_seconds):
    """UMA chamada docker (GODS_LAWS.md L-11) que compila o harness uma
    unica vez; os .obj resultantes sao reusados por link_one_test() em
    TODO alvo, em vez de cada alvo recompilar as mesmas 3 fontes."""
    objs_dir = os.path.join(scratch, "objs_harness")
    os.makedirs(objs_dir, exist_ok=True)
    source_args = " ".join(shlex.quote(s) for s in _HARNESS_SOURCES)
    command = (
        "cl /nologo /std:c++latest /Zc:__cplusplus /EHsc /W4 /WX /c "
        "/I /src/include /I /build/generated_include /I /src/src "
        "/D_WIN32=1 /DWIN32=1 /D_WIN32_WINNT=0x0A00 "
        '/Fo"/build/objs_harness/" '
        f"{source_args}"
    )
    returncode, stdout, stderr, elapsed, timed_out = _run_docker(
        image, repo_root, scratch, command, timeout_seconds
    )
    ok = (
        not timed_out
        and returncode == 0
        and all(os.path.isfile(os.path.join(objs_dir, n)) for n in _HARNESS_OBJ_NAMES)
    )
    return {
        "ok": ok,
        "returncode": returncode,
        "stdout": stdout,
        "stderr": stderr,
        "elapsed": elapsed,
        "timed_out": timed_out,
        "obj_paths": [f"/build/objs_harness/{n}" for n in _HARNESS_OBJ_NAMES] if ok else [],
    }


# --- 5. link_one_test -------------------------------------------------------


def link_one_test(image, repo_root, scratch, target, timeout_seconds, harness_objs=None):
    """UMA chamada `cl ... /link glintfx.lib <libs>` por alvo - o
    harness (tests/harness/*.cpp, ou os .obj pre-compilados por
    build_harness_objects() quando `harness_objs` e' dado - cl aceita
    misturar .cpp e .obj na mesma linha de comando) mais o proprio
    <nome>.cpp mais as fontes extras que tests/CMakeLists.txt lista
    para ESTE alvo (cmake/GlintfxTest.cmake's own glintfx_add_test():
    todo teste linca glintfx::glintfx inteiro E o proprio <nome>.cpp -
    o target_sources extra e sempre uma segunda compilacao de
    arquivo(s) de src/, nunca substitui esses dois)."""
    name = target["name"]
    harness_inputs = list(harness_objs) if harness_objs else list(_HARNESS_SOURCES)
    test_source = f"/src/tests/{name}.cpp"
    extra_sources = [f"/src/{src}" for src in target["sources"]]
    all_sources = harness_inputs + [test_source] + extra_sources

    objs_dir = os.path.join(scratch, f"objs_{name}")
    os.makedirs(objs_dir, exist_ok=True)

    lib_flags = " ".join(f"{lib}.lib" for lib in dict.fromkeys(target["libs"]))
    source_args = " ".join(shlex.quote(s) for s in all_sources)
    link_clause = f"/link /LIBPATH:/build glintfx.lib {lib_flags}".rstrip()
    # shlex.quote() na flag /D INTEIRA (nao so no valor): o valor pode
    # conter aspas literais (macro de string, "..." dentro do proprio
    # /D), e precisam chegar ao cl.exe intactas - shlex.quote() decide
    # sozinho a forma mais segura de preservar isso pelo bash -c do
    # container (mesma tecnica ja usada para source_args acima).
    define_flags = " ".join(shlex.quote(f"/D{d}") for d in target.get("defines", []))

    command = (
        "cl /nologo /std:c++latest /Zc:__cplusplus /EHsc /W4 /WX "
        "/I /src/include /I /build/generated_include /I /src/src "
        "/D_WIN32=1 /DWIN32=1 /D_WIN32_WINNT=0x0A00 "
        f"{define_flags + ' ' if define_flags else ''}"
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
# `cl.exe`/`link.exe` (codigo de saida pequeno e positivo - o cl.exe
# real usa 2, nao 1, medido ao vivo nesta maquina antes de escrever
# esta funcao). Reprovacao legitima tem DUAS formas, nao uma: falha de
# LINK (uma linha "LNK", ex. LNK2019/LNK1120) OU falha de COMPILACAO
# (uma linha "error C<numero>", ex. C2220/C3861/C1083) - um erro de
# compilacao acontece ANTES do link.exe rodar e por isso NUNCA carrega
# "LNK" (medido ao vivo, 10/09/2026: `cl /W4 /WX` sobre fonte com
# identificador inexistente sai com rc=2 e so' "error C3861", nenhuma
# linha LNK; a mesma forma do achado real revertendo o guard
# `#ifndef _MSC_VER` de tests/hostile_gawk_macros_shim.hpp, que produz
# 573 avisos C4081 mais 1 "error C2220" e nenhuma linha LNK). A versao
# anterior desta funcao classificava esse caso como "ambiente", o que
# e' FALSO: e' exatamente o tipo de regressao real de codigo que este
# portao existe para pegar. So' cai em "ambiente" a reprovacao que nao
# tem NENHUMA das duas formas: nem "LNK" nem "error C<numero>" no
# texto - ai' sim o cl.exe/link.exe nao chegou a produzir um
# diagnostico de codigo (erro de flag, arquivo ausente etc.), problema
# desta ferramenta/receita, nunca do codigo sob teste.
_MSVC_COMPILE_ERROR_RE = re.compile(r"error C\d+\b")


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
    if "LNK" in text or _MSVC_COMPILE_ERROR_RE.search(text):
        return "falhou", None
    return (
        "ambiente",
        f"codigo de saida {result['returncode']} sem linha LNK nem 'error C<numero>' - cl.exe/link.exe "
        "nao chegou a produzir um diagnostico de codigo",
    )


# --- 6a. measure_strict_family (WIN-CROSS-STAGE S4, D-3b) -------------------
#
# A familia de conversao numerica que o /W4 padrao deixa DESLIGADA
# (C4365/C4388/C4242/C4254 - avisos de conversao com possivel perda de
# dado). O texto de 06/09 falava em "familia de conversao numerica que
# quebrou uma rodada"; o historico real (`git log --format='%b' | grep
# -o 'C4[0-9]\{3\}'`) so mostra C4996/C4297/C4273/C4081 - nenhum de
# conversao. Esta e a familia MSVC padrao de conversao, nao a
# reconstrucao daquela rodada especifica (INFERENCIA do plano, nao
# fato).
#
# MEDE, NUNCA REPROVA (por isso roda SEM /WX): promover qualquer um
# destes quatro codigos a erro e decisao do lider (obrigaria mudar
# codigo de produto) - este script so imprime a contagem, sempre,
# mesmo zero (GODS_LAWS.md L-40, "sempre imprime" e o piso aqui, nao
# "zero reprova": ausencia de contagem impressa e que seria o defeito).
#
# Escopo: as fontes de glintfx_library (as mesmas 77 que build_library_
# dll ja compila) - e' codigo de PRODUTO, o que uma decisao futura de
# reprovar afetaria; os alvos de teste ficam fora desta medicao.
#
# UMA chamada docker extra (GODS_LAWS.md L-11): todas as fontes num so
# `cl /c` (sem /LD, sem link - so' se precisa do resultado textual dos
# avisos, nunca do binario).
_STRICT_WARNING_CODES = ("C4365", "C4388", "C4242", "C4254")
_STRICT_EXTRA_FLAGS = "/w44365 /w44388 /w44242 /w44254"


def measure_strict_family(image, repo_root, sources, generated_gl_source, generated_include_dir, timeout_seconds):
    scratch = tempfile.mkdtemp(prefix="glintfx-win32-strict-", dir=os.environ.get("TMPDIR"))
    try:
        write_msvc_export_header(scratch, generated_include_dir)

        container_sources = [f"/src/{src}" for src in sources]
        extra_include_flag = ""
        if generated_gl_source is not None:
            generated_dir = os.path.dirname(generated_gl_source)
            dest_dir = os.path.join(scratch, "generated_render_strict")
            os.makedirs(dest_dir, exist_ok=True)
            shutil.copyfile(generated_gl_source, os.path.join(dest_dir, "gl_functions.cpp"))
            shutil.copyfile(
                os.path.join(generated_dir, "gl_functions.hpp"),
                os.path.join(dest_dir, "gl_functions.hpp"),
            )
            container_sources.append("/build/generated_render_strict/gl_functions.cpp")
            extra_include_flag = "/I /build/generated_render_strict /I /src/src/render "

        objs_dir = os.path.join(scratch, "objs_strict")
        os.makedirs(objs_dir, exist_ok=True)
        source_args = " ".join(shlex.quote(s) for s in container_sources)

        command = (
            f"cl /nologo /std:c++latest /Zc:__cplusplus /EHsc /W4 {_STRICT_EXTRA_FLAGS} /c "
            "/I /src/include /I /build/generated_include /I /src/src "
            f"{extra_include_flag}"
            "/D_WIN32=1 /DWIN32=1 /D_WIN32_WINNT=0x0A00 /Dglintfx_library_EXPORTS "
            f'/Fo"/build/objs_strict/" '
            f"{source_args}"
        )
        returncode, stdout, stderr, elapsed, timed_out = _run_docker(
            image, repo_root, scratch, command, timeout_seconds
        )
    finally:
        shutil.rmtree(scratch, ignore_errors=True)

    text = (stdout or "") + (stderr or "")
    counts = {code: len(re.findall(rf"warning {code}\b", text)) for code in _STRICT_WARNING_CODES}
    return {
        "counts": counts,
        "returncode": returncode,
        "timed_out": timed_out,
        "elapsed": elapsed,
        "raw_tail": stderr_tail(text),
    }


def print_strict_summary(measurement):
    counts = measurement["counts"]
    parts = " ".join(f"{code}={counts[code]}" for code in _STRICT_WARNING_CODES)
    print(
        f"{SCRIPT_NAME}: avisos estritos ({measurement['elapsed']:.1f}s, SEM reprovar - decisao "
        f"do lider pendente, GODS_LAWS.md L-02/D-3b): {parts}"
    )
    if measurement["timed_out"] or measurement["returncode"] not in (0, None) and measurement["returncode"] != 0:
        # rc != 0 aqui nao e' o codigo destas quatro famílias (elas
        # nunca promovem a erro, /WX esta fora de proposito) - e' um
        # problema DIFERENTE (erro real de compilacao ou timeout);
        # declarado, nunca escondido atras da contagem.
        print(
            f"{SCRIPT_NAME}: aviso - a medicao estrita terminou com rc={measurement['returncode']} "
            f"timeout={measurement['timed_out']} (nao e' um dos quatro codigos medidos; isto e' "
            f"diferente de C4365/C4388/C4242/C4254): {measurement['raw_tail']}"
        )


# --- 6b. bloco "NAO MEDIDO AQUI" (WIN-CROSS-STAGE S5) ------------------------
#
# O resumo de S1-S4 so imprime contagens do que ESTE estagio faz. S5
# fecha a lacuna oposta: declarar, sempre, o que ele NAO faz - para que
# ninguem confunda "este estagio passou" com "o Windows inteiro foi
# provado". Duas partes fixas: (a) a versao real do cl.exe lida DENTRO
# do container (nunca presumida igual ao servidor - windows-latest
# atualiza sem aviso); (b) a lista do que fica de fora, cada item com o
# achado/job real que documenta por que (nunca uma alegacao vazia).


def _read_cl_version(image, repo_root, timeout_seconds):
    """`cl` sem nenhum argumento imprime o banner de versao no STDERR e
    sai com erro - achado real ao escrever esta funcao (10/09/2026,
    medido ao vivo neste container): o wrapper `cl` deste projeto
    (msvc-wine) so' entrega o banner quando WINE_MSVC_RAW_STDOUT=1 esta
    setada - sem essa variavel, o caminho normal (via msvctricks.exe,
    mkfifo) PERDE o banner (nao aparece nem em stdout nem em stderr).
    Toda outra invocacao deste script usa /nologo (que suprimiria o
    banner de qualquer forma - saida limpa de proposito para os outros
    parsers), por isso esta e' uma chamada docker PROPRIA (GODS_LAWS.md
    L-11: fixa, uma vez por execucao, nunca por arquivo/alvo)."""
    scratch = tempfile.mkdtemp(prefix="glintfx-win32-clver-", dir=os.environ.get("TMPDIR"))
    try:
        _returncode, stdout, stderr, _elapsed, _timed_out = _run_docker(
            image, repo_root, scratch, "WINE_MSVC_RAW_STDOUT=1 cl", timeout_seconds
        )
    finally:
        shutil.rmtree(scratch, ignore_errors=True)
    match = re.search(r"Version\s+([\d.]+)", (stdout or "") + (stderr or ""))
    return match.group(1) if match else None


def _raw_add_test_grep_count(repo_root):
    """Mesma convencao de `grep -c 'glintfx_add_test(' tests/CMakeLists.
    txt` (substring por LINHA, nao regex de nome) - inclui as poucas
    linhas de comentario/prosa que citam a chamada sem invoca-la
    (medido: 5 em 10/09/2026). Usado so' para a contagem de "alvos
    multiplataforma nao exercitados" enquanto WIN-CROSS-TESTS-LINK
    (WIN-CROSS-STAGE S6) nao fecha com uma contagem exata por motivo de
    exclusao - ver o comentario do proprio chamador."""
    text = read_file(os.path.join(repo_root, "tests", "CMakeLists.txt"))
    return sum(1 for line in text.splitlines() if "glintfx_add_test(" in line)


_NOT_MEASURED_ITEMS = (
    "clang-tidy (bugprone-exception-escape, misc-misplaced-const e as demais familias do job "
    "`Windows - Lint`) - so ele ve isto (WIN-LINT-ENUM, d2cf182)",
    "execucao de qualquer binario Windows - so o job `windows` do CI prova isto (ASSET-PARITY-WIN; "
    "Wine e pista, nunca oraculo - ver a matriz de roteamento do README)",
    "/analyze (analise estatica nativa do MSVC)",
    "rc/mt/LTCG (recursos, manifest, link-time code generation)",
    "o ramo if(WIN32) do proprio CMake como CONFIGURE real (defeitos ja vividos como 65ffe87 - este "
    "estagio nunca roda cmake, so' cl/link direto)",
)


def _not_measured_block_ok(text):
    """Piso de FORMATO do resumo (GODS_LAWS.md L-40, aplicado ao texto
    impresso, nao a uma varredura de arquivos): exige a linha de versao
    do compilador e pelo menos 6 linhas 'NAO MEDIDO AQUI:' (os 5 itens
    fixos + a contagem de alvos multiplataforma)."""
    has_version_line = "compilador deste estagio: cl.exe" in text
    not_measured_count = text.count("NAO MEDIDO AQUI:")
    return has_version_line and not_measured_count >= 6


def build_not_measured_block(image, repo_root, timeout_seconds, alvos_encontrados, alvos_excluidos):
    cl_version = _read_cl_version(image, repo_root, timeout_seconds)
    version_text = cl_version if cl_version else "desconhecida (nao foi possivel ler 'cl' no container)"
    # WIN-CROSS-STAGE S6 fechou: extract_win32_test_targets() ja devolve
    # TODO alvo aplicavel, entao a diferenca contra a contagem bruta de
    # texto (`grep -c 'glintfx_add_test('`) nunca mais e' "nao
    # exercitado" - e' so' a soma de duas coisas estruturais: exclusao
    # explicita (if(UNIX)/if(NOT WIN32)) e as poucas linhas de
    # comentario/prosa que citam a chamada sem invoca-la (5 em
    # 10/09/2026). Declarado como o que E, nunca mais como pendencia.
    raw_add_test_count = _raw_add_test_grep_count(repo_root)
    ruido_comentario = raw_add_test_count - alvos_encontrados - alvos_excluidos
    lines = [
        f"{SCRIPT_NAME}: compilador deste estagio: cl.exe {version_text} - o servidor usa o MSVC de "
        "`windows-latest`, versao lida so no log do CI, NUNCA presumida igual"
    ]
    for item in _NOT_MEASURED_ITEMS:
        lines.append(f"{SCRIPT_NAME}: NAO MEDIDO AQUI: {item}")
    lines.append(
        f"{SCRIPT_NAME}: NAO MEDIDO AQUI: {raw_add_test_count} ocorrencia(s) bruta(s) de "
        f"'glintfx_add_test(' no arquivo = {alvos_encontrados} aplicavel(is) e ligado(s) aqui + "
        f"{alvos_excluidos} excluido(s) estruturalmente (if(UNIX)/if(NOT WIN32)) + {ruido_comentario} "
        "mencao(oes) em comentario/prosa (nunca invocam a chamada de verdade)"
    )
    return "\n".join(lines)


def print_not_measured_block(image, repo_root, timeout_seconds, alvos_encontrados, alvos_excluidos):
    text = build_not_measured_block(image, repo_root, timeout_seconds, alvos_encontrados, alvos_excluidos)
    if not _not_measured_block_ok(text):
        fail(
            "bloco 'NAO MEDIDO AQUI' malformado (GODS_LAWS.md L-40 aplicado ao formato do resumo) - "
            f"nao imprimindo um resumo que finge ter a forma certa:\n{text}"
        )
    print(text)


def _selftest_not_measured_block_positive():
    sample = "check_win32_test_link.py: compilador deste estagio: cl.exe 19.51.36256 - ...\n" + "\n".join(
        f"check_win32_test_link.py: NAO MEDIDO AQUI: item {i}" for i in range(6)
    )
    ok = _not_measured_block_ok(sample)
    if not ok:
        print(
            "selftest: NOT-MEASURED-FORMATO-POSITIVO FALHOU (bloco bem formado foi reprovado)",
            file=sys.stderr,
        )
        return False
    print("selftest: NOT-MEASURED-FORMATO-POSITIVO OK")
    return True


def _selftest_not_measured_block_negative():
    # A MESMA sabotagem que a prova de S5 descreve: apaga a linha de
    # versao, mantem as 6 linhas NAO MEDIDO AQUI.
    sample = "\n".join(f"check_win32_test_link.py: NAO MEDIDO AQUI: item {i}" for i in range(6))
    ok = _not_measured_block_ok(sample)
    if ok:
        print(
            "selftest: NOT-MEASURED-FORMATO-NEGATIVO FALHOU (bloco sem linha de versao foi aprovado)",
            file=sys.stderr,
        )
        return False
    print("selftest: NOT-MEASURED-FORMATO-NEGATIVO OK (reprovado por faltar a linha de versao)")
    return True


def _fake_link_result(returncode, stdout="", stderr="", timed_out=False):
    return {
        "name": "fixture_sintetico",
        "returncode": returncode,
        "stdout": stdout,
        "stderr": stderr,
        "elapsed": 0.0,
        "timed_out": timed_out,
    }


# GODS_LAWS.md L-49: os quatro controles sinteticos abaixo cobrem a
# distincao que WIN-CROSS-STAGE reprovou (classify_link_result()
# classificando erro de COMPILACAO como "ambiente" so por nao ter
# linha "LNK") sem depender do container/imagem - rodam sempre, mesmo
# quando docker/glintfx-msvc:latest estao ausentes. O caso real com
# cl.exe de verdade (LNK2019 e compilacao) mora em _selftest_real_
# toolchain(), abaixo; estes sao a rede de seguranca rapida contra
# REGRESSAO da propria classificacao.
def _selftest_classify_compile_error_falhou():
    # Mesma forma do achado real (broken.cpp(1): error C3861: ...) -
    # rc pequeno e positivo, ZERO linhas "LNK".
    result = _fake_link_result(2, stdout="broken.cpp(1): error C3861: 'x': identifier not found\n")
    classification, note = classify_link_result(result)
    if classification != "falhou":
        print(
            f"selftest: CLASSIFY-COMPILACAO-FALHOU FALHOU: esperava 'falhou', veio "
            f"'{classification}' (nota={note})",
            file=sys.stderr,
        )
        return False
    print("selftest: CLASSIFY-COMPILACAO-FALHOU OK (error C sem LNK classifica 'falhou')")
    return True


def _selftest_classify_link_error_falhou():
    result = _fake_link_result(
        2,
        stdout="main.obj : error LNK2019: unresolved external symbol\nout.exe : fatal error LNK1120: 1 unresolved externals\n",
    )
    classification, note = classify_link_result(result)
    if classification != "falhou":
        print(
            f"selftest: CLASSIFY-LINK-FALHOU FALHOU: esperava 'falhou', veio '{classification}' (nota={note})",
            file=sys.stderr,
        )
        return False
    print("selftest: CLASSIFY-LINK-FALHOU OK (LNK2019/LNK1120 classifica 'falhou')")
    return True


def _selftest_classify_ambiente_sem_diagnostico():
    # O lado OPOSTO que o conserto nao pode inverter: rc pequeno e
    # positivo, mas SEM "LNK" e SEM "error C<numero>" - a forma real de
    # um erro de linha de comando do proprio cl.exe (driver, nao
    # compilador: "D8003" e' familia D, nunca C).
    result = _fake_link_result(2, stdout="cl : Command line error D8003: missing source filename\n")
    classification, note = classify_link_result(result)
    if classification != "ambiente":
        print(
            f"selftest: CLASSIFY-AMBIENTE-SEM-DIAGNOSTICO FALHOU: esperava 'ambiente', veio "
            f"'{classification}' (nota={note})",
            file=sys.stderr,
        )
        return False
    print("selftest: CLASSIFY-AMBIENTE-SEM-DIAGNOSTICO OK (sem LNK e sem error C continua 'ambiente')")
    return True


def _selftest_classify_ambiente_ferramenta_morrendo():
    # O outro lado que ja funcionava antes do conserto (timeout e
    # rc>=128) - continua intacto: nao pode virar "falhou" so porque um
    # texto qualquer contem a substring "error" em algum lugar.
    timeout_ok = classify_link_result(_fake_link_result(None, timed_out=True))[0] == "ambiente"
    killed_ok = classify_link_result(_fake_link_result(139))[0] == "ambiente"
    if not (timeout_ok and killed_ok):
        print(
            f"selftest: CLASSIFY-AMBIENTE-FERRAMENTA-MORRENDO FALHOU: timeout_ok={timeout_ok} "
            f"killed_ok={killed_ok}",
            file=sys.stderr,
        )
        return False
    print("selftest: CLASSIFY-AMBIENTE-FERRAMENTA-MORRENDO OK (timeout e rc>=128 continuam 'ambiente')")
    return True


# --- 6. run_link_check ------------------------------------------------------


def new_summary():
    return {
        "alvos_encontrados": 0,
        "fontes_biblioteca_encontradas": 0,
        "ligados": 0,
        "falharam": 0,
        "ambiente": 0,
        "exclusion_counts": {},
        "tempo_total_s": 0.0,
    }


def run_link_check(repo_root, image, timeout_seconds):
    cmake_text = read_file(os.path.join(repo_root, "tests", "CMakeLists.txt"))
    targets, exclusion_counts = extract_win32_test_targets(cmake_text)

    summary = new_summary()
    summary["alvos_encontrados"] = len(targets)
    summary["exclusion_counts"] = exclusion_counts
    errors = []
    detalhes_lnk = []

    if len(targets) == 0:
        errors.append(
            "varredura vazia: nenhum glintfx_add_test() aplicavel ao Windows (incondicional ou "
            "if(WIN32)) em tests/CMakeLists.txt - GODS_LAWS.md L-40, isto e sinal de coleta quebrada, nunca "
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

        # WIN-CROSS-STAGE S6: harness compilado UMA vez, reusado por
        # todo alvo (ver o comentario de build_harness_objects()). Se
        # ele falhar, e' pre-requisito ausente igual a DLL - nenhum
        # alvo chega a ser tentado, mesma contabilizacao "ambiente".
        harness_result = build_harness_objects(image, repo_root, scratch, timeout_seconds)
        summary["tempo_total_s"] += harness_result["elapsed"]
        if not harness_result["ok"]:
            summary["ambiente"] = len(targets)
            reason = "timeout" if harness_result["timed_out"] else f"rc={harness_result['returncode']}"
            errors.append(
                f"harness (pre-requisito de todo alvo) nao compilou ({reason}):\n"
                f"{stderr_tail(harness_result['stdout'] + harness_result['stderr'])}"
            )
            return summary, errors
        harness_objs = harness_result["obj_paths"]

        for target in targets:
            result = link_one_test(image, repo_root, scratch, target, timeout_seconds, harness_objs)
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


def _format_exclusion_counts(exclusion_counts):
    if not exclusion_counts:
        return "nenhum"
    return ", ".join(f"{cond}={n}" for cond, n in sorted(exclusion_counts.items()))


def print_summary(summary):
    total_pulados = sum(summary["exclusion_counts"].values())
    print(
        f"{SCRIPT_NAME}: alvos encontrados={summary['alvos_encontrados']} | "
        f"fontes biblioteca encontradas={summary['fontes_biblioteca_encontradas']} | "
        f"ligados={summary['ligados']} | falharam={summary['falharam']} | "
        f"ambiente={summary['ambiente']} | pulados={total_pulados} por: "
        f"{_format_exclusion_counts(summary['exclusion_counts'])} | "
        f"tempo total (s)={summary['tempo_total_s']:.1f}"
    )


# --- modo real ---------------------------------------------------------------


def real_main(repo_root, image, timeout_seconds, strict=False):
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

    abs_repo_root = os.path.abspath(repo_root)
    summary, errors = run_link_check(abs_repo_root, image, timeout_seconds)
    print_summary(summary)

    # WIN-CROSS-STAGE S4: a medicao estrita roda SEMPRE que --strict foi
    # pedido, mesmo quando o gate normal REPROVA logo abaixo - as duas
    # coisas sao ortogonais (um mede sem reprovar, o outro reprova sem
    # medir esta familia), e o chamador quer o numero de qualquer jeito.
    # Recalcula o layout da biblioteca (parse puro, sem docker - barato)
    # em vez de reaproveitar o de run_link_check, que ja fechou seu
    # proprio scratch antes de devolver.
    if strict:
        sources, _libs, needs_gl_loader, _visited = collect_win32_library_layout(abs_repo_root)
        if sources:
            build_roots = default_generated_build_roots(abs_repo_root)
            generated_gl_source = discover_generated_gl_source(build_roots) if needs_gl_loader else None
            generated_include_dir = discover_generated_include_dir(build_roots)
            measurement = measure_strict_family(
                image, abs_repo_root, sources, generated_gl_source, generated_include_dir, timeout_seconds
            )
            print_strict_summary(measurement)
        else:
            print(f"{SCRIPT_NAME}: avisos estritos: PULADO (nenhuma fonte de biblioteca encontrada)")

    # WIN-CROSS-STAGE S5: SEMPRE impresso, com ou sem --strict, com ou
    # sem erro no gate normal - e' declaracao de escopo, nao resultado
    # de teste.
    print_not_measured_block(
        image,
        abs_repo_root,
        timeout_seconds,
        summary["alvos_encontrados"],
        sum(summary["exclusion_counts"].values()),
    )

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
    # WIN-CROSS-STAGE S6: as TRES formas reais que tests/CMakeLists.txt
    # usa ao redor de glintfx_add_test() - incondicional (fake_gamma_
    # test, sem nenhum if ao redor), if(WIN32) (fake_alpha_test) e
    # if(UNIX) (fake_delta_test, tem que ser EXCLUIDO, nunca ligado).
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

glintfx_add_test(fake_gamma_test)
target_sources(fake_gamma_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src/common/gamma.cpp"
)

if(UNIX)
    glintfx_add_test(fake_delta_test)
    target_sources(fake_delta_test PRIVATE
        "${PROJECT_SOURCE_DIR}/src/platform/wayland/delta.cpp"
    )
endif()
"""
    targets, exclusion_counts = extract_win32_test_targets(cmake_text)
    names = sorted(t["name"] for t in targets)
    by_name = {t["name"]: t for t in targets}
    ok = (
        names == ["fake_alpha_test", "fake_gamma_test"]
        and by_name["fake_alpha_test"]["sources"] == [
            "src/platform/win32/alpha.cpp",
            "src/platform/win32/beta.cpp",
        ]
        and by_name["fake_alpha_test"]["libs"] == ["user32", "gdi32"]
        and by_name["fake_gamma_test"]["sources"] == ["src/common/gamma.cpp"]
        and exclusion_counts == {"UNIX": 1}
    )
    if not ok:
        print(
            f"selftest: PARSING-POSITIVO FALHOU: targets={targets} exclusion_counts={exclusion_counts}",
            file=sys.stderr,
        )
        return False
    print(f"selftest: PARSING-POSITIVO OK: targets={names} exclusion_counts={exclusion_counts}")
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
    targets, exclusion_counts = extract_win32_test_targets(cmake_text)
    if targets != [] or exclusion_counts != {}:
        print(
            f"selftest: PARSING-VAZIO FALHOU (esperava lista vazia): targets={targets} "
            f"exclusion_counts={exclusion_counts}",
            file=sys.stderr,
        )
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
    # VERMELHO de COMPILACAO (a forma que o conserto de classify_link_
    # result() acrescenta, GODS_LAWS.md L-49): identificador inexistente
    # e' erro de COMPILACAO, nao de link - `cl.exe` nunca chega a
    # invocar `link.exe`, entao o texto nao carrega nenhuma linha "LNK"
    # (medido ao vivo, 10/09/2026, ver o comentario de classify_link_
    # result()). Precisa classificar "falhou", nunca "ambiente".
    _write(
        os.path.join(root, "tests", "fixture_compile_error_test.cpp"),
        "int fixture_call_under_test() { return fixture_this_identifier_does_not_exist(); }\n",
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

    # Segundo vermelho, forma DIFERENTE do primeiro (GODS_LAWS.md L-49):
    # erro de COMPILACAO, nunca de link - prova que classify_link_
    # result() classifica "falhou" mesmo sem nenhuma linha "LNK" no
    # texto (o defeito que reprovou WIN-CROSS-STAGE classificava este
    # caso como "ambiente").
    compile_error_target = {"name": "fixture_compile_error_test", "sources": [], "libs": []}
    compile_error_result = link_one_test(
        image, fixture_root, build_scratch, compile_error_target, timeout_seconds
    )
    compile_error_class, compile_error_note = classify_link_result(compile_error_result)
    if compile_error_class != "falhou":
        print(
            f"selftest: REAL-TOOLCHAIN FALHOU (erro de compilacao real classificado como "
            f"'{compile_error_class}' em vez de 'falhou', nota={compile_error_note}) "
            f"rc={compile_error_result['returncode']}\n"
            f"{stderr_tail(compile_error_result['stdout'] + compile_error_result['stderr'])}",
            file=sys.stderr,
        )
        return False
    compile_error_combined = (compile_error_result["stdout"] or "") + (compile_error_result["stderr"] or "")
    if "error C" not in compile_error_combined or "LNK" in compile_error_combined:
        print(
            f"selftest: REAL-TOOLCHAIN FALHOU (vermelho de compilacao nao tem a forma esperada - "
            f"'error C<numero>' sem nenhuma linha LNK): {compile_error_combined}",
            file=sys.stderr,
        )
        return False
    print("selftest: REAL-TOOLCHAIN VERMELHO-COMPILACAO OK (error C real, nenhuma linha LNK, classificado 'falhou')")

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
        ("not-measured-formato-positivo", _selftest_not_measured_block_positive()),
        ("not-measured-formato-negativo", _selftest_not_measured_block_negative()),
        ("classify-compilacao-falhou", _selftest_classify_compile_error_falhou()),
        ("classify-link-falhou", _selftest_classify_link_error_falhou()),
        ("classify-ambiente-sem-diagnostico", _selftest_classify_ambiente_sem_diagnostico()),
        ("classify-ambiente-ferramenta-morrendo", _selftest_classify_ambiente_ferramenta_morrendo()),
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
    "[--timeout-seconds <n>] [--strict]  |  --selftest [--image <docker-image>] "
    "[--timeout-seconds <n>]"
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
    strict = False

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
        elif token == "--strict":
            # WIN-CROSS-STAGE S4: so vale para --exec (mede codigo de
            # produto contra docker/imagem reais); --selftest nunca
            # aceita esta flag (as fixtures de --selftest nao existem
            # para provar avisos MSVC, so' o comportamento format/tidy/
            # cppcheck - ver o cabecalho deste script).
            strict = True
        elif repo_root is None and not token.startswith("--"):
            repo_root = token
        else:
            fail(f"argumento nao reconhecido: {token}\n{_USAGE}")
        i += 1

    if mode == "--selftest":
        if strict:
            fail(f"--strict nao e valido com --selftest\n{_USAGE}")
        selftest_main(image, timeout_seconds)
        return
    if mode == "--exec":
        if not repo_root:
            fail(_USAGE)
        real_main(repo_root, image, timeout_seconds, strict)
        return
    fail(_USAGE)


if __name__ == "__main__":
    main()
