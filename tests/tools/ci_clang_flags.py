#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# ci_clang_flags.py - infra trail C2 (30/09/2026): prints, one per line, the
# cmake flags the CI job `clang` (.github/workflows/ci.yml) configures with
# for its FIRST matrix entry (modo compartilhado), so tools/preci.sh can run
# the same configure locally without anyone retyping the flags. The flags are
# READ from the workflow, never copied by hand: the day the job changes, the
# local stage follows.
#
# Reproves (L-40: never an empty or partial flag list that "passes"): no
# `clang:` job, no configure line in it, no matrix entry, or a result that
# lacks -DCMAKE_CXX_COMPILER=clang++ or -DGLINTFX_WERROR=ON.
#
# Usage:
#   ci_clang_flags.py <ci.yml>
#   ci_clang_flags.py --selftest

import os
import re
import shlex
import sys

SCRIPT_NAME = "ci_clang_flags.py"
REQUIRED = ("-DCMAKE_CXX_COMPILER=clang++", "-DGLINTFX_WERROR=ON")


def job_block(text):
    lines = text.splitlines()
    start = None
    for index, line in enumerate(lines):
        if start is None and re.match(r"^  clang:\s*$", line):
            start = index
            continue
        if start is not None and re.match(r"^  [A-Za-z0-9_-]+:\s*$", line):
            return lines[start:index]
    return lines[start:] if start is not None else None


def first_matrix_entry(block):
    entry = {}
    inside = False
    for line in block:
        if re.match(r"^\s+- modo:", line):
            if inside:
                break
            inside = True
        if inside:
            match = re.match(r'^\s+(?:- )?(\w+):\s*(.*?)\s*$', line)
            if match:
                entry[match.group(1)] = match.group(2).strip('"')
    return entry if inside else None


def extract(text):
    """(flags, None) or (None, error message)."""
    block = job_block(text)
    if block is None:
        return None, "job `clang` nao encontrado"
    entry = first_matrix_entry(block)
    if entry is None:
        return None, "nenhuma entrada de matriz no job `clang`"
    configure = None
    for line in block:
        if re.search(r"cmake -S \. -B \$\{\{ matrix\.builddir \}\}", line):
            configure = line
            break
    if configure is None:
        return None, "linha `cmake -S . -B ${{ matrix.builddir }}` nao encontrada no job `clang`"
    tail = configure.split("-G Ninja", 1)
    if len(tail) != 2:
        return None, "a linha de configure do job `clang` nao tem `-G Ninja`"
    rest = tail[1]
    for key, value in entry.items():
        rest = rest.replace("${{ matrix." + key + " }}", value)
    if "${{" in rest:
        return None, f"variavel de matriz nao resolvida na linha de configure: {rest.strip()}"
    flags = shlex.split(rest)
    missing = [flag for flag in REQUIRED if flag not in flags]
    if missing:
        return None, f"flag(s) ausente(s) do configure do job `clang`: {' '.join(missing)}"
    return flags, None


def run(path):
    try:
        with open(path, "r", encoding="utf-8") as handle:
            text = handle.read()
    except OSError as exc:
        print(f"{SCRIPT_NAME}: {path} ilegivel ({exc})", file=sys.stderr)
        return 1
    flags, error = extract(text)
    if error:
        print(f"{SCRIPT_NAME}: {error}", file=sys.stderr)
        return 1
    for flag in flags:
        print(flag)
    return 0


SAMPLE = """jobs:
  linux:
    name: x
  clang:
    name: Clang
    strategy:
      matrix:
        include:
          - modo: compartilhado
            builddir: build-shared
            shared_flag: ""
            oracle_flag: "-DGLINTFX_LAYERS_ORACLE=ON"
          - modo: estatico
            builddir: build-static
            shared_flag: "-DBUILD_SHARED_LIBS=OFF"
            oracle_flag: ""
    steps:
      - name: Build
        run: |
          cmake -S . -B ${{ matrix.builddir }} -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ ${{ matrix.shared_flag }} -DGLINTFX_WERROR=ON ${{ matrix.oracle_flag }}
  gitleaks:
    name: y
"""


def selftest_main():
    import contextlib
    import io

    def check(label, text, expect_flags=None, expect_error=None):
        flags, error = extract(text)
        if expect_flags is not None and flags != expect_flags:
            print(f"selftest: {label} FALHOU (flags {flags}, erro {error})", file=sys.stderr)
            return False
        if expect_error is not None and (flags is not None or expect_error not in (error or "")):
            print(f"selftest: {label} FALHOU (esperado erro '{expect_error}', obtido {flags} / {error})", file=sys.stderr)
            return False
        print(f"selftest: {label} OK")
        return True

    results = [
        check("flags da primeira entrada (compartilhado), variaveis resolvidas", SAMPLE,
              expect_flags=["-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_CXX_COMPILER=clang++",
                            "-DGLINTFX_WERROR=ON", "-DGLINTFX_LAYERS_ORACLE=ON"]),
        check("sem job clang", SAMPLE.replace("  clang:", "  outro:"), expect_error="job `clang` nao encontrado"),
        check("sem linha de configure", SAMPLE.replace("cmake -S . -B", "cmake -S x -B"),
              expect_error="nao encontrada"),
        check("sem -Werror reprova", SAMPLE.replace("-DGLINTFX_WERROR=ON", ""), expect_error="-DGLINTFX_WERROR=ON"),
        check("sem o compilador clang++ reprova", SAMPLE.replace("-DCMAKE_CXX_COMPILER=clang++", ""),
              expect_error="-DCMAKE_CXX_COMPILER=clang++"),
        # The job block must END at the next job: a following job with its own matching
        # configure line must never be read as the clang job's.
        check("o bloco do job clang termina no job seguinte",
              SAMPLE.replace("cmake -S . -B ${{ matrix.builddir }} -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ ${{ matrix.shared_flag }} -DGLINTFX_WERROR=ON ${{ matrix.oracle_flag }}", "true")
                    .replace("  gitleaks:\n    name: y\n",
                             "  gitleaks:\n    name: y\n    steps:\n      - run: cmake -S . -B ${{ matrix.builddir }} -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DGLINTFX_WERROR=ON\n"),
              expect_error="nao encontrada"),
        check("variavel de matriz nao resolvida reprova", SAMPLE.replace("${{ matrix.oracle_flag }}", "${{ matrix.outra }}"),
              expect_error="nao resolvida"),
    ]
    # The real workflow must yield a non-empty list with the two required flags.
    here = os.path.dirname(os.path.abspath(__file__))
    real = os.path.join(here, "..", "..", ".github", "workflows", "ci.yml")
    buffer = io.StringIO()
    with contextlib.redirect_stdout(buffer):
        rc = run(real)
    lines = [line for line in buffer.getvalue().splitlines() if line]
    if rc != 0 or len(lines) < 3 or not all(flag in lines for flag in REQUIRED):
        print(f"selftest: ci.yml REAL FALHOU (rc {rc}, flags {lines})", file=sys.stderr)
        results.append(False)
    else:
        print(f"selftest: ci.yml REAL OK ({len(lines)} flags lidas do workflow)")
        results.append(True)
    if not all(results):
        print(f"{SCRIPT_NAME} --selftest: FALHOU (ver acima)", file=sys.stderr)
        sys.exit(1)
    print(f"{SCRIPT_NAME} --selftest: os {len(results)} controles OK")


def main():
    args = sys.argv[1:]
    if args == ["--selftest"]:
        selftest_main()
        return
    if len(args) != 1:
        print(f"uso: {SCRIPT_NAME} <ci.yml>  |  --selftest", file=sys.stderr)
        sys.exit(2)
    sys.exit(run(args[0]))


if __name__ == "__main__":
    main()
