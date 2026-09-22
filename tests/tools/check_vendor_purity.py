#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_vendor_purity.py - CI gate for GODS_LAWS.md L-07 (dependencia
# zero) and L-40 (piso de varredura nao-vazia).
#
# WIDENED in fatia VENDOR-SWEEP-GATE (TODO.md, onda W7-B), plan at
# /var/tmp/glintfx-plan/vendor-sweep-gate.md. The gate used to look at
# ONE hardcoded path (third_party/khronos/) and declare that in its
# own name/messages - a vendorized folder anywhere else in the tree
# had no owner at all (rc=0, silently, exactly the GODS_LAWS.md L-40
# defect shape: "it does not look, and prints green"). This version
# alarges the SAME single question the gate always asked - "is
# anything foreign living outside the one exception the leader
# opened?" - to the whole tree, instead of adding a second gate next
# to it (GODS_LAWS.md L-17: one question, one atom; a second gate
# would have quadruplicated the closed Khronos-path list that already
# lives in this file, check_spdx.py and AUDITORIAS.md chapter 1).
#
# THREE ENUMERATIONS, ONE VERDICT (GODS_LAWS.md L-17: each function
# does one thing; the shape check_dep_zero.py's own a/b/c sub-checks
# already proved in this house):
#
#   E1 - the UNIVERSE, and the GODS_LAWS.md L-40 scan floor. Union of
#        `git ls-files` (tracked) and `git ls-files --others
#        --exclude-standard` (untracked, not gitignored) - the exact
#        shape check_spdx.py's own scanned_files() already uses.
#        len(E1) == 0 reproves: a repository with nothing in it is a
#        broken scan, never "nothing to reprove, so pass".
#
#   E2 - directories of VENDOR FORM: any path segment (case-folded)
#        matching a closed vocabulary (third_party, vendor, deps,
#        submodule, sdk, ...; see VENDOR_FORM_VOCABULARY below).
#        Every file under a matched directory must be in
#        KNOWN_VENDOR_FILES, or it is a violation. GODS_LAWS.md
#        DECISAO AUTONOMA 2 (plan SS3.4): zero files found under ANY
#        vendor-form directory ALSO reproves - the Khronos exception
#        has exactly three files today, and them silently vanishing
#        is news, never normal.
#
#   E3 - THIRD-PARTY ARTIFACT FORM: a file whose own NAME identifies
#        it as vendored regardless of which directory it sits in -
#        a binary library extension (.a/.lib/.so/.dylib/.dll/.o/.obj),
#        an archived source package (.zip/.tar/.tar.gz/.tar.bz2/
#        .tar.xz/.tgz/.7z/.rar/.jar/.whl/.crate/.gem), or the exact
#        basename '.gitmodules' (a git submodule declaration).
#
# WHY E3 IS A REAL FILESYSTEM WALK, NOT "E1 filtered by extension" AS
# THE PLAN DOCUMENT (SS3.3) LITERALLY DESCRIBES - the one deliberate,
# measured deviation from that document's wording in this fatia,
# recorded here per GODS_LAWS.md L-27 (fact separated from inference)
# and reported to the orchestrator at delivery time:
#
#   Measured against this repository's own .gitignore, section
#   "Artefatos de compilacao": *.o *.obj *.a *.lib *.so *.so.* *.dylib
#   *.dll *.exe are ALL repository-wide gitignored patterns - which is
#   EXACTLY the extension set E3 exists to catch. `git ls-files
#   --others --exclude-standard` is DEFINED to drop every path
#   .gitignore excludes (git-ls-files(1), "--exclude-standard"). A
#   design that builds E3 by filtering E1 would therefore be
#   STRUCTURALLY BLIND to a third-party .a/.o/.so/.dll/.lib planted
#   anywhere outside a vendor-form directory: the very GODS_LAWS.md
#   L-40 defect shape ("it does not look") this whole fatia exists to
#   close, reintroduced one layer up. Measured, not assumed: this
#   file's own --selftest (selftest_negative_binary_artifact_control)
#   plants src/core/libfoo.a and requires rc=1 - the sabotage this
#   fatia's plan document names S4. Under "E3 sobre E1" that fixture
#   is invisible by construction (.a is gitignored) and the control
#   could never turn red honestly. E3 below walks the real filesystem
#   instead (os.walk, same as E2, and the same reason GATE-TREE-
#   PARITY-NEWLINE already gives: an intruder has to be caught even
#   BEFORE `git add`, and a gitignored one is the earliest case of
#   that, not an exception to it).
#
# THE ONE HARD, EXPLICIT PRUNE THAT KEEPS THIS SAFE FOR PERFORMANCE
# AND FALSE POSITIVES (GODS_LAWS.md L-40 item 3: a prune is reported,
# never a silent hole): any directory segment matching build*
# (case-folded) is skipped outright, never descended, and its count is
# printed. Measured on this exact repository, 21/09/2026: seven such
# directories (build, build-static, build-preci, build-preci-debug,
# build-errcopy-red, build-f5-oom, build-verif-f4) hold ~4600 files
# between them, and EVERY .o/.a/.so/.exe the real compiler writes
# there is the project's OWN build output, never distributed - without
# this prune, E3 would flag thousands of legitimate build artifacts as
# "third-party". CLAUDE.md documents this naming convention and
# .gitignore's own "build/" + "build-*/" rules already assume it; this
# prune is the filesystem-walk equivalent of the same fact.
#
# WHY NOTHING ELSE IS PRUNED BY GENERIC GIT-IGNORE STATUS, ON PURPOSE:
# a vendor-form directory (E2) or a third-party artifact (E3) is never
# skipped for being gitignored, even though the plan document's SS3.3
# describes pruning "todo diretorio que git check-ignore reportar como
# ignorado". Hiding a vendored folder or a vendored binary behind a
# new .gitignore entry must not be a way past this gate - that would
# make the gate WORSE than absent (GODS_LAWS.md L-40's own framing:
# false-green confidence is worse than a known-missing gate). This is
# flagged to the orchestrator as a finding, not silently decided.
#
# PORT LINEAGE (unchanged from the version this widens): originally
# POSIX sh (tests/tools/check_vendor_purity.sh, ubuntu-only "leis" job,
# never registered as a ctest case), ported to python3 and registered
# unguarded in tests/CMakeLists.txt (vendor_purity_test,
# vendor_purity_selftest) so it runs inside the same ctest invocation
# on all five platforms - see git history for that port's own
# GATE-TREE-PARITY-NEWLINE fix (os.walk() reads filesystem entries as
# discrete strings; a hostile filename containing a literal newline
# byte cannot desync it, unlike a newline-joined shell listing).
#
# THE CLOSED KHRONOS LIST IS STILL DUPLICATED, NOT IMPORTED, from
# check_spdx.py's own KNOWN_KHRONOS_VENDOR_FILES - unchanged by this
# fatia (VENDOR-LIST-SIBLING, a separate TODO.md item, is what ties
# the two together with check_sibling_lists.py; out of scope here). If
# this three-entry list ever changes, update BOTH this file's
# KNOWN_VENDOR_FILES and check_spdx.py's KNOWN_KHRONOS_VENDOR_FILES.
#
# Usage:
#   check_vendor_purity.py <repo-root-directory>
#   check_vendor_purity.py --selftest
#
# --selftest runs the eight controls GODS_LAWS.md L-40 and this fatia's
# plan (SS5.3) require: positive, negative(E2: vendor-form directory),
# negative(E3: binary artifact), negative(E3: submodule marker), empty
# scan (universe, E1), empty scan (vendor exception vanished, DECISAO
# AUTONOMA 2), the ESCAPE control (widening the exception is only
# possible by editing this file's own source, never at the command
# line), and non-git (scan refusal, never presumed empty - the same
# discipline check_spdx.py's own --selftest already carries).
#
# Each function below does one thing (GODS_LAWS.md L-17).

import os
import shutil
import stat
import subprocess
import sys
import tempfile

SCRIPT_NAME = "check_vendor_purity.py"

# E2's exception list - DUPLICATED from check_spdx.py's own
# KNOWN_KHRONOS_VENDOR_FILES (see this file's header for why).
KNOWN_VENDOR_FILES = frozenset({
    "third_party/khronos/gl.xml",
    "third_party/khronos/LICENSE-APACHE-2.0.txt",
    "third_party/khronos/README.md",
})

# E3's exceptions. Both empty/False today, MEASURED (this file's
# header, "PORT LINEAGE" section; also `git ls-files | grep -Ei
# '\.(a|so|lib|dll|dylib|o|obj|zip|tar|gz|7z)$'` and `find ... -iname
# '.gitmodules'` both return nothing against the real tree, 21/09/2026)
# - never assumed. Widening either is ONLY possible by editing this
# file's own source (the ESCAPE control below proves it), the same
# escape check_dep_zero.py's FIND_PACKAGE_ALLOWLIST already uses.
KNOWN_BINARY_ARTIFACTS = frozenset()
ALLOW_GIT_SUBMODULES = False

# E2's closed vocabulary. Matched against a WHOLE path segment,
# case-folded (GODS_LAWS.md project L-04: same behaviour on every
# platform, including case-insensitive Windows filesystems), never a
# substring - "libs" does not match "lib", and "third_party_growth"
# (the container memory counter this repo already has, unrelated
# entirely) does not match "third_party" either.
#
# 'lib'/'libs' are DELIBERATELY left out: in a project that IS a
# library, they are common, legitimate directory names and would
# false-positive constantly. Registered here so nobody "completes" the
# list without thinking about it (this fatia's plan, SS3.3).
VENDOR_FORM_VOCABULARY = frozenset({
    "third_party", "thirdparty", "third-party",
    "3rdparty", "3rd_party", "3rd-party",
    "vendor", "vendors", "vendored",
    "external", "externals", "extern",
    "deps", "_deps", "dependencies",
    "contrib", "contribs",
    "submodule", "submodules", "subprojects",
    "imported", "bundled", "foreign", "upstream",
    "prebuilt", "prebuilts", "sdk", "sdks",
})

# E3's closed extension sets, case-folded. Checked in this order:
# multi-part archive suffixes first (".tar.gz" must not be classified
# by its final ".gz"-shaped component alone), then a single
# os.path.splitext() lookup against the two flat sets.
ARCHIVE_MULTI_SUFFIXES = (".tar.gz", ".tar.bz2", ".tar.xz")
BINARY_LIBRARY_EXTENSIONS = frozenset({
    ".a", ".lib", ".so", ".dylib", ".dll", ".o", ".obj",
})
ARCHIVE_PACKAGE_EXTENSIONS = frozenset({
    ".zip", ".tar", ".tgz", ".7z", ".rar", ".jar", ".whl", ".crate", ".gem",
})

FORM_VENDOR_DIRECTORY = "diretorio vendorizado"
FORM_BINARY_ARTIFACT = "artefato binario"
FORM_SUBMODULE = "submodulo"


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


# --- E1: the universe, and the GODS_LAWS.md L-40 scan floor ---------


def git_ls_files_z(root, extra_args):
    """Runs `git -C root ls-files -z <extra_args>`, decoded via the
    filesystem encoding with surrogateescape - the same tolerant,
    lossless round-trip check_spdx.py's own git_ls_files_z() already
    uses. Returns (paths, ok); ok is False when git itself refused
    (not a repository, or git missing) so the caller can turn that
    into an explicit, named refusal, never a silent empty result.
    """
    try:
        result = subprocess.run(
            ["git", "-C", root, "ls-files", "-z", *extra_args],
            capture_output=True,
        )
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
    paths = [chunk.decode(encoding, errors="surrogateescape") for chunk in raw.split(b"\0")]
    return paths, True


def scanned_repo_files(root):
    """E1: union of tracked and untracked-not-gitignored paths.
    Returns (tracked, untracked, ok).
    """
    tracked, ok = git_ls_files_z(root, [])
    if not ok:
        return [], [], False
    untracked, ok = git_ls_files_z(root, ["--others", "--exclude-standard"])
    if not ok:
        return [], [], False
    return tracked, untracked, True


# --- E2 + E3: the real filesystem walk -------------------------------


def is_build_output_dirname(name):
    """The one hard, explicit, always-reported prune (see this file's
    header). Matches 'build', 'build-static', 'build-preci-debug', ...
    """
    return name.casefold().startswith("build")


def is_vendor_form_segment(name):
    return name.casefold() in VENDOR_FORM_VOCABULARY


def artifact_form(relative_path):
    """Classifies a single path (POSIX form, relative to root) as a
    third-party artifact FORM, by name alone - content is never
    opened; that is a different question with its own dedicated gate
    (the sha256 check in src/render/CMakeLists.txt). Returns one of
    FORM_SUBMODULE / FORM_BINARY_ARTIFACT, or None.
    """
    basename = relative_path.rsplit("/", 1)[-1]
    folded = basename.casefold()
    if folded == ".gitmodules":
        return FORM_SUBMODULE
    for suffix in ARCHIVE_MULTI_SUFFIXES:
        if folded.endswith(suffix):
            return FORM_BINARY_ARTIFACT
    _stem, ext = os.path.splitext(folded)
    if ext in BINARY_LIBRARY_EXTENSIONS or ext in ARCHIVE_PACKAGE_EXTENSIONS:
        return FORM_BINARY_ARTIFACT
    return None


def _relative_posix(dirpath, root):
    rel = os.path.relpath(dirpath, root)
    return "" if rel == "." else rel.replace(os.sep, "/")


def _join_posix(rel_dir, name):
    return name if not rel_dir else f"{rel_dir}/{name}"


def _is_under_any(rel_path, ancestors):
    for ancestor in ancestors:
        if rel_path == ancestor or rel_path.startswith(ancestor + "/"):
            return True
    return False


def scan_tree(root):
    """Single os.walk pass from root, feeding BOTH E2 (vendor_files)
    and E3 (artifacts) at once - see this file's header for why E3 is
    a filesystem walk and not `git ls-files` filtered by extension.

    '.git' is never listed, never descended (not a vendor form; not
    a question this gate asks). Any 'build*' segment is pruned the
    same way, explicitly, and counted (pruned_build_dirs) - never a
    silent hole. Every OTHER directory is walked in full regardless of
    git-ignore status - deliberately (this file's header).

    A directory whose own segment matches VENDOR_FORM_VOCABULARY is
    NEVER pruned (matched or not), and every file physically under it
    - at any depth, including in a NESTED vendor-form directory - is
    E2 vendor_files. Since os.walk(topdown=True) visits a directory
    before its children, a segment discovered while processing its
    parent's dirnames is already in `vendor_dirs` by the time that
    child directory's own files are visited (_is_under_any() below).

    Returns (vendor_files, artifacts, pruned_build_dirs), all POSIX-
    relative to root, all de-duplicated and sorted.
    """
    vendor_dirs = []
    pruned_build_dirs = []
    vendor_files = []
    artifacts = []

    for dirpath, dirnames, filenames in os.walk(root, topdown=True):
        rel_dirpath = _relative_posix(dirpath, root)

        keep = []
        for name in dirnames:
            if name == ".git":
                continue
            if is_build_output_dirname(name):
                pruned_build_dirs.append(_join_posix(rel_dirpath, name))
                continue
            keep.append(name)
            if is_vendor_form_segment(name):
                vendor_dirs.append(_join_posix(rel_dirpath, name))
        dirnames[:] = keep

        under_vendor_dir = _is_under_any(rel_dirpath, vendor_dirs)
        for name in filenames:
            rel_path = _join_posix(rel_dirpath, name)
            if under_vendor_dir:
                vendor_files.append(rel_path)
            form = artifact_form(rel_path)
            if form is not None:
                artifacts.append((rel_path, form))

    return (
        sorted(set(vendor_files)),
        sorted(set(artifacts)),
        sorted(set(pruned_build_dirs)),
    )


# --- the single verdict -----------------------------------------------


def check_vendor_purity(root):
    """Mirrors the pre-widening function's own return-code contract:
    True (pass, prints the ok summary to stdout) or False (reprove,
    prints violations to stderr) - never raises for an ordinary
    reprove.
    """
    tracked, untracked, ok = scanned_repo_files(root)
    if not ok:
        print(
            f"{SCRIPT_NAME}: 'git ls-files' falhou em '{root}' (nao e "
            "repositorio git, ou git indisponivel) - varredura recusada, "
            "nunca presumida vazia",
            file=sys.stderr,
        )
        return False

    universe_count = len(tracked) + len(untracked)
    if universe_count == 0:
        print(
            f"{SCRIPT_NAME}: varredura vazia (0 arquivos rastreados ou "
            "nao rastreados em toda a arvore)",
            file=sys.stderr,
        )
        return False

    vendor_files, artifacts, pruned_build_dirs = scan_tree(root)

    # GODS_LAWS.md DECISAO AUTONOMA 2 (plan SS3.4): zero files under
    # ANY vendor-form directory is its own, DISTINCT reprove from the
    # universe-empty case above - the Khronos exception has three
    # files today; them vanishing is news, never "nothing to look at".
    if not vendor_files:
        print(
            f"{SCRIPT_NAME}: varredura vazia sob diretorio de forma "
            "vendorizada (0 arquivo(s) encontrado(s); a excecao "
            "GODS_LAWS.md L-07 No 1 tem tres arquivos hoje - se ela foi "
            "legitimamente fechada pelo lider, o caminho e editar "
            "KNOWN_VENDOR_FILES, nunca deixar a varredura silenciar)",
            file=sys.stderr,
        )
        return False

    violations = []
    for path in vendor_files:
        if path not in KNOWN_VENDOR_FILES:
            violations.append((path, FORM_VENDOR_DIRECTORY))
    for path, form in artifacts:
        if form == FORM_SUBMODULE and ALLOW_GIT_SUBMODULES:
            continue
        if form == FORM_BINARY_ARTIFACT and path in KNOWN_BINARY_ARTIFACTS:
            continue
        violations.append((path, form))

    if violations:
        violations = sorted(set(violations))
        print(
            f"{SCRIPT_NAME}: PROIBIDO (GODS_LAWS.md L-07: 'a excecao nao "
            f"e transitiva'): {len(violations)} caminho(s) de terceiro "
            "fora da lista fechada de excecoes:",
            file=sys.stderr,
        )
        for path, form in violations:
            print(f"  [{form}] {path}", file=sys.stderr)
        return False

    print(
        f"{SCRIPT_NAME}: {universe_count} caminho(s) varrido(s) na arvore, "
        f"{len(vendor_files)} sob diretorio de forma vendorizada, "
        f"{len(KNOWN_VENDOR_FILES)} na lista fechada - nenhum intruso "
        f"({len(pruned_build_dirs)} diretorio(s) build*/ podado(s))"
    )
    return True


# --- real mode -----------------------------------------------------------


def real_main(args):
    if len(args) != 1:
        fail("usage: check_vendor_purity.py <repo-root-directory>")
    root = args[0]
    if not os.path.isdir(root):
        fail(f"directory not found: {root}")
    if not check_vendor_purity(root):
        fail("arquivo de terceiro fora da excecao encontrado (ver mensagem acima)")


# --- fixtures and controls for --selftest -----------------------------


def make_scratch_workdir():
    # A hand written Unix path does not exist on every platform
    # (Windows has no /tmp). dir=os.environ.get("TMPDIR") without a
    # hardcoded fallback lets tempfile.mkdtemp fall through to
    # gettempdir(), which already checks TMPDIR/TEMP/TMP and then the
    # platform default.
    return tempfile.mkdtemp(
        prefix="glintfx-vendor-purity-selftest-",
        dir=os.environ.get("TMPDIR"),
    )


def _clear_readonly_and_retry(func, path, _exc_info_or_exc):
    """shutil.rmtree() onexc/onerror callback. DUPLICATED from
    check_spdx.py's own function of the same name (see that file's
    header on the house convention of duplicating small standalone-
    script helpers instead of sharing a module) - `git init` leaves
    read-only bits on Windows that a plain rmtree cannot remove.
    """
    for target in (os.path.dirname(path), path):
        if target and os.path.exists(target):
            try:
                os.chmod(target, stat.S_IWRITE | stat.S_IREAD | stat.S_IEXEC)
            except OSError:
                pass
    func(path)


def remove_tree_tolerant(path, ignore_errors=False):
    """DUPLICATED from check_spdx.py's own function of the same name -
    see there for the full measured account (GODS_LAWS.md L-40,
    04/09/2026)."""
    if not os.path.exists(path):
        return
    kwargs = {"onexc": _clear_readonly_and_retry} if sys.version_info >= (3, 12) else {"onerror": _clear_readonly_and_retry}
    try:
        shutil.rmtree(path, **kwargs)
    except OSError:
        if not ignore_errors:
            raise


def init_fixture_repo(root):
    """Every --selftest fixture is now a REAL git repository (E1
    needs `git ls-files` to work) - `git init` alone is enough, the
    controls never need a commit: an untracked-and-not-gitignored file
    already counts via `git ls-files --others --exclude-standard`,
    the same discipline check_spdx.py's own fixtures use.
    """
    os.makedirs(root, exist_ok=True)
    subprocess.run(["git", "-C", root, "init", "-q"], check=True)
    subprocess.run(["git", "-C", root, "config", "user.email", "selftest@check-vendor-purity.invalid"], check=True)
    subprocess.run(["git", "-C", root, "config", "user.name", "check_vendor_purity selftest"], check=True)


# Fixture content - the three files the exception named, nothing else.
# Fixture content, never the real gl.xml/LICENSE/README (does not need
# to be: this gate compares PATH, never content nor sha256 - that job
# belongs to the integrity gate in src/render/CMakeLists.txt).
def make_clean_fixture(root):
    vendor_dir = os.path.join(root, "third_party", "khronos")
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
    stderr redirected to an in-memory buffer - same shape check_spdx.py's
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


# Positive control: exactly the three named files. Expected: passes,
# and prints all three GODS_LAWS.md L-40 counts.
def selftest_positive_control(scratch, capture):
    root = os.path.join(scratch, "positive")
    init_fixture_repo(root)
    make_clean_fixture(root)

    outcome = capture(lambda: check_vendor_purity(root))
    if not outcome.result:
        print(
            "selftest: controle POSITIVO FALHOU (fixture limpa deveria ter sido aprovada)",
            file=sys.stderr,
        )
        print(outcome.text, file=sys.stderr)
        return False
    ok = True
    for needle in ("3 sob diretorio de forma vendorizada", "3 na lista fechada"):
        if needle not in outcome.text:
            print(
                f"selftest: controle POSITIVO FALHOU (aprovou, mas nao "
                f"imprimiu a contagem '{needle}')",
                file=sys.stderr,
            )
            ok = False
    if ok:
        print(
            "selftest: controle POSITIVO OK (exatamente os tres arquivos "
            "nomeados, nada mais, aprovado, tres contagens impressas)"
        )
    return ok


# Negative control, E2: the three legitimate files PLUS a vendor-form
# directory intruder elsewhere in the tree (src/vendor/, not
# third_party/). Expected: reproves, cites the intruder's exact path,
# names the form, never accuses any of the three legitimate ones.
def selftest_negative_vendor_dir_control(scratch, capture):
    root = os.path.join(scratch, "negative-vendor-dir")
    init_fixture_repo(root)
    make_clean_fixture(root)
    intruder_dir = os.path.join(root, "src", "vendor")
    os.makedirs(intruder_dir, exist_ok=True)
    with open(os.path.join(intruder_dir, "x.c"), "w", encoding="utf-8") as handle:
        handle.write("// SPDX-License-Identifier: AGPL-3.0-or-later\nint x;\n")

    outcome = capture(lambda: check_vendor_purity(root))
    if outcome.result:
        print(
            "selftest: controle NEGATIVO(E2) FALHOU (diretorio de forma "
            "vendorizada fora de third_party/ deveria ter sido reprovado)",
            file=sys.stderr,
        )
        print(outcome.text, file=sys.stderr)
        return False

    ok = True
    if "src/vendor/x.c" not in outcome.text or FORM_VENDOR_DIRECTORY not in outcome.text:
        print(
            "selftest: controle NEGATIVO(E2) FALHOU (reprovou, mas nao "
            "citou o caminho exato ou a forma)",
            file=sys.stderr,
        )
        ok = False
    for legitimo in KNOWN_VENDOR_FILES:
        if legitimo in outcome.text:
            print(
                f"selftest: controle NEGATIVO(E2) FALHOU (acusou o "
                f"arquivo legitimo '{legitimo}')",
                file=sys.stderr,
            )
            ok = False
    if ok:
        print(
            "selftest: controle NEGATIVO(E2) OK (src/vendor/x.c citado "
            "pelo caminho exato e pela forma, os tres legitimos intactos)"
        )
    return ok


# Negative control, E3-binary: the three legitimate files PLUS a
# binary library extension planted OUTSIDE any vendor-form directory
# (src/core/, an ordinary source directory). Expected: reproves,
# names the form and the exact path - and this is the control that
# would be structurally impossible under "E3 sobre E1" (see this
# file's header): .a is repository-wide gitignored.
def selftest_negative_binary_artifact_control(scratch, capture):
    root = os.path.join(scratch, "negative-binary")
    init_fixture_repo(root)
    make_clean_fixture(root)
    artifact_dir = os.path.join(root, "src", "core")
    os.makedirs(artifact_dir, exist_ok=True)
    with open(os.path.join(artifact_dir, "libfoo.a"), "wb") as handle:
        handle.write(b"0123456789")

    outcome = capture(lambda: check_vendor_purity(root))
    if outcome.result:
        print(
            "selftest: controle NEGATIVO(E3-binario) FALHOU (libfoo.a "
            "deveria ter sido reprovado)",
            file=sys.stderr,
        )
        print(outcome.text, file=sys.stderr)
        return False

    ok = True
    if "src/core/libfoo.a" not in outcome.text or FORM_BINARY_ARTIFACT not in outcome.text:
        print(
            "selftest: controle NEGATIVO(E3-binario) FALHOU (reprovou, "
            "mas nao citou o caminho exato ou a forma)",
            file=sys.stderr,
        )
        ok = False
    if ok:
        print(
            "selftest: controle NEGATIVO(E3-binario) OK (src/core/libfoo.a "
            "citado pelo caminho exato e pela forma 'artefato binario')"
        )
    return ok


# Negative control, E3-submodule: the three legitimate files PLUS a
# .gitmodules declaration at the repository root. Expected: reproves,
# names the form (submodulo) and the exact path.
def selftest_negative_submodule_control(scratch, capture):
    root = os.path.join(scratch, "negative-submodule")
    init_fixture_repo(root)
    make_clean_fixture(root)
    with open(os.path.join(root, ".gitmodules"), "w", encoding="utf-8") as handle:
        handle.write(
            '[submodule "evil"]\n\tpath = evil\n\turl = https://example.invalid/evil.git\n'
        )

    outcome = capture(lambda: check_vendor_purity(root))
    if outcome.result:
        print(
            "selftest: controle NEGATIVO(E3-submodulo) FALHOU "
            "(.gitmodules deveria ter sido reprovado)",
            file=sys.stderr,
        )
        print(outcome.text, file=sys.stderr)
        return False

    ok = True
    if ".gitmodules" not in outcome.text or FORM_SUBMODULE not in outcome.text:
        print(
            "selftest: controle NEGATIVO(E3-submodulo) FALHOU (reprovou, "
            "mas nao citou o caminho exato ou a forma)",
            file=sys.stderr,
        )
        ok = False
    if ok:
        print(
            "selftest: controle NEGATIVO(E3-submodulo) OK (.gitmodules "
            "citado pelo caminho exato e pela forma 'submodulo')"
        )
    return ok


# Empty-scan floor, universe (E1): a real git repository with
# literally zero files, not even the Khronos exception. Expected:
# reproves with "varredura vazia" AND the universe-specific wording -
# GODS_LAWS.md L-40's own reason to exist, never "nothing there, pass".
def selftest_empty_universe_control(scratch, capture):
    root = os.path.join(scratch, "empty-universe")
    init_fixture_repo(root)

    outcome = capture(lambda: check_vendor_purity(root))
    if outcome.result:
        print(
            "selftest: controle de VARREDURA VAZIA (universo) FALHOU "
            "(arvore sem nenhum arquivo deveria ter sido recusada)",
            file=sys.stderr,
        )
        print(outcome.text, file=sys.stderr)
        return False
    if "varredura vazia (0 arquivos rastreados ou nao rastreados em toda a arvore)" not in outcome.text:
        print(
            "selftest: controle de VARREDURA VAZIA (universo) FALHOU "
            "(recusou, mas nao com a mensagem do universo)",
            file=sys.stderr,
        )
        print(outcome.text, file=sys.stderr)
        return False
    print(
        "selftest: controle de VARREDURA VAZIA (universo) OK "
        "(arvore git sem nenhum arquivo recusada, nunca presumida ok)"
    )
    return True


# Empty-scan floor, vendor exception vanished (DECISAO AUTONOMA 2): a
# tree with ordinary files, but no third_party/ (or any other
# vendor-form directory) at all. Expected: reproves, with a message
# DISTINCT from the universe-empty one above.
def selftest_empty_vendor_scan_control(scratch, capture):
    root = os.path.join(scratch, "empty-vendor-scan")
    init_fixture_repo(root)
    ordinary_dir = os.path.join(root, "src")
    os.makedirs(ordinary_dir, exist_ok=True)
    with open(os.path.join(ordinary_dir, "main.cpp"), "w", encoding="utf-8") as handle:
        handle.write("// SPDX-License-Identifier: AGPL-3.0-or-later\nint main() { return 0; }\n")

    outcome = capture(lambda: check_vendor_purity(root))
    if outcome.result:
        print(
            "selftest: controle de VARREDURA VAZIA (excecao sumida) "
            "FALHOU (arvore sem third_party/ deveria ter sido recusada)",
            file=sys.stderr,
        )
        print(outcome.text, file=sys.stderr)
        return False
    if "varredura vazia sob diretorio de forma vendorizada" not in outcome.text:
        print(
            "selftest: controle de VARREDURA VAZIA (excecao sumida) "
            "FALHOU (recusou, mas nao com a mensagem distinta da "
            "excecao vendorizada)",
            file=sys.stderr,
        )
        print(outcome.text, file=sys.stderr)
        return False
    if "varredura vazia (0 arquivos rastreados ou nao rastreados em toda a arvore)" in outcome.text:
        print(
            "selftest: controle de VARREDURA VAZIA (excecao sumida) "
            "FALHOU (usou a mensagem do universo, as duas devem ser "
            "distintas)",
            file=sys.stderr,
        )
        return False
    print(
        "selftest: controle de VARREDURA VAZIA (excecao sumida) OK "
        "(third_party/ ausente recusado com mensagem propria, distinta "
        "da varredura vazia do universo)"
    )
    return True


# ESCAPE control (GODS_LAWS.md L-40 item 4 / this fatia's plan SS4.2):
# widening KNOWN_VENDOR_FILES is only possible by editing THIS FILE's
# own source - same shape check_dep_zero.py's own
# selftest_escape_via_allowlist_edit() already proved for
# FIND_PACKAGE_ALLOWLIST.
def selftest_escape_via_known_vendor_files_edit(scratch):
    root = os.path.join(scratch, "escape")
    init_fixture_repo(root)
    make_clean_fixture(root)
    intruder_dir = os.path.join(root, "third_party", "evil_lib")
    os.makedirs(intruder_dir, exist_ok=True)
    with open(os.path.join(intruder_dir, "evil.c"), "w", encoding="utf-8") as handle:
        handle.write("// SPDX-License-Identifier: AGPL-3.0-or-later\nint evil;\n")

    this_script = os.path.abspath(__file__)
    r = subprocess.run([sys.executable, this_script, root], capture_output=True)
    if r.returncode == 0:
        print(
            "selftest: ESCAPE control FAILED (third_party/evil_lib/ must "
            "be reproved BEFORE the allowlist edit)",
            file=sys.stderr,
        )
        return False

    original_text = open(this_script, "r", encoding="utf-8").read()
    needle = '"third_party/khronos/README.md",\n})'
    if needle not in original_text:
        print(
            "selftest: ESCAPE control FAILED (allowlist closing line not "
            "found - selftest itself is broken)",
            file=sys.stderr,
        )
        return False
    replacement = '"third_party/khronos/README.md",\n    "third_party/evil_lib/evil.c",\n})'
    edited_text = original_text.replace(needle, replacement)

    edited_path = os.path.join(scratch, "check_vendor_purity_edited.py")
    with open(edited_path, "w", encoding="utf-8") as handle:
        handle.write(edited_text)

    r = subprocess.run([sys.executable, edited_path, root], capture_output=True)
    if r.returncode != 0:
        print(
            "selftest: ESCAPE control FAILED (editing the allowlist is "
            "the documented escape and must pass)",
            file=sys.stderr,
        )
        print(r.stdout.decode("utf-8", "replace"), file=sys.stderr)
        print(r.stderr.decode("utf-8", "replace"), file=sys.stderr)
        return False
    print(
        "selftest: ESCAPE control OK (third_party/evil_lib/evil.c "
        "reproves; the SAME SCRIPT with that path added to "
        "KNOWN_VENDOR_FILES by editing this file's source passes - no "
        "other escape exists)"
    )
    return True


# Non-git control: a real directory with the clean fixture's files on
# disk, but never `git init`ed. Expected: reproves by scan refusal,
# never presumed empty - the same discipline check_spdx.py's own
# --selftest already carries.
def selftest_non_git_control(scratch, capture):
    root = os.path.join(scratch, "non-git")
    os.makedirs(root, exist_ok=True)
    make_clean_fixture(root)

    outcome = capture(lambda: check_vendor_purity(root))
    if outcome.result:
        print(
            "selftest: controle NAO-GIT FALHOU (diretorio sem .git "
            "deveria ter sido recusado, nunca presumido vazio-e-ok)",
            file=sys.stderr,
        )
        print(outcome.text, file=sys.stderr)
        return False
    if "repositorio git" not in outcome.text or "varredura recusada" not in outcome.text:
        print(
            "selftest: controle NAO-GIT FALHOU (recusou, mas nao com a "
            "mensagem de recusa de varredura)",
            file=sys.stderr,
        )
        print(outcome.text, file=sys.stderr)
        return False
    print(
        "selftest: controle NAO-GIT OK (diretorio sem .git recusado por "
        "'varredura recusada', nunca presumido vazio)"
    )
    return True


def selftest_main():
    scratch = make_scratch_workdir()
    capture = _make_capture()
    try:
        controls = [
            selftest_positive_control(scratch, capture),
            selftest_negative_vendor_dir_control(scratch, capture),
            selftest_negative_binary_artifact_control(scratch, capture),
            selftest_negative_submodule_control(scratch, capture),
            selftest_empty_universe_control(scratch, capture),
            selftest_empty_vendor_scan_control(scratch, capture),
            selftest_escape_via_known_vendor_files_edit(scratch),
            selftest_non_git_control(scratch, capture),
        ]
        if not all(controls):
            print("check_vendor_purity.py --selftest: FALHOU (ver acima)", file=sys.stderr)
            sys.exit(1)
        print(f"check_vendor_purity.py --selftest: os {len(controls)} controles OK")
    finally:
        remove_tree_tolerant(scratch, ignore_errors=True)


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
    else:
        real_main(args)


if __name__ == "__main__":
    main()
