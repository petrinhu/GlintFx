#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_version_matches_tag.py - CI gate for TODO.md item VERSION-TAG-
# SYNC (5.33, WM1). Ties the version declared in `project(glintfx
# VERSION A.B.C.D ...)` (CMakeLists.txt) to the git tag `vA.B.C.D`,
# so the declaration a consumer's `.pc`/CMake package reads can never
# silently drift from what got published.
#
# THE DEFECT THIS CLOSES, measured, not hypothetical: on 10/09/2026
# the declared version still read `0.3.0.0` while SEVEN tags had
# already been published on top of it (`v0.3.1.0` through
# `v0.3.7.0`), each one lying to the consumer about what it shipped.
# No mechanism anywhere in this tree noticed - `version_test` only
# proves header == generated macros == runtime string, never the tag.
# 119 commits on the main line carried this exact defect (measured
# against the real history, `git show <c>:CMakeLists.txt` vs `git
# describe --tags --match 'v[0-9]*' --abbrev=0 <c>`, one pair per
# commit from v0.2.0.0 to the fix in fbbbcd2).
#
# THE RULE (líder, 10/09/2026 - "sim, igualdade nos quatro
# componentes", reconfirmado 19/09/2026 via AskUserQuestion:
# "IGUALDADE NOS QUATRO NUMEROS" - registrado em ESCOPO.md SS3):
#
#   - HEAD carries a well-formed `v[0-9]+.[0-9]+.[0-9]+.[0-9]+` tag:
#     declared MUST equal that tag, all four components. This is the
#     clause that catches the real damage above.
#   - HEAD carries no such tag: declared MUST be >= the highest
#     reachable `v*` tag (numeric compare per component, never
#     string). Equal is the normal state between two tags; greater is
#     the legitimate "ahead, not yet tagged" state.
#   - Zero reachable `v[0-9]*` tags: REPROVES (GODS_LAWS.md L-40,
#     non-empty-scan floor - this repository has carried tags since
#     v0.2.0.0; seeing none means the checkout/scan is broken, not
#     that the rule does not apply) - UNLESS the checkout is a
#     shallow clone (`git rev-parse --is-shallow-repository`), in
#     which case zero is SKIPS DECLARED, never a pass and never a
#     fail. THE NARROW EXCEPTION, and why it stays narrow (GHA run
#     35455822838, 19/09/2026): 14 of this workflow's 15
#     `actions/checkout` steps ran with the actions/checkout default
#     (depth 1, no tags) while `version_matches_tag_test` is
#     registered unconditionally in the `consume` ctest label, so
#     those 14 jobs failed with `reachable_v_tags=0` on a tree that
#     genuinely has tags - the checkout, not the tree, was the
#     defect. "Zero because I could not look" (shallow: the ancestry
#     walk `--merged HEAD` cannot be trusted - a tag's target commit
#     may sit outside the fetched depth) is a DIFFERENT fact than
#     "zero because there is nothing to find" (full history, still
#     zero): the first is a broken instrument and reproves nothing;
#     the second is the L-40 floor doing its job and must keep
#     reproving. This is why the fix is BOTH sides at once (lider's
#     decision, same date): the 14 checkouts now also fetch full
#     history + tags (`fetch-depth: 0`, `fetch-tags: true`, matching
#     the `version-tag` job's own checkout at line ~3168) so the gate
#     has real data to judge in the common case, AND this shallow
#     check stays as the declared-skip escape hatch for whatever
#     checkout config drifts out of that guarantee later - a skip
#     that can never fire in normal CI is not a real skip, it is
#     dead code pretending to be one.
#   - A tag matching `v` + a digit that is NOT exactly four dot-
#     separated integers (`v0.4.0`, three components) counts as
#     malformed; if it sits on HEAD, REPROVES on form (GODS_LAWS.md
#     L-26 project law: the tag IS `vA.B.C.D`, never three).
#   - More than one well-formed `v*` tag on the same HEAD commit:
#     REPROVES (ambiguous - never happened, but a portao that would
#     silently pick one is worse than one that refuses to pick).
#
# WHAT THIS GATE DOES NOT COVER, BY DESIGN (declared, not silently
# out of scope): a declared version sitting ahead of the last tag for
# an unbounded time with nobody ever tagging it. That is not the
# damage this gate exists to catch (the library is announcing a
# number that was never published, not an old one) and deciding "how
# far ahead is too far" is release process (GODS_LAWS.md do projeto
# L-11: tag needs the leader's aval), not something a portao judges.
#
# PLUMBING ONLY, NEVER THE PORCELAIN VERB - this machine has a guard
# that blocks the literal `git tag` invocation (measured live,
# 19/09/2026: a `git tag --points-at HEAD` issued from the Bash tool
# was refused by a PreToolUse hook before it ever ran). This script
# never calls it, in either mode - real or --selftest fixtures build
# tag refs with `git update-ref`/`git hash-object -t tag`, the exact
# plumbing `git tag` itself uses internally. Reading `--tag`-shaped
# argv is intentionally NOT provided: this script only ever asks "at
# HEAD, right now" (--tree mode), which is what both the local ctest
# gate and the CI job triggered by `push: tags:` need - the pushed
# tag is what HEAD is checked out at.
#
# Modes:
#   check_version_matches_tag.py <repo-root-directory>
#   check_version_matches_tag.py --selftest
#
# Same shape as check_spdx.py/check_dash_pubdoc.py: real_main() vs
# selftest_main(), disposable git fixtures under tempfile.mkdtemp(),
# a capture() helper for stdout+stderr, GODS_LAWS.md L-17 (one
# function, one job).

import os
import re
import shutil
import stat
import subprocess
import sys
import tempfile

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="backslashreplace")
if hasattr(sys.stderr, "reconfigure"):
    sys.stderr.reconfigure(encoding="utf-8", errors="backslashreplace")

SCRIPT_NAME = "check_version_matches_tag.py"

# project(glintfx VERSION A.B.C.D ...) - single line in this tree
# today, but the regex does not require it to stay that way.
DECLARED_RE = re.compile(
    r"project\(\s*glintfx\s+VERSION\s+(\d+)\.(\d+)\.(\d+)\.(\d+)", re.IGNORECASE
)

# A tag is "in scope" for this gate the moment it starts with `v`
# followed by a digit (this is also the exact `refs/tags/v[0-9]*`
# glob passed to `git for-each-ref`) - `onda-w5` and similar never
# enter the scan at all, in scope or malformed.
WELL_FORMED_TAG_RE = re.compile(r"^v(\d+)\.(\d+)\.(\d+)\.(\d+)$")


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


# --- git plumbing (never `git tag`) -----------------------------------


def run_git(root, args, check=True):
    return subprocess.run(
        ["git", "-C", root, *args], capture_output=True, text=True, check=check
    )


def is_git_repo(root):
    try:
        result = run_git(root, ["rev-parse", "--git-dir"], check=False)
    except FileNotFoundError:
        return False
    return result.returncode == 0


def read_declared_version(root):
    """Reads CMakeLists.txt AS COMMITTED AT HEAD (`git show`, never the
    working tree) - a tag always points at a commit, so what a tag
    audits is what HEAD committed, not an uncommitted edit sitting in
    the working copy. Returns a 4-tuple of ints, or None if the file
    or the `project(glintfx VERSION ...)` line could not be found.
    """
    result = run_git(root, ["show", "HEAD:CMakeLists.txt"], check=False)
    if result.returncode != 0:
        return None
    match = DECLARED_RE.search(result.stdout)
    if not match:
        return None
    return tuple(int(g) for g in match.groups())


def all_scope_tags(root):
    """Every `refs/tags/v[0-9]*` ref: (name, commit_oid, well_formed,
    version_tuple_or_None). Dereferences annotated tags to their
    tagged commit via the `*objectname` atom - lightweight tags have
    an empty `*objectname`, so `objectname` itself is the commit.
    """
    result = run_git(
        root,
        [
            "for-each-ref",
            "--format=%(refname:short)\t%(objectname)\t%(*objectname)",
            "refs/tags/v[0-9]*",
        ],
        check=False,
    )
    if result.returncode != 0:
        return None
    entries = []
    for line in result.stdout.splitlines():
        if not line:
            continue
        name, oid, peeled = line.split("\t")
        commit_oid = peeled if peeled else oid
        match = WELL_FORMED_TAG_RE.match(name)
        version = tuple(int(g) for g in match.groups()) if match else None
        entries.append((name, commit_oid, match is not None, version))
    return entries


def reachable_tag_names(root):
    """Names of `refs/tags/v[0-9]*` refs that are ancestors of (or
    equal to) HEAD - `git for-each-ref --merged HEAD`.
    """
    result = run_git(
        root,
        ["for-each-ref", "--merged", "HEAD", "--format=%(refname:short)", "refs/tags/v[0-9]*"],
        check=False,
    )
    if result.returncode != 0:
        return None
    return {line for line in result.stdout.splitlines() if line}


def head_commit(root):
    result = run_git(root, ["rev-parse", "HEAD"], check=False)
    if result.returncode != 0:
        return None
    return result.stdout.strip()


def is_shallow_repo(root):
    """True when `root` is a shallow clone (`.git/shallow` present) -
    `git rev-parse --is-shallow-repository` prints exactly `true` or
    `false`. A shallow clone cannot be trusted to answer "which tags
    are reachable from HEAD" (the ancestry walk `--merged HEAD` needs
    every commit between the tag and HEAD, and a shallow fetch may
    have none of them) - see the header comment for the narrow
    exception this feeds. Never called for anything BUT deciding
    whether a zero-reachable-tags reading is "genuinely zero" or
    "instrument could not look" - it plays no role in the head-tagged
    branches above, which need no ancestry walk at all.
    """
    result = run_git(root, ["rev-parse", "--is-shallow-repository"], check=False)
    if result.returncode != 0:
        return False
    return result.stdout.strip() == "true"


# --- the rule -----------------------------------------------------------


def format_version(version):
    return ".".join(str(component) for component in version)


def check_version(root):
    if not is_git_repo(root):
        print(
            f"{SCRIPT_NAME}: '{root}' nao e um repositorio git - portao "
            "PULADO (tag so faz sentido com historico git; um tarball de "
            "consumidor nao carrega esta verificacao)"
        )
        return True

    declared = read_declared_version(root)
    if declared is None:
        fail(
            f"nao foi possivel ler 'project(glintfx VERSION A.B.C.D ...)' "
            f"de CMakeLists.txt no HEAD de '{root}' - erro de configuracao, "
            "nao de deriva de etiqueta"
        )

    all_tags = all_scope_tags(root)
    if all_tags is None:
        fail(f"'git for-each-ref' falhou em '{root}'")
    reachable_names = reachable_tag_names(root)
    if reachable_names is None:
        fail(f"'git for-each-ref --merged HEAD' falhou em '{root}'")
    head = head_commit(root)
    if head is None:
        fail(f"'git rev-parse HEAD' falhou em '{root}' (sem commit ainda?)")

    at_head = [entry for entry in all_tags if entry[1] == head]
    well_formed_at_head = [entry for entry in at_head if entry[2]]
    malformed_at_head = [entry for entry in at_head if not entry[2]]

    reachable_well_formed = [
        entry for entry in all_tags if entry[2] and entry[0] in reachable_names
    ]

    declared_str = format_version(declared)

    if malformed_at_head:
        names = ", ".join(entry[0] for entry in malformed_at_head)
        print(
            f"{SCRIPT_NAME}: declared={declared_str} head_tags_malformados=[{names}]",
        )
        print(
            f"{SCRIPT_NAME}: REPROVADO - etiqueta em HEAD nao tem a forma "
            "vA.B.C.D (GODS_LAWS.md L-26 do projeto: quatro componentes, "
            f"sempre): {names}",
            file=sys.stderr,
        )
        return False

    if len(well_formed_at_head) > 1:
        names = ", ".join(entry[0] for entry in well_formed_at_head)
        print(f"{SCRIPT_NAME}: declared={declared_str} head_tags=[{names}]")
        print(
            f"{SCRIPT_NAME}: REPROVADO - mais de uma etiqueta vA.B.C.D no "
            f"mesmo HEAD, ambiguo: {names}",
            file=sys.stderr,
        )
        return False

    if len(well_formed_at_head) == 1:
        tag_name, _, _, tag_version = well_formed_at_head[0]
        tag_str = format_version(tag_version)
        print(
            f"{SCRIPT_NAME}: declared={declared_str} head_tag={tag_name} "
            f"reachable_v_tags={len(reachable_well_formed)}"
        )
        if declared != tag_version:
            print(
                f"{SCRIPT_NAME}: REPROVADO - HEAD etiquetado {tag_name} "
                f"({tag_str}), mas project() declara {declared_str} - "
                "os quatro componentes tem de ser iguais (decisao do "
                "lider, ESCOPO.md SS3)",
                file=sys.stderr,
            )
            return False
        print(f"{SCRIPT_NAME}: OK - declared == head tag ({tag_str})")
        return True

    # HEAD sem etiqueta.
    if not reachable_well_formed:
        print(f"{SCRIPT_NAME}: declared={declared_str} reachable_v_tags=0")
        if is_shallow_repo(root):
            print(
                f"{SCRIPT_NAME}: PULADO - clone raso (git rev-parse "
                "--is-shallow-repository=true): a varredura de "
                "'--merged HEAD' nao pode ser confiada aqui, uma "
                "etiqueta pode existir fora do historico buscado - "
                "isto e 'nao pude olhar', nunca 'nao ha nada', entao "
                "o piso da GODS_LAWS.md L-40 nao se aplica (excecao "
                "estreita, ver o cabecalho deste arquivo)",
            )
            return True
        print(
            f"{SCRIPT_NAME}: REPROVADO - zero etiquetas vA.B.C.D "
            "alcancaveis a partir de HEAD, e o clone NAO e raso "
            "(piso de varredura nao-vazia, GODS_LAWS.md L-40 - "
            "repositorio sem nenhuma etiqueta publicada)",
            file=sys.stderr,
        )
        return False

    highest_name, _, _, highest_version = max(reachable_well_formed, key=lambda e: e[3])
    highest_str = format_version(highest_version)
    print(
        f"{SCRIPT_NAME}: declared={declared_str} head_tag=(nenhuma) "
        f"highest_reachable={highest_name} reachable_v_tags={len(reachable_well_formed)}"
    )
    if declared < highest_version:
        print(
            f"{SCRIPT_NAME}: REPROVADO - HEAD sem etiqueta declara "
            f"{declared_str}, abaixo da etiqueta alcancavel mais alta "
            f"{highest_name} ({highest_str}) - e exatamente a deriva "
            "medida em 10/09/2026 (119 commits, sete etiquetas erradas)",
            file=sys.stderr,
        )
        return False
    print(
        f"{SCRIPT_NAME}: OK - declared ({declared_str}) >= highest "
        f"reachable tag {highest_name} ({highest_str})"
    )
    return True


# --- real mode -----------------------------------------------------------


def real_main(args):
    if len(args) != 1:
        fail(f"usage: {SCRIPT_NAME} <repo-root-directory>")
    root = args[0]
    if not os.path.isdir(root):
        fail(f"directory not found: {root}")
    if not check_version(root):
        fail("versao declarada e etiqueta em desacordo (ver mensagem acima)")


# --- selftest fixtures --------------------------------------------------


def make_scratch_workdir():
    return tempfile.mkdtemp(prefix="glintfx-version-tag-selftest-")


def _clear_readonly_and_retry(func, path, _exc_info_or_exc):
    """DUPLICATED from check_spdx.py's own function of the same name -
    see that file's header for the full account of why both the
    failing path AND its parent need the chmod."""
    for target in (os.path.dirname(path), path):
        if target and os.path.exists(target):
            try:
                os.chmod(target, stat.S_IWRITE | stat.S_IREAD | stat.S_IEXEC)
            except OSError:
                pass
    func(path)


def remove_tree_tolerant(path, ignore_errors=False):
    """DUPLICATED from check_spdx.py's own function of the same name."""
    if not os.path.exists(path):
        return
    kwargs = {"onexc": _clear_readonly_and_retry} if sys.version_info >= (3, 12) else {"onerror": _clear_readonly_and_retry}
    try:
        shutil.rmtree(path, **kwargs)
    except OSError:
        if not ignore_errors:
            raise


def _run_quiet(args):
    """subprocess.run() with stdout/stderr captured (never inherited) -
    fixture plumbing (`git init`/`add`/`commit`/`update-ref`) has no
    business printing to this script's own terminal or --selftest
    capture buffer; a failure still surfaces via the raised
    CalledProcessError's .stdout/.stderr."""
    return subprocess.run(args, capture_output=True, text=True, check=True)


def init_fixture_repo(root):
    os.makedirs(root, exist_ok=True)
    _run_quiet(["git", "-C", root, "init", "-q"])
    # GATE-VERSION-TAG-SELFTEST-HOOK-ISOLATION: this machine sets
    # core.hooksPath GLOBALLY (~/.claude/githooks), which fires on
    # EVERY `git commit` anywhere, including a disposable fixture
    # repo under tempfile.mkdtemp() that has nothing to do with any
    # real project. Disabling it per-fixture-repo (local config
    # always wins over global, git-config(1)) keeps this selftest's
    # correctness independent of an unrelated global hook's behavior
    # - measured live, 19/09/2026: without this, the fixture commits
    # below print "encontrados=0 analisados=0 falharam=0" noise from
    # that hook chain straight to this script's own output.
    _run_quiet(["git", "-C", root, "config", "core.hooksPath", os.devnull])
    _run_quiet(["git", "-C", root, "config", "user.email", "selftest@check-version-tag.invalid"])
    _run_quiet(["git", "-C", root, "config", "user.name", "check_version_matches_tag selftest"])


def write_cmakelists(root, version, extra_comment=""):
    path = os.path.join(root, "CMakeLists.txt")
    with open(path, "w", encoding="utf-8") as handle:
        handle.write(f"cmake_minimum_required(VERSION 3.20)\nproject(glintfx VERSION {version} LANGUAGES CXX)\n{extra_comment}")


def commit_all(root, message):
    _run_quiet(["git", "-C", root, "add", "-A"])
    _run_quiet(["git", "-C", root, "commit", "-q", "-m", message])
    return _run_quiet(["git", "-C", root, "rev-parse", "HEAD"]).stdout.strip()


def create_lightweight_tag(root, name, commit_oid):
    """`git update-ref`, never `git tag` (this machine blocks the verb -
    see the file header). This is the exact plumbing `git tag <name>`
    uses under the hood for a lightweight tag."""
    _run_quiet(["git", "-C", root, "update-ref", f"refs/tags/{name}", commit_oid])


def create_annotated_tag(root, name, commit_oid, message="selftest tag"):
    """`git hash-object -t tag` + `git update-ref` - the plumbing an
    annotated `git tag -a` uses under the hood, never the porcelain
    verb itself.

    Fed as raw BYTES, never `text=True` + `str`: a git tag object's
    header is `\\n`-delimited and `fsck`-checked byte for byte. On
    Windows, `subprocess.run(text=True, input=<str>)` writes through
    an `io.TextIOWrapper(newline=None)`, which silently translates
    every `\\n` the caller wrote into `os.linesep` (`\\r\\n` there)
    before the child ever reads it - corrupting the object, so `git
    hash-object` refuses it with `fsck: unterminatedHeader`, exit 128
    (measured live on the Windows job of run 35670443221, GODS_LAWS.md
    L-04). Linux's `os.linesep` is `\\n`, a no-op, which is why this
    was invisible here. Binary mode (`input=<bytes>`, no `text=`) skips
    `TextIOWrapper` entirely and passes the bytes through unchanged on
    every platform - see `selftest_annotated_tag_survives_stdin_
    newline_translation` below for the regression control this earns."""
    tagger = "check_version_matches_tag selftest <selftest@check-version-tag.invalid> 0 +0000"
    content = f"object {commit_oid}\ntype commit\ntag {name}\ntagger {tagger}\n\n{message}\n"
    result = subprocess.run(
        ["git", "-C", root, "hash-object", "-w", "-t", "tag", "--stdin"],
        input=content.encode("utf-8"),
        capture_output=True,
        check=True,
    )
    tag_oid = result.stdout.decode("utf-8").strip()
    _run_quiet(["git", "-C", root, "update-ref", f"refs/tags/{name}", tag_oid])


class _Captured:
    __slots__ = ("result", "text")

    def __init__(self, result, text):
        self.result = result
        self.text = text


def _make_capture():
    import contextlib
    import io

    def capture(fn):
        buffer = io.StringIO()
        with contextlib.redirect_stdout(buffer), contextlib.redirect_stderr(buffer):
            result = fn()
        return _Captured(result, buffer.getvalue())

    return capture


# --- selftest controls ---------------------------------------------------


def selftest_head_tagged_equal(scratch, capture):
    root = os.path.join(scratch, "head-tagged-equal")
    init_fixture_repo(root)
    write_cmakelists(root, "0.3.7.0")
    c1 = commit_all(root, "v0.3.7.0")
    create_annotated_tag(root, "v0.3.7.0", c1)

    output = capture(lambda: check_version(root))
    if not output.result:
        print("selftest: controle HEAD-TAGGED-EQUAL FALHOU (declared == head tag deveria passar)", file=sys.stderr)
        return False
    print("selftest: controle HEAD-TAGGED-EQUAL OK (positivo)")
    return True


def selftest_head_tagged_behind_real_damage(scratch, capture):
    """O dano real medido em 10/09/2026: HEAD etiquetado v0.3.1.0,
    declarado ainda 0.3.0.0."""
    root = os.path.join(scratch, "head-tagged-behind")
    init_fixture_repo(root)
    write_cmakelists(root, "0.3.0.0")
    c1 = commit_all(root, "0.3.0.0, esquecido de subir")
    create_lightweight_tag(root, "v0.3.1.0", c1)

    output = capture(lambda: check_version(root))
    if output.result:
        print("selftest: controle HEAD-TAGGED-BEHIND (dano real) FALHOU (deveria reprovar)", file=sys.stderr)
        return False
    if "v0.3.1.0" not in output.text or "0.3.0.0" not in output.text:
        print("selftest: controle HEAD-TAGGED-BEHIND FALHOU (reprovou, mas nao citou etiqueta/declarada)", file=sys.stderr)
        return False
    print("selftest: controle HEAD-TAGGED-BEHIND OK (dano real de 10/09/2026 reproduzido e pego)")
    return True


def selftest_untagged_behind_highest(scratch, capture):
    """Os 119 commits: HEAD sem etiqueta, declarada abaixo da mais alta alcancavel."""
    root = os.path.join(scratch, "untagged-behind")
    init_fixture_repo(root)
    write_cmakelists(root, "0.3.0.0")
    c1 = commit_all(root, "0.3.0.0")
    create_annotated_tag(root, "v0.3.1.0", c1)
    write_cmakelists(root, "0.3.0.0", extra_comment="# commit seguinte, versao esquecida\n")
    commit_all(root, "ainda 0.3.0.0, um commit depois da etiqueta v0.3.1.0")

    output = capture(lambda: check_version(root))
    if output.result:
        print("selftest: controle UNTAGGED-BEHIND (os 119) FALHOU (deveria reprovar)", file=sys.stderr)
        return False
    print("selftest: controle UNTAGGED-BEHIND OK (os 119 commits reproduzidos e pegos)")
    return True


def selftest_untagged_ahead(scratch, capture):
    root = os.path.join(scratch, "untagged-ahead")
    init_fixture_repo(root)
    write_cmakelists(root, "0.3.0.0")
    c1 = commit_all(root, "0.3.0.0")
    create_annotated_tag(root, "v0.3.0.0", c1)
    write_cmakelists(root, "0.4.0.0")
    commit_all(root, "0.4.0.0 declarada, ainda sem etiqueta nova - a frente e legitimo")

    output = capture(lambda: check_version(root))
    if not output.result:
        print("selftest: controle UNTAGGED-AHEAD FALHOU (declarada a frente da mais alta deveria passar)", file=sys.stderr)
        return False
    print("selftest: controle UNTAGGED-AHEAD OK (a frente e o estado legitimo pre-etiqueta)")
    return True


def selftest_fourth_component_boundary(scratch, capture):
    """Um passo alem da fronteira (GODS_LAWS.md global L-43): so o
    quarto componente diverge - continua reprovando, os quatro
    componentes tem de bater, nunca so tres."""
    root = os.path.join(scratch, "fourth-component-boundary")
    init_fixture_repo(root)
    write_cmakelists(root, "0.4.0.1")
    c1 = commit_all(root, "0.4.0.1")
    create_annotated_tag(root, "v0.4.0.0", c1)

    output = capture(lambda: check_version(root))
    if output.result:
        print("selftest: controle FOURTH-COMPONENT-BOUNDARY FALHOU (0.4.0.1 contra v0.4.0.0 deveria reprovar - quarto componente diverge)", file=sys.stderr)
        return False
    print("selftest: controle FOURTH-COMPONENT-BOUNDARY OK (quarto componente sozinho ja reprova, decisao do lider de 19/09/2026)")
    return True


def selftest_zero_reachable_tags_floor(scratch, capture):
    root = os.path.join(scratch, "zero-reachable")
    init_fixture_repo(root)
    write_cmakelists(root, "0.1.0.0")
    commit_all(root, "sem etiqueta nenhuma ainda")

    output = capture(lambda: check_version(root))
    if output.result:
        print("selftest: controle ZERO-RECHABLE-TAGS-FLOOR FALHOU (zero etiquetas deveria reprovar pelo piso)", file=sys.stderr)
        return False
    if "piso" not in output.text and "L-40" not in output.text:
        print("selftest: controle ZERO-RECHABLE-TAGS-FLOOR FALHOU (reprovou, mas nao citou o piso)", file=sys.stderr)
        return False
    print("selftest: controle ZERO-RECHABLE-TAGS-FLOOR OK (piso de varredura nao-vazia)")
    return True


def selftest_shallow_clone_skip(scratch, capture):
    """A metade que ZERO-RECHABLE-TAGS-FLOOR acima NAO cobre: ali o
    repositorio tem historico completo e zero etiquetas e' o piso da
    L-40 fazendo o trabalho dele (REPROVA); aqui o zero vem de um
    clone GENUINAMENTE raso (`git clone --depth 1`, via `file://` para
    que o transporte nao caia no atalho local que a documentacao do
    git descreve, e que ignoraria `--depth`), onde o fetch nunca
    trouxe historico suficiente para a caminhada `--merged HEAD`
    responder direito - "nao pude olhar", nunca "nao ha nada" (ver o
    cabecalho do arquivo). Exige a escotilha PULO DECLARADO: nem passa
    silenciosamente (sucesso sem a palavra do pulo seria indistinguivel
    de um zero real, o defeito exato que a L-40 existe para matar),
    nem reprova (reprovar aqui puniria o checkout raso, nao a arvore).
    """
    source = os.path.join(scratch, "shallow-source")
    init_fixture_repo(source)
    write_cmakelists(source, "0.1.0.0")
    commit_all(source, "commit 1, sem etiqueta")
    write_cmakelists(source, "0.1.0.0", extra_comment="# segundo commit, ainda sem etiqueta\n")
    commit_all(source, "commit 2, ainda sem etiqueta")

    clone = os.path.join(scratch, "shallow-clone")
    _run_quiet(["git", "clone", "--quiet", "--depth", "1", f"file://{source}", clone])

    if not is_shallow_repo(clone):
        print(
            "selftest: controle SHALLOW-CLONE-SKIP FALHOU (fixture nao "
            "produziu um clone raso de verdade - 'git rev-parse "
            "--is-shallow-repository' nao disse 'true')",
            file=sys.stderr,
        )
        return False

    output = capture(lambda: check_version(clone))
    if not output.result:
        print(
            "selftest: controle SHALLOW-CLONE-SKIP FALHOU (clone raso "
            "sem etiqueta alcancavel deveria ser PULO DECLARADO, nunca "
            "reprovar)",
            file=sys.stderr,
        )
        return False
    if "PULADO" not in output.text or "raso" not in output.text:
        print(
            "selftest: controle SHALLOW-CLONE-SKIP FALHOU (passou, mas "
            "nao declarou o pulo - sucesso silencioso e indistinguivel "
            "de um zero real, exatamente o que a L-40 proibe)",
            file=sys.stderr,
        )
        return False
    print("selftest: controle SHALLOW-CLONE-SKIP OK (clone raso e pulo declarado, nunca sucesso silencioso)")
    return True


def selftest_malformed_tag_at_head(scratch, capture):
    root = os.path.join(scratch, "malformed-at-head")
    init_fixture_repo(root)
    write_cmakelists(root, "0.4.0.0")
    c1 = commit_all(root, "0.4.0.0")
    create_lightweight_tag(root, "v0.4.0", c1)  # tres componentes, forma errada

    output = capture(lambda: check_version(root))
    if output.result:
        print("selftest: controle MALFORMED-TAG-AT-HEAD FALHOU (v0.4.0 de tres componentes deveria reprovar pela forma)", file=sys.stderr)
        return False
    print("selftest: controle MALFORMED-TAG-AT-HEAD OK (etiqueta de tres componentes reprovada pela forma)")
    return True


def selftest_duplicate_tag_at_head(scratch, capture):
    root = os.path.join(scratch, "duplicate-at-head")
    init_fixture_repo(root)
    write_cmakelists(root, "0.4.0.0")
    c1 = commit_all(root, "0.4.0.0")
    create_annotated_tag(root, "v0.4.0.0", c1)
    create_lightweight_tag(root, "v0.4.0.1", c1)  # segunda etiqueta no MESMO commit

    output = capture(lambda: check_version(root))
    if output.result:
        print("selftest: controle DUPLICATE-TAG-AT-HEAD FALHOU (duas etiquetas no mesmo HEAD deveria reprovar, ambiguo)", file=sys.stderr)
        return False
    print("selftest: controle DUPLICATE-TAG-AT-HEAD OK (ambiguidade de duas etiquetas no mesmo commit reprovada)")
    return True


def selftest_numeric_not_string_compare(scratch, capture):
    """0.10.0.0 > 0.9.0.0 numericamente; uma comparacao de string
    faria "0.10.0.0" < "0.9.0.0" (o caractere '1' < '9')."""
    root = os.path.join(scratch, "numeric-compare")
    init_fixture_repo(root)
    write_cmakelists(root, "0.9.0.0")
    c1 = commit_all(root, "0.9.0.0")
    create_annotated_tag(root, "v0.9.0.0", c1)
    write_cmakelists(root, "0.10.0.0")
    commit_all(root, "0.10.0.0, a frente numericamente")

    output = capture(lambda: check_version(root))
    if not output.result:
        print("selftest: controle NUMERIC-NOT-STRING-COMPARE FALHOU (0.10.0.0 >= v0.9.0.0 numericamente, deveria passar)", file=sys.stderr)
        return False
    print("selftest: controle NUMERIC-NOT-STRING-COMPARE OK (0.10.0.0 > 0.9.0.0 por componente, nao por string)")
    return True


def selftest_not_a_repo(scratch, capture):
    root = os.path.join(scratch, "not-a-repo")
    os.makedirs(root, exist_ok=True)
    write_cmakelists(root, "0.1.0.0")

    output = capture(lambda: check_version(root))
    if not output.result:
        print("selftest: controle NOT-A-REPO FALHOU (diretorio sem .git deveria ser PULO declarado, nunca fatal)", file=sys.stderr)
        return False
    if "PULADO" not in output.text:
        print("selftest: controle NOT-A-REPO FALHOU (passou, mas nao declarou o motivo do pulo)", file=sys.stderr)
        return False
    print("selftest: controle NOT-A-REPO OK (sem .git = pulo declarado, nunca fatal, nunca silencioso)")
    return True


def _simulate_windows_stdin_newline_translation(real_run):
    """Returns a `subprocess.run` stand-in that reproduces, on ANY
    platform, exactly the corruption Windows performs and Linux does
    not: a text-mode stdin pipe (`text=True` fed a `str` `input`) is
    written through an `io.TextIOWrapper` with `newline=None`, which
    translates every `\\n` the caller wrote into `os.linesep` before
    the child process ever reads it. On Linux `os.linesep` is `\\n`
    (a no-op, which is why the real defect this guards against was
    invisible on this machine until it reached the Windows job of the
    CI matrix). `os.linesep` itself cannot be monkeypatched to prove
    this - CPython's `_io` C extension reads the OS-level constant at
    build time, not the Python attribute (measured directly: patching
    `os.linesep` and rerunning an unrelated `subprocess.run(text=True,
    input=...)` produces unchanged output). Intercepting `subprocess.
    run` at the call site sidesteps that and reproduces the OBSERVABLE
    corruption instead: any `str` `input` passed in text mode gets its
    `\\n` replaced by `\\r\\n`, byte-encoded, and handed to the REAL
    `subprocess.run` in binary mode - `bytes` `input` (the fixed path,
    immune by construction, same as a real binary-mode pipe) passes
    through untouched."""

    def run(*args, **kwargs):
        raw_input = kwargs.get("input")
        text_mode = bool(kwargs.get("text")) or bool(kwargs.get("universal_newlines"))
        if raw_input is not None and isinstance(raw_input, str) and text_mode:
            patched_kwargs = dict(kwargs)
            patched_kwargs["input"] = raw_input.replace("\n", "\r\n").encode("utf-8")
            patched_kwargs.pop("text", None)
            patched_kwargs.pop("universal_newlines", None)
            patched_kwargs.pop("encoding", None)
            result = real_run(*args, **patched_kwargs)
            if kwargs.get("text"):
                if isinstance(result.stdout, (bytes, bytearray)):
                    result.stdout = result.stdout.decode("utf-8")
                if isinstance(result.stderr, (bytes, bytearray)):
                    result.stderr = result.stderr.decode("utf-8")
            return result
        return real_run(*args, **kwargs)

    return run


def selftest_annotated_tag_survives_stdin_newline_translation(scratch, capture):
    """GODS_LAWS.md L-04 (paridade de comportamento entre sistemas):
    the defect measured on the Windows job of run 35670443221 was
    `create_annotated_tag` feeding `git hash-object -t tag --stdin` a
    `str` under `text=True` - on Windows this silently corrupts the
    tag object's `\\n`-delimited header into `\\r\\n`, and `git`
    refuses it with `fsck: unterminatedHeader`, exit 128 (reproduced
    directly against the real `git` binary on THIS machine by feeding
    it CRLF content by hand, before writing this control). This
    control catches a REGRESSION back to that path without needing a
    Windows machine: it wraps `subprocess.run` so any `str` input fed
    in text mode is corrupted exactly like Windows would corrupt it,
    then calls the real `create_annotated_tag` and demands it still
    succeeds - which it only can if it feeds `bytes`, never `str`
    under `text=True`, to the tag object's stdin."""
    root = os.path.join(scratch, "annotated-tag-newline-translation")
    init_fixture_repo(root)
    write_cmakelists(root, "0.5.0.0")
    c1 = commit_all(root, "0.5.0.0")

    real_run = subprocess.run
    subprocess.run = _simulate_windows_stdin_newline_translation(real_run)
    try:
        create_annotated_tag(root, "v0.5.0.0", c1)
    except subprocess.CalledProcessError as error:
        print(
            "selftest: controle ANNOTATED-TAG-SURVIVES-STDIN-NEWLINE-TRANSLATION FALHOU "
            f"(create_annotated_tag corrompeu sob traducao de stdin simulada do Windows: {error.stderr!r})",
            file=sys.stderr,
        )
        return False
    finally:
        subprocess.run = real_run

    output = capture(lambda: check_version(root))
    if not output.result:
        print(
            "selftest: controle ANNOTATED-TAG-SURVIVES-STDIN-NEWLINE-TRANSLATION FALHOU "
            "(etiqueta sobreviveu a criacao mas o portao nao a leu de volta)",
            file=sys.stderr,
        )
        return False
    print("selftest: controle ANNOTATED-TAG-SURVIVES-STDIN-NEWLINE-TRANSLATION OK (stdin binario, imune a traducao de nova linha)")
    return True


def selftest_main():
    scratch = make_scratch_workdir()
    capture = _make_capture()
    try:
        controls = [
            selftest_head_tagged_equal(scratch, capture),
            selftest_head_tagged_behind_real_damage(scratch, capture),
            selftest_untagged_behind_highest(scratch, capture),
            selftest_untagged_ahead(scratch, capture),
            selftest_fourth_component_boundary(scratch, capture),
            selftest_zero_reachable_tags_floor(scratch, capture),
            selftest_shallow_clone_skip(scratch, capture),
            selftest_malformed_tag_at_head(scratch, capture),
            selftest_duplicate_tag_at_head(scratch, capture),
            selftest_numeric_not_string_compare(scratch, capture),
            selftest_not_a_repo(scratch, capture),
            selftest_annotated_tag_survives_stdin_newline_translation(scratch, capture),
        ]
        expected = 12
        if len(controls) != expected:
            print(f"check_version_matches_tag.py --selftest: FALHOU (piso do selftest: {len(controls)} executados, {expected} esperados)", file=sys.stderr)
            sys.exit(1)
        if not all(controls):
            print("check_version_matches_tag.py --selftest: FALHOU (ver acima)", file=sys.stderr)
            sys.exit(1)
        print(f"check_version_matches_tag.py --selftest: controles: {len(controls)} executados, {expected} esperados - todos OK")
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
