#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
# check_pkgconfig.sh - proves the installed glintfx.pc (PKG-DIST) is a
# working pkg-config module: `pkg-config --cflags --libs glintfx` alone
# - no CMake, no find_package(glintfx), no hand-written -I/-L/-l -
# resolves and links tests/raw_link/main.cpp against the installed
# package, in FOUR scenarios that isolate what could silently regress:
#
#   1. "default layout": the ordinary single-arch install
#      (CMAKE_INSTALL_LIBDIR unset, GNUInstallDirs default), shared
#      build. Proves the module resolves and the RELOCATABLE prefix
#      (glintfx.pc's own `${pcfiledir}`-relative prefix, GODS_LAWS.md
#      LEI ZERO: an unknown consumer's install prefix is never this
#      machine's) RESOLVES to the SAME scratch prefix the file was
#      installed into (compared by realpath, not by a textual "starts
#      with" match - see assert_libdir_resolves_to_real_install_dir's
#      own comment for why a textual match under-proves this), not at
#      whatever CMAKE_INSTALL_PREFIX happened to be at configure time.
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
#   3. "malformed layout" (adversarial review, PKG-DIST achado 1,
#      reproduced live before the fix): CMAKE_INSTALL_LIBDIR given with
#      a trailing slash ("lib64/"), plausible packager input. Before
#      cmake_path(NORMAL_PATH) was added to
#      glintfx_compute_pkgconfig_relocatable_prefix()
#      (cmake/GlintfxInstall.cmake), the unnormalized double slash in
#      "lib64/" + "/pkgconfig" inflated the segment count by one, and
#      the emitted prefix walked ONE DIRECTORY TOO FAR UP -
#      `pkg-config --exists` still reported success (it never checks
#      that the resolved libdir/includedir contain anything), so the
#      failure was silent: exactly the wrong-path-that-looks-right
#      failure mode LEI ZERO exists to rule out. This scenario is the
#      regression test for that fix.
#   4. "static": BUILD_SHARED_LIBS=OFF, consumer built with
#      `pkg-config --cflags --libs --static glintfx`. Proves the
#      Libs.private field (-lwayland-client on Linux) has the correct
#      CONTENT and is actually EMITTED by pkg-config's --static line -
#      both directly observable and checked here.
#      DECLARED DOWNGRADE (adversarial review, PKG-DIST achado 2,
#      reproduced live: emptying Libs.private by hand still links and
#      runs through this exact path): this scenario does NOT prove the
#      field is load-bearing for a REAL static link today, because
#      tests/raw_link/main.cpp only calls glintfx::runtime_version()
#      (src/core/version.cpp.o) - it never references any symbol that
#      would make the linker pull glintfx_library's OTHER static
#      archive member (the Wayland xdg-shell protocol binding,
#      src/platform/wayland/, which is what actually needs
#      wayland-client's symbols) out of libglintfx.a, so nothing in
#      this link currently DEMANDS Libs.private to be non-empty. Static
#      linkers only extract archive members that resolve an outstanding
#      undefined reference; with none pending against that member, its
#      presence or absence in Libs.private is unobservable from this
#      consumer. Forcing that extraction would require this test to
#      reference a private, wayland-scanner-generated, hidden-visibility
#      symbol (e.g. xdg_wm_base_interface) that is not, and must not
#      become, part of glintfx's public surface (GODS_LAWS.md L-19) -
#      ruled out on purpose, not left out by oversight. The real proof
#      arrives naturally the day a consumer exists that calls into the
#      Wayland-backed code path (WL-DISPLAY, a later fatia); until then,
#      this scenario proves field CORRECTNESS and EMISSION, not LINK
#      NECESSITY, and no claim beyond that is made anywhere in this
#      file or in the fatia's commit history.
#
# Usage: check_pkgconfig.sh <glintfx-source-dir> <raw-consumer-source-file> <cxx-compiler>
#
# Each function below does one thing (GODS_LAWS.md L-17).

set -eu

readonly LIBDIR_ARCH="x86_64-linux-gnu"
readonly MULTIARCH_LIBDIR="lib/${LIBDIR_ARCH}"
readonly DEFAULT_LIBDIR="lib"
# The exact reproduction from the adversarial review (achado 1): a
# trailing slash is plausible packager input
# (-DCMAKE_INSTALL_LIBDIR=lib64/), and CMake's own install() already
# normalizes it when writing to disk (the real directory is
# "lib64/pkgconfig", not "lib64//pkgconfig") - the bug was entirely in
# glintfx's OWN segment-counting, not in where CMake actually put the
# file.
readonly MALFORMED_LIBDIR="lib64/"
readonly MALFORMED_LIBDIR_REAL="lib64"

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

# realpath -m is load-bearing for
# assert_libdir_resolves_to_real_install_dir below: it canonicalizes
# "/.." sequences WITHOUT requiring every path component to exist,
# which is exactly what is needed to catch a wrong-but-existing
# resolved directory (achado 1's failure mode) as well as a
# resolved-to-nowhere one.
require_realpath() {
    command -v realpath >/dev/null 2>&1 || fail "realpath not found on PATH"
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

# Proves the prefix inside glintfx.pc actually RESOLVES to the exact
# real install directory on disk - the whole point of a relocatable
# ${pcfiledir}-based prefix (see the file-level comment, scenarios
# 1/2/3).
#
# realpath -m the REPORTED value before comparing, not a textual
# "starts with the prefix" pattern match (adversarial review, PKG-DIST
# achado 1: the FIRST version of this assertion used exactly that
# textual match, and it MISSED the bug - pkg-config's own
# --variable=libdir output leaves any "/.." in the value UNRESOLVED,
# so a walked-too-far-up prefix like
# "$prefix/lib64/pkgconfig/../../../lib64/" still starts with the
# literal text of "$prefix", passing a substring/glob check while
# resolving to a directory ONE LEVEL ABOVE it). Comparing two
# realpath'd values is the only form of this check that observes what
# a real linker/compiler -I/-L flag would actually resolve to.
assert_libdir_resolves_to_real_install_dir() {
    pkgconfig_dir="$1"
    expected_libdir_abs="$2"
    reported_libdir="$(PKG_CONFIG_PATH="$pkgconfig_dir" pkg-config --variable=libdir glintfx)"
    resolved_reported="$(realpath -m "$reported_libdir")"
    resolved_expected="$(realpath -m "$expected_libdir_abs")"
    [ "$resolved_reported" = "$resolved_expected" ] \
        || fail "glintfx.pc libdir resolves to '${resolved_reported}' (raw pkg-config value: '${reported_libdir}'), expected it to resolve to the real install directory '${resolved_expected}' - the relocatable prefix expression did not walk back to the real install tree"
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
    assert_libdir_resolves_to_real_install_dir "$pkgconfig_dir" "${prefix}/${DEFAULT_LIBDIR}"

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
    assert_libdir_resolves_to_real_install_dir "$pkgconfig_dir" "${prefix}/${MULTIARCH_LIBDIR}"

    binary="${scratch}/consumer-multiarch"
    compile_and_run_dynamic_consumer "$pkgconfig_dir" "$consumer_src" "$cxx" "${prefix}/${MULTIARCH_LIBDIR}" "$binary"
    echo "ok: multiarch-layout scenario - relocatable prefix survives a three-segment pkgconfig install dir."
}

# Scenario 3: malformed layout (adversarial review, PKG-DIST achado 1) -
# CMAKE_INSTALL_LIBDIR given with a trailing slash, the exact
# reproduction from the review. Regression test for
# cmake_path(NORMAL_PATH) in
# glintfx_compute_pkgconfig_relocatable_prefix()
# (cmake/GlintfxInstall.cmake): without it, this scenario's own
# assert_libdir_resolves_to_real_install_dir call fails, resolving one
# directory ABOVE the real install prefix, silently ("pkg-config
# --exists" still reports success either way).
run_malformed_libdir_scenario() {
    glintfx_src="$1"
    consumer_src="$2"
    cxx="$3"
    scratch="$4"

    build_dir="${scratch}/build-malformed"
    prefix="${scratch}/prefix-malformed"
    configure_glintfx "$glintfx_src" "$build_dir" "$cxx" "$MALFORMED_LIBDIR" ON
    build_and_install_glintfx "$build_dir" "$prefix"

    # CMake's own install() already normalizes the trailing slash when
    # writing to disk (confirmed live: -DCMAKE_INSTALL_LIBDIR=lib64/
    # still installs into "<prefix>/lib64/pkgconfig", not
    # "lib64//pkgconfig") - MALFORMED_LIBDIR_REAL is that real,
    # normalized directory name, used here to locate the .pc file CMake
    # actually wrote and to compute the CORRECT expected libdir this
    # scenario checks against.
    pkgconfig_dir="$(pkgconfig_dir_for "$prefix" "$MALFORMED_LIBDIR_REAL")"
    assert_pkgconfig_finds_glintfx "$pkgconfig_dir"
    assert_libdir_resolves_to_real_install_dir "$pkgconfig_dir" "${prefix}/${MALFORMED_LIBDIR_REAL}"

    binary="${scratch}/consumer-malformed"
    compile_and_run_dynamic_consumer "$pkgconfig_dir" "$consumer_src" "$cxx" "${prefix}/${MALFORMED_LIBDIR_REAL}" "$binary"
    echo "ok: malformed-layout scenario - a trailing slash in CMAKE_INSTALL_LIBDIR (lib64/) does not inflate the relocatable prefix's directory-climb count."
}

# Scenario 4: static build - proves Libs.private's CONTENT and
# EMISSION, not link necessity (declared downgrade, adversarial
# review, PKG-DIST achado 2 - see the file-level comment for why, and
# what the honest scope of this scenario is).
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
    echo "ok: static scenario - pkg-config --libs --static glintfx carries the correct Libs.private content (-lwayland-client) and the raw link succeeds. NOT proven here: that omitting Libs.private would break this specific link - it would not, today, because tests/raw_link/main.cpp never pulls the Wayland-bound archive member (see the file-level comment, scenario 4)."
}

main() {
    require_args "$@"
    require_pkg_config
    require_realpath
    glintfx_src="$1"
    consumer_src="$2"
    cxx="$3"

    scratch="$(make_scratch_workdir)"
    trap 'rm -rf "$scratch"' EXIT

    run_default_layout_scenario "$glintfx_src" "$consumer_src" "$cxx" "$scratch"
    run_multiarch_layout_scenario "$glintfx_src" "$consumer_src" "$cxx" "$scratch"
    run_malformed_libdir_scenario "$glintfx_src" "$consumer_src" "$cxx" "$scratch"
    run_static_scenario "$glintfx_src" "$consumer_src" "$cxx" "$scratch"

    echo "ok: glintfx.pc resolves via pkg-config alone (no CMake, no find_package) in default layout, Debian-multiarch layout, a malformed (trailing-slash) layout and static mode."
}

main "$@"
