#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_dep_zero.py - CI + pre-commit gate for GODS_LAWS.md L-07 (zero
# dependency besides the C++23 standard library and OS APIs).
#
# PORT of the former tests/tools/check_dep_zero.sh (POSIX sh + a
# hand-written single-process awk engine, GATE-DEPZERO-NOFORK,
# 01/09/2026), retired in the same fatia that wrote this file
# (GODS_LAWS.md L-04, decisao do lider: "O comportamento deve ser
# igual em qualquer OS"). The sh version was `if(UNIX)`-guarded in
# tests/CMakeLists.txt and its pre-commit half (tools/git-hooks/
# pre-commit) never ran on Windows at all - the exact gap this port
# closes, the same shape check_spdx.py and check_vendor_purity.py
# already proved: an ordinary ctest case, unguarded, resolved via
# find_program(...NAMES python3 python REQUIRED) the way dep_zero_
# trace already does (tests/CMakeLists.txt).
#
# THE SCRIPT BEING PORTED IS THE SPECIFICATION: every sub-check, every
# allowlist, every advice string, every printed message this file
# emits is ported to preserve, byte for byte where practical, what the
# sh version already decided (GODS_LAWS.md L-07's own history of
# adversarial review findings, cited throughout as *** markers below).
# Two deliberate, DECLARED departures from that "verbatim" rule:
#
# 1. ENUMERATION VIA `git ... -z`, not a hand-written quote decoder.
#    The sh version carried ~100 lines of awk decoding git's C-style
#    quoting of hostile filenames (git-config(1): any byte >= 0x80, or
#    a control byte - tab, newline, double quote, backslash - quoted
#    UNCONDITIONALLY) because a plain `git ls-files`/`git diff
#    --cached --name-only` prints those paths quoted. `-z`
#    (NUL-terminated, never-quoted output - git-ls-files(1), git-
#    diff(1)) sidesteps the whole grammar - the same house pattern
#    check_spdx.py already established for the identical reason (see
#    that file's own header). This is not a change of INTENT (both aim
#    to recognize every tracked/staged path exactly, accented or
#    hostile-named alike) - it removes an entire class of decoder bug
#    by never needing a decoder. Two of the sh version's 39
#    --selftest controls (selftest_decode_rejects_malformed_escapes,
#    selftest_decode_rejects_octal_out_of_range) fed the DECODER
#    itself synthetic malformed escapes; with no decoder left to call,
#    they have no equivalent here BY CONSTRUCTION - declared, not
#    silently dropped. The end-to-end behavior those two controls
#    existed to protect (a hostile or accented real filename, seen and
#    reproved) is still proven, and proven more directly than before,
#    by selftest_negative_control_hostile_filename_tree,
#    selftest_staged_hostile_filename, selftest_negative_control_
#    accented_filename_tree and selftest_staged_accented_filename
#    below - now exercising the REAL git -z pipeline end to end
#    instead of a synthetic decoder in isolation.
#
# 2. SUB-CHECK (c) ON WINDOWS: the sh version's sub-check (c) (`readelf
#    -d`, the built artifact's own DT_NEEDED truth) only ever ran on
#    Linux, gated by the whole file being `if(UNIX)`-only. Making
#    sub-checks (a)/(b) run on Windows (this port's whole point) means
#    tests/CMakeLists.txt must call this script from the `windows` CI
#    job too - but sub-check (c) has NO Linux-shaped equivalent to run
#    there: DEPZERO-PARITY-WIN (GODS_LAWS.md L-04, 03/09/2026) already
#    gave Windows its OWN, separate parity script (tools/ci/check-dep-
#    zero-win.ps1, dumpbin /imports against glintfx.dll), deliberately
#    NOT folded into this file. So a THIRD library-path sentinel,
#    "WINDOWS-SEPARATE" (see check_needed_allowlist() below), is added
#    beside the sh version's existing "NONE" (static mode): it prints
#    its OWN honest, distinct message - never reusing the static-mode
#    text, which would be a lie on a real shared-library Windows build
#    - and is proven by its own NEW selftest control,
#    selftest_needed_windows_separate_skip, added in this port (there
#    are 38 controls below, not 39: the sh version's 39, minus the two
#    retired decoder controls above, plus this one new control for
#    logic that did not exist before this port).
#
# THREE SUB-CHECKS, closed by FORM and POLARITY, never by a growing
# list of forbidden library names (see the sh version's own header,
# preserved in git history, for the full adversarial-review record -
# REVIEW-DEPZERO-GATE.md, 28/08/2026, four CRITICAL findings; DEPZERO-
# SHALLOW, 31/08/2026, the warn-and-defer contract; GATE-DEPZERO-
# NOFORK, 01/09/2026, the single-process engine this file's own
# single-process design descends from):
#
#   (a) CMake surface (every git-tracked/staged CMakeLists.txt,
#       *.cmake, *.cmake.in). Scanned one PHYSICAL LINE at a time,
#       comment stripped before matching. DEPZERO-SHALLOW contract: a
#       call whose decisive evidence does not fit on that one line
#       WARNS and defers to the CI oracle (ctest test dep_zero_trace,
#       tests/tools/check_dep_zero_trace.py) instead of being resolved
#       here. Four shapes:
#         - UNCONDITIONALLY forbidden: include(FetchContent),
#           include(ExternalProject), FetchContent_Declare/
#           MakeAvailable/Populate, ExternalProject_Add, all four of
#           CPMAddPackage/CPMFindPackage/CPMDeclarePackage/
#           CPMGetPackage, any conan_*/vcpkg* call or include()
#           referencing a conan/vcpkg toolchain file, cmake_language()
#           in its entirety (any subcommand), include() whose argument
#           is built ENTIRELY from a variable with no literal ".cmake"
#           fragment anywhere in the statement, and file(DOWNLOAD)/
#           file(UPLOAD).
#         - conditional on FIND_PACKAGE_ALLOWLIST / PKG_CHECK_MODULES_
#           ALLOWLIST below (pkg-config version comparator stripped
#           first).
#         - conditional on EXECUTE_PROCESS_PROGRAM_ALLOWLIST below:
#           execute_process(COMMAND <program> ...) with a LITERAL
#           program name (basename, extension and case stripped).
#       Deliberately NOT done here: parsing target_link_libraries() -
#       the truth of linkage is read from the ARTIFACT (sub-check (c)),
#       never guessed from source text.
#
#   (b) Include surface (every git-tracked/staged .cpp/.cxx/.cc/.hpp/
#       .hxx/.hh/.h/.ipp). A line matching '^\s*#\s*include\s*<...>' is
#       permitted iff: (1) the name has neither '.' nor '/' (every
#       C++23 standard library header shape); or (2) the name is on
#       SO_HEADER_ALLOWLIST below; or (3) the name starts with
#       "glintfx/" AND a file of that exact name exists under
#       <root>/include/ (our OWN public header - STRUCTURAL check,
#       never a static name list; any ".." anywhere in the name is
#       rejected before the filesystem is even touched, REVIEW-DEPZERO
#       -GATE.md achado CRITICO #4). Quote includes (#include "...")
#       are out of scope by construction.
#
#   (c) The artifact's own truth: DT_NEEDED (`readelf -d <lib>`, always
#       under LC_ALL=C). Every NEEDED entry must be on NEEDED_ALLOWLIST
#       below. In static mode the argument is "NONE" and this sub-
#       check is SKIPPED WITH A PRINTED REASON, never silently; on a
#       platform where readelf parity is covered separately (see
#       departure 2 above) the argument is "WINDOWS-SEPARATE", same
#       shape.
#
# GODS_LAWS.md L-40 (non-empty-scan floor), per sub-check:
#   (a) 0 CMake surface files scanned -> REPROVES (tree mode only;
#       staged mode has a DECLARED pass for the legitimate
#       zero-relevant-among-N-staged case).
#   (b) 0 C++ surface files scanned -> REPROVES, same distinction.
#   (c) tree mode, shared library given: 0 NEEDED entries read ->
#       REPROVES. NONE/WINDOWS-SEPARATE are declared skips, not scans.
#   Every passing run prints the counts it scanned.
#
# DECISION D2 of the leader, 28/08/2026: a staged commit whose ONLY
# change is documentation PASSES, declaring both numbers (relevant
# count among staged count). Tree mode stays STRICT: 0 CMake or 0 C++
# files scanned there always reproves.
#
# DECISION D3 of the leader, 28/08/2026: NO environment-variable escape
# hatch. The only legitimate escape is editing the allowlist below,
# which shows up in the diff (proven in --selftest,
# selftest_escape_via_allowlist_edit).
#
# Usage:
#   check_dep_zero.py <source-root-directory> <path-to-.so-or-NONE-or-WINDOWS-SEPARATE>
#   check_dep_zero.py --staged <source-root-directory>
#   check_dep_zero.py --selftest
#
# Each function below does one thing (GODS_LAWS.md L-17).

import os
import re
import shutil
import subprocess
import sys
import tempfile

SCRIPT_NAME = "check_dep_zero.py"

# --- the closed allowlists (GODS_LAWS.md L-40 item 5: enumeration
# closed by CONSTRUCTION) - every entry names WHY it is here, and
# growing any of them is a conscious, reviewable edit of this file. ---

FIND_PACKAGE_ALLOWLIST = frozenset({"PkgConfig", "glintfx"})
# PkgConfig: CMake's own module to locate pkg-config, a build tool -
#   never linked into the artifact.
# glintfx: ourselves, consumed by tests/package/CMakeLists.txt to
#   prove find_package(glintfx) works outside the tree.

PKG_CHECK_MODULES_ALLOWLIST = frozenset({"wayland-client"})
# wayland-client: GODS_LAWS.md L-07 - libwayland-client counts as OS
#   API, same category as Win32.

PKG_CHECK_MODULES_KEYWORDS = frozenset(
    {"REQUIRED", "QUIET", "NO_CMAKE_PATH", "NO_CMAKE_ENVIRONMENT_PATH", "IMPORTED_TARGET", "GLOBAL"}
)
# CMake's own pkg_check_modules() keyword vocabulary - never a module name.

SO_HEADER_ALLOWLIST = frozenset(
    {
        "GL/gl.h",
        "poll.h",
        "sys/prctl.h",
        "sys/stat.h",
        "sys/sysmacros.h",
        "sys/types.h",
        "unistd.h",
        "wayland-client.h",
        "windows.h",
        "xdg-shell-client-protocol.h",
        "glintfx/export.hpp",
        "glintfx/version_macros.hpp",
    }
)
# Complete enumeration measured live in the real tree (sh version's own
# header, preserved in git history, for the two corrections found by
# measuring instead of trusting the plan). xdg-shell-client-protocol.h
# is wayland-scanner GENERATED from the system-installed xdg-shell.xml.
# glintfx/export.hpp and glintfx/version_macros.hpp are ALSO generated
# (generate_export_header()/configure_file()), born only under
# <build>/generated/include/glintfx/ - never on disk in <root>/
# include/, so rule 3's structural existence check cannot see them.

NEEDED_ALLOWLIST = frozenset(
    {"libwayland-client.so.0", "libgcc_s.so.1", "libstdc++.so.6", "libm.so.6", "libc.so.6"}
)
# Measured live against build/src/libglintfx.so - exactly the five
# DT_NEEDED entries this library links today.

EXECUTE_PROCESS_PROGRAM_ALLOWLIST = frozenset({"pkg-config", "pkgconf"})
# The only two program names execute_process() may run without
# STOPPING and asking the leader - the same two the trace oracle's own
# R8 already allows (tests/tools/check_dep_zero_trace.py).

# --- messages the gate ORDERS a fix with, not just reports (English:
# consumer-facing text, GODS_LAWS.md L-21 emenda) ---

FETCH_ADVICE = (
    "REMOVE this block entirely. If the functionality is genuinely needed, STOP "
    "and take it to the project leader (GODS_LAWS.md L-07): the answer is to "
    "write it in-house, never a dependency 'just for now'."
)
FINDPKG_ADVICE = (
    "REMOVE this call. If it is a build tool that never links, or an OS API, "
    "add it to this gate's allowlist WITH a justification comment, and cite "
    "the leader's decision in the commit."
)
PKGCHECK_ADVICE = (
    "REMOVE this call. If it is a build tool that never links, or an OS API, "
    "add it to this gate's allowlist WITH a justification comment, and cite "
    "the leader's decision in the commit."
)
INCLUDE_ADVICE = (
    "REMOVE this include. A new OS API header goes on this gate's allowlist "
    "WITH a justification; a third-party library header has no allowlist fix "
    "- GODS_LAWS.md L-07 says write it in-house."
)
NEEDED_ADVICE = (
    "The binary links this library. Find the link flag that brought it in and "
    "REMOVE it. There is no allowlist fix for third-party linkage without the "
    "leader's order."
)
INDIRECTION_ADVICE = (
    "REMOVE this call. This gate cannot verify what dependency-management code "
    "runs through indirection (cmake_language(), or an include() argument "
    "built entirely from a variable, with no literal filename in it) - "
    "GODS_LAWS.md L-07: STOP and take an opaque, indirect call like this to "
    "the leader; write the target literally instead."
)
FILE_NETWORK_ADVICE = (
    "REMOVE this call. file(DOWNLOAD/UPLOAD) is CMake's own native network "
    "transfer; there is no allowlist fix - GODS_LAWS.md L-07: STOP and take it "
    "to the leader, the answer is to write it in-house, never a dependency "
    "'just for now'."
)
EXECUTE_PROCESS_ADVICE = (
    "REMOVE this call or route it through an allowlisted program. "
    "execute_process() runs an arbitrary program at configure time - "
    "GODS_LAWS.md L-07: STOP and take it to the leader; only pkg-config/"
    "pkgconf are allowlisted here, and any other program is opaque to this "
    "gate by construction."
)

# --- patterns (ported verbatim from the sh version's own awk regexes;
# POSIX [:space:] narrowed to the same explicit set the sh version's
# own is_space_ch() used: space, tab, CR, FF, VT - never newline, which
# cannot occur inside a single already-split line). ---

_WS = r"[ \t\r\f\v]"

CMAKE_SURFACE_RE = re.compile(r"(^|/)CMakeLists\.txt$|\.cmake$|\.cmake\.in$")
CXX_SURFACE_RE = re.compile(r"\.(cpp|cxx|cc|hpp|hxx|hh|h|ipp)$")

CMAKE_FETCH_RE = re.compile(
    rf"^{_WS}*(include{_WS}*\({_WS}*(fetchcontent|externalproject){_WS}*\)"
    rf"|fetchcontent_(declare|makeavailable|populate){_WS}*\("
    rf"|externalproject_add{_WS}*\("
    rf"|cpm(addpackage|findpackage|declarepackage|getpackage){_WS}*\()",
    re.IGNORECASE,
)
# CPM's four PUBLIC entry points - REVIEW-DEPZERO-GATE.md achado
# CRITICO #3: only CPMAddPackage was covered before.

CMAKE_TOOLCHAIN_RE = re.compile(
    rf"^{_WS}*(conan_[a-z_]*{_WS}*\(|vcpkg[a-z_]*{_WS}*\(|include{_WS}*\([^)]*(conan|vcpkg)[^)]*\))",
    re.IGNORECASE,
)

CMAKE_LANGUAGE_RE = re.compile(rf"^{_WS}*cmake_language{_WS}*\(", re.IGNORECASE)
# cmake_language() is blocked UNCONDITIONALLY, never by allowlist -
# REVIEW-DEPZERO-GATE.md achado CRITICO #2: zero legitimate use in
# this tree, and the command exists precisely to dispatch to a NAME
# COMPUTED AT CONFIGURE TIME or execute a STRING of arbitrary CMake
# source, both opaque to static text scanning by construction.

CMAKE_FILE_NETWORK_RE = re.compile(rf"^{_WS}*file{_WS}*\({_WS}*(download|upload)({_WS}|$)", re.IGNORECASE)
# file(DOWNLOAD)/file(UPLOAD): CMake's own native network transfer -
# unconditional block, zero legitimate use measured in this tree.

FILE_OPEN_RE = re.compile(rf"^{_WS}*file{_WS}*\(", re.IGNORECASE)
EXECUTE_PROCESS_OPEN_RE = re.compile(rf"^{_WS}*execute_process{_WS}*\(", re.IGNORECASE)
INCLUDE_OPEN_RE = re.compile(rf"^{_WS}*include{_WS}*\(", re.IGNORECASE)
FIND_PACKAGE_OPEN_RE = re.compile(rf"^{_WS}*find_package{_WS}*\(", re.IGNORECASE)
PKG_CHECK_MODULES_OPEN_RE = re.compile(rf"^{_WS}*pkg_check_modules{_WS}*\(", re.IGNORECASE)
BLANK_LINE_RE = re.compile(rf"^{_WS}*$")

INCLUDE_CONTENT_RE = re.compile(r"^[ \t]*#[ \t]*include[ \t]*<[^>]+>")

SPACE_CHARS = " \t\r\f\v"
COMPARATOR_TOKENS = frozenset({">=", "<=", ">", "<", "="})
_VERSION_COMPARATOR_RE = re.compile(r"(>=|<=|>|<|=).*$")
_STRIP_LEADING_WS_RE = re.compile(rf"^{_WS}+")
_STRIP_FIRST_TOKEN_RE = re.compile(rf"^[^ \t\r\f\v]+")
_RTRIM_CLOSE_PAREN_RE = re.compile(r"\)[ \t]*$")


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


def print_violation_header():
    print(f"{SCRIPT_NAME}: PROHIBITED (GODS_LAWS.md L-07 zero dependency):", file=sys.stderr)


# --- generic helpers -----------------------------------------------------


def strip_cmake_comment(line):
    """Quote-aware: '#' only starts a comment outside a double-quoted
    string - CPM's own documented shorthand puts a real '#' inside one,
    "gh:fmtlib/fmt#1.0"."""
    in_quote = False
    out = []
    for c in line:
        if c == '"':
            in_quote = not in_quote
            out.append(c)
            continue
        if c == "#" and not in_quote:
            break
        out.append(c)
    return "".join(out)


def paren_first_token(line):
    """First token after '(' on THIS line, skipping leading whitespace,
    stopping at whitespace or ')'. Returns "" on a bare opener."""
    idx = line.find("(")
    if idx == -1:
        return ""
    i = idx + 1
    n = len(line)
    while i < n and line[i] in SPACE_CHARS:
        i += 1
    if i >= n or line[i] == ")":
        return ""
    out = []
    while i < n:
        c = line[i]
        if c in SPACE_CHARS or c == ")":
            break
        out.append(c)
        i += 1
    return "".join(out)


def pkgconfig_module_base_name(tok):
    """Strips a glued pkg-config version comparator (wayland-client>=1.20)."""
    return _VERSION_COMPARATOR_RE.sub("", tok)


def strip_first_token(s):
    s = _STRIP_LEADING_WS_RE.sub("", s)
    s = _STRIP_FIRST_TOKEN_RE.sub("", s)
    s = _STRIP_LEADING_WS_RE.sub("", s)
    return s


def rtrim_close_paren(s):
    return _RTRIM_CLOSE_PAREN_RE.sub("", s)


# --- the dep-zero engine: ONE Python process, no subprocess per file
# scanned (GODS_LAWS.md L-11 bloco 6 - the incident that cost four
# hours and a forced reboot). Every git listing crosses via a single
# `git ... -z` subprocess call per mode; every matching file is opened
# directly by THIS process (Python's own open(), never a shell-out per
# file) - cost independent of the number of files or lines, the same
# guarantee GATE-DEPZERO-NOFORK's awk engine established, reached here
# by simply never forking in the first place. ---


class DepZeroEngine:
    def __init__(self, check_root):
        self.check_root = check_root
        self.violations = []
        self.warnings = []
        self.failed_records = []
        self.cmake_found = 0
        self.cmake_analyzed = 0
        self.cxx_found = 0
        self.cxx_analyzed = 0
        self.failed = 0

    def emit_violation(self, disp, lineno, raw, advice):
        self.violations.append(f"{self.check_root}/{disp}:{lineno}: {raw}\n  -> {advice}")

    def emit_warning(self, disp, lineno, raw):
        self.warnings.append(
            f"{SCRIPT_NAME}: WARNING (this is NOT a pass verdict for this call):\n"
            f"{self.check_root}/{disp}:{lineno}: {raw}\n"
            "  -> This call spans lines or builds its decisive argument from a "
            "variable, and this shallow, line-by-line gate cannot judge it. It "
            "is NOT being cleared here: the CI oracle (ctest test dep_zero_trace, "
            "tests/tools/check_dep_zero_trace.py) is the authority that judges "
            "what CMake actually executes, and it WILL reprove a violation there "
            "(GODS_LAWS.md L-07). Leader's decision, 28/08/2026: ambiguous forms "
            "pass the hook with this warning so the CI can decide."
        )

    def emit_open_refused(self, disp):
        self.failed_records.append(
            f"{self.check_root}/{disp}: open refused (file not found in working tree, or unreadable)"
        )


# --- sub-check (a): CMake surface, one physical line at a time ---------


def eval_include_line(engine, disp, lineno, line, raw, closed):
    """include(): a line with no "${" at all is always fully literal and
    always resolves. A line WITH "${" resolves only when it ALSO
    carries a literal ".cmake" fragment (the real, legitimate
    parameterized-file-include shape already used in this tree) AND is
    closed on this same line."""
    if "${" not in line:
        return
    if ".cmake" in line.lower():
        if not closed:
            engine.emit_warning(disp, lineno, raw)
        return
    if closed:
        engine.emit_violation(disp, lineno, raw, INDIRECTION_ADVICE)
    else:
        engine.emit_warning(disp, lineno, raw)


def eval_pkg_check_modules_line(engine, disp, lineno, line, raw, closed):
    """Any BAD token visible on this line blocks regardless of closure.
    With every visible token clean, a CLOSED line passes silently; an
    unclosed one warns."""
    idx = line.find("(")
    content = rtrim_close_paren(line[idx + 1 :])
    rest = strip_first_token(content)
    unknown_hit = False
    for tok in re.split(rf"{_WS}+", rest):
        if tok == "":
            continue
        if tok in PKG_CHECK_MODULES_KEYWORDS:
            continue
        if tok in COMPARATOR_TOKENS:
            continue
        if tok[0].isdigit():
            continue
        base = pkgconfig_module_base_name(tok)
        if base not in PKG_CHECK_MODULES_ALLOWLIST:
            unknown_hit = True
    if unknown_hit:
        engine.emit_violation(disp, lineno, raw, PKGCHECK_ADVICE)
        return
    if not closed:
        engine.emit_warning(disp, lineno, raw)


def find_command_program(line):
    """Program token following the LAST "COMMAND" keyword on this line
    that is immediately followed by whitespace."""
    n = len(line)
    last_pos = -1
    search_from = 0
    while True:
        pos = line.find("COMMAND", search_from)
        if pos == -1:
            break
        after = pos + 7
        if after < n and line[after] in SPACE_CHARS:
            last_pos = pos
        search_from = pos + 1
    if last_pos == -1:
        return ""
    i = last_pos + 7
    while i < n and line[i] in SPACE_CHARS:
        i += 1
    if i >= n:
        return ""
    if line[i] == '"':
        i += 1
    out = []
    while i < n:
        c = line[i]
        if c == '"' or c == ")" or c in SPACE_CHARS:
            break
        out.append(c)
        i += 1
    return "".join(out)


def eval_execute_process_line(engine, disp, lineno, line, raw):
    """execute_process(): a LITERAL program on this line resolves - its
    basename (extension stripped, casefold) decides block vs pass."""
    program = find_command_program(line)
    if program == "" or "${" in program:
        engine.emit_warning(disp, lineno, raw)
        return
    base = program.rsplit("/", 1)[-1]
    lower_base = base.lower()
    for suffix in (".exe", ".bat", ".cmd"):
        if lower_base.endswith(suffix):
            lower_base = lower_base[: -len(suffix)]
            break
    if lower_base not in EXECUTE_PROCESS_PROGRAM_ALLOWLIST:
        engine.emit_violation(disp, lineno, raw, EXECUTE_PROCESS_ADVICE)


def eval_cmake_line(engine, disp, lineno, line, raw):
    """Evaluates ONE physical CMake line, comment already stripped. The
    DEPZERO-SHALLOW contract: what resolves on THIS physical line
    blocks; what does not (multi-line, variable-built) warns and
    defers to the CI oracle. Never both, never silent."""
    if BLANK_LINE_RE.match(line):
        return
    closed = ")" in line

    if CMAKE_FETCH_RE.match(line):
        engine.emit_violation(disp, lineno, raw, FETCH_ADVICE)
        return
    if CMAKE_TOOLCHAIN_RE.match(line):
        engine.emit_violation(disp, lineno, raw, FETCH_ADVICE)
        return
    if CMAKE_LANGUAGE_RE.match(line):
        engine.emit_violation(disp, lineno, raw, INDIRECTION_ADVICE)
        return
    if CMAKE_FILE_NETWORK_RE.match(line):
        engine.emit_violation(disp, lineno, raw, FILE_NETWORK_ADVICE)
        return
    if FILE_OPEN_RE.match(line):
        if paren_first_token(line) == "":
            engine.emit_warning(disp, lineno, raw)
        return
    if EXECUTE_PROCESS_OPEN_RE.match(line):
        eval_execute_process_line(engine, disp, lineno, line, raw)
        return
    if INCLUDE_OPEN_RE.match(line):
        eval_include_line(engine, disp, lineno, line, raw, closed)
        return
    if FIND_PACKAGE_OPEN_RE.match(line):
        name = paren_first_token(line)
        if name == "":
            engine.emit_warning(disp, lineno, raw)
            return
        if name not in FIND_PACKAGE_ALLOWLIST:
            engine.emit_violation(disp, lineno, raw, FINDPKG_ADVICE)
        return
    if PKG_CHECK_MODULES_OPEN_RE.match(line):
        eval_pkg_check_modules_line(engine, disp, lineno, line, raw, closed)
        return


def _read_lines_latin1(fname):
    """Opens fname in binary mode and yields (lineno, text) pairs, one
    per physical line with the trailing '\\n' stripped. latin-1
    (byte<->codepoint 1:1) mirrors the sh engine's own LC_ALL=C,
    byte-per-character treatment - never raises on non-UTF-8 content,
    and every ASCII pattern this file matches against still matches
    identically under latin-1 decoding."""
    lineno = 0
    with open(fname, "rb") as handle:
        for raw_bytes in handle:
            lineno += 1
            line = raw_bytes.decode("latin-1")
            if line.endswith("\n"):
                line = line[:-1]
            yield lineno, line


def scan_cmake_file(engine, content_root, path, disp):
    fname = os.path.join(content_root, *path.split("/"))
    try:
        for lineno, line in _read_lines_latin1(fname):
            eval_cmake_line(engine, disp, lineno, strip_cmake_comment(line), line)
    except OSError:
        engine.emit_open_refused(disp)
        return False
    return True


# --- sub-check (b): include surface, one line of CONTENT at a time -----


def extract_angle_include_name(line):
    i = line.find("<")
    if i == -1:
        return ""
    j = line.find(">", i + 1)
    if j == -1:
        return ""
    return line[i + 1 : j]


def probe_file_exists(fname):
    """Structural existence probe for rule 3: an empty file still
    counts as existing; a directory or missing path do not - the same
    verdict a plain stat-based test would give, checked by actually
    opening the path (never fooled by a symlink into believing a
    non-regular entry is absent when it is not)."""
    try:
        with open(fname, "rb"):
            return True
    except OSError:
        return False


def eval_include_content_line(engine, check_root, disp, lineno, line):
    if not INCLUDE_CONTENT_RE.match(line):
        return
    name = extract_angle_include_name(line)
    if name == "":
        return

    has_dot_or_slash = "." in name or "/" in name
    if not has_dot_or_slash:
        return

    if name in SO_HEADER_ALLOWLIST:
        return

    if name.startswith("glintfx/"):
        # REVIEW-DEPZERO-GATE.md achado CRITICO #4: reject ANY ".."
        # occurrence before ever touching the filesystem.
        if ".." not in name:
            probe_path = os.path.join(check_root, "include", *name.split("/"))
            if probe_file_exists(probe_path):
                return

    engine.emit_violation(disp, lineno, line, INCLUDE_ADVICE)


def scan_cxx_file(engine, content_root, path, disp):
    fname = os.path.join(content_root, *path.split("/"))
    try:
        for lineno, line in _read_lines_latin1(fname):
            eval_include_content_line(engine, engine.check_root, disp, lineno, line)
    except OSError:
        engine.emit_open_refused(disp)
        return False
    return True


def run_dep_zero_engine(check_root, content_root, paths):
    engine = DepZeroEngine(check_root)
    for path in paths:
        if path == "":
            continue
        disp = path
        if CMAKE_SURFACE_RE.search(path):
            engine.cmake_found += 1
            if scan_cmake_file(engine, content_root, path, disp):
                engine.cmake_analyzed += 1
            else:
                engine.failed += 1
        if CXX_SURFACE_RE.search(path):
            engine.cxx_found += 1
            if scan_cxx_file(engine, content_root, path, disp):
                engine.cxx_analyzed += 1
            else:
                engine.failed += 1
    return engine


# --- git enumeration, `-z` throughout (see this file's own header,
# departure 1): a CONSTANT number of git subprocess calls per mode,
# never one per file. ---


def git_ls_files_z(root, extra_args):
    """Runs `git -C root ls-files -z <extra_args>`. Returns (paths, ok);
    ok is False when git itself failed (not a repository, or git
    missing) - the caller turns that into an explicit, named refusal,
    never a silent empty result."""
    try:
        result = subprocess.run(["git", "-C", root, "ls-files", "-z", *extra_args], capture_output=True)
    except FileNotFoundError:
        return [], False
    if result.returncode != 0:
        return [], False
    raw = result.stdout
    if not raw:
        return [], True
    if raw.endswith(b"\0"):
        raw = raw[:-1]
    encoding = sys.getfilesystemencoding()
    return [chunk.decode(encoding, errors="surrogateescape") for chunk in raw.split(b"\0")], True


def git_diff_cached_name_only_z_raw(root):
    """Runs `git -C root diff --cached --name-only -z --diff-filter=ACMR`
    once, returning the RAW NUL-terminated bytes (reused both to build
    the path list and, unmodified, as `git checkout-index -z --stdin`'s
    own input - one subprocess call doing double duty, never per-file).
    Returns (raw_bytes, ok)."""
    try:
        result = subprocess.run(
            ["git", "-C", root, "diff", "--cached", "--name-only", "-z", "--diff-filter=ACMR"],
            capture_output=True,
        )
    except FileNotFoundError:
        return b"", False
    if result.returncode != 0:
        return b"", False
    return result.stdout, True


def decode_z_listing(raw):
    if not raw:
        return []
    body = raw[:-1] if raw.endswith(b"\0") else raw
    encoding = sys.getfilesystemencoding()
    return [chunk.decode(encoding, errors="surrogateescape") for chunk in body.split(b"\0")]


def is_git_repo(root):
    try:
        result = subprocess.run(
            ["git", "-C", root, "rev-parse", "--is-inside-work-tree"], capture_output=True
        )
    except FileNotFoundError:
        return False
    return result.returncode == 0


def materialize_staged_index(root, tmp_root, listing_raw):
    """Materializes the INDEX content (never the working tree,
    GODS_LAWS.md L-12) into tmp_root via `git checkout-index -z
    --stdin`, fed the SAME -z listing already obtained - one further
    subprocess call, constant cost regardless of N."""
    try:
        result = subprocess.run(
            ["git", "-C", root, "checkout-index", "-z", "--stdin", f"--prefix={tmp_root}{os.sep}"],
            input=listing_raw,
            capture_output=True,
        )
    except FileNotFoundError:
        return False
    return result.returncode == 0


# --- sub-check (c): the built artifact's own DT_NEEDED truth -----------


def needed_entries_of(library_path):
    env = dict(os.environ)
    env["LC_ALL"] = "C"
    result = subprocess.run(["readelf", "-d", library_path], capture_output=True, env=env)
    text = result.stdout.decode("latin-1", errors="replace")
    needed = []
    for line in text.splitlines():
        m = re.search(r"\(NEEDED\).*\[(.*)\]", line)
        if m:
            needed.append(m.group(1))
    return needed


def check_needed_allowlist(library_path, allowlist):
    """Returns (ok, message_text). The CALLER decides whether
    message_text goes to stdout (overall pass) or stderr (overall
    reprove) - mirrors the sh version's own `2>&1`-captured,
    caller-routed output (check_dep_zero_tree/_staged below)."""
    if library_path == "NONE":
        return True, (
            f"{SCRIPT_NAME}: (c) skipped - BUILD_SHARED_LIBS=OFF, a static "
            "archive has no dynamic symbol table / NEEDED entries to inspect"
        )
    if library_path == "WINDOWS-SEPARATE":
        return True, (
            f"{SCRIPT_NAME}: (c) skipped on this platform - sub-check (c) parity "
            "here is covered separately by tools/ci/check-dep-zero-win.ps1 "
            "(dumpbin /imports against glintfx.dll), not duplicated in this file "
            "(GODS_LAWS.md L-04, DEPZERO-PARITY-WIN, 03/09/2026)"
        )

    if not os.path.isfile(library_path):
        fail(f"shared library not found: {library_path}")
    if shutil.which("readelf") is None:
        fail("'readelf' not found in PATH (no silent skip)")

    needed = needed_entries_of(library_path)
    if not needed:
        return False, f"{SCRIPT_NAME}: empty scan (0 NEEDED entries in {library_path}) - GODS_LAWS.md L-40"

    violations = [f"{library_path}: {lib}\n  -> {NEEDED_ADVICE}" for lib in needed if lib not in allowlist]
    if violations:
        lines = [f"{SCRIPT_NAME}: PROHIBITED (GODS_LAWS.md L-07 zero dependency):"]
        lines.extend(violations)
        return False, "\n".join(lines)

    return True, f"{SCRIPT_NAME}: (c) {len(needed)} dynamic NEEDED entr(y/ies) scanned in {library_path}, all allowed"


# --- tree mode (CI shape): every git-tracked file, all 3 sub-checks ----


def check_dep_zero_tree(root, library_path):
    paths, ok = git_ls_files_z(root, [])
    if not ok:
        print(
            f"{SCRIPT_NAME}: 'git ls-files' failed in {root} (not a git repository, "
            "or git unavailable) - scan refused, never assumed empty",
            file=sys.stderr,
        )
        return False

    engine = run_dep_zero_engine(root, root, paths)

    if engine.cmake_found == 0:
        print(f"{SCRIPT_NAME}: empty scan (0 CMake surface files) - GODS_LAWS.md L-40", file=sys.stderr)
        return False
    if engine.cxx_found == 0:
        print(f"{SCRIPT_NAME}: empty scan (0 C++ surface files) - GODS_LAWS.md L-40", file=sys.stderr)
        return False

    scan_failed = (
        engine.failed != 0
        or engine.cmake_found != engine.cmake_analyzed
        or engine.cxx_found != engine.cxx_analyzed
    )

    needed_ok, needed_output = check_needed_allowlist(library_path, NEEDED_ALLOWLIST)
    warning_total = len(engine.warnings)

    if scan_failed or engine.violations or not needed_ok:
        print_violation_header()
        for v in engine.violations:
            print(v, file=sys.stderr)
        if not needed_ok:
            print(needed_output, file=sys.stderr)
        if scan_failed:
            print(
                f"{SCRIPT_NAME}: scan incomplete - {engine.failed} file(s) refused to "
                f"open, cmake {engine.cmake_analyzed}/{engine.cmake_found} analyzed, "
                f"c++ {engine.cxx_analyzed}/{engine.cxx_found} analyzed (GODS_LAWS.md "
                "L-40 fail-closed)",
                file=sys.stderr,
            )
            for rec in engine.failed_records:
                print(rec, file=sys.stderr)
        for w in engine.warnings:
            print(w, file=sys.stderr)
        print(
            f"{SCRIPT_NAME}: {engine.cmake_found} cmake file(s), {engine.cxx_found} "
            "c++ file(s) found (scan reproved, see above)",
            file=sys.stderr,
        )
        return False

    for w in engine.warnings:
        print(w, file=sys.stderr)
    print(
        f"{SCRIPT_NAME}: 0 violation(s), {warning_total} warning(s) deferred to the "
        f"CI oracle (dep_zero_trace) - {engine.cmake_found} cmake file(s), "
        f"{engine.cxx_found} c++ file(s) scanned"
    )
    print(needed_output)
    print(
        f"{SCRIPT_NAME}: out of scope by design, and not verified by any other gate "
        'either: documents (.md), shell scripts (.sh), and quote includes (#include '
        '"...") are not scanned here. check_spdx.py only checks for the PRESENCE of '
        "an SPDX header string in a file (a vendored third-party file with that "
        "string pasted in would still pass it); check_vendor_purity.py only guards "
        "the one named third_party/khronos/ exception from growing, not vendoring "
        "in general. Neither closes the quote-include vendor vector - this gate "
        "does not either."
    )
    print(
        f"{SCRIPT_NAME}: ambiguous multi-line or variable-built calls pass this "
        "shallow, line-by-line gate with a printed warning; the CI oracle (ctest "
        "test dep_zero_trace, tests/tools/check_dep_zero_trace.py) is the authority "
        "that judges what CMake actually executes."
    )
    return True


# --- staged mode (pre-commit shape): index content, sub-checks (a)/(b) -


def check_dep_zero_staged(root):
    if not is_git_repo(root):
        print(f"{SCRIPT_NAME}: not a git repository: {root}", file=sys.stderr)
        return False

    listing_raw, ok = git_diff_cached_name_only_z_raw(root)
    if not ok:
        print(f"{SCRIPT_NAME}: 'git diff --cached' failed in {root}", file=sys.stderr)
        return False
    staged = decode_z_listing(listing_raw)
    staged_count = len(staged)

    tmp_root = tempfile.mkdtemp(
        prefix="glintfx-dep-zero-staged-", dir=os.environ.get("TMPDIR", tempfile.gettempdir())
    )
    try:
        if not materialize_staged_index(root, tmp_root, listing_raw):
            print(
                f"{SCRIPT_NAME}: 'git checkout-index' failed materializing staged content in {root}",
                file=sys.stderr,
            )
            return False
        engine = run_dep_zero_engine(root, tmp_root, staged)
    finally:
        shutil.rmtree(tmp_root, ignore_errors=True)

    relevant_count = engine.cmake_found + engine.cxx_found
    if relevant_count == 0:
        print(
            f"{SCRIPT_NAME}: 0 relevant file(s) among {staged_count} staged file(s); "
            "CMake/C++ surfaces untouched by this commit (GODS_LAWS.md L-40: "
            "declared, not silent)"
        )
        return True

    scan_failed = (
        engine.failed != 0
        or engine.cmake_found != engine.cmake_analyzed
        or engine.cxx_found != engine.cxx_analyzed
    )
    warning_total = len(engine.warnings)

    if scan_failed or engine.violations:
        print_violation_header()
        for v in engine.violations:
            print(v, file=sys.stderr)
        if scan_failed:
            print(
                f"{SCRIPT_NAME}: scan incomplete - {engine.failed} file(s) refused to "
                f"open, cmake {engine.cmake_analyzed}/{engine.cmake_found} analyzed, "
                f"c++ {engine.cxx_analyzed}/{engine.cxx_found} analyzed (GODS_LAWS.md "
                "L-40 fail-closed)",
                file=sys.stderr,
            )
            for rec in engine.failed_records:
                print(rec, file=sys.stderr)
        for w in engine.warnings:
            print(w, file=sys.stderr)
        print(
            f"{SCRIPT_NAME}: {engine.cmake_found} cmake file(s), {engine.cxx_found} "
            f"c++ file(s) found among {staged_count} staged file(s) (scan reproved, "
            "see above)",
            file=sys.stderr,
        )
        return False

    for w in engine.warnings:
        print(w, file=sys.stderr)
    print(
        f"{SCRIPT_NAME}: 0 violation(s), {warning_total} warning(s) deferred to the "
        f"CI oracle (dep_zero_trace) - {engine.cmake_found} cmake file(s), "
        f"{engine.cxx_found} c++ file(s) among {staged_count} staged file(s) scanned"
    )
    return True


# --- real mode -------------------------------------------------------------


def real_main(args):
    if args and args[0] == "--staged":
        if len(args) != 2:
            fail("usage: check_dep_zero.py --staged <source-root-directory>")
        root = args[1]
        if not os.path.isdir(root):
            fail(f"directory not found: {root}")
        if not check_dep_zero_staged(root):
            fail("L-07 zero dependency violation found in staged changes (see message above)")
        return

    if len(args) != 2:
        fail("usage: check_dep_zero.py <source-root-directory> <path-to-.so-or-NONE>")
    root, library_path = args
    if not os.path.isdir(root):
        fail(f"directory not found: {root}")
    if not check_dep_zero_tree(root, library_path):
        fail("L-07 zero dependency violation found (see message above)")


# --- fixtures and controls for --selftest -----------------------------------


def make_scratch_workdir():
    return tempfile.mkdtemp(
        prefix="glintfx-dep-zero-selftest-", dir=os.environ.get("TMPDIR", tempfile.gettempdir())
    )


def _write(path, content):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(content)


def make_clean_fixture(root):
    """Minimal fixture mirroring the REAL tree's shape - if this
    control fails, the gate would scream against our own toolchain."""
    if os.path.isdir(root):
        shutil.rmtree(root)
    _write(
        os.path.join(root, "CMakeLists.txt"),
        "cmake_minimum_required(VERSION 3.25)\nproject(fixture)\nadd_subdirectory(src)\n",
    )
    _write(
        os.path.join(root, "cmake", "Wayland.cmake"),
        "# a consumer may embed glintfx via FetchContent, see docs/\n"
        "find_package(PkgConfig REQUIRED)\n"
        "pkg_check_modules(FixtureWayland REQUIRED wayland-client)\n",
    )
    _write(os.path.join(root, "include", "glintfx", "core", "err.hpp"), "#pragma once\n")
    _write(
        os.path.join(root, "src", "core", "err.cpp"),
        "#include <vector>\n#include <wayland-client.h>\n#include <windows.h>\n"
        "#include <glintfx/core/err.hpp>\nint f() { return 0; }\n",
    )
    _write(os.path.join(root, "src", "CMakeLists.txt"), "")


def git_init_fixture(root):
    subprocess.run(["git", "-C", root, "init", "-q"], check=True)
    subprocess.run(["git", "-C", root, "config", "user.email", "selftest@example.invalid"], check=True)
    subprocess.run(["git", "-C", root, "config", "user.name", "selftest"], check=True)
    subprocess.run(["git", "-C", root, "add", "-A"], check=True)
    subprocess.run(["git", "-C", root, "commit", "-q", "-m", "fixture"], check=True)


def _make_capture():
    """Returns a `capture(fn)` helper that runs fn() with stdout AND
    stderr redirected to an in-memory buffer, and hands back both the
    boolean result and everything printed - the "output" the sh
    version's `output="$(... 2>&1)"` pattern captured. Same shape
    check_spdx.py/check_vendor_purity.py already established."""
    import contextlib
    import io

    class _Captured:
        __slots__ = ("result", "text")

        def __init__(self, result, text):
            self.result = result
            self.text = text

    def capture(fn):
        buffer = io.StringIO()
        with contextlib.redirect_stdout(buffer), contextlib.redirect_stderr(buffer):
            result = fn()
        return _Captured(result, buffer.getvalue())

    return capture


def selftest_positive_control(scratch, capture):
    root = os.path.join(scratch, "positive")
    make_clean_fixture(root)
    git_init_fixture(root)

    out = capture(lambda: check_dep_zero_tree(root, "NONE"))
    if not out.result:
        print("selftest: POSITIVE control FAILED (clean fixture should have passed)", file=sys.stderr)
        print(out.text, file=sys.stderr)
        return False
    if "0 warning(s)" not in out.text:
        print("selftest: POSITIVE control FAILED (passed, but did not declare '0 warning(s)')", file=sys.stderr)
        print(out.text, file=sys.stderr)
        return False
    print(
        "selftest: POSITIVE control OK (clean fixture, comment mentioning "
        "FetchContent, allowlisted find_package/pkg_check_modules, own-header "
        "include all passed, 0 warning(s) declared)"
    )
    return True


def selftest_negative_control_cmake(scratch, capture):
    root = os.path.join(scratch, "negative-cmake")
    status = True
    cases = {
        "fetchcontent": ("include(FetchContent)\nFetchContent_Declare(fmt GIT_REPOSITORY x)\n", "FetchContent_Declare"),
        "externalproject": ("include(ExternalProject)\nExternalProject_Add(fmt URL x)\n", "ExternalProject_Add"),
        "cpm": ('CPMAddPackage("gh:fmtlib/fmt#1.0")\n', "CPMAddPackage"),
        "conan_toolchain": ("conan_cmake_run(REQUIRES fmt/1.0)\n", "conan_cmake_run"),
        "vcpkg_toolchain": ("include(${CMAKE_BINARY_DIR}/vcpkg-toolchain.cmake)\n", "vcpkg-toolchain"),
        "findpkg_unknown": ("find_package(Freetype REQUIRED)\n", "find_package(Freetype"),
        "pkgcheck_unknown": ("pkg_check_modules(Fixture REQUIRED freetype2)\n", "freetype2"),
    }
    for case_name, (addition, needle) in cases.items():
        make_clean_fixture(root)
        with open(os.path.join(root, "cmake", "Wayland.cmake"), "a", encoding="utf-8", newline="\n") as h:
            h.write(addition)
        git_init_fixture(root)

        out = capture(lambda: check_dep_zero_tree(root, "NONE"))
        if out.result:
            print(f"selftest: NEGATIVE(cmake/{case_name}) control FAILED (should have been reproved)", file=sys.stderr)
            status = False
        elif needle not in out.text:
            print(f"selftest: NEGATIVE(cmake/{case_name}) control FAILED (reproved, but did not cite '{needle}')", file=sys.stderr)
            print(out.text, file=sys.stderr)
            status = False
        shutil.rmtree(root, ignore_errors=True)

    if status:
        print("selftest: NEGATIVE(cmake) control OK (seven forms, each reproved and cited)")
    return status


def selftest_negative_control_include(scratch, capture):
    root = os.path.join(scratch, "negative-include")
    status = True
    cases = {
        "zlib": ("#include <zlib.h>\n", "zlib.h"),
        "boost_quote": ("#include <boost/any.hpp>\n", "boost/any.hpp"),
        "png_dot": ("#include <png.h>\n", "png.h"),
    }
    for case_name, (addition, needle) in cases.items():
        make_clean_fixture(root)
        with open(os.path.join(root, "src", "core", "err.cpp"), "a", encoding="utf-8", newline="\n") as h:
            h.write(addition)
        git_init_fixture(root)

        out = capture(lambda: check_dep_zero_tree(root, "NONE"))
        if out.result:
            print(f"selftest: NEGATIVE(include/{case_name}) control FAILED (should have been reproved)", file=sys.stderr)
            status = False
        elif needle not in out.text:
            print(f"selftest: NEGATIVE(include/{case_name}) control FAILED (reproved, but did not cite '{needle}')", file=sys.stderr)
            print(out.text, file=sys.stderr)
            status = False
        shutil.rmtree(root, ignore_errors=True)

    if status:
        print("selftest: NEGATIVE(include) control OK (three forms, each reproved and cited)")
    return status


def selftest_warn_control_cmake_multiline(scratch, capture):
    root = os.path.join(scratch, "warn-cmake-multiline")
    status = True
    cases = {
        "pkgcheck_multiline": "pkg_check_modules(\n    Fixture\n    REQUIRED\n    freetype2)\n",
        "pkgcheck_multiline_comment": "pkg_check_modules(\n    Fixture\n    # module list follows\n    REQUIRED\n    freetype2\n)\n",
        "pkgcheck_multiline_mixedcase": "Pkg_Check_Modules (\n    Fixture\n    REQUIRED\n    freetype2\n)\n",
        "findpkg_multiline_unknown": "find_package(\n    Freetype\n    REQUIRED\n)\n",
    }
    for case_name, addition in cases.items():
        make_clean_fixture(root)
        with open(os.path.join(root, "cmake", "Wayland.cmake"), "a", encoding="utf-8", newline="\n") as h:
            h.write(addition)
        git_init_fixture(root)

        out = capture(lambda: check_dep_zero_tree(root, "NONE"))
        if not out.result:
            print(f"selftest: WARN(cmake-multiline/{case_name}) control FAILED (must PASS with a warning)", file=sys.stderr)
            print(out.text, file=sys.stderr)
            status = False
        elif "WARNING" not in out.text or "deferred" not in out.text or "Wayland.cmake" not in out.text:
            print(f"selftest: WARN(cmake-multiline/{case_name}) control FAILED (missing WARNING/deferred/Wayland.cmake)", file=sys.stderr)
            print(out.text, file=sys.stderr)
            status = False
        elif "PROHIBITED" in out.text:
            print(f"selftest: WARN(cmake-multiline/{case_name}) control FAILED (also printed PROHIBITED)", file=sys.stderr)
            print(out.text, file=sys.stderr)
            status = False
        shutil.rmtree(root, ignore_errors=True)

    if status:
        print("selftest: WARN(cmake-multiline) control OK (four multi-line/format-varied forms, each passes with a printed warning)")
    return status


def selftest_positive_control_cmake_multiline(scratch, capture):
    root = os.path.join(scratch, "positive-cmake-multiline")
    make_clean_fixture(root)
    _write(
        os.path.join(root, "cmake", "Wayland.cmake"),
        "find_package(\n    PkgConfig\n    REQUIRED\n)\npkg_check_modules(\n    FixtureWayland\n    REQUIRED\n    wayland-client\n)\n",
    )
    git_init_fixture(root)

    out = capture(lambda: check_dep_zero_tree(root, "NONE"))
    if not out.result:
        print("selftest: POSITIVE(cmake-multiline) control FAILED", file=sys.stderr)
        print(out.text, file=sys.stderr)
        return False
    if "WARNING" not in out.text:
        print("selftest: POSITIVE(cmake-multiline) control FAILED (bare opener should have warned)", file=sys.stderr)
        print(out.text, file=sys.stderr)
        return False
    print("selftest: POSITIVE(cmake-multiline) control OK (find_package/pkg_check_modules multi-line, bare opener, pass with deferred warning)")
    return True


def selftest_positive_control_cmake_opener_resolved(scratch, capture):
    root = os.path.join(scratch, "positive-cmake-opener-resolved")
    make_clean_fixture(root)
    _write(
        os.path.join(root, "cmake", "Wayland.cmake"),
        "find_package(PkgConfig\n    REQUIRED\n)\npkg_check_modules(FixtureWayland REQUIRED wayland-client)\n",
    )
    git_init_fixture(root)

    out = capture(lambda: check_dep_zero_tree(root, "NONE"))
    if not out.result:
        print("selftest: POSITIVE(cmake-opener-resolved) control FAILED", file=sys.stderr)
        print(out.text, file=sys.stderr)
        return False
    if "WARNING" in out.text:
        print("selftest: POSITIVE(cmake-opener-resolved) control FAILED (opener already resolved should not warn)", file=sys.stderr)
        print(out.text, file=sys.stderr)
        return False
    print("selftest: POSITIVE(cmake-opener-resolved) control OK (opener carries decisive evidence, resolves without warning)")
    return True


def selftest_positive_control_pkgcheck_version(scratch, capture):
    root = os.path.join(scratch, "positive-pkgcheck-version")
    status = True
    for case_name, content in (
        ("glued", "pkg_check_modules(FixtureWayland REQUIRED wayland-client>=1.20)\n"),
        ("spaced", "pkg_check_modules(FixtureWayland REQUIRED wayland-client >= 1.20)\n"),
    ):
        make_clean_fixture(root)
        _write(os.path.join(root, "cmake", "Wayland.cmake"), content)
        git_init_fixture(root)
        out = capture(lambda: check_dep_zero_tree(root, "NONE"))
        if not out.result:
            print(f"selftest: POSITIVE(pkgcheck-version/{case_name}) control FAILED", file=sys.stderr)
            print(out.text, file=sys.stderr)
            status = False
        shutil.rmtree(root, ignore_errors=True)
    if status:
        print("selftest: POSITIVE(pkgcheck-version) control OK (glued and spaced version comparator, allowed module, pass)")
    return status


def selftest_negative_control_pkgcheck_version_unknown(scratch, capture):
    root = os.path.join(scratch, "negative-pkgcheck-version-unknown")
    make_clean_fixture(root)
    _write(os.path.join(root, "cmake", "Wayland.cmake"), "pkg_check_modules(Fixture REQUIRED freetype2>=2.10)\n")
    git_init_fixture(root)

    out = capture(lambda: check_dep_zero_tree(root, "NONE"))
    if out.result:
        print("selftest: NEGATIVE(pkgcheck-version-unknown) control FAILED", file=sys.stderr)
        return False
    if "freetype2" not in out.text:
        print("selftest: NEGATIVE(pkgcheck-version-unknown) control FAILED (did not cite freetype2)", file=sys.stderr)
        print(out.text, file=sys.stderr)
        return False
    print("selftest: NEGATIVE(pkgcheck-version-unknown) control OK (unknown module with version still reproves)")
    return True


def selftest_negative_control_cmake_indirection(scratch, capture):
    root = os.path.join(scratch, "negative-cmake-indirection")
    status = True
    cases = {
        "include_bare_var": ('set(mod_name "FetchContent")\ninclude(${mod_name})\n', "include(${mod_name})"),
        "cmake_language_call": (
            'set(fn_name "FetchContent_Declare")\ncmake_language(CALL ${fn_name} fmt GIT_REPOSITORY https://example.invalid/fmt.git)\n',
            "cmake_language",
        ),
        "cmake_language_eval": ('cmake_language(EVAL CODE "include(FetchContent)")\n', "cmake_language"),
        "mixedcase_call": ("Cmake_Language(CALL FetchContent_Declare fmt)\n", "Cmake_Language"),
        "spaced_call": ("cmake_language (CALL FetchContent_Declare fmt)\n", "cmake_language"),
    }
    for case_name, (addition, needle) in cases.items():
        make_clean_fixture(root)
        with open(os.path.join(root, "cmake", "Wayland.cmake"), "a", encoding="utf-8", newline="\n") as h:
            h.write(addition)
        git_init_fixture(root)

        out = capture(lambda: check_dep_zero_tree(root, "NONE"))
        if out.result:
            print(f"selftest: NEGATIVE(cmake-indirection/{case_name}) control FAILED", file=sys.stderr)
            status = False
        elif needle.lower() not in out.text.lower():
            print(f"selftest: NEGATIVE(cmake-indirection/{case_name}) control FAILED (did not cite '{needle}')", file=sys.stderr)
            print(out.text, file=sys.stderr)
            status = False
        shutil.rmtree(root, ignore_errors=True)

    if status:
        print("selftest: NEGATIVE(cmake-indirection) control OK (five indirection forms, each reproved and cited)")
    return status


def selftest_positive_control_cmake_indirection_legit(scratch, capture):
    root = os.path.join(scratch, "positive-cmake-indirection-legit")
    make_clean_fixture(root)
    _write(
        os.path.join(root, "cmake", "Wayland.cmake"),
        'find_package(PkgConfig REQUIRED)\npkg_check_modules(FixtureWayland REQUIRED wayland-client)\ninclude("${CMAKE_CURRENT_LIST_DIR}/glintfxTargets.cmake")\n',
    )
    git_init_fixture(root)

    out = capture(lambda: check_dep_zero_tree(root, "NONE"))
    if not out.result:
        print("selftest: POSITIVE(cmake-indirection-legit) control FAILED (parameterized include with literal .cmake suffix must pass)", file=sys.stderr)
        print(out.text, file=sys.stderr)
        return False
    print("selftest: POSITIVE(cmake-indirection-legit) control OK (parameterized include with literal .cmake suffix passes)")
    return True


def selftest_negative_control_cpm_variants(scratch, capture):
    root = os.path.join(scratch, "negative-cpm-variants")
    status = True
    for fn in ("CPMFindPackage", "CPMDeclarePackage", "CPMGetPackage"):
        make_clean_fixture(root)
        with open(os.path.join(root, "cmake", "Wayland.cmake"), "a", encoding="utf-8", newline="\n") as h:
            h.write(f"{fn}(NAME fmt GIT_REPOSITORY https://example.invalid/fmt.git GIT_TAG 1.0)\n")
        git_init_fixture(root)
        out = capture(lambda: check_dep_zero_tree(root, "NONE"))
        if out.result:
            print(f"selftest: NEGATIVE(cpm-variants/{fn}) control FAILED", file=sys.stderr)
            status = False
        elif fn.lower() not in out.text.lower():
            print(f"selftest: NEGATIVE(cpm-variants/{fn}) control FAILED (did not cite '{fn}')", file=sys.stderr)
            print(out.text, file=sys.stderr)
            status = False
        shutil.rmtree(root, ignore_errors=True)
    if status:
        print("selftest: NEGATIVE(cpm-variants) control OK (CPMFindPackage/CPMDeclarePackage/CPMGetPackage, each reproved and cited)")
    return status


def selftest_negative_control_opener_carries_bad_token(scratch, capture):
    root = os.path.join(scratch, "negative-opener-bad-token")
    status = True
    cases = {
        "pkgcheck_unclosed_bad": ("pkg_check_modules(Fixture REQUIRED freetype2\n", "freetype2"),
        "findpkg_unclosed_bad": ("find_package(Freetype\n", "find_package(Freetype"),
    }
    for case_name, (addition, needle) in cases.items():
        make_clean_fixture(root)
        with open(os.path.join(root, "cmake", "Wayland.cmake"), "a", encoding="utf-8", newline="\n") as h:
            h.write(addition)
        git_init_fixture(root)
        out = capture(lambda: check_dep_zero_tree(root, "NONE"))
        if out.result:
            print(f"selftest: NEGATIVE(opener-bad-token/{case_name}) control FAILED", file=sys.stderr)
            status = False
        elif needle not in out.text:
            print(f"selftest: NEGATIVE(opener-bad-token/{case_name}) control FAILED (did not cite '{needle}')", file=sys.stderr)
            print(out.text, file=sys.stderr)
            status = False
        shutil.rmtree(root, ignore_errors=True)
    if status:
        print("selftest: NEGATIVE(opener-bad-token) control OK (unclosed calls with a bad token already visible reprove)")
    return status


def selftest_negative_control_file_network(scratch, capture):
    root = os.path.join(scratch, "negative-file-network")
    status = True
    cases = {
        "download": ("file(DOWNLOAD https://example.invalid/x o)\n", "DOWNLOAD"),
        "upload": ("file(UPLOAD o https://example.invalid/x)\n", "UPLOAD"),
    }
    for case_name, (addition, needle) in cases.items():
        make_clean_fixture(root)
        with open(os.path.join(root, "cmake", "Wayland.cmake"), "a", encoding="utf-8", newline="\n") as h:
            h.write(addition)
        git_init_fixture(root)
        out = capture(lambda: check_dep_zero_tree(root, "NONE"))
        if out.result:
            print(f"selftest: NEGATIVE(file-network/{case_name}) control FAILED", file=sys.stderr)
            status = False
        elif needle not in out.text:
            print(f"selftest: NEGATIVE(file-network/{case_name}) control FAILED (did not cite '{needle}')", file=sys.stderr)
            print(out.text, file=sys.stderr)
            status = False
        shutil.rmtree(root, ignore_errors=True)
    if status:
        print("selftest: NEGATIVE(file-network) control OK (file(DOWNLOAD)/file(UPLOAD), each reproved and cited)")
    return status


def selftest_positive_control_file_legit(scratch, capture):
    root = os.path.join(scratch, "positive-file-legit")
    status = True
    for case_name, content in (
        ("make_directory", 'file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")\n'),
        ("sha256", 'file(SHA256 "${CMAKE_CURRENT_LIST_FILE}" out_hash)\n'),
        ("generate", 'file(GENERATE OUTPUT out.txt CONTENT "hi")\n'),
    ):
        make_clean_fixture(root)
        with open(os.path.join(root, "cmake", "Wayland.cmake"), "a", encoding="utf-8", newline="\n") as h:
            h.write(content)
        git_init_fixture(root)
        out = capture(lambda: check_dep_zero_tree(root, "NONE"))
        if not out.result:
            print(f"selftest: POSITIVE(file-legit/{case_name}) control FAILED", file=sys.stderr)
            print(out.text, file=sys.stderr)
            status = False
        elif "WARNING" in out.text:
            print(f"selftest: POSITIVE(file-legit/{case_name}) control FAILED (resolved subcommand should not warn)", file=sys.stderr)
            print(out.text, file=sys.stderr)
            status = False
        shutil.rmtree(root, ignore_errors=True)
    if status:
        print("selftest: POSITIVE(file-legit) control OK (MAKE_DIRECTORY/SHA256/GENERATE, the tree's real subcommands, pass without warning)")
    return status


def selftest_warn_control_file_opener_bare(scratch, capture):
    root = os.path.join(scratch, "warn-file-opener-bare")
    make_clean_fixture(root)
    with open(os.path.join(root, "cmake", "Wayland.cmake"), "a", encoding="utf-8", newline="\n") as h:
        h.write("file(\n    DOWNLOAD\n    https://example.invalid/x\n    o\n)\n")
    git_init_fixture(root)

    out = capture(lambda: check_dep_zero_tree(root, "NONE"))
    if not out.result:
        print("selftest: WARN(file-opener-bare) control FAILED", file=sys.stderr)
        print(out.text, file=sys.stderr)
        return False
    if "WARNING" not in out.text:
        print("selftest: WARN(file-opener-bare) control FAILED (passed without warning)", file=sys.stderr)
        print(out.text, file=sys.stderr)
        return False
    print("selftest: WARN(file-opener-bare) control OK (bare file( opener, DOWNLOAD on a later line, passes with a deferred warning)")
    return True


def selftest_negative_control_execute_process(scratch, capture):
    root = os.path.join(scratch, "negative-execute-process")
    status = True
    cases = {
        "curl": ("execute_process(COMMAND curl https://example.invalid/x)\n", "curl"),
        "git": ("execute_process(COMMAND git clone https://example.invalid/x)\n", "git"),
    }
    for case_name, (addition, needle) in cases.items():
        make_clean_fixture(root)
        with open(os.path.join(root, "cmake", "Wayland.cmake"), "a", encoding="utf-8", newline="\n") as h:
            h.write(addition)
        git_init_fixture(root)
        out = capture(lambda: check_dep_zero_tree(root, "NONE"))
        if out.result:
            print(f"selftest: NEGATIVE(execute-process/{case_name}) control FAILED", file=sys.stderr)
            status = False
        elif needle not in out.text:
            print(f"selftest: NEGATIVE(execute-process/{case_name}) control FAILED (did not cite '{needle}')", file=sys.stderr)
            print(out.text, file=sys.stderr)
            status = False
        shutil.rmtree(root, ignore_errors=True)
    if status:
        print("selftest: NEGATIVE(execute-process) control OK (curl/git via execute_process, each reproved and cited)")
    return status


def selftest_positive_control_execute_process_pkgconfig(scratch, capture):
    root = os.path.join(scratch, "positive-execute-process-pkgconfig")
    make_clean_fixture(root)
    with open(os.path.join(root, "cmake", "Wayland.cmake"), "a", encoding="utf-8", newline="\n") as h:
        h.write("execute_process(COMMAND pkg-config --exists foo)\n")
    git_init_fixture(root)

    out = capture(lambda: check_dep_zero_tree(root, "NONE"))
    if not out.result:
        print("selftest: POSITIVE(execute-process-pkgconfig) control FAILED", file=sys.stderr)
        print(out.text, file=sys.stderr)
        return False
    if "WARNING" in out.text:
        print("selftest: POSITIVE(execute-process-pkgconfig) control FAILED (literal program should not warn)", file=sys.stderr)
        print(out.text, file=sys.stderr)
        return False
    print("selftest: POSITIVE(execute-process-pkgconfig) control OK (literal pkg-config on its own line passes without warning)")
    return True


def selftest_warn_control_execute_process_variable(scratch, capture):
    root = os.path.join(scratch, "warn-execute-process-variable")
    make_clean_fixture(root)
    with open(os.path.join(root, "cmake", "Wayland.cmake"), "a", encoding="utf-8", newline="\n") as h:
        h.write('execute_process(\n    COMMAND "${PKG_CONFIG_EXECUTABLE}" --exists foo\n    RESULT_VARIABLE r\n)\n')
    git_init_fixture(root)

    out = capture(lambda: check_dep_zero_tree(root, "NONE"))
    if not out.result:
        print("selftest: WARN(execute-process-variable) control FAILED", file=sys.stderr)
        print(out.text, file=sys.stderr)
        return False
    if "WARNING" not in out.text:
        print("selftest: WARN(execute-process-variable) control FAILED (passed without warning)", file=sys.stderr)
        print(out.text, file=sys.stderr)
        return False
    print("selftest: WARN(execute-process-variable) control OK (bare opener, program built from a variable on a later line, passes with a deferred warning)")
    return True


def selftest_negative_control_include_traversal(scratch, capture):
    root = os.path.join(scratch, "negative-include-traversal")
    status = True
    cases = {
        "deep_traversal": "#include <glintfx/../../../../../../usr/include/zlib.h>",
        "short_traversal": "#include <glintfx/../../etc/passwd>",
        "dot_and_traversal": "#include <glintfx/core/./../../../../../etc/passwd>",
        "trailing_dotdot": "#include <glintfx/..>",
    }
    for case_name, inc in cases.items():
        make_clean_fixture(root)
        with open(os.path.join(root, "src", "core", "err.cpp"), "a", encoding="utf-8", newline="\n") as h:
            h.write(inc + "\n")
        git_init_fixture(root)
        out = capture(lambda: check_dep_zero_tree(root, "NONE"))
        if out.result:
            print(f"selftest: NEGATIVE(include-traversal/{case_name}) control FAILED", file=sys.stderr)
            status = False
        elif ".." not in out.text:
            print(f"selftest: NEGATIVE(include-traversal/{case_name}) control FAILED (citation does not show the traversal)", file=sys.stderr)
            print(out.text, file=sys.stderr)
            status = False
        shutil.rmtree(root, ignore_errors=True)
    if status:
        print("selftest: NEGATIVE(include-traversal) control OK (four traversal forms, each reproved)")
    return status


def selftest_scope_message_is_honest(scratch, capture):
    root = os.path.join(scratch, "scope-message")
    make_clean_fixture(root)
    git_init_fixture(root)

    out = capture(lambda: check_dep_zero_tree(root, "NONE"))
    if not out.result:
        print("selftest: SCOPE-MESSAGE control FAILED (clean fixture should pass before examining text)", file=sys.stderr)
        print(out.text, file=sys.stderr)
        return False
    if "vendor vector covered by" in out.text:
        print("selftest: SCOPE-MESSAGE control FAILED (still claims 'covered by' - overstates real coverage)", file=sys.stderr)
        print(out.text, file=sys.stderr)
        return False
    if "quote includes" not in out.text:
        print("selftest: SCOPE-MESSAGE control FAILED (does not declare quote includes out of scope)", file=sys.stderr)
        print(out.text, file=sys.stderr)
        return False
    if "the CI oracle" not in out.text:
        print("selftest: SCOPE-MESSAGE control FAILED (does not declare the deferred-warning contract)", file=sys.stderr)
        print(out.text, file=sys.stderr)
        return False
    print("selftest: SCOPE-MESSAGE control OK (out-of-scope line does not overstate coverage, and declares the deferred-warning contract)")
    return True


def _cc_available():
    return shutil.which("cc") is not None


def selftest_negative_control_needed(scratch):
    root = os.path.join(scratch, "negative-needed")
    os.makedirs(root, exist_ok=True)

    if not _cc_available():
        print("selftest: NEGATIVE(needed) control SKIPPED (cc unavailable to build fixture .so)")
        return True

    _write(os.path.join(root, "notallowed.c"), "int notallowed_fn(void) { return 7; }\n")
    r = subprocess.run(
        ["cc", "-shared", "-fPIC", "-o", os.path.join(root, "libnotallowed.so"), os.path.join(root, "notallowed.c"), "-Wl,-soname,libnotallowed.so"],
        capture_output=True,
    )
    if r.returncode != 0:
        print("selftest: NEGATIVE(needed) control SKIPPED (cc unavailable to build fixture .so)")
        return True

    _write(os.path.join(root, "intruder.c"), "extern int notallowed_fn(void);\nint intruder_fn(void) { return notallowed_fn(); }\n")
    subprocess.run(
        ["cc", "-shared", "-fPIC", "-o", os.path.join(root, "libintruder.so"), os.path.join(root, "intruder.c"), f"-L{root}", "-lnotallowed", f"-Wl,-rpath,{root}"],
        check=True,
    )

    ok, text = check_needed_allowlist(os.path.join(root, "libintruder.so"), NEEDED_ALLOWLIST)
    if ok:
        print("selftest: NEGATIVE(needed) control FAILED (intruder library should have been reproved)", file=sys.stderr)
        print(text, file=sys.stderr)
        return False
    if "libnotallowed.so" not in text:
        print("selftest: NEGATIVE(needed) control FAILED (did not cite libnotallowed.so)", file=sys.stderr)
        print(text, file=sys.stderr)
        return False
    print("selftest: NEGATIVE(needed) control OK (real .so linking an out-of-allowlist library, reproved and cited)")
    return True


def selftest_positive_control_needed(scratch):
    root = os.path.join(scratch, "positive-needed")
    os.makedirs(root, exist_ok=True)

    if not _cc_available():
        print("selftest: POSITIVE(needed) control SKIPPED (cc unavailable to build fixture .so)")
        return True

    # A plain cc -shared that references one real libc symbol (strlen)
    # survives --as-needed on every supported distro - see the sh
    # version's own history for why a symbol-free fixture is wrong.
    _write(os.path.join(root, "clean.c"), "#include <string.h>\nsize_t clean_fn(const char *s) { return strlen(s); }\n")
    r = subprocess.run(["cc", "-shared", "-fPIC", "-o", os.path.join(root, "libclean.so"), os.path.join(root, "clean.c")], capture_output=True)
    if r.returncode != 0:
        print("selftest: POSITIVE(needed) control SKIPPED (cc unavailable to build fixture .so)")
        return True

    ok, text = check_needed_allowlist(os.path.join(root, "libclean.so"), NEEDED_ALLOWLIST)
    if not ok:
        print("selftest: POSITIVE(needed) control FAILED (a plain libc-only .so should have passed)", file=sys.stderr)
        print(text, file=sys.stderr)
        return False
    print("selftest: POSITIVE(needed) control OK (libc-only .so, allowed)")
    return True


def selftest_needed_static_skip():
    ok, text = check_needed_allowlist("NONE", NEEDED_ALLOWLIST)
    if ok and "skipped" in text:
        print("selftest: NEEDED-static-skip control OK (NONE declared as skipped, not silently passed)")
        return True
    print("selftest: NEEDED-static-skip control FAILED (NONE should pass while printing a declared skip)", file=sys.stderr)
    return False


def selftest_needed_windows_separate_skip():
    """NEW control (see this file's header, departure 2): proves the
    Windows sentinel is a DECLARED skip, distinct from the static-mode
    one, and cites the separate parity script by name - never silently
    reused text that would misrepresent a real Windows shared build as
    'no dynamic symbol table'."""
    ok, text = check_needed_allowlist("WINDOWS-SEPARATE", NEEDED_ALLOWLIST)
    if ok and "skipped" in text and "check-dep-zero-win.ps1" in text:
        print("selftest: NEEDED-windows-separate-skip control OK (WINDOWS-SEPARATE declared as skipped, citing the separate parity script)")
        return True
    print("selftest: NEEDED-windows-separate-skip control FAILED (should pass, declare 'skipped', and cite check-dep-zero-win.ps1)", file=sys.stderr)
    print(text, file=sys.stderr)
    return False


def selftest_empty_scan_needed(scratch):
    root = os.path.join(scratch, "empty-needed")
    os.makedirs(root, exist_ok=True)

    if not _cc_available():
        print("selftest: EMPTY-SCAN(needed) control SKIPPED (cc -nostdlib unavailable to build fixture .so)")
        return True

    _write(os.path.join(root, "nolibc.c"), "int f(void) { return 1; }\n")
    r = subprocess.run(
        ["cc", "-shared", "-fPIC", "-nostdlib", "-o", os.path.join(root, "libnolibc.so"), os.path.join(root, "nolibc.c")],
        capture_output=True,
    )
    if r.returncode != 0:
        print("selftest: EMPTY-SCAN(needed) control SKIPPED (cc -nostdlib unavailable to build fixture .so)")
        return True

    ok, text = check_needed_allowlist(os.path.join(root, "libnolibc.so"), NEEDED_ALLOWLIST)
    if ok:
        print("selftest: EMPTY-SCAN(needed) control FAILED (a .so with zero NEEDED entries should have been reproved)", file=sys.stderr)
        print(text, file=sys.stderr)
        return False
    if "empty scan" not in text:
        print("selftest: EMPTY-SCAN(needed) control FAILED (did not say 'empty scan')", file=sys.stderr)
        print(text, file=sys.stderr)
        return False
    print("selftest: EMPTY-SCAN(needed) control OK (zero-NEEDED .so refused)")
    return True


def selftest_empty_scan_tree(scratch, capture):
    root = os.path.join(scratch, "empty-tree")
    os.makedirs(root, exist_ok=True)
    subprocess.run(["git", "-C", root, "init", "-q"], check=True)
    subprocess.run(["git", "-C", root, "config", "user.email", "selftest@example.invalid"], check=True)
    subprocess.run(["git", "-C", root, "config", "user.name", "selftest"], check=True)
    _write(os.path.join(root, "README.md"), "")
    subprocess.run(["git", "-C", root, "add", "-A"], check=True)
    subprocess.run(["git", "-C", root, "commit", "-q", "-m", "no cmake, no c++"], check=True)

    out = capture(lambda: check_dep_zero_tree(root, "NONE"))
    if out.result:
        print("selftest: EMPTY-SCAN(tree) control FAILED (a tree with no CMake/C++ surface should have been refused)", file=sys.stderr)
        print(out.text, file=sys.stderr)
        return False
    if "empty scan" not in out.text:
        print("selftest: EMPTY-SCAN(tree) control FAILED (did not say 'empty scan')", file=sys.stderr)
        print(out.text, file=sys.stderr)
        return False
    print("selftest: EMPTY-SCAN(tree) control OK (repository without CMake/C++ surface refused)")
    return True


def selftest_empty_scan_tree_cmake_only(scratch, capture):
    root = os.path.join(scratch, "empty-tree-cmake-only")
    os.makedirs(os.path.join(root, "src"), exist_ok=True)
    subprocess.run(["git", "-C", root, "init", "-q"], check=True)
    subprocess.run(["git", "-C", root, "config", "user.email", "selftest@example.invalid"], check=True)
    subprocess.run(["git", "-C", root, "config", "user.name", "selftest"], check=True)
    _write(os.path.join(root, "src", "f.cpp"), "int f(void) { return 0; }\n")
    subprocess.run(["git", "-C", root, "add", "-A"], check=True)
    subprocess.run(["git", "-C", root, "commit", "-q", "-m", "c++ present, no cmake surface at all"], check=True)

    out = capture(lambda: check_dep_zero_tree(root, "NONE"))
    if out.result:
        print("selftest: EMPTY-SCAN(tree/cmake-only-floor) control FAILED", file=sys.stderr)
        print(out.text, file=sys.stderr)
        return False
    if "0 CMake surface files" not in out.text:
        print("selftest: EMPTY-SCAN(tree/cmake-only-floor) control FAILED (did not specifically say '0 CMake surface files')", file=sys.stderr)
        print(out.text, file=sys.stderr)
        return False
    print("selftest: EMPTY-SCAN(tree/cmake-only-floor) control OK (CMake floor fires on its own, with a non-empty C++ surface present)")
    return True


def selftest_negative_control_accented_filename_tree(scratch, capture):
    root = os.path.join(scratch, "negative-accented-tree")
    make_clean_fixture(root)
    _write(os.path.join(root, "cmake", "Waylând.cmake"), "include(FetchContent)\nFetchContent_Declare(fmt GIT_REPOSITORY x)\n")
    git_init_fixture(root)

    out = capture(lambda: check_dep_zero_tree(root, "NONE"))
    if out.result:
        print("selftest: NEGATIVE(accented-filename/tree) control FAILED", file=sys.stderr)
        print(out.text, file=sys.stderr)
        return False
    if "FetchContent_Declare" not in out.text:
        print("selftest: NEGATIVE(accented-filename/tree) control FAILED (did not cite FetchContent_Declare)", file=sys.stderr)
        print(out.text, file=sys.stderr)
        return False
    print("selftest: NEGATIVE(accented-filename/tree) control OK (accented filename seen and reproved, git ls-files -z)")
    return True


def selftest_staged_accented_filename(scratch, capture):
    root = os.path.join(scratch, "staged-accented")
    make_clean_fixture(root)
    git_init_fixture(root)
    _write(os.path.join(root, "cmake", "Waylând.cmake"), "include(FetchContent)\nFetchContent_Declare(fmt GIT_REPOSITORY x)\n")
    subprocess.run(["git", "-C", root, "add", "cmake/Waylând.cmake"], check=True)

    out = capture(lambda: check_dep_zero_staged(root))
    if out.result:
        print("selftest: STAGED(accented-filename) control FAILED", file=sys.stderr)
        print(out.text, file=sys.stderr)
        return False
    if "FetchContent_Declare" not in out.text:
        print("selftest: STAGED(accented-filename) control FAILED (did not cite FetchContent_Declare)", file=sys.stderr)
        print(out.text, file=sys.stderr)
        return False
    print("selftest: STAGED(accented-filename) control OK (accented filename seen and reproved, git diff --cached -z)")
    return True


def selftest_staged_zero_relevant_declared(scratch, capture):
    root = os.path.join(scratch, "staged-doc-only")
    make_clean_fixture(root)
    git_init_fixture(root)
    _write(os.path.join(root, "README.md"), "more docs\n")
    subprocess.run(["git", "-C", root, "add", "README.md"], check=True)

    out = capture(lambda: check_dep_zero_staged(root))
    if not out.result:
        print("selftest: STAGED(doc-only) control FAILED (a documentation-only staged commit should pass)", file=sys.stderr)
        print(out.text, file=sys.stderr)
        return False
    if "0 relevant file" not in out.text:
        print("selftest: STAGED(doc-only) control FAILED (did not declare '0 relevant file')", file=sys.stderr)
        print(out.text, file=sys.stderr)
        return False
    print("selftest: STAGED(doc-only) control OK (0 relevant among N staged, declared and passed)")
    return True


def selftest_staged_blocks_violation(scratch, capture):
    root = os.path.join(scratch, "staged-violation")
    make_clean_fixture(root)
    git_init_fixture(root)
    with open(os.path.join(root, "cmake", "Wayland.cmake"), "a", encoding="utf-8", newline="\n") as h:
        h.write("include(FetchContent)\n")
    subprocess.run(["git", "-C", root, "add", "cmake/Wayland.cmake"], check=True)

    out = capture(lambda: check_dep_zero_staged(root))
    if out.result:
        print("selftest: STAGED(violation) control FAILED (a staged FetchContent should be reproved)", file=sys.stderr)
        return False
    if "FetchContent" not in out.text:
        print("selftest: STAGED(violation) control FAILED (did not cite FetchContent)", file=sys.stderr)
        print(out.text, file=sys.stderr)
        return False
    print("selftest: STAGED(violation) control OK (staged FetchContent reproved and cited)")
    return True


def selftest_staged_reads_index_not_worktree(scratch, capture):
    root = os.path.join(scratch, "staged-index-only")
    make_clean_fixture(root)
    git_init_fixture(root)
    with open(os.path.join(root, "cmake", "Wayland.cmake"), "a", encoding="utf-8", newline="\n") as h:
        h.write("find_package(PkgConfig REQUIRED)\n")
    subprocess.run(["git", "-C", root, "add", "cmake/Wayland.cmake"], check=True)
    with open(os.path.join(root, "cmake", "Wayland.cmake"), "a", encoding="utf-8", newline="\n") as h:
        h.write("include(FetchContent)\n")

    out = capture(lambda: check_dep_zero_staged(root))
    if not out.result:
        print("selftest: STAGED(index-not-worktree) control FAILED (a dirty WORKING TREE must not fail the hook)", file=sys.stderr)
        print(out.text, file=sys.stderr)
        return False
    print("selftest: STAGED(index-not-worktree) control OK (index read, not the dirtied working tree)")
    return True


def selftest_staged_warns_ambiguous(scratch, capture):
    root = os.path.join(scratch, "staged-warns-ambiguous")
    make_clean_fixture(root)
    git_init_fixture(root)
    with open(os.path.join(root, "cmake", "Wayland.cmake"), "a", encoding="utf-8", newline="\n") as h:
        h.write("pkg_check_modules(\n    Fixture\n    REQUIRED\n    freetype2\n)\n")
    subprocess.run(["git", "-C", root, "add", "cmake/Wayland.cmake"], check=True)

    out = capture(lambda: check_dep_zero_staged(root))
    if not out.result:
        print("selftest: STAGED(warns-ambiguous) control FAILED (ambiguous multi-line form should pass with a warning, not block)", file=sys.stderr)
        print(out.text, file=sys.stderr)
        return False
    if "WARNING" not in out.text:
        print("selftest: STAGED(warns-ambiguous) control FAILED (passed, but did not warn)", file=sys.stderr)
        print(out.text, file=sys.stderr)
        return False
    print("selftest: STAGED(warns-ambiguous) control OK (multi-line pkg_check_modules with hidden freetype2 passes the hook with a warning)")
    return True


def selftest_escape_via_allowlist_edit(scratch):
    root = os.path.join(scratch, "escape")
    make_clean_fixture(root)
    with open(os.path.join(root, "cmake", "Wayland.cmake"), "a", encoding="utf-8", newline="\n") as h:
        h.write("find_package(Xyz REQUIRED)\n")
    git_init_fixture(root)

    this_script = os.path.abspath(__file__)
    r = subprocess.run([sys.executable, this_script, root, "NONE"], capture_output=True)
    if r.returncode == 0:
        print("selftest: ESCAPE control FAILED (find_package(Xyz) must be reproved BEFORE the allowlist edit)", file=sys.stderr)
        return False

    original_text = open(this_script, "r", encoding="utf-8").read()
    needle = 'FIND_PACKAGE_ALLOWLIST = frozenset({"PkgConfig", "glintfx"})'
    if needle not in original_text:
        print("selftest: ESCAPE control FAILED (allowlist line not found - selftest itself is broken)", file=sys.stderr)
        return False
    edited_text = original_text.replace(needle, 'FIND_PACKAGE_ALLOWLIST = frozenset({"PkgConfig", "glintfx", "Xyz"})')

    edited_path = os.path.join(scratch, "check_dep_zero_edited.py")
    with open(edited_path, "w", encoding="utf-8") as h:
        h.write(edited_text)

    r = subprocess.run([sys.executable, edited_path, root, "NONE"], capture_output=True)
    if r.returncode != 0:
        print("selftest: ESCAPE control FAILED (editing the allowlist is the documented escape and must pass)", file=sys.stderr)
        print(r.stdout.decode("utf-8", "replace"), file=sys.stderr)
        print(r.stderr.decode("utf-8", "replace"), file=sys.stderr)
        return False
    print("selftest: ESCAPE control OK (find_package(Xyz) reproves; the SAME SCRIPT with Xyz added to the allowlist by editing this file's source passes - no other escape exists)")
    return True


def selftest_warn_counter_declared(scratch, capture):
    root_clean = os.path.join(scratch, "warn-counter", "clean")
    make_clean_fixture(root_clean)
    git_init_fixture(root_clean)
    out = capture(lambda: check_dep_zero_tree(root_clean, "NONE"))
    if not out.result:
        print("selftest: WARN-COUNTER control FAILED (clean fixture should pass)", file=sys.stderr)
        print(out.text, file=sys.stderr)
        return False
    if "0 warning(s)" not in out.text:
        print("selftest: WARN-COUNTER control FAILED (clean fixture should declare '0 warning(s)')", file=sys.stderr)
        print(out.text, file=sys.stderr)
        return False

    root_one = os.path.join(scratch, "warn-counter", "one-ambiguous")
    make_clean_fixture(root_one)
    with open(os.path.join(root_one, "cmake", "Wayland.cmake"), "a", encoding="utf-8", newline="\n") as h:
        h.write("find_package(\n    PkgConfig\n    REQUIRED\n)\n")
    git_init_fixture(root_one)
    out = capture(lambda: check_dep_zero_tree(root_one, "NONE"))
    if not out.result:
        print("selftest: WARN-COUNTER control FAILED (one ambiguous form should pass with a warning)", file=sys.stderr)
        print(out.text, file=sys.stderr)
        return False
    if "1 warning(s)" not in out.text:
        print("selftest: WARN-COUNTER control FAILED (should declare '1 warning(s)')", file=sys.stderr)
        print(out.text, file=sys.stderr)
        return False
    print("selftest: WARN-COUNTER control OK (summary declares 0 warning(s) clean, 1 warning(s) with one ambiguous form)")
    return True


def hostile_filename_case(case_name):
    return {
        "lf": "cmake/Way\nland2.cmake",
        "dquote": 'cmake/Way"land2.cmake',
        "backslash": "cmake/Way\\land2.cmake",
    }[case_name]


def selftest_negative_control_hostile_filename_tree(scratch, capture):
    root = os.path.join(scratch, "negative-hostile-tree")
    status = True
    for case_name in ("lf", "dquote", "backslash"):
        make_clean_fixture(root)
        name = hostile_filename_case(case_name)
        try:
            full_path = os.path.join(root, *name.split("/"))
            _write(full_path, "include(FetchContent)\n")
        except OSError:
            print(f"selftest: NEGATIVE(hostile-filename/tree/{case_name}) control SKIPPED (this byte is not representable in a filename on this platform)")
            shutil.rmtree(root, ignore_errors=True)
            continue
        git_init_fixture(root)

        out = capture(lambda: check_dep_zero_tree(root, "NONE"))
        if out.result:
            print(f"selftest: NEGATIVE(hostile-filename/tree/{case_name}) control FAILED (should have been reproved)", file=sys.stderr)
            status = False
        else:
            if "FetchContent" not in out.text:
                print(f"selftest: NEGATIVE(hostile-filename/tree/{case_name}) control FAILED (did not cite FetchContent)", file=sys.stderr)
                print(out.text, file=sys.stderr)
                status = False
            if "4 cmake file(s)" not in out.text:
                print(f"selftest: NEGATIVE(hostile-filename/tree/{case_name}) control FAILED (found-count did not count the hostile file among 'cmake file(s)')", file=sys.stderr)
                print(out.text, file=sys.stderr)
                status = False
        shutil.rmtree(root, ignore_errors=True)

    if status:
        print("selftest: NEGATIVE(hostile-filename/tree) control OK (newline/double-quote/backslash filename, each seen, reproved and cited)")
    return status


def selftest_staged_hostile_filename(scratch, capture):
    root = os.path.join(scratch, "staged-hostile")
    status = True
    for case_name in ("lf", "dquote", "backslash"):
        make_clean_fixture(root)
        git_init_fixture(root)
        name = hostile_filename_case(case_name)
        try:
            full_path = os.path.join(root, *name.split("/"))
            _write(full_path, "include(FetchContent)\n")
        except OSError:
            print(f"selftest: STAGED(hostile-filename/{case_name}) control SKIPPED (this byte is not representable in a filename on this platform)")
            shutil.rmtree(root, ignore_errors=True)
            continue
        subprocess.run(["git", "-C", root, "add", name], check=True)

        out = capture(lambda: check_dep_zero_staged(root))
        if out.result:
            print(f"selftest: STAGED(hostile-filename/{case_name}) control FAILED (should have been reproved)", file=sys.stderr)
            status = False
        elif "FetchContent" not in out.text:
            print(f"selftest: STAGED(hostile-filename/{case_name}) control FAILED (did not cite FetchContent)", file=sys.stderr)
            print(out.text, file=sys.stderr)
            status = False
        shutil.rmtree(root, ignore_errors=True)

    if status:
        print("selftest: STAGED(hostile-filename) control OK (newline/double-quote/backslash filename staged, each reproved and cited)")
    return status


def selftest_tree_missing_worktree_file(scratch, capture):
    root = os.path.join(scratch, "tree-missing-worktree-file")
    make_clean_fixture(root)
    git_init_fixture(root)
    os.remove(os.path.join(root, "cmake", "Wayland.cmake"))

    out = capture(lambda: check_dep_zero_tree(root, "NONE"))
    if out.result:
        print("selftest: TREE(missing-worktree-file) control FAILED (a tracked CMake file missing from the worktree must reprove)", file=sys.stderr)
        print(out.text, file=sys.stderr)
        return False
    if "scan incomplete" not in out.text:
        print("selftest: TREE(missing-worktree-file) control FAILED (did not declare 'scan incomplete')", file=sys.stderr)
        print(out.text, file=sys.stderr)
        return False
    if "open refused" not in out.text:
        print("selftest: TREE(missing-worktree-file) control FAILED (did not cite 'open refused')", file=sys.stderr)
        print(out.text, file=sys.stderr)
        return False
    print("selftest: TREE(missing-worktree-file) control OK (a tracked file deleted from the worktree is refused, not silently treated as clean)")
    return True


def selftest_main():
    scratch = make_scratch_workdir()
    capture = _make_capture()
    try:
        controls = [
            selftest_positive_control(scratch, capture),
            selftest_negative_control_cmake(scratch, capture),
            selftest_warn_control_cmake_multiline(scratch, capture),
            selftest_positive_control_cmake_multiline(scratch, capture),
            selftest_positive_control_cmake_opener_resolved(scratch, capture),
            selftest_positive_control_pkgcheck_version(scratch, capture),
            selftest_negative_control_pkgcheck_version_unknown(scratch, capture),
            selftest_negative_control_cmake_indirection(scratch, capture),
            selftest_positive_control_cmake_indirection_legit(scratch, capture),
            selftest_negative_control_cpm_variants(scratch, capture),
            selftest_negative_control_opener_carries_bad_token(scratch, capture),
            selftest_negative_control_file_network(scratch, capture),
            selftest_positive_control_file_legit(scratch, capture),
            selftest_warn_control_file_opener_bare(scratch, capture),
            selftest_negative_control_execute_process(scratch, capture),
            selftest_positive_control_execute_process_pkgconfig(scratch, capture),
            selftest_warn_control_execute_process_variable(scratch, capture),
            selftest_negative_control_include(scratch, capture),
            selftest_negative_control_include_traversal(scratch, capture),
            selftest_scope_message_is_honest(scratch, capture),
            selftest_positive_control_needed(scratch),
            selftest_negative_control_needed(scratch),
            selftest_needed_static_skip(),
            selftest_needed_windows_separate_skip(),
            selftest_empty_scan_needed(scratch),
            selftest_empty_scan_tree(scratch, capture),
            selftest_empty_scan_tree_cmake_only(scratch, capture),
            selftest_negative_control_accented_filename_tree(scratch, capture),
            selftest_staged_accented_filename(scratch, capture),
            selftest_staged_zero_relevant_declared(scratch, capture),
            selftest_staged_blocks_violation(scratch, capture),
            selftest_staged_reads_index_not_worktree(scratch, capture),
            selftest_staged_warns_ambiguous(scratch, capture),
            selftest_escape_via_allowlist_edit(scratch),
            selftest_warn_counter_declared(scratch, capture),
            selftest_negative_control_hostile_filename_tree(scratch, capture),
            selftest_staged_hostile_filename(scratch, capture),
            selftest_tree_missing_worktree_file(scratch, capture),
        ]
        if not all(controls):
            print(f"{SCRIPT_NAME} --selftest: FAILED (see above)", file=sys.stderr)
            sys.exit(1)
        print(f"{SCRIPT_NAME} --selftest: all {len(controls)} controls OK")
    finally:
        shutil.rmtree(scratch, ignore_errors=True)


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
    else:
        real_main(args)


if __name__ == "__main__":
    main()
