#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_selftest_check_effect.py - gate of SELFTEST-CHECK-CANARY, step CANARY-3 (D-W8-360 to D-W8-362;
# GODS_LAWS.md L-17, L-36, L-40).
#
# THE RULE IS THE EFFECT, NOT THE FORM OF THE TEXT. Every .py of the repository whose text has the word `check` is
# a candidate. Each candidate is imported in this process (importlib, output captured). If it then exposes a
# callable `check`, that check is called with a false condition, and it must stop with SystemExit whose code is
# neither 0 nor None. A candidate that does not import, a check that does not stop, and an empty enumeration are
# defects. The form of the binding (def, lambda, walrus, annotation, tuple, shadow) does not matter: the effect is
# the same for all of them.
#
# Limits by construction (D-W8-362, not debated): a name `check` built at call time (exec, globals(), setattr)
# with no such word in the text; a control helper with another name; a selftest that does not use check;
# vandalism of the line that calls this gate.
#
# Enumeration: `git ls-files` (tracked) plus `git ls-files --others --exclude-standard` (untracked, not ignored),
# over the repository root given as argument, never over the current directory. Zero candidates is a failure
# (L-40). A candidate that does not import is a named failure, never skipped.
#
# Usage:
#   check_selftest_check_effect.py --check <repository-root>
#   check_selftest_check_effect.py --selftest

import contextlib
import importlib.util
import io
import os
import re
import shutil
import subprocess
import sys
import tempfile
from collections import namedtuple

SCRIPT_NAME = "check_selftest_check_effect.py"
CANDIDATE_PATTERN = re.compile(rb"\bcheck\b")
MODULE_PREFIX = "_check_effect_"

Report = namedtuple("Report", "candidatos importados com_check mudos problems")


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


# --- enumeration: the candidates are the .py files whose text has the word `check` ---


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


def is_candidate(root, rel):
    with open(os.path.join(root, rel), "rb") as handle:
        return CANDIDATE_PATTERN.search(handle.read()) is not None


def find_candidates(root, rels, problems):
    candidates = []
    for rel in rels:
        try:
            if is_candidate(root, rel):
                candidates.append(rel)
        except OSError as problem:
            problems.append(f"{rel}: nao lido ({problem})")
    return candidates


# --- import and probe: the EFFECT of each candidate's check ---


def module_name(rel):
    return MODULE_PREFIX + re.sub(r"\W", "_", rel)


def import_quietly(root, rel):
    """Returns (module, None), or (None, named problem). The output of the module is captured, never printed."""
    path = os.path.join(root, rel)
    folder = os.path.dirname(path)
    name = module_name(rel)
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    sys.path.insert(0, folder)
    try:
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            spec.loader.exec_module(module)
    except (Exception, SystemExit) as problem:
        sys.modules.pop(name, None)
        return None, f"{rel}: nao importou ({type(problem).__name__}: {problem}), o efeito do check nao pode ser provado"
    finally:
        sys.path.remove(folder)
    return module, None


def probe_check(module, rel):
    """Returns None when check() stops with a non-zero SystemExit under a false condition, else the named problem."""
    check = getattr(module, "check")
    try:
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            check("efeito", False)
    except SystemExit as stopped:
        if stopped.code in (0, None):
            return f"{rel}: check() para com codigo {stopped.code!r} sob condicao falsa (tem de ser diferente de 0 e de None)"
        return None
    except Exception as problem:
        return f"{rel}: check() levanta {type(problem).__name__} em vez de parar com SystemExit (D-W8-362)"
    return f"{rel}: check() volta sem parar sob condicao falsa, autoteste mudo (D-W8-362)"


def analyse_candidates(root, candidates, problems):
    """Returns (importados, com_check, mudos). Every candidate is imported; every check exposed is probed."""
    importados = 0
    com_check = 0
    mudos = 0
    for rel in candidates:
        module, problem = import_quietly(root, rel)
        if problem is not None:
            problems.append(problem)
            continue
        importados += 1
        if callable(getattr(module, "check", None)):
            com_check += 1
            outcome = probe_check(module, rel)
            if outcome is not None:
                mudos += 1
                problems.append(outcome)
        sys.modules.pop(module.__name__, None)
    return importados, com_check, mudos


def analyse_repository(root):
    """Returns a Report, or None when `root` is not a git repository."""
    rels = list_python_files(root)
    if rels is None:
        return None
    problems = []
    candidates = find_candidates(root, rels, problems)
    importados, com_check, mudos = analyse_candidates(root, candidates, problems)
    if not candidates:
        problems.append(f"varredura vazia: 0 candidatos com a palavra check em {root} (L-40)")
    elif com_check == 0:
        problems.append("nenhum candidato expoe check() chamavel: o portao nao provaria nada (L-40)")
    if importados != len(candidates):
        problems.append(f"{len(candidates) - importados} candidato(s) nao importaram (importados != candidatos)")
    return Report(len(candidates), importados, com_check, mudos, problems)


def summary_line(report):
    return (f"check_selftest_check_effect: candidatos={report.candidatos} importados={report.importados} "
            f"com_check={report.com_check} mudos={report.mudos}")


def check_main(root):
    report = analyse_repository(root)
    if report is None:
        fail(f"nao e um repositorio git: {root}")
    print(summary_line(report))
    if report.problems:
        for problem in report.problems:
            print(f"{SCRIPT_NAME}: FALHOU {problem}", file=sys.stderr)
        sys.exit(1)
    print("check_selftest_check_effect: todo check exposto para com a condicao falsa (D-W8-362)")


# --- verdict helpers over a Report ---


def verdict_passes(report):
    return report is not None and not report.problems


def verdict_names(report, needle):
    return report is not None and any(needle in problem for problem in report.problems)


# --- --selftest: eight controls over temporary trees built here ---


def build_tree(files):
    root = tempfile.mkdtemp(prefix="check_effect_")
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


GOOD_CHECK = "import sys\n\n\ndef check(name, condition):\n    if not condition:\n        sys.exit(1)\n"

# Each control: (label with the expected outcome, the tree it builds, the expectation over its Report).
# The expectation is written BEFORE the code that makes it true (L-35, L-43).
CONTROLS = (
    ("a: um check certo passa, com_check=1",
     {"tests/tools/ctl_a.py": GOOD_CHECK},
     lambda r: verdict_passes(r) and r.com_check == 1),
    ("b: def check(n, c): pass reprova, nomeando o modulo",
     {"tests/tools/ctl_b.py": "def check(name, condition):\n    pass\n"},
     lambda r: verdict_names(r, "tests/tools/ctl_b.py")),
    ("c: (check := lambda ...) no topo reprova, nomeando o modulo",
     {"tests/tools/ctl_c.py": "(check := lambda name, condition: None)\n"},
     lambda r: verdict_names(r, "tests/tools/ctl_c.py")),
    ("d: check, _u = (lambda ...), 0 reprova, nomeando o modulo",
     {"tests/tools/ctl_d.py": "check, _unused = (lambda name, condition: None), 0\n"},
     lambda r: verdict_names(r, "tests/tools/ctl_d.py")),
    ("e: check com sys.exit(0) reprova, nomeando o modulo",
     {"tests/tools/ctl_e.py": "import sys\n\n\ndef check(name, condition):\n    sys.exit(0)\n"},
     lambda r: verdict_names(r, "tests/tools/ctl_e.py")),
    ("f: diretorio sem candidato reprova (varredura vazia)",
     {},
     lambda r: verdict_names(r, "varredura vazia")),
    ("g: candidato cujo import levanta excecao reprova, nomeando o modulo, nunca pula",
     {"tests/tools/ctl_g.py": "# check\nraise RuntimeError('boom')\n"},
     lambda r: verdict_names(r, "tests/tools/ctl_g.py") and verdict_names(r, "nao importou")),
    ("h: o (a) mais um modulo sem a palavra passa, candidatos=1",
     {"tests/tools/ctl_h.py": GOOD_CHECK, "tests/tools/ctl_h_plain.py": "VALUE = 1\n"},
     lambda r: verdict_passes(r) and r.candidatos == 1),
)


def control_verdicts():
    """Each control: (label, verdict). The verdict is computed by a real run of the gate over its tree."""
    return [(label, expects(analyse_tree(files))) for label, files, expects in CONTROLS]


def selftest_main():
    from selftest_controls import check
    controls = control_verdicts()
    for name, verdict in controls:
        check(name, verdict)
    print(f"selftest: {len(controls)} controles OK")


def main():
    sys.dont_write_bytecode = True
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
    elif len(args) == 2 and args[0] == "--check":
        check_main(args[1])
    else:
        fail("uso: check_selftest_check_effect.py --check <raiz-do-repositorio> | --selftest")


if __name__ == "__main__":
    main()
