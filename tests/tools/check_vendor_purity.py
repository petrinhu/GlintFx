#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_vendor_purity.py - CI gate for GODS_LAWS.md L-07 EXCECAO No 1
# (third_party/khronos/ e a UNICA excecao de glintfx a lei de
# dependencia zero) e L-40 (piso de varredura nao-vazia).
#
# PORT of the former tests/tools/check_vendor_purity.sh (POSIX sh),
# retired in the same fatia that wrote this file (GATE-TREE-PARITY,
# GODS_LAWS.md L-04, decisao do lider: "O comportamento deve ser igual
# em qualquer OS" - the sh version ran ONLY as two standalone steps in
# the ubuntu-only "leis" job of .github/workflows/ci.yml, never
# exercised on Windows/CachyOS/Arch separately, and never registered
# as a ctest case at all). Registered here, unguarded, as an ordinary
# ctest case (tests/CMakeLists.txt) - the same shape check_spdx.py and
# check_dep_zero_trace.py already proved works on all five platforms.
#
# GATE-TREE-PARITY-NEWLINE (fixed here, present in BOTH sh siblings
# this port retires): TODO.md documented, before this fatia, that
# check_vendor_purity.sh (`find ... | while IFS= read -r f`) and
# tests/tools/khronos_vendor_files.sh shared the SAME defect class
# GATE-DEPZERO-NOFORK closed for check_dep_zero.sh - a hostile filename
# containing a literal newline byte splits a newline-joined shell
# listing into two lines and desyncs the scan. os.walk() below reads
# directory entries directly from the filesystem as discrete Python
# strings, one per entry - there is no text stream to split, so this
# defect class does not exist here by construction, not by a fix
# layered on top.
#
# WHAT THIS GATE PROVES: third_party/khronos/ contains ONLY the files
# the exception named - the two vendored verbatim from the Khronos
# Group (gl.xml, LICENSE-APACHE-2.0.txt, obligation 1) plus glintfx's
# own README.md documenting provenance (obligation 4 - see
# third_party/khronos/README.md and this repo's own .gitattributes,
# which already documents that directory "also holds README.md, which
# is OUR OWN prose") - nothing else. Without this gate, nothing stops
# someone from adding a file there and the exception growing one file
# at a time until it becomes a real dependency - the exact thing
# GODS_LAWS.md L-07 exists to prevent.
#
# THE CLOSED LIST IS DUPLICATED, NOT IMPORTED, FROM khronos_vendor_
# files.sh - and khronos_vendor_files.sh is DELETED in this same fatia
# (GODS_LAWS.md L-27: fact separated from inference). That file's own
# header already noted it had exactly ONE remaining consumer after
# check_spdx.py's own port duplicated the same enumeration as a Python
# constant (SPDX-GATE-PY, 02/09/2026) - this port removes that last
# consumer, so the shared-library shape (two independent files that
# must agree with nothing enforcing it, the exact defect TODO.md's
# item VENDOR-PURITY already named once) is retired entirely rather
# than kept alive for a single caller. If this three-entry list ever
# changes, update BOTH this constant and check_spdx.py's own
# KNOWN_KHRONOS_VENDOR_FILES - each file's header cross-references the
# other for exactly this reason.
#
# Scans the real DIRECTORY (os.walk, not `git ls-files`): an intruder
# file has to be caught even BEFORE it is committed - the earliest the
# exception could start growing, and before an atomic `git add`
# (CLAUDE.md, "Estado atual do repositorio") gets a chance to bundle it
# alongside a legitimate slice unnoticed.
#
# Usage:
#   check_vendor_purity.py <repo-root-directory>
#   check_vendor_purity.py --selftest
#
# --selftest runs the same three controls the sh version did
# (positive, negative with an intruder file, empty scan) against
# disposable fixtures under a scratch directory, never against the
# real tracked tree - the three GODS_LAWS.md L-40 requires of every
# gate.
#
# Each function below does one thing (GODS_LAWS.md L-17).

import os
import shutil
import sys
import tempfile

SCRIPT_NAME = "check_vendor_purity.py"
VENDOR_SUBDIR = "third_party/khronos"

# DUPLICATED from check_spdx.py's own KNOWN_KHRONOS_VENDOR_FILES (see
# this file's header for why khronos_vendor_files.sh, the shell
# library both gates used to share, is deleted rather than kept for a
# single remaining consumer).
KNOWN_KHRONOS_VENDOR_FILES = frozenset({
    "third_party/khronos/gl.xml",
    "third_party/khronos/LICENSE-APACHE-2.0.txt",
    "third_party/khronos/README.md",
})


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


# --- enumeracao ------------------------------------------------------


def scanned_files(root):
    """Lists every file under <root>/third_party/khronos/, recursive,
    relative to root, in POSIX form ('/' separators regardless of
    platform) - the same format check_spdx.py's KNOWN_KHRONOS_VENDOR_
    FILES uses. os.walk() reads directory entries directly from the
    filesystem as discrete strings, one per entry (see this file's own
    header, GATE-TREE-PARITY-NEWLINE). A missing third_party/khronos/
    directory is not a usage error - it returns an empty list, and
    require_nonempty_scan() below reproves that as an empty scan,
    never as "nothing there, so nothing to reprove".
    """
    vendor_dir = os.path.join(root, *VENDOR_SUBDIR.split("/"))
    if not os.path.isdir(vendor_dir):
        return []
    found = []
    for dirpath, _dirnames, filenames in os.walk(vendor_dir):
        for name in filenames:
            full_path = os.path.join(dirpath, name)
            rel_path = os.path.relpath(full_path, root)
            found.append(rel_path.replace(os.sep, "/"))
    return found


# GODS_LAWS.md L-40: 0 files scanned (directory absent, or present and
# empty) is a FAILURE, never a silent success - the exact defect class
# this law exists to forbid. Note the difference from what this gate
# reproves when it DOES find something: "0 scanned" is the scan floor
# (this function); "N scanned, some outside the list" is a purity
# violation (check_vendor_purity() below) - two different questions,
# never merged into the same check.
def require_nonempty_scan(files):
    if not files:
        print(
            f"{SCRIPT_NAME}: varredura vazia (0 arquivos em {VENDOR_SUBDIR}/)",
            file=sys.stderr,
        )
        return False
    return True


# --- checagem ----------------------------------------------------------


def check_vendor_purity(root):
    """Mirrors check_vendor_purity.sh's own check_vendor_purity()
    shell function. Returns True (pass, prints the ok summary to
    stdout) or False (reprove, prints violations to stderr) - never
    raises for an ordinary reprove, matching the sh version's own
    return-code contract.
    """
    files = scanned_files(root)
    if not require_nonempty_scan(files):
        return False

    file_count = len(files)
    expected_count = len(KNOWN_KHRONOS_VENDOR_FILES)

    intruders = sorted(f for f in files if f not in KNOWN_KHRONOS_VENDOR_FILES)

    if intruders:
        print(
            f"{SCRIPT_NAME}: PROIBIDO (GODS_LAWS.md L-07 EXCECAO No 1): "
            f"{len(intruders)} arquivo(s) em {VENDOR_SUBDIR}/ fora da lista fechada:",
            file=sys.stderr,
        )
        for f in intruders:
            print(f"  {f}", file=sys.stderr)
        return False

    print(
        f"{SCRIPT_NAME}: {file_count} arquivo(s) varrido(s) em {VENDOR_SUBDIR}/, "
        f"{expected_count} esperado(s) pela excecao - nenhum intruso"
    )
    return True


# --- modo real -----------------------------------------------------------


def real_main(args):
    if len(args) != 1:
        fail("usage: check_vendor_purity.py <repo-root-directory>")
    root = args[0]
    if not os.path.isdir(root):
        fail(f"directory not found: {root}")
    if not check_vendor_purity(root):
        fail(f"arquivo fora da excecao encontrado em {VENDOR_SUBDIR}/ (ver mensagem acima)")


# --- fixtures e controles do --selftest -----------------------------------


def make_scratch_workdir():
    # A hand written Unix path does not exist on every platform (Windows
    # has no /tmp). dir=os.environ.get("TMPDIR") without a hardcoded
    # fallback lets tempfile.mkdtemp fall through to gettempdir(), which
    # already checks TMPDIR/TEMP/TMP and then the platform default.
    return tempfile.mkdtemp(
        prefix="glintfx-vendor-purity-selftest-",
        dir=os.environ.get("TMPDIR"),
    )


# Tree with EXACTLY the three files the exception named - fixture
# content, never the real gl.xml/LICENSE/README (does not need to be:
# the gate compares PATH, never content nor sha256 - that job belongs
# to the integrity gate in tools/gl_registry_codegen, already proven
# there).
def make_clean_fixture(root):
    vendor_dir = os.path.join(root, *VENDOR_SUBDIR.split("/"))
    os.makedirs(vendor_dir, exist_ok=True)
    with open(os.path.join(vendor_dir, "gl.xml"), "w", encoding="utf-8") as handle:
        handle.write("<comment>fixture, nao o gl.xml real</comment>\n")
    with open(
        os.path.join(vendor_dir, "LICENSE-APACHE-2.0.txt"), "w", encoding="utf-8"
    ) as handle:
        handle.write("Apache License 2.0 full text, fixture\n")
    with open(os.path.join(vendor_dir, "README.md"), "w", encoding="utf-8") as handle:
        handle.write("# fixture, nao o README real\n")


def _make_capture():
    """Returns a `capture(fn)` helper that runs fn() with stdout AND
    stderr redirected to an in-memory buffer, and hands back both the
    boolean result and everything printed - same shape check_spdx.py's
    own _make_capture() already established for this house.
    """
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


# Positive control: exactly the three named files. Expected: passes.
def selftest_positive_control(scratch, capture):
    root = os.path.join(scratch, "positive")
    make_clean_fixture(root)

    outcome = capture(lambda: check_vendor_purity(root))
    if outcome.result:
        print(
            "selftest: controle POSITIVO OK (exatamente os tres arquivos "
            "nomeados, nada mais, aprovado)"
        )
        return True
    print(
        "selftest: controle POSITIVO FALHOU (fixture limpa deveria ter sido aprovada)",
        file=sys.stderr,
    )
    print(outcome.text, file=sys.stderr)
    return False


# Negative control: the three legitimate files PLUS a fourth, intruder,
# outside the closed list. Expected: reproves, cites the intruder's
# exact path, never accuses any of the three legitimate ones.
def selftest_negative_control(scratch, capture):
    root = os.path.join(scratch, "negative")
    make_clean_fixture(root)
    vendor_dir = os.path.join(root, *VENDOR_SUBDIR.split("/"))
    intruder = os.path.join(vendor_dir, "mystery_vendor_file.dat")
    with open(intruder, "w", encoding="utf-8") as handle:
        handle.write("arquivo intruso, fora da excecao\n")

    outcome = capture(lambda: check_vendor_purity(root))
    if outcome.result:
        print(
            "selftest: controle NEGATIVO FALHOU (arquivo intruso deveria "
            "ter sido reprovado, mas passou)",
            file=sys.stderr,
        )
        print(outcome.text, file=sys.stderr)
        return False

    ok = True
    if "third_party/khronos/mystery_vendor_file.dat" not in outcome.text:
        print(
            "selftest: controle NEGATIVO FALHOU (reprovou, mas nao citou "
            "o arquivo intruso pelo caminho exato)",
            file=sys.stderr,
        )
        ok = False
    for legitimo in (
        "third_party/khronos/gl.xml",
        "third_party/khronos/LICENSE-APACHE-2.0.txt",
        "third_party/khronos/README.md",
    ):
        if legitimo in outcome.text:
            print(
                f"selftest: controle NEGATIVO FALHOU (acusou o arquivo "
                f"legitimo '{legitimo}')",
                file=sys.stderr,
            )
            ok = False

    if ok:
        print(
            "selftest: controle NEGATIVO OK (intruso citado pelo caminho "
            "exato, os tres legitimos intactos)"
        )
    print(outcome.text, file=sys.stderr)
    return ok


# Empty-scan floor: third_party/khronos/ does not even exist in the
# fixture. Expected: reproves with "varredura vazia" in the message -
# the very reason GODS_LAWS.md L-40 exists, never "directory absent,
# so nothing to reprove, pass".
def selftest_empty_scan_control(scratch, capture):
    root = os.path.join(scratch, "empty")
    os.makedirs(root, exist_ok=True)

    outcome = capture(lambda: check_vendor_purity(root))
    if outcome.result:
        print(
            "selftest: controle de VARREDURA VAZIA FALHOU "
            f"({VENDOR_SUBDIR}/ ausente deveria ter sido recusado, mas passou)",
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
    print(
        "selftest: controle de VARREDURA VAZIA OK "
        f"({VENDOR_SUBDIR}/ ausente recusado, nunca presumido vazio-e-ok)"
    )
    return True


def selftest_main():
    scratch = make_scratch_workdir()
    capture = _make_capture()
    try:
        controls = [
            selftest_positive_control(scratch, capture),
            selftest_negative_control(scratch, capture),
            selftest_empty_scan_control(scratch, capture),
        ]
        if not all(controls):
            print("check_vendor_purity.py --selftest: FALHOU (ver acima)", file=sys.stderr)
            sys.exit(1)
        print(f"check_vendor_purity.py --selftest: os {len(controls)} controles OK")
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
