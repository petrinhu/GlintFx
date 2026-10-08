#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_examples_pass.py - DEMO-1 D1-fix-b (D-W8-93): exercises the examples
# pass (cmake/GlintfxExamples.cmake) on each planta of tests/examples_pass/
# plantas/, one fresh configure per planta, WITHOUT configuring the library.
#
# For each planta the pass must do exactly what its expectation says:
#   - a planta that must PASS: configure exit code 0, and the printed line
#     of the pass carries the expected counts;
#   - a planta that must be REFUSED: configure exit code different from 0,
#     and the expected FATAL message appears in the output.
# A wrong exit code or a missing message is a failure of that planta, with
# the planta's own log kept in the work directory.
#
# Piso (L-40): the plantas are enumerated FROM DISK. The script always prints
#   plantas=<n> conferidas=<c> falharam=<k>
# and refuses (exit 1) when n is 0 or different from PLANT_COUNT, so a planta
# that disappears (or one that appears without an expectation) is visible.
#
# No --selftest on purpose: this script registers nothing and has no fixture
# tree of its own; the plantas ARE its fixtures.
#
# Usage:
#   check_examples_pass.py --cmake <cmake> --generator <gen> \
#       --source <raiz-do-repo> --work <dir-de-trabalho>

import argparse
import os
import shutil
import subprocess
import sys

PLANT_COUNT = 8
SCRIPT_NAME = "check_examples_pass.py"
PLANTAS_RELDIR = os.path.join("tests", "examples_pass", "plantas")
PROJECT_RELDIR = os.path.join("tests", "examples_pass")

# name -> (must_pass, expected text in the output)
EXPECTATIONS = {
    "ok": (
        True,
        "glintfx: exemplos diretorios=1 alcancados=1 executaveis_tratados=1",
    ),
    "dois_executaveis": (
        True,
        "glintfx: exemplos diretorios=1 alcancados=1 executaveis_tratados=2",
    ),
    "vazio": (
        True,
        "glintfx: exemplos diretorios=0 alcancados=0 executaveis_tratados=0",
    ),
    "nao_alcancado": (
        False,
        "glintfx: examples/foo/ tem CMakeLists.txt mas nao foi adicionado: "
        "acrescente add_subdirectory(foo) em examples/CMakeLists.txt",
    ),
    "so_biblioteca": (
        False,
        "glintfx: examples/lib_only/ nao declara nenhum executavel",
    ),
    "executavel_na_raiz": (
        False,
        "glintfx: o executavel root_tool foi declarado em examples/CMakeLists.txt; "
        "cada exemplo mora no proprio diretorio",
    ),
    "sem_examples": (
        False,
        "GLINTFX_BUILD_EXAMPLES is ON but examples/CMakeLists.txt does not exist",
    ),
    "aninhado": (
        True,
        "glintfx: exemplos diretorios=1 alcancados=1 executaveis_tratados=1",
    ),
}


def normalize_space(text):
    """Collapses every run of whitespace to one space (D-W8-97).

    CMake breaks the text of a FATAL_ERROR over several lines (indent of two
    spaces, about 77 columns), so a raw substring test would refuse a message
    that is correct. Applied to BOTH sides: the output and the expectation.
    """
    return " ".join(text.split())


def list_plantas(plantas_dir):
    """Names of the planta directories on disk, sorted."""
    if not os.path.isdir(plantas_dir):
        return []
    return sorted(
        name for name in os.listdir(plantas_dir)
        if os.path.isdir(os.path.join(plantas_dir, name))
    )


def configure_planta(args, planta, build_dir):
    """One fresh configure of the planta; returns (rc, combined output)."""
    shutil.rmtree(build_dir, ignore_errors=True)
    command = [
        args.cmake,
        "--fresh",
        "-S", os.path.join(args.source, PROJECT_RELDIR),
        "-B", build_dir,
        "-G", args.generator,
        "-DGLINTFX_SOURCE_DIR=" + args.source,
        "-DPLANTA=" + planta,
    ]
    completed = subprocess.run(command, capture_output=True, text=True, check=False)
    output = completed.stdout + completed.stderr
    return completed.returncode, output


def problems_for(planta, rc, output):
    """Empty list when the planta behaved as expected; otherwise the reasons."""
    if planta not in EXPECTATIONS:
        return ["planta sem expectativa escrita no " + SCRIPT_NAME]
    must_pass, expected_text = EXPECTATIONS[planta]
    reasons = []
    if must_pass and rc != 0:
        reasons.append("rc=%d, esperado 0" % rc)
    if not must_pass and rc == 0:
        reasons.append("rc=0, esperado diferente de 0 (a passada deixou passar)")
    if normalize_space(expected_text) not in normalize_space(output):
        reasons.append("mensagem esperada ausente: " + expected_text)
    return reasons


def report_planta(planta, rc, reasons, log_path):
    """Prints one line per planta; the reasons are printed on failure."""
    status = "OK" if not reasons else "FALHOU"
    print("planta=%s rc=%d resultado=%s log=%s" % (planta, rc, status, log_path))
    for reason in reasons:
        print("  motivo: " + reason)


def run_all(args):
    """Runs every planta; returns (n, conferidas, falharam)."""
    plantas_dir = os.path.join(args.source, PLANTAS_RELDIR)
    plantas = list_plantas(plantas_dir)
    failures = 0
    checked = 0
    for planta in plantas:
        build_dir = os.path.join(args.work, planta)
        rc, output = configure_planta(args, planta, build_dir)
        log_path = os.path.join(args.work, planta + ".log")
        with open(log_path, "w", encoding="utf-8") as log:
            log.write(output)
        reasons = problems_for(planta, rc, output)
        report_planta(planta, rc, reasons, log_path)
        checked += 1
        if reasons:
            failures += 1
    return len(plantas), checked, failures


def parse_args(argv):
    parser = argparse.ArgumentParser(description=SCRIPT_NAME)
    parser.add_argument("--cmake", required=True)
    parser.add_argument("--generator", required=True)
    parser.add_argument("--source", required=True)
    parser.add_argument("--work", required=True)
    return parser.parse_args(argv)


def main(argv):
    args = parse_args(argv)
    os.makedirs(args.work, exist_ok=True)
    n, checked, failures = run_all(args)
    print("plantas=%d conferidas=%d falharam=%d" % (n, checked, failures))
    if n == 0 or n != PLANT_COUNT:
        print(SCRIPT_NAME + ": FALHOU - plantas=%d, o esperado sao %d "
              "(tirar ou acrescentar uma planta muda esta contagem)" % (n, PLANT_COUNT))
        return 1
    if failures:
        print(SCRIPT_NAME + ": FALHOU - %d planta(s) fora do esperado" % failures)
        return 1
    print(SCRIPT_NAME + ": OK - %d plantas conferidas" % checked)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
