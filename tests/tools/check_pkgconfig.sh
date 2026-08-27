#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
# check_pkgconfig.sh - proves the installed glintfx.pc (PKG-DIST) is a
# working pkg-config module: `pkg-config --cflags --libs glintfx` alone
# - no CMake, no find_package(glintfx), no hand-written -I/-L/-l -
# resolves and links tests/raw_link/main.cpp against the installed
# package, in SIX scenarios that isolate what could silently regress.
#
# adversarial review round 3 changed the STRATEGY, not just the
# scenario count (Caetano's ruling, relayed verbatim: "parar de
# prever a entrada e passar a validar a saida. Zero algebra de caminho
# nova."). Rounds 1 and 2 fixed real bugs (achados 1/3/4 below) by
# computing paths more carefully at CMake configure time; round 3
# found a class of input - CMAKE_INSTALL_LIBDIR/INCLUDEDIR left
# relative, prefix decided only at INSTALL time via DESTDIR - where
# NO amount of configure-time computation can ever be correct, because
# the final install root does not exist, and is not knowable, at
# configure time. assert_pkgconfig_output_resolves_to_real_content
# (below) is the answer: instead of precomputing an "expected" path
# and comparing, it reads back whatever pkg-config actually says and
# checks THAT against reality on disk - the only form of check that
# still works when no expected value can be computed in advance. It
# is applied to EVERY scenario below, not only scenario 6, per the
# review's own generalization test ("se ele so reprovar em um, ele
# nao generalizou").
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
#      reproduced live before the fix; strengthened round 4, achado C):
#      CMAKE_INSTALL_LIBDIR given as "./lib64//" - a trailing slash, a
#      doubled path separator, AND a leading "./" all at once, the
#      exact three malformed forms PACKAGING.md promises are tested.
#      Before cmake_path(NORMAL_PATH) was added to
#      glintfx_compute_pkgconfig_relocatable_prefix()
#      (cmake/GlintfxInstall.cmake), an unnormalized doubled slash in
#      "lib64/" + "/pkgconfig" inflated the segment count by one, and
#      the emitted prefix walked ONE DIRECTORY TOO FAR UP -
#      `pkg-config --exists` still reported success (it never checks
#      that the resolved libdir/includedir contain anything), so the
#      failure was silent: exactly the wrong-path-that-looks-right
#      failure mode LEI ZERO exists to rule out. This scenario is the
#      regression test for that fix - round 4 combined all three
#      malformed forms into one value instead of testing only the
#      trailing slash, because PACKAGING.md claims all three are
#      tested and, before this change, two of them were not: the code
#      already handled them (confirmed live), only the test coverage
#      fell short of the doc's own claim.
#   4. "absolute install dirs" (adversarial review, PKG-DIST achado 3,
#      reproduced live before the fix): CMAKE_INSTALL_LIBDIR AND
#      CMAKE_INSTALL_INCLUDEDIR both set to ABSOLUTE paths, independent
#      of any prefix - GNUInstallDirs officially allows this (a
#      packager staging into a fixed system location), and
#      glintfx_compute_pkgconfig_relocatable_prefix() already handled
#      it correctly for `prefix=` itself, but `libdir=`/`includedir=`
#      still hand-typed a "${exec_prefix}/"/"${prefix}/" base in front
#      of the value regardless: with an absolute
#      CMAKE_INSTALL_LIBDIR=/var/tmp/gvabs/inst/lib64, the emitted line
#      was "libdir=${exec_prefix}//var/tmp/gvabs/inst/lib64" - the
#      prefix appeared TWICE, `pkg-config --libs` printed a path that
#      does not exist on disk, and `pkg-config --exists` still reported
#      success. Same silent failure mode as scenario 3, different input
#      shape; both libdir and includedir are exercised here because
#      they are independent CMake variables and the bug was independent
#      in each. This scenario is the regression test for
#      glintfx_compute_pkgconfig_path_expression() in
#      cmake/GlintfxInstall.cmake.
#   5. "static": BUILD_SHARED_LIBS=OFF. Proves the Libs.private field
#      (-lwayland-client on Linux) has the correct CONTENT, is
#      EMITTED by pkg-config's --static line (via
#      tests/raw_link/main.cpp, same as before), AND is genuinely
#      LOAD-BEARING for a real static link (adversarial review,
#      PKG-DIST achado 4 - corrects an earlier, WRONG version of this
#      scenario that only claimed the first two and cited
#      GODS_LAWS.md L-19 as the reason a real link proof was
#      impossible; L-19 governs glintfx's INSTALLED, DISTRIBUTED
#      public surface, and does not reach a throwaway .cpp generated
#      by THIS SCRIPT at test time, never tracked, never installed,
#      never distributed - that citation was simply wrong, proven
#      wrong by writing the probe below and watching it link).
#
#      The load-bearing proof needs its own consumer, GENERATED here
#      (write_wayland_symbol_probe_source, below), because
#      tests/raw_link/main.cpp only calls glintfx::runtime_version()
#      (src/core/version.cpp.o) - it never references any symbol that
#      would make the linker pull glintfx_library's OTHER static
#      archive member (the Wayland xdg-shell protocol binding,
#      src/platform/wayland/) out of libglintfx.a, so Libs.private's
#      presence or absence is unobservable through that file alone.
#      The probe instead references `xdg_wm_base_interface` directly -
#      wayland-scanner private-code's OWN generated symbol, hidden
#      visibility (suppresses DSO/dynamic export only, not STATIC
#      link-time resolution within the same final link) - which forces
#      the linker to extract that archive member, and THAT member has
#      its own real, unresolved references to wl_surface_interface/
#      wl_seat_interface/wl_output_interface (confirmed live: `nm -u`
#      on the extracted xdg-shell-protocol.c.o member lists exactly
#      those three as undefined) that only wayland-client provides.
#      Two controls, both executed: linking the probe WITHOUT
#      Libs.private (plain `pkg-config --cflags --libs glintfx`, no
#      --static) FAILS with those exact undefined references
#      (negative control - reproduced live before writing this
#      scenario); linking it WITH Libs.private (`--cflags --libs
#      --static`) SUCCEEDS (positive control).
#
#      Trade-off accepted, and it is real (this is now the actual, and
#      only, reason the proof stays this shape rather than something
#      broader): `xdg_wm_base_interface` is wayland-scanner
#      private-code's own generated name, not a name glintfx chose or
#      promises to keep - a future protocol added, or private-code mode
#      swapped for public-code, could rename or remove it for reasons
#      that have nothing to do with a real Libs.private regression, and
#      this probe would need updating. That coupling is deliberate and
#      contained to this ONE throwaway, never-shipped test file, not a
#      law being bent.
#   6. "DESTDIR, Fedora format" (adversarial review round 3, task 2;
#      the concrete case behind the ruling above): CMAKE_INSTALL_PREFIX
#      set to /usr, CMAKE_INSTALL_LIBDIR left UNSET (GNUInstallDirs'
#      own per-distro default), install run with `DESTDIR=<staging>
#      cmake --install` instead of `--prefix`. This is not a synthetic
#      edge case - it is verbatim what a real Fedora .spec's %cmake/
#      %cmake_install macros do (confirmed against
#      /usr/lib/rpm/macros.d/macros.cmake on this machine: %cmake sets
#      -DCMAKE_INSTALL_PREFIX:PATH=%{_prefix} and never passes
#      CMAKE_INSTALL_LIBDIR at all; %cmake_install runs
#      `DESTDIR="%{buildroot}" cmake --install`) - the format 100% of
#      real Fedora packages that use CMake actually build under, and
#      the one none of scenarios 1-5 exercise (they all use `--prefix
#      <scratch>`, never DESTDIR). None of glintfx's own CMake code
#      changed for this scenario - it was already correct, confirmed
#      live before writing this scenario, because the RELATIVE
#      `${pcfiledir}`-climbing prefix (scenarios 1-3's own mechanism)
#      is immune to DESTDIR by construction: ${pcfiledir} resolves to
#      wherever the .pc file physically sits when queried, which is
#      already inside the staging tree here, no extra staging-root
#      parameter needed anywhere in the validator.
#
# PERF-PKGCONFIG (27/08/2026): this file used to give scenarios 1, 2,
# 3, 4 and 6 (five of the six above - everything except the static
# scenario) their OWN fresh `cmake -S <src> -B <fresh-dir>` PLUS a full
# `cmake --build`, even though NONE of the five ever changes
# BUILD_SHARED_LIBS or any other flag that touches a compiled object
# file - they only vary CMAKE_INSTALL_LIBDIR, CMAKE_INSTALL_INCLUDEDIR
# and CMAKE_INSTALL_PREFIX, all three configure-time-only, install()-
# affecting values. Measured live: reconfiguring an already-built tree
# with any of those three changed, then rebuilding, prints "ninja: no
# work to do" and costs well under a second, not a recompile (the same
# fact PERF-PKGVALIDATE already established and reused for
# check_pkgconfig_validate.sh's own sibling gate). This file now
# builds glintfx TWICE, not six times: ONCE for the five scenarios
# above that only ever vary install-time layout (shared build,
# "$SHARED_BUILD_DIR" below), and once more for the static scenario,
# which genuinely needs a different compiled artifact
# (BUILD_SHARED_LIBS=OFF changes what is actually linked).
#
# The DESTDIR/Fedora-format scenario (6) is the one caller of the
# shared build's OWN first, virgin configure - it is the only scenario
# in this file that relies on CMAKE_INSTALL_LIBDIR being genuinely
# UNSET (GNUInstallDirs' own per-distro auto-default), and a CMake
# cache variable, once explicitly set, does not revert to "unset" on a
# later reconfigure without deleting the cache entry - so this
# scenario runs FIRST, against the shared build dir's first configure,
# before any other scenario ever passes an explicit
# -DCMAKE_INSTALL_LIBDIR. Every scenario after it reconfigures the
# SAME shared build dir with CMAKE_INSTALL_LIBDIR (and, where it
# matters, CMAKE_INSTALL_INCLUDEDIR) explicitly. CMAKE_INSTALL_PREFIX
# is passed explicitly on every single reconfigure of the shared
# build, in both this scenario's own case (needs "/usr") and every
# other one's (does not need any specific value, since each of them
# installs with its own `cmake --install --prefix <scratch>` override
# - confirmed live that this overrides a cached CMAKE_INSTALL_PREFIX
# for that one invocation - but scenario 4's absolute-dirs case DOES
# bake whatever CMAKE_INSTALL_PREFIX was cached at configure time into
# glintfx.pc's `prefix=` line, per
# glintfx_compute_pkgconfig_relocatable_prefix()'s own absolute-libdir
# fallback branch in cmake/GlintfxInstall.cmake - no scenario here
# currently asserts on that literal value, but leaving it to whatever
# a PREVIOUS scenario happened to cache would be exactly the kind of
# silent cross-scenario coupling GODS_LAWS.md L-27 exists to rule out,
# so it never is).
#
# The net effect: the SIX scenarios below, and every assertion each
# one makes, are byte-for-byte the same claims as before this change -
# only the EXECUTION ORDER changed (destdir now runs first, so it can
# be the shared build's first, unmodified configure) and the number of
# full compiles dropped from six to two.
#
# Usage: check_pkgconfig.sh <glintfx-source-dir> <raw-consumer-source-file> <cxx-compiler>
#
# Each function below does one thing (GODS_LAWS.md L-17).

set -eu

readonly LIBDIR_ARCH="x86_64-linux-gnu"
readonly MULTIARCH_LIBDIR="lib/${LIBDIR_ARCH}"
readonly DEFAULT_LIBDIR="lib"
# Combines the THREE malformations PACKAGING.md promises are handled -
# a trailing slash, a doubled path separator, and a leading "./" - in
# ONE value, so a single scenario run exercises everything the doc
# claims (adversarial review round 4, achado C: PACKAGING.md said
# "All of the following are tested, on every push", but the scenario
# before this one only ever fed a bare trailing slash - two of the
# three forms it lists were never actually run through a test, even
# though the code already handled them; the fix here is to make the
# claim true, not to weaken it). Confirmed live before adopting this
# value: `./lib64//` normalizes and resolves correctly end to end
# (configure, install, pkg-config resolution, AND a real compile+link
# against the result) - the code already handled all three, only the
# test coverage was short of the doc's own claim. CMake's own
# install() already normalizes it when writing to disk (the real
# directory is "lib64", matching MALFORMED_LIBDIR_REAL below) - the
# bug this scenario originally targeted (achado 1) was entirely in
# glintfx's OWN segment-counting, not in where CMake actually put the
# file.
readonly MALFORMED_LIBDIR="./lib64//"
readonly MALFORMED_LIBDIR_REAL="lib64"
# Explicit, inert default (PERF-PKGCONFIG): every shared-build
# reconfigure below states a CMAKE_INSTALL_PREFIX, and every scenario
# that installs with its own `--prefix <scratch>` override never reads
# this value at all - it exists so no scenario ever inherits whatever
# a PREVIOUS one happened to cache (see the file-level comment).
readonly INERT_PREFIX="/usr/local"

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

# Reconfigures (or, the first time, configures) the SHARED build dir
# used by scenarios 1, 2, 3, 4 and 6 (PERF-PKGCONFIG) - always with an
# explicit libdir, shared_libs and prefix, every call, so no scenario
# ever inherits a value a previous one happened to leave cached.
configure_glintfx() {
    glintfx_src="$1"
    build_dir="$2"
    cxx="$3"
    libdir="$4"
    shared_libs="$5"
    prefix_value="$6"
    cmake -S "$glintfx_src" -B "$build_dir" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_COMPILER="$cxx" \
        -DCMAKE_INSTALL_LIBDIR="$libdir" \
        -DBUILD_SHARED_LIBS="$shared_libs" \
        -DCMAKE_INSTALL_PREFIX="$prefix_value" \
        -DGLINTFX_BUILD_TESTS=OFF
}

build_and_install_glintfx() {
    build_dir="$1"
    prefix="$2"
    cmake --build "$build_dir"
    cmake --install "$build_dir" --prefix "$prefix"
}

# Scenario 4 (absolute install dirs) needs its own reconfigure call: it
# sets BOTH CMAKE_INSTALL_LIBDIR and CMAKE_INSTALL_INCLUDEDIR to
# absolute paths, which configure_glintfx() above has no parameter for
# (it only ever varies CMAKE_INSTALL_LIBDIR, relative, across the other
# scenarios that share its build dir).
configure_glintfx_with_absolute_dirs() {
    glintfx_src="$1"
    build_dir="$2"
    cxx="$3"
    abs_libdir="$4"
    abs_includedir="$5"
    prefix_value="$6"
    cmake -S "$glintfx_src" -B "$build_dir" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_COMPILER="$cxx" \
        -DCMAKE_INSTALL_LIBDIR="$abs_libdir" \
        -DCMAKE_INSTALL_INCLUDEDIR="$abs_includedir" \
        -DBUILD_SHARED_LIBS=ON \
        -DCMAKE_INSTALL_PREFIX="$prefix_value" \
        -DGLINTFX_BUILD_TESTS=OFF
}

# No --prefix, deliberately: CMake's install() ignores any prefix for a
# DESTINATION that is already absolute (GNUInstallDirs semantics), and
# both CMAKE_INSTALL_LIBDIR and CMAKE_INSTALL_INCLUDEDIR are absolute
# in this scenario - passing a --prefix here would be decorative, and
# its absence is itself part of what this scenario proves (the files
# land exactly at the absolute paths given, nothing else involved).
build_and_install_glintfx_no_prefix() {
    build_dir="$1"
    cmake --build "$build_dir"
    cmake --install "$build_dir"
}

# Scenario 6 (DESTDIR, Fedora format) configure step - and, since
# PERF-PKGCONFIG, the shared build dir's FIRST, virgin configure: the
# format a REAL Linux packager actually uses (confirmed against
# /usr/lib/rpm/macros.d/macros.cmake on this Fedora machine: %cmake
# passes -DCMAKE_INSTALL_PREFIX:PATH=%{_prefix} and NEVER passes
# CMAKE_INSTALL_LIBDIR at all; %cmake_install runs
# `DESTDIR="%{buildroot}" cmake --install`). None of the other five
# scenarios in this file exercise this input shape - they all set
# CMAKE_INSTALL_LIBDIR explicitly, which is exactly why this one has to
# run before any of them ever touch the shared build dir's cache.
configure_glintfx_fedora_style() {
    glintfx_src="$1"
    build_dir="$2"
    cxx="$3"
    cmake -S "$glintfx_src" -B "$build_dir" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_COMPILER="$cxx" \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DBUILD_SHARED_LIBS=ON \
        -DGLINTFX_BUILD_TESTS=OFF
}

# DESTDIR, not --prefix - the mechanism a real packager's
# %cmake_install/%make_install actually uses (RPM's `buildroot`, DEB's
# `debian/tmp`). Fundamentally different from --prefix: --prefix
# REWRITES relative destinations at install time; DESTDIR PREPENDS to
# EVERY destination, relative and absolute alike, without touching any
# value baked into installed file CONTENT (glintfx.pc still says
# "prefix=/usr" from the eventual real consumer's point of view, not
# the staging path) - and this is also the concrete case behind
# Caetano's ruling: the staging root does not exist, and is not
# knowable, at CMake CONFIGURE time ("o prefixo de instalacao ainda
# nao existe quando o calculo roda"), so no destdir-aware fix belongs
# in cmake/GlintfxInstall.cmake at all - DESTDIR is deliberately
# invisible to configure-time code, by CMake's own design. No real
# file ever touches the actual system /usr on this machine - DESTDIR
# redirects every destination under the given staging root.
build_and_install_glintfx_with_destdir() {
    build_dir="$1"
    destdir="$2"
    cmake --build "$build_dir"
    DESTDIR="$destdir" cmake --install "$build_dir"
}

# Locates the .pc file CMake actually wrote inside the staging tree -
# deliberately NOT hand-computed: GNUInstallDirs' own per-distro
# default for CMAKE_INSTALL_LIBDIR ("lib64" on this machine, "lib" or
# a multiarch path elsewhere) is exactly the kind of prediction this
# whole round moved away from making. Finding it instead of predicting
# it is itself an instance of "validate the output".
find_pkgconfig_dir_under_destdir() {
    destdir="$1"
    found="$(find "$destdir" -type d -name pkgconfig 2>/dev/null | head -n1)"
    [ -n "$found" ] || fail "no pkgconfig/ directory found anywhere under staging root ${destdir} after install"
    printf '%s' "$found"
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

# Same check, for `includedir=` (adversarial review, PKG-DIST achado 3:
# "o includedir tem a mesma exposicao, e o revisor o quebrou
# separadamente" - libdir and includedir are two INDEPENDENT
# CMAKE_INSTALL_* variables, each independently allowed to be relative
# or absolute by GNUInstallDirs, so proving one says nothing about the
# other).
assert_includedir_resolves_to_real_install_dir() {
    pkgconfig_dir="$1"
    expected_includedir_abs="$2"
    reported_includedir="$(PKG_CONFIG_PATH="$pkgconfig_dir" pkg-config --variable=includedir glintfx)"
    resolved_reported="$(realpath -m "$reported_includedir")"
    resolved_expected="$(realpath -m "$expected_includedir_abs")"
    [ "$resolved_reported" = "$resolved_expected" ] \
        || fail "glintfx.pc includedir resolves to '${resolved_reported}' (raw pkg-config value: '${reported_includedir}'), expected it to resolve to the real install directory '${resolved_expected}'"
}

# Resolves ONE directory-shaped value from pkg-config's own output (a
# --variable=X value, or an already-stripped -I/-L flag) via
# realpath -m and confirms it not only EXISTS but genuinely CONTAINS
# what glintfx.pc promises there (adversarial review round 3, achados
# 5/6 - "pare de prever a entrada e passe a validar a saida"). Two
# content shapes only, because those are the only two things any path
# from THIS module ever names: the installed public header tree (a
# "glintfx" subdirectory) or the installed library artifact
# (libglintfx.so*/libglintfx.a).
assert_dir_contains_expected_content() {
    label="$1"
    raw_path="$2"
    expected_content_kind="$3"
    # Explicit empty check before realpath (adversarial review round
    # 4, achado A): `realpath -m ""` fails with its own raw,
    # locale-dependent OS error text instead of this file's fail() -
    # still a failure either way (this label already never bails
    # silently), just a worse diagnostic. Cheap to fix, so fixed.
    [ -n "$raw_path" ] || fail "${label} is empty"
    resolved="$(realpath -m "$raw_path")"
    [ -d "$resolved" ] \
        || fail "${label} is '${raw_path}', which resolves to '${resolved}' - not a directory that exists on disk"
    case "$expected_content_kind" in
        headers)
            [ -d "${resolved}/glintfx" ] \
                || fail "${label} is '${raw_path}' (resolves to '${resolved}'), which exists but has no 'glintfx/' subdirectory - the installed public headers are not there"
            ;;
        library)
            found=""
            for candidate in "${resolved}"/libglintfx.so* "${resolved}"/libglintfx.a; do
                if [ -e "$candidate" ]; then
                    found="$candidate"
                fi
            done
            [ -n "$found" ] \
                || fail "${label} is '${raw_path}' (resolves to '${resolved}'), which exists but has no libglintfx.so*/libglintfx.a - the installed library artifact is not there"
            ;;
        *)
            fail "assert_dir_contains_expected_content: unknown expected_content_kind '${expected_content_kind}'"
            ;;
    esac
}

# THE output-validation atom (adversarial review round 3, task 1):
# every path pkg-config emits for glintfx - --variable=includedir,
# --variable=libdir, and every -I/-L token inside --cflags/--libs -
# resolves to something that EXISTS and CONTAINS what it promises. Not
# a new prediction of what a path SHOULD be (path algebra is exactly
# what the previous two rounds of fixes were, and Caetano's ruling
# ends that strategy: "parar de prever a entrada e passar a validar a
# saida. Zero algebra de caminho nova."); this only asks whether what
# pkg-config just told a real consumer actually holds up on disk -
# which is the ONLY form of check that still works when the real
# final location is not decidable at CMake configure time at all (a
# DESTDIR-staged install, scenario 6, being the concrete case: the
# install prefix does not exist yet when glintfx.pc's content is
# computed, so no "expected path" can be precomputed to compare
# against - only the output, read back after the fact, can be
# checked).
#
# Applied to EVERY scenario in this file, not only the ones a bug was
# found in (round-3 review: "se ele so reprovar em um, ele nao
# generalizou" - a validator that only fires where a known bug already
# lives is a regression test for that one bug, not a general content
# check).
assert_pkgconfig_output_resolves_to_real_content() {
    pkgconfig_dir="$1"
    static_flag="${2:-}"

    includedir_var="$(PKG_CONFIG_PATH="$pkgconfig_dir" pkg-config --variable=includedir glintfx)"
    libdir_var="$(PKG_CONFIG_PATH="$pkgconfig_dir" pkg-config --variable=libdir glintfx)"
    assert_dir_contains_expected_content "glintfx.pc includedir" "$includedir_var" headers
    assert_dir_contains_expected_content "glintfx.pc libdir" "$libdir_var" library

    # Intentional word-splitting, same reasoning as every other
    # pkg-config-output consumer in this file: this is exactly the
    # form the flags come in for a real build recipe.
    # shellcheck disable=SC2086 # static_flag is intentionally either empty or one flag, never quoted-word-split content
    cflags="$(PKG_CONFIG_PATH="$pkgconfig_dir" pkg-config --cflags ${static_flag} glintfx)"
    # shellcheck disable=SC2086 # static_flag is intentionally either empty or one flag, never quoted-word-split content
    libs="$(PKG_CONFIG_PATH="$pkgconfig_dir" pkg-config --libs ${static_flag} glintfx)"

    # Non-empty-scan floor (adversarial review round 4, achado B): a
    # `Cflags:`/`Libs:` line with no -I/-L token at all would make the
    # two loops below iterate zero times and this atom would still
    # print "check passed" - the classic empty-scan-prints-green
    # defect (TESTES.md/preci.sh's own enumeration-floor discipline,
    # applied here). Not exploitable TODAY (the compile step right
    # after this call would fail on a missing include/library), but
    # that safety net is check_pkgconfig.sh's OWN later steps, not
    # this atom's - PKG-VALIDATE (a later fatia) is expected to reuse
    # this atom on its own, without a compile step guaranteed to catch
    # the gap, so the floor belongs HERE, not in whatever calls it.
    include_flag_count=0
    library_flag_count=0

    for token in $cflags; do
        case "$token" in
            -I*)
                assert_dir_contains_expected_content "glintfx.pc -I flag" "${token#-I}" headers
                include_flag_count=$((include_flag_count + 1))
                ;;
        esac
    done
    for token in $libs; do
        case "$token" in
            -L*)
                assert_dir_contains_expected_content "glintfx.pc -L flag" "${token#-L}" library
                library_flag_count=$((library_flag_count + 1))
                ;;
        esac
    done

    [ "$include_flag_count" -gt 0 ] \
        || fail "glintfx.pc emitted ZERO -I flags in 'pkg-config --cflags ${static_flag} glintfx' ('${cflags}') - an empty Cflags: line would make this atom scan nothing and still report success"
    [ "$library_flag_count" -gt 0 ] \
        || fail "glintfx.pc emitted ZERO -L flags in 'pkg-config --libs ${static_flag} glintfx' ('${libs}') - an empty Libs: line would make this atom scan nothing and still report success"

    echo "check_pkgconfig.sh: output-content check passed - includedir/libdir and every -I/-L token pkg-config emitted (${include_flag_count} -I, ${library_flag_count} -L) resolve to real, populated directories"
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
    # shellcheck disable=SC2086 # cflags/libs are pkg-config output, a space-separated argument list meant to word-split
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
    # Intentional word-splitting, same reasoning as the dynamic
    # consumer above - and here it is load-bearing: --static is what
    # makes Libs.private (-lwayland-client) appear in $libs at all.
    # shellcheck disable=SC2086 # cflags/libs are pkg-config output, a space-separated argument list meant to word-split
    "$cxx" -std=c++23 ${cflags} "$consumer_src" ${libs} -o "$output_bin"
    [ -x "$output_bin" ] || fail "static consumer binary not found after compile: $output_bin"
    echo "check_pkgconfig.sh: running $output_bin (static, pkg-config --cflags --libs --static glintfx)"
    "$output_bin"
}

# Writes a tiny .cpp INTO THE SCRATCH DIR at test-run time (adversarial
# review, PKG-DIST achado 4) - never a tracked file in the repo, never
# installed, never distributed. Referencing xdg_wm_base_interface
# directly forces the linker to extract glintfx_library's Wayland
# xdg-shell protocol-binding archive member out of libglintfx.a, the
# ONLY way to make Libs.private's presence or absence OBSERVABLE
# through a real link (see the file-level comment, scenario 5, for the
# full reasoning and the trade-off this accepts).
write_wayland_symbol_probe_source() {
    scratch="$1"
    probe_src="${scratch}/wayland_symbol_probe.cpp"
    cat > "$probe_src" << 'PROBE_EOF'
// SPDX-License-Identifier: AGPL-3.0-or-later
// Generated at test time by check_pkgconfig.sh - never tracked, never
// installed, never distributed (PKG-DIST achado 4). Forces the
// linker to extract glintfx_library's Wayland xdg-shell
// protocol-binding archive member by referencing ONE of its own
// internal, generated symbols directly, so that member's own real
// undefined references to wl_surface_interface/wl_seat_interface/
// wl_output_interface become part of THIS link - proving
// Libs.private is genuinely load-bearing, not merely present.
extern "C" const struct wl_interface xdg_wm_base_interface;

const void* const g_glintfx_wayland_symbol_probe = &xdg_wm_base_interface;

int main() {
    return 0;
}
PROBE_EOF
    printf '%s' "$probe_src"
}

# Negative control: linking the probe WITHOUT Libs.private (the plain,
# non-static pkg-config line) must FAIL - if it links, either
# Libs.private stopped being necessary or the probe stopped forcing
# extraction, either way this scenario's whole premise is gone.
assert_probe_links_without_wayland_client_fails() {
    pkgconfig_dir="$1"
    probe_src="$2"
    cxx="$3"
    output_bin="$4"
    cflags="$(PKG_CONFIG_PATH="$pkgconfig_dir" pkg-config --cflags glintfx)"
    libs="$(PKG_CONFIG_PATH="$pkgconfig_dir" pkg-config --libs glintfx)"
    # shellcheck disable=SC2086 # cflags/libs are pkg-config output, a space-separated argument list meant to word-split
    if "$cxx" -std=c++23 ${cflags} "$probe_src" ${libs} -o "$output_bin" 2>/dev/null; then
        fail "negative control failed: linking the Wayland-symbol probe WITHOUT Libs.private (plain 'pkg-config --cflags --libs glintfx') unexpectedly SUCCEEDED - either Libs.private is no longer necessary or the probe no longer forces extraction of the Wayland-bound archive member"
    fi
    echo "check_pkgconfig.sh: negative control confirmed - linking the probe without Libs.private fails (undefined wl_* symbols), as expected"
}

# Positive control: the exact same probe, linked WITH Libs.private
# (`--static`), must SUCCEED.
assert_probe_links_with_wayland_client_succeeds() {
    pkgconfig_dir="$1"
    probe_src="$2"
    cxx="$3"
    output_bin="$4"
    cflags="$(PKG_CONFIG_PATH="$pkgconfig_dir" pkg-config --cflags --static glintfx)"
    libs="$(PKG_CONFIG_PATH="$pkgconfig_dir" pkg-config --libs --static glintfx)"
    # shellcheck disable=SC2086 # cflags/libs are pkg-config output, a space-separated argument list meant to word-split
    "$cxx" -std=c++23 ${cflags} "$probe_src" ${libs} -o "$output_bin" \
        || fail "positive control failed: linking the Wayland-symbol probe WITH Libs.private (pkg-config --cflags --libs --static glintfx) unexpectedly FAILED"
    [ -x "$output_bin" ] || fail "probe binary not found after compile: $output_bin"
    echo "check_pkgconfig.sh: positive control confirmed - linking the probe with Libs.private succeeds"
}

# Scenario 1: default single-arch layout, shared build. Reconfigures
# the SHARED build dir (PERF-PKGCONFIG) - callable only after scenario
# 6 (destdir) has already run its own, virgin configure of the same
# build dir, since this is the first scenario to set
# CMAKE_INSTALL_LIBDIR explicitly.
run_default_layout_scenario() {
    glintfx_src="$1"
    consumer_src="$2"
    cxx="$3"
    scratch="$4"
    build_dir="$5"

    prefix="${scratch}/prefix-default"
    configure_glintfx "$glintfx_src" "$build_dir" "$cxx" "$DEFAULT_LIBDIR" ON "$INERT_PREFIX"
    build_and_install_glintfx "$build_dir" "$prefix"

    pkgconfig_dir="$(pkgconfig_dir_for "$prefix" "$DEFAULT_LIBDIR")"
    assert_pkgconfig_finds_glintfx "$pkgconfig_dir"
    assert_version_has_four_components "$pkgconfig_dir"
    assert_libdir_resolves_to_real_install_dir "$pkgconfig_dir" "${prefix}/${DEFAULT_LIBDIR}"
    assert_pkgconfig_output_resolves_to_real_content "$pkgconfig_dir"

    binary="${scratch}/consumer-default"
    compile_and_run_dynamic_consumer "$pkgconfig_dir" "$consumer_src" "$cxx" "${prefix}/${DEFAULT_LIBDIR}" "$binary"
    echo "ok: default-layout scenario - glintfx.pc resolves and links via pkg-config alone."
}

# Scenario 2: Debian-multiarch-style layout, shared build - the level
# count in the relocatable prefix expression has three directory
# segments to climb here, not two (see the file-level comment).
# Reconfigures the same SHARED build dir (PERF-PKGCONFIG).
run_multiarch_layout_scenario() {
    glintfx_src="$1"
    consumer_src="$2"
    cxx="$3"
    scratch="$4"
    build_dir="$5"

    prefix="${scratch}/prefix-multiarch"
    configure_glintfx "$glintfx_src" "$build_dir" "$cxx" "$MULTIARCH_LIBDIR" ON "$INERT_PREFIX"
    build_and_install_glintfx "$build_dir" "$prefix"

    pkgconfig_dir="$(pkgconfig_dir_for "$prefix" "$MULTIARCH_LIBDIR")"
    assert_pkgconfig_finds_glintfx "$pkgconfig_dir"
    assert_libdir_resolves_to_real_install_dir "$pkgconfig_dir" "${prefix}/${MULTIARCH_LIBDIR}"
    assert_pkgconfig_output_resolves_to_real_content "$pkgconfig_dir"

    binary="${scratch}/consumer-multiarch"
    compile_and_run_dynamic_consumer "$pkgconfig_dir" "$consumer_src" "$cxx" "${prefix}/${MULTIARCH_LIBDIR}" "$binary"
    echo "ok: multiarch-layout scenario - relocatable prefix survives a three-segment pkgconfig install dir."
}

# Scenario 3: malformed layout (adversarial review, PKG-DIST achado 1;
# strengthened round 4, achado C) - CMAKE_INSTALL_LIBDIR given as
# "./lib64//": a trailing slash, a doubled path separator, and a
# leading "./" all at once, the exact three forms PACKAGING.md
# promises are tested. Regression test for cmake_path(NORMAL_PATH) in
# glintfx_compute_pkgconfig_relocatable_prefix()
# (cmake/GlintfxInstall.cmake): without it, this scenario's own
# assert_libdir_resolves_to_real_install_dir call fails, resolving one
# directory ABOVE the real install prefix, silently ("pkg-config
# --exists" still reports success either way). Reconfigures the same
# SHARED build dir (PERF-PKGCONFIG).
run_malformed_libdir_scenario() {
    glintfx_src="$1"
    consumer_src="$2"
    cxx="$3"
    scratch="$4"
    build_dir="$5"

    prefix="${scratch}/prefix-malformed"
    configure_glintfx "$glintfx_src" "$build_dir" "$cxx" "$MALFORMED_LIBDIR" ON "$INERT_PREFIX"
    build_and_install_glintfx "$build_dir" "$prefix"

    # CMake's own install() already normalizes "./lib64//" when writing
    # to disk (confirmed live: still installs into
    # "<prefix>/lib64/pkgconfig", not some malformed variant of it) -
    # MALFORMED_LIBDIR_REAL is that real, normalized directory name,
    # used here to locate the .pc file CMake actually wrote and to
    # compute the CORRECT expected libdir this scenario checks against.
    pkgconfig_dir="$(pkgconfig_dir_for "$prefix" "$MALFORMED_LIBDIR_REAL")"
    assert_pkgconfig_finds_glintfx "$pkgconfig_dir"
    assert_libdir_resolves_to_real_install_dir "$pkgconfig_dir" "${prefix}/${MALFORMED_LIBDIR_REAL}"
    assert_pkgconfig_output_resolves_to_real_content "$pkgconfig_dir"

    binary="${scratch}/consumer-malformed"
    compile_and_run_dynamic_consumer "$pkgconfig_dir" "$consumer_src" "$cxx" "${prefix}/${MALFORMED_LIBDIR_REAL}" "$binary"
    echo "ok: malformed-layout scenario - a trailing slash, a doubled path separator AND a leading ./ in CMAKE_INSTALL_LIBDIR (./lib64//), all at once, do not inflate the relocatable prefix's directory-climb count."
}

# Scenario 4: absolute install dirs (adversarial review, PKG-DIST
# achado 3) - CMAKE_INSTALL_LIBDIR and CMAKE_INSTALL_INCLUDEDIR both
# given as ABSOLUTE paths, independent of any prefix. Regression test
# for glintfx_compute_pkgconfig_path_expression() in
# cmake/GlintfxInstall.cmake: without it, libdir=/includedir= each
# hand-typed a "${exec_prefix}/"/"${prefix}/" base in front of an
# already-absolute value, duplicating the prefix (see the file-level
# comment for the exact reproduction). Reconfigures the same SHARED
# build dir (PERF-PKGCONFIG).
run_absolute_dirs_scenario() {
    glintfx_src="$1"
    consumer_src="$2"
    cxx="$3"
    scratch="$4"
    build_dir="$5"

    abs_libdir="${scratch}/abs-install/lib64"
    abs_includedir="${scratch}/abs-install/include"
    configure_glintfx_with_absolute_dirs "$glintfx_src" "$build_dir" "$cxx" "$abs_libdir" "$abs_includedir" "$INERT_PREFIX"
    build_and_install_glintfx_no_prefix "$build_dir"

    pkgconfig_dir="${abs_libdir}/pkgconfig"
    assert_pkgconfig_finds_glintfx "$pkgconfig_dir"
    assert_libdir_resolves_to_real_install_dir "$pkgconfig_dir" "$abs_libdir"
    assert_includedir_resolves_to_real_install_dir "$pkgconfig_dir" "$abs_includedir"
    assert_pkgconfig_output_resolves_to_real_content "$pkgconfig_dir"

    binary="${scratch}/consumer-absolute"
    compile_and_run_dynamic_consumer "$pkgconfig_dir" "$consumer_src" "$cxx" "$abs_libdir" "$binary"
    echo "ok: absolute-install-dirs scenario - CMAKE_INSTALL_LIBDIR and CMAKE_INSTALL_INCLUDEDIR given as absolute paths resolve to themselves, with no duplicated prefix segment."
}

# Scenario 5: static build - proves Libs.private's CONTENT, EMISSION
# AND load-bearing NECESSITY (adversarial review, PKG-DIST achado 4;
# see the file-level comment for the full reasoning and the trade-off
# accepted). The ONE scenario that genuinely needs its OWN, separate
# build (PERF-PKGCONFIG): BUILD_SHARED_LIBS=OFF changes what is
# actually compiled and linked, unlike every other scenario in this
# file.
run_static_scenario() {
    glintfx_src="$1"
    consumer_src="$2"
    cxx="$3"
    scratch="$4"
    build_dir="$5"

    prefix="${scratch}/prefix-static"
    configure_glintfx "$glintfx_src" "$build_dir" "$cxx" "$DEFAULT_LIBDIR" OFF "$INERT_PREFIX"
    build_and_install_glintfx "$build_dir" "$prefix"

    pkgconfig_dir="$(pkgconfig_dir_for "$prefix" "$DEFAULT_LIBDIR")"
    assert_pkgconfig_finds_glintfx "$pkgconfig_dir"
    assert_pkgconfig_output_resolves_to_real_content "$pkgconfig_dir" --static

    binary="${scratch}/consumer-static"
    compile_and_run_static_consumer "$pkgconfig_dir" "$consumer_src" "$cxx" "$binary"

    probe_src="$(write_wayland_symbol_probe_source "$scratch")"
    probe_negative_bin="${scratch}/probe-without-libsprivate"
    probe_positive_bin="${scratch}/probe-with-libsprivate"
    assert_probe_links_without_wayland_client_fails "$pkgconfig_dir" "$probe_src" "$cxx" "$probe_negative_bin"
    assert_probe_links_with_wayland_client_succeeds "$pkgconfig_dir" "$probe_src" "$cxx" "$probe_positive_bin"

    echo "ok: static scenario - pkg-config --libs --static glintfx carries the correct Libs.private content (-lwayland-client), it is emitted correctly, AND it is genuinely load-bearing: a probe that forces extraction of the Wayland-bound archive member fails to link without Libs.private and succeeds with it."
}

# Scenario 6: DESTDIR, Fedora format (adversarial review round 3, task
# 2). See configure_glintfx_fedora_style/build_and_install_glintfx_with_destdir
# above for the full reasoning; this is the concrete test of the
# "indecidivel no configure" finding that changed the whole strategy
# for this round. Runs FIRST (PERF-PKGCONFIG, see the file-level
# comment): it is the only scenario relying on CMAKE_INSTALL_LIBDIR
# being genuinely unset, so it must own the shared build dir's first,
# virgin configure, before any other scenario ever sets that variable
# explicitly on the same cache.
run_destdir_scenario() {
    glintfx_src="$1"
    consumer_src="$2"
    cxx="$3"
    scratch="$4"
    build_dir="$5"

    destdir="${scratch}/buildroot"
    configure_glintfx_fedora_style "$glintfx_src" "$build_dir" "$cxx"
    build_and_install_glintfx_with_destdir "$build_dir" "$destdir"

    pkgconfig_dir="$(find_pkgconfig_dir_under_destdir "$destdir")"
    assert_pkgconfig_finds_glintfx "$pkgconfig_dir"
    assert_pkgconfig_output_resolves_to_real_content "$pkgconfig_dir"

    binary="${scratch}/consumer-destdir"
    libdir_abs="$(PKG_CONFIG_PATH="$pkgconfig_dir" pkg-config --variable=libdir glintfx)"
    compile_and_run_dynamic_consumer "$pkgconfig_dir" "$consumer_src" "$cxx" "$libdir_abs" "$binary"
    echo "ok: DESTDIR scenario - the Fedora RPM packaging format (prefix=/usr at configure, DESTDIR at install, no CMAKE_INSTALL_LIBDIR override) resolves and links via pkg-config alone, validated from inside the staging tree."
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

    # PERF-PKGCONFIG: ONE shared build dir for the five scenarios that
    # only ever vary install-time layout, reused via reconfigure
    # (destdir MUST run first - see run_destdir_scenario's own
    # comment), and one SEPARATE build dir for the static scenario,
    # which genuinely needs a different compiled artifact. Two full
    # builds total, not six.
    shared_build_dir="${scratch}/build-shared"
    static_build_dir="${scratch}/build-static"

    run_destdir_scenario "$glintfx_src" "$consumer_src" "$cxx" "$scratch" "$shared_build_dir"
    run_default_layout_scenario "$glintfx_src" "$consumer_src" "$cxx" "$scratch" "$shared_build_dir"
    run_multiarch_layout_scenario "$glintfx_src" "$consumer_src" "$cxx" "$scratch" "$shared_build_dir"
    run_malformed_libdir_scenario "$glintfx_src" "$consumer_src" "$cxx" "$scratch" "$shared_build_dir"
    run_absolute_dirs_scenario "$glintfx_src" "$consumer_src" "$cxx" "$scratch" "$shared_build_dir"
    run_static_scenario "$glintfx_src" "$consumer_src" "$cxx" "$scratch" "$static_build_dir"

    echo "ok: glintfx.pc resolves via pkg-config alone (no CMake, no find_package) in default layout, Debian-multiarch layout, a malformed (trailing slash + doubled separator + leading ./) layout, absolute install dirs, static mode, and a DESTDIR-staged Fedora-format install."
}

main "$@"
