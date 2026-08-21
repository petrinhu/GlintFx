#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
# check_embed.sh - proves glintfx works consumed via add_subdirectory
# (FIX-CONSUMO, achado A7). FetchContent behaves the same way for this
# purpose: it populates a source tree and then calls add_subdirectory on
# it, so this script proves both consumption modes at once.
#
# Four things are asserted, not just "it configures":
#   1. configure/build/run of tests/embed/ succeeds.
#   2. glintfx's generated headers (export.hpp, version_macros.hpp) land
#      scoped under the embed build's own subdirectory, not spilled into
#      the consumer's own top-level build dir (that is the collision the
#      bug produced before the fix).
#   3. glintfx's own install() rules do NOT run for the consumer's
#      top-level `install` target when embedded (GLINTFX_INSTALL default
#      is PROJECT_IS_TOP_LEVEL): a consumer that embeds glintfx and later
#      installs its own binary must not have glintfx headers leak into
#      its install prefix by surprise.
#   4. A consumer that opts INTO GLINTFX_INSTALL=ON (the escape hatch
#      GlintfxOptions.cmake documents for "a consumer that DOES want
#      glintfx installed alongside its own artifacts") gets glintfx's
#      headers and CMake package for real. Without this pass,
#      glintfx_install_public_headers() and
#      glintfx_configure_package_config_file() (GlintfxInstall.cmake)
#      never run under this script (GLINTFX_INSTALL default is OFF when
#      embedded), so a regression in either could sit uncaught forever
#      (FIX-CONSUMO-2, achado QA-2).
#
# Usage: check_embed.sh <glintfx-source-dir> <embed-src-dir> <cxx-compiler>
#
# Each function below does one thing (GODS_LAWS.md L-17).

set -eu

fail() {
    echo "check_embed.sh: $1" >&2
    exit 1
}

require_args() {
    [ "$#" -eq 3 ] || fail "usage: check_embed.sh <glintfx-source-dir> <embed-src-dir> <cxx-compiler>"
    [ -d "$1" ] || fail "glintfx source dir not found: $1"
    [ -d "$2" ] || fail "embed source dir not found: $2"
}

make_scratch_workdir() {
    mktemp -d "${TMPDIR:-/tmp}/glintfx-embed-XXXXXX"
}

configure_embed() {
    embed_src="$1"
    embed_build="$2"
    glintfx_src="$3"
    cxx="$4"
    cmake -S "$embed_src" -B "$embed_build" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_COMPILER="$cxx" \
        -DGLINTFX_SOURCE_DIR="$glintfx_src"
}

# Same configure as above, plus the opt-in flag a consumer sets to get
# glintfx installed alongside its own artifacts (GlintfxOptions.cmake).
# Kept as its own function, not a parameterized variant of
# configure_embed(), because the two configure different consumer intent,
# not the same intent with a knob (GODS_LAWS.md L-17: a name that needs
# "and"/a conditional to stay honest is two functions).
configure_embed_with_install_opt_in() {
    embed_src="$1"
    embed_build="$2"
    glintfx_src="$3"
    cxx="$4"
    cmake -S "$embed_src" -B "$embed_build" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_COMPILER="$cxx" \
        -DGLINTFX_SOURCE_DIR="$glintfx_src" \
        -DGLINTFX_INSTALL=ON
}

build_embed() {
    cmake --build "$1"
}

run_embed() {
    embed_build="$1"
    binary="$embed_build/embed_consumer"
    [ -x "$binary" ] || fail "embed_consumer binary not found after build: $binary"
    echo "check_embed.sh: running $binary"
    "$binary"
}

assert_generated_headers_scoped() {
    embed_build="$1"
    scoped_header="$embed_build/glintfx-build/generated/include/glintfx/version_macros.hpp"
    leaked_header="$embed_build/generated/include/glintfx/version_macros.hpp"

    [ -f "$scoped_header" ] || fail "generated header missing where the embedded subdirectory should have put it: $scoped_header"
    [ -f "$leaked_header" ] && fail "generated header leaked into the consumer's own top-level build dir: $leaked_header"
    echo "check_embed.sh: generated headers scoped correctly under glintfx-build/"
}

assert_install_does_not_leak_headers() {
    embed_build="$1"
    scratch_prefix="$2"
    cmake --install "$embed_build" --prefix "$scratch_prefix" >/dev/null 2>&1 || true
    if [ -d "$scratch_prefix/include/glintfx" ]; then
        fail "glintfx headers were installed by an embedded consumer's install target (GLINTFX_INSTALL guard not honored)"
    fi
    echo "check_embed.sh: embedded install did not leak glintfx headers (GLINTFX_INSTALL default respected)"
}

# Opposite assertion of assert_install_does_not_leak_headers() above: here
# the consumer explicitly opted in (GLINTFX_INSTALL=ON), so the headers
# and the CMake package MUST appear. This is the only path that exercises
# glintfx_install_public_headers() and
# glintfx_configure_package_config_file() (GlintfxInstall.cmake) under
# this script (FIX-CONSUMO-2, achado QA-2).
assert_install_opt_in_installs_glintfx() {
    embed_build="$1"
    scratch_prefix="$2"
    cmake --install "$embed_build" --prefix "$scratch_prefix"
    header="$scratch_prefix/include/glintfx/core/version.hpp"
    [ -f "$header" ] || fail "glintfx header missing after GLINTFX_INSTALL=ON opt-in install: $header"
    find "$scratch_prefix" -name glintfxConfig.cmake 2>/dev/null | grep -q . \
        || fail "glintfx CMake package (glintfxConfig.cmake) missing after GLINTFX_INSTALL=ON opt-in install under $scratch_prefix"
    echo "check_embed.sh: embedded install with GLINTFX_INSTALL=ON opt-in installed glintfx headers and CMake package"
}

# Orchestrates the opt-in pass in its own scratch build/prefix, separate
# from the default pass above (GLINTFX_INSTALL is a cache variable fixed
# at configure time, so it needs its own build directory).
check_install_opt_in() {
    embed_src="$1"
    glintfx_src="$2"
    cxx="$3"
    scratch="$4"

    opt_in_build="$scratch/embed-build-install-opt-in"
    opt_in_prefix="$scratch/prefix-install-opt-in"

    configure_embed_with_install_opt_in "$embed_src" "$opt_in_build" "$glintfx_src" "$cxx"
    build_embed "$opt_in_build"
    assert_install_opt_in_installs_glintfx "$opt_in_build" "$opt_in_prefix"
}

main() {
    require_args "$@"
    glintfx_src="$1"
    embed_src="$2"
    cxx="$3"

    scratch="$(make_scratch_workdir)"
    trap 'rm -rf "$scratch"' EXIT

    embed_build="$scratch/embed-build"
    scratch_prefix="$scratch/prefix"

    configure_embed "$embed_src" "$embed_build" "$glintfx_src" "$cxx"
    build_embed "$embed_build"
    run_embed "$embed_build"
    assert_generated_headers_scoped "$embed_build"
    assert_install_does_not_leak_headers "$embed_build" "$scratch_prefix"

    check_install_opt_in "$embed_src" "$glintfx_src" "$cxx" "$scratch"

    echo "ok: glintfx consumed successfully via add_subdirectory (embedding)."
}

main "$@"
