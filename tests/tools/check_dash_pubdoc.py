#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_dash_pubdoc.py - CI gate for the house rule against the long
# dash (em dash, U+2014 "-") in text this project's own CLAUDE.md
# (project rule, "documento se decide pelo LEITOR") declares written
# for the EXTERNAL consumer, never for us. GODS_LAWS.md L-32 (global)
# already forbids this character in "texto user-facing"; the finding
# this gate closes (docs/decisoes-inbox-tres.md, item DASH-PUBDOC) is
# that the only mechanism enforcing L-32 today lives OUTSIDE this
# repository (~/.claude/hooks/no_mdash.py and
# ~/.claude/githooks/no_mdash_staged.py), classifies "public" by a
# name/folder list that does not know this project's own rule of who
# reads which document, never runs on the server, and does not travel
# with a clone - measured live: docs/api-conventions.md (70 dashes)
# and docs/gl-loop-portability-matrix.md (15) both declare themselves
# written for "a consumer of this library" and neither global hook's
# classifier matches `docs/*.md`.
#
# SAME SHAPE as tests/tools/check_spdx.py (copied on purpose, per the
# leader's decision recorded in docs/decisoes-inbox-tres.md §3.6): a
# portao that enumerates the WHOLE repository via `git ls-files -z`
# (tracked) and `git ls-files -z --others --exclude-standard`
# (untracked, not gitignored), registered as an ordinary, unguarded
# ctest case so it runs inside the SAME ctest invocation the five
# platforms already execute, chained into tools/git-hooks/pre-commit,
# listed in check_precommit_hook_chain.py's REQUIRED_GATES, and
# exercised by tools/preci.sh through the ctest stage it already runs
# (no dedicated preci.sh stanza needed - same as spdx_test/vendor_
# purity_test/readme_volatile_numbers_test, none of which get one
# either).
#
# SCOPE: only `.md` and `.txt` files are IN SCOPE at all (documentation
# and free text - never code, build, or config). Every other extension
# is out of scope by construction, not "exempt" (a distinct category:
# exempt is a document THAT IS in scope but declared internal).
#
# EXEMPTIONS - a CLOSED, NAMED enumeration (GODS_LAWS.md L-40 item 5:
# a small, enumerable exception space is enumerated whole, never
# matched by a directory-wide pattern), decided in docs/decisoes-
# inbox-tres.md §3.6 by the reader each document names under this
# project's own L-21 ("documento se decide pelo LEITOR"): the
# top-level canon files this project's own CLAUDE.md table already
# lists as leader/agent-facing, plus three closed docs/ prefixes for
# working documents this project writes to itself (plans, decisions,
# audits). A NEGATIVE pattern (public by default, exception named),
# never a positive "public documents" list - the same discipline
# check_spdx.py's own KNOWN_KHRONOS_VENDOR_FILES comment argues for:
# a document born tomorrow with a new name is covered by default,
# which is the exact way the GLOBAL hook's name/folder classifier
# failed here.
#
# Each function below does one thing (GODS_LAWS.md L-17).

import os
import shutil
import stat
import subprocess
import sys
import tempfile

SCRIPT_NAME = "check_dash_pubdoc.py"
FORBIDDEN_CHAR = "—"  # em dash

# Exact top-level filenames this project's own CLAUDE.md ("Autoridade
# documental" table) and GODS_LAWS.md L-21 ("documento se decide pelo
# LEITOR") already name as written for the leader or for agents
# working on this project, never for the external consumer.
EXEMPT_EXACT = frozenset(
    {
        "GODS_LAWS.md",
        "CONTRACT.md",
        "TESTES.md",
        "AUDITORIAS.md",
        "AGILE.md",
        "TODO.md",
        "DECISOES_AUTONOMAS.md",
        "CLAUDE.md",
        "ORG.md",
        "pipeline_release_1.0.md",
        "lideranca_pipeline_release.md",
        "TOOLING.md",
        "DEPLOY_CHECKLIST.md",
        "ESCOPO.md",
        # GAP measured against docs/decisoes-inbox-tres.md's own closed
        # enumeration (SS3.6): that decision names three docs/ PREFIXES
        # (plano-, decisoes-, auditoria-), but its own SS3.2 fact table
        # already classifies docs/gfss-property-registry-v1.md's reader
        # as "nos (pt-br, especificacao de origem da fatia)" - the
        # SAME internal-reader category the three prefixes exist to
        # cover - and its own SS3.6 marks this exact file as
        # "[INFERENCIA] cai hoje na excecao pelo proprio cabecalho",
        # yet no prefix or exact name in the closed list actually
        # matches "gfss-property-registry-v1.md". Proving this gate
        # red against the real tree (GODS_LAWS.md L-36) surfaced the
        # gap live: without this line the gate also reproves this
        # pt-br, agent-facing spec document, which nobody asked to be
        # translated for the external consumer. Added here as its own
        # EXACT path (never a new prefix - this is one document, not a
        # new open-ended pattern) so the closed-enumeration mechanism
        # itself stays exactly what SS3.6 asked for ("listados
        # explicitamente por caminho ou padrao fechado"). The
        # decision's own warning still holds and is repeated here: the
        # day DOCS-PUB translates this file for the external consumer,
        # this line must be deleted in the SAME commit, or the
        # translated text keeps a false pass.
        "docs/gfss-property-registry-v1.md",
    }
)

# Three closed docs/ prefixes for this project's own working
# documents (plans, decisions, audits) - internal by their own
# declared readership, per docs/decisoes-inbox-tres.md §3.6.
EXEMPT_DOCS_PREFIXES = ("docs/plano-", "docs/decisoes-", "docs/auditoria-")


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


# --- classification, closed by construction (GODS_LAWS.md L-40 item 5) ---


def in_scope(path):
    """Only `.md` and `.txt` are ever in scope - everything else
    (code, build, config, binary) is out of scope by construction,
    never reported as exempt (a distinct category below)."""
    return path.endswith(".md") or path.endswith(".txt")


def is_exempt(path):
    """path is already known to be in_scope() when this is called."""
    if path in EXEMPT_EXACT:
        return True
    if path.endswith(".md") and any(path.startswith(prefix) for prefix in EXEMPT_DOCS_PREFIXES):
        return True
    return False


# --- enumeration, identical mechanism to check_spdx.py -------------------


def git_ls_files_z(root, extra_args):
    """Runs `git -C root ls-files -z <extra_args>`, decoded via the
    filesystem encoding with surrogateescape - never crashes on a byte
    sequence that is not valid UTF-8. Returns (paths, ok); ok is False
    when git itself failed to run (not a git repository, or git
    missing) - the caller turns that into an explicit, named failure,
    never a silent empty result. DUPLICATED from check_spdx.py's own
    function of the same name (house convention: standalone scripts
    duplicate small helpers instead of sharing a module - see that
    file's own header)."""
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


def scanned_files(root):
    """The closed-by-construction universe: every path `git ls-files`
    (tracked) and `git ls-files --others --exclude-standard`
    (untracked, not gitignored) report. Returns (tracked, untracked, ok).
    """
    tracked, ok = git_ls_files_z(root, [])
    if not ok:
        return [], [], False
    untracked, ok = git_ls_files_z(root, ["--others", "--exclude-standard"])
    if not ok:
        return [], [], False
    return tracked, untracked, True


def count_dashes(root, path):
    """Reads root/path as UTF-8 (surrogateescape: a byte sequence that
    is not valid UTF-8 never crashes the gate) and returns the list of
    1-based line numbers containing at least one FORBIDDEN_CHAR, each
    paired with the count on that line. Returns None on an open/read
    failure - fail-closed, the same shape check_spdx.py's
    file_has_header() uses: a file the engine could not open is never
    silently "clean", it is its own, separately reported reason to
    reprove."""
    file_path = os.path.join(root, *path.split("/"))
    try:
        with open(file_path, "r", encoding="utf-8", errors="surrogateescape") as handle:
            lines = handle.readlines()
    except OSError:
        return None
    hits = []
    for line_number, line in enumerate(lines, start=1):
        occurrences = line.count(FORBIDDEN_CHAR)
        if occurrences:
            hits.append((line_number, occurrences))
    return hits


# --- checking --------------------------------------------------------------


def check_dash_pubdoc(root):
    tracked, untracked, ok = scanned_files(root)
    if not ok:
        print(
            f"{SCRIPT_NAME}: 'git ls-files' falhou em '{root}' (nao e "
            "repositorio git, ou git indisponivel) - varredura recusada, "
            "nunca presumida vazia",
            file=sys.stderr,
        )
        return False

    total_count = len(tracked) + len(untracked)
    if total_count == 0:
        print(f"{SCRIPT_NAME}: varredura vazia (0 arquivos rastreados ou nao rastreados)", file=sys.stderr)
        return False

    entries = [(p, False) for p in tracked] + [(p, True) for p in untracked]

    in_scope_count = 0
    exempt_count = 0
    analyzed_count = 0
    failed = []
    offenders = []  # (path, [(line, count), ...], is_untracked)

    for path, is_untracked in entries:
        if not in_scope(path):
            continue
        in_scope_count += 1
        if is_exempt(path):
            exempt_count += 1
            continue
        hits = count_dashes(root, path)
        if hits is None:
            failed.append(path)
            continue
        analyzed_count += 1
        if hits:
            offenders.append((path, hits, is_untracked))

    if in_scope_count == 0:
        print(
            f"{SCRIPT_NAME}: varredura vazia (0 arquivo .md/.txt entre "
            f"{total_count} rastreado(s)/nao-rastreado(s))",
            file=sys.stderr,
        )
        return False

    if failed:
        print(
            f"{SCRIPT_NAME}: varredura incompleta - {len(failed)} "
            f"arquivo(s) recusaram abrir, {analyzed_count}/{in_scope_count - exempt_count} "
            "analisados (GODS_LAWS.md L-40 fail-closed):",
            file=sys.stderr,
        )
        for path in failed:
            print(f"{SCRIPT_NAME}: {root}/{path}: open refused", file=sys.stderr)
        if offenders:
            _print_offenders(root, offenders)
        return False

    if offenders:
        total_occurrences = sum(count for _, hits, _ in offenders for _, count in hits)
        print(
            f"{SCRIPT_NAME}: PROIBIDO (GODS_LAWS.md L-32, este projeto CLAUDE.md "
            f"'Autoridade documental'): {total_occurrences} travessao(oes) longo(s) "
            f"(U+2014) em {len(offenders)} documento(s) publico(s):",
            file=sys.stderr,
        )
        _print_offenders(root, offenders)
        return False

    print(
        f"{SCRIPT_NAME}: 0 travessao(oes) longo(s) em documento publico "
        f"({in_scope_count} .md/.txt no total, {exempt_count} isento(s), "
        f"{analyzed_count} analisado(s), {len(tracked)} rastreado(s), "
        f"{len(untracked)} nao rastreado(s))"
    )
    return True


def _print_offenders(root, offenders):
    for path, hits, is_untracked in offenders:
        suffix = " (nao rastreado)" if is_untracked else ""
        occurrence_count = sum(count for _, count in hits)
        lines_text = ", ".join(f"linha {line} ({count}x)" for line, count in hits)
        print(f"{SCRIPT_NAME}: {root}/{path}{suffix}: {occurrence_count} travessao(oes) - {lines_text}", file=sys.stderr)


# --- real mode -----------------------------------------------------------


def real_main(args):
    if len(args) != 1:
        fail("usage: check_dash_pubdoc.py <repo-root-directory>")
    root = args[0]
    if not os.path.isdir(root):
        fail(f"directory not found: {root}")
    if not check_dash_pubdoc(root):
        fail("travessao longo (U+2014) em documento publico nao isento (ver mensagem acima)")


# --- selftest fixtures and controls -----------------------------------


def make_scratch_workdir():
    return tempfile.mkdtemp(prefix="glintfx-dash-pubdoc-selftest-")


def _clear_readonly_and_retry(func, path, _exc_info_or_exc):
    """DUPLICATED from check_spdx.py's own function of the same name -
    see that file's header on the house convention, and check_dep_
    zero.py's own comment on why BOTH the failing path and its parent
    need the chmod (Windows blocks on the FILE's own read-only
    attribute; POSIX blocks on the PARENT directory's write bit)."""
    for target in (os.path.dirname(path), path):
        if target and os.path.exists(target):
            try:
                os.chmod(target, stat.S_IWRITE | stat.S_IREAD | stat.S_IEXEC)
            except OSError:
                pass
    func(path)


def remove_tree_tolerant(path, ignore_errors=False):
    """DUPLICATED from check_spdx.py's own function of the same name -
    see that file's header for the full measured account of why plain
    shutil.rmtree(ignore_errors=True) is not enough on Windows."""
    if not os.path.exists(path):
        return
    kwargs = {"onexc": _clear_readonly_and_retry} if sys.version_info >= (3, 12) else {"onerror": _clear_readonly_and_retry}
    try:
        shutil.rmtree(path, **kwargs)
    except OSError:
        if not ignore_errors:
            raise


def init_fixture_repo(root):
    os.makedirs(root, exist_ok=True)
    subprocess.run(["git", "-C", root, "init", "-q"], check=True)
    subprocess.run(["git", "-C", root, "config", "user.email", "selftest@check-dash-pubdoc.invalid"], check=True)
    subprocess.run(["git", "-C", root, "config", "user.name", "check_dash_pubdoc selftest"], check=True)


def track_all(root):
    subprocess.run(["git", "-C", root, "add", "-A"], check=True)


def write_file(root, relative_path, content):
    full_path = os.path.join(root, *relative_path.split("/"))
    os.makedirs(os.path.dirname(full_path), exist_ok=True)
    with open(full_path, "w", encoding="utf-8") as handle:
        handle.write(content)


class Capture:
    def __init__(self, result, text):
        self.result = result
        self.text = text


def capture_stderr(callable_):
    import contextlib
    import io

    buffer = io.StringIO()
    with contextlib.redirect_stderr(buffer):
        result = callable_()
    return Capture(result, buffer.getvalue())


# Positive fixture: clean public doc, exempt canon file WITH a dash
# (must not be charged), non-.md/.txt file with a dash (out of scope,
# must not be charged either).
def make_positive_fixture(root):
    write_file(root, "docs/api-conventions.md", "# api\n\nNo dash here, only a comma, and a period.\n")
    write_file(root, "GODS_LAWS.md", "Lei interna com travessao — permitido aqui, leitor e' o proprio time.\n")
    write_file(root, "docs/plano-onda.md", "Plano interno com travessao — tambem permitido.\n")
    write_file(root, "src/foo.cpp", "// comentario com travessao — fora de escopo, nao e .md/.txt\n")
    track_all(root)


def selftest_positive_control(scratch):
    root = os.path.join(scratch, "positive")
    init_fixture_repo(root)
    make_positive_fixture(root)
    if check_dash_pubdoc(root):
        print(
            "selftest: controle POSITIVO OK (doc publico limpo passa; "
            "GODS_LAWS.md e docs/plano-*.md com travessao nao sao cobrados; "
            "src/foo.cpp com travessao fora de escopo nao e cobrado)"
        )
        return True
    print("selftest: controle POSITIVO FALHOU", file=sys.stderr)
    return False


def selftest_negative_control(scratch):
    root = os.path.join(scratch, "negative")
    init_fixture_repo(root)
    make_positive_fixture(root)
    write_file(root, "docs/gl-loop-portability-matrix.md", "Linha limpa.\nLinha com travessao — aqui, duas vezes — nesta linha.\n")
    track_all(root)

    output = capture_stderr(lambda: check_dash_pubdoc(root))
    if output.result:
        print("selftest: controle NEGATIVO FALHOU (deveria ter reprovado docs/gl-loop-portability-matrix.md)", file=sys.stderr)
        return False

    ok = True
    if "docs/gl-loop-portability-matrix.md" not in output.text:
        print("selftest: controle NEGATIVO FALHOU (nao citou o arquivo ofensor)", file=sys.stderr)
        ok = False
    if "linha 2 (2x)" not in output.text:
        print("selftest: controle NEGATIVO FALHOU (nao contou as 2 ocorrencias na linha 2)", file=sys.stderr)
        ok = False
    # "GODS_LAWS.md" nao entra nesta lista: a propria mensagem de
    # reprovacao cita "GODS_LAWS.md L-32" (a lei, nao o arquivo), entao
    # o nome aparece no texto de qualquer reprovacao por construcao -
    # nao seria um teste do isento, seria um falso-positivo do proprio
    # selftest. O isento GODS_LAWS.md ja e' coberto por
    # selftest_positive_control (onde ele tem travessao e o controle
    # POSITIVO exige que o portao ainda passe).
    for isento in ("docs/plano-onda.md", "src/foo.cpp"):
        if isento in output.text:
            print(f"selftest: controle NEGATIVO FALHOU (cobrou '{isento}', que deveria estar isento/fora de escopo)", file=sys.stderr)
            ok = False
    if ok:
        print("selftest: controle NEGATIVO OK (arquivo ofensor citado com a linha e a contagem certas, nada isento cobrado)")
    return ok


def selftest_exempt_exact_boundary_control(scratch):
    """Dois quase-isentos, nenhum dos dois o isento exato, precisam
    ser cobrados - a mesma disciplina de casamento exato de caminho
    que check_spdx.py's own comment exige (README.md nao esconde
    README.md.bak): 'docs/planoZZZ.md' nao comeca com o prefixo
    'docs/plano-' (falta o hifen logo apos 'plano'), e 'sub/TODO.md'
    tem o MESMO nome de arquivo que o isento exato 'TODO.md', mas em
    outro caminho - EXEMPT_EXACT casa o CAMINHO inteiro, nunca so o
    nome final."""
    root = os.path.join(scratch, "exempt_boundary")
    init_fixture_repo(root)
    write_file(root, "docs/planoZZZ.md", "Nao comeca com 'docs/plano-', deve ser cobrado — aqui.\n")
    write_file(root, "sub/TODO.md", "Mesmo nome do isento exato, caminho diferente, deve ser cobrado — aqui.\n")
    track_all(root)

    output = capture_stderr(lambda: check_dash_pubdoc(root))
    if output.result:
        print("selftest: controle EXEMPT-BOUNDARY FALHOU (deveria ter reprovado os dois arquivos)", file=sys.stderr)
        return False
    ok = True
    for exigido in ("docs/planoZZZ.md", "sub/TODO.md"):
        if exigido not in output.text:
            print(f"selftest: controle EXEMPT-BOUNDARY FALHOU (nao citou '{exigido}')", file=sys.stderr)
            ok = False
    if ok:
        print("selftest: controle EXEMPT-BOUNDARY OK (quase-isento por prefixo e por nome-em-outro-caminho, nenhum dos dois e o isento exato, continuam exigindo)")
    return ok


def selftest_empty_scan_control(scratch):
    root = os.path.join(scratch, "empty")
    init_fixture_repo(root)
    write_file(root, "src/only.cpp", "int f();\n")
    track_all(root)

    output = capture_stderr(lambda: check_dash_pubdoc(root))
    if output.result:
        print("selftest: controle de VARREDURA VAZIA FALHOU (repo sem .md/.txt deveria ter sido recusado)", file=sys.stderr)
        return False
    if "varredura vazia" not in output.text:
        print("selftest: controle de VARREDURA VAZIA FALHOU (recusou, mas nao disse 'varredura vazia')", file=sys.stderr)
        return False
    print("selftest: controle de VARREDURA VAZIA OK (repo sem nenhum .md/.txt recusado)")
    return True


def selftest_not_a_repo_control(scratch):
    root = os.path.join(scratch, "not_a_repo")
    write_file(root, "README.md", "sem repo git por baixo\n")

    output = capture_stderr(lambda: check_dash_pubdoc(root))
    if output.result:
        print("selftest: controle de NAO-E-REPO FALHOU (diretorio sem .git deveria ter sido recusado)", file=sys.stderr)
        return False
    if "nao e" not in output.text or "repositorio git" not in output.text:
        print("selftest: controle de NAO-E-REPO FALHOU (recusou, mas nao disse o motivo)", file=sys.stderr)
        return False
    print("selftest: controle de NAO-E-REPO OK (diretorio sem .git recusado, nao presumido vazio)")
    return True


def selftest_untracked_control(scratch):
    """GATE-SPDX-UNTRACKED's same fix, proved here too: um arquivo
    criado e nunca `git add`ado precisa ser cobrado igual, nao invisivel
    ate ser staged."""
    root = os.path.join(scratch, "untracked")
    init_fixture_repo(root)
    write_file(root, "README.md", "limpo\n")
    track_all(root)
    write_file(root, "docs/novo-doc-publico.md", "Travessao — aqui, nunca staged.\n")

    output = capture_stderr(lambda: check_dash_pubdoc(root))
    if output.result:
        print("selftest: controle UNTRACKED FALHOU (arquivo nao rastreado com travessao deveria reprovar)", file=sys.stderr)
        return False
    if "docs/novo-doc-publico.md" not in output.text or "nao rastreado" not in output.text:
        print("selftest: controle UNTRACKED FALHOU (nao citou o arquivo nao rastreado como tal)", file=sys.stderr)
        return False
    print("selftest: controle UNTRACKED OK (arquivo nunca staged foi cobrado e marcado 'nao rastreado')")
    return True


def selftest_main():
    scratch = make_scratch_workdir()
    try:
        controls = [
            selftest_positive_control(scratch),
            selftest_negative_control(scratch),
            selftest_exempt_exact_boundary_control(scratch),
            selftest_empty_scan_control(scratch),
            selftest_not_a_repo_control(scratch),
            selftest_untracked_control(scratch),
        ]
    finally:
        remove_tree_tolerant(scratch, ignore_errors=True)
    if not all(controls):
        print(f"{SCRIPT_NAME} --selftest: FALHOU (ver acima)", file=sys.stderr)
        sys.exit(1)
    print(f"{SCRIPT_NAME} --selftest: os {len(controls)} controles OK")


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
    else:
        real_main(args)


if __name__ == "__main__":
    main()
