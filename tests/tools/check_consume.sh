#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
# check_consume.sh - proves the INSTALLED package, not the in-tree build
# (FIX-CONSUMO, achados A2/A3/A4/A6 de auditoria-premissa.md).
#
# Installs the already-built glintfx (whatever BUILD_SHARED_LIBS mode the
# caller configured) into a throwaway prefix, then configures, builds and
# runs the standalone tests/package/ consumer against that prefix via
# find_package(glintfx). This is the artifact an unknown external
# consumer actually gets; the in-tree ctest cases never exercise it.
#
# Usage: check_consume.sh <glintfx-build-dir> <package-src-dir> <cxx-compiler>
#
# Each function below does one thing (GODS_LAWS.md L-17).

set -eu

fail() {
    echo "check_consume.sh: $1" >&2
    exit 1
}

require_args() {
    [ "$#" -eq 3 ] || fail "usage: check_consume.sh <glintfx-build-dir> <package-src-dir> <cxx-compiler>"
    [ -d "$1" ] || fail "glintfx build dir not found: $1"
    [ -d "$2" ] || fail "package source dir not found: $2"
}

make_scratch_prefix() {
    mktemp -d "${TMPDIR:-/tmp}/glintfx-consume-XXXXXX"
}

install_into_prefix() {
    build_dir="$1"
    prefix="$2"
    echo "check_consume.sh: cmake --install $build_dir --prefix $prefix"
    cmake --install "$build_dir" --prefix "$prefix"
}

configure_consumer() {
    package_src="$1"
    consumer_build="$2"
    prefix="$3"
    cxx="$4"
    cmake -S "$package_src" -B "$consumer_build" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_COMPILER="$cxx" \
        -DCMAKE_PREFIX_PATH="$prefix"
}

build_consumer() {
    cmake --build "$1"
}

run_consumer() {
    consumer_build="$1"
    binary="$consumer_build/consumer"
    [ -x "$binary" ] || fail "consumer binary not found after build: $binary"
    echo "check_consume.sh: running $binary"
    "$binary"
}

main() {
    require_args "$@"
    build_dir="$1"
    package_src="$2"
    cxx="$3"

    scratch="$(make_scratch_prefix)"
    trap 'rm -rf "$scratch"' EXIT

    prefix="$scratch/prefix"
    consumer_build="$scratch/consumer-build"

    install_into_prefix "$build_dir" "$prefix"
    configure_consumer "$package_src" "$consumer_build" "$prefix" "$cxx"
    build_consumer "$consumer_build"
    run_consumer "$consumer_build"

    echo "ok: installed package consumed successfully via find_package."
}

main "$@"
