#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
# check_install_includedir.sh - proves that a non-default
# CMAKE_INSTALL_INCLUDEDIR is honored by the EXPORTED CMake target set
# (glintfxTargets.cmake), not just by the files that land on disk
# (FIX-CONSUMO-2, achado QA-1).
#
# Before this test existed, GlintfxLibrary.cmake's
# $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}> (the real fix for
# achado A6 of the earlier onda) was masked by a redundant
# `INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}` clause on the
# install(TARGETS ...) rule (GlintfxInstall.cmake): both add the SAME
# path to the exported target's INTERFACE_INCLUDE_DIRECTORIES, so a
# regression of the first (e.g. reverting it to a hardcoded literal)
# went uncaught as long as the second kept compensating. This script
# greps the installed glintfxTargets.cmake directly, instead of building
# a full consumer (check_consume.sh already does that for the default
# layout), because that is the narrowest place the regression can hide.
#
# Usage: check_install_includedir.sh <glintfx-source-dir> <cxx-compiler>
#
# Each function below does one thing (GODS_LAWS.md L-17).

set -eu

NONDEFAULT_INCLUDEDIR="include/glintfx-nondefault-includedir"

fail() {
    echo "check_install_includedir.sh: $1" >&2
    exit 1
}

require_args() {
    [ "$#" -eq 2 ] || fail "usage: check_install_includedir.sh <glintfx-source-dir> <cxx-compiler>"
    [ -d "$1" ] || fail "glintfx source dir not found: $1"
}

make_scratch_workdir() {
    mktemp -d "${TMPDIR:-/tmp}/glintfx-includedir-XXXXXX"
}

configure_glintfx_with_nondefault_includedir() {
    glintfx_src="$1"
    build_dir="$2"
    cxx="$3"
    cmake -S "$glintfx_src" -B "$build_dir" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_COMPILER="$cxx" \
        -DCMAKE_INSTALL_INCLUDEDIR="$NONDEFAULT_INCLUDEDIR" \
        -DGLINTFX_BUILD_TESTS=OFF
}

build_and_install_glintfx() {
    build_dir="$1"
    prefix="$2"
    cmake --build "$build_dir"
    cmake --install "$build_dir" --prefix "$prefix"
}

assert_exported_includedir_is_nondefault() {
    prefix="$1"
    targets_file="$(find "$prefix" -name glintfxTargets.cmake 2>/dev/null | head -n1)"
    [ -n "$targets_file" ] || fail "glintfxTargets.cmake not found under $prefix"

    expected="\${_IMPORT_PREFIX}/${NONDEFAULT_INCLUDEDIR}"
    grep -F "$expected" "$targets_file" >/dev/null \
        || fail "INTERFACE_INCLUDE_DIRECTORIES in $targets_file does not contain the non-default include dir ($expected): the dynamic INSTALL_INTERFACE regressed"
    echo "check_install_includedir.sh: exported INTERFACE_INCLUDE_DIRECTORIES honors CMAKE_INSTALL_INCLUDEDIR=$NONDEFAULT_INCLUDEDIR"
}

main() {
    require_args "$@"
    glintfx_src="$1"
    cxx="$2"

    scratch="$(make_scratch_workdir)"
    trap 'rm -rf "$scratch"' EXIT

    build_dir="$scratch/build"
    prefix="$scratch/prefix"

    configure_glintfx_with_nondefault_includedir "$glintfx_src" "$build_dir" "$cxx"
    build_and_install_glintfx "$build_dir" "$prefix"
    assert_exported_includedir_is_nondefault "$prefix"

    echo "ok: CMAKE_INSTALL_INCLUDEDIR override propagates to the exported CMake package."
}

main "$@"
