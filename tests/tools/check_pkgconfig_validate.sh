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
#      GREEN half of GODS_LAWS.md L-20, and the only one of the thirteen
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
#      glintfx_pkgconfig_validate_content_flags() function carries actually
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
#   9. "Windows-forced broken install still REFUSES" (RED, PKG-WIN-SCOPE
#      regression proof, 27/08/2026): the SAME real, genuine
#      broken-library install as scenario 5, re-validated with
#      "-DWIN32=1" (script mode has no real target platform to force;
#      this override is confirmed live to take the WIN32 branch, see
#      GlintfxPkgConfigValidateInstalled.cmake.in's own header). Before
#      this fatia's fix, this exact scenario - reproduced live by
#      adversarial review, 27/08/2026 - printed a WARNING at exit 0
#      claiming "Filesystem-based checks already confirmed ... the
#      library artifact ... genuinely on disk", while the artifact had
#      just been deleted: the WIN32 branch used to run BEFORE the
#      filesystem/text-only content check, so one pkg-config-binary
#      conversation failure (for ANY reason) skipped that check
#      entirely and still reported success. This scenario is the
#      regression test: it must FATAL, closed, on the forced-Windows
#      path, exactly as it already does unforced (scenario 5).
#  10. "Windows-forced, healthy content, binary conversation itself
#      fails" (WARNING, not FATAL - the one legitimate downgrade that
#      survives this fatia): reuses scenario 1's own intact install
#      unmodified, re-validated with "-DWIN32=1". Forcing WIN32 makes
#      this file append a literal trailing native list separator (";")
#      to PKG_CONFIG_PATH (PKG-VALIDATE-WINSEP) - and the REAL
#      pkg-config on THIS test machine's PATH is a genuine POSIX
#      implementation that does not treat ';' as a separator, so it
#      genuinely fails to find glintfx via `--exists`, for a reason
#      that has nothing to do with content. Proves the WARNING branch
#      still exists, still exits 0, and - the whole point of this
#      fatia - now truthfully names its OWN, already-run content check
#      as the thing that verified the filesystem, never a check that
#      has not executed yet.
#  11. "headers tree missing, on both a real Unix run and a
#      Windows-forced run" (RED on both): a real install with its
#      ENTIRE installed `glintfx/` header subdirectory removed (leaving
#      `includedir` a real, existing, but now genuinely EMPTY
#      directory) - the "árvore de cabeçalhos"/"um diretório vazio"
#      case this file's own header used to declare NOT independently
#      re-proven (only the library/-L branch, parameterized the same
#      way, was exercised). Proves the includedir branch of
#      glintfx_pkgconfig_validate_content_variable() fires for real,
#      and that forcing WIN32 changes nothing about it: content checks
#      never depended on platform.
#  12. "relative libdir, attacker-controlled CWD, still REFUSES" (RED,
#      PKG-WIN-SCOPE adversarial review round 5 regression proof,
#      27/08/2026): a real install, its library artifact deleted (the
#      SAME genuinely-broken precondition as scenario 5), THEN
#      glintfx.pc's own libdir= line rewritten to a bare RELATIVE value
#      that does not go through "${pcfiledir}" - and the validator
#      invoked with its CURRENT WORKING DIRECTORY set to an unrelated
#      "attacker" directory that happens to contain a decoy file
#      matching the exact glob this validator looks for. Before this
#      fatia's fix, cmake_path(ABSOLUTE_PATH ...) with no BASE_DIRECTORY
#      resolved that relative value against CMAKE_CURRENT_SOURCE_DIR -
#      which in script mode equals the CALLER's CWD, not anything this
#      file controls - so the decoy satisfied the check and a genuinely
#      broken install (its real library deleted) passed at EXIT=0. This
#      scenario is the regression test: it must FATAL, closed, naming a
#      path anchored under glintfx.pc's OWN directory (pcfiledir), and
#      must NOT name the attacker directory anywhere in its message -
#      the second assertion is what tells a future regression (CWD
#      leaking back in) apart from an unrelated failure.
#  13. "real pkg-config's own variable syntax accepted, all three at
#      once" (GREEN, PKG-WIN-SCOPE round 5): a single hand-assembled
#      glintfx.pc where "exec_prefix"'s definition uses whitespace
#      around "=" AND carries a trailing "# ..." comment, and "libdir"
#      is defined TWICE, first pointing at a directory that does not
#      exist, then correctly - a shape chosen so the fixture can only
#      resolve to real, populated content if whitespace-around-"=",
#      comment-stripping, AND last-definition-wins are ALL honored at
#      once (get any one wrong and either "exec_prefix" never resolves,
#      corrupting "libdir" downstream, or "libdir" resolves to the
#      first, nonexistent definition). Cross-checked against a real
#      pkg-config/pkgconf binary on this machine's PATH, when one is
#      available, confirming the SAME fixture is genuinely good by an
#      implementation this project does not control - not merely
#      "passes because our own code says so".
#
# What this script does NOT test, declared (GODS_LAWS.md L-27):
# component-scoped installs (`cmake --install --component X`) and
# cross-compiled TARGET binaries: neither one is reachable through
# glintfx's own install() rules today (none of them declare a
# COMPONENT, and this validator never runs target-arch code) - see
# GlintfxPkgConfigValidateInstalled.cmake.in's own file header for why
# those are declared, not silently assumed, safe. Scenarios 9 and 10
# force WIN32 via "-DWIN32=1" rather than running on a real Windows
# machine - the SAME, declared limitation every other scenario in this
# POSIX shell script already has; PACKAGING.md's own Windows section is
# additionally, separately proven on real Windows CI by
# tools/ci/check-pkgconfig-installed.ps1 (see that file, and
# PACKAGING.md's "Packaging on Windows" section).
#
# PERF-PKGVALIDATE (27/08/2026): this file used to give EVERY scenario
# its own fresh `cmake -S <src> -B <fresh-dir>` PLUS a full
# `cmake --build` of the whole library - seven independent from-scratch
# compiles for a single ctest case. Measured live before this change:
# 103-111s wall clock on a fast, otherwise-idle 16-core desktop, and a
# flat 120s DART_TESTING_TIMEOUT (cmake/GlintfxTest.cmake) timeout on
# three of the four Linux CI matrix targets, with the fourth (Fedora,
# the primary target) failing at 115.95s - CI machines are slower and
# often shared, so the margin was already gone before this fatia's own
# validation logic is even considered. Measured SEPARATELY, and this is
# the fact that rules out "the validator itself is what got slower":
# a single `cmake --install` with the validator ON costs ~0.09s versus
# ~0.01s with it fully OFF (GLINTFX_SKIP_PKGCONFIG_VALIDATION=1) on this
# same machine - a ~80ms tax per install, not the multi-second one the
# timeout would need. The real cost was structural: NONE of this file's
# thirteen scenarios ever vary BUILD_SHARED_LIBS or CMAKE_INSTALL_LIBDIR
# (unlike check_pkgconfig.sh's sibling gate, which genuinely needs a
# different compiled artifact for its static scenario) - every
# scenario here only varies CMAKE_INSTALL_PREFIX and
# GLINTFX_SKIP_PKGCONFIG_VALIDATION, BOTH configure-time cache
# variables that do not touch a single compiled object file. Confirmed
# live: reconfiguring an already-built tree with either variable
# changed, then rebuilding, prints "ninja: no work to do" and costs
# ~0.08s, not a recompile. So this file now configures and builds
# glintfx exactly ONCE, and every scenario that needs a different
# CMAKE_INSTALL_PREFIX or GLINTFX_SKIP_PKGCONFIG_VALIDATION value
# reconfigures that SAME build directory (cheap) instead of starting a
# new one (expensive) - the claim proven by each scenario, and its own
# assertions, are UNCHANGED; only how the build each one installs FROM
# gets there changed. Scenarios 6 and 8 no longer need their own
# "third, dedicated" build either: they now reuse scenario 1's own
# intact default-layout install and the one shared build's own
# generated validator script, since neither one is ever mutated by any
# scenario that runs before them.
#
# Usage: check_pkgconfig_validate.sh <glintfx-source-dir> <cxx-compiler>
#
# Each function below does one thing (GODS_LAWS.md L-17).

set -eu

fail() {
    echo "check_pkgconfig_validate.sh: $1" >&2
    exit 1
}

# PKG-VALIDATE-WRAP: undoes CMake's own message() text-wrapping so a
# `case` pattern that searches for a MULTI-WORD phrase does not miss it
# just because the wrap happened to land between two of that phrase's
# words - the same defect CLASS this house already named for PDF text
# extraction ("grep de frase quebra entre linhas; normalize antes de
# contar"), here caused by cmake's own message() reflow instead of an
# extractor. REPRODUCED live before this fix, not merely reasoned: an
# unnormalized `case "$output"` search for "is not there"
# (run_broken_library_scenario's own RED assertion, below) matched for
# scratch-directory padding lengths 7+ and MISSED for lengths 0-6 -
# and mktemp's "XXXXXX" template always produces a 6-character random
# suffix, landing this scenario inside the miss range on every real
# run, passing only by the accident of a lucky suffix. Confirmed
# separately that cmake's message() wrapping never breaks INSIDE an
# unbroken run of non-whitespace (a path with no spaces stays intact
# on one line, however long) - only BETWEEN words - so collapsing
# every run of whitespace (newline included) back to one space is
# sufficient to reconstruct the original phrase; no word is ever
# split mid-token.
normalize_wrapped_message() {
    printf '%s' "$1" | tr '\n' ' ' | tr -s ' '
}

require_args() {
    [ "$#" -eq 2 ] || fail "usage: check_pkgconfig_validate.sh <glintfx-source-dir> <cxx-compiler>"
    [ -d "$1" ] || fail "glintfx source dir not found: $1"
    [ -n "$2" ] || fail "cxx-compiler argument is empty"
}

make_scratch_workdir() {
    mktemp -d "${TMPDIR:-/tmp}/glintfx-pkgvalidate-XXXXXX"
}

# Configures the ONE shared build directory this whole file reuses.
# ALWAYS passes both CMAKE_INSTALL_PREFIX and
# GLINTFX_SKIP_PKGCONFIG_VALIDATION explicitly, every single call
# (PERF-PKGVALIDATE) - never left to whatever the previous scenario's
# reconfigure happened to cache, so no scenario can silently inherit a
# stale value from the one before it. "/usr/local" is CMake's own
# built-in default prefix, spelled out here instead of omitted, for
# the same reason: every scenario that needs a specific prefix (only
# the DESTDIR one, scenario 2) says so explicitly, and every other
# scenario installs with its own `--prefix <scratch>` override anyway
# (confirmed live: `cmake --install <dir> --prefix <p>` overrides a
# CACHED CMAKE_INSTALL_PREFIX for that one invocation, regardless of
# what configure baked in), so this value is inert for them.
reconfigure_glintfx() {
    glintfx_src="$1"
    build_dir="$2"
    cxx="$3"
    prefix_value="$4"
    skip_validation="$5"
    cmake -S "$glintfx_src" -B "$build_dir" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_COMPILER="$cxx" \
        -DGLINTFX_BUILD_TESTS=OFF \
        -DCMAKE_INSTALL_PREFIX="$prefix_value" \
        -DGLINTFX_SKIP_PKGCONFIG_VALIDATION="$skip_validation"
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
# default is "lib64" and prove nothing. This value is the SAME across
# every scenario in this file (PERF-PKGVALIDATE: none of them ever set
# CMAKE_INSTALL_LIBDIR), so it is computed once, from scenario 1's own
# install, and reused rather than re-derived per scenario.
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

# Scenario 1: default layout, real end-to-end install. Runs against
# the shared build exactly as main() left it configured (prefix
# irrelevant - overridden below - and the escape hatch OFF), so no
# reconfigure of its own is needed.
run_default_layout_scenario() {
    build_dir="$1"
    prefix="$2"

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
# Needs CMAKE_INSTALL_PREFIX=/usr at CONFIGURE time (DESTDIR combines
# with whatever prefix is baked into cmake_install.cmake, not with a
# `--prefix` CLI override - confirmed live), so this is the one
# scenario in this file that reconfigures the shared build itself.
run_destdir_scenario() {
    glintfx_src="$1"
    cxx="$2"
    build_dir="$3"
    scratch="$4"

    reconfigure_glintfx "$glintfx_src" "$build_dir" "$cxx" "/usr" OFF >/dev/null
    build_glintfx "$build_dir" >/dev/null

    destdir="${scratch}/buildroot"
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
# ran. Reconfigures the shared build with the hatch ON; every scenario
# after this one that needs the hatch OFF again reconfigures back
# explicitly, never assuming this one already undid itself.
run_configure_time_hatch_scenario() {
    glintfx_src="$1"
    cxx="$2"
    build_dir="$3"
    prefix="$4"

    configure_output="$(reconfigure_glintfx "$glintfx_src" "$build_dir" "$cxx" "/usr/local" ON 2>&1)"
    case "$configure_output" in
        *"post-install pkg-config validation disabled at configure time"*) : ;;
        *) fail "configuring with -DGLINTFX_SKIP_PKGCONFIG_VALIDATION=ON did not print the expected configure-time disable message. Got:
${configure_output}" ;;
    esac

    build_glintfx "$build_dir" >/dev/null
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
# SEPARATE mechanism, read fresh at install time. Reconfigures the
# hatch back OFF itself (never assumes scenario 3 left it that way),
# because that is exactly the "NOT configured with the hatch"
# precondition this scenario's own claim depends on.
run_install_time_hatch_scenario() {
    glintfx_src="$1"
    cxx="$2"
    build_dir="$3"
    prefix="$4"

    reconfigure_glintfx "$glintfx_src" "$build_dir" "$cxx" "/usr/local" OFF >/dev/null
    build_glintfx "$build_dir" >/dev/null

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
# L-20's RED, with real, captured error output. Installs to its OWN
# throwaway prefix (never scenario 1's, and never reused afterward)
# precisely so the corruption is applied to files a real
# `cmake --install` actually wrote, not a hand-assembled fixture -
# PACKAGING.md's own "partially-removed install" example, reproduced
# live. Reconfigures the hatch OFF itself: this scenario's whole
# premise is a build the validator DOES run against, so it cannot
# inherit scenario 3's ON state by accident.
run_broken_library_scenario() {
    glintfx_src="$1"
    cxx="$2"
    build_dir="$3"
    prefix="$4"

    reconfigure_glintfx "$glintfx_src" "$build_dir" "$cxx" "/usr/local" OFF >/dev/null
    build_glintfx "$build_dir" >/dev/null
    cmake --install "$build_dir" --prefix "$prefix" >/dev/null

    validator_script="$(find_generated_validator_script "$build_dir")"
    find "$prefix" -maxdepth 3 -name 'libglintfx.so*' -o -maxdepth 3 -name 'libglintfx.a' 2>/dev/null \
        | while IFS= read -r artifact; do rm -f "$artifact"; done

    output="$(cmake -DCMAKE_INSTALL_PREFIX="$prefix" -P "$validator_script" 2>&1)" \
        && fail "re-running the validator against a real install with its library artifact REMOVED unexpectedly SUCCEEDED - it did not catch the broken install. Got:
${output}"

    normalized_output="$(normalize_wrapped_message "$output")"
    case "$normalized_output" in
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
# variable=includedir/libdir, blank flag lines. Needs only the shared
# build's own generated validator script and the real libdir subpath
# (both stable across the whole file, PERF-PKGVALIDATE) - no build or
# install of its own.
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
# glintfx_validate_installed_pkgconfig()) from scenario 5's. Own
# throwaway prefix and its own reconfigure-to-OFF, for the same
# self-sufficiency reason as scenario 5.
run_missing_pc_file_scenario() {
    glintfx_src="$1"
    cxx="$2"
    build_dir="$3"
    prefix="$4"

    reconfigure_glintfx "$glintfx_src" "$build_dir" "$cxx" "/usr/local" OFF >/dev/null
    build_glintfx "$build_dir" >/dev/null
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
# machine's real, installed pkg-config. Reuses scenario 1's own
# install (PERF-PKGVALIDATE): nothing between scenario 1 and this one
# ever mutates that prefix, so it is still exactly as intact as a
# fresh one would be.
run_pkgconfig_absent_scenario() {
    build_dir="$1"
    scratch="$2"
    intact_prefix="$3"

    validator_script="$(find_generated_validator_script "$build_dir")"
    empty_root="${scratch}/empty-find-root"
    mkdir -p "$empty_root"

    output="$(cmake -DCMAKE_INSTALL_PREFIX="$intact_prefix" \
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

# Scenario 9: PKG-WIN-SCOPE regression proof - the SAME real, genuine
# broken-library install as run_broken_library_scenario, re-validated
# with "-DWIN32=1" FORCING the Windows branch (see this file's header
# for why that override is trustworthy: confirmed live, not assumed).
# Own throwaway prefix and own reconfigure-to-OFF, for the same
# self-sufficiency reason as scenario 5.
run_windows_forced_broken_library_scenario() {
    glintfx_src="$1"
    cxx="$2"
    build_dir="$3"
    prefix="$4"

    reconfigure_glintfx "$glintfx_src" "$build_dir" "$cxx" "/usr/local" OFF >/dev/null
    build_glintfx "$build_dir" >/dev/null
    cmake --install "$build_dir" --prefix "$prefix" >/dev/null

    validator_script="$(find_generated_validator_script "$build_dir")"
    find "$prefix" -maxdepth 3 -name 'libglintfx.so*' -o -maxdepth 3 -name 'libglintfx.a' 2>/dev/null \
        | while IFS= read -r artifact; do rm -f "$artifact"; done

    output="$(cmake -DCMAKE_INSTALL_PREFIX="$prefix" -DWIN32=1 -P "$validator_script" 2>&1)" \
        && fail "re-running the validator against a real install with its library artifact REMOVED, with the Windows branch FORCED, unexpectedly SUCCEEDED - the PKG-WIN-SCOPE regression (a false 'already confirmed genuinely on disk' warning at exit 0) is back. Got:
${output}"

    normalized_output="$(normalize_wrapped_message "$output")"
    case "$normalized_output" in
        *"libglintfx.so"*"libglintfx.a"*"is not there"*) : ;;
        *) fail "the Windows-forced broken-library RED did not name the missing artifact by its own promise ('libglintfx.so*/libglintfx.a ... is not there'). Got:
${output}" ;;
    esac
    case "$output" in
        *"genuinely on disk"*)
            fail "the Windows-forced broken-library scenario still printed a claim that the library artifact is genuinely on disk, after deleting it - this is the exact false-verification defect PKG-WIN-SCOPE's adversarial review (27/08/2026) found and this fatia exists to fix. Got:
${output}"
            ;;
        *) : ;;
    esac
    echo "ok: Windows-forced broken-library RED - forcing the Windows branch no longer lets a real broken install (library artifact deleted) through with a false 'already confirmed genuinely on disk' warning; it FAILS, closed, naming the missing artifact, exactly as it already does unforced."
}

# Scenario 10: the ONE legitimate downgrade that survives this fatia -
# a GENUINELY intact install (reuses scenario 1's own, PERF-PKGVALIDATE:
# nothing between scenario 1 and this one ever mutates that prefix),
# re-validated with "-DWIN32=1". Forcing WIN32 makes this file append a
# literal trailing native list separator (";") to PKG_CONFIG_PATH
# (PKG-VALIDATE-WINSEP) - and the REAL pkg-config on THIS test
# machine's PATH is a genuine POSIX implementation that does not treat
# ';' as a separator, so `pkg-config --exists` genuinely fails to find
# glintfx, for a reason that has nothing to do with content (MEASURED
# live before writing this scenario, not assumed). Proves the WARNING
# branch still exists, still exits 0, and now truthfully names its OWN
# already-run content check as the thing that verified the filesystem.
run_windows_forced_healthy_conversation_warning_scenario() {
    build_dir="$1"
    intact_prefix="$2"

    validator_script="$(find_generated_validator_script "$build_dir")"

    output="$(cmake -DCMAKE_INSTALL_PREFIX="$intact_prefix" -DWIN32=1 -P "$validator_script" 2>&1)" \
        || fail "re-running the validator against a genuinely intact install, with only the Windows branch forced, unexpectedly FAILED instead of warning-and-succeeding. Got:
${output}"

    normalized_output="$(normalize_wrapped_message "$output")"
    case "$normalized_output" in
        *"CONVERSATION could not be verified on this Windows machine"*) : ;;
        *) fail "the Windows-forced healthy-content scenario did not print the expected binary-conversation warning. Got:
${output}" ;;
    esac
    case "$normalized_output" in
        *"filesystem-only content check already ran before this point"*"already confirmed glintfx.pc, the headers and the library artifact are genuinely on disk with real content"*) : ;;
        *) fail "the Windows-forced healthy-content warning did not honestly attribute the confirmation to its own content check having already run. Got:
${output}" ;;
    esac
    case "$output" in
        *"post-install pkg-config content check"*) : ;;
        *) fail "the Windows-forced healthy-content scenario never printed its own content-check STATUS message before the binary-conversation warning - the content check may not have actually run first. Got:
${output}" ;;
    esac
    echo "ok: Windows-forced healthy-content WARNING - with genuinely correct content on disk, only the pkg-config BINARY conversation itself failing (a real, measured quirk this file already fixes for on real Windows CI) still degrades to a WARNING at exit 0, and the warning now truthfully names its own, already-run content check as the thing that verified the filesystem."
}

# Scenario 11: headers tree missing, on both a real Unix run and a
# Windows-forced run (RED on both) - the "árvore de cabeçalhos"/"um
# diretório vazio" case this file's own header used to declare NOT
# independently re-proven (only the library/-L branch, parameterized
# the same way by glintfx_pkgconfig_validate_content_variable(), was
# exercised, by scenario 5). Removes the ENTIRE installed `glintfx/`
# header subdirectory, leaving includedir a real, existing, but now
# genuinely EMPTY directory. Own throwaway prefix and own
# reconfigure-to-OFF, for the same self-sufficiency reason as
# scenario 5.
run_headers_missing_scenario() {
    glintfx_src="$1"
    cxx="$2"
    build_dir="$3"
    prefix="$4"

    reconfigure_glintfx "$glintfx_src" "$build_dir" "$cxx" "/usr/local" OFF >/dev/null
    build_glintfx "$build_dir" >/dev/null
    cmake --install "$build_dir" --prefix "$prefix" >/dev/null

    validator_script="$(find_generated_validator_script "$build_dir")"
    headers_dir="$(find "$prefix" -maxdepth 3 -type d -name glintfx -path '*/include/*' 2>/dev/null | head -n1)"
    [ -n "$headers_dir" ] || fail "no installed 'include/.../glintfx' header directory found anywhere under ${prefix} - cannot set up the headers-missing scenario"
    rm -rf "$headers_dir"

    for forced_win32 in "" "-DWIN32=1"; do
        output="$(cmake -DCMAKE_INSTALL_PREFIX="$prefix" $forced_win32 -P "$validator_script" 2>&1)" \
            && fail "re-running the validator against a real install with its ENTIRE header directory REMOVED (forced_win32='${forced_win32}') unexpectedly SUCCEEDED. Got:
${output}"

        normalized_output="$(normalize_wrapped_message "$output")"
        case "$normalized_output" in
            *"has no 'glintfx/' subdirectory"*"installed public headers are not there"*) : ;;
            *) fail "the headers-missing RED (forced_win32='${forced_win32}') did not name the missing headers by its own promise. Got:
${output}" ;;
        esac
    done
    echo "ok: headers-missing RED, on both a real Unix run and a Windows-forced run - the includedir branch of the filesystem/text-only content check fires for a genuinely empty header directory, unconditionally on every platform."
}

# Scenario 12: relative libdir, attacker-controlled CWD, still REFUSES -
# PKG-WIN-SCOPE adversarial review round 5 regression proof (27/08/2026).
# Own throwaway prefix and own reconfigure-to-OFF, for the same
# self-sufficiency reason as scenario 5. Reproduces the LIVE attack the
# adversarial reviewer used against this validator's pre-fix shape: a
# real install, its library artifact deleted (same precondition as
# scenario 5), glintfx.pc's own libdir= line rewritten to a BARE
# relative value (no "${pcfiledir}"), then the validator invoked with
# its CURRENT WORKING DIRECTORY set to an "attacker" directory holding a
# decoy file matching this validator's own library glob under that same
# relative name. Before this fatia's fix, this exact scenario passed at
# EXIT=0.
run_relative_libdir_cwd_attack_scenario() {
    glintfx_src="$1"
    cxx="$2"
    build_dir="$3"
    prefix="$4"
    scratch="$5"

    reconfigure_glintfx "$glintfx_src" "$build_dir" "$cxx" "/usr/local" OFF >/dev/null
    build_glintfx "$build_dir" >/dev/null
    cmake --install "$build_dir" --prefix "$prefix" >/dev/null

    validator_script="$(find_generated_validator_script "$build_dir")"

    # Genuinely broken install: the real library artifact deleted,
    # exactly as scenario 5 does.
    find "$prefix" -maxdepth 3 -name 'libglintfx.so*' -o -maxdepth 3 -name 'libglintfx.a' 2>/dev/null \
        | while IFS= read -r artifact; do rm -f "$artifact"; done

    pc_file="$(find_pc_file_under "$prefix")"
    decoy_subdir_name="decoy-relative-libdir"
    # Rewrite libdir= to a BARE relative value - no "${pcfiledir}" - the
    # exact shape the adversarial review used to defeat this validator
    # with. sed -i.bak/rm .bak instead of a GNU-only in-place edit, since
    # this script also runs on macOS/BSD sed in some environments this
    # project has not fully enumerated (GODS_LAWS.md L-27).
    sed -i.bak "s#^libdir=.*#libdir=${decoy_subdir_name}#" "$pc_file"
    rm -f "${pc_file}.bak"

    # The "attacker": a directory with nothing to do with the real
    # install, containing a decoy file that matches this validator's own
    # library glob under the SAME relative name the rewritten glintfx.pc
    # now uses.
    attacker_dir="${scratch}/attacker-cwd"
    mkdir -p "${attacker_dir}/${decoy_subdir_name}"
    : > "${attacker_dir}/${decoy_subdir_name}/libglintfx.so.0.1.0.0"

    output="$(cd "$attacker_dir" && cmake -DCMAKE_INSTALL_PREFIX="$prefix" -P "$validator_script" 2>&1)" \
        && fail "re-running the validator, from inside an attacker-controlled CWD containing a decoy library, against a real install with its OWN library artifact REMOVED and glintfx.pc's libdir rewritten to a bare relative value, unexpectedly SUCCEEDED - the PKG-WIN-SCOPE round 5 regression (a relative path resolving against the CALLER's CWD instead of glintfx.pc's own directory) is back. Got:
${output}"

    normalized_output="$(normalize_wrapped_message "$output")"
    case "$normalized_output" in
        *"pkgconfig/${decoy_subdir_name}"*"does not exist on disk"*) : ;;
        *) fail "the relative-libdir CWD-attack RED did not name a path anchored under glintfx.pc's own directory ('.../pkgconfig/${decoy_subdir_name}'). Got:
${output}" ;;
    esac
    case "$output" in
        *"$attacker_dir"*)
            fail "the relative-libdir CWD-attack RED message named the ATTACKER directory (${attacker_dir}) - the relative value is still resolving against the caller's CWD instead of glintfx.pc's own directory, the exact regression this scenario exists to catch. Got:
${output}"
            ;;
        *) : ;;
    esac
    echo "ok: relative-libdir CWD-attack RED - a real install with its library artifact removed and glintfx.pc's libdir rewritten to a bare relative value REFUSES even when invoked from an attacker-controlled working directory holding a decoy library under the same relative name; the resolved path is anchored under glintfx.pc's own directory, never the caller's CWD."
}

# Scenario 13: real pkg-config's own variable syntax accepted, all three
# at once (whitespace around "=", a trailing "#" comment, and
# last-definition-wins for a variable defined twice) - PKG-WIN-SCOPE
# round 5. Hand-assembled fixture, same reasoning as scenario 6: needs
# only the shared build's own generated validator script and the real
# libdir subpath (both stable across the whole file, PERF-PKGVALIDATE) -
# no build or install of its own. The fixture is deliberately shaped so
# ALL THREE forms must be honored at once for it to resolve to real
# content: "exec_prefix"'s own definition needs the whitespace-around-"="
# AND comment-stripping fixes to resolve at all (get either wrong and
# "libdir", which depends on it, never resolves either), and "libdir"
# itself is defined twice, first pointing at a directory that does not
# exist, so only last-definition-wins reaches the real one. When a real
# pkg-config/pkgconf binary is on PATH, cross-checks the SAME fixture
# against it too (declared downgrade otherwise, GODS_LAWS.md L-27,
# mirroring scenario 8's own absence handling) - so this scenario's
# claim that the fixture is "genuinely good" does not rest solely on
# this project's own code agreeing with itself.
run_real_pkgconfig_syntax_variants_scenario() {
    build_dir="$1"
    scratch="$2"
    real_libdir="$3"

    validator_script="$(find_generated_validator_script "$build_dir")"

    fixture_prefix="${scratch}/prefix-syntax-variants-fixture"
    mkdir -p "${fixture_prefix}/${real_libdir}/pkgconfig" "${fixture_prefix}/include/glintfx"
    : > "${fixture_prefix}/${real_libdir}/libglintfx.so.0.1.0.0"
    ln -s libglintfx.so.0.1.0.0 "${fixture_prefix}/${real_libdir}/libglintfx.so"
    pkgconfig_dir="${fixture_prefix}/${real_libdir}/pkgconfig"
    cat > "${pkgconfig_dir}/glintfx.pc" << EOF
prefix=\${pcfiledir}/../..
exec_prefix = \${prefix}   # whitespace around "=" AND a trailing comment, both on this one line
includedir=\${prefix}/include
libdir=\${exec_prefix}/${real_libdir}-does-not-exist-yet
libdir=\${exec_prefix}/${real_libdir}

Name: glintfx
Description: check_pkgconfig_validate.sh L-40-adjacent syntax-variants fixture - whitespace around "=", a trailing comment, and a duplicate variable, all at once
Version: 0.1.0.0
Cflags: -I\${includedir}
Libs: -L\${libdir} -lglintfx
EOF

    output="$(cmake -DCMAKE_INSTALL_PREFIX="$fixture_prefix" -P "$validator_script" 2>&1)" \
        || fail "re-running the validator against a fixture using real pkg-config's own whitespace/comment/duplicate-variable syntax unexpectedly FAILED - this is the exact 'rejects a genuinely good install' defect class PKG-WIN-SCOPE's round 5 review found. Got:
${output}"

    case "$output" in
        *"post-install pkg-config validation passed"*) : ;;
        *) fail "the syntax-variants fixture installed cleanly but the validator never printed its own success message. Got:
${output}" ;;
    esac

    real_tool=""
    if command -v pkgconf >/dev/null 2>&1; then
        real_tool="pkgconf"
    elif command -v pkg-config >/dev/null 2>&1; then
        real_tool="pkg-config"
    fi

    if [ -n "$real_tool" ]; then
        real_output="$(PKG_CONFIG_PATH="$pkgconfig_dir" "$real_tool" --print-errors --cflags --libs glintfx 2>&1)" \
            || fail "a REAL ${real_tool} on this machine's PATH rejected the same syntax-variants fixture this validator just accepted - the fixture is not actually 'genuinely good' by an independent implementation. Got:
${real_output}"
        case "$real_output" in
            *"-lglintfx"*) : ;;
            *) fail "a real ${real_tool} run against the syntax-variants fixture did not emit the expected -lglintfx. Got:
${real_output}" ;;
        esac
        echo "ok: real pkg-config syntax variants (whitespace around '=', trailing comment, duplicate-variable-last-wins) - accepted by BOTH this validator and a real ${real_tool} on this machine's PATH, cross-checked against the same fixture."
    else
        echo "ok: real pkg-config syntax variants (whitespace around '=', trailing comment, duplicate-variable-last-wins) - accepted by this validator; no real pkg-config/pkgconf on PATH to additionally cross-check against (declared, GODS_LAWS.md L-27 - see scenario 8's own downgrade for the same absence)."
    fi
}

main() {
    require_args "$@"
    glintfx_src="$1"
    cxx="$2"

    scratch="$(make_scratch_workdir)"
    trap 'rm -rf "$scratch"' EXIT

    # PERF-PKGVALIDATE: ONE configure and ONE build for the whole file
    # (see the file-level comment above for the measured before/after).
    # "/usr/local" and OFF are CMake's/this project's own defaults,
    # spelled out explicitly rather than omitted - every reconfigure in
    # this file states both values, every time, so no scenario can
    # silently inherit a value a previous scenario happened to leave
    # cached.
    build_dir="${scratch}/build-shared"
    reconfigure_glintfx "$glintfx_src" "$build_dir" "$cxx" "/usr/local" OFF >/dev/null
    build_glintfx "$build_dir" >/dev/null

    prefix_default="${scratch}/prefix-default"
    run_default_layout_scenario "$build_dir" "$prefix_default"
    real_libdir="$(find_libdir_relative_to_prefix "$prefix_default")"

    run_destdir_scenario "$glintfx_src" "$cxx" "$build_dir" "$scratch"
    run_configure_time_hatch_scenario "$glintfx_src" "$cxx" "$build_dir" "${scratch}/prefix-hatch-configure"
    run_install_time_hatch_scenario "$glintfx_src" "$cxx" "$build_dir" "${scratch}/prefix-hatch-install"
    run_broken_library_scenario "$glintfx_src" "$cxx" "$build_dir" "${scratch}/prefix-broken-library"
    run_missing_pc_file_scenario "$glintfx_src" "$cxx" "$build_dir" "${scratch}/prefix-missing-pc"
    run_empty_flags_floor_scenario "$build_dir" "$scratch" "$real_libdir"
    run_pkgconfig_absent_scenario "$build_dir" "$scratch" "$prefix_default"
    run_windows_forced_broken_library_scenario "$glintfx_src" "$cxx" "$build_dir" "${scratch}/prefix-windows-forced-broken-library"
    run_windows_forced_healthy_conversation_warning_scenario "$build_dir" "$prefix_default"
    run_headers_missing_scenario "$glintfx_src" "$cxx" "$build_dir" "${scratch}/prefix-headers-missing"
    run_relative_libdir_cwd_attack_scenario "$glintfx_src" "$cxx" "$build_dir" "${scratch}/prefix-relative-cwd-attack" "$scratch"
    run_real_pkgconfig_syntax_variants_scenario "$build_dir" "$scratch" "$real_libdir"

    echo "ok: the PKG-VALIDATE install(CODE) step runs on real installs (default layout, DESTDIR), honors both halves of its escape hatch, fails closed with a self-sufficient diagnostic on a real broken library artifact, a real missing glintfx.pc, a real missing header tree, a hand-assembled empty-Cflags/Libs (L-40) fixture, and a relative libdir resolved from an attacker-controlled working directory - on the real Unix path AND with the Windows branch forced alike - while degrading to a WARNING (not a FATAL_ERROR) only when pkg-config itself is absent, or when a real pkg-config binary genuinely cannot be talked to despite content already confirmed correct; and accepts real pkg-config's own whitespace/comment/duplicate-variable syntax, cross-checked against a real pkg-config binary when one is on PATH."
}

main "$@"
