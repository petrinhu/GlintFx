#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
# check_pkgconfig.sh - proves the installed glintfx.pc (PKG-DIST) is a
# working pkg-config module: `pkg-config --cflags --libs glintfx` alone
# - no CMake, no find_package(glintfx), no hand-written -I/-L/-l -
# resolves and links tests/raw_link/main.cpp against the installed
# package, in THREE scenarios that isolate what could silently regress:
#
#   1. "default layout": the ordinary single-arch install
#      (CMAKE_INSTALL_LIBDIR unset, GNUInstallDirs default), shared
#      build. Proves the module resolves and the RELOCATABLE prefix
#      (glintfx.pc's own `${pcfiledir}`-relative prefix, GODS_LAWS.md
#      LEI ZERO: an unknown consumer's install prefix is never this
#      machine's) points at the SAME scratch prefix the file was
#      installed into, not at whatever CMAKE_INSTALL_PREFIX happened to
#      be at configure time.
#   2. "multiarch layout": CMAKE_INSTALL_LIBDIR set to a
#      Debian-multiarch-style path (lib/x86_64-linux-gnu), same
#      architecture check_install_packager_layout.sh already proves for
#      find_package(glintfx) - glintfx.pc lands under
#      lib/x86_64-linux-gnu/pkgconfig, three directory levels below the
#      prefix instead of two, and the relocatable prefix expression has
#      to get the level count right or this scenario alone catches it
#      (the default-layout scenario above cannot: with a two-segment
#      pkgconfig dir, an off-by-one in the level count and a
#      DIFFERENT-but-still-wrong absolute path can coincide by
#      accident on a single-segment layout).
#   3. "static": BUILD_SHARED_LIBS=OFF, consumer built with
#      `pkg-config --cflags --libs --static glintfx`. Proves the
#      Libs.private field (-lwayland-client on Linux) is actually
#      present and actually needed: glintfx_library links
#      wayland-client PRIVATE (cmake/GlintfxWaylandProtocols.cmake), and
#      CMake's own static-propagation of that PRIVATE dependency into
#      glintfx::glintfx's INTERFACE_LINK_LIBRARIES only helps a
#      find_package(glintfx)-based consumer - pkg-config has no CMake
#      target graph to read, so a bare `pkg-config --libs glintfx`
#      (non-static line) never carries it; without Libs.private, static
#      linking against the installed archive fails with undefined
#      references to wl_* symbols.
#
# Usage: check_pkgconfig.sh <glintfx-source-dir> <raw-consumer-source-file> <cxx-compiler>
#
# Each function below does one thing (GODS_LAWS.md L-17).

set -eu

readonly LIBDIR_ARCH="x86_64-linux-gnu"
readonly MULTIARCH_LIBDIR="lib/${LIBDIR_ARCH}"
readonly DEFAULT_LIBDIR="lib"

fail() {
    echo "check_pkgconfig.sh: $1" >&2
    exit 1
}

require_args() {
    [ "$#" -eq 3 ] || fail "usage: check_pkgconfig.sh <glintfx-source-dir> <raw-consumer-source-file> <cxx-compiler>"
    [ -d "$1" ] || fail "glintfx source dir not found: $1"
    [ -f "$2" ] || fail "raw consumer source file not found: $2"
}

require_pkg_config() {
    command -v pkg-config >/dev/null 2>&1 || fail "pkg-config not found on PATH"
}

make_scratch_workdir() {
    mktemp -d "${TMPDIR:-/tmp}/glintfx-pkgconfig-XXXXXX"
}

configure_glintfx() {
    glintfx_src="$1"
    build_dir="$2"
    cxx="$3"
    libdir="$4"
    shared_libs="$5"
    cmake -S "$glintfx_src" -B "$build_dir" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_COMPILER="$cxx" \
        -DCMAKE_INSTALL_LIBDIR="$libdir" \
        -DBUILD_SHARED_LIBS="$shared_libs" \
        -DGLINTFX_BUILD_TESTS=OFF
}

build_and_install_glintfx() {
    build_dir="$1"
    prefix="$2"
    cmake --build "$build_dir"
    cmake --install "$build_dir" --prefix "$prefix"
}

pkgconfig_dir_for() {
    prefix="$1"
    libdir="$2"
    echo "${prefix}/${libdir}/pkgconfig"
}

# Fails with pkg-config's OWN diagnostic when the module cannot be
# found (--print-errors) - deliberately no hand-written
# [ -f glintfx.pc ] precondition ahead of this: this line and its
# failure message ARE the RED this gate showed before glintfx.pc and
# its install wiring existed ("Package 'glintfx' not found"), and
# reaching pkg-config itself (not a shell path guess of ours) is what
# proves the search path was actually consulted.
assert_pkgconfig_finds_glintfx() {
    pkgconfig_dir="$1"
    PKG_CONFIG_PATH="$pkgconfig_dir" pkg-config --exists glintfx \
        || fail "$(PKG_CONFIG_PATH="$pkgconfig_dir" pkg-config --print-errors --exists glintfx 2>&1 || true)"
}

assert_version_has_four_components() {
    pkgconfig_dir="$1"
    version="$(PKG_CONFIG_PATH="$pkgconfig_dir" pkg-config --modversion glintfx)"
    case "$version" in
        *.*.*.*) : ;;
        *) fail "glintfx.pc Version is '${version}', expected four dot-separated components (GODS_LAWS.md L-26)" ;;
    esac
    echo "check_pkgconfig.sh: glintfx.pc Version is ${version}"
}

# Proves the prefix inside glintfx.pc actually resolved to THIS
# scratch prefix, not to wherever CMAKE_INSTALL_PREFIX pointed at
# configure time - the whole point of a relocatable ${pcfiledir}-based
# prefix (see the file-level comment, scenario 1/2).
assert_libdir_points_inside_prefix() {
    pkgconfig_dir="$1"
    prefix="$2"
    reported_libdir="$(PKG_CONFIG_PATH="$pkgconfig_dir" pkg-config --variable=libdir glintfx)"
    case "$reported_libdir" in
        "${prefix}"/*) : ;;
        *) fail "glintfx.pc libdir is '${reported_libdir}', expected it under the scratch prefix '${prefix}' - the relocatable prefix expression did not follow the moved install tree" ;;
    esac
}

compile_and_run_dynamic_consumer() {
    pkgconfig_dir="$1"
    consumer_src="$2"
    cxx="$3"
    libdir_abs="$4"
    output_bin="$5"
    cflags="$(PKG_CONFIG_PATH="$pkgconfig_dir" pkg-config --cflags glintfx)"
    libs="$(PKG_CONFIG_PATH="$pkgconfig_dir" pkg-config --libs glintfx)"
    # Word-splitting of $cflags/$libs is intentional: pkg-config's own
    # output is a space-separated argument list, exactly what a shell
    # build recipe (Makefile, Autotools, a packager's %build) passes
    # to the compiler unquoted.
    "$cxx" -std=c++23 ${cflags} "$consumer_src" ${libs} -Wl,-rpath,"$libdir_abs" -o "$output_bin"
    [ -x "$output_bin" ] || fail "dynamic consumer binary not found after compile: $output_bin"
    echo "check_pkgconfig.sh: running $output_bin (dynamic, pkg-config --cflags --libs glintfx)"
    "$output_bin"
}

assert_static_libs_mention_wayland_client() {
    libs_static="$1"
    case "$libs_static" in
        *-lwayland-client*) : ;;
        *) fail "pkg-config --libs --static glintfx did not mention -lwayland-client - Libs.private regressed: '${libs_static}'" ;;
    esac
}

compile_and_run_static_consumer() {
    pkgconfig_dir="$1"
    consumer_src="$2"
    cxx="$3"
    output_bin="$4"
    cflags="$(PKG_CONFIG_PATH="$pkgconfig_dir" pkg-config --cflags --static glintfx)"
    libs="$(PKG_CONFIG_PATH="$pkgconfig_dir" pkg-config --libs --static glintfx)"
    assert_static_libs_mention_wayland_client "$libs"
    # shellcheck-style word-splitting, same reasoning as the dynamic
    # consumer above - and here it is load-bearing: --static is what
    # makes Libs.private (-lwayland-client) appear in $libs at all.
    "$cxx" -std=c++23 ${cflags} "$consumer_src" ${libs} -o "$output_bin"
    [ -x "$output_bin" ] || fail "static consumer binary not found after compile: $output_bin"
    echo "check_pkgconfig.sh: running $output_bin (static, pkg-config --cflags --libs --static glintfx)"
    "$output_bin"
}

# Scenario 1: default single-arch layout, shared build.
run_default_layout_scenario() {
    glintfx_src="$1"
    consumer_src="$2"
    cxx="$3"
    scratch="$4"

    build_dir="${scratch}/build-default"
    prefix="${scratch}/prefix-default"
    configure_glintfx "$glintfx_src" "$build_dir" "$cxx" "$DEFAULT_LIBDIR" ON
    build_and_install_glintfx "$build_dir" "$prefix"

    pkgconfig_dir="$(pkgconfig_dir_for "$prefix" "$DEFAULT_LIBDIR")"
    assert_pkgconfig_finds_glintfx "$pkgconfig_dir"
    assert_version_has_four_components "$pkgconfig_dir"
    assert_libdir_points_inside_prefix "$pkgconfig_dir" "$prefix"

    binary="${scratch}/consumer-default"
    compile_and_run_dynamic_consumer "$pkgconfig_dir" "$consumer_src" "$cxx" "${prefix}/${DEFAULT_LIBDIR}" "$binary"
    echo "ok: default-layout scenario - glintfx.pc resolves and links via pkg-config alone."
}

# Scenario 2: Debian-multiarch-style layout, shared build - the level
# count in the relocatable prefix expression has three directory
# segments to climb here, not two (see the file-level comment).
run_multiarch_layout_scenario() {
    glintfx_src="$1"
    consumer_src="$2"
    cxx="$3"
    scratch="$4"

    build_dir="${scratch}/build-multiarch"
    prefix="${scratch}/prefix-multiarch"
    configure_glintfx "$glintfx_src" "$build_dir" "$cxx" "$MULTIARCH_LIBDIR" ON
    build_and_install_glintfx "$build_dir" "$prefix"

    pkgconfig_dir="$(pkgconfig_dir_for "$prefix" "$MULTIARCH_LIBDIR")"
    assert_pkgconfig_finds_glintfx "$pkgconfig_dir"
    assert_libdir_points_inside_prefix "$pkgconfig_dir" "$prefix"

    binary="${scratch}/consumer-multiarch"
    compile_and_run_dynamic_consumer "$pkgconfig_dir" "$consumer_src" "$cxx" "${prefix}/${MULTIARCH_LIBDIR}" "$binary"
    echo "ok: multiarch-layout scenario - relocatable prefix survives a three-segment pkgconfig install dir."
}

# Scenario 3: static build - proves Libs.private, not just Libs.
run_static_scenario() {
    glintfx_src="$1"
    consumer_src="$2"
    cxx="$3"
    scratch="$4"

    build_dir="${scratch}/build-static"
    prefix="${scratch}/prefix-static"
    configure_glintfx "$glintfx_src" "$build_dir" "$cxx" "$DEFAULT_LIBDIR" OFF
    build_and_install_glintfx "$build_dir" "$prefix"

    pkgconfig_dir="$(pkgconfig_dir_for "$prefix" "$DEFAULT_LIBDIR")"
    assert_pkgconfig_finds_glintfx "$pkgconfig_dir"

    binary="${scratch}/consumer-static"
    compile_and_run_static_consumer "$pkgconfig_dir" "$consumer_src" "$cxx" "$binary"
    echo "ok: static scenario - pkg-config --libs --static glintfx carries Libs.private (-lwayland-client) and the raw link succeeds."
}

main() {
    require_args "$@"
    require_pkg_config
    glintfx_src="$1"
    consumer_src="$2"
    cxx="$3"

    scratch="$(make_scratch_workdir)"
    trap 'rm -rf "$scratch"' EXIT

    run_default_layout_scenario "$glintfx_src" "$consumer_src" "$cxx" "$scratch"
    run_multiarch_layout_scenario "$glintfx_src" "$consumer_src" "$cxx" "$scratch"
    run_static_scenario "$glintfx_src" "$consumer_src" "$cxx" "$scratch"

    echo "ok: glintfx.pc resolves via pkg-config alone (no CMake, no find_package) in default layout, Debian-multiarch layout and static mode with Libs.private."
}

main "$@"
