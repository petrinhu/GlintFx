#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_selftest_helper.py - gate of SELFTEST-CHECK-CANARY, step CANARY-2 (D-W8-357; GODS_LAWS.md L-17,
# L-36, L-40).
#
# THE DEFECT THIS CLOSES: tests/tools/selftest_controls.py holds the one check() shared by the selftests,
# and the canary lives inside it. A tool that writes its own `def check(...)` skips the canary without any
# sign (its check never stops), and a shared check() whose body lost the call to _canary() disarms it.
# This gate fails when there is not EXACTLY one check() in tests/tools/selftest_controls.py, when any other
# .py of the repository defines check(), or when that check() does not call _canary() (and _canary() is
# not defined at module level there).
#
# Enumeration: `git ls-files` (tracked) plus `git ls-files --others --exclude-standard` (untracked, not
# ignored), the convention of check_spdx.py. The walk is over the repository root given as argument, never
# over the current directory. Zero files analysed is a failure (L-40); a .py that does not parse is a named
# failure, never skipped.
#
# Declared limit: a check bound by `import ... as check` or by a name in another module is not counted; the
# gate counts `def check` and `check = ...` bindings only.
#
# Usage:
#   check_selftest_helper.py --check <repository-root>
#   check_selftest_helper.py --selftest

import ast
import os
import shutil
import subprocess
import sys
import tempfile
from collections import namedtuple

SCRIPT_NAME = "check_selftest_helper.py"
OWNER_REL = "tests/tools/selftest_controls.py"
CHECK_NAME = "check"
CANARY_NAME = "_canary"
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SELFTEST_OK_LINE = "selftest: 5 controles OK"

Report = namedtuple("Report", "analisados donos fora arme problems")


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


# --- enumeration of the Python files of a repository ---


def git_ls_files(root, extra_args):
    result = subprocess.run(["git", "-C", root, "ls-files", "-z", *extra_args],
                            capture_output=True, check=False)
    if result.returncode != 0:
        return None
    return [item.decode("utf-8", "surrogateescape") for item in result.stdout.split(b"\0") if item]


def list_python_files(root):
    tracked = git_ls_files(root, [])
    untracked = git_ls_files(root, ["--others", "--exclude-standard"])
    if tracked is None or untracked is None:
        return None
    return sorted({rel for rel in tracked + untracked if rel.endswith(".py")})


# --- parsing and AST facts ---


def read_tree(root, rel):
    """Returns (tree, None) or (None, named problem). Never skips a file silently (L-40)."""
    try:
        with open(os.path.join(root, rel), "rb") as handle:
            return ast.parse(handle.read(), filename=rel), None
    except (SyntaxError, ValueError, OSError) as problem:
        return None, f"{rel}: nao analisado ({problem.__class__.__name__}: {problem})"


def binds_check(node):
    if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
        return node.name == CHECK_NAME
    if isinstance(node, ast.Assign):
        return any(isinstance(target, ast.Name) and target.id == CHECK_NAME for target in node.targets)
    return False


def check_bindings(tree):
    return [node for node in ast.walk(tree) if binds_check(node)]


def count_canary_calls(node):
    return sum(1 for item in ast.walk(node)
               if isinstance(item, ast.Call) and isinstance(item.func, ast.Name)
               and item.func.id == CANARY_NAME)


def has_canary_definition(tree):
    return any(isinstance(node, ast.FunctionDef) and node.name == CANARY_NAME for node in tree.body)


def owner_problems(tree):
    """Problems of the owner module: one module-level check() that calls _canary(), and _canary() there."""
    bindings = check_bindings(tree)
    if len(bindings) != 1:
        return [f"{OWNER_REL}: {len(bindings)} definicoes de check, tem de ser exatamente 1 (D-W8-357)"]
    problems = []
    if bindings[0] not in tree.body:
        problems.append(f"{OWNER_REL}: check() nao e de nivel de modulo")
    if not has_canary_definition(tree):
        problems.append(f"{OWNER_REL}: falta a funcao _canary (canario, D-W8-352)")
    if count_canary_calls(bindings[0]) == 0:
        problems.append(f"{OWNER_REL}: o check nao chama _canary() (canario desarmado, D-W8-352)")
    return problems


# --- the verdict over a repository ---


def scan_outside_owner(rel, tree):
    """Problems for each check binding of a module other than the owner (M2: a local check)."""
    return [f"{rel}: define check() fora de {OWNER_REL} (D-W8-357), linha {node.lineno}"
            for node in check_bindings(tree)]


def scan_files(root, rels):
    """Returns (problems, owner_tree, foreign_count) over the files `rels`."""
    problems = []
    owner_tree = None
    foreign = 0
    for rel in rels:
        tree, error = read_tree(root, rel)
        if error is not None:
            problems.append(error)
        elif rel == OWNER_REL:
            owner_tree = tree
        else:
            found = scan_outside_owner(rel, tree)
            foreign += len(found)
            problems.extend(found)
    return problems, owner_tree, foreign


def analyse_repository(root):
    """Returns a Report, or None when `root` is not a git repository."""
    rels = list_python_files(root)
    if rels is None:
        return None
    problems, owner_tree, foreign = scan_files(root, rels)
    if not rels:
        problems.append(f"varredura vazia: 0 arquivos .py analisados em {root} (L-40)")
    if owner_tree is None:
        problems.append(f"{OWNER_REL} nao existe entre os .py analisados (D-W8-357)")
        return Report(len(rels), 0, foreign, 0, problems)
    problems.extend(owner_problems(owner_tree))
    bindings = check_bindings(owner_tree)
    arme = count_canary_calls(bindings[0]) if len(bindings) == 1 else 0
    return Report(len(rels), len(bindings), foreign, arme, problems)


def summary_line(report):
    return (f"check_selftest_helper: analisados={report.analisados} donos={report.donos} "
            f"fora={report.fora} arme={report.arme}")


def check_main(root):
    report = analyse_repository(root)
    if report is None:
        fail(f"nao e um repositorio git: {root}")
    print(summary_line(report))
    if report.problems:
        for problem in report.problems:
            print(f"{SCRIPT_NAME}: FALHOU {problem}", file=sys.stderr)
        sys.exit(1)
    print("check_selftest_helper: um dono, o check comum, e ele chama o canario (D-W8-357)")


# --- --selftest: five controls over temporary trees built here ---


def owner_text():
    with open(os.path.join(SCRIPT_DIR, "selftest_controls.py"), encoding="utf-8") as handle:
        return handle.read()


def owner_with_replacement(old, new):
    text = owner_text()
    if text.count(old) != 1:
        fail(f"autoteste: trecho do dono ocorre {text.count(old)} vez(es), esperado 1")
    return text.replace(old, new)


def build_tree(files):
    root = tempfile.mkdtemp(prefix="selftest_helper_")
    for rel, text in files.items():
        path = os.path.join(root, rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8") as handle:
            handle.write(text)
    subprocess.run(["git", "-C", root, "init", "-q"], check=True)
    return root


def analyse_tree(files):
    root = build_tree(files)
    try:
        return analyse_repository(root)
    finally:
        shutil.rmtree(root, ignore_errors=True)


def owner_extra_check():
    return owner_text() + "\n\ndef check(name, condition):\n    pass\n"


COMMON_USER = "from selftest_controls import check\n\ncheck('controle', True)\n"
LOCAL_CHECK_MODULE = "CHECKS = []\n\n\ndef check(name, condition):\n    CHECKS.append(name)\n"


def verdict_passes(report):
    return report is not None and not report.problems


def verdict_names(report, needle):
    return report is not None and any(needle in problem for problem in report.problems)


def control_verdicts():
    """Each control: (name, verdict). The expected outcome of each is written before its run (L-43)."""
    owner = owner_text()
    reports = {
        "a": analyse_tree({OWNER_REL: owner, "tests/tools/probe_common.py": COMMON_USER}),
        "b": analyse_tree({OWNER_REL: owner, "tests/tools/probe_m2.py": LOCAL_CHECK_MODULE}),
        "c": analyse_tree({OWNER_REL: owner_with_replacement("        _canary()\n", "        pass\n")}),
        "d": analyse_tree({}),
        "e": analyse_tree({OWNER_REL: owner_extra_check()}),
    }
    return [
        ("a: so o comum, com o canario, passa", verdict_passes(reports["a"])),
        ("b: um check local reprova, nomeando o arquivo", verdict_names(reports["b"], "tests/tools/probe_m2.py")),
        ("c: o comum sem _canary() reprova", verdict_names(reports["c"], "_canary()")),
        ("d: nenhum .py reprova (varredura vazia)", verdict_names(reports["d"], "varredura vazia")),
        ("e: dois check no comum reprovam", verdict_names(reports["e"], "2 definicoes de check")),
    ]


def selftest_main():
    from selftest_controls import check
    controls = control_verdicts()
    for name, verdict in controls:
        check(name, verdict)
    print(f"selftest: {len(controls)} controles OK")


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
    elif len(args) == 2 and args[0] == "--check":
        check_main(args[1])
    else:
        fail("uso: check_selftest_helper.py --check <raiz-do-repositorio> | --selftest")


if __name__ == "__main__":
    main()
