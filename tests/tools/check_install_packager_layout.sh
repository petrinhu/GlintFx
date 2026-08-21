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
# This script therefore configures the consumer the way a REAL
# multiarch packaging setup does: with CMAKE_LIBRARY_ARCHITECTURE set to
# the same arch suffix used at install time (a native Debian/Ubuntu
# toolchain would set this automatically; explicit here so the proof is
# deterministic across every image in the CI matrix, including the ones
# whose own compiler does not report a multiarch path).
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
    cmake -S "$glintfx_src" -B "$build_dir" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_COMPILER="$cxx" \
        -DCMAKE_INSTALL_LIBDIR="$NONDEFAULT_LIBDIR" \
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

build_and_run_consumer() {
    consumer_build="$1"
    cmake --build "$consumer_build"
    binary="$consumer_build/consumer"
    [ -x "$binary" ] || fail "consumer binary not found after build: $binary"
    echo "check_install_packager_layout.sh: running $binary"
    "$binary"
}

main() {
    require_args "$@"
    glintfx_src="$1"
    package_src="$2"
    cxx="$3"

    scratch="$(make_scratch_workdir)"
    trap 'rm -rf "$scratch"' EXIT

    glintfx_build="$scratch/glintfx-build"
    prefix="$scratch/prefix"
    consumer_build="$scratch/consumer-build"

    configure_glintfx_with_packager_layout "$glintfx_src" "$glintfx_build" "$cxx"
    build_and_install_glintfx "$glintfx_build" "$prefix"
    configure_consumer_with_architecture_hint "$package_src" "$consumer_build" "$prefix" "$cxx"
    build_and_run_consumer "$consumer_build"

    echo "ok: find_package(glintfx) resolves a non-default multiarch-style install layout (CMAKE_INSTALL_LIBDIR=$NONDEFAULT_LIBDIR, CMAKE_INSTALL_INCLUDEDIR=$NONDEFAULT_INCLUDEDIR)."
}

main "$@"
