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
# source of the hook and reproves the moment any gate's own filename
# stops appearing in it - a structural check, not an execution of the
# hook itself (running the real gates end-to-end against a scratch git
# clone was done once, by hand, as this fatia's own red/green proof -
# see TODO.md's own INBOX entry for the literal output; a ctest that
# clones and commits into a throwaway repo on every run would be the
# "trabalho pesado" GODS_LAWS.md L-11 reserves one-at-a-time, not a
# cheap always-on gate).
#
# DASH-PUBDOC (docs/decisoes-inbox-tres.md SS3): a third gate,
# tests/tools/check_dash_pubdoc.py, joined the chain for the identical
# reason SPDX-GATE did - the long-dash rule for public documentation
# used to live ONLY outside this repository (a global hook that never
# runs on the server and never travels with a clone). REQUIRED_GATES
# grew to three; every control below that used to say "the two gates"
# now says "the three gates".
#
# Usage:
#   check_precommit_hook_chain.py <repo-root>
#   check_precommit_hook_chain.py --selftest

import sys
import tempfile
from pathlib import Path

SCRIPT_NAME = "check_precommit_hook_chain.py"

# The three gates the commit-time hook must chain into, by the exact
# filename each one's own real_main()/argparse expects on the command
# line - a rename of any script is a real event this gate SHOULD
# reprove, not silently ignore (the same "check a fixed token" house
# convention check_readme_volatile_numbers.py's own header cites).
REQUIRED_GATES = ("check_dep_zero.py", "check_spdx.py", "check_dash_pubdoc.py")


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


def strip_shell_comments(text):
    """Truncates every line at its first '#' (POSIX sh's own comment
    marker), so missing_gates() below only ever sees CODE, never the
    hook's own header comment describing it. Achado do mesmo dia
    (drenagem 06/09/2026, "nenhum portao de arquivo esta armado neste
    clone"): esse cabecalho ja cita os dois nomes de arquivo em prosa
    ("tests/tools/check_spdx.py (SPDX-GATE...)", "tests/tools/
    check_dep_zero.py"), entao remover a linha de chamada REAL de um
    dos dois gates nunca fazia o nome dele sumir de `hook_text` - o
    proprio comentario que explica o gate ja o mantinha presente.
    Nenhuma linha deste hook usa '#' dentro de um argumento entre
    aspas (as duas chamadas reais sao `"$python_bin" "$repo_root/.../
    check_*.py" ...`, sem '#'), entao truncar no primeiro '#' nunca
    come sintaxe real aqui."""
    return "\n".join(line[: line.find("#")] if "#" in line else line for line in text.splitlines())


def missing_gates(hook_text):
    """Returns the subset of REQUIRED_GATES whose filename does not
    appear in `hook_text` OUTSIDE a comment (see strip_shell_comments())
    - order preserved, so the message below always names them in the
    same order regardless of how many are missing."""
    code_text = strip_shell_comments(hook_text)
    return [gate for gate in REQUIRED_GATES if gate not in code_text]


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


def selftest_all_present_passes(tmp_dir):
    case_dir = Path(tmp_dir) / "all_present"
    _write_hook(
        case_dir,
        '"$python_bin" check_dep_zero.py --staged "$repo_root"\n'
        '"$python_bin" check_spdx.py "$repo_root"\n'
        'exec "$python_bin" check_dash_pubdoc.py "$repo_root"\n',
    )
    if check_precommit_hook_chain(case_dir):
        print("selftest: controle ALL-PRESENT OK (os tres portoes encadeados passam)")
        return True
    print("selftest: controle ALL-PRESENT FALHOU (deveria passar)", file=sys.stderr)
    return False


def selftest_spdx_missing_fails(tmp_dir):
    case_dir = Path(tmp_dir) / "spdx_missing"
    _write_hook(
        case_dir,
        '"$python_bin" check_dep_zero.py --staged "$repo_root"\n'
        'exec "$python_bin" check_dash_pubdoc.py "$repo_root"\n',
    )
    if not check_precommit_hook_chain(case_dir):
        print("selftest: controle SPDX-MISSING OK (reprovou faltando check_spdx.py)")
        return True
    print("selftest: controle SPDX-MISSING FALHOU (deveria reprovar)", file=sys.stderr)
    return False


def selftest_dep_zero_missing_fails(tmp_dir):
    case_dir = Path(tmp_dir) / "dep_zero_missing"
    _write_hook(
        case_dir,
        '"$python_bin" check_spdx.py "$repo_root"\n'
        'exec "$python_bin" check_dash_pubdoc.py "$repo_root"\n',
    )
    if not check_precommit_hook_chain(case_dir):
        print("selftest: controle DEP-ZERO-MISSING OK (reprovou faltando check_dep_zero.py)")
        return True
    print("selftest: controle DEP-ZERO-MISSING FALHOU (deveria reprovar)", file=sys.stderr)
    return False


def selftest_dash_pubdoc_missing_fails(tmp_dir):
    case_dir = Path(tmp_dir) / "dash_pubdoc_missing"
    _write_hook(
        case_dir,
        '"$python_bin" check_dep_zero.py --staged "$repo_root"\n'
        'exec "$python_bin" check_spdx.py "$repo_root"\n',
    )
    if not check_precommit_hook_chain(case_dir):
        print("selftest: controle DASH-PUBDOC-MISSING OK (reprovou faltando check_dash_pubdoc.py)")
        return True
    print("selftest: controle DASH-PUBDOC-MISSING FALHOU (deveria reprovar)", file=sys.stderr)
    return False


def selftest_all_missing_fails(tmp_dir):
    case_dir = Path(tmp_dir) / "all_missing"
    _write_hook(case_dir, "echo nada aqui\n")
    if not check_precommit_hook_chain(case_dir):
        print("selftest: controle ALL-MISSING OK (reprovou nomeando os tres)")
        return True
    print("selftest: controle ALL-MISSING FALHOU (deveria reprovar)", file=sys.stderr)
    return False


def selftest_missing_hook_file_fails(tmp_dir):
    case_dir = Path(tmp_dir) / "no_hook_at_all"
    Path(case_dir).mkdir(parents=True, exist_ok=True)
    if not check_precommit_hook_chain(case_dir):
        print("selftest: controle NO-HOOK-FILE OK (reprovou, arquivo nao existe)")
        return True
    print("selftest: controle NO-HOOK-FILE FALHOU (deveria reprovar)", file=sys.stderr)
    return False


def selftest_real_hook_passes(repo_root):
    ok = check_precommit_hook_chain(repo_root)
    if not ok:
        print("selftest: controle REAL-HOOK FALHOU (o gancho real deveria passar hoje)", file=sys.stderr)
        return False
    print(
        f"selftest: controle REAL-HOOK OK (o gancho real deste repositorio encadeia os "
        f"{len(REQUIRED_GATES)} portoes)"
    )
    return True


def selftest_real_hook_sabotaged_reproves(repo_root, tmp_dir):
    """GODS_LAWS.md L-36 (portao so conta depois de PROVADO vermelho
    CONTRA O QUE ELE DEVE BARRAR): as cinco fixtures sinteticas acima
    nunca tem o cabecalho de prosa que o gancho REAL carrega - nenhuma
    delas teria pego a regressao real (INBOX 06/09/2026: a chamada real
    de check_spdx.py removida do gancho, e o portao continuou dizendo
    'encadeia os 2 portoes exigidos', codigo zero, porque o proprio
    cabecalho do gancho ja cita 'check_spdx.py' em prosa - ver
    strip_shell_comments()). Este controle reproduz a MESMA sabotagem
    sobre uma COPIA do gancho real (nunca in-place - GODS_LAWS.md L-27)
    e exige reprovacao.

    ACHADO DE JUNCAO (nao de commit antigo - corrigido apos atribuicao
    errada): o `exec` do gancho so pode estar na ULTIMA chamada (senao
    as seguintes nunca rodam) - quando o portao do travessao entrou
    (commit 35f30d8, ramo docs/dash-pubdoc, NESTA juncao), o `exec`
    migrou de check_spdx.py para check_dash_pubdoc.py, e o needle fixo
    abaixo (que so reconhecia a forma COM `exec`) parou de bater. A
    busca agora aceita a chamada COM ou SEM o prefixo `exec `, para nao
    quebrar de novo quando um quarto portao entrar e o `exec` migrar de
    novo."""
    sabotaged = Path(tmp_dir) / "real_hook_sabotaged"
    hooks_dir = sabotaged / "tools" / "git-hooks"
    hooks_dir.mkdir(parents=True, exist_ok=True)
    real_hook_path = Path(repo_root) / "tools" / "git-hooks" / "pre-commit"
    original = real_hook_path.read_text(encoding="utf-8")
    call = '"$python_bin" "$repo_root/tests/tools/check_spdx.py" "$repo_root"\n'
    needle = None
    for candidate in ("exec " + call, call):
        if candidate in original:
            needle = candidate
            break
    if needle is None:
        print(
            "selftest: controle REAL-HOOK-SABOTAGED FALHOU (linha de chamada real nao encontrada - "
            "selftest esta desatualizado)",
            file=sys.stderr,
        )
        return False
    (hooks_dir / "pre-commit").write_text(original.replace(needle, "", 1), encoding="utf-8")

    ok = check_precommit_hook_chain(sabotaged)
    if ok:
        print(
            "selftest: controle REAL-HOOK-SABOTAGED FALHOU (removeu a chamada real de check_spdx.py "
            "e o portao continuou passando)",
            file=sys.stderr,
        )
        return False
    print(
        "selftest: controle REAL-HOOK-SABOTAGED OK (o gancho real, com a chamada real removida, reprova)"
    )
    return True


def selftest_main():
    repo_root = Path(__file__).resolve().parents[2]
    with tempfile.TemporaryDirectory(prefix="glintfx-precommit-hook-chain-selftest-") as tmp_dir:
        controls = [
            selftest_all_present_passes(tmp_dir),
            selftest_spdx_missing_fails(tmp_dir),
            selftest_dep_zero_missing_fails(tmp_dir),
            selftest_dash_pubdoc_missing_fails(tmp_dir),
            selftest_all_missing_fails(tmp_dir),
            selftest_missing_hook_file_fails(tmp_dir),
            selftest_real_hook_passes(repo_root),
            selftest_real_hook_sabotaged_reproves(repo_root, tmp_dir),
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
