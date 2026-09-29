#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# tools/ci/floor.sh - CI-SPLIT-PER-OS D-A14 (item 2): piso de ferramentas
# do lado Linux, fonte UNICA para o job `linux` e para os 5 jobs Linux
# fixos que compilam (lint, sanitizer, gl-codegen-host-cross, debug,
# clang). Era o passo "Piso de ferramentas" escrito dentro do job
# `linux` (A3b); os fixos nao tinham piso nenhum. O passo continua se
# chamando "Piso de ferramentas" no ci.yml e roda ANTES do marco
# `id: prep` (G5 de tests/tools/check_ci_step_independence.py confere).
#
# Falha alto e cedo, com a causa explicita, se qualquer piso falhar:
#   - GCC >= 14 (`__GNUC__` de `${CXX:-c++}`);
#   - CMake >= 4.1 (cmake_minimum_required, CMakeLists.txt), com a guarda
#     B2 (revisao do CTO): `cmake --version` quebrado deixava a versao
#     VAZIA e `[ "" -lt 4 ]` nao e' erro fatal dentro de `if` sob `set
#     -e` - o piso imprimia "OK" com rc=0 (medido com um stub de cmake
#     que sai 127). `case` valida numerico (major e minor) antes de
#     qualquer comparacao `-lt`/`-eq`;
#   - python3 ou python no PATH (DEPZERO-TRACE, tests/CMakeLists.txt);
#   - xdg-shell.xml presente (L-05, Wayland puro).
#
# Compilador Clang (job `clang`, CXX=clang++): `clang++` define
# `__GNUC__` = 4 por compatibilidade, entao o piso de GCC nao se aplica
# a ele. O projeto NAO declara piso de versao de Clang em lugar nenhum
# (CMakeLists.txt, README.md, ESCOPO.md, CONTRACT.md: medido em
# 29/09/2026) - este script so' valida que `__clang_major__` e' numerico
# e imprime a versao; o numero do piso, se existir, e' decisao do lider
# (L-01) e entra aqui quando ele decidir.
#
# `--autoteste`: roda o proprio script contra compilador e cmake FALSOS
# (nunca contra a maquina), provando que cada piso reprova quando deve
# - inclusive o conserto B2 (cmake sem versao). Chamado por
# `tools/preci.sh --selftest`.

set -eu

CXX_BIN="${CXX:-c++}"

# stderr, nunca stdout: as checagens abaixo rodam dentro de `$(...)`, que
# engoliria a mensagem no valor de retorno (medido pelo proprio
# autoteste, primeira versao deste arquivo).
fail_floor() {
    echo "PISO NAO ATENDIDO: $1" >&2
    exit 1
}

compiler_macros() {
    "$CXX_BIN" -dM -E -x c++ /dev/null 2>/dev/null || true
}

macro_value() {
    printf '%s\n' "$1" | awk -v m="$2" '$2 == m {print $3}'
}

# Ecoa a descricao do compilador ("GCC 14", "Clang 21") ou sai 1.
check_compiler() {
    macros="$(compiler_macros)"
    clang_major="$(macro_value "$macros" __clang_major__)"
    if [ -n "$clang_major" ]; then
        case "$clang_major" in
            *[!0-9]*) fail_floor "__clang_major__ nao numerico ('$clang_major', CXX=$CXX_BIN)" ;;
        esac
        echo "Clang $clang_major"
        return
    fi
    gnuc_major="$(macro_value "$macros" __GNUC__)"
    case "$gnuc_major" in
        ''|*[!0-9]*)
            fail_floor "__GNUC__ nao numerico ou ausente (saida: '${gnuc_major:-<vazio>}', CXX=$CXX_BIN) - compilador quebrado ou nao aceita -dM -E"
            ;;
    esac
    if [ "$gnuc_major" -lt 14 ]; then
        fail_floor "__GNUC__ = '$gnuc_major', piso do projeto e' 14 (CXX=$CXX_BIN)"
    fi
    echo "GCC $gnuc_major"
}

# Ecoa a versao do CMake ou sai 1.
check_cmake() {
    cmake_versao="$(cmake --version 2>/dev/null | head -1 | awk '{print $3}')" || true
    [ -n "$cmake_versao" ] \
        || fail_floor "'cmake --version' nao devolveu versao nenhuma - cmake ausente ou quebrado"
    cmake_major="${cmake_versao%%.*}"
    cmake_minor="${cmake_versao#*.}"
    cmake_minor="${cmake_minor%%.*}"
    case "$cmake_major" in
        ''|*[!0-9]*) fail_floor "cmake --version = '$cmake_versao', major nao numerico" ;;
    esac
    case "$cmake_minor" in
        ''|*[!0-9]*) fail_floor "cmake --version = '$cmake_versao', minor nao numerico" ;;
    esac
    if [ "$cmake_major" -lt 4 ] || { [ "$cmake_major" -eq 4 ] && [ "$cmake_minor" -lt 1 ]; }; then
        fail_floor "cmake --version = '$cmake_versao', piso do projeto e' 4.1 (CMakeLists.txt)"
    fi
    echo "$cmake_versao"
}

check_python() {
    if ! command -v python3 >/dev/null 2>&1 && ! command -v python >/dev/null 2>&1; then
        fail_floor "nem python3 nem python no PATH (DEPZERO-TRACE, tests/CMakeLists.txt exige um dos dois)"
    fi
}

# Ecoa o caminho do xdg-shell.xml ou sai 1.
check_xdg_shell() {
    xml="$(pkg-config --variable=pkgdatadir wayland-protocols 2>/dev/null)/stable/xdg-shell/xdg-shell.xml" || true
    [ -f "$xml" ] || fail_floor "falta xdg-shell.xml em $xml (L-05, Wayland puro)"
    echo "$xml"
}

run_floor() {
    compilador="$(check_compiler)" || exit 1
    cmake_ver="$(check_cmake)" || exit 1
    check_python
    xml_path="$(check_xdg_shell)" || exit 1
    echo "piso de ferramentas OK: $compilador, CMake $cmake_ver, python presente, $xml_path presente"
}

# --- autoteste: compilador, cmake e pkg-config FALSOS ---------------------

write_stub_compiler() {
    printf '#!/bin/sh\nprintf "%%s\\n" %s\n' "$2" >"$1"
    chmod +x "$1"
}

write_stub_cmake() {
    printf '#!/bin/sh\n%s\n' "$2" >"$1"
    chmod +x "$1"
}

# Roda o floor.sh real (`$0`) num subprocesso com o PATH e o CXX dados.
# Imprime "<rc>" numa linha e a saida do piso nas seguintes.
run_floor_isolated() {
    (
        CXX="$1" PATH="$2:$PATH" bash "$0" 2>&1
    ) || echo "rc=$?"
}

expect_reproved() {
    nome="$1"
    resultado="$(run_floor_isolated "$2" "$3")" || true
    case "$resultado" in
        *"PISO NAO ATENDIDO"*"$4"*)
            echo "autoteste: $nome OK (reprovou, com a causa certa)"
            ;;
        *)
            echo "autoteste: $nome FALHOU (esperava reprovar por '$4'): $resultado"
            return 1
            ;;
    esac
}

expect_passes() {
    nome="$1"
    resultado="$(run_floor_isolated "$2" "$3")" || true
    case "$resultado" in
        *"piso de ferramentas OK: $4"*)
            echo "autoteste: $nome OK ($4)"
            ;;
        *)
            echo "autoteste: $nome FALHOU (esperava passar com '$4'): $resultado"
            return 1
            ;;
    esac
}

run_autoteste() {
    work="$(mktemp -d "${TMPDIR:-/tmp}/glintfx-floor-autoteste.XXXXXX")"
    mkdir -p "$work/xdg/stable/xdg-shell" "$work/ok" "$work/cmake40" "$work/nocmake" "$work/cmakejunk" "$work/noxdg"
    : >"$work/xdg/stable/xdg-shell/xdg-shell.xml"
    for d in ok cmake40 nocmake cmakejunk noxdg; do
        xdgdir="$work/xdg"
        [ "$d" = "noxdg" ] && xdgdir="$work/inexistente"
        printf '#!/bin/sh\necho %s\n' "$xdgdir" >"$work/$d/pkg-config"
        chmod +x "$work/$d/pkg-config"
    done
    write_stub_cmake "$work/ok/cmake" 'echo "cmake version 4.1.6"'
    write_stub_cmake "$work/cmake40/cmake" 'echo "cmake version 4.0.9"'
    write_stub_cmake "$work/nocmake/cmake" 'exit 127'
    write_stub_cmake "$work/cmakejunk/cmake" 'echo "cmake version x.y"'
    write_stub_cmake "$work/noxdg/cmake" 'echo "cmake version 4.1.6"'
    write_stub_compiler "$work/gcc14" '"#define __GNUC__ 14"'
    write_stub_compiler "$work/gcc13" '"#define __GNUC__ 13"'
    write_stub_compiler "$work/clang" '"#define __GNUC__ 4" "#define __clang_major__ 21"'
    write_stub_compiler "$work/broken" '""'

    codigo=0
    expect_passes "positivo (GCC 14, CMake 4.1.6)" "$work/gcc14" "$work/ok" "GCC 14, CMake 4.1.6" || codigo=1
    expect_passes "clang nao cai no piso de GCC (__GNUC__=4)" "$work/clang" "$work/ok" "Clang 21, CMake 4.1.6" || codigo=1
    expect_reproved "GCC 13" "$work/gcc13" "$work/ok" "piso do projeto e' 14" || codigo=1
    expect_reproved "compilador sem __GNUC__" "$work/broken" "$work/ok" "__GNUC__ nao numerico ou ausente" || codigo=1
    expect_reproved "P3: cmake sem versao (B2)" "$work/gcc14" "$work/nocmake" "nao devolveu versao nenhuma" || codigo=1
    expect_reproved "cmake 4.0" "$work/gcc14" "$work/cmake40" "piso do projeto e' 4.1" || codigo=1
    expect_reproved "cmake nao numerico" "$work/gcc14" "$work/cmakejunk" "major nao numerico" || codigo=1
    expect_reproved "xdg-shell.xml ausente" "$work/gcc14" "$work/noxdg" "falta xdg-shell.xml" || codigo=1
    rm -rf "$work"

    if [ "$codigo" -ne 0 ]; then
        echo "floor.sh --autoteste: FALHOU"
        exit 1
    fi
    echo "floor.sh --autoteste: os 8 controles OK"
}

case "${1:-}" in
    --autoteste) run_autoteste ;;
    "") run_floor ;;
    *) echo "uso: floor.sh [--autoteste]"; exit 2 ;;
esac
