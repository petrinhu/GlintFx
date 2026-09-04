#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_layers.py - CI gate for GODS_LAWS.md L-19 ("a CI gate reproves
# the violation" instead of trusting the discipline of whoever writes
# the code).
#
# PORT of the former tests/tools/check_layers.sh (POSIX sh), retired
# in the same fatia that wrote this file (GATE-TREE-PARITY, GODS_LAWS.md
# L-04, decisao do lider: "O comportamento deve ser igual em qualquer
# OS" - the sh version was if(UNIX)-guarded in tests/CMakeLists.txt,
# so nothing checked layer discipline on the Windows CI job at all).
# Registered here, unguarded, as an ordinary ctest case - the same
# shape check_spdx.py and check_hygiene_coverage.py already proved
# works on all five platforms.
#
# Verifies that the core layer (src/core/, include/glintfx/core/) does
# not include (a) a header from a layer above (glintfx/platform/), nor
# (b) an operating system header. The pure core knows nothing about
# the OS.
#
# Usage:
#   check_layers.py <source-root-directory>
#   check_layers.py --selftest
#
# --selftest runs the same four GODS_LAWS.md L-40 controls the sh
# version did (positive, negative, a SECOND negative specific to the
# file-I/O headers added by the ASSET-LOAD conserto of 28/08/2026,
# empty-scan) against disposable fixtures under a scratch directory,
# never against the real tracked tree.
#
# Each function below does one thing (GODS_LAWS.md L-17).

import os
import re
import shutil
import sys
import tempfile

SCRIPT_NAME = "check_layers.py"

# Layer above the core (FUND-2's own note: not created yet at the time
# the sh version was written, but the pattern stays ready for when it
# is born).
UPPER_LAYER_NEEDLE = "glintfx/platform/"

# OS headers covered by this gate: Wayland, Win32, GL/EGL, the most
# common low-level POSIX calls, and (ASSET-LOAD conserto, 28/08/2026,
# GODS_LAWS.md L-19/L-40) file I/O - <filesystem> and <fstream>. Ported
# verbatim from OS_HEADER_PATTERN in the sh version - see that file's
# own history (its own --selftest's selftest_negative_control_file_
# header) for why <filesystem>/<fstream> are here: a scratch core file
# that #includes <fstream> passed the PREVIOUS pattern clean before
# that fix.
OS_HEADER_NEEDLES = (
    "wayland",
    "windows.h",
    "winuser",
    "GL/",
    "EGL/",
    "<dlfcn",
    "<unistd",
    "<sys/",
    "<fcntl",
    "<filesystem",
    "<fstream",
)

_HEADER_EXTENSIONS = (".hpp", ".cpp", ".h", ".hh", ".hxx", ".cc", ".cxx")

_FORBIDDEN_PATTERN = re.compile(
    "|".join(re.escape(needle) for needle in (UPPER_LAYER_NEEDLE,) + OS_HEADER_NEEDLES)
)


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


def core_source_dirs(root):
    for candidate in (
        os.path.join(root, "src", "core"),
        os.path.join(root, "include", "glintfx", "core"),
    ):
        if os.path.isdir(candidate):
            yield candidate


def core_source_files(root):
    """One os.walk() per candidate directory - directory entries come
    back as discrete strings from the filesystem, never a newline-
    joined text stream a hostile filename could split (the same
    GATE-TREE-PARITY-NEWLINE reasoning check_vendor_purity.py's own
    header documents).
    """
    files = []
    for source_dir in core_source_dirs(root):
        for dirpath, _dirnames, filenames in os.walk(source_dir):
            for name in filenames:
                if name.endswith(_HEADER_EXTENSIONS):
                    files.append(os.path.join(dirpath, name))
    return files


def violations_in_file(path):
    violations = []
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as handle:
            for lineno, line in enumerate(handle, start=1):
                if _FORBIDDEN_PATTERN.search(line):
                    violations.append((path, lineno))
    except OSError as exc:
        print(f"{SCRIPT_NAME}: {path}: open refused ({exc})", file=sys.stderr)
    return violations


# GODS_LAWS.md L-40 (piso de varredura nao-vazia): zero files found
# under src/core/ or include/glintfx/core/ is not "nothing to report",
# and reproves.
def require_nonempty_scan(file_count):
    if file_count == 0:
        print(
            f"{SCRIPT_NAME}: varredura vazia (0 arquivos em src/core ou "
            "include/glintfx/core) - GODS_LAWS.md L-40",
            file=sys.stderr,
        )
        return False
    return True


# The actual gate logic, factored out of real_main() so --selftest
# exercises the EXACT same function - not a reimplementation that
# could drift from production.
def check_layers(root):
    files = core_source_files(root)
    file_count = len(files)

    if not require_nonempty_scan(file_count):
        return False

    violations = []
    for f in files:
        violations.extend(violations_in_file(f))

    if violations:
        print(f"{SCRIPT_NAME}: layer violations (GODS_LAWS.md L-19):", file=sys.stderr)
        for path, lineno in violations:
            print(f"{path}:{lineno}", file=sys.stderr)
        return False

    print(f"{SCRIPT_NAME}: violations: 0 in {file_count} files scanned")
    return True


# --- real mode -------------------------------------------------------


def real_main(args):
    if len(args) != 1:
        fail("usage: check_layers.py <source-root-directory>")
    root = args[0]
    if not os.path.isdir(root):
        fail(f"directory not found: {root}")
    if not check_layers(root):
        fail("layer violation found (GODS_LAWS.md L-19; see message above)")


# --- fixtures and controls for --selftest -----------------------------


def make_scratch_workdir():
    return tempfile.mkdtemp(prefix="glintfx-layers-selftest-", dir=os.environ.get("TMPDIR", "/tmp"))


def make_clean_fixture(root):
    core_dir = os.path.join(root, "src", "core")
    os.makedirs(core_dir, exist_ok=True)
    with open(os.path.join(core_dir, "clean.cpp"), "w", encoding="utf-8") as handle:
        handle.write("#include <cstdint>\n// clean core file, no OS or upper-layer header\n")


def _make_capture():
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


# Positive control: clean fixture. Expected: passes.
def selftest_positive_control(scratch, capture):
    root = os.path.join(scratch, "positive")
    make_clean_fixture(root)

    outcome = capture(lambda: check_layers(root))
    if outcome.result:
        print("selftest: controle POSITIVO OK (fixture limpa aprovada)")
        return True
    print(
        "selftest: controle POSITIVO FALHOU (fixture limpa deveria ter sido aprovada)",
        file=sys.stderr,
    )
    print(outcome.text, file=sys.stderr)
    return False


# Negative control: plants a forbidden OS header inside
# include/glintfx/core/. Expected: reproves and cites the planted file.
def selftest_negative_control(scratch, capture):
    root = os.path.join(scratch, "negative")
    make_clean_fixture(root)
    target_dir = os.path.join(root, "include", "glintfx", "core")
    os.makedirs(target_dir, exist_ok=True)
    target = os.path.join(target_dir, "dirty.hpp")
    with open(target, "w", encoding="utf-8") as handle:
        handle.write("#include <wayland-client.h>\n")

    outcome = capture(lambda: check_layers(root))
    if outcome.result:
        print(
            "selftest: controle NEGATIVO FALHOU (header do SO em "
            "include/glintfx/core/ nao foi pego)",
            file=sys.stderr,
        )
        return False
    if target not in outcome.text:
        print(
            f"selftest: controle NEGATIVO FALHOU (reprovou, mas nao citou {target})",
            file=sys.stderr,
        )
        print(outcome.text, file=sys.stderr)
        return False
    print("selftest: controle NEGATIVO OK (header do SO em include/glintfx/core/ pego e citado)")
    return True


# Second negative control (ASSET-LOAD conserto, 28/08/2026): plants
# <fstream> - a file-I/O header, not a Wayland/GL/POSIX one - inside
# src/core/. Expected: reproves and cites the planted file. Separate
# function, separate fixture, on purpose (GODS_LAWS.md L-40 "enumeracao
# fechada por construcao"): the ORIGINAL negative control above only
# ever exercises the wayland-client.h branch of OS_HEADER_NEEDLES, so a
# regression that broke JUST the <filesystem>/<fstream> entries would
# pass every other control silently - this is the control that closes
# that gap specifically.
def selftest_negative_control_file_header(scratch, capture):
    root = os.path.join(scratch, "negative_file_header")
    make_clean_fixture(root)
    target = os.path.join(root, "src", "core", "dirty_file_io.cpp")
    with open(target, "w", encoding="utf-8") as handle:
        handle.write("#include <fstream>\n")

    outcome = capture(lambda: check_layers(root))
    if outcome.result:
        print(
            "selftest: controle NEGATIVO (header de arquivo) FALHOU "
            "(<fstream> em src/core/ nao foi pego)",
            file=sys.stderr,
        )
        return False
    if target not in outcome.text:
        print(
            "selftest: controle NEGATIVO (header de arquivo) FALHOU "
            f"(reprovou, mas nao citou {target})",
            file=sys.stderr,
        )
        print(outcome.text, file=sys.stderr)
        return False
    print(
        "selftest: controle NEGATIVO (header de arquivo) OK "
        "(<fstream> em src/core/ pego e citado)"
    )
    return True


# Empty-scan floor: neither src/core/ nor include/glintfx/core/ exists.
# Expected: reproves with "varredura vazia" in the message.
def selftest_empty_scan_control(scratch, capture):
    root = os.path.join(scratch, "empty")
    os.makedirs(root, exist_ok=True)

    outcome = capture(lambda: check_layers(root))
    if outcome.result:
        print(
            "selftest: controle de VARREDURA VAZIA FALHOU (raiz sem "
            "src/core nem include/glintfx/core deveria ter sido "
            "recusada, mas passou)",
            file=sys.stderr,
        )
        print(outcome.text, file=sys.stderr)
        return False
    if "varredura vazia" not in outcome.text:
        print(
            "selftest: controle de VARREDURA VAZIA FALHOU (recusou, mas "
            "nao disse 'varredura vazia')",
            file=sys.stderr,
        )
        print(outcome.text, file=sys.stderr)
        return False
    print("selftest: controle de VARREDURA VAZIA OK (raiz sem diretorio de core recusada)")
    return True


def selftest_main():
    scratch = make_scratch_workdir()
    capture = _make_capture()
    try:
        controls = [
            selftest_positive_control(scratch, capture),
            selftest_negative_control(scratch, capture),
            selftest_negative_control_file_header(scratch, capture),
            selftest_empty_scan_control(scratch, capture),
        ]
        if not all(controls):
            print("check_layers.py --selftest: FALHOU (ver acima)", file=sys.stderr)
            sys.exit(1)
        print(f"check_layers.py --selftest: os {len(controls)} controles OK")
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
