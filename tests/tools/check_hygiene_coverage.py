#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_hygiene_coverage.py - mechanical coverage gate for
# tests/header_hygiene_test.cpp (HDR-HYGIENE-FIX-2). Turns the "every
# new public header enters this translation unit" comment-only
# contract that used to sit at the top of header_hygiene_test.cpp into
# a portal that fails the build when a public header is added and NOT
# wired into that hygiene test's include list.
#
# PORT of the former tests/tools/check_hygiene_coverage.sh (POSIX sh),
# retired in the same fatia that wrote this file (GODS_LAWS.md L-04,
# decisao do lider de 02/09/2026: "O comportamento deve ser igual em
# qualquer OS"). The sh version was registered inside an if(UNIX)
# guard in tests/CMakeLists.txt, so it never ran on the windows-latest
# CI job. This file preserves every rule the sh version enforced and
# is registered UNGUARDED, so it enters the same ctest run the linux
# and windows CI jobs already execute on all five platforms - the same
# shape check_dep_zero_trace.py (python3, no if(UNIX) guard) proved.
#
# METHOD: ENUMERATION, not directed search. Directed search ("grep for
# headers used somewhere") finds what you already suspect; enumeration
# finds what you did not know to suspect - house rule of this project.
# Every *.hpp/*.h/*.hh/*.hxx found under <include_dir> is listed, and
# each one must have a literal `#include <path>` line (relative to
# <include_dir> itself, ALWAYS with forward slashes - the C++
# #include grammar and this project's own include convention never
# use a backslash, even on Windows) inside <tu_file>.
#
# LIMITATION, declared here on purpose: headers GENERATED at configure
# or build time (export.hpp, version_macros.hpp - see cmake/
# GlintfxLibrary.cmake, glintfx_generate_export_header) are never
# under <include_dir> in the SOURCE tree; they are written under the
# build directory's generated include path instead. This gate
# enumerates what is COMMITTED. It treats generated headers as covered
# by transitivity: glintfx never ships without generating them, and
# header_hygiene_test.cpp already includes glintfx/version_macros.hpp
# directly.
#
# Usage:
#   check_hygiene_coverage.py <include_dir> <tu_file>
#   check_hygiene_coverage.py --selftest
#
# The real invocation (see tests/CMakeLists.txt) passes
# "${PROJECT_SOURCE_DIR}/include" as <include_dir>, so a header at
# include/glintfx/core/version.hpp enumerates as
# "glintfx/core/version.hpp" and the required line is literally
# `#include <glintfx/core/version.hpp>`.
#
# --selftest runs five controls (positive, negative, empty-scan floor,
# .h extension, commented-include) against throwaway fixture trees
# under a real temp directory, never against the real <include_dir>/
# <tu_file> - see selftest_main() below. Same five the sh version's
# own --selftest ran.
#
# Each function below does one thing (GODS_LAWS.md L-17).

import os
import re
import sys
import tempfile

SCRIPT_NAME = "check_hygiene_coverage.py"

_HEADER_SUFFIXES = (".hpp", ".h", ".hh", ".hxx")
_COMMENT_LINE_RE = re.compile(r"^\s*//")


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


# --- enumeration -------------------------------------------------------


def enumerate_public_headers(include_dir):
    """One path per entry, relative to include_dir, forward-slash
    separated regardless of host OS (matching the literal #include
    grammar every caller of this list checks against), for every
    public header found under it - e.g. "glintfx/core/version.hpp".
    Sorted for deterministic output.
    """
    headers = []
    for root, _dirs, files in os.walk(include_dir):
        for name in files:
            if name.endswith(_HEADER_SUFFIXES):
                full_path = os.path.join(root, name)
                relative = os.path.relpath(full_path, include_dir)
                headers.append(relative.replace(os.sep, "/"))
    headers.sort()
    return headers


def header_is_included_in_tu(header, tu_lines):
    """Achado F6 (sh version, 26/08/2026): the old match was literal
    and comment-blind (`grep -qF`), so "// #include <glintfx/...>"
    counted as covered - a header whose inclusion was disabled in a
    refactor passed as covered without exercising anything. Filters
    out commented lines (start, after optional leading whitespace,
    with "//") BEFORE the literal check; a real include on the same TU
    still matches normally.
    """
    needle = f"#include <{header}>"
    for line in tu_lines:
        if _COMMENT_LINE_RE.match(line):
            continue
        if needle in line:
            return True
    return False


def missing_headers(headers, tu_lines):
    return [header for header in headers if not header_is_included_in_tu(header, tu_lines)]


# --- coverage check ------------------------------------------------------


def check_coverage(include_dir, tu_file):
    headers = enumerate_public_headers(include_dir)
    if not headers:
        print(f"{SCRIPT_NAME}: varredura vazia (0 headers)", file=sys.stderr)
        return False

    with open(tu_file, "r", encoding="utf-8") as handle:
        tu_lines = handle.readlines()

    missing = missing_headers(headers, tu_lines)
    if missing:
        print(f"{SCRIPT_NAME}: header(s) publico(s) sem cobertura em {tu_file}:", file=sys.stderr)
        for header in missing:
            print(header, file=sys.stderr)
        return False

    print(f"{SCRIPT_NAME}: {len(headers)} header(s) publico(s) cobertos em {tu_file}")
    return True


# --- real mode -----------------------------------------------------------


def real_main(args):
    if len(args) != 2:
        fail("usage: check_hygiene_coverage.py <include_dir> <tu_file>")
    include_dir, tu_file = args
    if not os.path.isdir(include_dir):
        fail(f"include dir not found: {include_dir}")
    if not os.path.isfile(tu_file):
        fail(f"tu file not found: {tu_file}")
    if not check_coverage(include_dir, tu_file):
        fail("cobertura de higiene de header incompleta (ver mensagem acima)")


# --- selftest fixtures and controls ---------------------------------------


def make_scratch_workdir():
    return tempfile.mkdtemp(prefix="glintfx-hygiene-coverage-")


def write_file(path, content):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as handle:
        handle.write(content)


class _Captured:
    __slots__ = ("result", "text")

    def __init__(self, result, text):
        self.result = result
        self.text = text


def _capture(fn):
    import contextlib
    import io

    buffer = io.StringIO()
    with contextlib.redirect_stdout(buffer), contextlib.redirect_stderr(buffer):
        result = fn()
    return _Captured(result, buffer.getvalue())


# Positive control: one header, and the TU includes it. Expected: pass.
def selftest_positive_control(scratch):
    include_dir = os.path.join(scratch, "positive", "include")
    write_file(os.path.join(include_dir, "pkg", "foo.hpp"), "// fixture header\n")
    tu_file = os.path.join(scratch, "positive", "tu.cpp")
    write_file(tu_file, "#include <pkg/foo.hpp>\n")

    output = _capture(lambda: check_coverage(include_dir, tu_file))
    if output.result:
        print("selftest: controle POSITIVO OK (fixture completa aprovada)")
        return True
    print("selftest: controle POSITIVO FALHOU (fixture completa deveria ter sido aprovada)", file=sys.stderr)
    print(output.text, file=sys.stderr)
    return False


# Negative control: two headers, TU includes only one. Expected: fail,
# naming the missing header (pkg/bar.hpp) in the message.
def selftest_negative_control(scratch):
    include_dir = os.path.join(scratch, "negative", "include")
    write_file(os.path.join(include_dir, "pkg", "foo.hpp"), "// fixture header\n")
    write_file(os.path.join(include_dir, "pkg", "bar.hpp"), "// fixture header, deliberately NOT included below\n")
    tu_file = os.path.join(scratch, "negative", "tu.cpp")
    write_file(tu_file, "#include <pkg/foo.hpp>\n")

    output = _capture(lambda: check_coverage(include_dir, tu_file))
    if output.result:
        print("selftest: controle NEGATIVO FALHOU (deveria acusar pkg/bar.hpp faltando, mas passou)", file=sys.stderr)
        print(output.text, file=sys.stderr)
        return False
    if "pkg/bar.hpp" not in output.text:
        print("selftest: controle NEGATIVO FALHOU (acusou algo, mas nao citou pkg/bar.hpp)", file=sys.stderr)
        print(output.text, file=sys.stderr)
        return False
    print("selftest: controle NEGATIVO OK (acusou pkg/bar.hpp corretamente)")
    return True


# Empty-scan floor: directory with zero headers. Expected: fail with
# "varredura vazia" in the message.
def selftest_empty_scan_control(scratch):
    include_dir = os.path.join(scratch, "empty", "include")
    os.makedirs(include_dir, exist_ok=True)
    tu_file = os.path.join(scratch, "empty", "tu.cpp")
    write_file(tu_file, "// nada a incluir\n")

    output = _capture(lambda: check_coverage(include_dir, tu_file))
    if output.result:
        print("selftest: controle de VARREDURA VAZIA FALHOU (deveria recusar diretorio sem headers)", file=sys.stderr)
        print(output.text, file=sys.stderr)
        return False
    if "varredura vazia" not in output.text:
        print("selftest: controle de VARREDURA VAZIA FALHOU (recusou, mas nao disse 'varredura vazia')", file=sys.stderr)
        print(output.text, file=sys.stderr)
        return False
    print("selftest: controle de VARREDURA VAZIA OK (recusou diretorio sem headers)")
    return True


# Achado F6 (parte 1, sh version, 26/08/2026): a .h header not
# included, next to a covered .hpp, must appear as missing - before
# the sh fix it was INVISIBLE (enumeration only looked at *.hpp).
def selftest_extension_h_control(scratch):
    include_dir = os.path.join(scratch, "extension_h", "include")
    write_file(os.path.join(include_dir, "pkg", "foo.hpp"), "// fixture .hpp, coberta\n")
    write_file(os.path.join(include_dir, "pkg", "legacy.h"), "// fixture .h, deliberadamente NAO incluida abaixo\n")
    tu_file = os.path.join(scratch, "extension_h", "tu.cpp")
    write_file(tu_file, "#include <pkg/foo.hpp>\n")

    output = _capture(lambda: check_coverage(include_dir, tu_file))
    if output.result:
        print("selftest: controle de EXTENSAO .h FALHOU (deveria acusar pkg/legacy.h faltando)", file=sys.stderr)
        print(output.text, file=sys.stderr)
        return False
    if "pkg/legacy.h" not in output.text:
        print("selftest: controle de EXTENSAO .h FALHOU (reprovou, mas nao citou pkg/legacy.h)", file=sys.stderr)
        print(output.text, file=sys.stderr)
        return False
    print("selftest: controle de EXTENSAO .h OK (pkg/legacy.h enumerado e acusado como faltando)")
    return True


# Achado F6 (parte 2, sh version, 26/08/2026): a COMMENTED include
# ("// #include <...>") must not count as coverage.
def selftest_commented_include_control(scratch):
    include_dir = os.path.join(scratch, "commented", "include")
    write_file(os.path.join(include_dir, "pkg", "disabled.hpp"), "// fixture header, cuja inclusao abaixo esta comentada\n")
    tu_file = os.path.join(scratch, "commented", "tu.cpp")
    write_file(tu_file, "// #include <pkg/disabled.hpp>\n")

    output = _capture(lambda: check_coverage(include_dir, tu_file))
    if output.result:
        print("selftest: controle de INCLUSAO COMENTADA FALHOU (deveria acusar pkg/disabled.hpp faltando)", file=sys.stderr)
        print(output.text, file=sys.stderr)
        return False
    if "pkg/disabled.hpp" not in output.text:
        print("selftest: controle de INCLUSAO COMENTADA FALHOU (reprovou, mas nao citou pkg/disabled.hpp)", file=sys.stderr)
        print(output.text, file=sys.stderr)
        return False
    print("selftest: controle de INCLUSAO COMENTADA OK (inclusao comentada nao contou como cobertura)")
    return True


def selftest_main():
    scratch = make_scratch_workdir()
    try:
        controls = [
            selftest_positive_control(scratch),
            selftest_negative_control(scratch),
            selftest_empty_scan_control(scratch),
            selftest_extension_h_control(scratch),
            selftest_commented_include_control(scratch),
        ]
        if not all(controls):
            print("check_hygiene_coverage.py --selftest: FALHOU (ver acima)", file=sys.stderr)
            sys.exit(1)
        print(f"check_hygiene_coverage.py --selftest: os {len(controls)} controles OK")
    finally:
        import shutil

        shutil.rmtree(scratch, ignore_errors=True)


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
    else:
        real_main(args)


if __name__ == "__main__":
    main()
