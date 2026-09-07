#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_precommit_hook_chain.py - GODS_LAWS.md L-36/L-40 gate.
#
# INBOX (drenagem de 06/09/2026, "nenhum portao de arquivo esta armado
# neste clone: o gancho local nunca foi instalado aqui"): the finding's
# root cause was two-layered. (1) tools/git-hooks/install.sh had never
# been run in that clone, so .git/hooks/pre-commit did not exist at all
# - a per-CLONE fact this repository's own tree cannot see or fix (no
# gate here reaches outside the working tree into .git/hooks/, which is
# never versioned - see install.sh's own header). (2) EVEN when
# installed, tools/git-hooks/pre-commit only ever chained tests/tools/
# check_dep_zero.py - the license gate (tests/tools/check_spdx.py,
# SPDX-GATE, GODS_LAWS.md L-08) ran in CI only, so a file born without
# its SPDX header would not be caught until the server did. This gate
# closes (2) as a permanent regression check: it reads the VERSIONED
# source of the hook and reproves the moment either gate's own filename
# stops appearing in it - a structural check, not an execution of the
# hook itself (running the real gates end-to-end against a scratch git
# clone was done once, by hand, as this fatia's own red/green proof -
# see TODO.md's own INBOX entry for the literal output; a ctest that
# clones and commits into a throwaway repo on every run would be the
# "trabalho pesado" GODS_LAWS.md L-11 reserves one-at-a-time, not a
# cheap always-on gate).
#
# Usage:
#   check_precommit_hook_chain.py <repo-root>
#   check_precommit_hook_chain.py --selftest

import sys
import tempfile
from pathlib import Path

SCRIPT_NAME = "check_precommit_hook_chain.py"

# The two gates the commit-time hook must chain into, by the exact
# filename each one's own real_main()/argparse expects on the command
# line - a rename of either script is a real event this gate SHOULD
# reprove, not silently ignore (the same "check a fixed token" house
# convention check_readme_volatile_numbers.py's own header cites).
REQUIRED_GATES = ("check_dep_zero.py", "check_spdx.py")


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


def missing_gates(hook_text):
    """Returns the subset of REQUIRED_GATES whose filename does not
    appear anywhere in `hook_text` - order preserved, so the message
    below always names them in the same order regardless of how many
    are missing."""
    return [gate for gate in REQUIRED_GATES if gate not in hook_text]


def check_precommit_hook_chain(repo_root):
    hook_path = Path(repo_root) / "tools" / "git-hooks" / "pre-commit"
    try:
        hook_text = hook_path.read_text(encoding="utf-8")
    except OSError as exc:
        print(f"{SCRIPT_NAME}: nao foi possivel ler {hook_path} ({exc})", file=sys.stderr)
        return False

    absent = missing_gates(hook_text)
    if absent:
        print(
            f"{SCRIPT_NAME}: {hook_path} nao encadeia {len(absent)} de "
            f"{len(REQUIRED_GATES)} portao(oes) exigido(s): {', '.join(absent)}",
            file=sys.stderr,
        )
        return False

    print(
        f"{SCRIPT_NAME}: {hook_path} encadeia os {len(REQUIRED_GATES)} portao(oes) exigido(s) "
        f"({', '.join(REQUIRED_GATES)})"
    )
    return True


# --- selftest ------------------------------------------------------------


def _write_hook(tmp_dir, body):
    hooks_dir = Path(tmp_dir) / "tools" / "git-hooks"
    hooks_dir.mkdir(parents=True, exist_ok=True)
    (hooks_dir / "pre-commit").write_text(body, encoding="utf-8")


def selftest_both_present_passes(tmp_dir):
    case_dir = Path(tmp_dir) / "both_present"
    _write_hook(
        case_dir,
        '"$python_bin" check_dep_zero.py --staged "$repo_root"\n'
        'exec "$python_bin" check_spdx.py "$repo_root"\n',
    )
    if check_precommit_hook_chain(case_dir):
        print("selftest: controle BOTH-PRESENT OK (os dois portoes encadeados passam)")
        return True
    print("selftest: controle BOTH-PRESENT FALHOU (deveria passar)", file=sys.stderr)
    return False


def selftest_spdx_missing_fails(tmp_dir):
    case_dir = Path(tmp_dir) / "spdx_missing"
    _write_hook(case_dir, 'exec "$python_bin" check_dep_zero.py --staged "$repo_root"\n')
    if not check_precommit_hook_chain(case_dir):
        print("selftest: controle SPDX-MISSING OK (reprovou faltando check_spdx.py)")
        return True
    print("selftest: controle SPDX-MISSING FALHOU (deveria reprovar)", file=sys.stderr)
    return False


def selftest_dep_zero_missing_fails(tmp_dir):
    case_dir = Path(tmp_dir) / "dep_zero_missing"
    _write_hook(case_dir, 'exec "$python_bin" check_spdx.py "$repo_root"\n')
    if not check_precommit_hook_chain(case_dir):
        print("selftest: controle DEP-ZERO-MISSING OK (reprovou faltando check_dep_zero.py)")
        return True
    print("selftest: controle DEP-ZERO-MISSING FALHOU (deveria reprovar)", file=sys.stderr)
    return False


def selftest_both_missing_fails(tmp_dir):
    case_dir = Path(tmp_dir) / "both_missing"
    _write_hook(case_dir, "echo nada aqui\n")
    if not check_precommit_hook_chain(case_dir):
        print("selftest: controle BOTH-MISSING OK (reprovou nomeando os dois)")
        return True
    print("selftest: controle BOTH-MISSING FALHOU (deveria reprovar)", file=sys.stderr)
    return False


def selftest_missing_hook_file_fails(tmp_dir):
    case_dir = Path(tmp_dir) / "no_hook_at_all"
    Path(case_dir).mkdir(parents=True, exist_ok=True)
    if not check_precommit_hook_chain(case_dir):
        print("selftest: controle NO-HOOK-FILE OK (reprovou, arquivo nao existe)")
        return True
    print("selftest: controle NO-HOOK-FILE FALHOU (deveria reprovar)", file=sys.stderr)
    return False


def selftest_main():
    with tempfile.TemporaryDirectory(prefix="glintfx-precommit-hook-chain-selftest-") as tmp_dir:
        controls = [
            selftest_both_present_passes(tmp_dir),
            selftest_spdx_missing_fails(tmp_dir),
            selftest_dep_zero_missing_fails(tmp_dir),
            selftest_both_missing_fails(tmp_dir),
            selftest_missing_hook_file_fails(tmp_dir),
        ]
    if not all(controls):
        print(f"{SCRIPT_NAME} --selftest: FALHOU (ver acima)", file=sys.stderr)
        sys.exit(1)
    print(f"{SCRIPT_NAME} --selftest: os {len(controls)} controles OK")


def real_main(args):
    if len(args) != 1:
        fail("usage: check_precommit_hook_chain.py <repo-root>")
    root = args[0]
    if not Path(root).is_dir():
        fail(f"directory not found: {root}")
    if not check_precommit_hook_chain(root):
        fail("tools/git-hooks/pre-commit nao encadeia todos os portoes exigidos (ver mensagem acima)")


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
    else:
        real_main(args)


if __name__ == "__main__":
    main()
