#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# ctest_aggregate.py - CI-SPLIT-PER-OS A2 (achado 4 da revisao do CTO,
# 29/09/2026): o resultado agregado declarados/executados/passaram/
# falharam de um job de ctest, MEDIDO da mesma forma nos dois sistemas
# (L-04). Antes, o passo escrito em bash (linux) e em pwsh (windows)
# fazia `executados=$declarados`, entao a checagem `executados !=
# declarados` era uma TAUTOLOGIA que nunca disparava - e um teste
# pulado passava calado.
#
#   declarados : rodape "Total Tests: N" do `ctest -N` (o proprio ctest
#                dizendo quantos ELE registrou - L-45, nunca contar linha);
#   executados : casos que o ctest REALMENTE rodou, contados no snapshot
#                do LastTest.log (`LastTest.log.pnc-fases-snapshot`, tirado
#                pelo passo "Testes" antes de o `ctest -N` reescrever o
#                log): uma linha "K/N Testing: <nome>" por caso executado;
#   falharam   : linhas "K:<nome>" de LastTestsFailed.log (ausente = 0);
#   passaram   : executados - falharam.
#
# Reprova se declarados == 0 (varredura vazia, L-40), se snapshot ausente
# ou ilegivel (os testes nao rodaram), se executados != declarados (teste
# pulado) ou se falharam > 0. Sempre imprime as quatro contagens.
#
# Usage:
#   ctest_aggregate.py --builddir <dir> --inventory <parity_inventory.txt>
#   ctest_aggregate.py --selftest

import os
import re
import sys

SCRIPT_NAME = "ctest_aggregate.py"
SNAPSHOT = os.path.join("Testing", "Temporary", "LastTest.log.pnc-fases-snapshot")
FAILED_LOG = os.path.join("Testing", "Temporary", "LastTestsFailed.log")

_TOTAL_RE = re.compile(r"Total Tests: (\d+)")
_EXECUTED_RE = re.compile(r"^\d+/\d+ Testing: ", re.MULTILINE)
_FAILED_RE = re.compile(r"^\d+:", re.MULTILINE)


def read_text(path):
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as handle:
            return handle.read()
    except OSError:
        return None


def measure(builddir, inventory_path):
    """(declarados, executados, falharam, erros) - erros e' a lista de
    causas que impedem a medicao (arquivo ausente etc.)."""
    errors = []
    inventory = read_text(inventory_path)
    declared = None
    if inventory is None or not _TOTAL_RE.search(inventory):
        errors.append(
            f"rodape 'Total Tests: N' nao encontrado em {inventory_path} - ctest -N nao terminou "
            f"de escrever, ou o formato mudou (GODS_LAWS.md L-40)"
        )
    else:
        declared = int(_TOTAL_RE.search(inventory).group(1))
    snapshot = read_text(os.path.join(builddir, SNAPSHOT))
    executed = 0
    if snapshot is None:
        errors.append(f"{os.path.join(builddir, SNAPSHOT)} ausente - o passo 'Testes' nao rodou ate o fim")
    else:
        executed = len(_EXECUTED_RE.findall(snapshot))
    failed_log = read_text(os.path.join(builddir, FAILED_LOG))
    failed = len(_FAILED_RE.findall(failed_log)) if failed_log is not None else 0
    return declared, executed, failed, errors


def verdict(declared, executed, failed, errors):
    """Lista de causas de reprovacao (vazia = aprovado)."""
    problems = list(errors)
    if declared == 0:
        problems.append("piso de varredura vazia (GODS_LAWS.md L-40): 'ctest -N' nao registrou teste nenhum")
    if declared is not None and executed != declared:
        problems.append(
            f"executados ({executed}) != declarados ({declared}) - teste declarado que nao "
            f"foi executado (pulado) ou nao foi contado no resultado"
        )
    if failed > 0:
        problems.append(f"{failed} teste(s) falharam")
    return problems


def run(builddir, inventory_path):
    declared, executed, failed, errors = measure(builddir, inventory_path)
    passed = executed - failed
    shown = "?" if declared is None else declared
    print(f"declarados: {shown}, executados: {executed}, passaram: {passed}, falharam: {failed}")
    problems = verdict(declared, executed, failed, errors)
    for problem in problems:
        print(f"{SCRIPT_NAME}: {problem}", file=sys.stderr)
    return 1 if problems else 0


# --- selftest ---------------------------------------------------------


def _fixture_build(root, declared, executed_names, failed_names, inventory_total=None):
    """Monta um builddir falso; devolve (builddir, caminho do inventario)."""
    builddir = os.path.join(root, "build")
    temp = os.path.join(builddir, "Testing", "Temporary")
    os.makedirs(temp, exist_ok=True)
    total = declared if inventory_total is None else inventory_total
    inv = os.path.join(root, "parity_inventory.txt")
    with open(inv, "w", encoding="utf-8") as handle:
        handle.write(f"Test project x\n  Test #1: a\n\nTotal Tests: {total}\n")
    if executed_names is not None:
        lines = [f"{i + 1}/{declared} Testing: {n}\n{i + 1}/{declared} Test: {n}\n" for i, n in enumerate(executed_names)]
        with open(os.path.join(builddir, SNAPSHOT), "w", encoding="utf-8") as handle:
            handle.write("Start testing\n" + "".join(lines) + "End testing\n")
    if failed_names:
        with open(os.path.join(builddir, FAILED_LOG), "w", encoding="utf-8") as handle:
            handle.write("".join(f"{i + 1}:{n}\n" for i, n in enumerate(failed_names)))
    return builddir, inv


def _selftest_case(name, expect_rc, declared, executed_names, failed_names=(), inventory_total=None):
    import contextlib
    import io
    import tempfile

    with tempfile.TemporaryDirectory() as root:
        builddir, inv = _fixture_build(root, declared, executed_names, list(failed_names), inventory_total)
        buffer = io.StringIO()
        with contextlib.redirect_stdout(buffer), contextlib.redirect_stderr(buffer):
            rc = run(builddir, inv)
    if rc != expect_rc:
        print(f"selftest: {name} FALHOU (rc={rc}, esperado {expect_rc}): {buffer.getvalue()!r}", file=sys.stderr)
        return False
    print(f"selftest: {name} OK")
    return True


def selftest_main():
    controls = [
        _selftest_case("POSITIVO (3 declarados, 3 executados, 0 falhas)", 0, 3, ["a", "b", "c"]),
        # O mutante que o CTO pediu: um teste pulado tem de reprovar.
        _selftest_case("TESTE-PULADO (3 declarados, 2 executados)", 1, 3, ["a", "b"]),
        _selftest_case("EXECUTOU-A-MAIS (2 declarados, 3 executados)", 1, 2, ["a", "b", "c"]),
        _selftest_case("FALHA (3 executados, 1 falhou)", 1, 3, ["a", "b", "c"], ["b"]),
        _selftest_case("SNAPSHOT-AUSENTE (nao rodou)", 1, 3, None),
        _selftest_case("VARREDURA-VAZIA (0 declarados, 0 executados)", 1, 0, []),
        _selftest_case("RODAPE-AUSENTE", 1, 3, ["a", "b", "c"], inventory_total="sem rodape"),
    ]
    if not all(controls):
        print(f"{SCRIPT_NAME} --selftest: FALHOU (ver acima)", file=sys.stderr)
        sys.exit(1)
    print(f"{SCRIPT_NAME} --selftest: os {len(controls)} controles OK")


def _parse_real_args(args):
    if len(args) == 4 and args[0] == "--builddir" and args[2] == "--inventory":
        return args[1], args[3]
    print(f"usage: {SCRIPT_NAME} --builddir <dir> --inventory <arquivo>  |  --selftest", file=sys.stderr)
    sys.exit(2)


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
        return
    builddir, inventory = _parse_real_args(args)
    sys.exit(run(builddir, inventory))


if __name__ == "__main__":
    main()
