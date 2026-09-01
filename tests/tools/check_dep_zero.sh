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
#      network on purpose (see the dep-zero awk engine below,
#      GATE-DEPZERO-NOFORK, 01/09/2026): a violation resolvable from ONE line
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
#       the dep-zero awk engine below, GATE-DEPZERO-NOFORK, 01/09/2026) -
#       DEPZERO-SHALLOW, 31/08/2026: a call whose decisive evidence does not
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
#            touched - see the dep-zero awk engine below,
#            GATE-DEPZERO-NOFORK, 01/09/2026. ***
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

readonly SO_HEADER_ALLOWLIST="GL/gl.h poll.h sys/prctl.h sys/stat.h sys/sysmacros.h sys/types.h unistd.h wayland-client.h windows.h xdg-shell-client-protocol.h glintfx/export.hpp glintfx/version_macros.hpp"
# Complete enumeration measured live 28/08/2026 (see header comment
# rule 2). xdg-shell-client-protocol.h is wayland-scanner GENERATED
# from the system-installed xdg-shell.xml - already judged OS API by
# GODS_LAWS.md L-07 (the Wayland precedent), never vendored, never
# hand-written.
#
# poll.h added WL-DISPLAY fatia D (TODO.md): POSIX <poll.h> (poll(),
# struct pollfd, POLLIN/POLLOUT) - same OS-API category as sys/*.h and
# unistd.h already on this list, needed by src/platform/wayland/
# display_adapter.cpp's own pump_events() to implement the canonical
# prepare_read/flush/poll/read_events sequence wl_display(3) documents
# (w4-plano.md sec. 1.1) without EVER calling a blocking dispatch.
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

# Counts WARNING occurrences (not lines - each record is 3 printed
# lines) by counting the fixed header line, closed with `|| true`
# (GODS_LAWS.md L-45: `grep -c` on zero matches exits 1, which would
# abort this assignment under `set -eu` since it is not itself inside
# a conditional). Operates on the 'W|' text the shell callers extract
# from the dep-zero engine's output - unaffected by GATE-DEPZERO-NOFORK
# (the engine emits the SAME warning text, byte-identical).
count_warning_records() {
    [ -z "$1" ] && { echo 0; return; }
    printf '%s\n' "$1" | grep -cF 'WARNING (this is NOT a pass verdict' || true
}

# --- GATE-DEPZERO-NOFORK, 01/09/2026 (GODS_LAWS.md L-11 bloco 6: no
# process per item scanned - the incident that cost four hours and a
# forced reboot). Sub-checks (a) and (b) used to loop in the SHELL, one
# `cat`/`git show` PLUS one small awk-per-line comment-stripper PER
# FILE (and, before that, one further fork per LINE inside the old
# per-line matcher) - thousands of forks on this tree, growing without
# bound as the tree grows. That loop is ALSO how a hostile filename
# (a real newline, double quote, or backslash byte, which git quotes
# UNCONDITIONALLY per git-config(1) regardless of core.quotepath -
# quotepath only ever controlled bytes >= 0x80) went unseen: the old
# shell filtered `git ls-files`/`git diff --cached --name-only` with a
# plain `grep -E` against CMAKE_SURFACE_PATTERN/CXX_SURFACE_PATTERN,
# and a C-quoted line like "cmake/Way\nland.cmake" never matches a
# pattern anchored on a literal `.cmake$` - the file vanished from the
# scan while a clean sibling kept the non-empty-scan floor above zero,
# and the gate passed MUDO.
#
# Both defects share ONE fix: sub-checks (a) and (b) now run inside a
# SINGLE awk process (`write_dep_zero_engine_awk_program`/
# `run_dep_zero_engine` below) that reads the git listing from stdin
# (one line per path, in the C-quoted form git already emits),
# DECODES that quoting itself (git-config(1)/quote.c grammar: \a \b \f
# \n \r \t \v \" \\ and 3-digit octal \ooo, reconstructed byte-safe
# under LC_ALL=C), classifies CMake vs C++ surface, and opens each
# matching file itself via `getline < path` - one process, one pass,
# cost INDEPENDENT of the number of files or lines (measured: ~10
# processes in tree mode, ~13 in staged mode, proved equal at N=5 and
# at the real tree's N=124 with `strace -fqc -e trace=fork,vfork,clone,
# execve`, cited in the commit that introduced this). Every physical
# line is still evaluated with the EXACT same rules as before (ported
# verbatim: strip_cmake_comment, evaluate_cmake_line and its four
# sub-evaluators, include_content_violations) - only the process
# topology changed, never the DEPZERO-SHALLOW contract (blocks what
# resolves on one line, warns-and-defers what does not).
#
# The awk program text lives in a quoted heredoc (`<<'AWK'`) written to
# a fresh mktemp file per invocation, never a single-quoted shell
# string: the warning text below carries a real apostrophe ("Leader's
# decision"), and every allowlist/advice/pattern value crosses into awk
# via ENVIRON (exported on the `awk` command's own environment, never
# `-v name=value` - that form runs its own C-style backslash-escape
# processing and would corrupt a legitimate literal "\${" this tree's
# CMake files already contain, cmake/GlintfxInstall.cmake:183 etc.).
# POSIX awk only (no gensub/asort/nextfile/multi-char RS): the CI's
# Ubuntu job runs mawk, not gawk.

write_dep_zero_engine_awk_program() {
    prog="$(mktemp "${TMPDIR:-/tmp}/glintfx-dep-zero-engine-XXXXXX.awk")" || return 1
    cat > "$prog" <<'AWK'
function is_space_ch(c) {
    return (c == " " || c == "\t" || c == "\r" || c == "\f" || c == "\v")
}

function oct2dec(o,   d1, d2, d3) {
    d1 = index("01234567", substr(o, 1, 1)) - 1
    d2 = index("01234567", substr(o, 2, 1)) - 1
    d3 = index("01234567", substr(o, 3, 1)) - 1
    return d1 * 64 + d2 * 8 + d3
}

# Decodes ONE line of `git ls-files`/`git diff --cached --name-only`
# output. Grammar (git-config(1), quote.c): a quoted path is wrapped in
# double quotes and every byte outside the printable-ASCII-minus-
# special set is escaped as one of \a \b \f \n \r \t \v \" \\ or a
# 3-digit octal \ooo. An unquoted line (no leading '"') is already
# literal and returned unchanged. Sets g_decode_ok=0 on any grammar
# violation (unterminated string, dangling backslash, unknown escape,
# incomplete octal, octal outside \000-\377) - fail-closed, never
# best-effort. The octal range check matters because a byte only ever
# has 256 values: git never emits \400-\777 (511 down to 256 decimal),
# and a decoder that accepted them anyway would silently reduce them
# modulo 256 in some awk implementations (measured: gawk 5.3.2 folds
# \777, decimal 511, onto the SAME byte 0xFF as the legitimate \377,
# decimal 255 - two different escapes, one indistinguishable byte) -
# infidelity to the grammar this decoder claims to be strict about.
function decode_git_quoted(s,    n, inner, out, i, c, e, oct, val, ok) {
    g_decode_ok = 1
    n = length(s)
    if (n < 2 || substr(s, 1, 1) != "\"" || substr(s, n, 1) != "\"") {
        return s
    }
    inner = substr(s, 2, n - 2)
    n = length(inner)
    out = ""
    i = 1
    ok = 1
    while (i <= n) {
        c = substr(inner, i, 1)
        if (c == "\\") {
            i++
            if (i > n) { ok = 0; break }
            e = substr(inner, i, 1)
            if (e == "a") { out = out sprintf("%c", 7); i++ }
            else if (e == "b") { out = out sprintf("%c", 8); i++ }
            else if (e == "f") { out = out sprintf("%c", 12); i++ }
            else if (e == "n") { out = out sprintf("%c", 10); i++ }
            else if (e == "r") { out = out sprintf("%c", 13); i++ }
            else if (e == "t") { out = out sprintf("%c", 9); i++ }
            else if (e == "v") { out = out sprintf("%c", 11); i++ }
            else if (e == "\"") { out = out "\""; i++ }
            else if (e == "\\") { out = out "\\"; i++ }
            else if (e >= "0" && e <= "7") {
                if (i + 2 > n) { ok = 0; break }
                oct = substr(inner, i, 3)
                # [0-3] on the first digit, never [0-7]: a byte tops
                # out at \377 (255 decimal) - \4nn upward (256+) is out
                # of range and must be refused, never silently reduced
                # modulo 256 by sprintf("%c", ...) (I3, GATE-DEPZERO-NOFORK).
                if (oct !~ /^[0-3][0-7][0-7]$/) { ok = 0; break }
                val = oct2dec(oct)
                out = out sprintf("%c", val)
                i += 3
            }
            else { ok = 0; break }
        } else {
            out = out c
            i++
        }
    }
    if (!ok) { g_decode_ok = 0; return "" }
    return out
}

function name_is_known(candidate, list,    n, arr, i, found) {
    n = split(list, arr, / /)
    found = 0
    for (i = 1; i <= n; i++) if (arr[i] == candidate) { found = 1; break }
    return found
}

function imatch(str, pat) {
    return (tolower(str) ~ tolower(pat))
}

# --- sub-check (a): CMake surface, one physical line at a time,
# comment stripped before matching (quote-aware: '#' only starts a
# comment outside a double-quoted string - CPM's own documented
# shorthand puts a real '#' inside one, "gh:fmtlib/fmt#1.0"). Ported
# verbatim from the pre-GATE-DEPZERO-NOFORK shell/awk hybrid. ---
function strip_cmake_comment(line,    in_quote, out, n, i, c) {
    in_quote = 0
    out = ""
    n = length(line)
    for (i = 1; i <= n; i++) {
        c = substr(line, i, 1)
        if (c == "\"") { in_quote = !in_quote; out = out c; continue }
        if (c == "#" && !in_quote) break
        out = out c
    }
    return out
}

# Emits a BLOCKING finding ('V|') / a DEFERRED finding ('W|'), read
# back out by the shell caller with `sed -n 's/^V|//p'`/`'^W|'` - never
# grep (GODS_LAWS.md L-45: a zero-match grep exits 1). Citations print
# check_root/disp - disp is the RAW git-quoted line (always ONE
# physical line, even when the decoded path contains a literal
# newline), so the V|/W| record framing can never split across lines.
function emit_violation(disp, lineno, raw, advice) {
    printf "V|%s/%s:%d: %s\n", check_root, disp, lineno, raw
    printf "V|  -> %s\n", advice
}

function emit_warning(disp, lineno, raw) {
    printf "W|check_dep_zero.sh: WARNING (this is NOT a pass verdict for this call):\n"
    printf "W|%s/%s:%d: %s\n", check_root, disp, lineno, raw
    printf "W|  -> This call spans lines or builds its decisive argument from a variable, and this shallow, line-by-line gate cannot judge it. It is NOT being cleared here: the CI oracle (ctest test dep_zero_trace, tests/tools/check_dep_zero_trace.py) is the authority that judges what CMake actually executes, and it WILL reprove a violation there (GODS_LAWS.md L-07). Leader's decision, 28/08/2026: ambiguous forms pass the hook with this warning so the CI can decide.\n"
}

# Mirrors paren_first_token/first_paren_arg: first token after '(' on
# THIS line, skipping leading whitespace, stopping at whitespace or
# ')'. Returns "" on a bare opener (nothing but whitespace/')' after
# the paren) - the exact "lixo" this gate must never mistake for a
# real package/module name.
function paren_first_token(line,    idx, i, n, c, out) {
    idx = index(line, "(")
    if (idx == 0) return ""
    i = idx + 1
    n = length(line)
    while (i <= n && is_space_ch(substr(line, i, 1))) i++
    if (i > n) return ""
    c = substr(line, i, 1)
    if (c == ")") return ""
    out = ""
    while (i <= n) {
        c = substr(line, i, 1)
        if (is_space_ch(c) || c == ")") break
        out = out c
        i++
    }
    return out
}

# pkg-config module token may carry a GLUED version comparator
# (wayland-client>=1.20, standard pkg-config syntax) - strips it so the
# base module name can be checked against the allowlist
# (REVIEW-DEPZERO-GATE.md achado IMPORTANTE #6).
function pkgconfig_module_base_name(tok) {
    sub(/(>=|<=|>|<|=).*$/, "", tok)
    return tok
}

function strip_first_token(s) {
    sub(/^[ \t\r\f\v]+/, "", s)
    sub(/^[^ \t\r\f\v]+/, "", s)
    sub(/^[ \t\r\f\v]+/, "", s)
    return s
}

function rtrim_close_paren(s) {
    sub(/\)[ \t]*$/, "", s)
    return s
}

# include(): a line with no "${" at all is always fully literal and
# always resolves, closed or not. A line WITH "${" resolves only when
# it ALSO carries a literal ".cmake" fragment (the real, legitimate
# parameterized-file-include shape already used twice in this tree)
# AND is closed on this same line; unclosed, or "${" with no ".cmake"
# anywhere and closed, are the two remaining cases (warn, block).
function eval_include_line(disp, lineno, line, raw, closed) {
    if (index(line, "${") == 0) return
    if (index(tolower(line), ".cmake") > 0) {
        if (!closed) emit_warning(disp, lineno, raw)
        return
    }
    if (closed) emit_violation(disp, lineno, raw, INDIRECTION_ADVICE)
    else emit_warning(disp, lineno, raw)
}

# pkg_check_modules(): any BAD token visible on this line blocks
# regardless of closure. With every visible token clean, a CLOSED line
# passes silently; an unclosed one warns.
function eval_pkg_check_modules_line(disp, lineno, line, raw, closed,    idx, content, rest, n, toks, i, tok, base, unknown_hit) {
    idx = index(line, "(")
    content = substr(line, idx + 1)
    content = rtrim_close_paren(content)
    rest = strip_first_token(content)
    unknown_hit = 0
    n = split(rest, toks, /[ \t\r\f\v]+/)
    for (i = 1; i <= n; i++) {
        tok = toks[i]
        if (tok == "") continue
        if (name_is_known(tok, PKG_CHECK_MODULES_KEYWORDS)) continue
        if (tok == ">=" || tok == "<=" || tok == ">" || tok == "<" || tok == "=") continue
        if (tok ~ /^[0-9]/) continue
        base = pkgconfig_module_base_name(tok)
        if (!name_is_known(base, PKG_CHECK_MODULES_ALLOWLIST)) unknown_hit = 1
    }
    if (unknown_hit) { emit_violation(disp, lineno, raw, PKGCHECK_ADVICE); return }
    if (!closed) emit_warning(disp, lineno, raw)
}

# Finds the program token following the LAST "COMMAND" keyword on this
# line that is immediately followed by whitespace (mirrors the
# greedy-.* backtracking of the sed this replaces - a `COMMAND_ECHO`
# token never satisfies the whitespace-after test, so it is skipped,
# exactly as the original regex would skip it too).
function find_command_program(line,    n, search_from, pos, abspos, after, last_pos, i, c, out) {
    n = length(line)
    last_pos = 0
    search_from = 1
    while (1) {
        pos = index(substr(line, search_from), "COMMAND")
        if (pos == 0) break
        abspos = search_from + pos - 1
        after = abspos + 7
        if (after <= n && is_space_ch(substr(line, after, 1))) last_pos = abspos
        search_from = abspos + 1
    }
    if (last_pos == 0) return ""
    i = last_pos + 7
    while (i <= n && is_space_ch(substr(line, i, 1))) i++
    if (i > n) return ""
    if (substr(line, i, 1) == "\"") i++
    out = ""
    while (i <= n) {
        c = substr(line, i, 1)
        if (c == "\"" || c == ")" || is_space_ch(c)) break
        out = out c
        i++
    }
    return out
}

# execute_process(): a LITERAL program on this line resolves - its
# basename (extension stripped, casefold) decides block vs pass
# against EXECUTE_PROCESS_PROGRAM_ALLOWLIST. No COMMAND+program visible
# on this line, or a program built from "${var}", both warn.
function eval_execute_process_line(disp, lineno, line, raw,    program, base, lower_base) {
    program = find_command_program(line)
    if (program == "") { emit_warning(disp, lineno, raw); return }
    if (index(program, "${") > 0) { emit_warning(disp, lineno, raw); return }
    base = program
    gsub(/^.*\//, "", base)
    lower_base = tolower(base)
    if (lower_base ~ /\.exe$/) sub(/\.exe$/, "", lower_base)
    else if (lower_base ~ /\.bat$/) sub(/\.bat$/, "", lower_base)
    else if (lower_base ~ /\.cmd$/) sub(/\.cmd$/, "", lower_base)
    if (!name_is_known(lower_base, EXECUTE_PROCESS_PROGRAM_ALLOWLIST)) emit_violation(disp, lineno, raw, EXECUTE_PROCESS_ADVICE)
}

# Evaluates ONE physical CMake line, comment already stripped. The
# DEPZERO-SHALLOW contract (unchanged by this port): what resolves on
# THIS physical line blocks; what does not (multi-line, variable-built)
# warns and defers to the CI oracle. Never both, never silent.
function eval_cmake_line(disp, lineno, line, raw,    closed, subcmd, name) {
    if (line ~ /^[ \t\r\f\v]*$/) return

    closed = (index(line, ")") > 0) ? 1 : 0

    if (imatch(line, CMAKE_FETCH_PATTERN)) { emit_violation(disp, lineno, raw, FETCH_ADVICE); return }
    if (imatch(line, CMAKE_TOOLCHAIN_PATTERN)) { emit_violation(disp, lineno, raw, FETCH_ADVICE); return }
    if (imatch(line, CMAKE_LANGUAGE_PATTERN)) { emit_violation(disp, lineno, raw, INDIRECTION_ADVICE); return }
    if (imatch(line, CMAKE_FILE_NETWORK_PATTERN)) { emit_violation(disp, lineno, raw, FILE_NETWORK_ADVICE); return }
    if (imatch(line, "^[ \t\r\f\v]*file[ \t\r\f\v]*\\(")) {
        subcmd = paren_first_token(line)
        if (subcmd == "") emit_warning(disp, lineno, raw)
        return
    }
    if (imatch(line, "^[ \t\r\f\v]*execute_process[ \t\r\f\v]*\\(")) {
        eval_execute_process_line(disp, lineno, line, raw)
        return
    }
    if (imatch(line, "^[ \t\r\f\v]*include[ \t\r\f\v]*\\(")) {
        eval_include_line(disp, lineno, line, raw, closed)
        return
    }
    if (imatch(line, "^[ \t\r\f\v]*find_package[ \t\r\f\v]*\\(")) {
        name = paren_first_token(line)
        if (name == "") { emit_warning(disp, lineno, raw); return }
        if (!name_is_known(name, FIND_PACKAGE_ALLOWLIST)) emit_violation(disp, lineno, raw, FINDPKG_ADVICE)
        return
    }
    if (imatch(line, "^[ \t\r\f\v]*pkg_check_modules[ \t\r\f\v]*\\(")) {
        eval_pkg_check_modules_line(disp, lineno, line, raw, closed)
        return
    }
}

# Opens the file itself (byte-safe for any name without NUL) via
# `getline < path` - >0 means a line was read, 0 means clean EOF (an
# empty file counts as opened, zero lines - the same verdict `test -f`
# gives an empty file), <0 means the open/read itself failed (missing
# from the working tree, a directory, or unreadable) and is reported as
# an 'F|' record, never silently treated as "no content".
function scan_cmake_file(path, disp,    fname, line, lineno, rc) {
    fname = content_root "/" path
    lineno = 0
    rc = 1
    while ((rc = (getline line < fname)) > 0) {
        lineno++
        eval_cmake_line(disp, lineno, strip_cmake_comment(line), line)
    }
    close(fname)
    if (rc < 0) {
        print "F|" check_root "/" disp ": open refused (file not found in working tree, or unreadable)"
        return 0
    }
    return 1
}

# --- sub-check (b): include surface, one line of CONTENT at a time -----

function extract_angle_include_name(line,    i, j) {
    i = index(line, "<")
    if (i == 0) return ""
    j = index(line, ">")
    if (j == 0 || j <= i) return ""
    return substr(line, i + 1, j - i - 1)
}

# Structural existence probe for rule 3 (our OWN public header): >= 0
# means the open succeeded (an empty file reads 0 and counts as
# existing, same verdict as `test -f`); a directory or a missing path
# both read -1 and count as not existing - identical to `test -f`'s
# own behavior, never a plain filesystem stat that a symlink or a
# directory could fool.
function probe_file_exists(fname,    rc, dummy) {
    rc = (getline dummy < fname)
    close(fname)
    return (rc >= 0)
}

function eval_include_content_line(disp, lineno, line,    name, has_dot_or_slash) {
    if (!(line ~ /^[ \t]*#[ \t]*include[ \t]*<[^>]+>/)) return
    name = extract_angle_include_name(line)
    if (name == "") return

    has_dot_or_slash = (index(name, ".") > 0 || index(name, "/") > 0)
    if (!has_dot_or_slash) return

    if (name_is_known(name, SO_HEADER_ALLOWLIST)) return

    if (substr(name, 1, 8) == "glintfx/") {
        # REVIEW-DEPZERO-GATE.md achado CRITICO #4: reject ANY ".."
        # occurrence before ever touching the filesystem - a plain
        # existence probe resolves ".." against the real filesystem and
        # can walk clean out of include/.
        if (index(name, "..") == 0) {
            if (probe_file_exists(check_root "/include/" name)) return
        }
    }

    printf "V|%s/%s:%d: %s\n", check_root, disp, lineno, line
    printf "V|  -> %s\n", INCLUDE_ADVICE
}

function scan_cxx_file(path, disp,    fname, line, lineno, rc) {
    fname = content_root "/" path
    lineno = 0
    rc = 1
    while ((rc = (getline line < fname)) > 0) {
        lineno++
        eval_include_content_line(disp, lineno, line)
    }
    close(fname)
    if (rc < 0) {
        print "F|" check_root "/" disp ": open refused (file not found in working tree, or unreadable)"
        return 0
    }
    return 1
}

BEGIN {
    check_root = ENVIRON["DEP_ZERO_ROOT"]
    content_root = ENVIRON["DEP_ZERO_CONTENT_ROOT"]
    FETCH_ADVICE = ENVIRON["FETCH_ADVICE"]
    FINDPKG_ADVICE = ENVIRON["FINDPKG_ADVICE"]
    PKGCHECK_ADVICE = ENVIRON["PKGCHECK_ADVICE"]
    INCLUDE_ADVICE = ENVIRON["INCLUDE_ADVICE"]
    INDIRECTION_ADVICE = ENVIRON["INDIRECTION_ADVICE"]
    FILE_NETWORK_ADVICE = ENVIRON["FILE_NETWORK_ADVICE"]
    EXECUTE_PROCESS_ADVICE = ENVIRON["EXECUTE_PROCESS_ADVICE"]
    FIND_PACKAGE_ALLOWLIST = ENVIRON["DEP_ZERO_FIND_PACKAGE_ALLOWLIST"]
    PKG_CHECK_MODULES_ALLOWLIST = ENVIRON["DEP_ZERO_PKG_CHECK_MODULES_ALLOWLIST"]
    PKG_CHECK_MODULES_KEYWORDS = ENVIRON["DEP_ZERO_PKG_CHECK_MODULES_KEYWORDS"]
    SO_HEADER_ALLOWLIST = ENVIRON["DEP_ZERO_SO_HEADER_ALLOWLIST"]
    EXECUTE_PROCESS_PROGRAM_ALLOWLIST = ENVIRON["DEP_ZERO_EXECUTE_PROCESS_PROGRAM_ALLOWLIST"]
    CMAKE_SURFACE_PATTERN = ENVIRON["DEP_ZERO_CMAKE_SURFACE_PATTERN"]
    CXX_SURFACE_PATTERN = ENVIRON["DEP_ZERO_CXX_SURFACE_PATTERN"]
    CMAKE_FETCH_PATTERN = ENVIRON["DEP_ZERO_CMAKE_FETCH_PATTERN"]
    CMAKE_TOOLCHAIN_PATTERN = ENVIRON["DEP_ZERO_CMAKE_TOOLCHAIN_PATTERN"]
    CMAKE_LANGUAGE_PATTERN = ENVIRON["DEP_ZERO_CMAKE_LANGUAGE_PATTERN"]
    CMAKE_FILE_NETWORK_PATTERN = ENVIRON["DEP_ZERO_CMAKE_FILE_NETWORK_PATTERN"]

    cmake_found = 0; cmake_analyzed = 0
    cxx_found = 0; cxx_analyzed = 0
    failed = 0
}

# One record per tracked/staged path, C-quoted exactly as git printed
# it. Decoded once here; the surface patterns are matched against the
# DECODED path (so an escaped ".cmake$"/"\.cpp$" etc. matches again),
# citations always use the original C-quoted 'disp' (framing-safe).
{
    disp = $0
    if (disp == "") next

    path = decode_git_quoted(disp)
    if (!g_decode_ok) {
        failed++
        print "F|" check_root "/" disp ": decode refused (malformed git quoting)"
        next
    }

    if (path ~ CMAKE_SURFACE_PATTERN) {
        cmake_found++
        if (scan_cmake_file(path, disp)) cmake_analyzed++
        else failed++
    }
    if (path ~ CXX_SURFACE_PATTERN) {
        cxx_found++
        if (scan_cxx_file(path, disp)) cxx_analyzed++
        else failed++
    }
}

# GODS_LAWS.md L-40 item 3 / L-36: printed ALWAYS, including all-zero -
# the shell caller reproves if this record is absent, duplicated, or
# shows found != analyzed / failed > 0 (the engine died mid-scan, or a
# file it enumerated could not be opened - fail-closed, never a silent
# "0 relevant").
END {
    printf "S|cmake_found=%d cmake_analyzed=%d cxx_found=%d cxx_analyzed=%d failed=%d\n", cmake_found, cmake_analyzed, cxx_found, cxx_analyzed, failed
}
AWK
    printf '%s\n' "$prog"
}

# Runs the dep-zero engine once against the listing on stdin.
# $1 = check_root (citations, and the REAL filesystem root the
#      glintfx/ own-header existence probe always reads against - same
#      in tree AND staged mode, matching the pre-port behavior).
# $2 = content_root (where file BYTES are read from: check_root itself
#      in tree mode, a checkout-index scratch prefix in staged mode -
#      GODS_LAWS.md L-12, the index blob, never the working tree).
# Every value the awk program needs crosses via ENVIRON, exported only
# on this one command's environment (a shell prefix assignment list
# exports for that single invocation without touching the caller's own
# globals) - never `-v` (armadilha already documented above). The
# eleven allowlist/pattern values are declared `readonly` at the top of
# this file (GODS_LAWS.md L-40 item 5) - a prefix assignment CANNOT
# reuse the SAME name as an existing readonly shell variable (measured
# live: bash refuses it, "variavel permite somente leitura", even
# though the assignment is scoped to the one child command) - so they
# cross under a DEP_ZERO_-prefixed name instead, read back out of
# ENVIRON under that same prefixed name in the awk program's BEGIN
# block above. The seven *_ADVICE values are plain (non-readonly)
# variables and keep their own name.
run_dep_zero_engine() {
    engine_check_root="$1"
    engine_content_root="$2"
    engine_prog="$(write_dep_zero_engine_awk_program)" || { echo "check_dep_zero.sh: mktemp failed writing the dep-zero awk engine" >&2; return 1; }

    LC_ALL=C \
        DEP_ZERO_ROOT="$engine_check_root" \
        DEP_ZERO_CONTENT_ROOT="$engine_content_root" \
        FETCH_ADVICE="$FETCH_ADVICE" \
        FINDPKG_ADVICE="$FINDPKG_ADVICE" \
        PKGCHECK_ADVICE="$PKGCHECK_ADVICE" \
        INCLUDE_ADVICE="$INCLUDE_ADVICE" \
        INDIRECTION_ADVICE="$INDIRECTION_ADVICE" \
        FILE_NETWORK_ADVICE="$FILE_NETWORK_ADVICE" \
        EXECUTE_PROCESS_ADVICE="$EXECUTE_PROCESS_ADVICE" \
        DEP_ZERO_FIND_PACKAGE_ALLOWLIST="$FIND_PACKAGE_ALLOWLIST" \
        DEP_ZERO_PKG_CHECK_MODULES_ALLOWLIST="$PKG_CHECK_MODULES_ALLOWLIST" \
        DEP_ZERO_PKG_CHECK_MODULES_KEYWORDS="$PKG_CHECK_MODULES_KEYWORDS" \
        DEP_ZERO_SO_HEADER_ALLOWLIST="$SO_HEADER_ALLOWLIST" \
        DEP_ZERO_EXECUTE_PROCESS_PROGRAM_ALLOWLIST="$EXECUTE_PROCESS_PROGRAM_ALLOWLIST" \
        DEP_ZERO_CMAKE_SURFACE_PATTERN="$CMAKE_SURFACE_PATTERN" \
        DEP_ZERO_CXX_SURFACE_PATTERN="$CXX_SURFACE_PATTERN" \
        DEP_ZERO_CMAKE_FETCH_PATTERN="$CMAKE_FETCH_PATTERN" \
        DEP_ZERO_CMAKE_TOOLCHAIN_PATTERN="$CMAKE_TOOLCHAIN_PATTERN" \
        DEP_ZERO_CMAKE_LANGUAGE_PATTERN="$CMAKE_LANGUAGE_PATTERN" \
        DEP_ZERO_CMAKE_FILE_NETWORK_PATTERN="$CMAKE_FILE_NETWORK_PATTERN" \
        awk -f "$engine_prog"
    engine_rc=$?
    rm -f "$engine_prog"
    return "$engine_rc"
}

# Parses the engine's single 'S|' summary record into the five shell
# variables the two callers below both need. Reprove (never trust a
# partial/duplicated/absent record) is the CALLER's job - this only
# parses what is there.
parse_dep_zero_summary() {
    summary_line="$1"
    cmake_found=0; cmake_analyzed=0; cxx_found=0; cxx_analyzed=0; failed=0
    eval "$(printf '%s\n' "$summary_line" | sed -E 's/^cmake_found=([0-9]+) cmake_analyzed=([0-9]+) cxx_found=([0-9]+) cxx_analyzed=([0-9]+) failed=([0-9]+)$/cmake_found=\1; cmake_analyzed=\2; cxx_found=\3; cxx_analyzed=\4; failed=\5/')"
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

    # -c core.quotepath=false (GATE-QUOTEPATH, 01/09/2026): git's own
    # default (core.quotepath=true) prints any tracked path with a byte
    # >= 0x80 as a C-style octal-escaped, double-quoted string. Kept for
    # DISPLAY AFFINITY (an accented citation reads as "Waylând.cmake",
    # not an escaped "Wayl\303\244nd.cmake") - the actual FIX for both
    # the accented case AND the always-quoted control-byte case (tab,
    # newline, double quote, backslash - quoted UNCONDITIONALLY per
    # git-config(1), regardless of core.quotepath) is the dep-zero
    # engine's own decoder (GATE-DEPZERO-NOFORK, 01/09/2026, see above).
    all_files="$(git -c core.quotepath=false -C "$root" ls-files 2>/dev/null)" \
        || { echo "check_dep_zero.sh: 'git ls-files' failed in $root (not a git repository, or git unavailable) - scan refused, never assumed empty" >&2; return 1; }

    engine_raw="$(printf '%s\n' "$all_files" | run_dep_zero_engine "$root" "$root")" || {
        echo "check_dep_zero.sh: the dep-zero engine (awk) itself failed to run - scan refused, never assumed clean" >&2
        return 1
    }

    violations="$(printf '%s\n' "$engine_raw" | sed -n 's/^V|//p')"
    warnings="$(printf '%s\n' "$engine_raw" | sed -n 's/^W|//p')"
    failed_records="$(printf '%s\n' "$engine_raw" | sed -n 's/^F|//p')"
    summary_records="$(printf '%s\n' "$engine_raw" | sed -n 's/^S|//p')"
    summary_count=0
    [ -n "$summary_records" ] && summary_count="$(count_lines "$summary_records")"

    if [ "$summary_count" -ne 1 ]; then
        echo "check_dep_zero.sh: the dep-zero engine (awk) did not print exactly one summary record (got $summary_count) - the scan is not trusted, GODS_LAWS.md L-36/L-40" >&2
        return 1
    fi
    parse_dep_zero_summary "$summary_records"

    if [ "$cmake_found" -eq 0 ]; then
        echo "check_dep_zero.sh: empty scan (0 CMake surface files) - GODS_LAWS.md L-40" >&2
        return 1
    fi
    if [ "$cxx_found" -eq 0 ]; then
        echo "check_dep_zero.sh: empty scan (0 C++ surface files) - GODS_LAWS.md L-40" >&2
        return 1
    fi

    # GODS_LAWS.md L-40 item 3/L-36 fail-closed: a file the engine
    # enumerated but could not open (deleted from the worktree between
    # commit and scan, a directory, unreadable) must reprove, never be
    # silently treated as "clean because empty".
    scan_failed=0
    if [ "$failed" -ne 0 ] || [ "$cmake_found" -ne "$cmake_analyzed" ] || [ "$cxx_found" -ne "$cxx_analyzed" ]; then
        scan_failed=1
    fi

    needed_ok=1
    needed_output=""
    if ! needed_output="$(check_needed_allowlist "$library_path" "$NEEDED_ALLOWLIST" 2>&1)"; then
        needed_ok=0
    fi

    warning_total="$(count_warning_records "$warnings")"

    if [ "$scan_failed" -eq 1 ] || [ -n "$violations" ] || [ "$needed_ok" -eq 0 ]; then
        print_violation_header
        [ -n "$violations" ] && printf '%s\n' "$violations" >&2
        [ "$needed_ok" -eq 0 ] && printf '%s\n' "$needed_output" >&2
        if [ "$scan_failed" -eq 1 ]; then
            echo "check_dep_zero.sh: scan incomplete - $failed file(s) refused to open, cmake $cmake_analyzed/$cmake_found analyzed, c++ $cxx_analyzed/$cxx_found analyzed (GODS_LAWS.md L-40 fail-closed)" >&2
            [ -n "$failed_records" ] && printf '%s\n' "$failed_records" >&2
        fi
        [ -n "$warnings" ] && printf '%s\n' "$warnings" >&2
        # "encontrados" must include what the OLD grep-based enumeration
        # never saw (GATE-DEPZERO-NOFORK C4) - declared on every reprove
        # path, not only the clean-pass summary line below.
        echo "check_dep_zero.sh: $cmake_found cmake file(s), $cxx_found c++ file(s) found (scan reproved, see above)" >&2
        return 1
    fi

    [ -n "$warnings" ] && printf '%s\n' "$warnings" >&2
    echo "check_dep_zero.sh: 0 violation(s), ${warning_total} warning(s) deferred to the CI oracle (dep_zero_trace) - $cmake_found cmake file(s), $cxx_found c++ file(s) scanned"
    printf '%s\n' "$needed_output"
    echo "check_dep_zero.sh: out of scope by design, and not verified by any other gate either: documents (.md), shell scripts (.sh), and quote includes (#include \"...\") are not scanned here. check_spdx.sh only checks for the PRESENCE of an SPDX header string in a file (a vendored third-party file with that string pasted in would still pass it); check_vendor_purity.sh only guards the one named third_party/khronos/ exception from growing, not vendoring in general. Neither closes the quote-include vendor vector - this gate does not either."
    echo "check_dep_zero.sh: ambiguous multi-line or variable-built calls pass this shallow, line-by-line gate with a printed warning; the CI oracle (ctest test dep_zero_trace, tests/tools/check_dep_zero_trace.py) is the authority that judges what CMake actually executes."
}

# --- staged mode (pre-commit shape): index content, sub-checks (a)/(b) -

check_dep_zero_staged() {
    root="$1"

    git -C "$root" rev-parse --is-inside-work-tree >/dev/null 2>&1 \
        || { echo "check_dep_zero.sh: not a git repository: $root" >&2; return 1; }

    # -c core.quotepath=false: same GATE-QUOTEPATH/GATE-DEPZERO-NOFORK
    # reasoning as check_dep_zero_tree above, applied to the staged
    # (index) listing - this one drives the engine's classification and
    # citations, and is separate from the -z enumeration below (used
    # only to materialize CONTENT, never to decide what matches).
    staged="$(git -c core.quotepath=false -C "$root" diff --cached --name-only --diff-filter=ACMR)" \
        || { echo "check_dep_zero.sh: 'git diff --cached' failed in $root" >&2; return 1; }

    staged_count=0
    [ -n "$staged" ] && staged_count="$(count_lines "$staged")"

    # Materializes the INDEX content (never the working tree,
    # GODS_LAWS.md L-12) into a scratch prefix: ONE -z enumeration plus
    # ONE checkout-index, both constant-cost regardless of N
    # (GODS_LAWS.md L-11 bloco 6) - `git show ":$p"` per file is what
    # this replaces. The listing goes to a FILE, never a variable ($(...)
    # silently drops embedded NUL bytes - GODS_LAWS.md L-45 territory).
    tmp_root="$(mktemp -d "${TMPDIR:-/tmp}/glintfx-dep-zero-staged-XXXXXX")" \
        || { echo "check_dep_zero.sh: mktemp failed materializing staged content" >&2; return 1; }
    listing_z="$(mktemp "${TMPDIR:-/tmp}/glintfx-dep-zero-staged-list-XXXXXX")" \
        || { rm -rf "$tmp_root"; echo "check_dep_zero.sh: mktemp failed materializing staged content" >&2; return 1; }

    if ! git -C "$root" diff --cached --name-only -z --diff-filter=ACMR > "$listing_z"; then
        rm -rf "$tmp_root"; rm -f "$listing_z"
        echo "check_dep_zero.sh: 'git diff --cached -z' failed in $root" >&2
        return 1
    fi
    if ! git -C "$root" checkout-index -z --stdin --prefix="$tmp_root/" < "$listing_z"; then
        rm -rf "$tmp_root"; rm -f "$listing_z"
        echo "check_dep_zero.sh: 'git checkout-index' failed materializing staged content in $root" >&2
        return 1
    fi
    rm -f "$listing_z"

    engine_raw="$(printf '%s\n' "$staged" | run_dep_zero_engine "$root" "$tmp_root")"
    engine_status=$?
    rm -rf "$tmp_root"
    if [ "$engine_status" -ne 0 ]; then
        echo "check_dep_zero.sh: the dep-zero engine (awk) itself failed to run - scan refused, never assumed clean" >&2
        return 1
    fi

    violations="$(printf '%s\n' "$engine_raw" | sed -n 's/^V|//p')"
    warnings="$(printf '%s\n' "$engine_raw" | sed -n 's/^W|//p')"
    failed_records="$(printf '%s\n' "$engine_raw" | sed -n 's/^F|//p')"
    summary_records="$(printf '%s\n' "$engine_raw" | sed -n 's/^S|//p')"
    summary_count=0
    [ -n "$summary_records" ] && summary_count="$(count_lines "$summary_records")"

    if [ "$summary_count" -ne 1 ]; then
        echo "check_dep_zero.sh: the dep-zero engine (awk) did not print exactly one summary record (got $summary_count) - the scan is not trusted, GODS_LAWS.md L-36/L-40" >&2
        return 1
    fi
    parse_dep_zero_summary "$summary_records"

    relevant_count=$((cmake_found + cxx_found))
    if [ "$relevant_count" -eq 0 ]; then
        echo "check_dep_zero.sh: 0 relevant file(s) among $staged_count staged file(s); CMake/C++ surfaces untouched by this commit (GODS_LAWS.md L-40: declared, not silent)"
        return 0
    fi

    scan_failed=0
    if [ "$failed" -ne 0 ] || [ "$cmake_found" -ne "$cmake_analyzed" ] || [ "$cxx_found" -ne "$cxx_analyzed" ]; then
        scan_failed=1
    fi

    warning_total="$(count_warning_records "$warnings")"

    if [ "$scan_failed" -eq 1 ] || [ -n "$violations" ]; then
        print_violation_header
        [ -n "$violations" ] && printf '%s\n' "$violations" >&2
        if [ "$scan_failed" -eq 1 ]; then
            echo "check_dep_zero.sh: scan incomplete - $failed file(s) refused to open, cmake $cmake_analyzed/$cmake_found analyzed, c++ $cxx_analyzed/$cxx_found analyzed (GODS_LAWS.md L-40 fail-closed)" >&2
            [ -n "$failed_records" ] && printf '%s\n' "$failed_records" >&2
        fi
        [ -n "$warnings" ] && printf '%s\n' "$warnings" >&2
        # "encontrados" must include what the OLD grep-based enumeration
        # never saw (GATE-DEPZERO-NOFORK C4) - declared on every reprove
        # path, not only the clean-pass summary line below.
        echo "check_dep_zero.sh: $cmake_found cmake file(s), $cxx_found c++ file(s) found among $staged_count staged file(s) (scan reproved, see above)" >&2
        return 1
    fi

    [ -n "$warnings" ] && printf '%s\n' "$warnings" >&2
    echo "check_dep_zero.sh: 0 violation(s), ${warning_total} warning(s) deferred to the CI oracle (dep_zero_trace) - $cmake_found cmake file(s), $cxx_found c++ file(s) among $staged_count staged file(s) scanned"
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

# DEPZERO-SHALLOW (01/09/2026): selftest_empty_scan_tree above zeroes
# BOTH surfaces at once (only README.md tracked), so it can never tell
# whether the CMake floor's own "return 1" actually fired, or whether
# the C++ floor below it (also zero in that fixture) is the one doing
# the work. Measured live: commenting out the CMake floor's "return 1"
# left selftest_empty_scan_tree fully green, because check_dep_zero_tree
# fell through to the C++ floor and printed "empty scan (0 C++ surface
# files)" instead - a message that still contains the substring "empty
# scan" the old control greps for, so it never noticed the CMake floor
# was gone. This control isolates the CMake floor: a tree with a real
# C++ file (cxx_count > 0) and ZERO CMake surface files, so only the
# CMake floor's own branch can possibly fire, and it greps for the
# CMake-specific wording, not the shared substring.
selftest_empty_scan_tree_cmake_only() {
    scratch="$1"
    root="$scratch/empty-tree-cmake-only"
    mkdir -p "$root/src"
    git -C "$root" init -q
    git -C "$root" config user.email "selftest@example.invalid"
    git -C "$root" config user.name "selftest"
    printf 'int f(void) { return 0; }\n' > "$root/src/f.cpp"
    git -C "$root" add -A
    git -C "$root" commit -q -m "c++ present, no cmake surface at all"

    if output="$(check_dep_zero_tree "$root" "NONE" 2>&1)"; then
        echo "selftest: EMPTY-SCAN(tree/cmake-only-floor) control FAILED (a tree with C++ but zero CMake surface files should have been refused)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    if ! printf '%s\n' "$output" | grep -qF "0 CMake surface files"; then
        echo "selftest: EMPTY-SCAN(tree/cmake-only-floor) control FAILED (reproved, but did not specifically say '0 CMake surface files' - the CMake floor may not be the branch that actually fired)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    echo "selftest: EMPTY-SCAN(tree/cmake-only-floor) control OK (CMake floor fires on its own, distinct message, with a non-empty C++ surface present)"
}

# GATE-QUOTEPATH (01/09/2026): git's own default (core.quotepath=true)
# prints any tracked path containing a byte >= 0x80 as a C-style
# octal-escaped, double-quoted string (e.g. "Wayl\303\244nd.cmake"
# instead of Waylând.cmake). core.quotepath only ever controls THIS -
# bytes >= 0x80 - never tab/newline/quote/backslash, which git quotes
# UNCONDITIONALLY (git-config(1)); the OLD grep-based enumeration
# (`git ls-files | grep -cE '\.cmake$'`) matched neither shape of a
# quoted line, since '"...\303\244nd.cmake"' never ends in the bare
# '.cmake$' the pattern anchors on. Before the fix this control proved
# the exact silent hole: an accented-named .cmake file carrying an
# undeniable violation (FetchContent) was scanned as if it did not
# exist, and the tree PASSED when it should have reproved. The fix has
# two parts, both still load-bearing: `git -c core.quotepath=false`
# (kept for DISPLAY AFFINITY - citations read "Waylând.cmake", not an
# escaped form) plus the dep-zero engine's own git-quote DECODER
# (GATE-DEPZERO-NOFORK, 01/09/2026, see the engine above), which is
# what actually makes a quoted line - accented or hostile - resolve
# against the surface patterns again.
selftest_negative_control_accented_filename_tree() {
    scratch="$1"
    root="$scratch/negative-accented-tree"
    make_clean_fixture "$root"
    printf 'include(FetchContent)\nFetchContent_Declare(fmt GIT_REPOSITORY x)\n' > "$root/cmake/Waylând.cmake"
    git_init_fixture "$root"

    if output="$(check_dep_zero_tree "$root" "NONE" 2>&1)"; then
        echo "selftest: NEGATIVE(accented-filename/tree) control FAILED (cmake/Waylând.cmake carries FetchContent and should have been reproved - GATE-QUOTEPATH)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    if ! printf '%s\n' "$output" | grep -qF "FetchContent_Declare"; then
        echo "selftest: NEGATIVE(accented-filename/tree) control FAILED (reproved, but did not cite FetchContent_Declare - the accented file may have been skipped for a different reason)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    echo "selftest: NEGATIVE(accented-filename/tree) control OK (cmake/Waylând.cmake seen and reproved despite the accented byte, GATE-QUOTEPATH)"
}

# Same defect, staged (pre-commit) mode: check_dep_zero_staged feeds
# the SAME `git diff --cached --name-only` listing (quoted exactly like
# `git ls-files`) to the SAME dep-zero engine, so the decoder above
# resolves it here too.
selftest_staged_accented_filename() {
    scratch="$1"
    root="$scratch/staged-accented"
    make_clean_fixture "$root"
    git_init_fixture "$root"
    printf 'include(FetchContent)\nFetchContent_Declare(fmt GIT_REPOSITORY x)\n' > "$root/cmake/Waylând.cmake"
    git -C "$root" add "cmake/Waylând.cmake"

    if output="$(check_dep_zero_staged "$root" 2>&1)"; then
        echo "selftest: STAGED(accented-filename) control FAILED (staged cmake/Waylând.cmake carries FetchContent and should have been reproved - GATE-QUOTEPATH)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    if ! printf '%s\n' "$output" | grep -qF "FetchContent_Declare"; then
        echo "selftest: STAGED(accented-filename) control FAILED (reproved, but did not cite FetchContent_Declare)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    echo "selftest: STAGED(accented-filename) control OK (staged cmake/Waylând.cmake seen and reproved despite the accented byte, GATE-QUOTEPATH)"
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

# GATE-DEPZERO-NOFORK (01/09/2026): the three C-quoted forms a
# hostile filename can take that the OLD per-file shell loop never
# even saw (git-config(1): tab/newline/quote/backslash are quoted
# UNCONDITIONALLY, independent of core.quotepath). Each carries a real
# violation (include(FetchContent)) beside the clean sibling
# make_clean_fixture already writes, so the non-empty-scan floor stays
# above zero on its own and cannot be the reason a reprove happens.
hostile_filename_case() {
    case_name="$1"
    case "$case_name" in
        lf) printf 'cmake/Way\nland2.cmake' ;;
        dquote) printf 'cmake/Way"land2.cmake' ;;
        backslash) printf 'cmake/Way\\land2.cmake' ;;
    esac
}

# C1 (plan DEPZERO-NOFORK secao 3): tree mode. Also carries C4 (an
# assertion, not a separate control): the summary must COUNT the
# hostile file among "cmake file(s)" - "encontrados" now includes what
# used to be invisible, not just "reproved for the right reason".
selftest_negative_control_hostile_filename_tree() {
    scratch="$1"
    root="$scratch/negative-hostile-tree"
    # DEPZERO-SELFTEST-FIX: see selftest_negative_control_cmake's comment.
    hostile_tree_status=0

    for case_name in lf dquote backslash; do
        make_clean_fixture "$root"
        name="$(hostile_filename_case "$case_name")"
        printf 'include(FetchContent)\n' > "$root/$name"
        git_init_fixture "$root"

        if output="$(check_dep_zero_tree "$root" "NONE" 2>&1)"; then
            echo "selftest: NEGATIVE(hostile-filename/tree/$case_name) control FAILED (should have been reproved - GATE-DEPZERO-NOFORK)" >&2
            hostile_tree_status=1
        else
            if ! printf '%s\n' "$output" | grep -qF "FetchContent"; then
                echo "selftest: NEGATIVE(hostile-filename/tree/$case_name) control FAILED (reproved, but did not cite FetchContent)" >&2
                printf '%s\n' "$output" >&2
                hostile_tree_status=1
            fi
            if ! printf '%s\n' "$output" | grep -qF "4 cmake file(s)"; then
                echo "selftest: NEGATIVE(hostile-filename/tree/$case_name) control FAILED (reproved, but the found-count did not count the hostile file among 'cmake file(s)' - 'encontrados' must include what used to be invisible, C4)" >&2
                printf '%s\n' "$output" >&2
                hostile_tree_status=1
            fi
        fi
        rm -rf "$root"
    done

    [ "$hostile_tree_status" -eq 0 ] && echo "selftest: NEGATIVE(hostile-filename/tree) control OK (newline/double-quote/backslash filename, each seen, reproved and cited, GATE-DEPZERO-NOFORK)"
    return "$hostile_tree_status"
}

# C2: the same three forms, staged (pre-commit) mode.
selftest_staged_hostile_filename() {
    scratch="$1"
    root="$scratch/staged-hostile"
    # DEPZERO-SELFTEST-FIX: see selftest_negative_control_cmake's comment.
    hostile_staged_status=0

    for case_name in lf dquote backslash; do
        make_clean_fixture "$root"
        git_init_fixture "$root"
        name="$(hostile_filename_case "$case_name")"
        printf 'include(FetchContent)\n' > "$root/$name"
        git -C "$root" add "$name"

        if output="$(check_dep_zero_staged "$root" 2>&1)"; then
            echo "selftest: STAGED(hostile-filename/$case_name) control FAILED (should have been reproved - GATE-DEPZERO-NOFORK)" >&2
            hostile_staged_status=1
        elif ! printf '%s\n' "$output" | grep -qF "FetchContent"; then
            echo "selftest: STAGED(hostile-filename/$case_name) control FAILED (reproved, but did not cite FetchContent)" >&2
            printf '%s\n' "$output" >&2
            hostile_staged_status=1
        fi
        rm -rf "$root"
    done

    [ "$hostile_staged_status" -eq 0 ] && echo "selftest: STAGED(hostile-filename) control OK (newline/double-quote/backslash filename staged, each reproved and cited, GATE-DEPZERO-NOFORK)"
    return "$hostile_staged_status"
}

# C3: a tracked CMake file deleted from the WORKTREE (not the index)
# before the tree-mode scan runs. The OLD `cat "$root/$p"` failed
# silently (empty stdin, zero lines evaluated, no violation possible)
# and the file's disappearance never affected the verdict. The engine
# now reports it as an 'F|' open-refusal, and the shell's fail-closed
# found!=analyzed check must reprove. Deletion, never chmod 000
# (GODS_LAWS.md L-47: the CI container runs as root and ignores
# permission bits - a chmod-000 fixture would pass there for the wrong
# reason).
selftest_tree_missing_worktree_file() {
    scratch="$1"
    root="$scratch/tree-missing-worktree-file"
    make_clean_fixture "$root"
    git_init_fixture "$root"
    rm -f "$root/cmake/Wayland.cmake"

    if output="$(check_dep_zero_tree "$root" "NONE" 2>&1)"; then
        echo "selftest: TREE(missing-worktree-file) control FAILED (a tracked CMake file missing from the worktree must reprove - fail-closed, GODS_LAWS.md L-40)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    if ! printf '%s\n' "$output" | grep -qF "scan incomplete"; then
        echo "selftest: TREE(missing-worktree-file) control FAILED (reproved, but did not declare 'scan incomplete')" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    if ! printf '%s\n' "$output" | grep -qF "open refused"; then
        echo "selftest: TREE(missing-worktree-file) control FAILED (reproved, but did not cite 'open refused' for the missing file)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    echo "selftest: TREE(missing-worktree-file) control OK (a tracked file deleted from the worktree is refused, not silently treated as empty/clean)"
}

# I2 (adversarial review, DEPZERO-NOFORK, 01/09/2026): none of the
# controls above ever fed decode_git_quoted a malformed escape - the
# comment on the function promises "g_decode_ok=0 on any grammar
# violation", but that branch was never exercised. Proof at the time:
# swapping, in an EXTRACTED copy, the unknown-escape refusal for one
# that passes the escape through as literal still left --selftest at
# 37/37. This control feeds the SAME awk engine that consumes real
# `git ls-files` output with SYNTHETIC lines that emulate a malformed
# escape git itself never emits - fail-closed exists precisely for the
# escape nobody foresaw, so the control cannot depend on git producing
# one. Three distinct grammar violations, one control:
#   "\z"    - unknown escape letter
#   "\      - dangling backslash: the only inner character IS the
#             backslash, so the loop runs out of string before it can
#             read an escape character after it
#   "\189"  - digits present but not all octal: the 3-character window
#             the parser grabs ("189") never matches [0-7][0-7][0-7]
selftest_decode_rejects_malformed_escapes() {
    scratch="$1"
    root="$scratch/decode-malformed-escapes"

    bad_lines='"\z"
"\"
"\189"'

    out="$(printf '%s\n' "$bad_lines" | run_dep_zero_engine "$root" "$root")" || {
        echo "selftest: DECODE-REFUSE control FAILED (the awk engine itself failed to run)" >&2
        return 1
    }
    refused_count="$(printf '%s\n' "$out" | grep -c 'decode refused (malformed git quoting)' || true)"
    failed_in_summary="$(printf '%s\n' "$out" | sed -n 's/^S|.*failed=\([0-9]*\)$/\1/p')"

    status=0
    if [ "$refused_count" -ne 3 ]; then
        echo "selftest: DECODE-REFUSE control FAILED (expected 3 malformed lines refused, got $refused_count) - I2" >&2
        printf '%s\n' "$out" >&2
        status=1
    fi
    if [ "$failed_in_summary" != "3" ]; then
        echo "selftest: DECODE-REFUSE control FAILED (summary declared failed=$failed_in_summary, expected 3 - the decode-refused branch must count toward the fail-closed 'failed' total)" >&2
        printf '%s\n' "$out" >&2
        status=1
    fi
    [ "$status" -eq 0 ] && echo "selftest: DECODE-REFUSE control OK (unknown escape, dangling backslash, and digits-but-not-octal all refused by decode_git_quoted and counted in 'failed' - I2)"
    return "$status"
}

# I3 (same adversarial review): \777 is 511 decimal, above the 255
# (\377) ceiling a real byte can hold. The OLD pattern
# [0-7][0-7][0-7] only checked each digit individually, so \777 (and
# \400 - 256 decimal, one past the ceiling) both matched and
# oct2dec/sprintf("%c", ...) silently folded them onto SOME byte
# instead of refusing - gawk 5.3.2 measured folding \777 onto the SAME
# 0xFF byte as the legitimate \377, indistinguishable from a real
# maximum-byte escape. The fix restricts the first digit to [0-3] (a
# byte's high octal digit never exceeds 3). \377 itself, the real
# boundary, must keep decoding - this control checks both directions.
selftest_decode_rejects_octal_out_of_range() {
    scratch="$1"
    root="$scratch/decode-octal-range"

    invalid_lines='"\777"
"\400"'
    valid_line='"\377"'

    out_invalid="$(printf '%s\n' "$invalid_lines" | run_dep_zero_engine "$root" "$root")" || {
        echo "selftest: OCTAL-RANGE control FAILED (the awk engine itself failed to run on the invalid lines)" >&2
        return 1
    }
    refused_count="$(printf '%s\n' "$out_invalid" | grep -c 'decode refused (malformed git quoting)' || true)"

    out_valid="$(printf '%s\n' "$valid_line" | run_dep_zero_engine "$root" "$root")" || {
        echo "selftest: OCTAL-RANGE control FAILED (the awk engine itself failed to run on the valid boundary line)" >&2
        return 1
    }
    valid_refused="$(printf '%s\n' "$out_valid" | grep -c 'decode refused (malformed git quoting)' || true)"

    status=0
    if [ "$refused_count" -ne 2 ]; then
        echo "selftest: OCTAL-RANGE control FAILED (\\777 and \\400 - both above \\377 - should have been refused; expected 2, got $refused_count) - I3" >&2
        printf '%s\n' "$out_invalid" >&2
        status=1
    fi
    if [ "$valid_refused" -ne 0 ]; then
        echo "selftest: OCTAL-RANGE control FAILED (\\377, the real maximum byte, was wrongly refused)" >&2
        printf '%s\n' "$out_valid" >&2
        status=1
    fi
    [ "$status" -eq 0 ] && echo "selftest: OCTAL-RANGE control OK (\\777 and \\400 refused for sitting above \\377, and \\377 itself still decodes - I3, GATE-DEPZERO-NOFORK)"
    return "$status"
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
    run_control selftest_empty_scan_tree_cmake_only "$scratch"
    run_control selftest_negative_control_accented_filename_tree "$scratch"
    run_control selftest_staged_accented_filename "$scratch"
    run_control selftest_staged_zero_relevant_declared "$scratch"
    run_control selftest_staged_blocks_violation "$scratch"
    run_control selftest_staged_reads_index_not_worktree "$scratch"
    run_control selftest_staged_warns_ambiguous "$scratch"
    run_control selftest_escape_via_allowlist_edit "$scratch"
    run_control selftest_warn_counter_declared "$scratch"
    run_control selftest_negative_control_hostile_filename_tree "$scratch"
    run_control selftest_staged_hostile_filename "$scratch"
    run_control selftest_tree_missing_worktree_file "$scratch"
    run_control selftest_decode_rejects_malformed_escapes "$scratch"
    run_control selftest_decode_rejects_octal_out_of_range "$scratch"

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
