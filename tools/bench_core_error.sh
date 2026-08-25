#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
# bench_core_error.sh - CE-7 of CORE-ERROR (TODO.md): compiles and runs
# tools/bench/core_error_cost.cpp against a REAL, already-configured
# glintfx build, and prints the measured numbers to stdout. This is a
# MEASUREMENT tool, not a pass/fail gate: it has no assertion of its
# own (the only hard check is core_error_cost.cpp's own static_assert,
# which fails the COMPILE if the ABI classification it documents ever
# stopped holding) - reading the printed numbers and deciding what, if
# anything, to do about them is the leader's call, not this script's.
#
# Usage:
#   bench_core_error.sh <include-dir> <generated-include-dir> <library-dir> <cxx-compiler>
#
# <library-dir> must contain the ALREADY-BUILT libglintfx.so (or .a) -
# this script does not configure or build glintfx itself, the same
# division of labor tests/tools/check_output_name.sh and friends use
# (they take a build dir; this one takes the two include dirs plus the
# library dir directly, because -O2 optimization flags matter for a
# benchmark in a way they do not for a plain compile-and-link check,
# and passing them explicitly here keeps that choice visible instead
# of inherited from whatever CMAKE_BUILD_TYPE the build dir happened to
# use).
#
# Each function below does one thing (GODS_LAWS.md L-17).

set -eu

readonly ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
readonly BENCH_DIR="${ROOT_DIR}/tools/bench"
readonly BENCH_MAIN_SRC="${BENCH_DIR}/core_error_cost.cpp"
readonly BENCH_FUNCTIONS_SRC="${BENCH_DIR}/core_error_cost_functions.cpp"

fail() {
    echo "bench_core_error.sh: $1" >&2
    exit 1
}

require_args() {
    [ "$#" -eq 4 ] || fail "usage: bench_core_error.sh <include-dir> <generated-include-dir> <library-dir> <cxx-compiler>"
    [ -d "$1" ] || fail "include dir not found: $1"
    [ -d "$2" ] || fail "generated include dir not found: $2"
    [ -d "$3" ] || fail "library dir not found: $3"
    [ -f "$BENCH_MAIN_SRC" ] || fail "benchmark source not found: $BENCH_MAIN_SRC"
    [ -f "$BENCH_FUNCTIONS_SRC" ] || fail "benchmark functions source not found: $BENCH_FUNCTIONS_SRC"
}

make_scratch_workdir() {
    mktemp -d "${TMPDIR:-/tmp}/glintfx-bench-XXXXXX"
}

# TWO SEPARATE COMPILATION UNITS, compiled here in ONE invocation but
# each parsed and optimized independently by the compiler front end
# (no -flto anywhere in this project) - see
# tools/bench/core_error_cost_functions.hpp's own comment for why a
# single-TU version of this benchmark measured the optimizer instead
# of the ABI. Passing both .cpp files to one `$cxx` invocation still
# compiles each as its own TU before linking; it is NOT equivalent to
# #include-ing one into the other.
compile_and_link() {
    includedir="$1"
    generated_includedir="$2"
    libdir="$3"
    cxx="$4"
    output_bin="$5"
    "$cxx" -std=c++23 -O2 -Wall -Wextra -Werror \
        -I "$includedir" -I "$generated_includedir" \
        "$BENCH_MAIN_SRC" "$BENCH_FUNCTIONS_SRC" \
        -L "$libdir" -Wl,-rpath,"$libdir" -lglintfx \
        -o "$output_bin"
}

main() {
    require_args "$@"
    includedir="$1"
    generated_includedir="$2"
    libdir="$3"
    cxx="$4"

    scratch="$(make_scratch_workdir)"
    trap 'rm -rf "$scratch"' EXIT

    binary="$scratch/core_error_cost"
    echo "bench_core_error.sh: compiling $BENCH_MAIN_SRC + $BENCH_FUNCTIONS_SRC (-O2, split TUs, static_assert is the ABI proof)"
    compile_and_link "$includedir" "$generated_includedir" "$libdir" "$cxx" "$binary"

    echo "bench_core_error.sh: running $binary"
    "$binary"
}

main "$@"
