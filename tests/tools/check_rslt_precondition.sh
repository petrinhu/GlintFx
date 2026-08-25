#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
# check_rslt_precondition.sh - proves BOTH halves of the debug-only
# precondition guard on gltfx_rslt<T>::value()/error() (correction of
# 25/08/2026, GODS_LAWS.md CORE-ERROR): calling value() on a result
# that holds an error, or error() on a result that holds a value, is a
# documented precondition violation. In a Debug build (NDEBUG
# undefined) it now stops DETERMINISTICALLY with a message naming the
# violation; in a Release build (NDEBUG defined, this project's
# default) the guard costs nothing and the behavior is UNCHANGED from
# before this fix.
#
# WHY THIS COMPILES THE FIXTURE TWICE INSTEAD OF RECONFIGURING A
# SEPARATE CMAKE BUILD TREE: value()/error() and their assert() calls
# are HEADER-ONLY inline functions (include/glintfx/core/err.hpp,
# gltfx_rslt<T> is a template - it cannot be hidden behind the
# library's ABI boundary, see that header's own comment). NDEBUG is
# therefore a property of how the CONSUMER'S OWN translation unit is
# compiled, not of how libglintfx.so itself was built - the already-
# built library (whatever mode it was configured in) is linked
# against UNCHANGED in both cases below; only the FIXTURE's own two
# compiles differ, by exactly the one flag (-DNDEBUG) that is the
# actual subject under test. This mirrors CMake's own convention (a
# Release build type defines NDEBUG; Debug does not) without spinning
# up a second full glintfx build merely to flip it.
#
# THE TWO SHAPES OF UNDEFINED BEHAVIOR, PROVEN SEPARATELY (see
# tools/bench/... sibling reasoning: measure, don't assume) - the two
# gltfx_rslt<T> forms are independent implementations, so each needs
# its own live proof of what happens when the guard is compiled out:
#   "primary" (gltfx_rslt<int>, std::variant<T, gltfx_err> storage):
#     std::get_if returns a genuine null pointer on the wrong
#     alternative; dereferencing it faults (SIGSEGV, page zero
#     unmapped on this project's five target platforms) in Release.
#   "void" (gltfx_rslt<void>, std::variant<std::monostate, gltfx_err>
#     storage AS OF 25/08/2026 - was std::optional<gltfx_err> before,
#     see include/glintfx/core/err.hpp's own comment at the
#     gltfx_rslt<void> specialization for the full history): now
#     dereferences std::get_if the SAME way the primary template does,
#     so it faults the SAME way - assert_release_void_faults_via_
#     null_dereference() below proves this by requiring the EXACT
#     SIGSEGV exit status, not just "no debug message". THE OLD
#     std::optional-based form did NOT have this property -
#     std::optional::operator*() on an unengaged optional reads the
#     optional's own buffer directly, which does not reliably fault
#     (proven live before this change, distro hardening explicitly
#     disabled: silently returned a fabricated gltfx_err, exit 0). That
#     was the case showing "the process dies either way" is not
#     automatically true, and it is WHY the debug-only guard stays
#     valuable even after this storage change (structural fault is
#     still undefined behavior, not a guarantee, on a platform this
#     project has not measured).
#
# Usage:
#   check_rslt_precondition.sh <include-dir> <generated-include-dir> <library-dir> <cxx-compiler>
#
# Each function below does one thing (GODS_LAWS.md L-17).

set -eu

readonly ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
readonly FIXTURE_SRC="${ROOT_DIR}/tests/precondition_fixtures/precondition_fixture.cpp"
readonly ASSERT_MESSAGE_PRIMARY="value() called on a result that holds an error"
readonly ASSERT_MESSAGE_VOID="error() called on a result that holds success"

fail() {
    echo "check_rslt_precondition.sh: $1" >&2
    exit 1
}

require_args() {
    [ "$#" -eq 4 ] || fail "usage: check_rslt_precondition.sh <include-dir> <generated-include-dir> <library-dir> <cxx-compiler>"
    [ -d "$1" ] || fail "include dir not found: $1"
    [ -d "$2" ] || fail "generated include dir not found: $2"
    [ -d "$3" ] || fail "library dir not found: $3"
    [ -f "$FIXTURE_SRC" ] || fail "fixture source not found: $FIXTURE_SRC"
}

make_scratch_workdir() {
    mktemp -d "${TMPDIR:-/tmp}/glintfx-rslt-precond-XXXXXX"
}

# ndebug_flag: "-DNDEBUG" for the release-mode compile, empty string
# for the debug-mode compile - the ONE variable under test.
compile_fixture() {
    includedir="$1"
    generated_includedir="$2"
    libdir="$3"
    cxx="$4"
    ndebug_flag="$5"
    output_bin="$6"
    # shellcheck disable=SC2086 # ndebug_flag is intentionally either empty or one flag, never quoted-word-split content
    "$cxx" -std=c++23 -O0 -g -Wall -Wextra -Werror $ndebug_flag \
        -I "$includedir" -I "$generated_includedir" \
        "$FIXTURE_SRC" \
        -L "$libdir" -Wl,-rpath,"$libdir" -lglintfx \
        -o "$output_bin"
}

# Runs $binary with $case_arg, captures BOTH stdout and stderr (the
# assert message goes to stderr) and the exit status - never lets a
# nonzero/signal exit abort this script under `set -e`.
run_capture() {
    binary="$1"
    case_arg="$2"
    output_file="$3"
    set +e
    "$binary" "$case_arg" >"$output_file" 2>&1
    status=$?
    set -e
    echo "$status"
}

assert_debug_case_stops_with_message() {
    binary="$1"
    case_arg="$2"
    expected_message="$3"
    output_file="$4"

    status="$(run_capture "$binary" "$case_arg" "$output_file")"
    echo "check_rslt_precondition.sh: debug/$case_arg exited with status $status, output:"
    cat "$output_file"

    [ "$status" -ne 0 ] || fail "debug/$case_arg exited 0 - the precondition violation did not stop the process at all"
    grep -qF "$expected_message" "$output_file" || fail "debug/$case_arg stopped (status $status) but its output did not name the violation (expected to contain: $expected_message)"

    echo "check_rslt_precondition.sh: debug/$case_arg OK (stopped deterministically, message present)"
}

assert_release_case_shows_no_debug_message() {
    binary="$1"
    case_arg="$2"
    forbidden_message="$3"
    output_file="$4"

    status="$(run_capture "$binary" "$case_arg" "$output_file")"
    echo "check_rslt_precondition.sh: release/$case_arg exited with status $status, output:"
    cat "$output_file"

    if grep -qF "$forbidden_message" "$output_file"; then
        fail "release/$case_arg printed the DEBUG-ONLY assert message even though compiled with -DNDEBUG - the guard is not actually compiled out"
    fi

    echo "check_rslt_precondition.sh: release/$case_arg OK (no debug-only message - assert compiled to nothing, whatever happened is the SAME undefined behavior this code already had)"
}

# CORE-ERROR finding, 25/08/2026 (CTO-measured, leader-decided):
# gltfx_rslt<void>'s storage moved from std::optional<gltfx_err> to
# std::variant<std::monostate, gltfx_err>, mirroring the primary
# template - error() now dereferences std::get_if<1>(&m_storage), a
# GENUINE null pointer when the wrong alternative is active, the same
# shape gltfx_rslt<T>'s own value()/error() already had. Page zero is
# unmapped on all five of this project's target platforms, so this
# null-pointer read is expected to SIGSEGV structurally - not because
# any standard or library guarantees it (it remains undefined
# behavior), but because of how virtual memory is universally laid out
# on every real OS this project ships on. THE OLD std::optional-based
# form did NOT have this property (its operator*() reads the
# optional's own internal buffer directly - proven live before this
# change, with GCC's distro-default _GLIBCXX_ASSERTIONS hardening
# explicitly disabled, to return a FABRICATED gltfx_err with exit 0,
# no fault at all). GODS_LAWS.md L-40: this assertion has to be SEEN
# TO REPROVE against the code it is meant to catch a regression of,
# before it is trusted - not adjusted after the fact to match whatever
# the code already does.
assert_release_void_faults_via_null_dereference() {
    binary="$1"
    output_file="$2"
    readonly SIGSEGV_EXIT_STATUS=139 # 128 + SIGSEGV(11), this project's Linux gates already read exit status this way

    status="$(run_capture "$binary" void "$output_file")"
    echo "check_rslt_precondition.sh: release/void (null-dereference check) exited with status $status, output:"
    cat "$output_file"

    [ "$status" -eq "$SIGSEGV_EXIT_STATUS" ] || fail "release/void exited $status, expected $SIGSEGV_EXIT_STATUS (SIGSEGV) - gltfx_rslt<void>::error() no longer faults via a real null-pointer dereference the way gltfx_rslt<T>'s primary template already does"

    echo "check_rslt_precondition.sh: release/void OK (SIGSEGV via a real null-pointer dereference, structural on this platform - page zero unmapped - not a library-level guarantee)"
}

main() {
    require_args "$@"
    includedir="$1"
    generated_includedir="$2"
    libdir="$3"
    cxx="$4"

    scratch="$(make_scratch_workdir)"
    trap 'rm -rf "$scratch"' EXIT

    debug_bin="$scratch/precondition_fixture_debug"
    release_bin="$scratch/precondition_fixture_release"

    echo "check_rslt_precondition.sh: compiling debug fixture (NDEBUG undefined)"
    compile_fixture "$includedir" "$generated_includedir" "$libdir" "$cxx" "" "$debug_bin"

    echo "check_rslt_precondition.sh: compiling release fixture (-DNDEBUG)"
    compile_fixture "$includedir" "$generated_includedir" "$libdir" "$cxx" "-DNDEBUG" "$release_bin"

    assert_debug_case_stops_with_message "$debug_bin" primary "$ASSERT_MESSAGE_PRIMARY" "$scratch/debug_primary.out"
    assert_debug_case_stops_with_message "$debug_bin" void "$ASSERT_MESSAGE_VOID" "$scratch/debug_void.out"

    assert_release_case_shows_no_debug_message "$release_bin" primary "$ASSERT_MESSAGE_PRIMARY" "$scratch/release_primary.out"
    assert_release_case_shows_no_debug_message "$release_bin" void "$ASSERT_MESSAGE_VOID" "$scratch/release_void.out"

    assert_release_void_faults_via_null_dereference "$release_bin" "$scratch/release_void_sigsegv.out"

    echo "ok: gltfx_rslt<T>'s debug-only precondition guard stops deterministically with a message in Debug, and costs nothing (no message, unchanged behavior) in Release, for both the primary template and the void specialization. gltfx_rslt<void>'s release-mode fault is a real null-pointer dereference (SIGSEGV), structurally the same shape the primary template already had."
}

main "$@"
