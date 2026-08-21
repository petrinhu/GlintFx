#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
# check_output_name.sh - proves the OUTPUT_NAME contract that
# cmake/GlintfxLibrary.cmake sets (FIX-CONSUMO-3, achado QA-2-2 da
# revisao adversarial): the COMPILED ARTIFACT basename must stay
# libglintfx.so*/libglintfx.a, never the internal CMake target name
# glintfx_library (see src/CMakeLists.txt), because a consumer that
# links with a bare `-lglintfx` - pkg-config, a hand-written Makefile,
# or a distro's %files/.spec/PKGBUILD entry - never goes through the
# glintfx::glintfx CMake target that every OTHER gate in this suite
# (consume_test, embed_test, install_packager_layout_test,
# no_target_collision_test) exercises. None of those gates would notice
# OUTPUT_NAME regressing; this one does, by installing the build and
# raw-linking against it with no CMake and no find_package at all.
#
# Usage: check_output_name.sh <glintfx-build-dir> <raw-consumer-source-file> <cxx-compiler>
#
# Each function below does one thing (GODS_LAWS.md L-17).

set -eu

fail() {
    echo "check_output_name.sh: $1" >&2
    exit 1
}

require_args() {
    [ "$#" -eq 3 ] || fail "usage: check_output_name.sh <glintfx-build-dir> <raw-consumer-source-file> <cxx-compiler>"
    [ -d "$1" ] || fail "glintfx build dir not found: $1"
    [ -f "$2" ] || fail "raw consumer source file not found: $2"
}

make_scratch_workdir() {
    mktemp -d "${TMPDIR:-/tmp}/glintfx-outputname-XXXXXX"
}

install_into_prefix() {
    build_dir="$1"
    prefix="$2"
    echo "check_output_name.sh: cmake --install $build_dir --prefix $prefix"
    cmake --install "$build_dir" --prefix "$prefix"
}

find_installed_artifact_file() {
    prefix="$1"
    find "$prefix" -type f \( -name 'libglintfx.so.*' -o -name 'libglintfx.a' \) | head -n1
}

assert_artifact_found_matching_contract() {
    prefix="$1"
    artifact="$2"
    [ -n "$artifact" ] || fail "no libglintfx.so.* or libglintfx.a found under $prefix - OUTPUT_NAME regressed (the artifact was installed under some OTHER basename, e.g. libglintfx_library.*)"
    [ -f "$artifact" ] || fail "artifact path resolved but is not a regular file: $artifact"
}

compile_raw_consumer() {
    consumer_src="$1"
    includedir="$2"
    libdir="$3"
    cxx="$4"
    output_bin="$5"
    "$cxx" -std=c++23 -I "$includedir" "$consumer_src" \
        -L "$libdir" -Wl,-rpath,"$libdir" -lglintfx \
        -o "$output_bin"
}

run_raw_consumer() {
    binary="$1"
    [ -x "$binary" ] || fail "raw-linked consumer binary not found after compile: $binary"
    echo "check_output_name.sh: running $binary"
    "$binary"
}

main() {
    require_args "$@"
    build_dir="$1"
    consumer_src="$2"
    cxx="$3"

    scratch="$(make_scratch_workdir)"
    trap 'rm -rf "$scratch"' EXIT

    prefix="$scratch/prefix"
    install_into_prefix "$build_dir" "$prefix"

    artifact="$(find_installed_artifact_file "$prefix")"
    assert_artifact_found_matching_contract "$prefix" "$artifact"
    echo "check_output_name.sh: installed artifact basename is $(basename "$artifact")"

    includedir="$prefix/include"
    libdir="$(dirname "$artifact")"
    binary="$scratch/raw_link_consumer"

    compile_raw_consumer "$consumer_src" "$includedir" "$libdir" "$cxx" "$binary"
    run_raw_consumer "$binary"

    echo "ok: OUTPUT_NAME contract holds - a bare -lglintfx raw link (no CMake, no find_package, no glintfx::glintfx target) compiles, links and runs against the installed artifact."
}

main "$@"
