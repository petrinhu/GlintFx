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
# ALSO watched by check_dep_zero.sh, the older, still-active,
# text-based gate this fatia is explicitly forbidden from editing -
# find_program() satisfies both oracles without widening scope).
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

_VERSION_COMPARATOR_RE = re.compile(r"^([^<>=!]+)")


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
    real_candidate = os.path.realpath(candidate_path)
    real_root = os.path.realpath(root_path)
    return real_candidate == real_root or real_candidate.startswith(
        real_root + os.sep
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


def run_configure_with_trace(source_root, build_dir, trace_path):
    """Re-configures build_dir, writing the json-v1 trace to trace_path.

    Deliberately does NOT branch on the returncode here - see the
    header comment's "cmake's own exit code is not a reliable signal"
    section. The caller always proceeds to read whatever trace and
    codemodel reply exist; the floor checks decide usability.
    """
    prepare_codemodel_query(build_dir)
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
    """Returns (events, unparsed_count). See floor check (i)."""
    events = []
    unparsed = 0
    for raw_line in lines[1:]:  # lines[0] is the {"version": ...} header
        try:
            events.append(json.loads(raw_line))
        except json.JSONDecodeError:
            unparsed += 1
    return events, unparsed


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


# --- R6: the codemodel, as a witness ---------------------------------------


def evaluate_r6_codemodel(codemodel, reply_dir, source_root):
    """Returns (violations, link_libraries_witness_note)."""
    violations = []
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
            if not link_libraries_available:
                continue
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
    return violations, note


# --- the four floor checks (GODS_LAWS.md L-40) ------------------------------


def check_floor_parity(trace_lines, events, unparsed_count):
    expected = max(len(trace_lines) - 1, 0)  # minus the {"version": ...} header
    parsed = len(events)
    if unparsed_count > 0 or parsed != expected - unparsed_count:
        return (
            False,
            f"FLOOR(parity): parser lost {unparsed_count} line(s) out of"
            f" {expected} trace line(s) - a parser that loses lines"
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


def check_floor_sentinel(events, source_root, require_pkgconfig_sentinel):
    root_cmakelists = os.path.realpath(
        os.path.join(source_root, "CMakeLists.txt")
    )
    project_seen = any(
        e.get("cmd", "").lower() == "project"
        and os.path.realpath(e.get("file", "")) == root_cmakelists
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


# --- the whole judgement, assembled -----------------------------------------


class Report:
    def __init__(self):
        self.violations = []
        self.floor_lines = []
        self.floor_ok = True
        self.notes = []


def judge(source_root, build_dir, trace_path, require_pkgconfig_sentinel):
    report = Report()

    trace_lines = read_trace_lines(trace_path)
    events, unparsed_count = parse_trace_events(trace_lines)

    parity_ok, parity_line = check_floor_parity(trace_lines, events, unparsed_count)
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

    if codemodel_ok:
        reply_dir = os.path.join(build_dir, ".cmake", "api", "v1", "reply")
        r6_violations, r6_note = evaluate_r6_codemodel(
            codemodel, reply_dir, source_root
        )
        report.violations.extend(r6_violations)
        report.notes.append(r6_note)

    return report


# --- real mode --------------------------------------------------------------


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

    print(
        f"{SCRIPT_NAME}: 0 violation(s) - this oracle judges the executed"
        " configuration; untaken branches are covered only as literal text"
        " by the shallow net (tools/git-hooks/pre-commit), and per-platform"
        " by the CI matrix"
    )


# --- --selftest: fixtures and controls --------------------------------------


def make_scratch_workdir():
    return tempfile.mkdtemp(
        prefix="glintfx-dep-zero-trace-selftest-", dir=os.environ.get("TMPDIR")
    )


def write_fixture(root, cmakelists_body):
    os.makedirs(root, exist_ok=True)
    with open(os.path.join(root, "CMakeLists.txt"), "w", encoding="utf-8") as handle:
        handle.write(cmakelists_body)


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
        run_configure_with_trace(source_root, build_dir, trace_path)
        return judge(source_root, build_dir, trace_path, False)
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
    os.makedirs(build_dir, exist_ok=True)
    subprocess.run(["cmake", "-S", root, "-B", build_dir], capture_output=True)
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
    os.makedirs(build_dir, exist_ok=True)
    subprocess.run(["cmake", "-S", root, "-B", build_dir], capture_output=True)
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
    os.makedirs(build_dir, exist_ok=True)
    subprocess.run(["cmake", "-S", root, "-B", build_dir], capture_output=True)
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
find_package(PkgConfig REQUIRED)
pkg_check_modules(Fixture REQUIRED 7zip)
""",
    )
    build_dir = os.path.join(root, "build")
    os.makedirs(build_dir, exist_ok=True)
    subprocess.run(["cmake", "-S", root, "-B", build_dir], capture_output=True)
    report = configure_and_judge(root, build_dir)
    ok = any(
        v.rule == "R4" and "7zip" in v.message for v in report.violations
    )
    return selftest_report(
        "hostile: pkg_check_modules(... 7zip) (digit-leading module name)",
        ok,
        detail=f"violations={[v.format() for v in report.violations]}",
    )


def selftest_hostile_add_subdirectory_outside_tree(scratch):
    outside = os.path.join(scratch, "outside_the_tree")
    write_fixture(outside, "cmake_minimum_required(VERSION 3.25)\nproject(outsider NONE)\n")
    root = os.path.join(scratch, "hostile_add_subdirectory")
    write_fixture(
        root,
        f"""cmake_minimum_required(VERSION 3.25)
project(hostile_add_subdirectory NONE)
add_subdirectory({outside} outsider-build)
""",
    )
    build_dir = os.path.join(root, "build")
    os.makedirs(build_dir, exist_ok=True)
    subprocess.run(["cmake", "-S", root, "-B", build_dir], capture_output=True)
    report = configure_and_judge(root, build_dir)
    ok = any(v.rule == "R5" for v in report.violations)
    return selftest_report(
        "hostile: add_subdirectory() pointing outside the source root",
        ok,
        detail=f"violations={[v.format() for v in report.violations]}",
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
    REQUIRED
)
# A real target, so FLOOR(codemodel) (GODS_LAWS.md L-40, "0 targets
# reproves") judges what this control actually means to prove, not an
# unrelated, empty-project artifact of the fixture being minimal.
add_custom_target(dummy)
""",
    )
    build_dir = os.path.join(root, "build")
    os.makedirs(build_dir, exist_ok=True)
    subprocess.run(["cmake", "-S", root, "-B", build_dir], capture_output=True)
    report = configure_and_judge(root, build_dir)
    ok = not report.violations and report.floor_ok
    return selftest_report(
        "positive: find_package(PkgConfig REQUIRED) written across multiple lines",
        ok,
        detail=f"violations={[v.format() for v in report.violations]}"
        f" floor_ok={report.floor_ok} floor_lines={report.floor_lines}",
    )


def selftest_positive_pkg_check_modules_version_comparator(scratch):
    root = os.path.join(scratch, "positive_version_comparator")
    write_fixture(
        root,
        """cmake_minimum_required(VERSION 3.25)
project(positive_version_comparator NONE)
find_package(PkgConfig REQUIRED)
pkg_check_modules(Fixture REQUIRED wayland-client>=1.20)
add_custom_target(dummy)
""",
    )
    build_dir = os.path.join(root, "build")
    os.makedirs(build_dir, exist_ok=True)
    subprocess.run(["cmake", "-S", root, "-B", build_dir], capture_output=True)
    report = configure_and_judge(root, build_dir)
    ok = not report.violations and report.floor_ok
    return selftest_report(
        "positive: pkg_check_modules(... wayland-client>=1.20) (version comparator)",
        ok,
        detail=f"violations={[v.format() for v in report.violations]}"
        f" floor_ok={report.floor_ok} floor_lines={report.floor_lines}",
    )


# Floor controls (GODS_LAWS.md L-40: positive / negative / empty-scan
# are not enough on their own - this file's floors are proven directly).


def selftest_floor_empty_trace_reproves(scratch):
    root = os.path.join(scratch, "floor_empty_trace")
    write_fixture(root, "cmake_minimum_required(VERSION 3.25)\nproject(floor_empty NONE)\n")
    build_dir = os.path.join(root, "build")
    os.makedirs(build_dir, exist_ok=True)
    subprocess.run(["cmake", "-S", root, "-B", build_dir], capture_output=True)
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


def selftest_floor_missing_sentinel_reproves(scratch):
    # A trace that never touches the REAL root's own CMakeLists.txt -
    # built by configuring one fixture, then judging its trace against
    # a DIFFERENT fixture's source_root, so the sentinel's realpath
    # comparison genuinely fails (not simulated).
    real_root = os.path.join(scratch, "floor_sentinel_real")
    write_fixture(real_root, "cmake_minimum_required(VERSION 3.25)\nproject(floor_sentinel_real NONE)\n")
    real_build = os.path.join(real_root, "build")
    os.makedirs(real_build, exist_ok=True)
    subprocess.run(["cmake", "-S", real_root, "-B", real_build], capture_output=True)

    other_root = os.path.join(scratch, "floor_sentinel_other")
    write_fixture(other_root, "cmake_minimum_required(VERSION 3.25)\nproject(floor_sentinel_other NONE)\n")

    trace_scratch = make_scratch_workdir()
    try:
        trace_path = os.path.join(trace_scratch, "trace.json")
        run_configure_with_trace(real_root, real_build, trace_path)
        # Judge the REAL trace, but against OTHER_ROOT's source root -
        # the project() event's file will never match other_root's
        # CMakeLists.txt.
        report = judge(other_root, real_build, trace_path, False)
        ok = not report.floor_ok and any("sentinel" in line for line in report.floor_lines)
        return selftest_report(
            "floor: a trace whose project() event never matches the given"
            " source root reproves by sentinel",
            ok,
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
        selftest_positive_find_package_multiline,
        selftest_positive_pkg_check_modules_version_comparator,
        selftest_floor_empty_trace_reproves,
        selftest_floor_corrupted_line_reproves_by_parity,
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
