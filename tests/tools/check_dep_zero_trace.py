#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_dep_zero_trace.py - CI oracle for GODS_LAWS.md L-07 (dependency
# zero) that ASKS CMAKE WHAT IT EXECUTED instead of interpreting the
# TEXT of CMakeLists.txt/*.cmake files.
#
# WHY THIS FILE EXISTS, AND WHY IT REPLACES A TEXT SCANNER
# ----------------------------------------------------------------
# tests/tools/check_dep_zero.sh (the previous, text-based gate for the
# same law) was reproved TWICE by adversarial review
# (/var/tmp/glintfx-plan/REVIEW-DEPZERO-GATE.md,
# REVIEW2-DEPZERO-GATE.md), nine escapes total. The signature was
# always the same: the gate was a hand-written parser of the CMake
# language in POSIX sh, and every fix taught an attacker a new
# spelling that the regex had not seen yet (multi-line calls,
# `include(${var})` + `cmake_language(CALL ${fn} ...)` indirection,
# CPM's three sibling entry points besides CPMAddPackage, a bare
# `add_subdirectory` with no textual anchor at all). Text cannot
# decide what a PROGRAM does; the interpreter that runs the program
# can. Decision of the leader (AskUserQuestion, 28/08/2026, verbatim):
# "Trocar o mecanismo: perguntar ao CMake" - see
# /var/tmp/glintfx-plan/brief-DEPZERO-TRACE.md for the three-rodada
# history and /var/tmp/glintfx-plan/PLANO-DEPZERO-TRACE.md for the
# design this file implements (its own section 3, rules R1..R6, the
# four floor checks, and the --selftest shape).
#
# check_dep_zero.sh is NOT touched by this fatia (DEPZERO-TRACE, the
# second of three - see the ORDEM DE SERVICO). It keeps running as
# dep_zero_test/dep_zero_selftest: sub-check (b) (include surface) is
# undefeated in two rounds of adversarial review and stays the titular
# oracle for "did a C++ file #include something foreign"; sub-check
# (c) (the built artifact's own DT_NEEDED via readelf) is the final
# truth for "did the LINKED binary carry something foreign in" and was
# never fooled either. Only sub-check (a) - "did the CMake surface
# introduce a dependency" - is superseded here, because THAT is the
# one nine escapes were found against. Retiring check_dep_zero.sh's
# own CMake-text machinery is a SEPARATE fatia (DEPZERO-SHALLOW),
# scoped to run only after this oracle has bitten in the real CI at
# least once (GODS_LAWS.md, house rule of 28/08/2026: "portao que
# nunca reprovou nao e portao" - a gate proven only against fixtures
# under mktemp has not yet earned that trust).
#
# TWO ORACLES, TWO INDEPENDENT SOURCES OF TRUTH
# ----------------------------------------------------------------
#   1. THE TRACE (--trace-expand --trace-format=json-v1): a promise
#      documented by cmake(1) - "useful for external tools such as
#      CIs or debuggers" - of one JSON document per line, every value
#      ALREADY EXPANDED (variables, cmake_language(CALL ...)
#      indirection, multi-line formatting: all resolved by CMake's own
#      interpreter before the line ever reaches this parser). Prior
#      art: Meson introspects CMake subprojects the same way
#      (github.com/mesonbuild/meson issues #11521, #10104 - both also
#      warn the mechanism has been flaky at the EDGE in their own
#      history, which is why this file pins the format to json-v1 and
#      carries the floor checks below instead of trusting a bare
#      "it ran" verdict).
#   2. THE CODEMODEL (file API, codemodel-v2): the SAME reply the real
#      IDEs (VS Code CMake Tools, CLion) consume as ground truth for a
#      project's own target graph, on every platform including
#      Windows (cmake-file-api(7)). Witness, not primary: R6 below.
#
# PYTHON3 IS THE AUTHORIZED TOOL FOR THIS JOB, NOT A HOUSE-STYLE BREAK
# ----------------------------------------------------------------
# GODS_LAWS.md L-07, "ESCLARECIMENTO DE FRONTEIRA" of 28/08/2026: a
# tool that helps BUILD OR VERIFY and is never linked into the shipped
# artifact is not a dependency - the same category pkg-config,
# wayland-scanner, CMake and Ninja already lived in. python3 is
# installed by default on four of the five supported platforms
# (measured live, 28-29/08/2026, in the SAME container images the CI
# matrix uses: fedora:latest, ubuntu:latest, archlinux:latest all
# lacked it and needed one explicit install line each;
# cachyos/cachyos:latest already ships python3 3.14.7; windows-latest
# ships Python 3.12.10 by default per actions/runner-images' own
# Windows2025-Readme.md, fetched 29/08/2026 - GODS_LAWS.md L-43). The
# ALTERNATIVE was measured and rejected, not assumed inferior:
# `cmake -P` with `string(JSON)` parsed only 1409 of 22330 real trace
# lines on this project's own tree - CMake's OWN list semantics lose
# 94% of the events IN SILENCE, which is the exact defect class
# GODS_LAWS.md L-40 exists to forbid. Reading CMake's own trace with
# CMake's own scripting language reopens the "guess the escape" war
# the leader ordered closed. A real interpreter (json.loads, stdlib
# only, no third-party package) does not have that failure mode.
#
# THE SIX RULES, EACH CLOSED BY FORM, NOT BY A GROWING NAME LIST
# ----------------------------------------------------------------
#   R1 - a forbidden FETCH COMMAND was executed, under ANY spelling
#        (direct call, cmake_language(CALL ...), a variable holding
#        the function name): the trace records the RESOLVED command
#        name CMake actually ran, never the source text that reached
#        it. Measured live (29/08/2026, /var/tmp/depzero-trace-
#        explore/indirect_trace.json): `cmake_language(CALL
#        FetchContent_Declare fmt ...)` produces TWO trace events -
#        one for the cmake_language dispatch itself, and a SECOND,
#        independent one with cmd == "fetchcontent_declare" and the
#        real, expanded arguments. R1 only has to look at "cmd";
#        indirection dissolves before it gets there.
#   R2 - a forbidden MODULE was loaded and EXECUTED, whether or not
#        anything inside it was ever called by name. Loading the
#        module is the violation (its own top-level statements run
#        immediately). Measured live: `include(${mod_name})` with
#        mod_name == "FetchContent" produces, among the events sourced
#        FROM CMakeLists.txt, at least one event whose OWN "file" is
#        the absolute path to FetchContent.cmake itself (its line 1146
#        include of ExternalProject/shared_internal_commands.cmake) -
#        the module denounces itself by having RUN, independent of R1.
#   R3 - find_package() outside the allowlist, PLUS the "module
#        witness": any event sourced from a file shaped like
#        Find<X>.cmake or <X>Config[-version].cmake whose <X> is not
#        allowlisted is ALSO a violation, even with zero find_package
#        calls in the trace - this is what catches a direct
#        `include(FindOpenSSL)` that skips the find_package() command
#        entirely. Verified NOT to false-positive on the project's own
#        legitimate PkgConfig usage: FindPkgConfig.cmake's own body
#        (find_program, execute_process, ...) sources dozens of real
#        events, all correctly recognized as "PkgConfig", allowlisted.
#   R4 - pkg_check_modules() naming a module outside the allowlist.
#        Operates on the trace's OWN already-tokenized, already-
#        expanded argument LIST - a multi-line call is indistinguishable
#        from a one-line call once CMake has parsed it, which is
#        exactly the property that defeated check_dep_zero.sh's own
#        line-oriented scanner (REVIEW-DEPZERO-GATE.md CRITICO #1).
#        Keyword vocabulary read from FindPkgConfig.cmake's own
#        documented signature (/usr/share/cmake/Modules/
#        FindPkgConfig.cmake, "pkg_check_modules" and
#        "pkg_search_module" command blocks), not guessed: REQUIRED,
#        QUIET, NO_CMAKE_PATH, NO_CMAKE_ENVIRONMENT_PATH,
#        IMPORTED_TARGET, GLOBAL. A version comparator suffix
#        (<module><cmp><version>) is trimmed before the allowlist
#        check, so a digit-leading module name (the "7zip" escape,
#        REVIEW-DEPZERO-GATE.md CRITICO of the SAME shape as check_no_
#        x11.sh's own historical bug class) is judged on its actual
#        name, never mistaken for a bare version token.
#   R5 - add_subdirectory() naming a directory outside the source
#        root. args[0] (already expanded by --trace-expand) is
#        resolved against dirname(event["file"]) when relative,
#        realpath()'d, and required to sit under realpath(source_root)
#        - the door the previous gate never touched at all (REVIEW-
#        DEPZERO-GATE.md CRITICO #3, "nenhum padrao do portao toca
#        este comando").
#   R6 - the file API's own target graph, as a WITNESS (not the
#        primary oracle - R1..R5 already cover the trace exhaustively):
#        every target's paths.source must resolve under the source
#        root (a second, independent proof that R5's door is shut),
#        and every fragment in every target's linkLibraries must sit
#        in a small, MEASURED allowlist. linkLibraries/
#        interfaceLinkLibraries are new codemodel fields as of CMake
#        4.2 (codemodel object version >= 2.9 - cmake-file-api(7),
#        release notes cmake.org/cmake/help/latest/release/4.2.html) -
#        measured live (29/08/2026) that this machine's CMake 4.3.0
#        reports codemodel version 2.10 and the field is present; ALSO
#        measured live that this project's own Windows CI job pins
#        CMake 4.1.6 on purpose (CI-WIN comment,
#        .github/workflows/ci.yml, "testing the true floor
#        cmake_minimum_required declares"), which predates 4.2. R6(b)
#        (the linkLibraries witness) therefore DEGRADES, DECLARED, on
#        any codemodel older than 2.9: it is skipped with a printed
#        note, never silently, and R6(a) (paths.source) still runs -
#        this is the SAME "field absent is not automatically an
#        error, but declare what you could not check" shape as the
#        codemodel's own documented "dependencies" field, which can be
#        absent on ANY target regardless of CMake version (CMake
#        Discourse, discourse.cmake.org/t/how-to-traverse-target-link-
#        libraries-dependencies-in-the-file-api/14384) - this file
#        reads BOTH fields with .get(), never presuming presence.
#        Coverage is not reduced by the degradation: R1..R5, which do
#        not depend on the CMake version at all (--trace-format=json-v1
#        has existed since CMake 3.17), are the primary enforcement on
#        every platform; R6(b) is defense in depth, present where the
#        CMake version allows it and DECLARED where it does not - the
#        same shape as every other "declared downgrade" already in
#        this codebase (see check_exports.sh's own Windows note).
#
# FOUR FLOOR CHECKS (GODS_LAWS.md L-40: "contou zero, reprova - nunca
# um sucesso silencioso"), every one of them printed on BOTH verdicts:
#   (i)   PARITY - every non-header line of the trace file must parse
#         as JSON; a line that does not is counted and REPROVES with
#         the count ("parser perdeu N linha(s)"). This is the harness
#         that would have caught `cmake -P`'s own 94%-silent-loss bug
#         had this file used that tool instead of python3 - kept as a
#         live regression guard even though json.loads does not share
#         that failure mode, because "a parser as PROMISE, not a
#         parser as absence of a bug I have seen" is exactly what
#         L-40 asks for.
#   (ii)  NON-EMPTY REPO SCAN - at least one trace event must be
#         sourced from a file under the source root, and the count is
#         printed on the passing path too (never just on failure) -
#         GODS_LAWS.md L-40 item 3, "a contagem aparece na saida,
#         mesmo quando passa".
#   (iii) POSITIVE SENTINEL - the root CMakeLists.txt's own project()
#         call MUST appear in the trace, on all five platforms; on
#         non-Windows platforms (where this project's own
#         if(UNIX)-guarded PkgConfig branch always runs -
#         CMakeLists.txt), a SECOND sentinel (a find_package event
#         naming "PkgConfig") is also required. Missing sentinel means
#         the configure did not really run against this tree, or ran
#         against the wrong one, or the trace redirect silently failed
#         - EXACTLY the shape the orchestrator hit personally while
#         measuring this fatia (brief-DEPZERO-TRACE.md: "a armadilha
#         de invocacao medida em mim mesmo... re-configure sem -S
#         falhou engolido pelo redirect e produziu traco de 1 linha
#         'verde'"). This floor turns that near-miss into a hard
#         reprova with a named cause, on every run, forever.
#   (iv)  NON-EMPTY CODEMODEL - the codemodel reply must resolve with
#         at least one target; an "error" reply object (the file
#         API's own documented shape when "no buildsystem generated" -
#         measured live, 29/08/2026, forcing a REQUIRED pkg_check_
#         modules() to fail: CMake writes reply/error-<ts>.json with
#         exactly {"reply": {"codemodel-v2": {"error": "..."}}}, no
#         index-<ts>.json at all) or a target list of length zero both
#         REPROVE here, by name, instead of silently skipping R6.
#
# A SEPARATE, IMPORTANT MEASUREMENT: cmake's OWN exit code is NOT a
# reliable signal for "did this gate run correctly". A REQUIRED
# pkg_check_modules() that cannot find its module makes CMake itself
# exit 1 with a FATAL_ERROR - but the trace file, written incrementally
# as CMake executes each statement, still faithfully records the
# pkg_check_modules() call and its real, expanded arguments BEFORE the
# fatal error fires (measured live, 29/08/2026,
# /var/tmp/depzero-trace-explore/pkgcheck7zip_trace.json: the
# violating call is present and citable by file:line even though
# `cmake` itself returned 1). This file therefore NEVER treats a
# non-zero cmake return code as "abort, cannot judge" - it always
# attempts to read and judge whatever trace and codemodel reply DO
# exist, and only the floor checks above (which do not care about
# cmake's own exit code) decide whether the run counts as usable.
#
# Usage:
#   check_dep_zero_trace.py <source-root-directory> <build-directory>
#       real mode - re-configures <build-directory> (which must
#       already exist and have been configured once, normally by the
#       SAME ctest run this test is registered into) with
#       --trace-expand --trace-format=json-v1, requests the codemodel-
#       v2 file API reply, and judges both. RUN_SERIAL (tests/
#       CMakeLists.txt) is required on the registered test: this
#       re-configure regenerates build files under a shared directory,
#       and running it concurrently with another ctest case would
#       race the generator.
#   check_dep_zero_trace.py --selftest
#       the six rules plus the four floor checks, each proven against
#       a REAL, disposable CMake project configured under mktemp
#       (never against the tracked tree) - this file runs actual
#       cmake invocations in --selftest too, because the mechanism
#       IS "run cmake for real"; there is no meaningful way to fake
#       that and still prove anything.
#
# Each function below does one thing (GODS_LAWS.md L-17).

import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

SCRIPT_NAME = "check_dep_zero_trace.py"

# --- the closed, form-based rule tables (GODS_LAWS.md L-07) -------------

# R1: commands whose mere EXECUTION, under any name that resolves to
# them, is the violation - see the header comment for why indirection
# (cmake_language(CALL ...), a variable holding the function name)
# does not survive to reach this set: the trace records the RESOLVED
# command CMake actually ran.
FETCH_COMMANDS = frozenset({
    "fetchcontent_declare",
    "fetchcontent_makeavailable",
    "fetchcontent_populate",
    "externalproject_add",
    "cpmaddpackage",
    "cpmfindpackage",
    "cpmdeclarepackage",
    "cpmgetpackage",
})

# R2: modules whose mere LOADING is the violation. "fetchcontentserial.
# cmake" is named in the plan this file implements
# (/var/tmp/glintfx-plan/PLANO-DEPZERO-TRACE.md section 3, rule R2) but
# was NOT found under this machine's CMake 4.3.0 Modules/ tree when
# searched live (29/08/2026: `find /usr/share/cmake* -iname
# '*fetchcontent*'` lists only FetchContent.cmake and the FetchContent/
# support directory, no "Serial" file). Kept in the set anyway per the
# plan's explicit text - a name that never matches costs nothing, and
# R1 (which does not depend on which module file a call routes
# through) is the primary defense for the commands this entry would
# have covered regardless. Declared here rather than silently dropped
# (GODS_LAWS.md L-27: fact separated from inference).
FETCH_MODULE_BASENAMES = frozenset({
    "fetchcontent.cmake",
    "externalproject.cmake",
    "fetchcontentserial.cmake",
})

# R3: find_package() names this project already accepts, casefolded.
#
# NOTE, kept because it is a real, measured finding (GODS_LAWS.md
# L-27): this gate's own registration (tests/CMakeLists.txt) first
# tried find_package(Python3 COMPONENTS Interpreter REQUIRED) to
# resolve the interpreter, and R3 correctly reproved it the first time
# this test ran for real ("find_package() outside the allowlist:
# Python3", citing tests/CMakeLists.txt by file:line) - proof the rule
# bites on the project's OWN CMake surface, not only on fixtures.
# tests/CMakeLists.txt now uses find_program() instead (see that
# file's own comment on the switch: find_package(Python3 ...) is
# ALSO watched by check_dep_zero.py (check_dep_zero.sh at the time
# this note was written, ported to Python since, DEPZERO-GATE-PY),
# the sibling text-based gate this fatia is explicitly forbidden from
# editing - find_program() satisfies both oracles without widening
# scope).
# "python3"/"Python3" is therefore NOT in this allowlist: nothing in
# this project's real CMake surface calls find_package(Python3)
# anymore, and adding an unused entry would be exactly the kind of
# allowlist drift GODS_LAWS.md L-07 exists to prevent (an entry that
# no longer maps to any real, exercised call).
FIND_PACKAGE_ALLOWLIST = frozenset({"pkgconfig", "glintfx"})

# R3 witness: file-basename shapes CMake uses for the two module kinds
# find_package() can load - "Find<X>.cmake" (traditional find modules)
# and "<X>Config.cmake" / "<X>-config.cmake" / "<X>ConfigVersion.cmake"
# (config-file packages). Matched case-insensitively, same reasoning
# as FIND_PACKAGE_ALLOWLIST above: a filesystem that is case-sensitive
# on four of the five platforms and is not on the fifth must not be
# the thing that decides whether a name is allowed.
_FIND_MODULE_RE = re.compile(r"^find(?P<name>.+)\.cmake$", re.IGNORECASE)
_CONFIG_MODULE_RE = re.compile(
    r"^(?P<name>.+?)-?config(-version)?\.cmake$", re.IGNORECASE
)

# R4: pkg_check_modules()'s own keyword vocabulary, read from
# FindPkgConfig.cmake's own documented command signature (see header
# comment for the exact source), not guessed.
PKG_CHECK_MODULES_KEYWORDS = frozenset({
    "REQUIRED",
    "QUIET",
    "NO_CMAKE_PATH",
    "NO_CMAKE_ENVIRONMENT_PATH",
    "IMPORTED_TARGET",
    "GLOBAL",
})

# R4: pkg-config module names this project already links against.
# Case-sensitive on purpose: pkg-config module names are literal
# strings handed to pkgconf, never resolved against a filesystem path,
# so there is no cross-platform case-folding question here the way
# there is for R3's CMake module filenames.
PKG_CHECK_MODULES_ALLOWLIST = frozenset({"wayland-client"})

# R6(b): codemodel target linkLibraries fragments this project already
# links. Measured live against the real tree, 28-29/08/2026 (cmake
# 4.3.0, codemodel v2.10): ['wayland-client', 'm']. Windows entry is
# seeded EMPTY on purpose (GODS_LAWS.md house rule of 25/08/2026:
# "portao que nunca rodou no ambiente real nao e portao" - a number
# for a platform this fatia has not yet exercised in real CI is a
# guess, not a measurement, and a guess written down as a fact ages
# into exactly the L-40 defect this whole file exists to avoid). If a
# real Windows CI run reproves R6(b) on a legitimate library, measure
# the log and add it here with the same citation shape as this
# comment - never widen this ahead of the measurement.
CODEMODEL_LINK_LIBRARIES_ALLOWLIST = frozenset({"wayland-client", "m"})

# codemodel-v2 target "type" values that compile source files, per
# cmake-file-api(7)'s own documented enumeration - the complement
# ("UTILITY", made by add_custom_target(); "INTERFACE_LIBRARY", header-
# only by construction) has NO source files by design and is exactly
# the false-positive check_floor_r9_sources_nonempty() below must NOT
# flag: this file's OWN --selftest fixtures for find_package/pkg_check_
# modules/execute_process each add a bare add_custom_target(dummy) on
# purpose (their own comments: "so FLOOR(codemodel) judges what this
# control actually means to prove"), and treating THEIR zero sources as
# suspicious would make the new floor reprove three genuinely clean
# controls the first time it ran - caught locally before ever reaching
# the server, not a fourth guess.
COMPILED_TARGET_TYPES = frozenset(
    {"EXECUTABLE", "STATIC_LIBRARY", "SHARED_LIBRARY", "MODULE_LIBRARY", "OBJECT_LIBRARY"}
)

# R7 (REVIEW3-DEPZERO-TRACE.md CRITICO #1): file(DOWNLOAD ...) and
# file(UPLOAD ...) are CMake's own NATIVE network transfer commands -
# no FetchContent/ExternalProject/CPM/find_package/pkg_check_modules
# involved at all, so R1..R6 (which only look at DEPENDENCY-MANAGER
# vocabulary) never saw them. Blocked UNCONDITIONALLY, not via an
# allowlist: the reviewer measured, live, that the real tree has
# exactly TWO file() calls in total, neither one DOWNLOAD or UPLOAD
# (confirmed independently while writing this fix: `git grep -n
# "file("` finds file(MAKE_DIRECTORY ...) in cmake/
# GlintfxWaylandProtocols.cmake and file(GENERATE ...) in cmake/
# GlintfxPkgConfigValidate.cmake - neither is network-capable). A
# command with zero legitimate uses today and a real, reproduced
# network vector needs no allowlist to grow later; the leader's own
# decision (relayed via the orchestrator) was explicit: "bloqueio
# incondicional e seguro". Independently re-confirmed while writing
# this fix (`git grep -nE '(^|[^a-zA-Z_])file\(' -- '*.cmake'
# '*.txt'`, word-boundary so it does not also match configure_file()):
# exactly two real file() calls in the whole tree, file(MAKE_DIRECTORY
# ...) (cmake/GlintfxWaylandProtocols.cmake) and file(SHA256 ...)
# (src/render/CMakeLists.txt) - neither DOWNLOAD nor UPLOAD, matching
# the reviewer's own count exactly. Scoped to DOWNLOAD/UPLOAD only -
# NOT ARCHIVE_EXTRACT/COPY, which operate on a file ALREADY on disk
# and were not part of what was measured or asked; widening this set
# is a future decision, not one this fix makes unilaterally.
FILE_SUBCOMMANDS_FORBIDDEN = frozenset({"download", "upload"})

# R8 (REVIEW3-DEPZERO-TRACE.md CRITICO #2): execute_process(COMMAND
# ...) runs ANY external program with the full authority of the
# configure process - curl, wget, git clone, an interpreter fed a
# script on stdin, anything. R1..R6 never looked at it either. Unlike
# file(DOWNLOAD)/file(UPLOAD), this one has exactly ONE real,
# necessary use in the tree TODAY (cmake/GlintfxWaylandProtocols.cmake,
# glintfx_locate_xdg_shell_protocol_xml(): `execute_process(COMMAND
# "${PKG_CONFIG_EXECUTABLE}" --variable=pkgdatadir wayland-protocols
# ...)`, resolving where the SYSTEM's wayland-protocols package keeps
# xdg-shell.xml - GODS_LAWS.md L-05's own Wayland-without-vendoring
# design depends on this call existing), so an unconditional block
# would false-positive on the very first real run (the exact "portao
# que barra o certo e desligado" failure the leader named). Judged by
# the PROGRAM actually executed (the trace's own args are already
# expanded, so "${PKG_CONFIG_EXECUTABLE}" resolves to the real,
# absolute path CMake found), matched by basename with any Windows
# ".exe"/".bat"/".cmd" suffix stripped and casefolded - the same
# cross-platform-name reasoning R3's witness already uses for CMake
# module basenames, applied here to an executable name instead.
EXECUTE_PROCESS_PROGRAM_ALLOWLIST = frozenset({"pkg-config", "pkgconf"})

_VERSION_COMPARATOR_RE = re.compile(r"^([^<>=!]+)")
_EXECUTABLE_SUFFIX_RE = re.compile(r"\.(exe|bat|cmd)$", re.IGNORECASE)


# --- small, single-purpose helpers ---------------------------------------


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


class Violation:
    """One reprovable fact: which rule, where, and why."""

    def __init__(self, rule, file, line, message):
        self.rule = rule
        self.file = file
        self.line = line
        self.message = message

    def format(self):
        return f"{self.file}:{self.line}: [{self.rule}] {self.message}"


def casefold_strip(text):
    return text.strip().casefold()


def strip_version_comparator(token):
    """"<module><cmp><version>" -> "<module>" (R4's own trimming rule)."""
    match = _VERSION_COMPARATOR_RE.match(token)
    return match.group(1) if match else token


def event_file_basename(event):
    return os.path.basename(event.get("file", ""))


def is_under_root(candidate_path, root_path):
    # REVIEW3-DEPZERO-TRACE.md CRITICO #4: os.path.realpath("") resolves
    # to the process's own CURRENT WORKING DIRECTORY, not to an invalid
    # sentinel - confirmed live by the reviewer, and reproduced here:
    # `os.path.realpath("")` on this machine, run from inside the repo,
    # returns the repo root itself. Every caller of this function passes
    # a trace event's own "file" field (or a path built from it), and a
    # malformed/truncated trace event missing that field used to reach
    # here as event.get("file", "") == "" - which this function then
    # silently and WRONGLY judged as "under the root", because the empty
    # string resolved to cwd, and ctest/this script both normally run
    # with cwd INSIDE source_root. Fail closed instead: an empty or
    # non-absolute candidate is NEVER "under root", full stop, before
    # realpath() ever gets a chance to reinterpret it as something else.
    # A well-formed trace event's "file" is always an absolute path (the
    # json-v1 trace format's own guarantee); anything else reaching this
    # function is malformed input, and malformed input does not get the
    # benefit of the doubt.
    if not candidate_path or not os.path.isabs(candidate_path):
        return False
    real_candidate = os.path.realpath(candidate_path)
    real_root = os.path.realpath(root_path)
    # DEPZERO-TRACE, R9's Windows debut (server run 33253849485,
    # 29/08/2026, ORDEM DE SERVICO OS-WINDOWS-R9): realpath() resolves
    # symlinks AND, per Python's own documented Windows behavior
    # (ntpath.realpath uses GetFinalPathNameByHandle, which the
    # Microsoft docs describe as querying "the normalized name of each"
    # path component in turn - it walks the WHOLE path, not just the
    # leaf), short 8.3 names too - so a RUNNER~1-vs-runneradmin
    # mismatch should not survive past the two realpath() calls above,
    # on its own. What this function did NOT do, and every OTHER
    # realpath()-then-compare site in this file already does
    # (same_path_platform_aware(), written for check_floor_sentinel
    # with the identical "these are already-resolved paths" framing),
    # is normcase() the result before comparing: NTFS is
    # case-preserving but case-INSENSITIVE, and nothing guarantees the
    # two independent realpath() calls above return identical casing
    # for a path component neither side of this comparison ever wrote
    # itself (a Windows profile/temp directory reached here via a
    # literal Python string built from the raw TEMP environment
    # variable on one side, and CMake's own codemodel report on the
    # other) - this was the one is_under_root() call in the whole file
    # that skipped the pattern every sibling call already follows.
    # Verified this does not change Linux/macOS behavior: normcase() is
    # a documented no-op there (posixpath.normcase == identity), and
    # this file's own --selftest (every hostile/positive control)
    # resolves identically before and after this change.
    normalized_candidate = os.path.normcase(real_candidate)
    normalized_root = os.path.normcase(real_root)
    return normalized_candidate == normalized_root or normalized_candidate.startswith(
        normalized_root + os.sep
    )


# --- trace acquisition -----------------------------------------------------


def prepare_codemodel_query(build_dir):
    """Registers the file API's shared, stateless codemodel-v2 query.

    An empty file at this exact path is CMake's own documented
    convention for "a client that does not care about identifying
    itself" (cmake-file-api(7), "Shared Stateless Queries") - the
    reply then appears in the reply/ directory of the SAME build tree
    after the next configure, no client subdirectory needed.
    """
    query_dir = os.path.join(build_dir, ".cmake", "api", "v1", "query")
    os.makedirs(query_dir, exist_ok=True)
    query_file = os.path.join(query_dir, "codemodel-v2")
    if not os.path.exists(query_file):
        open(query_file, "a", encoding="utf-8").close()


def clear_stale_codemodel_reply_pointers(build_dir):
    """Deletes any existing index-*.json/error-*.json in reply/.

    REVIEW3-DEPZERO-TRACE.md CRITICO #4, second half: read_codemodel_
    reply() below picks "the newest index-*.json/error-*.json by
    mtime" - which, before this fix, had NO tie to the specific
    reconfigure that produced the trace being judged. If THIS
    reconfigure crashes, hangs, or is killed (OOM, disk full - the
    exact near-miss this file's own header already records happening
    to the orchestrator once) AFTER CMake starts writing trace lines
    but BEFORE the file API gets to regenerate its reply, an OLD reply
    from a PREVIOUS, successful configure of the SAME build_dir just
    sits there and gets picked up as if it were fresh - reproduced live
    by the reviewer against the real glintfx build/: a synthetic
    5-line truncated trace plus a stale reply's real 61 targets add up
    to "0 violation(s)" with full confidence, describing a configure
    that never actually finished.
    Deleting the pointer files (never the content-addressed target-
    *.json/codemodel-v2-*.json siblings, which are safe and CORRECT to
    reuse when their content has not changed) makes the fix
    unconditional and cheap: after this reconfigure, EITHER a fresh
    index-*.json/error-*.json exists (this run genuinely reached the
    point of writing one), OR reply/ has none at all - and
    read_codemodel_reply()'s existing "no index-*.json or error-*.json
    in reply/" floor failure already covers that second case correctly.
    No mtime comparison can achieve the same guarantee: mtime only
    orders what is ALREADY there, it cannot distinguish "this run wrote
    nothing" from "an old run's file happens to be the newest one
    present" the way deleting first, unconditionally, does.
    """
    reply_dir = os.path.join(build_dir, ".cmake", "api", "v1", "reply")
    if not os.path.isdir(reply_dir):
        return
    for name in os.listdir(reply_dir):
        if name.startswith("index-") or name.startswith("error-"):
            os.remove(os.path.join(reply_dir, name))


def run_configure_with_trace(source_root, build_dir, trace_path):
    """Re-configures build_dir, writing the json-v1 trace to trace_path.

    Deliberately does NOT branch on the returncode here - see the
    header comment's "cmake's own exit code is not a reliable signal"
    section. The caller always proceeds to read whatever trace and
    codemodel reply exist; the floor checks decide usability.
    """
    prepare_codemodel_query(build_dir)
    clear_stale_codemodel_reply_pointers(build_dir)
    command = [
        "cmake",
        "-S",
        source_root,
        "-B",
        build_dir,
        "--trace-expand",
        "--trace-format=json-v1",
        f"--trace-redirect={trace_path}",
    ]
    try:
        return subprocess.run(command, capture_output=True, text=True)
    except FileNotFoundError:
        fail("'cmake' not found in PATH")


def read_trace_lines(trace_path):
    if not os.path.isfile(trace_path):
        return []
    with open(trace_path, "r", encoding="utf-8") as handle:
        return handle.read().splitlines()


def parse_trace_events(lines):
    """Returns (events, invalid_count). See floor check (i).

    An event line can be unusable two ways: it may not be valid JSON
    at all (json.loads raises), or it may be valid JSON that is
    missing "file" or "cmd" (REVIEW3-DEPZERO-TRACE.md CRITICO #4: a
    trace TRUNCATED mid-configure can still contain well-formed-but-
    INCOMPLETE JSON objects on the lines it did manage to flush - a
    real, reproduced-live shape, not a hypothetical one. Before this
    fix, such an event's missing "file" silently became "" via
    event.get("file", ""), and is_under_root("", source_root) returned
    True by accident (os.path.realpath("") resolves to the process's
    own cwd, which is normally INSIDE source_root) - so a truncated
    trace could pass FLOOR(repo-scan) by counting events that prove
    NOTHING about where they came from. Both failure shapes are
    counted the SAME way here, as "this parser could not use this
    line" - exactly what FLOOR(parity) exists to catch: a parser that
    quietly drops or misreads lines must never look identical to one
    that read every line correctly.
    REVIEW4-DEPZERO-TRACE.md IMPORTANTE (item 3): the round-3 version of
    this check used `not event.get("file")`/`not event.get("cmd")` -
    truthy tests, not TYPE tests. A field of the WRONG type that is
    still truthy (measured live: `"file": 12345`, an integer) slipped
    past both checks as "present", was accepted into `events`, and
    later crashed `is_under_root()` with an unhandled `TypeError`
    (`os.path.isabs()` does not accept an int) - a real crash, not a
    bypass (ctest reports the crash as a failure, not "0 violation(s)"),
    but `invalid_count` LIED about it: it reported 0 invalid lines for
    an event that was, in fact, unusable. Fixed by testing the TYPE,
    not just truthiness - a non-string "file"/"cmd" is invalid the same
    way an absent or empty one is, and is counted (never crashes
    downstream, and FLOOR(parity) reports the true count instead of a
    stack trace).
    """
    events = []
    invalid = 0
    for raw_line in lines[1:]:  # lines[0] is the {"version": ...} header
        try:
            event = json.loads(raw_line)
        except json.JSONDecodeError:
            invalid += 1
            continue
        if not isinstance(event, dict):
            invalid += 1
            continue
        file_field = event.get("file")
        cmd_field = event.get("cmd")
        if not isinstance(file_field, str) or not file_field:
            invalid += 1
            continue
        if not isinstance(cmd_field, str) or not cmd_field:
            invalid += 1
            continue
        # Same type-not-just-truthiness reasoning extended to "args":
        # every rule function indexes into it (args[0], args[1:], ...);
        # a present-but-wrong-typed "args" (not a list) would crash the
        # SAME way "file"/"cmd" used to, the first time any rule tried
        # to use it. Absent "args" is fine (treated as [] downstream by
        # every caller's own event.get("args", [])).
        args_field = event.get("args", [])
        if not isinstance(args_field, list):
            invalid += 1
            continue
        events.append(event)
    return events, invalid


def read_codemodel_reply(build_dir):
    """Returns (codemodel_dict_or_None, error_message_or_None).

    codemodel_dict is the top-level codemodel-v2 reply object itself
    (kind/version/paths/configurations), NOT the file-API index - the
    index is read here only to find which jsonFile to load next, or to
    learn the documented "error" shape (see header comment for the
    live-measured example).
    """
    reply_dir = os.path.join(build_dir, ".cmake", "api", "v1", "reply")
    if not os.path.isdir(reply_dir):
        return None, "no reply/ directory - the file API query never ran"
    candidates = [
        os.path.join(reply_dir, name)
        for name in os.listdir(reply_dir)
        if name.startswith("index-") or name.startswith("error-")
    ]
    if not candidates:
        return None, "no index-*.json or error-*.json in reply/"
    newest = max(candidates, key=os.path.getmtime)
    with open(newest, "r", encoding="utf-8") as handle:
        index = json.load(handle)
    codemodel_entry = index.get("reply", {}).get("codemodel-v2", {})
    if "error" in codemodel_entry:
        return None, f"codemodel-v2 query error: {codemodel_entry['error']}"
    json_file = codemodel_entry.get("jsonFile")
    if not json_file:
        return None, "codemodel-v2 reply entry has neither jsonFile nor error"
    with open(os.path.join(reply_dir, json_file), "r", encoding="utf-8") as handle:
        return json.load(handle), None


def read_target(reply_dir, json_file):
    with open(os.path.join(reply_dir, json_file), "r", encoding="utf-8") as handle:
        return json.load(handle)


# --- R1 / R2: the trace's own command and file-provenance fields ----------


def evaluate_r1_forbidden_commands(events):
    violations = []
    for event in events:
        cmd = event.get("cmd", "").lower()
        if cmd in FETCH_COMMANDS:
            violations.append(
                Violation(
                    "R1",
                    event.get("file", "?"),
                    event.get("line", "?"),
                    f"forbidden fetch/vendor command executed: {cmd}(...)"
                    " (GODS_LAWS.md L-07)",
                )
            )
    return violations


def evaluate_r2_forbidden_modules(events):
    """One violation per distinct forbidden module actually loaded.

    Deliberately reports only the FIRST event sourced from each
    forbidden basename (with a total count) rather than every one:
    loading FetchContent.cmake and then calling
    FetchContent_Declare() legitimately sources dozens of further
    events from FetchContent.cmake's own body (measured live,
    29/08/2026) - repeating the same violation dozens of times would
    bury the file:line that actually matters under noise, which is
    its own kind of "the gate does not really look" (GODS_LAWS.md
    L-40).
    """
    first_seen = {}
    counts = {}
    for event in events:
        basename = event_file_basename(event).lower()
        if basename in FETCH_MODULE_BASENAMES:
            counts[basename] = counts.get(basename, 0) + 1
            if basename not in first_seen:
                first_seen[basename] = event
    violations = []
    for basename, event in first_seen.items():
        violations.append(
            Violation(
                "R2",
                event.get("file", "?"),
                event.get("line", "?"),
                f"forbidden CMake module executed: {basename}"
                f" ({counts[basename]} statement(s) traced from it)"
                " (GODS_LAWS.md L-07)",
            )
        )
    return violations


# --- R3: find_package(), by literal argument AND by module witness -------
#
# The witness is scoped to include() EVENTS WHOSE OWN CALLING FILE IS
# UNDER THE SOURCE ROOT - not to "any trace event sourced from a
# matching-shaped file", which was tried first and produced a real,
# measured false positive: find_package(PkgConfig REQUIRED) legitimately
# makes CMake's OWN FindPkgConfig.cmake internally
# `include(FindPackageHandleStandardArgs)`, which in turn includes
# FindPackageMessage.cmake - both basenames matching the Find<X>.cmake
# shape, neither of them a package OUR code ever asked for. Caught live
# (29/08/2026, this file's own --selftest run against its first
# implementation) before it ever reached a reviewer: the witness's job
# is "did something WE WROTE reach for a module outside the allowlist",
# and CMake's own system modules including EACH OTHER is not that -
# scoping to the CALLING file (where the include() statement is
# textually written) is what tells the two apart, and it survives
# indirection exactly like R1/R2 do (args are expanded, so
# include(${var}) written in our own tree still shows the real name).


def _find_module_witness_name(argument):
    """"FindOpenSSL" / "FindOpenSSL.cmake" / "/abs/path/OpenSSLConfig.cmake"
    -> "OpenSSL". None if the argument is not shaped like either a
    traditional find-module or a config-file package.
    """
    stem = os.path.basename(argument)
    if stem.lower().endswith(".cmake"):
        stem = stem[: -len(".cmake")]
    match = _FIND_MODULE_RE.match(stem)
    if match:
        return match.group("name")
    match = _CONFIG_MODULE_RE.match(stem)
    if match:
        return match.group("name")
    return None


def evaluate_r3_find_package(events, source_root):
    violations = []
    witnessed_first_seen = {}
    for event in events:
        cmd = event.get("cmd", "").lower()
        args = event.get("args", [])
        if cmd == "find_package":
            if not args:
                continue
            name = args[0]
            if casefold_strip(name) not in FIND_PACKAGE_ALLOWLIST:
                violations.append(
                    Violation(
                        "R3",
                        event.get("file", "?"),
                        event.get("line", "?"),
                        f"find_package() outside the allowlist: {name}"
                        " (GODS_LAWS.md L-07)",
                    )
                )
        elif cmd == "include":
            if not args or not is_under_root(event.get("file", ""), source_root):
                continue  # only OUR OWN include() statements are witnessed
            witness_name = _find_module_witness_name(args[0])
            if witness_name is None:
                continue
            if casefold_strip(witness_name) in FIND_PACKAGE_ALLOWLIST:
                continue
            key = witness_name.casefold()
            if key not in witnessed_first_seen:
                witnessed_first_seen[key] = (witness_name, event)
    for witness_name, event in witnessed_first_seen.values():
        violations.append(
            Violation(
                "R3-witness",
                event.get("file", "?"),
                event.get("line", "?"),
                "our own tree include()s a find_package()-shaped module"
                f" outside the allowlist: {witness_name}"
                " (bypasses find_package() entirely, direct or indirect -"
                " either way it ran) (GODS_LAWS.md L-07)",
            )
        )
    return violations


# --- R4: pkg_check_modules(), on the trace's own tokenized arguments ------


def evaluate_r4_pkg_check_modules(events):
    violations = []
    for event in events:
        if event.get("cmd", "").lower() != "pkg_check_modules":
            continue
        args = event.get("args", [])
        if len(args) < 2:
            continue  # malformed call; CMake itself will already refuse it
        for token in args[1:]:  # args[0] is the result-variable prefix
            if token.upper() in PKG_CHECK_MODULES_KEYWORDS:
                continue
            module_name = strip_version_comparator(token)
            if module_name not in PKG_CHECK_MODULES_ALLOWLIST:
                violations.append(
                    Violation(
                        "R4",
                        event.get("file", "?"),
                        event.get("line", "?"),
                        "pkg_check_modules() names a module outside the"
                        f" allowlist: {token!r} (GODS_LAWS.md L-07)",
                    )
                )
    return violations


# --- R5: add_subdirectory(), resolved against the calling file's own dir -


def evaluate_r5_add_subdirectory(events, source_root):
    violations = []
    for event in events:
        if event.get("cmd", "").lower() != "add_subdirectory":
            continue
        args = event.get("args", [])
        if not args:
            continue
        target = args[0]
        if not os.path.isabs(target):
            calling_dir = os.path.dirname(event.get("file", ""))
            target = os.path.join(calling_dir, target)
        if not is_under_root(target, source_root):
            violations.append(
                Violation(
                    "R5",
                    event.get("file", "?"),
                    event.get("line", "?"),
                    "add_subdirectory() names a directory outside the"
                    f" source root: {args[0]} (resolves to"
                    f" {os.path.realpath(target)}) (GODS_LAWS.md L-07)",
                )
            )
    return violations


# --- R7: file(DOWNLOAD|UPLOAD) - CMake's own native network transfer -----


def evaluate_r7_file_network(events):
    violations = []
    for event in events:
        if event.get("cmd", "").lower() != "file":
            continue
        args = event.get("args", [])
        if not args:
            continue
        subcommand = args[0].lower()
        if subcommand in FILE_SUBCOMMANDS_FORBIDDEN:
            violations.append(
                Violation(
                    "R7",
                    event.get("file", "?"),
                    event.get("line", "?"),
                    f"file({args[0]} ...) is CMake's own native network"
                    " transfer command, forbidden unconditionally"
                    " (GODS_LAWS.md L-07)",
                )
            )
    return violations


# --- R8: execute_process(COMMAND ...), judged by the program it runs -----


def _execute_process_programs(args):
    """Every program named after a "COMMAND" keyword in an
    execute_process() event's own args - there can be more than one
    (a piped chain: COMMAND a ... COMMAND b ...), and this returns all
    of them, in order.

    Skips EMPTY-STRING tokens immediately after "COMMAND", instead of
    treating the empty string itself as "the program". Measured live
    (29/08/2026, while re-verifying this fix against the real tree):
    CMake's OWN FindPython/Support.cmake (loaded by a plain find_
    package(Python3 ...), no indirection needed to trigger it) traces
    execute_process(COMMAND "" "/usr/bin/python3.14" -c ...) - an empty
    launcher-prefix slot BEFORE the real program, the exact shape
    CMAKE_CROSSCOMPILING_EMULATOR expands to when the variable is
    unset (which it is, on a normal, non-cross-compiling build). This
    is CMake's own internal convention, not an attack: treating the
    empty slot as "a program named ''" produced a real false positive
    against a completely ordinary find_package(Python3) - flagged the
    same day it was written, before ever reaching a reviewer, by
    re-running this fix's own verification against the real tree.
    """
    programs = []
    index = 0
    while index < len(args):
        if args[index] == "COMMAND":
            probe = index + 1
            while probe < len(args) and args[probe] == "":
                probe += 1
            if probe < len(args) and args[probe] != "COMMAND":
                programs.append(args[probe])
        index += 1
    return programs


def _program_basename_casefold(program_path):
    stem = os.path.basename(program_path)
    stem = _EXECUTABLE_SUFFIX_RE.sub("", stem)
    return stem.casefold()


def evaluate_r8_execute_process(events, source_root, build_dir):
    """REVIEW4-DEPZERO-TRACE.md CRITICO #1: basename-only matching
    answers "is this program NAMED pkg-config/pkgconf", never "IS this
    program pkg-config/pkgconf" - reproduced live by the reviewer, a
    script with the literal name "pkg-config" placed at an ARBITRARY
    path passed the old check unconditionally. Fixed the cheap way the
    leader asked for (via the orchestrator): a program whose absolute
    path resolves INSIDE the tree being judged (source_root OR
    build_dir) is never a genuine system tool - pkg-config/pkgconf
    ship with the OS, at a system prefix, never committed to or
    generated inside this project's own tree. This does NOT prove
    identity in the cryptographic sense (a bare, non-absolute program
    name relies on PATH at real execution time, which this script does
    not re-resolve; and an attacker who could rewrite /usr/bin itself
    is a threat model this project does not defend against anywhere)
    - the residual limit is declared in scope_declaration(), not
    silently assumed solved.
    """
    violations = []
    for event in events:
        if event.get("cmd", "").lower() != "execute_process":
            continue
        args = event.get("args", [])
        programs = _execute_process_programs(args)
        if not programs:
            # execute_process() with no "COMMAND" keyword at all is not
            # valid CMake (the command REQUIRES at least one COMMAND
            # clause) - nothing to judge, and CMake's own configure
            # will already have refused the call before this trace
            # event could even represent a running program.
            continue
        for program in programs:
            if _program_basename_casefold(program) not in EXECUTE_PROCESS_PROGRAM_ALLOWLIST:
                violations.append(
                    Violation(
                        "R8",
                        event.get("file", "?"),
                        event.get("line", "?"),
                        "execute_process() runs a program outside the"
                        f" allowlist: {program!r} (GODS_LAWS.md L-07)",
                    )
                )
                continue
            if os.path.isabs(program) and (
                is_under_root(program, source_root) or is_under_root(program, build_dir)
            ):
                violations.append(
                    Violation(
                        "R8-identity",
                        event.get("file", "?"),
                        event.get("line", "?"),
                        "execute_process() runs a program whose NAME is"
                        f" allowlisted ({program!r}) but whose PATH"
                        " resolves INSIDE the tree being judged - a"
                        " genuine system tool never does"
                        " (GODS_LAWS.md L-07)",
                    )
                )
    return violations


# --- R6: the codemodel, as a witness ---------------------------------------


def evaluate_r6_codemodel(codemodel, reply_dir, source_root, build_dir):
    """Returns (violations, link_libraries_witness_note, r9_diagnostics,
    compiled_targets_missing_sources).

    Also runs R9 (REVIEW4-DEPZERO-TRACE.md CRITICO #2, "o mais
    fundamental de toda a familia de quatro rodadas") - see that
    rule's own inline comment below for why it exists and what it
    reads.

    r9_diagnostics (ORDEM DE SERVICO OS-WINDOWS-R9, 29/08/2026, "the
    single most valuable thing you can bring back is the measurement,
    not the guess"): one formatted line PER source entry R9 judged,
    win or lose - not just the ones that turned into a Violation. The
    Windows-only debut this instruments (server run 33253849485) is a
    FALSE NEGATIVE: R9 accepted a source it should have rejected, so
    there is no Violation.message to carry the forensic detail the way
    there would be for a false positive. Every line here carries the
    raw path AS CMAKE REPORTED IT, the resolved path this file computed
    from it, BOTH sides' realpath() output, and which of the two
    accepting branches (source_root/build_dir) fired - so whichever
    component actually differs (short-name form, casing, drive
    spelling, something not yet guessed) is visible without a second
    round of instrument-then-wait.
    """
    violations = []
    r9_diagnostics = []
    # compiled_targets_missing_sources: names of targets whose own
    # codemodel "type" compiles source files (COMPILED_TARGET_TYPES'
    # own comment) but whose "sources" list is empty/absent - the
    # precise signal check_floor_r9_sources_nonempty() reproves on.
    # UTILITY (add_custom_target()) and INTERFACE_LIBRARY targets are
    # NOT compiled types and never land here, on purpose: this file's
    # own find_package/pkg_check_modules/execute_process --selftest
    # fixtures each carry a bare add_custom_target(dummy) precisely so
    # FLOOR(codemodel) has a real target to count, and flagging THEIR
    # zero sources as suspicious would reprove three controls that are
    # genuinely clean.
    compiled_targets_missing_sources = []
    version = codemodel.get("version", {})
    link_libraries_available = version.get("major") == 2 and version.get(
        "minor", 0
    ) >= 9
    if link_libraries_available:
        note = "R6(b) linkLibraries witness: ACTIVE (codemodel v{}.{})".format(
            version.get("major"), version.get("minor")
        )
    else:
        note = (
            "R6(b) linkLibraries witness: DECLARED DOWNGRADE, unavailable"
            " on codemodel v{}.{} (needs >= v2.9 / CMake >= 4.2) -"
            " R1..R5 (trace-based, CMake-version-independent) remain the"
            " primary enforcement; R6(a) paths.source still runs".format(
                version.get("major"), version.get("minor")
            )
        )

    for configuration in codemodel.get("configurations", []):
        for target_ref in configuration.get("targets", []):
            target = read_target(reply_dir, target_ref["jsonFile"])
            target_name = target.get("name", target_ref.get("name", "?"))
            target_type = target.get("type", "?")
            target_sources = target.get("sources", [])
            if target_type in COMPILED_TARGET_TYPES and not target_sources:
                compiled_targets_missing_sources.append(
                    f"{target_name!r} (type={target_type})"
                )
            source_path = target.get("paths", {}).get("source", "")
            resolved_source = os.path.join(source_root, source_path)
            if not is_under_root(resolved_source, source_root):
                violations.append(
                    Violation(
                        "R6a",
                        target_ref.get("jsonFile", "?"),
                        "?",
                        f"target '{target_name}' has paths.source outside"
                        f" the source root: {source_path}"
                        " (GODS_LAWS.md L-07)",
                    )
                )
            # ORDEM DE SERVICO OS-WINDOWS-R9 rodada 2 (29/08/2026): THE
            # ROOT CAUSE, measured, not supposed - reproduced on LINUX,
            # no Windows machine needed. This used to be
            # "if not link_libraries_available: continue" - a `continue`
            # inside THIS SAME `for target_ref` loop, written to skip
            # only the linkLibraries sub-loop immediately below (which
            # genuinely does need codemodel >= v2.9), but R9's own
            # sources sub-loop sits AFTER it in the SAME loop body, so
            # the `continue` skipped R9 too, for every target, whenever
            # link_libraries_available was False. windows-latest's CI
            # pins CMake 4.1.6 (ci.yml's own CI-WIN comment) - measured
            # live in a container, CMake 4.1.6 reports codemodel version
            # {'major': 2, 'minor': 8}, one minor below the >= 2.9 floor
            # link_libraries_available requires. R9 needs NO such floor
            # (codemodel's own "sources" field has existed since v2.0),
            # so gating it on the SAME condition as R6b was the defect:
            # not a Windows toolchain quirk, not a short-name/casing
            # issue (both already hardened, and neither was wrong to
            # fix) - a `continue` reaching one line too far. Confirmed
            # by running THIS FILE's own --selftest under CMake 4.1.6 on
            # Linux: hostile_target_sources_external failed identically
            # to the server's own report (r9_diagnostics=[]) before this
            # fix, and passed after it - the family's fifth instance,
            # closed the same way the other four were: read what
            # actually ran, not what was assumed to run.
            if link_libraries_available:
                for fragment_entry in target.get("linkLibraries", []):
                    fragment = fragment_entry.get("fragment", "")
                    if fragment and fragment not in CODEMODEL_LINK_LIBRARIES_ALLOWLIST:
                        violations.append(
                            Violation(
                                "R6b",
                                target_ref.get("jsonFile", "?"),
                                "?",
                                f"target '{target_name}' links a fragment"
                                f" outside the allowlist: {fragment!r}"
                                " (GODS_LAWS.md L-07)",
                            )
                        )

            # R9 (REVIEW4-DEPZERO-TRACE.md CRITICO #2, the single most
            # fundamental finding of the whole four-round family): NONE
            # of R1..R8, nor R6a/R6b above, ever asked where a target's
            # own SOURCE FILES come from - only the target's own base
            # directory (paths.source) and its linked libraries. A
            # source added by `add_library`/`add_executable`/`target_
            # sources` from an absolute path already sitting somewhere
            # on disk needs no fetch command, no module load, no
            # execute_process(), not even file() - reproduced live by
            # the reviewer: add_library(dummylib STATIC "/var/tmp/.../
            # evil.c") passed EVERY existing rule with "0 violation(s)".
            #
            # Deliberately POSITIVE, not another blocklist entry - the
            # leader's own instruction (relayed by the orchestrator):
            # enumerating commands is a race this family has now lost
            # four times running; the codemodel ALREADY lists every
            # source file explicitly (measured live, 29/08/2026,
            # against the real tree's own glintfx_library target - this
            # comment does not guess the shape), so the fix is to READ
            # what CMake already told us, not add a ninth forbidden
            # spelling to a list a tenth spelling will defeat next
            # round. This closes add_library/add_executable/target_
            # sources AND any future command nobody has invented yet
            # that ends up populating a target's own "sources" list -
            # the SAME "the module denounces itself by having executed"
            # turn this family already won once (R2), applied here to
            # "the target denounces its own sources by having a
            # codemodel entry for them".
            #
            # Resolution rule, measured against the real tree before
            # being written (not assumed): codemodel-v2's own "sources"
            # entries carry a "path" that is RELATIVE to the target's
            # OWN paths.source for ordinary, hand-written sources
            # (measured: "src/core/version.cpp") but ABSOLUTE, already
            # pointing INSIDE the build directory, for GENERATED files
            # (measured: "isGenerated": true, path "<build_dir>/
            # generated/render/gl_functions.cpp" - the exact shape the
            # leader warned to accept: "arquivos gerados durante a
            # construcao... estao fora da arvore-fonte de proposito, e
            # sao legitimos"). Both shapes are handled by the SAME
            # resolve-then-check: absolute paths are used as-is,
            # relative ones are joined onto the target's own base
            # directory (resolved_source, already computed above for
            # R6a) - then accepted if the result sits under source_root
            # OR under build_dir, exactly the same is_under_root() this
            # whole file already uses everywhere else.
            for source_entry in target_sources:
                raw_path = source_entry.get("path", "")
                if not raw_path:
                    continue
                if os.path.isabs(raw_path):
                    resolved_src = raw_path
                else:
                    resolved_src = os.path.join(resolved_source, raw_path)
                under_source_root = is_under_root(resolved_src, source_root)
                under_build_dir = is_under_root(resolved_src, build_dir)
                r9_diagnostics.append(
                    f"R9 target={target_name!r} raw_path={raw_path!r}"
                    f" resolved_src={resolved_src!r}"
                    f" realpath(resolved_src)={os.path.realpath(resolved_src)!r}"
                    f" realpath(source_root)={os.path.realpath(source_root)!r}"
                    f" realpath(build_dir)={os.path.realpath(build_dir)!r}"
                    f" under_source_root={under_source_root}"
                    f" under_build_dir={under_build_dir}"
                )
                if under_source_root or under_build_dir:
                    continue
                violations.append(
                    Violation(
                        "R9",
                        target_ref.get("jsonFile", "?"),
                        "?",
                        f"target '{target_name}' has a source file"
                        " outside BOTH the source root and the build"
                        f" directory: {raw_path} (resolves to"
                        f" {os.path.realpath(resolved_src)})"
                        " (GODS_LAWS.md L-07)",
                    )
                )
    return violations, note, r9_diagnostics, compiled_targets_missing_sources


# --- the four floor checks (GODS_LAWS.md L-40) ------------------------------


def check_floor_parity(trace_lines, events, invalid_count):
    expected = max(len(trace_lines) - 1, 0)  # minus the {"version": ...} header
    parsed = len(events)
    if invalid_count > 0 or parsed != expected - invalid_count:
        return (
            False,
            f"FLOOR(parity): parser could not use {invalid_count} line(s)"
            f" out of {expected} trace line(s) (not valid JSON, or valid"
            ' JSON missing "file"/"cmd" - REVIEW3-DEPZERO-TRACE.md'
            " CRITICO #4) - a parser that loses or misreads lines"
            " silently is the exact defect GODS_LAWS.md L-40 forbids",
        )
    return True, f"FLOOR(parity): {parsed}/{expected} trace line(s) parsed"


def check_floor_nonempty_repo_scan(events, source_root):
    repo_events = [
        e for e in events if is_under_root(e.get("file", ""), source_root)
    ]
    if not repo_events:
        return (
            False,
            "FLOOR(repo-scan): 0 trace events sourced from the repository"
            " - empty scan (GODS_LAWS.md L-40)",
        )
    return (
        True,
        f"FLOOR(repo-scan): {len(repo_events)} trace event(s) sourced from"
        " the repository",
    )


def same_path_platform_aware(path_a, path_b):
    """True if two ALREADY-RESOLVED paths name the same file, tolerant
    of case on a case-insensitive-but-case-PRESERVING filesystem
    (Windows/NTFS).

    REVIEW3-DEPZERO-TRACE.md, item 8 ("IMPORTANTE... nao confirmado nem
    refutado"): the reviewer could not run this on a real Windows
    machine, and neither can this comment - the finding is declared as
    a hypothesis there, and stays one here: os.path.normcase() is
    Python's OWN documented, platform-aware answer to "normalize a
    path for comparison" (a no-op on POSIX, case-folding plus
    separator normalization on Windows) - the exact tool the standard
    library ships for this exact problem, not a bespoke workaround.
    Extracted as its own function so it is unit-testable on ANY
    platform without needing two real filesystem entries that differ
    only by case (which POSIX, being case-sensitive, cannot even
    create as "the same file" to test against) - see this file's own
    --selftest for what IS and is NOT provable from Linux.
    """
    return os.path.normcase(path_a) == os.path.normcase(path_b)


def check_floor_sentinel(events, source_root, require_pkgconfig_sentinel):
    root_cmakelists = os.path.realpath(
        os.path.join(source_root, "CMakeLists.txt")
    )
    project_seen = any(
        e.get("cmd", "").lower() == "project"
        and same_path_platform_aware(os.path.realpath(e.get("file", "")), root_cmakelists)
        for e in events
    )
    if not project_seen:
        return (
            False,
            "FLOOR(sentinel): the root CMakeLists.txt's own project() call"
            " never appeared in the trace - the configure did not really"
            " run against this tree, or the trace was truncated"
            " (GODS_LAWS.md L-40)",
        )
    if not require_pkgconfig_sentinel:
        return True, "FLOOR(sentinel): root project() seen"
    pkgconfig_seen = any(
        e.get("cmd", "").lower() == "find_package"
        and e.get("args", [None])[0:1] == ["PkgConfig"]
        for e in events
    )
    if not pkgconfig_seen:
        return (
            False,
            "FLOOR(sentinel): root project() seen, but the second"
            " (non-Windows) sentinel - find_package(PkgConfig) - never"
            " appeared (GODS_LAWS.md L-40)",
        )
    return True, "FLOOR(sentinel): root project() and find_package(PkgConfig) seen"


def check_floor_codemodel_nonempty(codemodel, codemodel_error):
    if codemodel_error is not None:
        return False, f"FLOOR(codemodel): {codemodel_error} (GODS_LAWS.md L-40)"
    target_count = sum(
        len(c.get("targets", [])) for c in codemodel.get("configurations", [])
    )
    if target_count == 0:
        return (
            False,
            "FLOOR(codemodel): codemodel resolved with 0 targets"
            " (GODS_LAWS.md L-40)",
        )
    return True, f"FLOOR(codemodel): {target_count} target(s) resolved"


def check_floor_r9_sources_nonempty(compiled_targets_missing_sources):
    """GODS_LAWS.md L-40, the piso item 2 of ORDEM DE SERVICO OS-
    WINDOWS-R9 rodada 2 demands, on TOP of check_floor_codemodel_
    nonempty above, not instead of it: "codemodel resolved with >= 1
    target" (the existing floor) does NOT imply "R9 examined >= 1
    source" for a target that OUGHT to have one - the codemodel-v2
    target object's own "sources" field is documented as OMITTED
    ENTIRELY, not present-and-empty, "when the target has no sources
    that compile" (cmake-file-api(7)), so a compiled-type target whose
    ONE source silently failed to be recognized produces EXACTLY the
    same "0 violation(s)" this project's own dep_zero_trace prints for
    a genuinely clean tree. This is the fifth instance of the SAME
    family this fatia has already found and fixed four times (a
    sanitizer that printed its own error and exited 0; a selftest
    aggregator that erased its own red; two controls that passed on an
    empty scan; a fixture file that was never even read) - "did not
    detect" and "did not look" produce the identical "0 violation(s)"
    from the outside, and only a piso that reproves the SECOND shape
    closes the gap the first four fixes each closed for their own
    mechanism.

    Judges compiled_targets_missing_sources (evaluate_r6_codemodel's
    own per-target COMPILED_TARGET_TYPES check), NOT a bare "was
    r9_diagnostics empty" - a bare emptiness check reproved three of
    this file's OWN --selftest controls the first time it ran
    (positive_find_package_multiline and siblings, each a bare
    add_custom_target(dummy) with genuinely zero sources BY
    CONSTRUCTION, not by defect - COMPILED_TARGET_TYPES's own comment).
    An empty compiled_targets_missing_sources means either "every
    compiled-type target had sources" (the common case) or "there were
    no compiled-type targets to examine at all" (a UTILITY/INTERFACE-
    only fixture) - both are genuinely clean, and neither should reprove.

    Deliberately UNCONDITIONAL on platform: this floor runs wherever
    evaluate_r6_codemodel() runs - the REAL dep_zero_trace check
    against this project's own glintfx_library target (a STATIC_LIBRARY/
    SHARED_LIBRARY with real, numerous .cpp sources on every currently-
    green platform) included. If it ever reproves there, that is not a
    false alarm to silence; it is this exact silent gap having reached
    the one target the whole mechanism exists to protect - GODS_LAWS.md
    L-40's own reason to exist ("contou zero, reprova"), not a downgrade.
    """
    if compiled_targets_missing_sources:
        return (
            False,
            "FLOOR(r9-sources): compiled-type target(s) reported ZERO"
            " source file entries: "
            f"{compiled_targets_missing_sources} - a target of type"
            " EXECUTABLE/STATIC_LIBRARY/SHARED_LIBRARY/MODULE_LIBRARY/"
            "OBJECT_LIBRARY should always report at least one. Zero is"
            " not proof the tree is clean, it is proof nobody looked"
            " (GODS_LAWS.md L-40)",
        )
    return True, "FLOOR(r9-sources): every compiled-type target reported >= 1 source"


# --- the whole judgement, assembled -----------------------------------------


class Report:
    def __init__(self):
        self.violations = []
        self.floor_lines = []
        self.floor_ok = True
        self.notes = []
        # r9_diagnostics: forensic detail for EVERY source R9 judged
        # (evaluate_r6_codemodel's own docstring explains why) - never
        # printed by real_main()'s own 0-violation path (the real tree
        # has many more sources than a selftest fixture; printing every
        # one would bury the signal), read directly by --selftest's own
        # hostile_target_sources_external control instead.
        self.r9_diagnostics = []
        # reconfigure_result: the CompletedProcess of the SECOND cmake
        # invocation (run_configure_with_trace's own, --trace-expand,
        # the one that actually produces the trace/codemodel this
        # Report judges) - set by configure_and_judge()/real_main()'s
        # own caller, never by judge() itself (judge() only reads
        # whatever trace/reply already exists on disk, it does not run
        # cmake). ORDEM DE SERVICO OS-WINDOWS-R9 rodada 2 (29/08/2026):
        # this result used to be silently discarded in
        # configure_and_judge() - real_main() already captured and
        # printed it on the violations/floor-failure path, but the
        # --selftest helper never did, which is exactly why
        # hostile_target_sources_external's own FAILED line carried no
        # evidence about what the SECOND configure actually saw.
        self.reconfigure_result = None


def judge(source_root, build_dir, trace_path, require_pkgconfig_sentinel):
    report = Report()

    trace_lines = read_trace_lines(trace_path)
    events, invalid_count = parse_trace_events(trace_lines)

    parity_ok, parity_line = check_floor_parity(trace_lines, events, invalid_count)
    report.floor_lines.append(parity_line)
    report.floor_ok = report.floor_ok and parity_ok

    scan_ok, scan_line = check_floor_nonempty_repo_scan(events, source_root)
    report.floor_lines.append(scan_line)
    report.floor_ok = report.floor_ok and scan_ok

    sentinel_ok, sentinel_line = check_floor_sentinel(
        events, source_root, require_pkgconfig_sentinel
    )
    report.floor_lines.append(sentinel_line)
    report.floor_ok = report.floor_ok and sentinel_ok

    codemodel, codemodel_error = read_codemodel_reply(build_dir)
    codemodel_ok, codemodel_line = check_floor_codemodel_nonempty(
        codemodel or {}, codemodel_error
    )
    report.floor_lines.append(codemodel_line)
    report.floor_ok = report.floor_ok and codemodel_ok

    # R1..R5 need a usable trace; R6 needs a usable codemodel. Each is
    # judged independently of the OTHER's floor failing, so a broken
    # codemodel query does not hide a real R1..R5 finding, and vice
    # versa - but violations found against an UNUSABLE oracle would be
    # noise, so this guards each rule group on its OWN floor result.
    if parity_ok and scan_ok and sentinel_ok:
        report.violations.extend(evaluate_r1_forbidden_commands(events))
        report.violations.extend(evaluate_r2_forbidden_modules(events))
        report.violations.extend(evaluate_r3_find_package(events, source_root))
        report.violations.extend(evaluate_r4_pkg_check_modules(events))
        report.violations.extend(evaluate_r5_add_subdirectory(events, source_root))
        report.violations.extend(evaluate_r7_file_network(events))
        report.violations.extend(evaluate_r8_execute_process(events, source_root, build_dir))

    if codemodel_ok:
        reply_dir = os.path.join(build_dir, ".cmake", "api", "v1", "reply")
        (
            r6_violations,
            r6_note,
            r9_diagnostics,
            compiled_targets_missing_sources,
        ) = evaluate_r6_codemodel(codemodel, reply_dir, source_root, build_dir)
        report.violations.extend(r6_violations)
        report.notes.append(r6_note)
        report.r9_diagnostics.extend(r9_diagnostics)

        # FLOOR(r9-sources), GODS_LAWS.md L-40 (ORDEM DE SERVICO
        # OS-WINDOWS-R9 rodada 2, item 2 - "vale mesmo que o item 1 se
        # resolva"): check_floor_codemodel_nonempty above only proves
        # "the codemodel has >= 1 target"; it says nothing about
        # whether that target's own sources were ever reported. See
        # check_floor_r9_sources_nonempty()'s own docstring for why
        # this is a DIFFERENT floor, not a duplicate.
        r9_sources_ok, r9_sources_line = check_floor_r9_sources_nonempty(
            compiled_targets_missing_sources
        )
        report.floor_lines.append(r9_sources_line)
        report.floor_ok = report.floor_ok and r9_sources_ok

    return report


# --- real mode --------------------------------------------------------------


def scope_declaration():
    """What "0 violation(s)" does and does NOT promise.

    REVIEW3-DEPZERO-TRACE.md CRITICO #3: the PREVIOUS wording of this
    declaration ("untaken branches are covered... by the shallow net")
    was TRUE for R1..R6's own vocabulary but SILENTLY FALSE for
    file(DOWNLOAD|UPLOAD)/execute_process(). Fixed once already; this
    round (REVIEW4-DEPZERO-TRACE.md item 6) found the SAME defect
    class repeated, now against the string's COMPLETENESS rather than
    its truth: it never named the three vectors that round found -
    add_library()/add_executable()/target_sources() with an external
    source (CRITICO #2, now closed by R9 - see evaluate_r6_codemodel's
    own comment), file(COPY)/file(ARCHIVE_EXTRACT) (CRITICO #3,
    genuinely still open), and R8's basename-vs-identity gap (CRITICO
    #1, now partially closed - see evaluate_r8_execute_process's own
    comment). "A true but incomplete declaration" has now happened
    three times in this family (round 2's shallow-net "out of scope"
    line, round 3's shallow-net promise, round 4's own scope string) -
    the fix each time is the same discipline: say ONLY what has been
    proven, and name every gap that was found, not just the first one.
    """
    return (
        "this oracle judges the executed configuration. Untaken"
        " branches naming FetchContent/ExternalProject/CPM/"
        "find_package/pkg_check_modules/add_subdirectory are covered"
        " as literal text by the shallow net regardless of the branch"
        " being taken, and per-platform by the CI matrix. Untaken"
        " branches naming file(DOWNLOAD|UPLOAD) or execute_process()"
        " are NOT covered by either oracle today. R9 (codemodel"
        " sources) covers add_library/add_executable/target_sources"
        " with an external source file in ANY configuration (it reads"
        " the resolved target graph, not text, so a taken/untaken"
        " branch makes no difference to it) - but file(COPY ...)/"
        " file(ARCHIVE_EXTRACT ...) that copy or extract an external"
        " file INTO the build directory, which a target then compiles"
        " as a source, are NOT caught by R9 either: once the file sits"
        " under build_dir it is indistinguishable from a genuinely"
        " generated one - a narrower gap than before this fix (a"
        " copied-but-never-compiled file was never a risk, and still"
        " is not), but a real, open one. execute_process()'s allowlist"
        " (R8) now also rejects an allowlisted NAME resolving to an"
        " ABSOLUTE path inside the tree being judged, but does not"
        " (and cannot, from a static trace read) verify the identity"
        " of a program resolved by bare name via PATH at real"
        " execution time, nor defend against the system path itself"
        " (e.g. /usr/bin) being compromised - REVIEW4-DEPZERO-TRACE.md"
        " CRITICOS #1/#2/#3"
    )


def trust_boundary_declaration():
    """REVIEW3-DEPZERO-TRACE.md item 4 (IMPORTANTE): this script RUNS
    the CMake configure of whatever <source-root>/<build-dir> it is
    given, with the full process authority that implies - R7/R8 exist
    precisely because a CMake configure can execute an arbitrary
    program or download from an arbitrary URL. There is no identity
    check on source_root (project name, GODS_LAWS.md presence, git
    remote, expected commit) - low risk TODAY because the only real
    caller (tests/CMakeLists.txt's dep_zero_trace/dep_zero_trace_
    selftest) passes fixed argv, never argv controlled by a third
    party, but that is a property of THIS project's own CI wiring, not
    of this script, and the reviewer's own reproduction
    (/var/tmp/depzero-review3/foreign-tree) confirms a foreign,
    unrelated CMake project is judged with no complaint about
    identity. This declaration exists so nobody copies this script
    into a context where argv IS attacker-influenced without reading
    this paragraph first.

    REVIEW4-DEPZERO-TRACE.md item 4 (IMPORTANTE, not independently
    reproduced by the reviewer - "risco identificado por leitura de
    codigo, nao... demonstrado"): clear_stale_codemodel_reply_pointers()
    plus the reconfigure it precedes are not atomic against a SECOND,
    concurrent invocation of this script (or a developer's own manual
    `cmake --build`) against the SAME build_dir - RUN_SERIAL (tests/
    CMakeLists.txt) only serializes cases within one ctest run, never
    across two separate ones. Declared here rather than silently
    assumed away; not fixed in this round (the leader's own scope for
    this pass named three CRITICOs and two IMPORTANTEs, and this was
    not one of the ones asked for).
    """
    return (
        "this script RUNS the CMake configure of the tree it is"
        " pointed at (that is how it observes what CMake executes) -"
        " it must never be invoked with a source-root/build-dir pair"
        " that did not come from a trusted, fixed call site"
        " (tests/CMakeLists.txt's own dep_zero_trace registration is"
        " the only trusted caller today; there is no identity check"
        " in this script itself). It also assumes exclusive access to"
        " build_dir: nothing here serializes against a SECOND,"
        " concurrent invocation of this script or a manual build"
        " against the same directory - RUN_SERIAL only protects"
        " within a single ctest run"
    )


def platform_requires_pkgconfig_sentinel():
    return not sys.platform.startswith("win")


def real_main(argv):
    if len(argv) != 2:
        fail("usage: check_dep_zero_trace.py <source-root> <build-dir>")
    source_root, build_dir = argv
    if not os.path.isdir(source_root):
        fail(f"source directory not found: {source_root}")
    if not os.path.isdir(build_dir):
        fail(f"build directory not found: {build_dir}")

    # A hand written Unix path does not exist on every platform (Windows
    # has no /tmp). dir=os.environ.get("TMPDIR") with no hardcoded
    # fallback lets tempfile.mkdtemp fall through to gettempdir(), which
    # already checks TMPDIR/TEMP/TMP and then the platform default.
    scratch = tempfile.mkdtemp(
        prefix="glintfx-dep-zero-trace-", dir=os.environ.get("TMPDIR")
    )
    try:
        trace_path = os.path.join(scratch, "trace.json")
        configure_result = run_configure_with_trace(source_root, build_dir, trace_path)
        report = judge(
            source_root,
            build_dir,
            trace_path,
            platform_requires_pkgconfig_sentinel(),
        )
    finally:
        # Higiene (plan section 3.6): the trace with --trace-expand can
        # carry expanded environment/cache values - it lives only in
        # this scratch dir, under this job's own workspace, and is
        # deleted here regardless of verdict. It is never an artifact.
        shutil.rmtree(scratch, ignore_errors=True)

    for line in report.floor_lines:
        print(f"{SCRIPT_NAME}: {line}")
    for note in report.notes:
        print(f"{SCRIPT_NAME}: {note}")
    # Printed on BOTH verdicts (GODS_LAWS.md L-40, same reasoning as the
    # floor lines above: a declaration read only on the green path is a
    # declaration nobody reads on the day it would matter most).
    print(f"{SCRIPT_NAME}: {scope_declaration()}")
    print(f"{SCRIPT_NAME}: {trust_boundary_declaration()}")

    if report.violations or not report.floor_ok:
        print(
            f"{SCRIPT_NAME}: PROIBIDO (GODS_LAWS.md L-07 dependencia zero):",
            file=sys.stderr,
        )
        for violation in report.violations:
            print(violation.format(), file=sys.stderr)
        if configure_result.returncode != 0:
            print(
                f"{SCRIPT_NAME}: note - the reconfigure itself also exited"
                f" {configure_result.returncode}; stderr follows",
                file=sys.stderr,
            )
            print(configure_result.stderr, file=sys.stderr)
        sys.exit(1)

    print(f"{SCRIPT_NAME}: 0 violation(s)")


# --- --selftest: fixtures and controls --------------------------------------


def make_scratch_workdir():
    # A hand written Unix path does not exist on every platform (Windows
    # has no /tmp). dir=os.environ.get("TMPDIR") with no hardcoded
    # fallback lets tempfile.mkdtemp fall through to gettempdir(), which
    # already checks TMPDIR/TEMP/TMP and then the platform default.
    return tempfile.mkdtemp(
        prefix="glintfx-dep-zero-trace-selftest-", dir=os.environ.get("TMPDIR")
    )


def cmake_path_literal(path):
    """A filesystem path, safe to interpolate as CMake TEXT (never a
    subprocess argv element - those never go through a parser at all,
    backslash or not).

    REVIEW4-DEPZERO-TRACE.md / server debut round 4 (29/08/2026, GHA
    run 33252888557): selftest_hostile_target_sources_external wrote
    an absolute Windows path (os.path.join()'s own native separator)
    straight into a CMakeLists.txt string literal -
    add_library(dummylib STATIC "C:...evil.c") with backslashes where
    the dots are - and CMake's own string-literal grammar treats a
    backslash as the START of an escape sequence, not a separator: the
    two-character sequence backslash-then-capital-U is not a
    recognized escape, so CMake refused to even PARSE the file
    ("Invalid character escape") before any rule ever ran. The failure
    looked like "R9 did not detect the violation" (violations=[]) - it
    was actually "the fixture never configured at all", the exact "no
    buildsystem generated" shape this whole family has chased three
    times already, now from a FOURTH mechanism (backslash-vs-CMake-
    string-escape, not REQUIRED/generator/found-tool). CMake's own
    parser accepts forward slashes unconditionally on every platform
    including Windows - its own canonical form (cmake-language(7):
    "It is common on Windows to also accept forward slashes"; CMake
    normalizes to forward slashes internally throughout). This
    function is called at EVERY point a fixture interpolates a Python
    path into CMake TEXT, so a Windows-separated path (the only one
    os.path.join() ever produces there) never reaches a CMakeLists.txt
    string literal unconverted again.

    NOTE ON THIS DOCSTRING ITSELF (found while writing it, GODS_LAWS.md
    L-27 - a fact too on-the-nose not to record): an earlier draft of
    this exact paragraph spelled the invalid escape out literally
    inside this docstring's own text - which made THIS FILE fail to
    py_compile with the SAME "truncated escape" class of error the
    docstring was describing. Left un-spelled here on purpose, the
    same reason the fixtures themselves need this function instead of
    a raw path: text a parser reads is not inert, regardless of which
    parser, Python's or CMake's.
    """
    return path.replace("\\", "/")


def write_fixture(root, cmakelists_body):
    os.makedirs(root, exist_ok=True)
    with open(os.path.join(root, "CMakeLists.txt"), "w", encoding="utf-8") as handle:
        handle.write(cmakelists_body)


def configure_fixture_with_ninja(source_root, build_dir):
    """The INITIAL, fresh configure every --selftest fixture needs
    before configure_and_judge()'s own SECOND, trace-generating
    reconfigure can do anything useful (that second call, through
    run_configure_with_trace(), deliberately never passes -G: it must
    REUSE whatever generator is already cached, exactly like the REAL
    production call against the actual glintfx build/ does - forcing a
    generator there would hit the exact "does not match the generator
    used previously" error this project's own tests/CMakeLists.txt
    already documents for embed_dll_colocation_test).

    -G Ninja explicit here, unlike the bare `cmake -S -B` this used to
    be: CAUSA 2 of the server debut (29/08/2026, GitHub Actions run
    33248706044, branch depzero-gate) - on windows-latest, a bare
    `cmake -S -B` (no -G) in the plain pwsh shell this ctest step runs
    in fell through to a generator this project's OWN ci.yml already
    diagnosed and fixed once before, for the SAME reason (see
    ci.yml's own CI-WIN-GEN comment: "sem -G explicito, o CMake...
    cai para NMake Makefiles... que exige vcvarsall" - a plain pwsh is
    not that). The REAL project's own outer build already proved Ninja
    works in this exact job (the "Build" step passes -G Ninja and
    succeeds before ctest ever runs) - reusing that SAME choice here,
    not inventing a new one, is what makes this fix low-risk: Ninja is
    already a REQUIRED tool on all five platforms in this project's own
    CI matrix (installed explicitly on the four Linux jobs; downloaded/
    available on Windows already, proven by that same outer build
    step), so there is no new dependency introduced by naming it here.
    """
    os.makedirs(build_dir, exist_ok=True)
    return subprocess.run(
        ["cmake", "-S", source_root, "-B", build_dir, "-G", "Ninja"],
        capture_output=True,
    )


def initial_configure_diagnostic(result):
    """DEPZERO-TRACE, achado da segunda estreia (29/08/2026, GHA run
    33249693240, branch depzero-gate): four --selftest controls on
    windows-latest failed with "FLOOR(codemodel): ...no buildsystem
    generated", and this file never printed why - every fixture
    function discarded configure_fixture_with_ninja()'s own stdout/
    stderr. This function surfaces it instead.

    RESOLVED (GHA run 33251519907, third estreia): the real cause,
    read straight from this instrumentation's own output in the CI
    log - windows-latest genuinely has NO pkg-config at all
    (`CMake Error ... Could NOT find PkgConfig (missing:
    PKG_CONFIG_EXECUTABLE)`). REQUIRED on find_package(PkgConfig ...)
    turns "not found" into a FATAL_ERROR that stops the configure
    before ANYTHING after it - including the very statement (pkg_
    check_modules(), execute_process(), add_library()) each fixture
    exists to exercise - ever gets a chance to be TRACED at all.
    Confirmed live (container, pkg-config genuinely uninstalled,
    matching the runner exactly): a bare find_package(PkgConfig)
    WITHOUT REQUIRED still reports "Could NOT find PkgConfig" and
    still lets the configure CONTINUE - and the very next statement
    (pkg_check_modules(), tested directly) is traced with its real,
    expanded args before IT ALSO fails on its own internal REQUIRED -
    proving the fix (drop REQUIRED from find_package(PkgConfig...) in
    fixtures ONLY, never in the real project's own CMakeLists.txt,
    where REQUIRED is genuine protection) preserves exactly what R3/
    R4/R8 need: the trace event, not the tool actually being found.

    The "NOT find_package(PkgConfig REQUIRED) always fails" hypothesis
    above was only HALF wrong: selftest_positive_execute_process_
    pkgconfig did call it identically and did report OK - but for the
    WRONG reason. Its own assertion (`not r8_violations`) is a
    NEGATIVE check that passes VACUOUSLY when R8 never runs at all
    (gated on the SAME fatal find_package failure, one statement
    earlier) - reproduced live in the identical hidden-pkg-config
    container: report.violations comes back empty (nothing to flag,
    correctly) while report.floor_ok is False (codemodel never
    formed) - a false pass this file's own assertion never checked.
    selftest_positive_target_sources_generated_in_build_dir had the
    identical latent bug (`not r9_violations`, also gated on the same
    failure one statement earlier, also never checked floor_ok) -
    caught by this same audit, not by a fourth guess.
    """
    return f" initial_configure(rc={result.returncode}, stderr={result.stderr!r})"


def reconfigure_diagnostic(report):
    """The SECOND configure's own rc/stderr - the one run_configure_
    with_trace() runs, that actually produces the trace and codemodel
    reply judge() reads. ORDEM DE SERVICO OS-WINDOWS-R9 rodada 2
    (29/08/2026): this result used to be silently discarded inside
    configure_and_judge() - real_main()'s own violations/floor-failure
    path already prints it (this exact line's own twin, a few hundred
    lines below in real_main()), but no --selftest control ever saw
    it. A hostile fixture whose SECOND configure genuinely fails or
    warns (a missing-source warning, for instance - CMake does not
    treat "cannot verify a source file's existence" as fatal, and the
    codemodel's own "sources" field is documented as OMITTED, not
    zero-length-and-present, "when the target has no sources that
    compile") would previously report "violations=[]" with nothing
    else to go on - indistinguishable, from this file's own selftest
    output, from is_under_root() actually mis-judging a real entry.
    Report.reconfigure_result is None for any caller that never set it
    (only configure_and_judge() does), so this stays a safe no-op
    everywhere else.
    """
    result = report.reconfigure_result
    if result is None:
        return " reconfigure=<not captured>"
    return f" reconfigure(rc={result.returncode}, stderr={result.stderr!r})"


def configure_and_judge(source_root, build_dir):
    """The same pipeline real_main() uses, for a --selftest fixture.

    require_pkgconfig_sentinel is always False here, deliberately NOT
    platform_requires_pkgconfig_sentinel(): that second sentinel is
    specific to the REAL glintfx tree (whose root CMakeLists.txt calls
    find_package(PkgConfig) inside if(UNIX) unconditionally - see the
    FLOOR(sentinel) header comment). A --selftest fixture is an
    arbitrary, disposable mini-project; most of the fixtures below have
    no reason to touch PkgConfig at all, and holding them to a
    requirement that is true of glintfx but not of them would be
    testing this file's OWN assumption, not the rule under test - this
    was caught live (29/08/2026): four hostile fixtures reported
    violations=[] not because R1/R2/R5 missed anything, but because
    FLOOR(sentinel) failed FIRST on an unrelated requirement and gated
    R1..R5 off entirely (see judge()'s own comment on why rule
    evaluation is gated on the trace floors).
    """
    scratch = make_scratch_workdir()
    try:
        trace_path = os.path.join(scratch, "trace.json")
        # ORDEM DE SERVICO OS-WINDOWS-R9 rodada 2: this result used to
        # be discarded (the bare call below, no assignment) - captured
        # now and attached to the Report so every --selftest control
        # has the SAME evidence real_main()'s own violations-path
        # already prints (Report.__init__'s own comment explains why).
        reconfigure_result = run_configure_with_trace(source_root, build_dir, trace_path)
        report = judge(source_root, build_dir, trace_path, False)
        report.reconfigure_result = reconfigure_result
        return report
    finally:
        shutil.rmtree(scratch, ignore_errors=True)


def selftest_report(name, condition, detail=""):
    if condition:
        print(f"selftest: {name} OK")
        return True
    print(f"selftest: {name} FAILED {detail}", file=sys.stderr)
    return False


# Five hostile forms (GODS_LAWS.md house rule of 28/08/2026, "portao
# que nunca reprovou nao e portao" - and the ORDEM DE SERVICO's own "A
# PROVA DE MORDIDA" section): each is the exact shape that escaped one
# of the two adversarial reviews of check_dep_zero.sh
# (/var/tmp/glintfx-plan/REVIEW-DEPZERO-GATE.md, REVIEW2-...md).


def selftest_hostile_multiline_and_literal(scratch):
    root = os.path.join(scratch, "hostile_multiline")
    write_fixture(
        root,
        """cmake_minimum_required(VERSION 3.25)
project(hostile_multiline NONE)
include(FetchContent)
message("a multi-line (
  string with a stray paren inside it, the exact shape that blinded
  check_dep_zero.sh to the rest of the file (REVIEW-DEPZERO-GATE.md)
")
FetchContent_Declare(evil GIT_REPOSITORY https://example.invalid/evil.git)
""",
    )
    build_dir = os.path.join(root, "build")
    configure_fixture_with_ninja(root, build_dir)
    report = configure_and_judge(root, build_dir)
    rules = {v.rule for v in report.violations}
    ok = bool(report.violations) and ("R1" in rules or "R2" in rules)
    cited = any(
        v.file.endswith("CMakeLists.txt") or v.file.endswith("FetchContent.cmake")
        for v in report.violations
    )
    return selftest_report(
        "hostile: multi-line string + literal FetchContent_Declare",
        ok and cited,
        detail=f"violations={[v.format() for v in report.violations]}",
    )


def selftest_hostile_indirection(scratch):
    root = os.path.join(scratch, "hostile_indirection")
    write_fixture(
        root,
        """cmake_minimum_required(VERSION 3.25)
project(hostile_indirection NONE)
set(mod_name "FetchContent")
include(${mod_name})
set(fn_name "FetchContent_Declare")
cmake_language(CALL ${fn_name} fmt GIT_REPOSITORY https://example.invalid/fmt.git)
""",
    )
    build_dir = os.path.join(root, "build")
    configure_fixture_with_ninja(root, build_dir)
    report = configure_and_judge(root, build_dir)
    rules = {v.rule for v in report.violations}
    ok = "R1" in rules and "R2" in rules  # BOTH: the resolved call AND the module load
    return selftest_report(
        "hostile: set(mod_name) + include(${mod_name}) + cmake_language(CALL ${fn})",
        ok,
        detail=f"violations={[v.format() for v in report.violations]}",
    )


def selftest_hostile_cpm_sibling(scratch):
    root = os.path.join(scratch, "hostile_cpm")
    write_fixture(
        root,
        """cmake_minimum_required(VERSION 3.25)
project(hostile_cpm NONE)
# Stands in for a project that vendored CPM.cmake elsewhere (that act
# is ALREADY caught by R1/R2 above); this fixture proves R1 covers all
# four of CPM's public entry points, not only CPMAddPackage - the
# escape REVIEW-DEPZERO-GATE.md CRITICO #3 named ("a API publica do
# CPM.cmake tem no minimo quatro entradas").
function(CPMFindPackage)
endfunction()
CPMFindPackage(NAME fmt GIT_REPOSITORY https://example.invalid/fmt.git GIT_TAG 1.0)
""",
    )
    build_dir = os.path.join(root, "build")
    configure_fixture_with_ninja(root, build_dir)
    report = configure_and_judge(root, build_dir)
    ok = any(v.rule == "R1" and "cpmfindpackage" in v.message for v in report.violations)
    return selftest_report(
        "hostile: CPMFindPackage (the sibling REVIEW-DEPZERO-GATE.md found missing)",
        ok,
        detail=f"violations={[v.format() for v in report.violations]}",
    )


def selftest_hostile_pkg_check_modules_digit_name(scratch):
    root = os.path.join(scratch, "hostile_pkgcheck")
    write_fixture(
        root,
        """cmake_minimum_required(VERSION 3.25)
project(hostile_pkgcheck NONE)
find_package(PkgConfig)
pkg_check_modules(Fixture REQUIRED 7zip)
""",
    )
    build_dir = os.path.join(root, "build")
    initial = configure_fixture_with_ninja(root, build_dir)
    report = configure_and_judge(root, build_dir)
    ok = any(
        v.rule == "R4" and "7zip" in v.message for v in report.violations
    )
    return selftest_report(
        "hostile: pkg_check_modules(... 7zip) (digit-leading module name)",
        ok,
        detail=f"violations={[v.format() for v in report.violations]}"
        f"{initial_configure_diagnostic(initial)}",
    )


def selftest_hostile_add_subdirectory_outside_tree(scratch):
    outside = os.path.join(scratch, "outside_the_tree")
    write_fixture(outside, "cmake_minimum_required(VERSION 3.25)\nproject(outsider NONE)\n")
    root = os.path.join(scratch, "hostile_add_subdirectory")
    write_fixture(
        root,
        f"""cmake_minimum_required(VERSION 3.25)
project(hostile_add_subdirectory NONE)
add_subdirectory("{cmake_path_literal(outside)}" outsider-build)
""",
    )
    build_dir = os.path.join(root, "build")
    configure_fixture_with_ninja(root, build_dir)
    report = configure_and_judge(root, build_dir)
    ok = any(v.rule == "R5" for v in report.violations)
    return selftest_report(
        "hostile: add_subdirectory() pointing outside the source root",
        ok,
        detail=f"violations={[v.format() for v in report.violations]}",
    )


def selftest_hostile_target_sources_external(scratch):
    """REVIEW4-DEPZERO-TRACE.md CRITICO #2, the most fundamental
    finding of the whole four-round family: add_library()/add_
    executable()/target_sources() with a source file at an absolute
    path OUTSIDE the tree, no fetch/module/execute_process/file()
    involved at all - the most ordinary way to add a file to a target.
    """
    vendor_dir = os.path.join(scratch, "preexisting_vendor_r9")
    os.makedirs(vendor_dir, exist_ok=True)
    evil_c = os.path.join(vendor_dir, "evil.c")
    with open(evil_c, "w", encoding="utf-8") as handle:
        handle.write("int vendored_evil(void) { return 1; }\n")

    root = os.path.join(scratch, "hostile_target_sources_external")
    write_fixture(
        root,
        f"""cmake_minimum_required(VERSION 3.25)
project(hostile_target_sources_external C)
add_library(dummylib STATIC "{cmake_path_literal(evil_c)}")
""",
    )
    build_dir = os.path.join(root, "build")
    initial = configure_fixture_with_ninja(root, build_dir)
    report = configure_and_judge(root, build_dir)
    ok = any(v.rule == "R9" and "evil.c" in v.message for v in report.violations)
    # ORDEM DE SERVICO OS-WINDOWS-R9 (29/08/2026): this control's own
    # FAILURE shape on Windows is a false NEGATIVE (violations=[]),
    # which by definition leaves no Violation.message to inspect -
    # report.r9_diagnostics (evaluate_r6_codemodel's own docstring)
    # carries the forensic line for THIS fixture's own source
    # regardless of verdict, filtered here by the one needle this
    # fixture itself controls (evil.c is not a name any other fixture
    # in this file uses).
    evil_diagnostics = [d for d in report.r9_diagnostics if "evil.c" in d]
    return selftest_report(
        "hostile: add_library() with a source file OUTSIDE both the"
        " source root and the build directory",
        ok,
        # floor_ok/floor_lines (ORDEM DE SERVICO OS-WINDOWS-R9 rodada 2):
        # r9_diagnostics=[] is ambiguous on its own between "no source
        # was there to examine" (a floor problem, R6 never even
        # iterated a target's sources) and "a source WAS examined and
        # is_under_root() wrongly accepted it" (a logic problem, no
        # floor involved) - printing floor_ok/floor_lines lets the next
        # run's log tell the two apart without a THIRD round of
        # instrumentation.
        detail=f"violations={[v.format() for v in report.violations]}"
        f" r9_diagnostics={evil_diagnostics}"
        f" floor_ok={report.floor_ok} floor_lines={report.floor_lines}"
        f"{initial_configure_diagnostic(initial)}"
        f"{reconfigure_diagnostic(report)}",
    )


def selftest_positive_target_sources_generated_in_build_dir(scratch):
    """R9's own positive control (ORDEM DE SERVICO's own warning, and
    the leader's own instruction: "arquivos gerados... estao fora da
    arvore-fonte de proposito, e sao legitimos... mede a lista real de
    fontes... antes de escrever a condicao"). Measured live,
    29/08/2026, against the real tree's own glintfx_library target
    before writing R9: a genuinely generated source
    (generated/render/gl_functions.cpp) has an ABSOLUTE "path" already
    pointing inside build_dir - this fixture reproduces that exact
    shape with file(WRITE ...) (a plain, non-network file() sub-
    command, unrelated to R7) creating the file INSIDE ${CMAKE_BINARY_
    DIR} at configure time, then adding it as a target source by its
    absolute, build_dir-rooted path.

    build_dir is a SIBLING of root, deliberately NOT root's own child
    (every other fixture in this file nests build/ under its own
    source root, the same layout this project's real CI uses -
    `cmake -S . -B build-shared` from the repo root). Caught live while
    writing this control: with a nested build/, the generated file
    sits under BOTH source_root and build_dir at once, so the control
    stayed green even with R9's own build_dir acceptance branch
    deleted outright - a positive control that cannot fail is not
    proving what it claims. An out-of-tree build_dir is what actually
    forces this control through R9's "is_under_root(..., build_dir)"
    branch specifically, rather than through source_root by accident.
    """
    root = os.path.join(scratch, "positive_target_sources_generated")
    write_fixture(
        root,
        """cmake_minimum_required(VERSION 3.25)
project(positive_target_sources_generated C)
set(generated_c "${CMAKE_BINARY_DIR}/generated_ok.c")
file(WRITE "${generated_c}" "int generated_ok(void) { return 0; }\\n")
add_library(dummylib STATIC "${generated_c}")
""",
    )
    build_dir = os.path.join(scratch, "positive_target_sources_generated_build")
    initial = configure_fixture_with_ninja(root, build_dir)
    report = configure_and_judge(root, build_dir)
    r9_violations = [v for v in report.violations if v.rule == "R9"]
    # report.floor_ok, not just "no R9 violations": a fatal error one
    # statement earlier would ALSO leave r9_violations empty, VACUOUSLY,
    # because R9 never gets a chance to run at all (gated on codemodel_
    # ok) - see initial_configure_diagnostic()'s own docstring for the
    # real instance of this that hid on Windows. This fixture no longer
    # has an earlier fatal-prone statement (the unneeded find_package(
    # PkgConfig REQUIRED) was removed - R9 needs no PkgConfig at all),
    # but the check stays as a permanent guard against the same class
    # of false pass recurring.
    ok = not r9_violations and report.floor_ok
    return selftest_report(
        "positive: a source file that genuinely lives under the build"
        " directory (the shape a real generated file takes) is not"
        " an R9 violation",
        ok,
        detail=f"R9 violations={[v.format() for v in r9_violations]}"
        f" r9_diagnostics={report.r9_diagnostics}"
        f" floor_ok={report.floor_ok} floor_lines={report.floor_lines}"
        f"{initial_configure_diagnostic(initial)}"
        f"{reconfigure_diagnostic(report)}",
    )


def selftest_hostile_execute_process_identity_spoof(scratch):
    """REVIEW4-DEPZERO-TRACE.md CRITICO #1: a program with the exact
    ALLOWLISTED NAME ("pkg-config"), but at an arbitrary path, is a
    different program - basename comparison alone cannot tell them
    apart. The fix (evaluate_r8_execute_process's own comment) rejects
    an allowlisted name resolving to an ABSOLUTE path INSIDE the tree
    being judged - this fixture plants the fake binary inside the
    fixture's own source tree, exactly the shape the reviewer's own
    reproduction used ("inclusive dentro da propria arvore").
    """
    root = os.path.join(scratch, "hostile_execute_process_identity")
    fake_bin_dir = os.path.join(root, "fake_bin")
    os.makedirs(fake_bin_dir, exist_ok=True)
    fake_pkgconfig = os.path.join(fake_bin_dir, "pkg-config")
    with open(fake_pkgconfig, "w", encoding="utf-8") as handle:
        handle.write("#!/bin/sh\necho fake\n")
    os.chmod(fake_pkgconfig, 0o755)
    write_fixture(
        root,
        f"""cmake_minimum_required(VERSION 3.25)
project(hostile_execute_process_identity NONE)
execute_process(COMMAND "{cmake_path_literal(fake_pkgconfig)}" RESULT_VARIABLE rc)
add_custom_target(dummy)
""",
    )
    build_dir = os.path.join(root, "build")
    configure_fixture_with_ninja(root, build_dir)
    report = configure_and_judge(root, build_dir)
    ok = any(v.rule == "R8-identity" for v in report.violations)
    return selftest_report(
        "hostile: execute_process() runs a program NAMED pkg-config at"
        " an arbitrary path INSIDE the tree being judged - not the"
        " real system tool",
        ok,
        detail=f"violations={[v.format() for v in report.violations]}",
    )


def selftest_floor_wrong_type_field_reproves_by_parity(scratch):
    """REVIEW4-DEPZERO-TRACE.md IMPORTANTE (item 3): a trace event with
    "file" present but the WRONG TYPE (an int, not a string) used to
    slip past the truthy-only check (`not event.get("file")` is False
    for a truthy int) and later crash is_under_root() with an
    unhandled TypeError - fail-closed in effect (ctest reports the
    crash as a failure), but invalid_count LIED about the cause. Fixed
    by checking isinstance(..., str), not just truthiness - this
    control proves the crash is gone AND the count is honest.
    """
    root = os.path.join(scratch, "floor_wrong_type_field")
    write_fixture(
        root,
        "cmake_minimum_required(VERSION 3.25)\n"
        "project(floor_wrong_type_field NONE)\n"
        "add_custom_target(dummy)\n",
    )
    build_dir = os.path.join(root, "build")
    configure_fixture_with_ninja(root, build_dir)

    root_cmakelists = os.path.realpath(os.path.join(root, "CMakeLists.txt"))
    synthetic_lines = [
        json.dumps({"version": {"major": 1, "minor": 2}}),
        json.dumps({
            "cmd": "cmake_minimum_required",
            "file": root_cmakelists,
            "line": 1,
            "args": ["VERSION", "3.25"],
        }),
        json.dumps({
            "cmd": "project",
            "file": root_cmakelists,
            "line": 2,
            "args": ["floor_wrong_type_field", "NONE"],
        }),
        # The bug shape: "file" present, but an INTEGER, not a string.
        json.dumps({"cmd": "set", "file": 12345, "line": 3, "args": ["x", "y"]}),
    ]
    synthetic_scratch = make_scratch_workdir()
    try:
        synthetic_path = os.path.join(synthetic_scratch, "trace.json")
        with open(synthetic_path, "w", encoding="utf-8") as handle:
            handle.write("\n".join(synthetic_lines) + "\n")
        try:
            report = judge(root, build_dir, synthetic_path, False)
        except Exception as exc:  # noqa: BLE001 - a crash here IS the finding
            return selftest_report(
                "floor: a trace event with a WRONG-TYPE \"file\" field is"
                " treated as invalid, not a crash",
                False,
                detail=f"judge() raised {exc!r} instead of reporting a floor failure",
            )
        parity_line = _floor_line_for(report, "parity")
        ok = (
            not report.floor_ok
            and parity_line is not None
            and "could not use 1" in parity_line
        )
        return selftest_report(
            "floor: a trace event with a WRONG-TYPE \"file\" field (an"
            " int, not a string) is treated as invalid and reproves via"
            " parity, never silently accepted and never a crash",
            ok,
            detail=f"floor_lines={report.floor_lines}",
        )
    finally:
        shutil.rmtree(synthetic_scratch, ignore_errors=True)


def selftest_hostile_file_download(scratch):
    """REVIEW3-DEPZERO-TRACE.md CRITICO #1: file(DOWNLOAD ...), a
    native CMake command, no manager vocabulary involved at all.
    """
    root = os.path.join(scratch, "hostile_file_download")
    write_fixture(
        root,
        """cmake_minimum_required(VERSION 3.25)
project(hostile_file_download NONE)
file(DOWNLOAD
    "https://example.invalid/evil.h"
    "${CMAKE_BINARY_DIR}/evil.h"
    STATUS dl_status
)
""",
    )
    build_dir = os.path.join(root, "build")
    configure_fixture_with_ninja(root, build_dir)
    report = configure_and_judge(root, build_dir)
    ok = any(v.rule == "R7" and "DOWNLOAD" in v.message for v in report.violations)
    return selftest_report(
        "hostile: file(DOWNLOAD ...) - native network transfer, no"
        " dependency-manager vocabulary at all",
        ok,
        detail=f"violations={[v.format() for v in report.violations]}",
    )


def selftest_hostile_execute_process_disallowed_program(scratch):
    """REVIEW3-DEPZERO-TRACE.md CRITICO #2: execute_process() can run
    ANY program - here, a stand-in for "curl a tarball and extract it"
    (the reviewer's own example, kept inert: this just writes a local
    file, no real network access from a --selftest fixture).
    """
    root = os.path.join(scratch, "hostile_execute_process")
    write_fixture(
        root,
        """cmake_minimum_required(VERSION 3.25)
project(hostile_execute_process NONE)
execute_process(
    COMMAND sh -c "echo 'int vendored_evil(void){return 1;}' > ${CMAKE_BINARY_DIR}/vendored_evil.c"
    RESULT_VARIABLE dl_rc
)
""",
    )
    build_dir = os.path.join(root, "build")
    configure_fixture_with_ninja(root, build_dir)
    report = configure_and_judge(root, build_dir)
    ok = any(v.rule == "R8" and "'sh'" in v.message for v in report.violations)
    return selftest_report(
        "hostile: execute_process(COMMAND sh ...) - a program outside"
        " the allowlist",
        ok,
        detail=f"violations={[v.format() for v in report.violations]}",
    )


def selftest_positive_execute_process_pkgconfig(scratch):
    """The ONE real, necessary execute_process() use in this project
    (cmake/GlintfxWaylandProtocols.cmake's own glintfx_locate_xdg_
    shell_protocol_xml()) MUST keep passing - R8 exists to judge the
    program, not to blanket-ban the command CMake's own FindPkgConfig.
    cmake module (loaded here via find_package(PkgConfig), no
    REQUIRED) ALSO uses internally for its own `pkg-config --version`
    probe - this fixture exercises BOTH calls, proving neither one
    false-positives.

    if(PkgConfig_FOUND) guards the execute_process() call - measured
    live (see initial_configure_diagnostic()'s own docstring):
    windows-latest genuinely has no pkg-config at all, and the REAL
    file this fixture models (cmake/GlintfxWaylandProtocols.cmake) is
    if(UNIX)-guarded there too - this fixture never claims to prove
    the execute_process(pkg-config) shape works WITHOUT pkg-config
    (there is nothing to prove: the real code never runs there
    either), only that R8 does not false-positive when it DOES run.
    Confirmed live, both branches: pkg-config present -> the exact
    real call is traced with its expanded args (proving R8's
    allowlist accepts it); pkg-config absent -> the guard skips it
    entirely and the configure still completes cleanly (report.floor_
    ok stays True, proving the REST of this fixture - and the empty
    R8 result - is a genuine pass, not the vacuous one initial_
    configure_diagnostic()'s docstring documents this exact control
    having had before this fix).
    """
    root = os.path.join(scratch, "positive_execute_process_pkgconfig")
    write_fixture(
        root,
        """cmake_minimum_required(VERSION 3.25)
project(positive_execute_process_pkgconfig NONE)
find_package(PkgConfig)
if(PkgConfig_FOUND)
    execute_process(
        COMMAND "${PKG_CONFIG_EXECUTABLE}" --variable=pkgdatadir wayland-protocols
        OUTPUT_VARIABLE dummy_out
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE dummy_result
    )
endif()
add_custom_target(dummy)
""",
    )
    build_dir = os.path.join(root, "build")
    initial = configure_fixture_with_ninja(root, build_dir)
    report = configure_and_judge(root, build_dir)
    r8_violations = [v for v in report.violations if v.rule == "R8"]
    # report.floor_ok, not just "no R8 violations" - see this
    # function's own docstring and initial_configure_diagnostic()'s:
    # a fatal error one statement earlier used to leave r8_violations
    # empty VACUOUSLY (R8 never got a chance to run, gated on the same
    # trace floors as everything else) - this is the exact control
    # that hid that bug on Windows.
    ok = not r8_violations and report.floor_ok
    return selftest_report(
        "positive: execute_process(COMMAND pkg-config ...) - the ONE"
        " real use in this project, plus CMake's OWN internal"
        " FindPkgConfig.cmake probe - neither false-positives",
        ok,
        detail=f"R8 violations={[v.format() for v in r8_violations]}"
        f" floor_ok={report.floor_ok}{initial_configure_diagnostic(initial)}",
    )


def selftest_execute_process_empty_launcher_slot_not_misidentified(scratch):
    """execute_process()'s EMPTY launcher-prefix slot (CMake's own
    CMAKE_CROSSCOMPILING_EMULATOR-style convention) must never itself
    be reported as "the program". Not a hypothetical: measured live,
    29/08/2026, re-verifying THIS fatia's own R8 against the real
    tree - a plain find_package(Python3 COMPONENTS Interpreter
    REQUIRED), no indirection, made CMake's own FindPython/
    Support.cmake trace execute_process(COMMAND "" "/usr/bin/
    python3.14" -c ...) nine separate times, and R8's first version
    (args[index+1] straight after "COMMAND", no empty-slot skip)
    reported all nine as "program outside the allowlist: ''".

    Deliberately NOT asserting "zero R8 violations" here: python3.14
    itself is genuinely NOT in EXECUTE_PROCESS_PROGRAM_ALLOWLIST (this
    project's real CMakeLists.txt never calls find_package(Python3) -
    it resolves the interpreter via find_program() instead, precisely
    to avoid this module's OWN surface entirely - see tests/
    CMakeLists.txt's own comment on that switch), so R8 correctly,
    CORRECTLY still flags the REAL resolved program once the empty
    slot is skipped - that is R8 working as designed, not a bug to
    hide. The ONLY claim this control makes is narrower and precise:
    no violation ever names the EMPTY STRING as the offending program.
    """
    root = os.path.join(scratch, "execute_process_empty_launcher_slot")
    write_fixture(
        root,
        """cmake_minimum_required(VERSION 3.25)
project(execute_process_empty_launcher_slot NONE)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_custom_target(dummy)
""",
    )
    build_dir = os.path.join(root, "build")
    configure_fixture_with_ninja(root, build_dir)
    report = configure_and_judge(root, build_dir)
    r8_violations = [v for v in report.violations if v.rule == "R8"]
    empty_program_violations = [v for v in r8_violations if "''" in v.message]
    real_program_violations = [v for v in r8_violations if "python3" in v.message.lower()]
    # Positive AND negative in one control: the empty slot must be
    # invisible (empty_program_violations == []), and the REAL program
    # this fixture actually runs must still be visible
    # (real_program_violations != []) - a control that only checked
    # "no empty-string violation" could pass just as well if R8 had
    # gone blind entirely, which would be a very different, much worse
    # bug hiding behind an apparently-green result.
    ok = not empty_program_violations and bool(real_program_violations)
    return selftest_report(
        "execute_process(): an empty launcher-prefix slot before the"
        " real program (CMake's own FindPython/Support.cmake shape) is"
        " never itself reported as the violating program - the REAL"
        " program (not allowlisted here on purpose) still is",
        ok,
        detail=f"R8 violations={[v.format() for v in r8_violations]}",
    )


# Two positive controls (ORDEM DE SERVICO: "tao obrigatorios quanto" -
# a gate that blocks the legitimate is disabled within a week).


def selftest_positive_find_package_multiline(scratch):
    root = os.path.join(scratch, "positive_multiline_find_package")
    write_fixture(
        root,
        """cmake_minimum_required(VERSION 3.25)
project(positive_multiline NONE)
find_package(
    PkgConfig
)
# A real target, so FLOOR(codemodel) (GODS_LAWS.md L-40, "0 targets
# reproves") judges what this control actually means to prove, not an
# unrelated, empty-project artifact of the fixture being minimal.
add_custom_target(dummy)
""",
    )
    build_dir = os.path.join(root, "build")
    initial = configure_fixture_with_ninja(root, build_dir)
    report = configure_and_judge(root, build_dir)
    ok = not report.violations and report.floor_ok
    return selftest_report(
        "positive: find_package(PkgConfig REQUIRED) written across multiple lines",
        ok,
        detail=f"violations={[v.format() for v in report.violations]}"
        f" floor_ok={report.floor_ok} floor_lines={report.floor_lines}"
        f"{initial_configure_diagnostic(initial)}",
    )


def selftest_positive_pkg_check_modules_version_comparator(scratch):
    root = os.path.join(scratch, "positive_version_comparator")
    write_fixture(
        root,
        """cmake_minimum_required(VERSION 3.25)
project(positive_version_comparator NONE)
find_package(PkgConfig)
pkg_check_modules(Fixture wayland-client>=1.20)
add_custom_target(dummy)
""",
    )
    build_dir = os.path.join(root, "build")
    initial = configure_fixture_with_ninja(root, build_dir)
    report = configure_and_judge(root, build_dir)
    ok = not report.violations and report.floor_ok
    return selftest_report(
        "positive: pkg_check_modules(... wayland-client>=1.20) (version comparator)",
        ok,
        detail=f"violations={[v.format() for v in report.violations]}"
        f" floor_ok={report.floor_ok} floor_lines={report.floor_lines}"
        f"{initial_configure_diagnostic(initial)}",
    )


# Floor controls (GODS_LAWS.md L-40: positive / negative / empty-scan
# are not enough on their own - this file's floors are proven directly).


def selftest_floor_empty_trace_reproves(scratch):
    root = os.path.join(scratch, "floor_empty_trace")
    write_fixture(root, "cmake_minimum_required(VERSION 3.25)\nproject(floor_empty NONE)\n")
    build_dir = os.path.join(root, "build")
    configure_fixture_with_ninja(root, build_dir)
    empty_trace_scratch = make_scratch_workdir()
    try:
        empty_trace_path = os.path.join(empty_trace_scratch, "trace.json")
        open(empty_trace_path, "w", encoding="utf-8").close()  # 0 bytes, no header
        report = judge(root, build_dir, empty_trace_path, False)
        ok = not report.floor_ok and any(
            "parity" in line or "repo-scan" in line or "sentinel" in line
            for line in report.floor_lines
        )
        return selftest_report(
            "floor: an empty trace file reproves (never a silent success)",
            ok,
            detail=f"floor_lines={report.floor_lines}",
        )
    finally:
        shutil.rmtree(empty_trace_scratch, ignore_errors=True)


def selftest_floor_corrupted_line_reproves_by_parity(scratch):
    root = os.path.join(scratch, "floor_corrupted_trace")
    write_fixture(root, "cmake_minimum_required(VERSION 3.25)\nproject(floor_corrupted NONE)\n")
    build_dir = os.path.join(root, "build")
    os.makedirs(build_dir, exist_ok=True)
    real_scratch = make_scratch_workdir()
    try:
        real_trace_path = os.path.join(real_scratch, "trace.json")
        run_configure_with_trace(root, build_dir, real_trace_path)
        lines = read_trace_lines(real_trace_path)
        if len(lines) < 2:
            return selftest_report(
                "floor: a corrupted trace line reproves by parity",
                False,
                detail="fixture trace too short to corrupt - cannot prove this control",
            )
        lines[1] = "{not valid json"
        corrupted_path = os.path.join(real_scratch, "corrupted.json")
        with open(corrupted_path, "w", encoding="utf-8") as handle:
            handle.write("\n".join(lines) + "\n")
        report = judge(root, build_dir, corrupted_path, False)
        ok = not report.floor_ok and any("parity" in line for line in report.floor_lines)
        return selftest_report(
            "floor: a corrupted trace line reproves by parity",
            ok,
            detail=f"floor_lines={report.floor_lines}",
        )
    finally:
        shutil.rmtree(real_scratch, ignore_errors=True)


def selftest_is_under_root_rejects_empty_and_relative(scratch):
    """REVIEW3-DEPZERO-TRACE.md CRITICO #4, the root cause in its
    purest, most direct form - the reviewer's OWN reproduction was a
    single interpreter line: `os.path.realpath("")` resolves to the
    process's own CURRENT WORKING DIRECTORY, not an invalid sentinel.
    selftest_floor_events_without_file_do_not_inflate_scan (above) is
    the end-to-end proof through judge(), but it runs from fixtures
    under a scratch dir that does NOT coincide with this process's own
    cwd - the exact "normal situation" that makes the bug dangerous
    (ctest/this script both typically run with cwd INSIDE source_root)
    is not naturally exercised there. This control tests is_under_
    root() directly, using THIS PROCESS'S OWN cwd as the root - the
    precise condition the reviewer measured - so there is no ambiguity
    about whether the fix's root cause, not just a downstream symptom
    of it, is what is being proven.
    """
    cwd = os.getcwd()
    empty_rejected = is_under_root("", cwd) is False
    relative_rejected = is_under_root("some/relative/path", cwd) is False
    # Positive control alongside the two negatives (GODS_LAWS.md L-40:
    # a check that only ever returns False could be trivially "correct"
    # by being broken in the OTHER direction) - a real absolute path
    # genuinely under cwd must still be accepted.
    real_child = os.path.join(cwd, "some_real_child_of_cwd")
    positive_accepted = is_under_root(real_child, cwd) is True
    ok = empty_rejected and relative_rejected and positive_accepted
    return selftest_report(
        "is_under_root(): empty string and relative paths are NEVER"
        ' "under root" (os.path.realpath("") resolving to this'
        " process's own cwd is exactly the reviewer's own reproduction),"
        " while a real absolute child path still is",
        ok,
        detail=f"cwd={cwd!r} empty_rejected={empty_rejected}"
        f" relative_rejected={relative_rejected}"
        f" positive_accepted={positive_accepted}",
    )


def selftest_floor_events_without_file_do_not_inflate_scan(scratch):
    """REVIEW3-DEPZERO-TRACE.md CRITICO #4, first half: a trace event
    that IS valid JSON but is missing "file" (the exact shape a
    mid-configure truncation can produce - CMake flushes each line as
    it goes, so a kill mid-statement can leave a syntactically-valid-
    but-semantically-incomplete object on disk) used to be silently
    counted as "under the repository" by is_under_root("", root), which
    resolves the empty string to the process's own cwd via os.path.
    realpath("") - reproduced live by the reviewer against the REAL
    glintfx build/. This control builds that exact shape by hand (3
    "file"-less events glued onto 2 real, well-formed ones) and proves
    BOTH halves of the fix: the file-less events are excluded from
    FLOOR(repo-scan)'s own count (2, not 5), AND they reprove the whole
    judgement via FLOOR(parity) instead of passing silently.
    """
    root = os.path.join(scratch, "floor_no_file_field")
    write_fixture(
        root,
        "cmake_minimum_required(VERSION 3.25)\n"
        "project(floor_no_file_field NONE)\n"
        "add_custom_target(dummy)\n",
    )
    build_dir = os.path.join(root, "build")
    configure_fixture_with_ninja(root, build_dir)

    root_cmakelists = os.path.realpath(os.path.join(root, "CMakeLists.txt"))
    synthetic_lines = [
        json.dumps({"version": {"major": 1, "minor": 2}}),
        json.dumps({
            "cmd": "cmake_minimum_required",
            "file": root_cmakelists,
            "line": 1,
            "args": ["VERSION", "3.25"],
        }),
        json.dumps({
            "cmd": "project",
            "file": root_cmakelists,
            "line": 2,
            "args": ["floor_no_file_field", "NONE"],
        }),
    ]
    for _ in range(3):
        # The bug shape: valid JSON, no "file" at all - not "", ABSENT.
        synthetic_lines.append(json.dumps({"cmd": "set", "args": ["x", "y"]}))

    synthetic_scratch = make_scratch_workdir()
    try:
        synthetic_path = os.path.join(synthetic_scratch, "trace.json")
        with open(synthetic_path, "w", encoding="utf-8") as handle:
            handle.write("\n".join(synthetic_lines) + "\n")
        report = judge(root, build_dir, synthetic_path, False)

        parity_line = _floor_line_for(report, "parity")
        scan_line = _floor_line_for(report, "repo-scan")
        ok = (
            not report.floor_ok
            and parity_line is not None
            and "could not use 3" in parity_line
            and scan_line is not None
            and "2 trace event(s)" in scan_line
        )
        return selftest_report(
            "floor: trace events missing \"file\" are never counted as"
            " repository events, and reprove via parity instead of"
            " silently passing (REVIEW3-DEPZERO-TRACE.md CRITICO #4)",
            ok,
            detail=f"floor_lines={report.floor_lines}",
        )
    finally:
        shutil.rmtree(synthetic_scratch, ignore_errors=True)


def selftest_codemodel_reply_not_stale(scratch):
    """REVIEW3-DEPZERO-TRACE.md CRITICO #4, second half.

    Measured live while writing this control (NOT assumed): CMake's
    OWN file-API generation already deletes a stale index-*.json by
    ITSELF on a reconfigure that reaches "Generating done" - a plain
    second `cmake -S -B`, with nothing wrong, silently makes this
    exact bug look fixed whether or not clear_stale_codemodel_reply_
    pointers() ever runs, because CMake's own success path already
    cleans up. That is NOT the scenario the reviewer's finding is
    about: the finding is what happens when a reconfigure does NOT
    reach that point (crash/kill mid-configure - CMake never gets a
    chance to clean up). A true process kill is not something this
    control can time deterministically, but a CMakeLists.txt PARSE
    ERROR reproduces the identical file-API shape WITHOUT any timing
    race: measured live that CMake, on a pure parse failure (before
    project() ever runs), writes a fresh error-*.json for the current
    attempt but does NOT touch any PRE-EXISTING index-*.json/other
    error-*.json files sitting in reply/ - the exact "stale reply
    lingers" shape, portable and 100% reproducible.
    """
    root = os.path.join(scratch, "codemodel_staleness_fixture")
    good_cmakelists = (
        "cmake_minimum_required(VERSION 3.25)\n"
        "project(codemodel_staleness_fixture NONE)\n"
        "add_custom_target(dummy)\n"
    )
    write_fixture(root, good_cmakelists)
    build_dir = os.path.join(root, "build")
    configure_fixture_with_ninja(root, build_dir)

    # Establishes a genuine, fresh, SUCCESSFUL reply first.
    first_scratch = make_scratch_workdir()
    try:
        run_configure_with_trace(root, build_dir, os.path.join(first_scratch, "trace.json"))
    finally:
        shutil.rmtree(first_scratch, ignore_errors=True)

    reply_dir = os.path.join(build_dir, ".cmake", "api", "v1", "reply")
    fake_stale = os.path.join(reply_dir, "index-0000-01-01T00-00-00-0000.json")
    with open(fake_stale, "w", encoding="utf-8") as handle:
        json.dump(
            {
                "cmake": {},
                "objects": [],
                "reply": {
                    "codemodel-v2": {
                        "error": "PLANTED STALE ENTRY - if this is what got"
                        " read, the staleness fix did not run"
                    }
                },
            },
            handle,
        )
    # Force this planted file to look NEWEST by mtime, so a naive
    # max(candidates, key=os.path.getmtime) (the pre-fix logic) would
    # pick it over any OTHER stale file present, removing any doubt
    # about mtime ordering deciding the outcome by accident.
    future = time.time() + 3600
    os.utime(fake_stale, (future, future))

    # NOW break the tree with a pure parse error - CMake will fail
    # before ever reaching project(), so its own success-path cleanup
    # (confirmed live, see this function's own docstring) never runs.
    write_fixture(root, "cmake_minimum_required(VERSION 3.25\nproject(unclosed NONE)\n")

    second_scratch = make_scratch_workdir()
    try:
        configure_result = run_configure_with_trace(
            root, build_dir, os.path.join(second_scratch, "trace.json")
        )
        codemodel, error = read_codemodel_reply(build_dir)
        ok = (
            configure_result.returncode != 0  # the parse error really happened
            and not os.path.exists(fake_stale)  # the plant is GONE, not just outranked
            and error is not None
            and "PLANTED STALE ENTRY" not in error  # never read the plant as truth
        )
        return selftest_report(
            "floor: a planted stale codemodel reply pointer, with its"
            " mtime forced into the future, is REMOVED before a"
            " reconfigure that itself FAILS (parse error, before CMake's"
            " own success-path cleanup would ever run) - never read as"
            " if it were an answer for this attempt",
            ok,
            detail=f"configure_rc={configure_result.returncode}"
            f" fake_stale_exists={os.path.exists(fake_stale)} error={error!r}",
        )
    finally:
        shutil.rmtree(second_scratch, ignore_errors=True)


def selftest_same_path_platform_aware(_scratch):
    """REVIEW3-DEPZERO-TRACE.md item 8 (declared as a hypothesis,
    "nao confirmado nem refutado" - could not be proven from Linux).

    CAUSA 2 of the server debut (29/08/2026, GHA run 33248706044,
    branch depzero-gate): this control's FIRST version hardcoded the
    assertion for POSIX only ("case_differs_on_posix" - it NAMED the
    platform it assumed) - on the real windows-latest runner, NTFS
    correctly folds case, os.path.normcase() correctly returns the
    SAME normalized string for "CMakeLists.txt" and "cmakelists.txt"
    there, and the POSIX-only assertion failed - not because the FIX
    (normcase()) was wrong, but because the TEST assumed one platform
    family. The uncertainty was declared honestly in round 3
    ("UNPROVABLE from Linux, not claimed as fact") - which is exactly
    why it surfaced as a VISIBLE failure on the first real Windows run
    instead of a silent wrong assumption baked into a passing test
    (GODS_LAWS.md house lesson: a declared limit becomes a found
    defect the moment it CAN be tested, never a surprise).

    This version asks os.path.normcase() itself which behavior is
    correct HERE, on whichever platform actually runs it, instead of
    assuming: sys.platform decides the EXPECTED direction, and the
    same two fixed strings are checked against it on both platform
    families - Linux proves the POSIX direction (case matters),
    Windows proves the NTFS direction (case does not) - one control,
    two provably-correct outcomes, neither one assumed.
    """
    identical_ok = same_path_platform_aware("/a/b/CMakeLists.txt", "/a/b/CMakeLists.txt")
    case_different_paths_equal = same_path_platform_aware(
        "/a/b/CMakeLists.txt", "/a/b/cmakelists.txt"
    )
    on_case_insensitive_platform = sys.platform.startswith("win")
    # NTFS (Windows): case-insensitive, case-different paths name the
    # SAME file - normcase() must fold them together, so this expects
    # True. POSIX (everything else in this project's CI matrix):
    # case-SENSITIVE, case-different paths are DIFFERENT files -
    # normcase() is a no-op there, so this expects False.
    case_behaves_correctly = case_different_paths_equal == on_case_insensitive_platform
    ok = identical_ok and case_behaves_correctly
    return selftest_report(
        "same_path_platform_aware(): identical paths match; on THIS"
        " platform, case-different paths behave EXACTLY the way"
        " os.path.normcase() promises for it (folded together on"
        " Windows/NTFS, kept distinct on POSIX) - proven on whichever"
        " platform actually runs this, not assumed for one",
        ok,
        detail=f"platform={sys.platform!r} identical_ok={identical_ok}"
        f" case_different_paths_equal={case_different_paths_equal}"
        f" on_case_insensitive_platform={on_case_insensitive_platform}",
    )


def selftest_declarations_printed_on_every_run(scratch):
    """REVIEW3-DEPZERO-TRACE.md item 4 (trust boundary, undeclared) and
    CRITICO #3 (scope declaration overstated the shallow net's
    coverage of file()/execute_process()): both declarations must
    appear on every REAL invocation (--selftest's own fixtures never
    go through real_main(), so this is the one control that actually
    runs the script as a SUBPROCESS, the same way tests/CMakeLists.txt
    does, and reads its stdout).
    """
    root = os.path.join(scratch, "declarations_fixture")
    write_fixture(
        root,
        "cmake_minimum_required(VERSION 3.25)\n"
        "project(declarations_fixture NONE)\n"
        "add_custom_target(dummy)\n",
    )
    build_dir = os.path.join(root, "build")
    configure_fixture_with_ninja(root, build_dir)

    script_path = os.path.abspath(__file__)
    result = subprocess.run(
        [sys.executable, script_path, root, build_dir],
        capture_output=True,
        text=True,
    )
    # REVIEW4-DEPZERO-TRACE.md item 6: the round-3 declaration was
    # complete FOR THAT ROUND's own findings, but round 4 found the
    # SAME "true but incomplete" defect against a longer list -
    # asserting on the three round-4 gaps too, not just the two from
    # round 3, is what keeps this control from suffering the identical
    # fate a fourth time.
    ok = (
        "file(DOWNLOAD|UPLOAD) or execute_process()" in result.stdout
        and "must never be invoked with a source-root/build-dir pair" in result.stdout
        and "file(COPY" in result.stdout
        and "file(ARCHIVE_EXTRACT" in result.stdout
        and "add_library/add_executable/target_sources" in result.stdout
        and "concurrent invocation" in result.stdout
    )
    return selftest_report(
        "declares, on every run, the scope limit of the shallow-net"
        " promise, the file(COPY)/file(ARCHIVE_EXTRACT) gap, the R9"
        " coverage claim, the trust boundary, and the concurrency"
        " limit - not just the two declarations round 3 checked",
        ok,
        detail=f"stdout={result.stdout!r}",
    )


def _floor_line_for(report, label):
    """The one line in report.floor_lines starting "FLOOR(<label>):",
    or None. Lets a selftest control assert on ONE floor's own verdict
    without being confused by the others (see this function's own
    caller for why that distinction matters).
    """
    prefix = f"FLOOR({label}):"
    for line in report.floor_lines:
        if line.startswith(prefix):
            return line
    return None


def selftest_floor_missing_sentinel_reproves(scratch):
    """REVIEW3-DEPZERO-TRACE.md item 6: the PREVIOUS version of this
    control judged a real trace against an UNRELATED source_root, which
    made FLOOR(repo-scan) and FLOOR(codemodel) ALSO fail (0 events
    under an unrelated root; 0 targets, no add_custom_target in that
    tiny fixture) - three floors failing for three DIFFERENT reasons in
    the same fixture, while the assertion only checked "the word
    'sentinel' appears somewhere in the output", which is true of the
    PASSING sentinel line's own label too. The reviewer mutated
    check_floor_sentinel() to always return True and this control
    stayed green, because floor_ok was still False from the OTHER two
    floors regardless.

    Fixed by construction, not by a smarter assertion alone: source_root
    passed to judge() is the fixture's PARENT directory (an ANCESTOR of
    where the real configure ran), not an unrelated tree. That keeps
    FLOOR(repo-scan) passing (the real project()/add_custom_target
    events are still textually UNDER the parent directory - is_under_
    root()'s own startswith(root + os.sep) check does not care how far
    below the root a file sits) and FLOOR(codemodel) passing (the real
    build still resolves its own real add_custom_target(dummy)), while
    FLOOR(sentinel) is the ONLY one that can fail: the root CMakeLists.
    txt project() event MUST sit AT source_root/CMakeLists.txt exactly,
    and it does not - it sits one directory deeper. _floor_line_for()
    then asserts on the FOUR floor lines INDIVIDUALLY, not on whether
    the word "sentinel" merely occurs.
    """
    real_root = os.path.join(scratch, "floor_sentinel_real")
    write_fixture(
        real_root,
        "cmake_minimum_required(VERSION 3.25)\n"
        "project(floor_sentinel_real NONE)\n"
        "add_custom_target(dummy)\n",
    )
    real_build = os.path.join(real_root, "build")
    configure_fixture_with_ninja(real_root, real_build)

    # scratch itself is the ANCESTOR source_root - real_root sits one
    # level below it, and has no CMakeLists.txt of its own.
    ancestor_source_root = scratch

    trace_scratch = make_scratch_workdir()
    try:
        trace_path = os.path.join(trace_scratch, "trace.json")
        run_configure_with_trace(real_root, real_build, trace_path)
        report = judge(ancestor_source_root, real_build, trace_path, False)

        parity_line = _floor_line_for(report, "parity")
        scan_line = _floor_line_for(report, "repo-scan")
        sentinel_line = _floor_line_for(report, "sentinel")
        codemodel_line = _floor_line_for(report, "codemodel")

        isolated = (
            not report.floor_ok
            and parity_line is not None
            and "could not use" not in parity_line
            and scan_line is not None
            and "empty scan" not in scan_line
            and sentinel_line is not None
            and "never appeared" in sentinel_line
            and codemodel_line is not None
            and "0 targets" not in codemodel_line
        )
        return selftest_report(
            "floor: a trace whose project() event sits one directory"
            " below the given source root reproves by sentinel ALONE"
            " (parity/repo-scan/codemodel stay green - isolation proven,"
            " not just floor_ok being False for some reason)",
            isolated,
            detail=f"floor_lines={report.floor_lines}",
        )
    finally:
        shutil.rmtree(trace_scratch, ignore_errors=True)


def selftest_main():
    scratch = make_scratch_workdir()
    controls = [
        selftest_hostile_multiline_and_literal,
        selftest_hostile_indirection,
        selftest_hostile_cpm_sibling,
        selftest_hostile_pkg_check_modules_digit_name,
        selftest_hostile_add_subdirectory_outside_tree,
        selftest_hostile_file_download,
        selftest_hostile_execute_process_disallowed_program,
        selftest_hostile_target_sources_external,
        selftest_hostile_execute_process_identity_spoof,
        selftest_positive_find_package_multiline,
        selftest_positive_pkg_check_modules_version_comparator,
        selftest_positive_execute_process_pkgconfig,
        selftest_execute_process_empty_launcher_slot_not_misidentified,
        selftest_positive_target_sources_generated_in_build_dir,
        selftest_floor_empty_trace_reproves,
        selftest_floor_corrupted_line_reproves_by_parity,
        selftest_is_under_root_rejects_empty_and_relative,
        selftest_floor_events_without_file_do_not_inflate_scan,
        selftest_floor_wrong_type_field_reproves_by_parity,
        selftest_codemodel_reply_not_stale,
        selftest_same_path_platform_aware,
        selftest_declarations_printed_on_every_run,
        selftest_floor_missing_sentinel_reproves,
    ]
    try:
        results = [control(scratch) for control in controls]
    finally:
        shutil.rmtree(scratch, ignore_errors=True)

    if not all(results):
        print(
            f"{SCRIPT_NAME} --selftest: FAILED ({results.count(False)} of"
            f" {len(results)} control(s) - see above)",
            file=sys.stderr,
        )
        sys.exit(1)
    print(f"{SCRIPT_NAME} --selftest: all {len(results)} controls OK")


def main(argv):
    if argv[:1] == ["--selftest"]:
        selftest_main()
    else:
        real_main(argv)


if __name__ == "__main__":
    main(sys.argv[1:])
