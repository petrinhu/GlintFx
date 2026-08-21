#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
# check_embed.sh - proves glintfx works consumed via add_subdirectory
# (FIX-CONSUMO, achado A7). FetchContent behaves the same way for this
# purpose: it populates a source tree and then calls add_subdirectory on
# it, so this script proves both consumption modes at once.
#
# Three things are asserted, not just "it configures":
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

    echo "ok: glintfx consumed successfully via add_subdirectory (embedding)."
}

main "$@"
