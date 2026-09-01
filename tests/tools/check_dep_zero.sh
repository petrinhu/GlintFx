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
# GODS_LAWS.md L-07 says to stop and ask the leader).
#
# *** REVIEW-DEPZERO-GATE.md, 28/08/2026 (SHA e07a12f): an adversarial
# review REPROVED the first version of this gate with FOUR CRITICAL
# findings, confirmed by the orchestrator, all of them shapes the "no
# allowlist saves these" claim below did not actually cover. Fixed in
# this same commit; each one now has a --selftest control that covers
# the FAMILY of the form, not the exact reproduction string, plus a
# real mutation/bite against a copy of the real tree:
#   1. A multi-line call (ordinary CMake formatting, not an attack) was
#      invisible to the old line-by-line matcher - "0 violation(s)" for
#      a staged freetype2 dependency, and this DEFEATED the real
#      pre-commit hook end to end. Fixed THEN by accumulating physical
#      lines into one balanced-paren STATEMENT before matching.
#      *** DEPZERO-SHALLOW, 31/08/2026: that statement-accumulation
#      machine is GONE. This gate is now a SHALLOW, per-PHYSICAL-LINE
#      network on purpose (see cmake_content_violations/
#      evaluate_cmake_line below): a violation resolvable from ONE line
#      still BLOCKS here, exactly as before; a call whose decisive
#      argument spans lines, or is built from a variable this gate
#      cannot read, no longer silently passes as "0 violation(s)" -
#      it PASSES WITH A PRINTED WARNING (never silent) and is DEFERRED
#      to the CI oracle (ctest test dep_zero_trace,
#      tests/tools/check_dep_zero_trace.py), which reads what CMake
#      ACTUALLY executed and WILL reprove a real violation there.
#      Leader's decision (AskUserQuestion, 28/08/2026, verbatim):
#      "Deixa passar avisando que o servidor decide". ***
#   2. cmake_language(CALL ${fn} ...) / cmake_language(EVAL CODE "...")
#      and include(${var}) with NO literal filename in the argument
#      bootstrapped FetchContent through pure indirection, matching no
#      literal pattern. cmake_language() is now blocked UNCONDITIONALLY
#      (zero legitimate use in this tree); a BARE-variable include() is
#      blocked unless the statement also contains a literal ".cmake"
#      fragment (the real, legitimate parameterized-file-include shape
#      already used twice in this tree, kept passing on purpose).
#   3. CPMFindPackage/CPMDeclarePackage/CPMGetPackage - three of CPM's
#      four public entry points - were absent from the blocklist, which
#      only named CPMAddPackage. All four are covered now.
#   4. A ".." path segment inside a <glintfx/...> include let the
#      structural rule-3 check resolve OUTSIDE include/ to a real system
#      header - and the identical include compiles for real under any
#      -Iinclude toolchain, so this was not a gate-only bug. Any ".."
#      occurrence is now rejected outright, before the filesystem is
#      even consulted.
# Two IMPORTANT findings (false positives that would have gotten this
# gate turned off) were fixed the same way: a multi-line find_package()/
# pkg_check_modules() that IS on the allowlist no longer reproves just
# for being formatted across several lines, and a pkg-config version
# constraint (wayland-client>=1.20, or the three-token spaced form) no
# longer reproves an already-allowed module. A seventh, IMPORTANT
# finding about the closing "out of scope" sentence overclaiming what
# check_spdx.sh/check_vendor_purity.sh actually cover is fixed by
# rewording it to state their real, narrower behavior (see the tree-mode
# closing message near the end of this file). ***
#
#   (a) CMake surface (every git-tracked CMakeLists.txt, *.cmake,
#       *.cmake.in - a *.pc.in is pkg-config template syntax, not
#       CMake, and is out of scope by construction). Scanned one
#       PHYSICAL LINE at a time, comment stripped before matching (see
#       cmake_content_violations/evaluate_cmake_line below) - DEPZERO-
#       SHALLOW, 31/08/2026: a call whose decisive evidence does not
#       fit on that one line is not resolved here, it WARNS and defers
#       to the CI oracle (see the fatia's note a few lines above).
#       Four shapes (the fourth new in DEPZERO-SHALLOW, 31/08/2026 -
#       SH-R7/SH-R8, closing a hole the CI oracle alone cannot see: a
#       CONDITIONAL BRANCH NOT TAKEN is invisible to dep_zero_trace by
#       construction, since it only reads what CMake actually executed;
#       this shallow, textual scanner sees every branch):
#         - UNCONDITIONALLY forbidden: include(FetchContent),
#           include(ExternalProject), FetchContent_Declare/
#           MakeAvailable/Populate, ExternalProject_Add, and all four
#           of CPMAddPackage/CPMFindPackage/CPMDeclarePackage/
#           CPMGetPackage; any conan_*/vcpkg* call or include()
#           referencing a conan/vcpkg toolchain file; cmake_language()
#           in its entirety, any subcommand (see CMAKE_LANGUAGE_PATTERN
#           above); include() whose argument is built ENTIRELY from
#           a variable with no literal ".cmake" fragment anywhere in the
#           statement; and file(DOWNLOAD)/file(UPLOAD) (see
#           CMAKE_FILE_NETWORK_PATTERN - CMake's own native network
#           transfer, zero legitimate use in this tree). No allowlist
#           saves any of these - the call itself (or the indirection
#           itself) is the violation.
#         - conditional on the ALLOWLIST below: find_package(<name>
#           and pkg_check_modules(... <module>), matched by base module
#           name with a pkg-config version comparator stripped first.
#           Today's allowlist is exactly what the tree uses:
#           PkgConfig/glintfx for find_package (a build tool that never
#           links, and our own library consumed by tests/package/),
#           wayland-client for pkg_check_modules (GODS_LAWS.md L-07: a
#           system API, same category as Win32).
#         - conditional on EXECUTE_PROCESS_PROGRAM_ALLOWLIST below:
#           execute_process(COMMAND <program> ...) with a LITERAL
#           program name (basename, extension and case stripped) -
#           only pkg-config/pkgconf pass, the same two names the trace
#           oracle's own R8 already allows.
#       Any of these four shapes whose decisive evidence does not fit
#       on the ONE physical line being scanned - the program built from
#       a variable, the DOWNLOAD/UPLOAD subcommand on a later line -
#       WARNS and defers to the CI oracle instead (section (a)'s own
#       DEPZERO-SHALLOW note above).
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
#            *** REVIEW-DEPZERO-GATE.md achado CRITICO #4, 28/08/2026:
#            "exists under <root>/include/" was checked with a plain
#            `test -f`, which resolves ".." against the real
#            filesystem - glintfx/../../../../../../usr/include/zlib.h
#            walked OUT of include/ entirely and landed on a real
#            system header, and the identical include compiles under
#            any -Iinclude toolchain (same path-resolution rule the
#            preprocessor itself follows). Any ".." anywhere in the
#            name is now rejected before the filesystem is even
#            touched - see include_content_violations below. ***
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
INDIRECTION_ADVICE="REMOVE this call. This gate cannot verify what dependency-management code runs through indirection (cmake_language(), or an include() argument built entirely from a variable, with no literal filename in it) - GODS_LAWS.md L-07: STOP and take an opaque, indirect call like this to the leader; write the target literally instead."
FILE_NETWORK_ADVICE="REMOVE this call. file(DOWNLOAD/UPLOAD) is CMake's own native network transfer; there is no allowlist fix - GODS_LAWS.md L-07: STOP and take it to the leader, the answer is to write it in-house, never a dependency 'just for now'."
EXECUTE_PROCESS_ADVICE="REMOVE this call or route it through an allowlisted program. execute_process() runs an arbitrary program at configure time - GODS_LAWS.md L-07: STOP and take it to the leader; only pkg-config/pkgconf are allowlisted here, and any other program is opaque to this gate by construction."

readonly CMAKE_SURFACE_PATTERN='(^|/)CMakeLists\.txt$|\.cmake$|\.cmake\.in$'
readonly CXX_SURFACE_PATTERN='\.(cpp|cxx|cc|hpp|hxx|hh|h|ipp)$'

readonly CMAKE_FETCH_PATTERN='^[[:space:]]*(include[[:space:]]*\([[:space:]]*(fetchcontent|externalproject)[[:space:]]*\)|fetchcontent_(declare|makeavailable|populate)[[:space:]]*\(|externalproject_add[[:space:]]*\(|cpm(addpackage|findpackage|declarepackage|getpackage)[[:space:]]*\()'
# CPM's four PUBLIC entry points (CPM.cmake docs), not just the one the
# planning document named as example - REVIEW-DEPZERO-GATE.md achado
# CRITICO #3, 28/08/2026: only CPMAddPackage was covered before.
readonly CMAKE_TOOLCHAIN_PATTERN='^[[:space:]]*(conan_[a-z_]*[[:space:]]*\(|vcpkg[a-z_]*[[:space:]]*\(|include[[:space:]]*\([^)]*(conan|vcpkg)[^)]*\))'
# cmake_language() is blocked UNCONDITIONALLY (any subcommand: CALL,
# EVAL CODE, DEFER, ...), never by allowlist - REVIEW-DEPZERO-GATE.md
# achado CRITICO #2: this project has ZERO legitimate uses of it today
# (measured 28/08/2026), and the command exists precisely to dispatch
# to a NAME COMPUTED AT CONFIGURE TIME (CALL ${fn}) or execute a STRING
# of arbitrary CMake source (EVAL CODE "..."), both opaque to static
# text scanning by construction - there is no "closed by form" middle
# ground here, only "never used, so never allowed".
readonly CMAKE_LANGUAGE_PATTERN='^[[:space:]]*cmake_language[[:space:]]*\('

# DEPZERO-SHALLOW SH-R7, 31/08/2026 (plan section 3): file(DOWNLOAD)/
# file(UPLOAD) is CMake's own native network transfer - unconditional
# block, no allowlist, same reasoning as CMAKE_LANGUAGE_PATTERN above
# (zero legitimate use in this tree, measured 28/08/2026: every real
# file() call uses MAKE_DIRECTORY/SHA256/READ/WRITE/GLOB/GENERATE).
readonly CMAKE_FILE_NETWORK_PATTERN='^[[:space:]]*file[[:space:]]*\([[:space:]]*(download|upload)([[:space:]]|$)'

# DEPZERO-SHALLOW SH-R8: the only two program names execute_process()
# may run without STOPPING and asking the leader - the same two the
# trace's own R8 already allows (tests/tools/check_dep_zero_trace.py).
readonly EXECUTE_PROCESS_PROGRAM_ALLOWLIST="pkg-config pkgconf"

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
    printf '%s\n' "$1" | sed -E 's/^[^(]*\([[:space:]]*([^[:space:])]+).*/\1/'
}

# pkg-config module token may carry a GLUED version comparator
# (wayland-client>=1.20, standard pkg-config syntax) - strips it so the
# base module name can be checked against the allowlist
# (REVIEW-DEPZERO-GATE.md achado IMPORTANTE #6).
pkgconfig_module_base_name() {
    printf '%s\n' "$1" | sed -E 's/(>=|<=|>|<|=).*$//'
}

# --- sub-check (a): CMake surface, one PHYSICAL LINE at a time ---------
# Reads from STDIN (not a filename) so the SAME function serves tree
# mode (cat "$root/$p" | ...) and staged mode (git show ":$p" | ...) -
# GODS_LAWS.md L-12: staged mode must read the INDEX blob, never the
# working tree, and a plain filename argument could not express that.
#
# *** DEPZERO-SHALLOW, 31/08/2026 (leader's order, verbatim: "Deixa
# passar avisando que o servidor decide"): this gate was REBAIXADO from
# a multi-line statement interpreter back to a shallow, per-physical-
# line network. The regra-mae: the physical line, comment stripped, is
# the ONLY evidence. What resolves on that one line still BLOCKS
# exactly as before (find_package(Freetype) with no closing paren
# blocks just as much as the closed form); what does NOT resolve on
# one line - a call whose decisive argument sits on a LATER physical
# line, or is built from a "${var}" this gate cannot read - no longer
# silently passes as "0 violation(s)" the way the pre-28/08/2026 line
# matcher did. It PASSES WITH A PRINTED WARNING (see emit_warning
# below) and is explicitly DEFERRED to the CI oracle (ctest test
# dep_zero_trace, tests/tools/check_dep_zero_trace.py), which reads
# what CMake ACTUALLY executed (expanded arguments, every branch really
# taken) and WILL reprove a real violation there. Two permanent
# warnings are the accepted, measured cost of this on the real tree
# today (cmake/GlintfxWaylandProtocols.cmake and
# cmake/GlintfxPkgConfigValidateInstalled.cmake.in each open
# execute_process() bare and carry COMMAND "${PKG_CONFIG_EXECUTABLE}"
# ..." on the next line) - inert, because dep_zero_trace runs in the
# same suite and is the authority for exactly this shape.
#
# QUOTE-AWARE, single-process comment strip: a bare sed 's/#.*$//'
# truncates INSIDE a double-quoted string, and CPM's own documented
# shorthand ("gh:fmtlib/fmt#1.0") puts a real '#' inside one - stripping
# it there ate the call's closing ')' and silently hid CPMAddPackage
# from the scanner entirely (caught by REVIEW-DEPZERO-GATE.md's own
# conserto, 28/08/2026 - "prova antes de confiar" applied to a fix, not
# just to new code). Toggles a quote flag; '#' only starts a comment
# OUTSIDE quotes. DEPZERO-SHALLOW, 31/08/2026: this used to also count
# '(' / ')' for the now-removed statement-accumulation machine; that
# counting is gone, this returns ONE line of output (the comment-
# stripped text), never three. The line is piped through STDIN, never
# passed via awk's own `-v name=value` - that form runs its OWN
# C-style backslash-escape processing on the value, and this tree has
# real, legitimate lines containing a literal "\${"
# (cmake/GlintfxInstall.cmake:183 etc., CMake's own way to write an
# UN-expanded "${...}" into generated text) - `-v` silently ate the
# backslash and printed a warning on every one of them. Known, declared
# limit: does not understand a backslash-escaped quote inside a string
# (\") - not used anywhere in this tree's CMake files (checked
# 28/08/2026).
strip_cmake_comment() {
    printf '%s\n' "$1" | awk '
    {
        in_quote = 0
        out = ""
        n = length($0)
        for (i = 1; i <= n; i++) {
            c = substr($0, i, 1)
            if (c == "\"") { in_quote = !in_quote; out = out c; continue }
            if (c == "#" && !in_quote) { break }
            out = out c
        }
        print out
    }'
}

cmake_content_violations() {
    display_path="$1"
    line_no=0

    while IFS= read -r raw_line || [ -n "$raw_line" ]; do
        line_no=$((line_no + 1))
        stripped_line="$(strip_cmake_comment "$raw_line")"
        evaluate_cmake_line "$display_path" "$line_no" "$stripped_line" "$raw_line"
    done
}

# Emits a BLOCKING finding: every line of the record carries the 'V|'
# prefix. cmake_content_violations/evaluate_cmake_line run inside a
# $(...) subshell in every caller (check_dep_zero_tree/_staged), so a
# variable set here would never reach the caller - the prefix is the
# channel. Callers split it back out with `sed -n 's/^V|//p'` (never
# grep - GODS_LAWS.md L-45: grep with no match exits 1 and would abort
# the pipeline under `set -eu`; sed -n exits 0 unconditionally).
emit_violation() {
    printf 'V|%s:%s: %s\n' "$1" "$2" "$3"
    printf 'V|  -> %s\n' "$4"
}

# Emits a DEFERRED finding: this call is NOT resolvable from this one
# physical line, so this gate does not clear it - it prints a warning
# and lets the CI oracle (dep_zero_trace) have the last word. The
# literal text is fixed on purpose (GODS_LAWS.md L-40 item 3: a printed
# warning proves "looked, could not decide" is distinguishable from
# silence) and the substring "FAILED" is FORBIDDEN in it - ctest
# registers dep_zero_selftest with FAIL_REGULAR_EXPRESSION "FAILED"
# (tests/CMakeLists.txt), and this warning path exits 0, never 1.
emit_warning() {
    printf 'W|check_dep_zero.sh: WARNING (this is NOT a pass verdict for this call):\n'
    printf 'W|%s:%s: %s\n' "$1" "$2" "$3"
    printf 'W|  -> This call spans lines or builds its decisive argument from a variable, and this shallow, line-by-line gate cannot judge it. It is NOT being cleared here: the CI oracle (ctest test dep_zero_trace, tests/tools/check_dep_zero_trace.py) is the authority that judges what CMake actually executes, and it WILL reprove a violation there (GODS_LAWS.md L-07). Leader'"'"'s decision, 28/08/2026: ambiguous forms pass the hook with this warning so the CI can decide.\n'
}

# Counts WARNING occurrences (not lines - each record is 3 printed
# lines) by counting the fixed header line, closed with `|| true`
# (GODS_LAWS.md L-45: `grep -c` on zero matches exits 1, which would
# abort this assignment under `set -eu` since it is not itself inside
# a conditional).
count_warning_records() {
    [ -z "$1" ] && { echo 0; return; }
    printf '%s\n' "$1" | grep -cF 'WARNING (this is NOT a pass verdict' || true
}

# $1 = a line already known to open a vigiar command at '('. Returns
# the first token after '(' on THIS line, or "" if nothing but
# whitespace/a closing ')' follows - a bare opener. Guards
# first_paren_arg's own known limit (its regex requires at least one
# non-space, non-')' char to match; on a bare "find_package(" it simply
# does not match, and sed then prints the INPUT UNCHANGED - the exact
# "lixo" this gate must never mistake for a real package name).
paren_first_token() {
    printf '%s\n' "$1" | grep -qE '\([[:space:]]*[^[:space:])]' || { echo ""; return; }
    first_paren_arg "$1"
}

# Evaluates ONE physical CMake line, comment already stripped ($3); $4
# is the raw line (comment included) printed in citations. DEPZERO-
# SHALLOW, 31/08/2026: replaces evaluate_cmake_statement - see this
# file's DEPZERO-SHALLOW header note above for the contract this
# implements (block what one line resolves, warn-and-defer what it
# cannot).
evaluate_cmake_line() {
    display_path="$1"
    line_no="$2"
    line="$3"
    raw="$4"

    # A blank/whitespace-only line (a stripped pure-comment line) has
    # nothing to evaluate.
    case "$line" in
        *[![:space:]]*) : ;;
        *) return ;;
    esac

    closed=1
    case "$line" in
        *')'*) : ;;
        *) closed=0 ;;
    esac

    if printf '%s\n' "$line" | grep -qiE "$CMAKE_FETCH_PATTERN"; then
        emit_violation "$display_path" "$line_no" "$raw" "$FETCH_ADVICE"
        return
    fi
    if printf '%s\n' "$line" | grep -qiE "$CMAKE_TOOLCHAIN_PATTERN"; then
        emit_violation "$display_path" "$line_no" "$raw" "$FETCH_ADVICE"
        return
    fi
    if printf '%s\n' "$line" | grep -qiE "$CMAKE_LANGUAGE_PATTERN"; then
        emit_violation "$display_path" "$line_no" "$raw" "$INDIRECTION_ADVICE"
        return
    fi
    if printf '%s\n' "$line" | grep -qiE "$CMAKE_FILE_NETWORK_PATTERN"; then
        emit_violation "$display_path" "$line_no" "$raw" "$FILE_NETWORK_ADVICE"
        return
    fi
    if printf '%s\n' "$line" | grep -qiE '^[[:space:]]*file[[:space:]]*\('; then
        # A DOWNLOAD/UPLOAD subcommand already matched (and returned)
        # above; any OTHER visible subcommand here is a clean pass
        # (F4: MAKE_DIRECTORY, SHA256, READ, WRITE, GLOB, GENERATE, the
        # real forms this tree uses). A BARE opener (nothing visible
        # after '(' on this line) cannot rule out DOWNLOAD/UPLOAD on a
        # later line, so it warns instead of passing silently.
        subcmd="$(paren_first_token "$line")"
        [ -z "$subcmd" ] && emit_warning "$display_path" "$line_no" "$raw"
        return
    fi
    if printf '%s\n' "$line" | grep -qiE '^[[:space:]]*execute_process[[:space:]]*\('; then
        evaluate_execute_process_line "$display_path" "$line_no" "$line" "$raw"
        return
    fi
    if printf '%s\n' "$line" | grep -qiE '^[[:space:]]*include[[:space:]]*\('; then
        evaluate_include_line "$display_path" "$line_no" "$line" "$raw" "$closed"
        return
    fi
    if printf '%s\n' "$line" | grep -qiE '^[[:space:]]*find_package[[:space:]]*\('; then
        name="$(paren_first_token "$line")"
        if [ -z "$name" ]; then
            emit_warning "$display_path" "$line_no" "$raw"
            return
        fi
        # find_package(<allowlisted> ... - the FIRST argument is always
        # the package name; a later argument on a later line can never
        # change it, so a visible, allowlisted name resolves whether or
        # not this line closes (plan section 2.2).
        name_is_known "$name" "$FIND_PACKAGE_ALLOWLIST" || emit_violation "$display_path" "$line_no" "$raw" "$FINDPKG_ADVICE"
        return
    fi
    if printf '%s\n' "$line" | grep -qiE '^[[:space:]]*pkg_check_modules[[:space:]]*\('; then
        evaluate_pkg_check_modules_line "$display_path" "$line_no" "$line" "$raw" "$closed"
        return
    fi
}

# include(): a line with no "${" at all is always fully literal and
# always resolves, closed or not. A line WITH "${" resolves only when
# it ALSO carries a literal ".cmake" fragment (the real, legitimate
# parameterized-file-include shape already used twice in this tree -
# cmake/glintfx-config.cmake.in:5, tests/embed_dll_colocation/
# CMakeLists.txt:43-44 - include("${SOME_DIR}/File.cmake")) AND is
# closed on this same line; unclosed, or "${" with no ".cmake"
# anywhere and closed, are the two remaining cases (warn, block).
evaluate_include_line() {
    display_path="$1"
    line_no="$2"
    line="$3"
    raw="$4"
    closed="$5"

    printf '%s\n' "$line" | grep -qF '${' || return

    if printf '%s\n' "$line" | grep -qi '\.cmake'; then
        [ "$closed" -eq 0 ] && emit_warning "$display_path" "$line_no" "$raw"
        return
    fi

    if [ "$closed" -eq 1 ]; then
        emit_violation "$display_path" "$line_no" "$raw" "$INDIRECTION_ADVICE"
    else
        emit_warning "$display_path" "$line_no" "$raw"
    fi
}

# pkg_check_modules(): any BAD token visible on this line blocks
# regardless of closure (a bad token already visible is resolvable, no
# matter what a later line might also carry). With every visible token
# clean, a CLOSED line passes silently; an unclosed one warns - a later
# line could still carry a token this line never showed.
evaluate_pkg_check_modules_line() {
    display_path="$1"
    line_no="$2"
    line="$3"
    raw="$4"
    closed="$5"

    content="$(printf '%s\n' "$line" | sed -E 's/^[^(]*\(//; s/\)[[:space:]]*$//')"
    rest="$(printf '%s\n' "$content" | sed -E 's/^[[:space:]]*[^[:space:]]+[[:space:]]*//')"
    unknown_hit=0
    for tok in $rest; do
        if name_is_known "$tok" "$PKG_CHECK_MODULES_KEYWORDS"; then
            continue
        fi
        case "$tok" in
            '>='|'<='|'>'|'<'|'=') continue ;;
            [0-9]*) continue ;;
        esac
        base_tok="$(pkgconfig_module_base_name "$tok")"
        if ! name_is_known "$base_tok" "$PKG_CHECK_MODULES_ALLOWLIST"; then
            unknown_hit=1
        fi
    done

    if [ "$unknown_hit" -eq 1 ]; then
        emit_violation "$display_path" "$line_no" "$raw" "$PKGCHECK_ADVICE"
        return
    fi
    [ "$closed" -eq 0 ] && emit_warning "$display_path" "$line_no" "$raw"
}

# execute_process(): finds the program that follows a COMMAND keyword
# ON THIS LINE (optionally quoted - `COMMAND "pkg-config" ...` and
# `COMMAND pkg-config ...` are both real forms). No COMMAND+program
# visible on this line at all (the real tree's own shape - a bare
# opener, COMMAND on line 2) or a program built from "${var}" both
# warn: neither is a literal this gate can judge. A LITERAL program
# resolves: its basename (extension stripped, casefold) decides block
# vs pass against EXECUTE_PROCESS_PROGRAM_ALLOWLIST.
evaluate_execute_process_line() {
    display_path="$1"
    line_no="$2"
    line="$3"
    raw="$4"

    program="$(printf '%s\n' "$line" | sed -nE 's/.*COMMAND[[:space:]]+"?([^")[:space:]]+).*/\1/p')"
    if [ -z "$program" ]; then
        emit_warning "$display_path" "$line_no" "$raw"
        return
    fi
    case "$program" in
        *'${'*) emit_warning "$display_path" "$line_no" "$raw"; return ;;
    esac

    base="$(printf '%s\n' "$program" | sed -E 's|.*/||')"
    lower_base="$(printf '%s\n' "$base" | tr '[:upper:]' '[:lower:]')"
    case "$lower_base" in
        *.exe) lower_base="${lower_base%.exe}" ;;
        *.bat) lower_base="${lower_base%.bat}" ;;
        *.cmd) lower_base="${lower_base%.cmd}" ;;
    esac
    name_is_known "$lower_base" "$EXECUTE_PROCESS_PROGRAM_ALLOWLIST" || emit_violation "$display_path" "$line_no" "$raw" "$EXECUTE_PROCESS_ADVICE"
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
                # REVIEW-DEPZERO-GATE.md achado CRITICO #4, 28/08/2026:
                # `test -f` resolves ".." against the real filesystem,
                # so glintfx/../../../../../../usr/include/zlib.h
                # walked clean off include/ and reached a REAL system
                # header - and the SAME include compiles for real under
                # any -Iinclude toolchain, because the preprocessor
                # resolves <...> the identical way. Reject ANY ".."
                # occurrence before ever touching the filesystem - this
                # is a structural check, not a name list, so there is
                # no allowlist that could rescue a traversal attempt.
                case "$name" in
                    *..*) : ;;
                    *) [ -f "$root/include/$name" ] && continue ;;
                esac
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

    cmake_raw="$(printf '%s\n' "$cmake_files" | while IFS= read -r p; do
        [ -z "$p" ] && continue
        cat "$root/$p" | cmake_content_violations "$root/$p"
    done)"
    cmake_violations="$(printf '%s\n' "$cmake_raw" | sed -n 's/^V|//p')"
    cmake_warnings="$(printf '%s\n' "$cmake_raw" | sed -n 's/^W|//p')"
    include_violations="$(printf '%s\n' "$cxx_files" | while IFS= read -r p; do
        [ -z "$p" ] && continue
        cat "$root/$p" | include_content_violations "$root" "$root/$p"
    done)"

    needed_ok=1
    needed_output=""
    if ! needed_output="$(check_needed_allowlist "$library_path" "$NEEDED_ALLOWLIST" 2>&1)"; then
        needed_ok=0
    fi

    warning_total="$(count_warning_records "$cmake_warnings")"

    if [ -n "$cmake_violations" ] || [ -n "$include_violations" ] || [ "$needed_ok" -eq 0 ]; then
        print_violation_header
        [ -n "$cmake_violations" ] && printf '%s\n' "$cmake_violations" >&2
        [ -n "$include_violations" ] && printf '%s\n' "$include_violations" >&2
        [ "$needed_ok" -eq 0 ] && printf '%s\n' "$needed_output" >&2
        [ -n "$cmake_warnings" ] && printf '%s\n' "$cmake_warnings" >&2
        return 1
    fi

    [ -n "$cmake_warnings" ] && printf '%s\n' "$cmake_warnings" >&2
    echo "check_dep_zero.sh: 0 violation(s), ${warning_total} warning(s) deferred to the CI oracle (dep_zero_trace) - $cmake_count cmake file(s), $cxx_count c++ file(s) scanned"
    printf '%s\n' "$needed_output"
    echo "check_dep_zero.sh: out of scope by design, and not verified by any other gate either: documents (.md), shell scripts (.sh), and quote includes (#include \"...\") are not scanned here. check_spdx.sh only checks for the PRESENCE of an SPDX header string in a file (a vendored third-party file with that string pasted in would still pass it); check_vendor_purity.sh only guards the one named third_party/khronos/ exception from growing, not vendoring in general. Neither closes the quote-include vendor vector - this gate does not either."
    echo "check_dep_zero.sh: ambiguous multi-line or variable-built calls pass this shallow, line-by-line gate with a printed warning; the CI oracle (ctest test dep_zero_trace, tests/tools/check_dep_zero_trace.py) is the authority that judges what CMake actually executes."
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
    cmake_warnings=""
    if [ -n "$cmake_relevant" ]; then
        cmake_raw="$(printf '%s\n' "$cmake_relevant" | while IFS= read -r p; do
            [ -z "$p" ] && continue
            git -C "$root" show ":$p" 2>/dev/null | cmake_content_violations "$root/$p"
        done)"
        cmake_violations="$(printf '%s\n' "$cmake_raw" | sed -n 's/^V|//p')"
        cmake_warnings="$(printf '%s\n' "$cmake_raw" | sed -n 's/^W|//p')"
    fi
    include_violations=""
    if [ -n "$cxx_relevant" ]; then
        include_violations="$(printf '%s\n' "$cxx_relevant" | while IFS= read -r p; do
            [ -z "$p" ] && continue
            git -C "$root" show ":$p" 2>/dev/null | include_content_violations "$root" "$root/$p"
        done)"
    fi

    warning_total="$(count_warning_records "$cmake_warnings")"

    if [ -n "$cmake_violations" ] || [ -n "$include_violations" ]; then
        print_violation_header
        [ -n "$cmake_violations" ] && printf '%s\n' "$cmake_violations" >&2
        [ -n "$include_violations" ] && printf '%s\n' "$include_violations" >&2
        [ -n "$cmake_warnings" ] && printf '%s\n' "$cmake_warnings" >&2
        return 1
    fi

    [ -n "$cmake_warnings" ] && printf '%s\n' "$cmake_warnings" >&2
    echo "check_dep_zero.sh: 0 violation(s), ${warning_total} warning(s) deferred to the CI oracle (dep_zero_trace) - $cmake_count cmake file(s), $cxx_count c++ file(s) among $staged_count staged file(s) scanned"
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

    if ! output="$(check_dep_zero_tree "$root" "NONE" 2>&1)"; then
        echo "selftest: POSITIVE control FAILED (clean fixture should have passed)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    # DEPZERO-SHALLOW piso do contador (N9): a fixture limpa nao tem
    # nenhuma forma ambigua, entao o resumo declara "0 warning(s)" -
    # nunca omite o numero (GODS_LAWS.md L-40 item 3).
    if ! printf '%s\n' "$output" | grep -qF "0 warning(s)"; then
        echo "selftest: POSITIVE control FAILED (passed, but did not declare '0 warning(s)')" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    echo "selftest: POSITIVE control OK (clean fixture, comment mentioning FetchContent, allowlisted find_package/pkg_check_modules, own-header include all passed, 0 warning(s) declared)"
}

selftest_negative_control_cmake() {
    scratch="$1"
    root="$scratch/negative-cmake"
    # DEPZERO-SELFTEST-FIX (28/08/2026): function-owned name, never
    # "overall" - selftest_main also names its own aggregator "overall",
    # and POSIX sh functions share ONE global namespace (no "local").
    # Before this fix, every selftest_* function that used the SAME name
    # "overall" for its own per-case tally silently clobbered whatever
    # verdict selftest_main had already accumulated from EARLIER calls,
    # the instant this function's "overall=0" ran - see GODS_LAWS.md L-40
    # and the fix report for the exact mutation that proved it.
    cmake_forms_status=0

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
            cmake_forms_status=1
        elif ! printf '%s\n' "$output" | grep -qF "$needle"; then
            echo "selftest: NEGATIVE(cmake/$case_name) control FAILED (reproved, but did not cite '$needle')" >&2
            printf '%s\n' "$output" >&2
            cmake_forms_status=1
        fi
        rm -rf "$root"
    done

    [ "$cmake_forms_status" -eq 0 ] && echo "selftest: NEGATIVE(cmake) control OK (seven forms, each reproved and cited)"
    return "$cmake_forms_status"
}

selftest_negative_control_include() {
    scratch="$1"
    root="$scratch/negative-include"
    # DEPZERO-SELFTEST-FIX: see selftest_negative_control_cmake's comment.
    include_forms_status=0

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
            include_forms_status=1
        elif ! printf '%s\n' "$output" | grep -qF "$needle"; then
            echo "selftest: NEGATIVE(include/$case_name) control FAILED (reproved, but did not cite '$needle')" >&2
            printf '%s\n' "$output" >&2
            include_forms_status=1
        fi
        rm -rf "$root"
    done

    [ "$include_forms_status" -eq 0 ] && echo "selftest: NEGATIVE(include) control OK (three forms, each reproved and cited)"
    return "$include_forms_status"
}

# Fixture .so's compiled with the system cc, in scratch - depends on no
# package inside the container (plan section 4, caminho 1).
# --- REVIEW-DEPZERO-GATE conserto (SHA e07a12f reprovado, 4 CRITICO):
# sete controles novos, um por achado, cobrindo a FAMILIA da forma que
# escapava, nao so a string exata do revisor. Adicionados ANTES do
# conserto (GODS_LAWS.md L-20: vermelho real capturado contra o codigo
# ainda nao corrigido, colado no relatorio ao lider).

# CRITICO #1 historico (multi-line escapava o antigo matcher linha-a-
# linha por completo, "0 violation(s)" para um freetype2 escondido).
# DEPZERO-SHALLOW, 31/08/2026: a maquina de reagrupamento por parenteses
# que resolvia isso foi removida (rebaixamento, ordem do lider); as
# QUATRO formas desta fixture continuam existindo tal como estao - sao
# a memoria adversarial do achado - mas a expectativa muda: nenhuma
# delas resolve numa linha so, entao nenhuma BLOQUEIA mais. Cada uma
# agora passa (exit 0) com um AVISO impresso, deferindo ao oraculo do
# CI (dep_zero_trace) - exatamente a decisao do lider (verbatim,
# 28/08/2026: "Deixa passar avisando que o servidor decide"). O aviso
# cita a linha do ABRIDOR (ex.: "pkg_check_modules(" ou "find_package("
# sozinha), onde "freetype2" NAO aparece - por isso o needle mudou de
# per-caso para as quatro asserções fixas do contrato (plan secao 2.4).
selftest_warn_control_cmake_multiline() {
    scratch="$1"
    root="$scratch/warn-cmake-multiline"
    # DEPZERO-SELFTEST-FIX: see selftest_negative_control_cmake's comment.
    multiline_forms_status=0

    for case_name in pkgcheck_multiline pkgcheck_multiline_comment pkgcheck_multiline_mixedcase findpkg_multiline_unknown; do
        make_clean_fixture "$root"
        case "$case_name" in
            pkgcheck_multiline)
                cat >> "$root/cmake/Wayland.cmake" <<'EOF'
pkg_check_modules(
    Fixture
    REQUIRED
    freetype2)
EOF
                ;;
            pkgcheck_multiline_comment)
                cat >> "$root/cmake/Wayland.cmake" <<'EOF'
pkg_check_modules(
    Fixture
    # module list follows
    REQUIRED
    freetype2
)
EOF
                ;;
            pkgcheck_multiline_mixedcase)
                cat >> "$root/cmake/Wayland.cmake" <<'EOF'
Pkg_Check_Modules (
    Fixture
    REQUIRED
    freetype2
)
EOF
                ;;
            findpkg_multiline_unknown)
                cat >> "$root/cmake/Wayland.cmake" <<'EOF'
find_package(
    Freetype
    REQUIRED
)
EOF
                ;;
        esac
        git_init_fixture "$root"

        if ! output="$(check_dep_zero_tree "$root" "NONE" 2>&1)"; then
            echo "selftest: WARN(cmake-multiline/$case_name) control FAILED (an unresolvable-on-one-line form must PASS with a warning, not be reproved)" >&2
            printf '%s\n' "$output" >&2
            multiline_forms_status=1
        elif ! printf '%s\n' "$output" | grep -qF "WARNING"; then
            echo "selftest: WARN(cmake-multiline/$case_name) control FAILED (passed, but did not print WARNING)" >&2
            printf '%s\n' "$output" >&2
            multiline_forms_status=1
        elif ! printf '%s\n' "$output" | grep -qF "deferred"; then
            echo "selftest: WARN(cmake-multiline/$case_name) control FAILED (passed with a warning, but did not say 'deferred')" >&2
            printf '%s\n' "$output" >&2
            multiline_forms_status=1
        elif ! printf '%s\n' "$output" | grep -qF "Wayland.cmake"; then
            echo "selftest: WARN(cmake-multiline/$case_name) control FAILED (passed with a warning, but did not cite Wayland.cmake)" >&2
            printf '%s\n' "$output" >&2
            multiline_forms_status=1
        elif printf '%s\n' "$output" | grep -qF "PROHIBITED"; then
            echo "selftest: WARN(cmake-multiline/$case_name) control FAILED (passed with exit 0, but ALSO printed PROHIBITED - the warning must never be a silent block)" >&2
            printf '%s\n' "$output" >&2
            multiline_forms_status=1
        fi
        rm -rf "$root"
    done

    [ "$multiline_forms_status" -eq 0 ] && echo "selftest: WARN(cmake-multiline) control OK (four multi-line/format-varied forms, each passes with a printed warning deferred to the CI oracle, never blocked)"
    return "$multiline_forms_status"
}

# IMPORTANTE #5 (falso positivo: chamada multi-linha ALLOWLISTED nao
# pode reprovar - senao o primeiro CMake formatado "bonito" desliga o
# portao). DEPZERO-SHALLOW, 31/08/2026: cobre find_package E
# pkg_check_modules multi-linha, os dois ja permitidos hoje - mas com
# os abridores NUS (nada mais na mesma linha), a rede rasa nao pode
# mais RESOLVER a linha sozinha; o resultado agora e exit 0 + AVISO
# (nunca PROHIBITED), nao mais um passe silencioso.
selftest_positive_control_cmake_multiline() {
    scratch="$1"
    root="$scratch/positive-cmake-multiline"
    make_clean_fixture "$root"
    cat > "$root/cmake/Wayland.cmake" <<'EOF'
find_package(
    PkgConfig
    REQUIRED
)
pkg_check_modules(
    FixtureWayland
    REQUIRED
    wayland-client
)
EOF
    git_init_fixture "$root"

    if ! output="$(check_dep_zero_tree "$root" "NONE" 2>&1)"; then
        echo "selftest: POSITIVE(cmake-multiline) control FAILED (allowlisted find_package/pkg_check_modules multi-linha deveria passar, mesmo com aviso)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    if ! printf '%s\n' "$output" | grep -qF "WARNING"; then
        echo "selftest: POSITIVE(cmake-multiline) control FAILED (passou, mas abridor nu sem evidencia na linha deveria ter avisado)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    echo "selftest: POSITIVE(cmake-multiline) control OK (find_package e pkg_check_modules multi-linha, abridor nu, passam com aviso deferido)"
}

# Caso NOVO do plano DEPZERO-SHALLOW secao 2.2, irmao do controle
# acima: quando o abridor JA carrega a evidencia decisiva na mesma
# linha - find_package(PkgConfig sem fechar (primeiro argumento
# sempre resolve o nome do pacote, closed ou nao) e
# pkg_check_modules(FixtureWayland REQUIRED wayland-client) fechada em
# UMA linha - a rede rasa resolve de verdade e passa SEM aviso nenhum.
selftest_positive_control_cmake_opener_resolved() {
    scratch="$1"
    root="$scratch/positive-cmake-opener-resolved"
    make_clean_fixture "$root"
    cat > "$root/cmake/Wayland.cmake" <<'EOF'
find_package(PkgConfig
    REQUIRED
)
pkg_check_modules(FixtureWayland REQUIRED wayland-client)
EOF
    git_init_fixture "$root"

    if ! output="$(check_dep_zero_tree "$root" "NONE" 2>&1)"; then
        echo "selftest: POSITIVE(cmake-opener-resolved) control FAILED (abridor com evidencia decisiva na propria linha deveria passar)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    if printf '%s\n' "$output" | grep -qF "WARNING"; then
        echo "selftest: POSITIVE(cmake-opener-resolved) control FAILED (abridor ja resolvido na linha nao deveria gerar aviso nenhum)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    echo "selftest: POSITIVE(cmake-opener-resolved) control OK (find_package(PkgConfig e pkg_check_modules(...wayland-client) fechada resolvem na propria linha, sem aviso)"
}

# IMPORTANTE #6 (restricao de versao reprova modulo ja permitido):
# forma colada (wayland-client>=1.20) E forma separada em tres tokens
# (wayland-client >= 1.20) - as duas sao sintaxe padrao do pkg-config.
# Controle NEGATIVO irmao: modulo DESCONHECIDO com restricao de versao
# continua reprovando (prova que o corte de sufixo nao abre allowlist
# para nomes errados).
selftest_positive_control_pkgcheck_version() {
    scratch="$1"
    root="$scratch/positive-pkgcheck-version"
    # DEPZERO-SELFTEST-FIX: see selftest_negative_control_cmake's comment.
    pkgcheck_version_status=0

    for case_name in glued spaced; do
        make_clean_fixture "$root"
        case "$case_name" in
            glued) printf 'pkg_check_modules(FixtureWayland REQUIRED wayland-client>=1.20)\n' > "$root/cmake/Wayland.cmake" ;;
            spaced) printf 'pkg_check_modules(FixtureWayland REQUIRED wayland-client >= 1.20)\n' > "$root/cmake/Wayland.cmake" ;;
        esac
        git_init_fixture "$root"

        if ! output="$(check_dep_zero_tree "$root" "NONE" 2>&1)"; then
            echo "selftest: POSITIVE(pkgcheck-version/$case_name) control FAILED (wayland-client com restricao de versao deveria passar)" >&2
            printf '%s\n' "$output" >&2
            pkgcheck_version_status=1
        fi
        rm -rf "$root"
    done

    [ "$pkgcheck_version_status" -eq 0 ] && echo "selftest: POSITIVE(pkgcheck-version) control OK (forma colada e forma separada, modulo ja permitido, passam)"
    return "$pkgcheck_version_status"
}

selftest_negative_control_pkgcheck_version_unknown() {
    scratch="$1"
    root="$scratch/negative-pkgcheck-version-unknown"
    make_clean_fixture "$root"
    printf 'pkg_check_modules(Fixture REQUIRED freetype2>=2.10)\n' > "$root/cmake/Wayland.cmake"
    git_init_fixture "$root"

    if output="$(check_dep_zero_tree "$root" "NONE" 2>&1)"; then
        echo "selftest: NEGATIVE(pkgcheck-version-unknown) control FAILED (freetype2>=2.10 deveria reprovar)" >&2
        return 1
    fi
    if ! printf '%s\n' "$output" | grep -qF "freetype2"; then
        echo "selftest: NEGATIVE(pkgcheck-version-unknown) control FAILED (reprovou, mas nao citou freetype2)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    echo "selftest: NEGATIVE(pkgcheck-version-unknown) control OK (modulo desconhecido com versao continua reprovando)"
}

# CRITICO #2 (indirecao de variavel burla FetchContent por completo):
# include(${var}) puro, cmake_language(CALL ${var} ...), cmake_language
# (EVAL CODE "..."), caixa mista, espaco antes do parenteses. Controle
# POSITIVO irmao: os DOIS usos REAIS e legitimos deste padrao na arvore
# hoje (include("${VAR}/arquivo.cmake") - caminho parametrizado com
# sufixo .cmake literal) continuam passando - sem isso o conserto
# quebraria CMakeLists.txt/tests/embed_dll_colocation de verdade.
selftest_negative_control_cmake_indirection() {
    scratch="$1"
    root="$scratch/negative-cmake-indirection"
    # DEPZERO-SELFTEST-FIX: see selftest_negative_control_cmake's comment.
    indirection_forms_status=0

    for case_name in include_bare_var cmake_language_call cmake_language_eval mixedcase_call spaced_call; do
        make_clean_fixture "$root"
        case "$case_name" in
            include_bare_var)
                cat >> "$root/cmake/Wayland.cmake" <<'EOF'
set(mod_name "FetchContent")
include(${mod_name})
EOF
                needle="include(\${mod_name})"
                ;;
            cmake_language_call)
                cat >> "$root/cmake/Wayland.cmake" <<'EOF'
set(fn_name "FetchContent_Declare")
cmake_language(CALL ${fn_name} fmt GIT_REPOSITORY https://example.invalid/fmt.git)
EOF
                needle="cmake_language"
                ;;
            cmake_language_eval)
                cat >> "$root/cmake/Wayland.cmake" <<'EOF'
cmake_language(EVAL CODE "include(FetchContent)")
EOF
                needle="cmake_language"
                ;;
            mixedcase_call)
                cat >> "$root/cmake/Wayland.cmake" <<'EOF'
Cmake_Language(CALL FetchContent_Declare fmt)
EOF
                needle="Cmake_Language"
                ;;
            spaced_call)
                cat >> "$root/cmake/Wayland.cmake" <<'EOF'
cmake_language (CALL FetchContent_Declare fmt)
EOF
                needle="cmake_language"
                ;;
        esac
        git_init_fixture "$root"

        if output="$(check_dep_zero_tree "$root" "NONE" 2>&1)"; then
            echo "selftest: NEGATIVE(cmake-indirection/$case_name) control FAILED (deveria ter reprovado)" >&2
            indirection_forms_status=1
        elif ! printf '%s\n' "$output" | grep -qiF "$needle"; then
            echo "selftest: NEGATIVE(cmake-indirection/$case_name) control FAILED (reprovou, mas nao citou '$needle')" >&2
            printf '%s\n' "$output" >&2
            indirection_forms_status=1
        fi
        rm -rf "$root"
    done

    [ "$indirection_forms_status" -eq 0 ] && echo "selftest: NEGATIVE(cmake-indirection) control OK (cinco formas de indirecao, cada uma reprovada e citada)"
    return "$indirection_forms_status"
}

selftest_positive_control_cmake_indirection_legit() {
    scratch="$1"
    root="$scratch/positive-cmake-indirection-legit"
    make_clean_fixture "$root"
    cat > "$root/cmake/Wayland.cmake" <<'EOF'
find_package(PkgConfig REQUIRED)
pkg_check_modules(FixtureWayland REQUIRED wayland-client)
include("${CMAKE_CURRENT_LIST_DIR}/glintfxTargets.cmake")
EOF
    git_init_fixture "$root"

    if ! output="$(check_dep_zero_tree "$root" "NONE" 2>&1)"; then
        echo "selftest: POSITIVE(cmake-indirection-legit) control FAILED (include(\"\${VAR}/arquivo.cmake\") e o padrao REAL usado em cmake/glintfx-config.cmake.in e tests/embed_dll_colocation/ - nao pode reprovar)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    echo "selftest: POSITIVE(cmake-indirection-legit) control OK (include parametrizado com sufixo .cmake literal, o padrao real da arvore, passa)"
}

# CRITICO #3 (blocklist do CPM incompleta - so CPMAddPackage estava
# coberta; API publica tem quatro entradas).
selftest_negative_control_cpm_variants() {
    scratch="$1"
    root="$scratch/negative-cpm-variants"
    # DEPZERO-SELFTEST-FIX: see selftest_negative_control_cmake's comment.
    cpm_forms_status=0

    for fn in CPMFindPackage CPMDeclarePackage CPMGetPackage; do
        make_clean_fixture "$root"
        printf '%s(NAME fmt GIT_REPOSITORY https://example.invalid/fmt.git GIT_TAG 1.0)\n' "$fn" >> "$root/cmake/Wayland.cmake"
        git_init_fixture "$root"

        if output="$(check_dep_zero_tree "$root" "NONE" 2>&1)"; then
            echo "selftest: NEGATIVE(cpm-variants/$fn) control FAILED (deveria ter reprovado)" >&2
            cpm_forms_status=1
        elif ! printf '%s\n' "$output" | grep -qiF "$fn"; then
            echo "selftest: NEGATIVE(cpm-variants/$fn) control FAILED (reprovou, mas nao citou '$fn')" >&2
            printf '%s\n' "$output" >&2
            cpm_forms_status=1
        fi
        rm -rf "$root"
    done

    [ "$cpm_forms_status" -eq 0 ] && echo "selftest: NEGATIVE(cpm-variants) control OK (CPMFindPackage/CPMDeclarePackage/CPMGetPackage, cada uma reprovada e citada)"
    return "$cpm_forms_status"
}

# DEPZERO-SHALLOW, 31/08/2026, secao 2.1 itens 5-6: um token RUIM ja
# visivel na propria linha do abridor RESOLVE, mesmo sem ')' fechando -
# closed nao importa quando ja ha evidencia decisiva de bloqueio.
# Prova que a rede rasa nao vira permissiva so porque a chamada nao
# fecha na mesma linha.
selftest_negative_control_opener_carries_bad_token() {
    scratch="$1"
    root="$scratch/negative-opener-bad-token"
    # DEPZERO-SELFTEST-FIX: see selftest_negative_control_cmake's comment.
    opener_bad_token_status=0

    for case_name in pkgcheck_unclosed_bad findpkg_unclosed_bad; do
        make_clean_fixture "$root"
        case "$case_name" in
            pkgcheck_unclosed_bad)
                printf 'pkg_check_modules(Fixture REQUIRED freetype2\n' >> "$root/cmake/Wayland.cmake"
                needle="freetype2"
                ;;
            findpkg_unclosed_bad)
                printf 'find_package(Freetype\n' >> "$root/cmake/Wayland.cmake"
                needle="find_package(Freetype"
                ;;
        esac
        git_init_fixture "$root"

        if output="$(check_dep_zero_tree "$root" "NONE" 2>&1)"; then
            echo "selftest: NEGATIVE(opener-bad-token/$case_name) control FAILED (token ruim visivel na propria linha deveria ter reprovado, mesmo sem fechar)" >&2
            opener_bad_token_status=1
        elif ! printf '%s\n' "$output" | grep -qF "$needle"; then
            echo "selftest: NEGATIVE(opener-bad-token/$case_name) control FAILED (reprovou, mas nao citou '$needle')" >&2
            printf '%s\n' "$output" >&2
            opener_bad_token_status=1
        fi
        rm -rf "$root"
    done

    [ "$opener_bad_token_status" -eq 0 ] && echo "selftest: NEGATIVE(opener-bad-token) control OK (find_package/pkg_check_modules sem fechar, com token ruim ja visivel, cada um reprovado)"
    return "$opener_bad_token_status"
}

# DEPZERO-SHALLOW N1, 31/08/2026 - SH-R7 (plano secao 3): fecha o
# buraco herdado do trace (R7 so ve o que o CMake de fato EXECUTA; um
# ramo condicional nao tomado escapa dos dois oraculos hoje). Duas
# formas de 1 linha, subcomando visivel - bloqueio INCONDICIONAL, sem
# allowlist (nenhum uso legitimo hoje).
selftest_negative_control_file_network() {
    scratch="$1"
    root="$scratch/negative-file-network"
    # DEPZERO-SELFTEST-FIX: see selftest_negative_control_cmake's comment.
    file_network_forms_status=0

    for case_name in download upload; do
        make_clean_fixture "$root"
        case "$case_name" in
            download)
                printf 'file(DOWNLOAD https://example.invalid/x o)\n' >> "$root/cmake/Wayland.cmake"
                needle="DOWNLOAD"
                ;;
            upload)
                printf 'file(UPLOAD o https://example.invalid/x)\n' >> "$root/cmake/Wayland.cmake"
                needle="UPLOAD"
                ;;
        esac
        git_init_fixture "$root"

        if output="$(check_dep_zero_tree "$root" "NONE" 2>&1)"; then
            echo "selftest: NEGATIVE(file-network/$case_name) control FAILED (should have been reproved)" >&2
            file_network_forms_status=1
        elif ! printf '%s\n' "$output" | grep -qF "$needle"; then
            echo "selftest: NEGATIVE(file-network/$case_name) control FAILED (reproved, but did not cite '$needle')" >&2
            printf '%s\n' "$output" >&2
            file_network_forms_status=1
        fi
        rm -rf "$root"
    done

    [ "$file_network_forms_status" -eq 0 ] && echo "selftest: NEGATIVE(file-network) control OK (file(DOWNLOAD)/file(UPLOAD), cada um reprovado e citado)"
    return "$file_network_forms_status"
}

# DEPZERO-SHALLOW N2 - o irmao positivo: os subcomandos REAIS que a
# arvore usa hoje (F4: MAKE_DIRECTORY, SHA256, GENERATE, entre outros)
# nunca sao DOWNLOAD/UPLOAD, e passam sem aviso nenhum - a rede rasa
# nao pode gritar contra o proprio uso legitimo de file().
selftest_positive_control_file_legit() {
    scratch="$1"
    root="$scratch/positive-file-legit"
    # DEPZERO-SELFTEST-FIX: see selftest_negative_control_cmake's comment.
    file_legit_forms_status=0

    for case_name in make_directory sha256 generate; do
        make_clean_fixture "$root"
        case "$case_name" in
            make_directory) printf 'file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")\n' >> "$root/cmake/Wayland.cmake" ;;
            sha256) printf 'file(SHA256 "${CMAKE_CURRENT_LIST_FILE}" out_hash)\n' >> "$root/cmake/Wayland.cmake" ;;
            generate) printf 'file(GENERATE OUTPUT out.txt CONTENT "hi")\n' >> "$root/cmake/Wayland.cmake" ;;
        esac
        git_init_fixture "$root"

        if ! output="$(check_dep_zero_tree "$root" "NONE" 2>&1)"; then
            echo "selftest: POSITIVE(file-legit/$case_name) control FAILED (deveria ter passado)" >&2
            printf '%s\n' "$output" >&2
            file_legit_forms_status=1
        elif printf '%s\n' "$output" | grep -qF "WARNING"; then
            echo "selftest: POSITIVE(file-legit/$case_name) control FAILED (subcomando visivel e resolvido, nao deveria gerar aviso)" >&2
            printf '%s\n' "$output" >&2
            file_legit_forms_status=1
        fi
        rm -rf "$root"
    done

    [ "$file_legit_forms_status" -eq 0 ] && echo "selftest: POSITIVE(file-legit) control OK (MAKE_DIRECTORY/SHA256/GENERATE, os subcomandos reais da arvore, passam sem aviso)"
    return "$file_legit_forms_status"
}

# DEPZERO-SHALLOW N3 - abridor NU (nada mais na linha), DOWNLOAD numa
# linha posterior: nao resolve numa linha so, entao AVISA em vez de
# passar em silencio (prova que a forma escondida nao escapa mudo).
selftest_warn_control_file_opener_bare() {
    scratch="$1"
    root="$scratch/warn-file-opener-bare"
    make_clean_fixture "$root"
    cat >> "$root/cmake/Wayland.cmake" <<'EOF'
file(
    DOWNLOAD
    https://example.invalid/x
    o
)
EOF
    git_init_fixture "$root"

    if ! output="$(check_dep_zero_tree "$root" "NONE" 2>&1)"; then
        echo "selftest: WARN(file-opener-bare) control FAILED (abridor nu deveria passar com aviso, nao reprovar)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    if ! printf '%s\n' "$output" | grep -qF "WARNING"; then
        echo "selftest: WARN(file-opener-bare) control FAILED (passou, mas nao avisou)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    echo "selftest: WARN(file-opener-bare) control OK (file( abridor nu, DOWNLOAD numa linha posterior, passa com aviso deferido)"
}

# DEPZERO-SHALLOW N4, 31/08/2026 - SH-R8 (plano secao 3): mesmo buraco
# herdado, agora para execute_process(). Programa LITERAL fora da
# allowlist {pkg-config, pkgconf} bloqueia, mesmo sem allowlist de
# nomes de programa em geral - so os dois nomes que a arvore usa hoje
# (mesmos dois do R8 do trace) sao aceitos.
selftest_negative_control_execute_process() {
    scratch="$1"
    root="$scratch/negative-execute-process"
    # DEPZERO-SELFTEST-FIX: see selftest_negative_control_cmake's comment.
    execute_process_forms_status=0

    for case_name in curl git; do
        make_clean_fixture "$root"
        case "$case_name" in
            curl) printf 'execute_process(COMMAND curl https://example.invalid/x)\n' >> "$root/cmake/Wayland.cmake"; needle="curl" ;;
            git) printf 'execute_process(COMMAND git clone https://example.invalid/x)\n' >> "$root/cmake/Wayland.cmake"; needle="git" ;;
        esac
        git_init_fixture "$root"

        if output="$(check_dep_zero_tree "$root" "NONE" 2>&1)"; then
            echo "selftest: NEGATIVE(execute-process/$case_name) control FAILED (should have been reproved)" >&2
            execute_process_forms_status=1
        elif ! printf '%s\n' "$output" | grep -qF "$needle"; then
            echo "selftest: NEGATIVE(execute-process/$case_name) control FAILED (reproved, but did not cite '$needle')" >&2
            printf '%s\n' "$output" >&2
            execute_process_forms_status=1
        fi
        rm -rf "$root"
    done

    [ "$execute_process_forms_status" -eq 0 ] && echo "selftest: NEGATIVE(execute-process) control OK (curl/git via execute_process, cada um reprovado e citado)"
    return "$execute_process_forms_status"
}

# DEPZERO-SHALLOW N5 - o programa literal allowlisted (pkg-config, o
# unico uso real da arvore fora do proprio glintfx) passa sem aviso.
selftest_positive_control_execute_process_pkgconfig() {
    scratch="$1"
    root="$scratch/positive-execute-process-pkgconfig"
    make_clean_fixture "$root"
    printf 'execute_process(COMMAND pkg-config --exists foo)\n' >> "$root/cmake/Wayland.cmake"
    git_init_fixture "$root"

    if ! output="$(check_dep_zero_tree "$root" "NONE" 2>&1)"; then
        echo "selftest: POSITIVE(execute-process-pkgconfig) control FAILED (pkg-config literal deveria passar)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    if printf '%s\n' "$output" | grep -qF "WARNING"; then
        echo "selftest: POSITIVE(execute-process-pkgconfig) control FAILED (programa literal e resolvido, nao deveria gerar aviso)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    echo "selftest: POSITIVE(execute-process-pkgconfig) control OK (pkg-config literal na propria linha passa sem aviso)"
}

# DEPZERO-SHALLOW N6 - a forma REAL desta arvore (F4:
# cmake/GlintfxWaylandProtocols.cmake e
# cmake/GlintfxPkgConfigValidateInstalled.cmake.in): abridor nu,
# COMMAND "${PKG_CONFIG_EXECUTABLE}" ... numa linha posterior. O
# programa e uma VARIAVEL, nao um literal - esta linha nao resolve
# sozinha, entao avisa e defere ao trace (que expande a variavel de
# verdade).
selftest_warn_control_execute_process_variable() {
    scratch="$1"
    root="$scratch/warn-execute-process-variable"
    make_clean_fixture "$root"
    cat >> "$root/cmake/Wayland.cmake" <<'EOF'
execute_process(
    COMMAND "${PKG_CONFIG_EXECUTABLE}" --exists foo
    RESULT_VARIABLE r
)
EOF
    git_init_fixture "$root"

    if ! output="$(check_dep_zero_tree "$root" "NONE" 2>&1)"; then
        echo "selftest: WARN(execute-process-variable) control FAILED (abridor nu com COMMAND em variavel numa linha posterior deveria passar com aviso, nao reprovar)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    if ! printf '%s\n' "$output" | grep -qF "WARNING"; then
        echo "selftest: WARN(execute-process-variable) control FAILED (passou, mas nao avisou)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    echo "selftest: WARN(execute-process-variable) control OK (a forma real da arvore - abridor nu, programa em variavel numa linha posterior - passa com aviso deferido)"
}

# CRITICO #4 (travessia de caminho engana a regra 3 estrutural, E o
# include resultante COMPILA de verdade). Quatro formas: a exata do
# revisor, uma mais curta, um "." solto no meio, e ".." sozinho como
# segmento final.
selftest_negative_control_include_traversal() {
    scratch="$1"
    root="$scratch/negative-include-traversal"
    # DEPZERO-SELFTEST-FIX: see selftest_negative_control_cmake's comment.
    # (This is the ONE function of the eight whose "overall=0" happened
    # to sit LAST in selftest_main's call sequence - nothing after it
    # could clobber the shared name, which is exactly why it was the
    # only one of the eight that already reported correctly before this
    # fix. Renaming it too closes the fragility for good: any future
    # call added AFTER it in selftest_main would otherwise reopen the
    # same hole this fatia exists to close.)
    traversal_forms_status=0

    for case_name in deep_traversal short_traversal dot_and_traversal trailing_dotdot; do
        make_clean_fixture "$root"
        case "$case_name" in
            deep_traversal) inc='#include <glintfx/../../../../../../usr/include/zlib.h>' ;;
            short_traversal) inc='#include <glintfx/../../etc/passwd>' ;;
            dot_and_traversal) inc='#include <glintfx/core/./../../../../../etc/passwd>' ;;
            trailing_dotdot) inc='#include <glintfx/..>' ;;
        esac
        printf '%s\n' "$inc" >> "$root/src/core/err.cpp"
        git_init_fixture "$root"

        if output="$(check_dep_zero_tree "$root" "NONE" 2>&1)"; then
            echo "selftest: NEGATIVE(include-traversal/$case_name) control FAILED (deveria ter reprovado)" >&2
            traversal_forms_status=1
        elif ! printf '%s\n' "$output" | grep -qF '..'; then
            echo "selftest: NEGATIVE(include-traversal/$case_name) control FAILED (reprovou, mas a citacao nao mostra a travessia)" >&2
            printf '%s\n' "$output" >&2
            traversal_forms_status=1
        fi
        rm -rf "$root"
    done

    [ "$traversal_forms_status" -eq 0 ] && echo "selftest: NEGATIVE(include-traversal) control OK (quatro formas de travessia, cada uma reprovada)"
    return "$traversal_forms_status"
}

# IMPORTANTE #7 (a linha de fora-de-escopo superestima cobertura real
# de check_spdx.sh/check_vendor_purity.sh). Controle de TEXTO: a
# mensagem nao pode alegar que os dois gates "cobrem" o vetor de
# include-por-aspas de vendor - e tem de dizer o que eles REALMENTE
# fazem (presenca de string SPDX; so o diretorio nomeado da excecao
# Khronos).
selftest_scope_message_is_honest() {
    scratch="$1"
    root="$scratch/scope-message"
    make_clean_fixture "$root"
    git_init_fixture "$root"

    output="$(check_dep_zero_tree "$root" "NONE" 2>&1)" || {
        echo "selftest: SCOPE-MESSAGE control FAILED (fixture limpa deveria passar antes de examinar o texto)" >&2
        printf '%s\n' "$output" >&2
        return 1
    }
    if printf '%s\n' "$output" | grep -qF "vendor vector covered by"; then
        echo "selftest: SCOPE-MESSAGE control FAILED (ainda alega 'covered by' - superestima a cobertura real dos dois gates citados)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    if ! printf '%s\n' "$output" | grep -qF "quote includes"; then
        echo "selftest: SCOPE-MESSAGE control FAILED (nao declara mais que includes de aspas ficam fora do escopo)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    # DEPZERO-SHALLOW, 31/08/2026: o fechamento tambem declara o
    # contrato novo - forma ambigua passa com aviso, o oraculo do CI
    # (dep_zero_trace) e quem decide de verdade.
    if ! printf '%s\n' "$output" | grep -qF "the CI oracle"; then
        echo "selftest: SCOPE-MESSAGE control FAILED (nao declara o contrato novo: aviso deferido ao oraculo do CI)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    echo "selftest: SCOPE-MESSAGE control OK (a linha de fora-de-escopo nao alega cobertura que os dois gates citados nao tem, e declara o contrato de aviso deferido)"
}

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

    # DEPZERO-TRACE, achado do bloqueador de estreia de 29/08/2026,
    # segunda rodada (GHA run 33249693240, branch depzero-gate):
    # "portao que congela um fato do ambiente onde nasceu", terceira
    # encarnacao nesta fatia. A fixture ORIGINAL era `int clean_fn(void)
    # { return 42; }` - uma funcao que nao referencia NADA da libc.
    # Medido ao vivo, 29/08/2026, em container fresco de cada distro
    # (nao presumido): no Fedora e no Arch, `cc -shared -fPIC` ainda
    # assim emite NEEDED libc.so.6 (o proprio crt de inicializacao
    # referencia simbolos da libc mesmo sem chamada explicita no C);
    # no Ubuntu/Debian, o gcc tem `--as-needed` LIGADO por padrao nas
    # specs de distro (um patch de longa data do Debian, nao um
    # comportamento generico do GCC), e o linker OMITE libc.so.6 por
    # inteiro quando nenhum simbolo dela e de fato referenciado - a
    # biblioteca resultante tem ZERO entradas NEEDED. O piso da L-40
    # ("0 entradas NEEDED e sempre suspeito, nunca 'limpo'") entao
    # reprova - corretamente, pelo proprio desenho do piso: uma
    # biblioteca com zero NEEDED e mesmo anomala, so que a fixture
    # original produzia essa anomalia por acidente de plataforma, nao
    # por um defeito real. Conserto na FIXTURE, nao no piso nem no
    # allowlist: a funcao agora chama strlen() de <string.h>, uma
    # referencia real e minima a libc, que sobrevive a --as-needed em
    # QUALQUER plataforma - confirmado ao vivo, Fedora/Ubuntu/Arch,
    # os tres emitem exatamente um NEEDED (libc.so.6), ja na allowlist.
    printf '#include <string.h>\nsize_t clean_fn(const char *s) { return strlen(s); }\n' > "$root/clean.c"
    cc -shared -fPIC -o "$root/libclean.so" "$root/clean.c" \
        || { echo "selftest: POSITIVE(needed) control SKIPPED (cc unavailable to build fixture .so)"; return 0; }

    # A plain cc -shared that references one real libc symbol links
    # only libc.so.6, which is on the real allowlist - proves the
    # sub-check does not scream at ordinary, allowed linkage, on any
    # of this project's supported Linux distributions (not just the
    # one the fixture was originally measured on).
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

# DEPZERO-SHALLOW N8, 31/08/2026: prova a decisao do lider na forma
# EXATA do gancho - um pkg_check_modules( multi-linha staged, com
# freetype2 escondida numa linha posterior, PASSA (exit 0) com um
# aviso impresso, nunca bloqueia o commit. E a mesma decisao de
# selftest_warn_control_cmake_multiline, mas contra check_dep_zero_staged
# (o gancho de verdade), nao contra o modo tree.
selftest_staged_warns_ambiguous() {
    scratch="$1"
    root="$scratch/staged-warns-ambiguous"
    make_clean_fixture "$root"
    git_init_fixture "$root"
    cat >> "$root/cmake/Wayland.cmake" <<'EOF'
pkg_check_modules(
    Fixture
    REQUIRED
    freetype2
)
EOF
    git -C "$root" add cmake/Wayland.cmake

    if ! output="$(check_dep_zero_staged "$root" 2>&1)"; then
        echo "selftest: STAGED(warns-ambiguous) control FAILED (forma ambigua multi-linha deveria passar com aviso, nao bloquear o commit)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    if ! printf '%s\n' "$output" | grep -qF "WARNING"; then
        echo "selftest: STAGED(warns-ambiguous) control FAILED (passou, mas nao avisou)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    echo "selftest: STAGED(warns-ambiguous) control OK (pkg_check_modules multi-linha com freetype2 escondida passa o gancho com aviso, decisao do lider 28/08/2026)"
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

# DEPZERO-SHALLOW N9, 31/08/2026: o contador de avisos aparece SEMPRE
# no resumo, inclusive quando zero - "0 warning(s)" nao pode ser
# indistinguivel de "o canal de aviso quebrou e nao contou nada"
# (GODS_LAWS.md L-40 item 3). Prova as duas pontas: fixture limpa
# declara "0 warning(s)"; fixture com uma forma ambigua declara
# "1 warning(s)".
selftest_warn_counter_declared() {
    scratch="$1"
    root="$scratch/warn-counter"

    root_clean="$root/clean"
    make_clean_fixture "$root_clean"
    git_init_fixture "$root_clean"
    if ! output="$(check_dep_zero_tree "$root_clean" "NONE" 2>&1)"; then
        echo "selftest: WARN-COUNTER control FAILED (fixture limpa deveria passar)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    if ! printf '%s\n' "$output" | grep -qF "0 warning(s)"; then
        echo "selftest: WARN-COUNTER control FAILED (fixture limpa deveria declarar '0 warning(s)')" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi

    root_one="$root/one-ambiguous"
    make_clean_fixture "$root_one"
    printf 'find_package(\n    PkgConfig\n    REQUIRED\n)\n' >> "$root_one/cmake/Wayland.cmake"
    git_init_fixture "$root_one"
    if ! output="$(check_dep_zero_tree "$root_one" "NONE" 2>&1)"; then
        echo "selftest: WARN-COUNTER control FAILED (uma forma ambigua deveria passar com aviso, nao reprovar)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    if ! printf '%s\n' "$output" | grep -qF "1 warning(s)"; then
        echo "selftest: WARN-COUNTER control FAILED (uma forma ambigua deveria declarar '1 warning(s)')" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    echo "selftest: WARN-COUNTER control OK (o resumo declara 0 warning(s) na fixture limpa e 1 warning(s) com uma forma ambigua)"
}

# DEPZERO-SHALLOW F5, 31/08/2026: replaces the hand-counted literal
# "all twenty-one controls OK" - a number written by hand ages the
# instant a control is added or split (measured: this exact literal
# had already drifted once before, see DOC-ESTADO's own lesson in
# CLAUDE.md). Owns "overall" and "controls_run" as the two globals
# selftest_main aggregates into (POSIX sh has no "local" - GODS_LAWS.md
# L-40, TESTES.md - so every control below still names its OWN
# per-case tally, never "overall").
run_control() {
    if ! "$@"; then
        overall=1
    fi
    controls_run=$((controls_run + 1))
}

selftest_main() {
    scratch="$(make_scratch_workdir)"
    trap 'rm -rf "$scratch"' EXIT

    overall=0
    controls_run=0
    run_control selftest_positive_control "$scratch"
    run_control selftest_negative_control_cmake "$scratch"
    run_control selftest_warn_control_cmake_multiline "$scratch"
    run_control selftest_positive_control_cmake_multiline "$scratch"
    run_control selftest_positive_control_cmake_opener_resolved "$scratch"
    run_control selftest_positive_control_pkgcheck_version "$scratch"
    run_control selftest_negative_control_pkgcheck_version_unknown "$scratch"
    run_control selftest_negative_control_cmake_indirection "$scratch"
    run_control selftest_positive_control_cmake_indirection_legit "$scratch"
    run_control selftest_negative_control_cpm_variants "$scratch"
    run_control selftest_negative_control_opener_carries_bad_token "$scratch"
    run_control selftest_negative_control_file_network "$scratch"
    run_control selftest_positive_control_file_legit "$scratch"
    run_control selftest_warn_control_file_opener_bare "$scratch"
    run_control selftest_negative_control_execute_process "$scratch"
    run_control selftest_positive_control_execute_process_pkgconfig "$scratch"
    run_control selftest_warn_control_execute_process_variable "$scratch"
    run_control selftest_negative_control_include "$scratch"
    run_control selftest_negative_control_include_traversal "$scratch"
    run_control selftest_scope_message_is_honest "$scratch"
    run_control selftest_positive_control_needed "$scratch"
    run_control selftest_negative_control_needed "$scratch"
    run_control selftest_needed_static_skip
    run_control selftest_empty_scan_needed "$scratch"
    run_control selftest_empty_scan_tree "$scratch"
    run_control selftest_staged_zero_relevant_declared "$scratch"
    run_control selftest_staged_blocks_violation "$scratch"
    run_control selftest_staged_reads_index_not_worktree "$scratch"
    run_control selftest_staged_warns_ambiguous "$scratch"
    run_control selftest_escape_via_allowlist_edit "$scratch"
    run_control selftest_warn_counter_declared "$scratch"

    if [ "$overall" -ne 0 ]; then
        echo "check_dep_zero.sh --selftest: FAILED (see above)" >&2
        exit 1
    fi
    echo "check_dep_zero.sh --selftest: all ${controls_run} controls OK"
}

main() {
    if [ "${1:-}" = "--selftest" ]; then
        selftest_main
    else
        real_main "$@"
    fi
}

main "$@"
