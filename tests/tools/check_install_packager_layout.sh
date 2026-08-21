#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
# check_install_packager_layout.sh - proves find_package(glintfx) works
# against a Debian-multiarch-style install layout (CMAKE_INSTALL_LIBDIR
# and CMAKE_INSTALL_INCLUDEDIR both non-default), the scenario named by
# the FIX-CONSUMO auditoria (A2/A6) and confirmed still open by the
# FIX-CONSUMO-2 revisao adversarial (achado QA-4).
#
# Root cause investigated (not just re-asserted): CMake's Config-mode
# search only tries "<prefix>/lib/<arch>/cmake/<name>*/" when
# CMAKE_LIBRARY_ARCHITECTURE is set for the CONSUMER's own configure.
# That variable is not a Debian-only patch - it is populated by stock
# CMake itself (cmake_parse_library_architecture(),
# Modules/CMakeParseLibraryArchitecture.cmake) by pattern-matching the
# COMPILER's own implicit link directories against
# CMAKE_LIBRARY_ARCHITECTURE_REGEX (Modules/Platform/Linux-Initialize.cmake).
# On a genuine Debian/Ubuntu multiarch toolchain, gcc reports an
# arch-suffixed implicit lib dir (e.g. /usr/lib/x86_64-linux-gnu), so
# CMAKE_LIBRARY_ARCHITECTURE auto-populates and find_package resolves
# with no extra flag. On Fedora/Arch/CachyOS (no multiarch), gcc never
# reports such a dir, so the variable stays empty and the automatic
# lib/<arch> search path is never tried - this is upstream CMake
# behaviour tied to the COMPILER, not a bug in glintfx-config.cmake.in
# (confirmed empirically: pointing glintfx_DIR or setting
# CMAKE_LIBRARY_ARCHITECTURE by hand on the consumer always resolves it).
#
# TWO scenarios, not one (FIX-CONSUMO-3, secao 5 da revisao adversarial
# de FIX-CONSUMO-2: a versao anterior deste script injetava o hint de
# arquitetura INCONDICIONALMENTE, inclusive na imagem Ubuntu da matriz
# de CI - o unico dos cinco alvos onde a variavel se autopreencheria de
# verdade - entao o caminho zero-flag, o mais comum na pratica
# Debian/Ubuntu, nunca era exercitado em lugar nenhum):
#
#   1. "hinted": configura o consumidor com CMAKE_LIBRARY_ARCHITECTURE
#      explicito - o formato que cross-compilacao, sysroot ou um
#      pipeline de empacotamento de terceiros que fixa a variavel a mao
#      realmente usa. Sempre roda, em qualquer imagem.
#   2. "native zero-flag": configura o consumidor SEM nenhum -D extra,
#      contra um install cuja CMAKE_INSTALL_LIBDIR usa a MESMA
#      arquitetura que ESTE compilador reporta nativamente
#      (CMAKE_CXX_LIBRARY_ARCHITECTURE, lido do proprio
#      CMakeCXXCompiler.cmake que a deteccao de ABI do CMake escreve -
#      nao uma reimplementacao propria do regex). So roda de verdade
#      quando o compilador da imagem reporta essa arquitetura (hoje:
#      Ubuntu). Nas imagens onde nao reporta (Fedora/Arch/CachyOS), o
#      script DECLARA a limitacao com a causa, em vez de fingir que
#      provou o caminho - resultado negativo honesto (GODS_LAWS.md L-27).
#
# Usage: check_install_packager_layout.sh <glintfx-source-dir> <package-src-dir> <cxx-compiler>
#
# Each function below does one thing (GODS_LAWS.md L-17).

set -eu

readonly LIBDIR_ARCH="x86_64-linux-gnu"
readonly NONDEFAULT_LIBDIR="lib/${LIBDIR_ARCH}"
readonly NONDEFAULT_INCLUDEDIR="include/glintfx-packager"

fail() {
    echo "check_install_packager_layout.sh: $1" >&2
    exit 1
}

require_args() {
    [ "$#" -eq 3 ] || fail "usage: check_install_packager_layout.sh <glintfx-source-dir> <package-src-dir> <cxx-compiler>"
    [ -d "$1" ] || fail "glintfx source dir not found: $1"
    [ -d "$2" ] || fail "package source dir not found: $2"
}

make_scratch_workdir() {
    mktemp -d "${TMPDIR:-/tmp}/glintfx-pkglayout-XXXXXX"
}

configure_glintfx_with_packager_layout() {
    glintfx_src="$1"
    build_dir="$2"
    cxx="$3"
    libdir="$4"
    cmake -S "$glintfx_src" -B "$build_dir" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_COMPILER="$cxx" \
        -DCMAKE_INSTALL_LIBDIR="$libdir" \
        -DCMAKE_INSTALL_INCLUDEDIR="$NONDEFAULT_INCLUDEDIR" \
        -DGLINTFX_BUILD_TESTS=OFF
}

build_and_install_glintfx() {
    build_dir="$1"
    prefix="$2"
    cmake --build "$build_dir"
    cmake --install "$build_dir" --prefix "$prefix"
}

configure_consumer_with_architecture_hint() {
    package_src="$1"
    consumer_build="$2"
    prefix="$3"
    cxx="$4"
    cmake -S "$package_src" -B "$consumer_build" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_COMPILER="$cxx" \
        -DCMAKE_PREFIX_PATH="$prefix" \
        -DCMAKE_LIBRARY_ARCHITECTURE="$LIBDIR_ARCH"
}

configure_consumer_without_architecture_hint() {
    package_src="$1"
    consumer_build="$2"
    prefix="$3"
    cxx="$4"
    cmake -S "$package_src" -B "$consumer_build" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_COMPILER="$cxx" \
        -DCMAKE_PREFIX_PATH="$prefix"
}

build_and_run_consumer() {
    consumer_build="$1"
    cmake --build "$consumer_build"
    binary="$consumer_build/consumer"
    [ -x "$binary" ] || fail "consumer binary not found after build: $binary"
    echo "check_install_packager_layout.sh: running $binary"
    "$binary"
}

# Reads back what CMake's OWN compiler-ABI detection already decided for
# this compiler (Modules/CMakeDetermineCompilerABI.cmake writes it into
# CMakeCXXCompiler.cmake) - ground truth, not a reimplementation of
# CMAKE_LIBRARY_ARCHITECTURE_REGEX. Empty output means this compiler
# reports no arch-suffixed implicit library directory, i.e. the
# automatic lib/<arch> search this test proves can never trigger here.
detect_native_library_architecture() {
    build_dir="$1"
    compiler_info_file="$(find "$build_dir/CMakeFiles" -maxdepth 2 -name 'CMakeCXXCompiler.cmake' 2>/dev/null | head -n1)"
    [ -n "$compiler_info_file" ] || fail "CMakeCXXCompiler.cmake not found under $build_dir/CMakeFiles (compiler ABI detection did not run)"
    sed -n 's/^set(CMAKE_CXX_LIBRARY_ARCHITECTURE "\(.*\)")$/\1/p' "$compiler_info_file"
}

declare_zero_flag_limitation() {
    cxx="$1"
    build_dir="$2"
    echo "check_install_packager_layout.sh: zero-flag find_package path NOT exercised for '$cxx' on this image - CMAKE_CXX_LIBRARY_ARCHITECTURE is empty in $build_dir (compiler ABI detection found no arch-suffixed implicit library directory). CMake's automatic lib/<arch> search (Modules/CMakeParseLibraryArchitecture.cmake) never activates for this toolchain; that is a property of the compiler/distro packaging (Fedora/Arch/CachyOS gcc report no multiarch implicit dir), not a defect in glintfx or in glintfx-config.cmake.in. Proven instead by the hinted scenario above, which reproduces exactly what CMAKE_LIBRARY_ARCHITECTURE is set to automatically on a genuine Debian/Ubuntu multiarch toolchain."
}

# Orchestrates the zero-flag scenario in its own build/prefix, separate
# from the hinted one above (CMAKE_INSTALL_LIBDIR is a cache variable
# fixed at glintfx's own configure time, so it needs its own build dir).
# Declares the limitation instead of running when this image's compiler
# cannot autopopulate CMAKE_LIBRARY_ARCHITECTURE (GODS_LAWS.md L-27:
# honest negative result over a gate that overclaims).
run_native_zero_flag_scenario() {
    glintfx_src="$1"
    package_src="$2"
    cxx="$3"
    scratch="$4"

    hinted_build="$scratch/glintfx-build-hinted"
    native_arch="$(detect_native_library_architecture "$hinted_build")"
    if [ -z "$native_arch" ]; then
        declare_zero_flag_limitation "$cxx" "$hinted_build"
        return 0
    fi

    native_build="$scratch/glintfx-build-native"
    native_prefix="$scratch/prefix-native"
    native_consumer_build="$scratch/consumer-build-native"

    configure_glintfx_with_packager_layout "$glintfx_src" "$native_build" "$cxx" "lib/${native_arch}"
    build_and_install_glintfx "$native_build" "$native_prefix"
    configure_consumer_without_architecture_hint "$package_src" "$native_consumer_build" "$native_prefix" "$cxx"
    build_and_run_consumer "$native_consumer_build"

    echo "ok: find_package(glintfx) resolves a non-default multiarch-style install layout with NO hint flag at all (zero-flag path), using this toolchain's own native library architecture ($native_arch) - the scenario a real Debian/Ubuntu packager or end user experiences after installing the -dev package and calling find_package(glintfx), with no -D flag involved."
}

main() {
    require_args "$@"
    glintfx_src="$1"
    package_src="$2"
    cxx="$3"

    scratch="$(make_scratch_workdir)"
    trap 'rm -rf "$scratch"' EXIT

    hinted_build="$scratch/glintfx-build-hinted"
    hinted_prefix="$scratch/prefix-hinted"
    hinted_consumer_build="$scratch/consumer-build-hinted"

    configure_glintfx_with_packager_layout "$glintfx_src" "$hinted_build" "$cxx" "$NONDEFAULT_LIBDIR"
    build_and_install_glintfx "$hinted_build" "$hinted_prefix"
    configure_consumer_with_architecture_hint "$package_src" "$hinted_consumer_build" "$hinted_prefix" "$cxx"
    build_and_run_consumer "$hinted_consumer_build"
    echo "ok: find_package(glintfx) resolves a non-default multiarch-style install layout when the consumer supplies the architecture hint (CMAKE_INSTALL_LIBDIR=$NONDEFAULT_LIBDIR, CMAKE_INSTALL_INCLUDEDIR=$NONDEFAULT_INCLUDEDIR)."

    run_native_zero_flag_scenario "$glintfx_src" "$package_src" "$cxx" "$scratch"
}

main "$@"
