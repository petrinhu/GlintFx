#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
# check_pkgconfig_validate.sh - proves the PKG-VALIDATE install(CODE)
# step (cmake/GlintfxPkgConfigValidate.cmake,
# cmake/GlintfxPkgConfigValidateInstalled.cmake.in) actually runs on
# `cmake --install`, actually reaches real broken layouts with a
# self-sufficient diagnostic, and actually skips via BOTH halves of
# its escape hatch - as opposed to tests/tools/check_pkgconfig.sh
# (PKG-DIST), which proves glintfx.pc's CONTENT is correct across six
# layouts OUR OWN CI enumerates. This script proves the DIFFERENT
# claim: that a validator riding along inside every real
# `cmake --install`, on a packager's machine we will never see, fires
# when it should, stays silent when it should, and never blocks an
# install for a reason its own message cannot explain unaided
# (GODS_LAWS.md LEI ZERO, PKG-VALIDATE's own scope in TODO.md).
#
#   1. "default layout, real end-to-end install": ordinary `cmake
#      --install --prefix <scratch>`, no flags. Proves the mechanism
#      is wired: install(CODE) fires, finds pkg-config, and reports
#      success against a genuinely correct install - this is the
#      GREEN half of GODS_LAWS.md L-20, and the only one of the eight
#      scenarios below that exercises the FULL real pipeline glintfx's
#      own build produces (configure -> build -> install), not a
#      hand-assembled fixture.
#   2. "DESTDIR, Fedora format": CMAKE_INSTALL_PREFIX=/usr at
#      configure, DESTDIR=<staging> at install, no CMAKE_INSTALL_LIBDIR
#      override - the format check_pkgconfig.sh's own run_destdir_scenario
#      already proves for glintfx.pc's CONTENT; this proves the
#      SEPARATE claim that the validator itself resolves the correct,
#      STAGED, DESTDIR-aware physical location and does not either
#      false-fail (looking outside the staging root) or false-pass
#      (never actually looking).
#   3. "configure-time escape hatch": -DGLINTFX_SKIP_PKGCONFIG_VALIDATION=ON.
#      Proves the install(CODE) step is never even REGISTERED - not
#      merely a validator that runs and silently no-ops, an install
#      log with NO trace of it running at all.
#   4. "install-time escape hatch": GLINTFX_SKIP_PKGCONFIG_VALIDATION=1
#      in the environment of a `cmake --install` invocation against a
#      build NOT configured with the hatch above. Proves the second,
#      independent half of the escape hatch - the one a packaging
#      pipeline that reuses one configured build directory across
#      several install invocations needs (see
#      GlintfxPkgConfigValidateInstalled.cmake.in's own file header).
#   5. "broken install, library artifact removed" (GODS_LAWS.md L-20's
#      RED, with REAL error output): a genuine install, then the
#      exact "partially-removed install" PACKAGING.md itself already
#      names as the reason `pkg-config --exists` alone is not enough -
#      the compiled library artifact deleted after a real install
#      wrote it, the validator re-run directly against the resulting,
#      genuinely broken tree. Not a simulation: this is the real
#      failure mode the whole fatia exists to catch, reproduced live.
#   6. "empty Cflags/Libs floor" (GODS_LAWS.md L-40): a hand-assembled
#      fixture with CORRECT variable=includedir/libdir but BLANK
#      Cflags:/Libs: lines - the exact defect class named in
#      GODS_LAWS.md L-40's own case table ("um arquivo com as
#      variaveis certas e as linhas de flag vazias imprimia
#      'passou'"). Proves the floor this validator's own
#      glintfx_pkgconfig_validate_flags() function carries actually
#      fires, not merely that it is written.
#   7. "missing glintfx.pc" (RED): a real install, then glintfx.pc
#      itself deleted (not the library) before re-running the
#      validator directly - the OTHER half of "partially-removed
#      install", and a distinct FATAL_ERROR branch
#      (glintfx_validate_installed_pkgconfig's own EXISTS check) from
#      scenario 5's.
#   8. "pkg-config absent" (WARNING, not FATAL - declared downgrade):
#      a real install, re-validated with find_program() restricted to
#      an empty root via CMAKE_FIND_ROOT_PATH_MODE_PROGRAM=ONLY (the
#      same, legitimate CMake mechanism cross-compiling toolchain
#      files use to sandbox find_program lookups to a sysroot - not a
#      destructive edit of this machine's real pkg-config). Proves a
#      packager's machine with no pkg-config on PATH gets a WARNING
#      and a successful install, never a FATAL_ERROR for something
#      that is not glintfx's own defect.
#
# What this script does NOT test, declared (GODS_LAWS.md L-27): the
# includedir/headers-missing branch of
# glintfx_pkgconfig_validate_variable() and the -I-token-content branch
# of glintfx_pkgconfig_validate_flags() are the SAME function/loop as
# the libdir/library branch scenario 5 already exercises, parameterized
# only by expected_content_kind - proving the library branch proves the
# mechanism; the headers branch is not independently re-proven here.
# Component-scoped installs (`cmake --install --component X`) and
# cross-compiled TARGET binaries are not exercised either: neither one
# is reachable through glintfx's own install() rules today (none of
# them declare a COMPONENT, and this validator never runs target-arch
# code) - see GlintfxPkgConfigValidateInstalled.cmake.in's own file
# header for why those are declared, not silently assumed, safe.
#
# Usage: check_pkgconfig_validate.sh <glintfx-source-dir> <cxx-compiler>
#
# Each function below does one thing (GODS_LAWS.md L-17).

set -eu

fail() {
    echo "check_pkgconfig_validate.sh: $1" >&2
    exit 1
}

require_args() {
    [ "$#" -eq 2 ] || fail "usage: check_pkgconfig_validate.sh <glintfx-source-dir> <cxx-compiler>"
    [ -d "$1" ] || fail "glintfx source dir not found: $1"
    [ -n "$2" ] || fail "cxx-compiler argument is empty"
}

make_scratch_workdir() {
    mktemp -d "${TMPDIR:-/tmp}/glintfx-pkgvalidate-XXXXXX"
}

configure_glintfx() {
    glintfx_src="$1"
    build_dir="$2"
    cxx="$3"
    shift 3
    # $@ carries any EXTRA -D flags a scenario needs (the escape
    # hatch's cache option, a non-default CMAKE_INSTALL_PREFIX, ...) -
    # intentional word-splitting of caller-provided -D tokens, same
    # reasoning as every other configure wrapper in this test suite.
    cmake -S "$glintfx_src" -B "$build_dir" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_COMPILER="$cxx" \
        -DGLINTFX_BUILD_TESTS=OFF \
        "$@"
}

build_glintfx() {
    build_dir="$1"
    cmake --build "$build_dir"
}

# Locates the .pc file this validator's own resolution logic would
# find, by SEARCHING the install tree rather than predicting the
# libdir - same "validate the output, do not predict the input"
# discipline check_pkgconfig.sh's own find_pkgconfig_dir_under_destdir
# already applies.
find_pc_file_under() {
    root="$1"
    found="$(find "$root" -name glintfx.pc 2>/dev/null | head -n1)"
    [ -n "$found" ] || fail "no glintfx.pc found anywhere under ${root} after install"
    printf '%s' "$found"
}

# Reads back the ACTUAL libdir subdirectory (e.g. "lib", "lib64",
# "lib/x86_64-linux-gnu") a real install used, relative to its prefix -
# found by SEARCHING for the pkgconfig/ directory a real install just
# wrote, never predicted from GNUInstallDirs' own per-distro default
# (which differs between this machine and the CI matrix - "lib64" on
# Fedora, "lib" on many other Linux distros). Scenario 6 needs this to
# place its hand-assembled fixture under the SAME subdirectory the
# validator (baked with THIS build's own CMAKE_INSTALL_LIBDIR at
# configure time) will actually look under - a fixture placed under a
# hardcoded "lib" would silently miss on any machine whose real
# default is "lib64" and prove nothing.
find_libdir_relative_to_prefix() {
    prefix="$1"
    pkgconfig_dir="$(find "$prefix" -type d -name pkgconfig 2>/dev/null | head -n1)"
    [ -n "$pkgconfig_dir" ] || fail "no pkgconfig/ directory found anywhere under ${prefix} - cannot derive the real libdir subdirectory"
    libdir_abs="$(dirname "$pkgconfig_dir")"
    case "$libdir_abs" in
        "$prefix"/*) printf '%s' "${libdir_abs#"$prefix"/}" ;;
        *) fail "pkgconfig/ directory ${pkgconfig_dir} is not under prefix ${prefix} - cannot derive a relative libdir" ;;
    esac
}

find_generated_validator_script() {
    build_dir="$1"
    script="${build_dir}/GlintfxPkgConfigValidateInstalled.cmake"
    [ -f "$script" ] || fail "GlintfxPkgConfigValidateInstalled.cmake not found at ${script} - glintfx_register_pkgconfig_validation() did not generate it (was GLINTFX_SKIP_PKGCONFIG_VALIDATION=ON at configure time for this build?)"
    printf '%s' "$script"
}

# Scenario 1: default layout, real end-to-end install.
run_default_layout_scenario() {
    glintfx_src="$1"
    cxx="$2"
    scratch="$3"

    build_dir="${scratch}/build-default"
    prefix="${scratch}/prefix-default"
    configure_glintfx "$glintfx_src" "$build_dir" "$cxx"
    build_glintfx "$build_dir"

    output="$(cmake --install "$build_dir" --prefix "$prefix" 2>&1)" \
        || fail "default-layout install unexpectedly FAILED:
${output}"

    case "$output" in
        *"post-install pkg-config validation passed"*) : ;;
        *) fail "default-layout install succeeded, but never printed the validator's own success message. Got:
${output}" ;;
    esac

    find_pc_file_under "$prefix" >/dev/null
    echo "ok: default-layout scenario - the install(CODE) validator ran and reported success against a genuinely correct install."
}

# Scenario 2: DESTDIR, Fedora format - see check_pkgconfig.sh's own
# run_destdir_scenario for the packaging-format reasoning; this proves
# the validator's OWN staged-path resolution, not glintfx.pc's content.
run_destdir_scenario() {
    glintfx_src="$1"
    cxx="$2"
    scratch="$3"

    build_dir="${scratch}/build-destdir"
    destdir="${scratch}/buildroot"
    configure_glintfx "$glintfx_src" "$build_dir" "$cxx" -DCMAKE_INSTALL_PREFIX=/usr
    build_glintfx "$build_dir"

    output="$(DESTDIR="$destdir" cmake --install "$build_dir" 2>&1)" \
        || fail "DESTDIR install unexpectedly FAILED:
${output}"

    case "$output" in
        *"post-install pkg-config validation passed"*) : ;;
        *) fail "DESTDIR install succeeded, but never printed the validator's own success message. Got:
${output}" ;;
    esac

    pc_file="$(find_pc_file_under "$destdir")"
    case "$output" in
        *"$pc_file"*) : ;;
        *) fail "DESTDIR install's success message did not name the STAGED pc file path (${pc_file}) - the validator may have resolved a path outside the staging root and passed for the wrong reason. Got:
${output}" ;;
    esac
    echo "ok: DESTDIR scenario - the validator resolved the STAGED, DESTDIR-aware physical location (${pc_file}) and reported success against it."
}

# Scenario 3: configure-time escape hatch. Proves the install(CODE)
# step is never REGISTERED at all - checked by its absence from the
# INSTALL log, not merely a "skipped" message from a step that still
# ran.
run_configure_time_hatch_scenario() {
    glintfx_src="$1"
    cxx="$2"
    scratch="$3"

    build_dir="${scratch}/build-hatch-configure"
    prefix="${scratch}/prefix-hatch-configure"
    configure_output="$(configure_glintfx "$glintfx_src" "$build_dir" "$cxx" -DGLINTFX_SKIP_PKGCONFIG_VALIDATION=ON 2>&1)"
    case "$configure_output" in
        *"post-install pkg-config validation disabled at configure time"*) : ;;
        *) fail "configuring with -DGLINTFX_SKIP_PKGCONFIG_VALIDATION=ON did not print the expected configure-time disable message. Got:
${configure_output}" ;;
    esac

    build_glintfx "$build_dir"
    install_output="$(cmake --install "$build_dir" --prefix "$prefix" 2>&1)" \
        || fail "install with the configure-time hatch set unexpectedly FAILED:
${install_output}"

    case "$install_output" in
        *"pkg-config validation"*)
            fail "install with -DGLINTFX_SKIP_PKGCONFIG_VALIDATION=ON at configure time still printed a pkg-config-validation message during INSTALL - the step should never have been registered at all. Got:
${install_output}"
            ;;
        *) : ;;
    esac
    find_pc_file_under "$prefix" >/dev/null
    echo "ok: configure-time escape hatch - glintfx.pc still installs correctly, and the validation install(CODE) step is never registered."
}

# Scenario 4: install-time escape hatch, against a build NOT
# configured with the hatch above - proves this is a genuinely
# SEPARATE mechanism, read fresh at install time.
run_install_time_hatch_scenario() {
    glintfx_src="$1"
    cxx="$2"
    scratch="$3"

    build_dir="${scratch}/build-hatch-install"
    prefix="${scratch}/prefix-hatch-install"
    configure_glintfx "$glintfx_src" "$build_dir" "$cxx"
    build_glintfx "$build_dir"

    output="$(GLINTFX_SKIP_PKGCONFIG_VALIDATION=1 cmake --install "$build_dir" --prefix "$prefix" 2>&1)" \
        || fail "install with GLINTFX_SKIP_PKGCONFIG_VALIDATION=1 in the environment unexpectedly FAILED:
${output}"

    case "$output" in
        *"post-install pkg-config validation skipped"*) : ;;
        *) fail "install with GLINTFX_SKIP_PKGCONFIG_VALIDATION=1 did not print the expected install-time skip message. Got:
${output}" ;;
    esac
    case "$output" in
        *"post-install pkg-config validation passed"*)
            fail "install with the install-time hatch set still printed the validator's SUCCESS message - it ran the real checks instead of skipping them. Got:
${output}"
            ;;
        *) : ;;
    esac
    find_pc_file_under "$prefix" >/dev/null
    echo "ok: install-time escape hatch - a build configured WITHOUT the configure-time hatch still skips validation when GLINTFX_SKIP_PKGCONFIG_VALIDATION=1 is set in the install environment."
}

# Scenario 5: broken install, library artifact removed - GODS_LAWS.md
# L-20's RED, with real, captured error output. Reuses the build from
# scenario 1's own default-layout install (a SEPARATE, throwaway
# prefix here) precisely so the corruption is applied to files a real
# `cmake --install` actually wrote, not a hand-assembled fixture -
# PACKAGING.md's own "partially-removed install" example, reproduced
# live.
run_broken_library_scenario() {
    glintfx_src="$1"
    cxx="$2"
    scratch="$3"

    build_dir="${scratch}/build-broken-library"
    prefix="${scratch}/prefix-broken-library"
    configure_glintfx "$glintfx_src" "$build_dir" "$cxx"
    build_glintfx "$build_dir"
    cmake --install "$build_dir" --prefix "$prefix" >/dev/null

    validator_script="$(find_generated_validator_script "$build_dir")"
    find "$prefix" -maxdepth 3 -name 'libglintfx.so*' -o -maxdepth 3 -name 'libglintfx.a' 2>/dev/null \
        | while IFS= read -r artifact; do rm -f "$artifact"; done

    output="$(cmake -DCMAKE_INSTALL_PREFIX="$prefix" -P "$validator_script" 2>&1)" \
        && fail "re-running the validator against a real install with its library artifact REMOVED unexpectedly SUCCEEDED - it did not catch the broken install. Got:
${output}"

    case "$output" in
        *"libglintfx.so"*"libglintfx.a"*"is not there"*) : ;;
        *) fail "the broken-library RED did not name the missing artifact by its own promise ('libglintfx.so*/libglintfx.a ... is not there'). Got:
${output}" ;;
    esac
    case "$output" in
        *"GLINTFX_SKIP_PKGCONFIG_VALIDATION=1"*) : ;;
        *) fail "the broken-library RED message did not mention the escape hatch (GLINTFX_SKIP_PKGCONFIG_VALIDATION=1) - a real packager hitting this needs to know how to skip it without reading this repository. Got:
${output}" ;;
    esac
    echo "ok: broken-library RED - the validator, re-run directly against a real install with its library artifact deleted, fails with a self-sufficient diagnostic naming the missing artifact and the escape hatch."
}

# Scenario 6: empty Cflags/Libs floor (GODS_LAWS.md L-40) - a
# hand-assembled fixture, because provoking THIS specific defect
# through glintfx's own, already-correct CMake code is not possible
# (the code never emits a blank Cflags:/Libs: line); the fixture
# reproduces exactly the shape L-40's own case table names: correct
# variable=includedir/libdir, blank flag lines.
run_empty_flags_floor_scenario() {
    build_dir="$1"
    scratch="$2"
    real_libdir="$3"

    validator_script="$(find_generated_validator_script "$build_dir")"

    fixture_prefix="${scratch}/prefix-floor-fixture"
    mkdir -p "${fixture_prefix}/${real_libdir}/pkgconfig" "${fixture_prefix}/include/glintfx"
    : > "${fixture_prefix}/${real_libdir}/libglintfx.so.0.1.0.0"
    ln -s libglintfx.so.0.1.0.0 "${fixture_prefix}/${real_libdir}/libglintfx.so"
    cat > "${fixture_prefix}/${real_libdir}/pkgconfig/glintfx.pc" << EOF
prefix=${fixture_prefix}
libdir=\${prefix}/${real_libdir}
includedir=\${prefix}/include

Name: glintfx
Description: check_pkgconfig_validate.sh L-40 floor fixture - correct variables, blank flag lines
Version: 0.1.0.0
Libs:
Cflags:
EOF

    output="$(cmake -DCMAKE_INSTALL_PREFIX="$fixture_prefix" -P "$validator_script" 2>&1)" \
        && fail "re-running the validator against a fixture with CORRECT variables but BLANK Cflags:/Libs: lines unexpectedly SUCCEEDED - the L-40 floor did not fire. Got:
${output}"

    case "$output" in
        *"ZERO -I flags"*"GODS_LAWS.md L-40"*) : ;;
        *) fail "the empty-flags RED did not name the L-40 floor by its own promise ('ZERO -I flags ... GODS_LAWS.md L-40'). Got:
${output}" ;;
    esac
    echo "ok: empty-Cflags/Libs floor RED - a fixture with correct includedir/libdir but blank flag lines is REPROVED, not silently accepted (GODS_LAWS.md L-40)."
}

# Scenario 7: missing glintfx.pc itself - the OTHER half of
# "partially-removed install", and a distinct FATAL_ERROR branch
# (the top-level EXISTS check in
# glintfx_validate_installed_pkgconfig()) from scenario 5's.
run_missing_pc_file_scenario() {
    glintfx_src="$1"
    cxx="$2"
    scratch="$3"

    build_dir="${scratch}/build-missing-pc"
    prefix="${scratch}/prefix-missing-pc"
    configure_glintfx "$glintfx_src" "$build_dir" "$cxx"
    build_glintfx "$build_dir"
    cmake --install "$build_dir" --prefix "$prefix" >/dev/null

    validator_script="$(find_generated_validator_script "$build_dir")"
    pc_file="$(find_pc_file_under "$prefix")"
    rm -f "$pc_file"

    output="$(cmake -DCMAKE_INSTALL_PREFIX="$prefix" -P "$validator_script" 2>&1)" \
        && fail "re-running the validator against a real install with glintfx.pc itself REMOVED unexpectedly SUCCEEDED. Got:
${output}"

    case "$output" in
        *"right after installing it, and it is not there"*) : ;;
        *) fail "the missing-pc-file RED did not use its own promised wording. Got:
${output}" ;;
    esac
    case "$output" in
        *"GLINTFX_SKIP_PKGCONFIG_VALIDATION"*) : ;;
        *) fail "the missing-pc-file RED message did not mention the escape hatch. Got:
${output}" ;;
    esac
    echo "ok: missing-glintfx.pc RED - the validator, re-run against a real install with glintfx.pc itself deleted, fails with a self-sufficient diagnostic."
}

# Scenario 8: pkg-config absent (WARNING, not FATAL - declared
# downgrade). CMAKE_FIND_ROOT_PATH_MODE_PROGRAM=ONLY restricts
# find_program() to an EMPTY, throwaway root - the same legitimate
# CMake mechanism cross-compiling toolchain files use to sandbox
# find_program lookups to a sysroot, never a destructive edit of this
# machine's real, installed pkg-config.
run_pkgconfig_absent_scenario() {
    build_dir="$1"
    scratch="$2"
    prefix_from_default="$3"

    validator_script="$(find_generated_validator_script "$build_dir")"
    empty_root="${scratch}/empty-find-root"
    mkdir -p "$empty_root"

    output="$(cmake -DCMAKE_INSTALL_PREFIX="$prefix_from_default" \
        -DCMAKE_FIND_ROOT_PATH="$empty_root" \
        -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=ONLY \
        -P "$validator_script" 2>&1)" \
        || fail "re-running the validator with find_program() sandboxed to an empty root (simulating a machine with no pkg-config on PATH) unexpectedly FAILED instead of warning-and-skipping. Got:
${output}"

    case "$output" in
        *"pkg-config (and pkgconf) not found on PATH"*) : ;;
        *) fail "the pkg-config-absent scenario did not print the expected warning. Got:
${output}" ;;
    esac
    echo "ok: pkg-config-absent scenario - a machine with no pkg-config/pkgconf on PATH gets a WARNING and a SUCCESSFUL (exit 0) validation step, never a FATAL_ERROR for a missing tool that is not glintfx's own defect."
}

main() {
    require_args "$@"
    glintfx_src="$1"
    cxx="$2"

    scratch="$(make_scratch_workdir)"
    trap 'rm -rf "$scratch"' EXIT

    run_default_layout_scenario "$glintfx_src" "$cxx" "$scratch"
    run_destdir_scenario "$glintfx_src" "$cxx" "$scratch"
    run_configure_time_hatch_scenario "$glintfx_src" "$cxx" "$scratch"
    run_install_time_hatch_scenario "$glintfx_src" "$cxx" "$scratch"
    run_broken_library_scenario "$glintfx_src" "$cxx" "$scratch"
    run_missing_pc_file_scenario "$glintfx_src" "$cxx" "$scratch"

    # Scenarios 6 and 8 reuse a THIRD, dedicated default-layout build
    # (not scenario 1's or 5's - both of those either finish clean or
    # get their own artifacts deleted by the time these two would run)
    # purely to obtain a fresh GlintfxPkgConfigValidateInstalled.cmake
    # and, for scenario 8, an intact install to validate.
    shared_build_dir="${scratch}/build-shared-fixtures"
    shared_prefix="${scratch}/prefix-shared-fixtures"
    configure_glintfx "$glintfx_src" "$shared_build_dir" "$cxx"
    build_glintfx "$shared_build_dir"
    cmake --install "$shared_build_dir" --prefix "$shared_prefix" >/dev/null
    real_libdir="$(find_libdir_relative_to_prefix "$shared_prefix")"

    run_empty_flags_floor_scenario "$shared_build_dir" "$scratch" "$real_libdir"
    run_pkgconfig_absent_scenario "$shared_build_dir" "$scratch" "$shared_prefix"

    echo "ok: the PKG-VALIDATE install(CODE) step runs on real installs (default layout, DESTDIR), honors both halves of its escape hatch, and fails closed with a self-sufficient diagnostic on a real broken library artifact, a real missing glintfx.pc, and a hand-assembled empty-Cflags/Libs (L-40) fixture, while degrading to a WARNING (not a FATAL_ERROR) when pkg-config itself is absent."
}

main "$@"
