#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_dep_zero.sh - CI + pre-commit gate for GODS_LAWS.md L-07 (zero
# dependency besides the C++23 standard library and OS APIs).
#
# Born 28/08/2026 from a lacuna measured against the 21 gates already
# under tests/tools/: none of them barred the INTRODUCTION of a
# third-party dependency. check_vendor_purity.sh guards the Khronos
# exception from growing, check_layers.sh guards layer discipline,
# check_no_x11.sh guards L-05/L-06 - none of the three ever looks for
# FetchContent, find_package, pkg_check_modules or a foreign #include.
# Decision of the leader (AskUserQuestion, 28/08/2026): the gate bites
# in BOTH places, commit and CI, not one or the other:
#
#   check_dep_zero.sh <source-root-directory> <path-to-.so-or-NONE>
#       tree mode - every file GIT TRACKS in the tree, all three
#       sub-checks. This is the CI shape (dep_zero_test).
#   check_dep_zero.sh --staged <source-root-directory>
#       staged mode - only the files staged for THIS commit, read from
#       the git INDEX (never the working tree - GODS_LAWS.md L-12 "read
#       the committed blob, not the tree that may be mid mutation",
#       applied here to the index instead of a commit sha). Sub-checks
#       (a)/(b) only: there is no built artifact at commit time, so
#       sub-check (c) does not run here. This is the pre-commit shape.
#   check_dep_zero.sh --selftest
#       the three paths (blocks / lets through / conscious escape)
#       exercised against disposable fixtures under mktemp, never
#       against the tracked tree.
#
# THREE SUB-CHECKS, closed by FORM and POLARITY, never by a growing
# list of forbidden library names (a name list is infinite and ages;
# an ALLOWLIST of what we already accept is small, closed, and grows
# only by a conscious edit of THIS file, which is exactly the moment
# GODS_LAWS.md L-07 says to stop and ask the leader):
#
#   (a) CMake surface (every git-tracked CMakeLists.txt, *.cmake,
#       *.cmake.in - a *.pc.in is pkg-config template syntax, not
#       CMake, and is out of scope by construction). Two shapes:
#         - UNCONDITIONALLY forbidden: include(FetchContent),
#           include(ExternalProject), FetchContent_Declare/
#           MakeAvailable/Populate, ExternalProject_Add,
#           CPMAddPackage, and any conan_*/vcpkg* call or include()
#           referencing a conan/vcpkg toolchain file. No allowlist
#           saves these - loading the module is the violation.
#         - conditional on the ALLOWLIST below: find_package(<name>
#           and pkg_check_modules(... <module>). Today's allowlist is
#           exactly what the tree uses: PkgConfig/glintfx for
#           find_package (a build tool that never links, and our own
#           library consumed by tests/package/), wayland-client for
#           pkg_check_modules (GODS_LAWS.md L-07: a system API, same
#           category as Win32).
#       Deliberately NOT done here: parsing target_link_libraries().
#       Textual, multi-line, keyword-laden parsing of that call is the
#       exact shape that manufactures false positives against our own
#       toolchain (a gate that screams wrong gets disabled the next
#       week) - the truth of linkage is read from the ARTIFACT, not
#       guessed from source text; that is sub-check (c).
#
#   (b) Include surface (every git-tracked .cpp/.cxx/.cc/.hpp/.hxx/.hh/
#       .h/.ipp - wider than the two extensions in use today, per the
#       GODS_LAWS.md L-40 case 3 lesson "header hygiene enumerated only
#       one extension"). A line matching
#       '^\s*#\s*include\s*<([^>]+)>' is permitted iff:
#         1. the name has neither '.' nor '/' - the exact shape of
#            every C++23 standard library header (<vector>, <print>,
#            <charconv>, ...); or
#         2. the name is on the closed SO_HEADER_ALLOWLIST below,
#            seeded by the complete enumeration measured live in this
#            tree on 28/08/2026 (GL/gl.h, sys/*.h, unistd.h,
#            wayland-client.h, windows.h, and the wayland-scanner
#            GENERATED xdg-shell-client-protocol.h - generating it is
#            already judged OS API by GODS_LAWS.md L-07, the tool
#            itself never links); or
#         3. *** CORRECTION found by measuring the real tree before
#            writing this gate (GODS_LAWS.md L-43), NOT part of the
#            2-rule enumeration the planning document's section 2
#            wrote out loud - flagged to the orchestrator in this
#            fatia's report, not decided silently *** - the name
#            starts with "glintfx/" AND a file of that exact name
#            exists under <root>/include/. This is our OWN public
#            header, consumed via the public include path the way an
#            external consumer would (<glintfx/core/err.hpp> etc.) -
#            not third-party code by any definition, and the check is
#            STRUCTURAL (does the file really exist in OUR tree),
#            never a static name list that could silently drift, the
#            same "truth from the artifact, not from a promise" spirit
#            as sub-check (c). Measured 28/08/2026: 25+ production and
#            test files use this exact form; without rule 3 this gate
#            would reprove nearly the entire tree on its very first
#            real run.
#       Quote includes (#include "...") are not scanned: they resolve
#       inside the tree or a generated build directory, and a tracked
#       file they point at already falls under vector 1
#       (check_spdx.sh/check_vendor_purity.sh). Free bonus of the rule
#       above: <string.h> (has a '.', not on the allowlist) reproves -
#       correctly, since this tree already only uses the
#       <cstring>-prefixed standard header.
#
#   (c) The artifact's own truth: DT_NEEDED. `readelf -d <lib>` (always
#       run under LC_ALL=C - a locale-dependent field label, "Shared
#       library:" in English vs. "Biblioteca partilhada:" in pt-BR on
#       this very machine, measured live 28/08/2026, would silently
#       break parsing on any machine whose locale differs from the
#       author's - the exact "portao que congela um fato do ambiente"
#       trap this house has hit before) must show every NEEDED entry
#       on the closed NEEDED_ALLOWLIST below, measured live 28/08/2026:
#       libwayland-client.so.0, libgcc_s.so.1, libstdc++.so.6,
#       libm.so.6, libc.so.6. An entry outside the list means the
#       binary links a third party, and it does not matter which door
#       (a)/(b) it came through - and it catches what (a) deliberately
#       does not parse (a raw -lfoo in target_link_libraries). In
#       static mode (BUILD_SHARED_LIBS=OFF) the argument is "NONE" and
#       this sub-check is SKIPPED WITH A PRINTED REASON, never silently
#       (a static archive has no dynamic symbol table to inspect) -
#       same shape check_port_privacy.sh's own sub-check (d) already
#       uses for the identical situation.
#
# GODS_LAWS.md L-40 (non-empty-scan floor), per sub-check:
#   (a) 0 CMake surface files scanned -> REPROVES (tree mode only;
#       staged mode has a DECLARED pass for the legitimate
#       zero-relevant-among-N-staged case, see below).
#   (b) 0 C++ surface files scanned -> REPROVES, same distinction.
#   (c) tree mode, shared library given: 0 NEEDED entries read ->
#       REPROVES (a .so truly linking nothing is `readelf` broken or
#       the wrong file, never "clean" - even a hello-world links
#       libc). NONE (static mode) is a declared skip, not a scan.
#   Every passing run prints the counts it scanned - "olhou e estava
#   bem" is distinguishable from "não olhou" only because the number
#   is on the screen (L-40 item 3).
#
# *** DECISION D2 of the leader, 28/08/2026 (AskUserQuestion) - do NOT
# reopen: a commit with zero relevant file among N staged (e.g. a
# documentation-only commit) PASSES, DECLARING both numbers ("0
# relevant file among N staged; CMake/C++ surfaces untouched by this
# commit"). What L-40 forbids is the BLIND green - here the
# not-looking is printed and distinguishable. Tree mode (the CI shape)
# stays STRICT and unconditional: 0 CMake or 0 C++ files scanned there
# always reproves - there is no legitimate "0 relevant" reading of the
# whole tracked tree. Enumerator failure (git diff --cached with a
# non-zero exit, or running outside a repository) REPROVES in EVERY
# mode, staged included - that is never a legitimate zero. ***
#
# *** DECISION D3 of the leader, 28/08/2026 (AskUserQuestion): NO
# environment-variable escape hatch (no GLINTFX_DEP_ZERO_SKIP=1 or
# equivalent). Bypassing this gate means editing the allowlist below,
# which shows up in the diff - the only legitimate escape, and the
# only one this file implements (GODS_LAWS.md L-07: only the leader
# suspends the law, and an unaudited emergency shortcut defeats that
# by construction). ***
#
# Usage:
#   check_dep_zero.sh <source-root-directory> <path-to-.so-or-NONE>
#   check_dep_zero.sh --staged <source-root-directory>
#   check_dep_zero.sh --selftest
#
# Each function below does one thing (GODS_LAWS.md L-17).

set -eu

# --- the closed allowlists (GODS_LAWS.md L-40 item 5: enumeration
# closed by CONSTRUCTION) - every entry names WHY it is here, and
# growing any of them is a conscious, reviewable edit of this file. ---

readonly FIND_PACKAGE_ALLOWLIST="PkgConfig glintfx"
# PkgConfig: CMake's own module to locate pkg-config, a build tool -
#   never linked into the artifact (measured: cmake/GlintfxWaylandProtocols.cmake:34).
# glintfx: ourselves, consumed by tests/package/CMakeLists.txt:15 to
#   prove find_package(glintfx) works outside the tree.

readonly PKG_CHECK_MODULES_ALLOWLIST="wayland-client"
# wayland-client: GODS_LAWS.md L-07 fronteira registrada - libwayland-client
#   counts as OS API, same category as Win32 (measured: cmake/GlintfxWaylandProtocols.cmake:35).

readonly PKG_CHECK_MODULES_KEYWORDS="REQUIRED QUIET NO_CMAKE_PATH NO_CMAKE_ENVIRONMENT_PATH IMPORTED_TARGET GLOBAL"
# CMake's own pkg_check_modules() keyword vocabulary - never a module name.

readonly SO_HEADER_ALLOWLIST="GL/gl.h sys/prctl.h sys/stat.h sys/sysmacros.h sys/types.h unistd.h wayland-client.h windows.h xdg-shell-client-protocol.h glintfx/export.hpp glintfx/version_macros.hpp"
# Complete enumeration measured live 28/08/2026 (see header comment
# rule 2). xdg-shell-client-protocol.h is wayland-scanner GENERATED
# from the system-installed xdg-shell.xml - already judged OS API by
# GODS_LAWS.md L-07 (the Wayland precedent), never vendored, never
# hand-written.
#
# *** SECOND CORRECTION found by measuring the real tree (GODS_LAWS.md
# L-43), also not in the planning document's enumeration - flagged in
# this fatia's report: glintfx/export.hpp (generate_export_header(),
# cmake/GlintfxLibrary.cmake:114) and glintfx/version_macros.hpp
# (configure_file() from cmake/version_macros.hpp.in,
# cmake/GlintfxLibrary.cmake:122-123) are ALSO generated, born only
# under <build>/generated/include/glintfx/ - never on disk in
# <root>/include/, so rule 3's structural existence check cannot see
# them, the exact same reason xdg-shell-client-protocol.h is a static
# entry instead of a structural one. Measured 28/08/2026: 13 files
# across include/, src/ and tests/ include one of the two. ***

readonly NEEDED_ALLOWLIST="libwayland-client.so.0 libgcc_s.so.1 libstdc++.so.6 libm.so.6 libc.so.6"
# Measured live 28/08/2026 against build/src/libglintfx.so - exactly
# the five DT_NEEDED entries this library links today.

# --- messages the gate ORDERS a fix with, not just reports (aviso 3 do
# gusworld, GODS_LAWS.md L-21 emenda: consumer-facing text -> English) ---

FETCH_ADVICE="REMOVE this block entirely. If the functionality is genuinely needed, STOP and take it to the project leader (GODS_LAWS.md L-07): the answer is to write it in-house, never a dependency 'just for now'."
FINDPKG_ADVICE="REMOVE this call. If it is a build tool that never links, or an OS API, add it to this gate's allowlist WITH a justification comment, and cite the leader's decision in the commit."
PKGCHECK_ADVICE="REMOVE this call. If it is a build tool that never links, or an OS API, add it to this gate's allowlist WITH a justification comment, and cite the leader's decision in the commit."
INCLUDE_ADVICE="REMOVE this include. A new OS API header goes on this gate's allowlist WITH a justification; a third-party library header has no allowlist fix - GODS_LAWS.md L-07 says write it in-house."
NEEDED_ADVICE="The binary links this library. Find the link flag that brought it in and REMOVE it. There is no allowlist fix for third-party linkage without the leader's order."

readonly CMAKE_SURFACE_PATTERN='(^|/)CMakeLists\.txt$|\.cmake$|\.cmake\.in$'
readonly CXX_SURFACE_PATTERN='\.(cpp|cxx|cc|hpp|hxx|hh|h|ipp)$'

readonly CMAKE_FETCH_PATTERN='^[[:space:]]*(include[[:space:]]*\([[:space:]]*(fetchcontent|externalproject)[[:space:]]*\)|fetchcontent_(declare|makeavailable|populate)[[:space:]]*\(|externalproject_add[[:space:]]*\(|cpmaddpackage[[:space:]]*\()'
readonly CMAKE_TOOLCHAIN_PATTERN='^[[:space:]]*(conan_[a-z_]*[[:space:]]*\(|vcpkg[a-z_]*[[:space:]]*\(|include[[:space:]]*\([^)]*(conan|vcpkg)[^)]*\))'

fail() {
    echo "check_dep_zero.sh: $1" >&2
    exit 1
}

print_violation_header() {
    echo "check_dep_zero.sh: PROHIBITED (GODS_LAWS.md L-07 zero dependency):" >&2
}

# --- generic helpers ---------------------------------------------------

count_lines() {
    if [ -z "$1" ]; then
        echo 0
        return
    fi
    printf '%s\n' "$1" | wc -l | tr -d ' '
}

name_is_known() {
    candidate="$1"
    list="$2"
    for known in $list; do
        [ "$candidate" = "$known" ] && return 0
    done
    return 1
}

first_paren_arg() {
    printf '%s\n' "$1" | sed -E 's/^[^(]*\(([^[:space:])]+).*/\1/'
}

# --- sub-check (a): CMake surface, one line of CONTENT at a time -------
# Reads from STDIN (not a filename) so the SAME function serves tree
# mode (cat "$root/$p" | ...) and staged mode (git show ":$p" | ...) -
# GODS_LAWS.md L-12: staged mode must read the INDEX blob, never the
# working tree, and a plain filename argument could not express that.

cmake_content_violations() {
    display_path="$1"
    line_no=0
    while IFS= read -r line || [ -n "$line" ]; do
        line_no=$((line_no + 1))

        if printf '%s\n' "$line" | grep -qiE "$CMAKE_FETCH_PATTERN"; then
            printf '%s:%s: %s\n  -> %s\n' "$display_path" "$line_no" "$line" "$FETCH_ADVICE"
            continue
        fi
        if printf '%s\n' "$line" | grep -qiE "$CMAKE_TOOLCHAIN_PATTERN"; then
            printf '%s:%s: %s\n  -> %s\n' "$display_path" "$line_no" "$line" "$FETCH_ADVICE"
            continue
        fi
        if printf '%s\n' "$line" | grep -qiE '^[[:space:]]*find_package[[:space:]]*\('; then
            name="$(first_paren_arg "$line")"
            if ! name_is_known "$name" "$FIND_PACKAGE_ALLOWLIST"; then
                printf '%s:%s: %s\n  -> %s\n' "$display_path" "$line_no" "$line" "$FINDPKG_ADVICE"
            fi
            continue
        fi
        if printf '%s\n' "$line" | grep -qiE '^[[:space:]]*pkg_check_modules[[:space:]]*\('; then
            content="$(printf '%s\n' "$line" | sed -E 's/^[^(]*\(//; s/\)[[:space:]]*$//')"
            rest="$(printf '%s\n' "$content" | sed -E 's/^[^[:space:]]+[[:space:]]*//')"
            unknown_hit=0
            for tok in $rest; do
                if name_is_known "$tok" "$PKG_CHECK_MODULES_KEYWORDS"; then
                    continue
                fi
                if ! name_is_known "$tok" "$PKG_CHECK_MODULES_ALLOWLIST"; then
                    unknown_hit=1
                fi
            done
            if [ "$unknown_hit" -eq 1 ]; then
                printf '%s:%s: %s\n  -> %s\n' "$display_path" "$line_no" "$line" "$PKGCHECK_ADVICE"
            fi
        fi
    done
}

# --- sub-check (b): include surface, one line of CONTENT at a time -----

include_content_violations() {
    root="$1"
    display_path="$2"
    line_no=0
    while IFS= read -r line || [ -n "$line" ]; do
        line_no=$((line_no + 1))

        printf '%s\n' "$line" | grep -qE '^[[:space:]]*#[[:space:]]*include[[:space:]]*<[^>]+>' || continue
        name="$(printf '%s\n' "$line" | sed -E 's/^[[:space:]]*#[[:space:]]*include[[:space:]]*<([^>]+)>.*/\1/')"

        has_dot_or_slash=0
        case "$name" in
            *.*|*/*) has_dot_or_slash=1 ;;
        esac
        [ "$has_dot_or_slash" -eq 0 ] && continue

        name_is_known "$name" "$SO_HEADER_ALLOWLIST" && continue

        case "$name" in
            glintfx/*)
                [ -f "$root/include/$name" ] && continue
                ;;
        esac

        printf '%s:%s: %s\n  -> %s\n' "$display_path" "$line_no" "$line" "$INCLUDE_ADVICE"
    done
}

# --- sub-check (c): the built artifact's own DT_NEEDED truth -----------

needed_entries_of() {
    library_path="$1"
    LC_ALL=C readelf -d "$library_path" 2>/dev/null | sed -n 's/.*(NEEDED).*\[\(.*\)\].*/\1/p'
}

check_needed_allowlist() {
    library_path="$1"
    allowlist="$2"

    if [ "$library_path" = "NONE" ]; then
        echo "check_dep_zero.sh: (c) skipped - BUILD_SHARED_LIBS=OFF, a static archive has no dynamic symbol table / NEEDED entries to inspect"
        return 0
    fi
    [ -f "$library_path" ] || fail "shared library not found: $library_path"
    command -v readelf >/dev/null 2>&1 || fail "'readelf' not found in PATH (no silent skip)"

    needed="$(needed_entries_of "$library_path")"
    needed_count=0
    [ -n "$needed" ] && needed_count="$(count_lines "$needed")"
    if [ "$needed_count" -eq 0 ]; then
        echo "check_dep_zero.sh: empty scan (0 NEEDED entries in $library_path) - GODS_LAWS.md L-40" >&2
        return 1
    fi

    violations="$(printf '%s\n' "$needed" | while IFS= read -r lib; do
        [ -z "$lib" ] && continue
        name_is_known "$lib" "$allowlist" || printf '%s: %s\n  -> %s\n' "$library_path" "$lib" "$NEEDED_ADVICE"
    done)"

    if [ -n "$violations" ]; then
        print_violation_header
        printf '%s\n' "$violations" >&2
        return 1
    fi
    echo "check_dep_zero.sh: (c) $needed_count dynamic NEEDED entr(y/ies) scanned in $library_path, all allowed"
}

# --- tree mode (CI shape): every git-tracked file, all 3 sub-checks ----

check_dep_zero_tree() {
    root="$1"
    library_path="$2"

    all_files="$(git -C "$root" ls-files 2>/dev/null)" \
        || { echo "check_dep_zero.sh: 'git ls-files' failed in $root (not a git repository, or git unavailable) - scan refused, never assumed empty" >&2; return 1; }

    cmake_files="$(printf '%s\n' "$all_files" | grep -E "$CMAKE_SURFACE_PATTERN" || true)"
    cxx_files="$(printf '%s\n' "$all_files" | grep -E "$CXX_SURFACE_PATTERN" || true)"

    cmake_count=0
    [ -n "$cmake_files" ] && cmake_count="$(count_lines "$cmake_files")"
    if [ "$cmake_count" -eq 0 ]; then
        echo "check_dep_zero.sh: empty scan (0 CMake surface files) - GODS_LAWS.md L-40" >&2
        return 1
    fi

    cxx_count=0
    [ -n "$cxx_files" ] && cxx_count="$(count_lines "$cxx_files")"
    if [ "$cxx_count" -eq 0 ]; then
        echo "check_dep_zero.sh: empty scan (0 C++ surface files) - GODS_LAWS.md L-40" >&2
        return 1
    fi

    cmake_violations="$(printf '%s\n' "$cmake_files" | while IFS= read -r p; do
        [ -z "$p" ] && continue
        cat "$root/$p" | cmake_content_violations "$root/$p"
    done)"
    include_violations="$(printf '%s\n' "$cxx_files" | while IFS= read -r p; do
        [ -z "$p" ] && continue
        cat "$root/$p" | include_content_violations "$root" "$root/$p"
    done)"

    needed_ok=1
    needed_output=""
    if ! needed_output="$(check_needed_allowlist "$library_path" "$NEEDED_ALLOWLIST" 2>&1)"; then
        needed_ok=0
    fi

    if [ -n "$cmake_violations" ] || [ -n "$include_violations" ] || [ "$needed_ok" -eq 0 ]; then
        print_violation_header
        [ -n "$cmake_violations" ] && printf '%s\n' "$cmake_violations" >&2
        [ -n "$include_violations" ] && printf '%s\n' "$include_violations" >&2
        [ "$needed_ok" -eq 0 ] && printf '%s\n' "$needed_output" >&2
        return 1
    fi

    echo "check_dep_zero.sh: 0 violation(s) - $cmake_count cmake file(s), $cxx_count c++ file(s) scanned"
    printf '%s\n' "$needed_output"
    echo "check_dep_zero.sh: out of scope by design: documents (.md), shell scripts (.sh), quote includes (vendor vector covered by check_spdx.sh/check_vendor_purity.sh)"
}

# --- staged mode (pre-commit shape): index content, sub-checks (a)/(b) -

check_dep_zero_staged() {
    root="$1"

    git -C "$root" rev-parse --is-inside-work-tree >/dev/null 2>&1 \
        || { echo "check_dep_zero.sh: not a git repository: $root" >&2; return 1; }

    staged="$(git -C "$root" diff --cached --name-only --diff-filter=ACMR)" \
        || { echo "check_dep_zero.sh: 'git diff --cached' failed in $root" >&2; return 1; }

    staged_count=0
    [ -n "$staged" ] && staged_count="$(count_lines "$staged")"

    cmake_relevant="$(printf '%s\n' "$staged" | grep -E "$CMAKE_SURFACE_PATTERN" || true)"
    cxx_relevant="$(printf '%s\n' "$staged" | grep -E "$CXX_SURFACE_PATTERN" || true)"

    cmake_count=0
    [ -n "$cmake_relevant" ] && cmake_count="$(count_lines "$cmake_relevant")"
    cxx_count=0
    [ -n "$cxx_relevant" ] && cxx_count="$(count_lines "$cxx_relevant")"
    relevant_count=$((cmake_count + cxx_count))

    if [ "$relevant_count" -eq 0 ]; then
        echo "check_dep_zero.sh: 0 relevant file(s) among $staged_count staged file(s); CMake/C++ surfaces untouched by this commit (GODS_LAWS.md L-40: declared, not silent)"
        return 0
    fi

    cmake_violations=""
    if [ -n "$cmake_relevant" ]; then
        cmake_violations="$(printf '%s\n' "$cmake_relevant" | while IFS= read -r p; do
            [ -z "$p" ] && continue
            git -C "$root" show ":$p" 2>/dev/null | cmake_content_violations "$root/$p"
        done)"
    fi
    include_violations=""
    if [ -n "$cxx_relevant" ]; then
        include_violations="$(printf '%s\n' "$cxx_relevant" | while IFS= read -r p; do
            [ -z "$p" ] && continue
            git -C "$root" show ":$p" 2>/dev/null | include_content_violations "$root" "$root/$p"
        done)"
    fi

    if [ -n "$cmake_violations" ] || [ -n "$include_violations" ]; then
        print_violation_header
        [ -n "$cmake_violations" ] && printf '%s\n' "$cmake_violations" >&2
        [ -n "$include_violations" ] && printf '%s\n' "$include_violations" >&2
        return 1
    fi

    echo "check_dep_zero.sh: 0 violation(s) - $cmake_count cmake file(s), $cxx_count c++ file(s) among $staged_count staged file(s) scanned"
}

# --- real mode -----------------------------------------------------------

real_main() {
    if [ "${1:-}" = "--staged" ]; then
        [ "$#" -eq 2 ] || fail "usage: check_dep_zero.sh --staged <source-root-directory>"
        [ -d "$2" ] || fail "directory not found: $2"
        check_dep_zero_staged "$2" || fail "L-07 zero dependency violation found in staged changes (see message above)"
        return 0
    fi

    [ "$#" -eq 2 ] || fail "usage: check_dep_zero.sh <source-root-directory> <path-to-.so-or-NONE>"
    [ -d "$1" ] || fail "directory not found: $1"
    check_dep_zero_tree "$1" "$2" || fail "L-07 zero dependency violation found (see message above)"
}

# --- fixtures and controls for --selftest -------------------------------

make_scratch_workdir() {
    mktemp -d "${TMPDIR:-/tmp}/glintfx-dep-zero-selftest-XXXXXX"
}

# Minimal fixture that mirrors the REAL tree's shape - if this control
# fails, the gate would scream against our own toolchain, the exact
# defect that gets a gate disabled the following week.
make_clean_fixture() {
    root="$1"
    mkdir -p "$root/cmake" "$root/src/core" "$root/include/glintfx/core"
    cat > "$root/CMakeLists.txt" <<'EOF'
cmake_minimum_required(VERSION 3.25)
project(fixture)
add_subdirectory(src)
EOF
    cat > "$root/cmake/Wayland.cmake" <<'EOF'
# a consumer may embed glintfx via FetchContent, see docs/
find_package(PkgConfig REQUIRED)
pkg_check_modules(FixtureWayland REQUIRED wayland-client)
EOF
    cat > "$root/include/glintfx/core/err.hpp" <<'EOF'
#pragma once
EOF
    cat > "$root/src/core/err.cpp" <<'EOF'
#include <vector>
#include <wayland-client.h>
#include <windows.h>
#include <glintfx/core/err.hpp>
int f() { return 0; }
EOF
    mkdir -p "$root/src"
    : > "$root/src/CMakeLists.txt"
}

git_init_fixture() {
    root="$1"
    git -C "$root" init -q
    git -C "$root" config user.email "selftest@example.invalid"
    git -C "$root" config user.name "selftest"
    git -C "$root" add -A
    git -C "$root" commit -q -m "fixture"
}

selftest_positive_control() {
    scratch="$1"
    root="$scratch/positive"
    make_clean_fixture "$root"
    git_init_fixture "$root"

    if output="$(check_dep_zero_tree "$root" "NONE" 2>&1)"; then
        echo "selftest: POSITIVE control OK (clean fixture, comment mentioning FetchContent, allowlisted find_package/pkg_check_modules, own-header include all passed)"
        return 0
    fi
    echo "selftest: POSITIVE control FAILED (clean fixture should have passed)" >&2
    printf '%s\n' "$output" >&2
    return 1
}

selftest_negative_control_cmake() {
    scratch="$1"
    root="$scratch/negative-cmake"
    overall=0

    for case_name in fetchcontent externalproject cpm conan_toolchain vcpkg_toolchain findpkg_unknown pkgcheck_unknown; do
        make_clean_fixture "$root"
        case "$case_name" in
            fetchcontent)
                printf 'include(FetchContent)\nFetchContent_Declare(fmt GIT_REPOSITORY x)\n' >> "$root/cmake/Wayland.cmake"
                needle="FetchContent_Declare"
                ;;
            externalproject)
                printf 'include(ExternalProject)\nExternalProject_Add(fmt URL x)\n' >> "$root/cmake/Wayland.cmake"
                needle="ExternalProject_Add"
                ;;
            cpm)
                printf 'CPMAddPackage("gh:fmtlib/fmt#1.0")\n' >> "$root/cmake/Wayland.cmake"
                needle="CPMAddPackage"
                ;;
            conan_toolchain)
                printf 'conan_cmake_run(REQUIRES fmt/1.0)\n' >> "$root/cmake/Wayland.cmake"
                needle="conan_cmake_run"
                ;;
            vcpkg_toolchain)
                printf 'include(${CMAKE_BINARY_DIR}/vcpkg-toolchain.cmake)\n' >> "$root/cmake/Wayland.cmake"
                needle="vcpkg-toolchain"
                ;;
            findpkg_unknown)
                printf 'find_package(Freetype REQUIRED)\n' >> "$root/cmake/Wayland.cmake"
                needle="find_package(Freetype"
                ;;
            pkgcheck_unknown)
                printf 'pkg_check_modules(Fixture REQUIRED freetype2)\n' >> "$root/cmake/Wayland.cmake"
                needle="freetype2"
                ;;
        esac
        git_init_fixture "$root"

        if output="$(check_dep_zero_tree "$root" "NONE" 2>&1)"; then
            echo "selftest: NEGATIVE(cmake/$case_name) control FAILED (should have been reproved)" >&2
            overall=1
        elif ! printf '%s\n' "$output" | grep -qF "$needle"; then
            echo "selftest: NEGATIVE(cmake/$case_name) control FAILED (reproved, but did not cite '$needle')" >&2
            printf '%s\n' "$output" >&2
            overall=1
        fi
        rm -rf "$root"
    done

    [ "$overall" -eq 0 ] && echo "selftest: NEGATIVE(cmake) control OK (seven forms, each reproved and cited)"
    return "$overall"
}

selftest_negative_control_include() {
    scratch="$1"
    root="$scratch/negative-include"
    overall=0

    for case_name in zlib boost_quote png_dot; do
        make_clean_fixture "$root"
        case "$case_name" in
            zlib) printf '#include <zlib.h>\n' >> "$root/src/core/err.cpp"; needle="zlib.h" ;;
            boost_quote) printf '#include <boost/any.hpp>\n' >> "$root/src/core/err.cpp"; needle="boost/any.hpp" ;;
            png_dot) printf '#include <png.h>\n' >> "$root/src/core/err.cpp"; needle="png.h" ;;
        esac
        git_init_fixture "$root"

        if output="$(check_dep_zero_tree "$root" "NONE" 2>&1)"; then
            echo "selftest: NEGATIVE(include/$case_name) control FAILED (should have been reproved)" >&2
            overall=1
        elif ! printf '%s\n' "$output" | grep -qF "$needle"; then
            echo "selftest: NEGATIVE(include/$case_name) control FAILED (reproved, but did not cite '$needle')" >&2
            printf '%s\n' "$output" >&2
            overall=1
        fi
        rm -rf "$root"
    done

    [ "$overall" -eq 0 ] && echo "selftest: NEGATIVE(include) control OK (three forms, each reproved and cited)"
    return "$overall"
}

# Fixture .so's compiled with the system cc, in scratch - depends on no
# package inside the container (plan section 4, caminho 1).
selftest_negative_control_needed() {
    scratch="$1"
    root="$scratch/negative-needed"
    mkdir -p "$root"

    printf 'int notallowed_fn(void) { return 7; }\n' > "$root/notallowed.c"
    cc -shared -fPIC -o "$root/libnotallowed.so" "$root/notallowed.c" -Wl,-soname,libnotallowed.so \
        || { echo "selftest: NEGATIVE(needed) control SKIPPED (cc unavailable to build fixture .so)"; return 0; }

    printf 'extern int notallowed_fn(void);\nint intruder_fn(void) { return notallowed_fn(); }\n' > "$root/intruder.c"
    cc -shared -fPIC -o "$root/libintruder.so" "$root/intruder.c" -L"$root" -lnotallowed -Wl,-rpath,"$root"

    if output="$(check_needed_allowlist "$root/libintruder.so" "$NEEDED_ALLOWLIST" 2>&1)"; then
        echo "selftest: NEGATIVE(needed) control FAILED (intruder library should have been reproved)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    if ! printf '%s\n' "$output" | grep -qF "libnotallowed.so"; then
        echo "selftest: NEGATIVE(needed) control FAILED (reproved, but did not cite libnotallowed.so)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    echo "selftest: NEGATIVE(needed) control OK (real .so linking an out-of-allowlist library, reproved and cited)"
}

selftest_positive_control_needed() {
    scratch="$1"
    root="$scratch/positive-needed"
    mkdir -p "$root"

    printf 'int clean_fn(void) { return 42; }\n' > "$root/clean.c"
    cc -shared -fPIC -o "$root/libclean.so" "$root/clean.c" \
        || { echo "selftest: POSITIVE(needed) control SKIPPED (cc unavailable to build fixture .so)"; return 0; }

    # A plain cc -shared links only libc.so.6, which is on the real
    # allowlist - proves the sub-check does not scream at ordinary,
    # allowed linkage.
    if ! output="$(check_needed_allowlist "$root/libclean.so" "$NEEDED_ALLOWLIST" 2>&1)"; then
        echo "selftest: POSITIVE(needed) control FAILED (a plain libc-only .so should have passed)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    echo "selftest: POSITIVE(needed) control OK (libc-only .so, allowed)"
}

selftest_needed_static_skip() {
    if output="$(check_needed_allowlist "NONE" "$NEEDED_ALLOWLIST" 2>&1)"; then
        if printf '%s\n' "$output" | grep -qF "skipped"; then
            echo "selftest: NEEDED-static-skip control OK (NONE declared as skipped, not silently passed)"
            return 0
        fi
    fi
    echo "selftest: NEEDED-static-skip control FAILED (NONE should pass while printing a declared skip)" >&2
    return 1
}

selftest_empty_scan_needed() {
    scratch="$1"
    root="$scratch/empty-needed"
    mkdir -p "$root"

    printf 'int f(void) { return 1; }\n' > "$root/nolibc.c"
    cc -shared -fPIC -nostdlib -o "$root/libnolibc.so" "$root/nolibc.c" \
        || { echo "selftest: EMPTY-SCAN(needed) control SKIPPED (cc -nostdlib unavailable to build fixture .so)"; return 0; }

    if output="$(check_needed_allowlist "$root/libnolibc.so" "$NEEDED_ALLOWLIST" 2>&1)"; then
        echo "selftest: EMPTY-SCAN(needed) control FAILED (a .so with zero NEEDED entries should have been reproved)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    if ! printf '%s\n' "$output" | grep -qF "empty scan"; then
        echo "selftest: EMPTY-SCAN(needed) control FAILED (reproved, but did not say 'empty scan')" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    echo "selftest: EMPTY-SCAN(needed) control OK (zero-NEEDED .so refused)"
}

selftest_empty_scan_tree() {
    scratch="$1"
    root="$scratch/empty-tree"
    mkdir -p "$root"
    git -C "$root" init -q
    git -C "$root" config user.email "selftest@example.invalid"
    git -C "$root" config user.name "selftest"
    : > "$root/README.md"
    git -C "$root" add -A
    git -C "$root" commit -q -m "no cmake, no c++"

    if output="$(check_dep_zero_tree "$root" "NONE" 2>&1)"; then
        echo "selftest: EMPTY-SCAN(tree) control FAILED (a tree with no CMake/C++ surface should have been refused)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    if ! printf '%s\n' "$output" | grep -qF "empty scan"; then
        echo "selftest: EMPTY-SCAN(tree) control FAILED (reproved, but did not say 'empty scan')" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    echo "selftest: EMPTY-SCAN(tree) control OK (repository without CMake/C++ surface refused)"
}

# GODS_LAWS.md L-27 decision D2: a staged commit whose ONLY change is
# documentation passes, declaring the two numbers.
selftest_staged_zero_relevant_declared() {
    scratch="$1"
    root="$scratch/staged-doc-only"
    make_clean_fixture "$root"
    git_init_fixture "$root"
    printf 'more docs\n' > "$root/README.md"
    git -C "$root" add README.md

    if ! output="$(check_dep_zero_staged "$root" 2>&1)"; then
        echo "selftest: STAGED(doc-only) control FAILED (a documentation-only staged commit should pass)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    if ! printf '%s\n' "$output" | grep -qF "0 relevant file"; then
        echo "selftest: STAGED(doc-only) control FAILED (passed, but did not declare '0 relevant file')" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    echo "selftest: STAGED(doc-only) control OK (0 relevant among N staged, declared and passed)"
}

selftest_staged_blocks_violation() {
    scratch="$1"
    root="$scratch/staged-violation"
    make_clean_fixture "$root"
    git_init_fixture "$root"
    printf 'include(FetchContent)\n' >> "$root/cmake/Wayland.cmake"
    git -C "$root" add cmake/Wayland.cmake

    if output="$(check_dep_zero_staged "$root" 2>&1)"; then
        echo "selftest: STAGED(violation) control FAILED (a staged FetchContent should be reproved)" >&2
        return 1
    fi
    if ! printf '%s\n' "$output" | grep -qF "FetchContent"; then
        echo "selftest: STAGED(violation) control FAILED (reproved, but did not cite FetchContent)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    echo "selftest: STAGED(violation) control OK (staged FetchContent reproved and cited)"
}

# The index control (plan section 4): stage a CLEAN version, then dirty
# the WORKING TREE with a violation - the hook must still PASS, proving
# it reads the git index (`git show :path`), never the working tree.
selftest_staged_reads_index_not_worktree() {
    scratch="$1"
    root="$scratch/staged-index-only"
    make_clean_fixture "$root"
    git_init_fixture "$root"
    printf 'find_package(PkgConfig REQUIRED)\n' >> "$root/cmake/Wayland.cmake"
    git -C "$root" add cmake/Wayland.cmake
    printf 'include(FetchContent)\n' >> "$root/cmake/Wayland.cmake"

    if ! output="$(check_dep_zero_staged "$root" 2>&1)"; then
        echo "selftest: STAGED(index-not-worktree) control FAILED (staged content is clean; a dirty WORKING TREE must not fail the hook)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    echo "selftest: STAGED(index-not-worktree) control OK (index read, not the dirtied working tree)"
}

# Caminho 3 - the only legitimate escape is editing the allowlist
# itself, in a subshell so the real global is never mutated.
selftest_escape_via_allowlist_edit() {
    scratch="$1"
    root="$scratch/escape"
    make_clean_fixture "$root"
    printf 'find_package(Xyz REQUIRED)\n' >> "$root/cmake/Wayland.cmake"
    git_init_fixture "$root"

    if "$0" "$root" "NONE" >/dev/null 2>&1; then
        echo "selftest: ESCAPE control FAILED (find_package(Xyz) must be reproved BEFORE the allowlist edit)" >&2
        return 1
    fi

    # readonly (set below main()) means a subshell env override cannot
    # express the escape - which is the POINT: D3 forbids a runtime
    # bypass. The only real escape is editing the allowlist LINE of
    # this file, so the control edits a SCRATCH COPY of this exact
    # script (never in-place, GODS_LAWS.md 28/07 rule) and runs THAT.
    edited="$1/check_dep_zero_edited.sh"
    sed -E 's/^(readonly FIND_PACKAGE_ALLOWLIST=")PkgConfig glintfx(")$/\1PkgConfig glintfx Xyz\2/' "$0" > "$edited"
    chmod +x "$edited"
    if ! grep -qF 'PkgConfig glintfx Xyz' "$edited"; then
        echo "selftest: ESCAPE control FAILED (sed did not find the allowlist line to edit - selftest itself is broken)" >&2
        return 1
    fi

    if ! "$edited" "$root" "NONE" >/dev/null 2>&1; then
        echo "selftest: ESCAPE control FAILED (editing the allowlist is the documented escape and must pass)" >&2
        return 1
    fi
    echo "selftest: ESCAPE control OK (find_package(Xyz) reproves; the SAME SCRIPT with Xyz added to the allowlist by editing this file's source passes - no other escape exists)"
}

selftest_main() {
    scratch="$(make_scratch_workdir)"
    trap 'rm -rf "$scratch"' EXIT

    overall=0
    selftest_positive_control "$scratch" || overall=1
    selftest_negative_control_cmake "$scratch" || overall=1
    selftest_negative_control_include "$scratch" || overall=1
    selftest_positive_control_needed "$scratch" || overall=1
    selftest_negative_control_needed "$scratch" || overall=1
    selftest_needed_static_skip || overall=1
    selftest_empty_scan_needed "$scratch" || overall=1
    selftest_empty_scan_tree "$scratch" || overall=1
    selftest_staged_zero_relevant_declared "$scratch" || overall=1
    selftest_staged_blocks_violation "$scratch" || overall=1
    selftest_staged_reads_index_not_worktree "$scratch" || overall=1
    selftest_escape_via_allowlist_edit "$scratch" || overall=1

    if [ "$overall" -ne 0 ]; then
        echo "check_dep_zero.sh --selftest: FAILED (see above)" >&2
        exit 1
    fi
    echo "check_dep_zero.sh --selftest: all twelve controls OK"
}

main() {
    if [ "${1:-}" = "--selftest" ]; then
        selftest_main
    else
        real_main "$@"
    fi
}

main "$@"
