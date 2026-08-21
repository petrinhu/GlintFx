#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
# check_no_target_collision.sh - proves an embedding consumer can name
# its OWN CMake target `glintfx` (the bare name glintfx's own internal
# library target used to have) without colliding with it (FIX-CONSUMO-2,
# achado QA-3).
#
# Two things are asserted, not just "it configures":
#   1. configure resolves: add_library(glintfx ...) in the fixture
#      project (tests/embed_name_collision/) does not hit
#      CMake Error ... policy CMP0002, which it did before the rename.
#   2. build/run works: the REAL glintfx::glintfx alias still links and
#      runs correctly, proving the rename did not silently break linkage
#      in the tree where the collision used to happen.
#
# Usage: check_no_target_collision.sh <glintfx-source-dir> <fixture-src-dir> <cxx-compiler>
#
# Each function below does one thing (GODS_LAWS.md L-17).

set -eu

fail() {
    echo "check_no_target_collision.sh: $1" >&2
    exit 1
}

require_args() {
    [ "$#" -eq 3 ] || fail "usage: check_no_target_collision.sh <glintfx-source-dir> <fixture-src-dir> <cxx-compiler>"
    [ -d "$1" ] || fail "glintfx source dir not found: $1"
    [ -d "$2" ] || fail "fixture source dir not found: $2"
}

make_scratch_workdir() {
    mktemp -d "${TMPDIR:-/tmp}/glintfx-collision-XXXXXX"
}

configure_fixture() {
    fixture_src="$1"
    fixture_build="$2"
    glintfx_src="$3"
    cxx="$4"
    cmake -S "$fixture_src" -B "$fixture_build" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_COMPILER="$cxx" \
        -DGLINTFX_SOURCE_DIR="$glintfx_src"
}

build_fixture() {
    cmake --build "$1"
}

run_fixture() {
    fixture_build="$1"
    binary="$fixture_build/collision_consumer"
    [ -x "$binary" ] || fail "collision_consumer binary not found after build: $binary"
    echo "check_no_target_collision.sh: running $binary"
    "$binary"
}

main() {
    require_args "$@"
    glintfx_src="$1"
    fixture_src="$2"
    cxx="$3"

    scratch="$(make_scratch_workdir)"
    trap 'rm -rf "$scratch"' EXIT

    fixture_build="$scratch/fixture-build"

    configure_fixture "$fixture_src" "$fixture_build" "$glintfx_src" "$cxx"
    build_fixture "$fixture_build"
    run_fixture "$fixture_build"

    echo "ok: a consumer target named glintfx does not collide with glintfx's own internal target."
}

main "$@"
