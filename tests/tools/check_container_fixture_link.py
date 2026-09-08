#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_container_fixture_link.py - closes the blind spot the sibling
# gate (check_container_fixture_includes.py) cannot: it proves every
# #include in tests/container/Containerfile's own TUs RESOLVES to a real
# file, but never proves those TUs actually LINK. Compile-time closure
# and link-time closure are DIFFERENT graphs - display_adapter.cpp
# #includes nothing new when it starts calling a function from a sibling
# .cpp file, so the includes gate stays green while the g++ invocation
# that never lists that sibling .cpp dies at `ld` time with "undefined
# reference". That is the exact, real, measured shape of the defect
# TODO.md's INBOX names on 07/09/2026 ("Lista de fontes mantida a mao e
# a familia que mais mordeu nesta onda: TRES vezes, em tres arquivos"),
# and its FOURTH occurrence, on 08/09/2026 (server run 34192811273):
# flush_retry_policy.cpp (src/platform/wayland/) was born, display_
# adapter.cpp started calling flush_write_wait_is_fatal() from it, and
# every one of this Containerfile's own g++ invocations that already
# linked display_adapter.cpp kept its OLD file list - the includes gate
# had nothing to say about it (no header moved, nothing failed to
# resolve), and only a full `docker build`, eleven minutes in, ever
# found out.
#
# WHAT THIS SCRIPT DOES, AND WHY IT IS GROUND TRUTH, NOT A GUESS: it
# does not try to model "which symbol does file X call that file Y
# defines" - that is exactly the kind of hand-maintained inventory this
# whole family of defect is about (GODS_LAWS.md L-17: a portao that
# itself keeps a curated list just moves the disease one file over). It
# reads the SAME COPY/RUN instructions of the Containerfile's own FIRST
# stage (arch-ports-builder) that `docker build` reads, and actually
# RUNS them - the real g++/ld, against the real staged tree
# (tests/container/_arch_ports_src/, produced by this same script's own
# sibling, prepare_arch_ports_fixture.sh) - in a throwaway local
# directory, never inside Docker. A `RUN dnf -y install ...` line is
# recognized and SKIPPED (this project's own build/CI machines already
# carry the wayland-client/wayland-egl/egl/wayland-protocols dev
# packages a native `cmake --build` already depends on - GODS_LAWS.md
# L-14 forbids installing packages without the leader's authorization,
# and none is needed here); every `wayland-scanner`/`g++`/`gcc` line
# runs for real, with `/build` rewritten to the throwaway directory.
# Any reason a target fails to link - a missing atom, a genuine ODR
# violation, a typo in a flag - surfaces as the REAL toolchain error,
# never a heuristic approximation of one.
#
# GODS_LAWS.md L-11 (no fork-per-item-scanned): this is NOT a sweep
# that forks a process per file varrido - it is the build itself,
# fewer than twenty subprocess calls total, the same count `docker
# build` would spend on this exact stage. GODS_LAWS.md L-40 (piso de
# varredura nao-vazia): the g++/gcc invocation count is printed
# unconditionally, and zero reproves - a Containerfile that stops
# naming any compile step is a broken portao, not a clean one.
#
# Usage:
#   check_container_fixture_link.py --exec <Containerfile> <context-dir> <staged-dir> <repo-root>
#   check_container_fixture_link.py --selftest
#
# Wired into tests/container/prepare_arch_ports_fixture.sh's own
# main(), right after verify_fixture_includes() - same reason that
# script's own header comment already gives for running the includes
# gate there: .github/workflows/ci.yml's `wayland-container` job runs
# that script BEFORE `docker build`, so a link gap reproves before the
# far more expensive image build ever starts (GODS_LAWS.md L-40).
#
# Each function below does one thing (GODS_LAWS.md L-17).

import os
import re
import shutil
import subprocess
import sys
import tempfile

SCRIPT_NAME = "check_container_fixture_link.py"
STDERR_TAIL_LINES = 20


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


def read_file(path):
    try:
        with open(path, "r", encoding="utf-8") as handle:
            return handle.read()
    except OSError as exc:
        fail(f"arquivo nao encontrado: {path} ({exc})")


# --- parsing the Containerfile's FIRST stage only ------------------------


# The image has two stages (arch-ports-builder, then the runtime
# stage that only COPYs the already-built binaries in). Only the FIRST
# stage compiles anything - the bare `FROM fedora:44` line (no `AS ...`)
# that starts the second stage is where this walk stops, so a `RUN
# chmod ...`/COPY --from=... in the runtime stage is never mistaken for
# a build instruction.
def extract_builder_stage_lines(containerfile_text):
    lines = containerfile_text.splitlines()
    start = None
    end = len(lines)
    for i, line in enumerate(lines):
        stripped = line.strip()
        if stripped.startswith("FROM ") and " AS " in stripped and start is None:
            start = i + 1
            continue
        if start is not None and stripped.startswith("FROM ") and " AS " not in stripped:
            end = i
            break
    if start is None:
        return []
    return lines[start:end]


# COPY lines in this file are always single physical lines (never a `\`
# continuation) - confirmed against the real file before writing this
# function (GODS_LAWS.md L-44: measured, not assumed).
_COPY_RE = re.compile(r"^COPY\s+(\S+)\s+(/build/\S+)\s*$")


# RUN blocks join `\`-continued physical lines into ONE logical string,
# same shape as check_container_fixture_includes.py's own
# extract_run_blocks() (duplicated here rather than imported: these two
# scripts are independent portoes on purpose, GODS_LAWS.md L-36 - one's
# own parsing bug must never silently disable the other).
def extract_instructions(builder_stage_lines):
    instructions = []
    i = 0
    n = len(builder_stage_lines)
    while i < n:
        line = builder_stage_lines[i]
        stripped = line.strip()
        if stripped.startswith("RUN "):
            parts = []
            while True:
                current = builder_stage_lines[i].rstrip()
                if current.endswith("\\"):
                    parts.append(current[:-1].strip())
                    i += 1
                else:
                    parts.append(current.strip())
                    i += 1
                    break
            joined = " ".join(parts)
            if joined.startswith("RUN "):
                joined = joined[len("RUN "):]
            instructions.append(("RUN", joined))
            continue
        match = _COPY_RE.match(stripped)
        if match:
            instructions.append(("COPY", (match.group(1), match.group(2))))
        i += 1
    return instructions


def split_subcommands(run_block_text):
    return [chunk.strip() for chunk in re.split(r"\s&&\s", run_block_text) if chunk.strip()]


# --- materializing COPY instructions into a throwaway build dir ----------


# `COPY _arch_ports_src /build/_arch_ports_src` mirrors the WHOLE staged
# tree; every other `COPY <file> /build/<file>` mirrors ONE fixture
# source straight from the build context (tests/container/). A symlink
# is enough for the tree (this script never writes into it) and cheaper
# than copying a tree prepare_arch_ports_fixture.sh may have staged with
# hundreds of files.
def apply_copy(src_token, dst_token, context_dir, staged_dir, build_dir):
    if not dst_token.startswith("/build/"):
        fail(f"instrucao COPY com alvo fora de /build: {dst_token}")
    rel_dst = dst_token[len("/build/"):]
    dst_path = os.path.join(build_dir, rel_dst)
    os.makedirs(os.path.dirname(dst_path) or build_dir, exist_ok=True)
    if src_token == "_arch_ports_src":
        os.symlink(staged_dir, dst_path)
        return
    src_path = os.path.join(context_dir, src_token)
    if not os.path.isfile(src_path):
        fail(f"COPY cita {src_token}, que nao existe em {context_dir}")
    shutil.copyfile(src_path, dst_path)


# --- executing RUN subcommands against the throwaway build dir -----------


_OUTPUT_RE = re.compile(r"-o\s+(\S+)")


def target_label(subcommand):
    match = _OUTPUT_RE.search(subcommand)
    if match:
        return match.group(1)
    return subcommand.split()[0] if subcommand.split() else "<vazio>"


def is_dnf_line(subcommand):
    tokens = subcommand.split()
    return bool(tokens) and tokens[0] == "dnf"


def is_compile_line(subcommand):
    tokens = subcommand.split()
    return bool(tokens) and tokens[0] in ("g++", "gcc")


def is_wayland_scanner_line(subcommand):
    tokens = subcommand.split()
    return bool(tokens) and tokens[0] == "wayland-scanner"


# `/build` is rewritten to the real throwaway directory - the ONLY
# absolute-path family this Containerfile's own build stage ever
# names (measured before writing this function), so a plain substring
# replace is exact, never a partial match on something else.
def rewrite_build_prefix(subcommand, build_dir):
    return subcommand.replace("/build", build_dir)


def run_subcommand(subcommand, build_dir):
    rewritten = rewrite_build_prefix(subcommand, build_dir)
    result = subprocess.run(
        ["sh", "-c", rewritten],
        cwd=build_dir,
        capture_output=True,
        text=True,
    )
    return result.returncode, result.stdout, result.stderr


def stderr_tail(stderr_text, n=STDERR_TAIL_LINES):
    lines = stderr_text.splitlines()
    if len(lines) <= n:
        return "\n".join(lines)
    return "\n".join(["...(truncado)..."] + lines[-n:])


# --- the run itself, factored out for --selftest -------------------------


# Returns (summary_dict, errors) - summary is printed UNCONDITIONALLY
# (GODS_LAWS.md L-40: the count appears even when the gate passes).
def run_link_check(containerfile_text, context_dir, staged_dir, build_dir):
    instructions = extract_instructions(extract_builder_stage_lines(containerfile_text))

    copy_count = 0
    dnf_skipped = 0
    scanner_run = 0
    compile_total = 0
    compile_ok = 0
    failures = []

    for kind, payload in instructions:
        if kind == "COPY":
            src_token, dst_token = payload
            apply_copy(src_token, dst_token, context_dir, staged_dir, build_dir)
            copy_count += 1
            continue
        # kind == "RUN"
        for subcommand in split_subcommands(payload):
            if is_dnf_line(subcommand):
                dnf_skipped += 1
                continue
            if is_wayland_scanner_line(subcommand):
                returncode, _stdout, stderr = run_subcommand(subcommand, build_dir)
                scanner_run += 1
                if returncode != 0:
                    failures.append({
                        "target": "wayland-scanner",
                        "command": subcommand,
                        "stderr": stderr_tail(stderr),
                    })
                continue
            if is_compile_line(subcommand):
                compile_total += 1
                returncode, _stdout, stderr = run_subcommand(subcommand, build_dir)
                if returncode == 0:
                    compile_ok += 1
                else:
                    failures.append({
                        "target": target_label(subcommand),
                        "command": subcommand,
                        "stderr": stderr_tail(stderr),
                    })
                continue
            # Anything else in a RUN block this stage might grow (there is
            # none today) is neither silently run nor silently ignored -
            # it is executed too, so a future subcommand kind is proven,
            # not assumed harmless.
            returncode, _stdout, stderr = run_subcommand(subcommand, build_dir)
            if returncode != 0:
                failures.append({
                    "target": target_label(subcommand),
                    "command": subcommand,
                    "stderr": stderr_tail(stderr),
                })

    summary = {
        "copy": copy_count,
        "dnf_skipped": dnf_skipped,
        "scanner_run": scanner_run,
        "compile_total": compile_total,
        "compile_ok": compile_ok,
        "failures": failures,
    }

    errors = []
    if compile_total == 0:
        errors.append(
            "varredura vazia: nenhuma invocacao g++/gcc encontrada no estagio arch-ports-builder de "
            "tests/container/Containerfile - GODS_LAWS.md L-40, isto e sinal de coleta quebrada"
        )
    for failure in failures:
        errors.append(
            f"{failure['target']}: nao compilou/ligou -> {failure['command']}\n{failure['stderr']}"
        )

    return summary, errors


def print_summary(summary):
    print(
        f"{SCRIPT_NAME}: COPY aplicado(s): {summary['copy']} | "
        f"RUN dnf pulado(s): {summary['dnf_skipped']} | "
        f"wayland-scanner executado(s): {summary['scanner_run']} | "
        f"g++/gcc encontrado(s): {summary['compile_total']} | "
        f"ligaram: {summary['compile_ok']} | "
        f"falharam: {len(summary['failures'])}"
    )


# --- real mode -------------------------------------------------------


def real_main(args):
    if len(args) != 4:
        fail(
            "usage: check_container_fixture_link.py --exec "
            "<Containerfile> <context-dir> <staged-dir> <repo-root>"
        )
    containerfile_path, context_dir, staged_dir, _repo_root = args
    containerfile_text = read_file(containerfile_path)

    if not os.path.isdir(staged_dir):
        fail(
            f"staged-dir nao existe: {staged_dir} - rode prepare_arch_ports_fixture.sh antes "
            "(este portao le a fixture ja estagiada, nunca a estagia sozinho)"
        )

    for pkg in ("wayland-client", "wayland-protocols"):
        probe = subprocess.run(["pkg-config", "--exists", pkg])
        if probe.returncode != 0:
            fail(
                f"pkg-config nao encontra '{pkg}' neste host - este portao roda os mesmos g++/"
                "wayland-scanner do Containerfile FORA do container, e precisa dos mesmos pacotes "
                "de desenvolvimento que 'cmake --build' ja exige (GODS_LAWS.md L-14: instale-os "
                "com autorizacao do lider antes de rodar este portao, nunca por conta propria)"
            )

    build_dir = tempfile.mkdtemp(prefix="glintfx-fixture-link-", dir=os.environ.get("TMPDIR"))
    try:
        summary, errors = run_link_check(containerfile_text, context_dir, staged_dir, build_dir)
    finally:
        shutil.rmtree(build_dir, ignore_errors=True)

    print_summary(summary)

    if errors:
        print(f"{SCRIPT_NAME}: REPROVADO ({len(errors)} problema(s)):", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        sys.exit(1)

    print(f"{SCRIPT_NAME}: OK - todo alvo g++/gcc do estagio arch-ports-builder compilou e ligou")


# --- fixtures and controls for --selftest -----------------------------


def _write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as handle:
        handle.write(text)


def _make_scratch():
    return tempfile.mkdtemp(prefix="glintfx-fixture-link-selftest-", dir=os.environ.get("TMPDIR"))


# Two-file "production layer" (an atom.cpp/.hpp pair, mirroring
# flush_retry_policy.cpp/.hpp) plus one fixture .cpp that calls into it
# (mirroring display_adapter.cpp calling flush_write_wait_is_fatal()) -
# the SAME shape as the real defect, kept hermetic (no wayland headers,
# so this selftest never needs the host's dev packages).
def _build_base_fixture(scratch, label, include_atom_in_link):
    root = os.path.join(scratch, label)
    context_dir = os.path.join(root, "tests", "container")
    staged_dir = os.path.join(context_dir, "_arch_ports_src")

    _write(
        os.path.join(staged_dir, "src", "atom.hpp"),
        "#pragma once\nint atom_value();\n",
    )
    _write(
        os.path.join(staged_dir, "src", "atom.cpp"),
        '#include "atom.hpp"\nint atom_value() { return 42; }\n',
    )
    _write(
        os.path.join(staged_dir, "src", "consumer.hpp"),
        "#pragma once\nint consumer_value();\n",
    )
    _write(
        os.path.join(staged_dir, "src", "consumer.cpp"),
        '#include "consumer.hpp"\n#include "atom.hpp"\nint consumer_value() { return atom_value() + 1; }\n',
    )
    _write(
        os.path.join(context_dir, "main_smoke.cpp"),
        '#include "consumer.hpp"\n#include <cstdio>\nint main() { std::printf("%d\\n", consumer_value()); return 0; }\n',
    )

    consumer_line = "        /build/_arch_ports_src/src/consumer.cpp \\\n"
    atom_line = "        /build/_arch_ports_src/src/atom.cpp \\\n" if include_atom_in_link else ""

    containerfile = (
        "FROM fedora:44 AS arch-ports-builder\n"
        "RUN dnf -y install gcc-c++ \\\n"
        "    && dnf clean all\n"
        "COPY _arch_ports_src /build/_arch_ports_src\n"
        "COPY main_smoke.cpp /build/main_smoke.cpp\n"
        "RUN g++ -std=c++23 -O2 -Wall -Wextra -Werror \\\n"
        "        -I /build/_arch_ports_src/src \\\n"
        "        -o /build/main_smoke \\\n"
        + consumer_line
        + atom_line
        + "        /build/main_smoke.cpp\n"
        "\n"
        "FROM fedora:44\n"
        "RUN echo runtime-stage-never-compiles-anything\n"
    )

    return root, context_dir, staged_dir, containerfile


def _run_selftest_case(scratch, label, include_atom_in_link):
    root, context_dir, staged_dir, containerfile = _build_base_fixture(scratch, label, include_atom_in_link)
    build_dir = tempfile.mkdtemp(prefix=f"glintfx-fixture-link-selftest-build-{label}-", dir=scratch)
    try:
        return run_link_check(containerfile, context_dir, staged_dir, build_dir)
    finally:
        shutil.rmtree(build_dir, ignore_errors=True)


def selftest_positive_control(scratch):
    summary, errors = _run_selftest_case(scratch, "positive", include_atom_in_link=True)
    if errors:
        print(f"selftest: controle POSITIVO FALHOU (esperava zero erros, veio {errors})", file=sys.stderr)
        return False
    if summary["compile_total"] != 1 or summary["compile_ok"] != 1:
        print(f"selftest: controle POSITIVO FALHOU (contagens inesperadas: {summary})", file=sys.stderr)
        return False
    print(f"selftest: controle POSITIVO OK (atom.cpp presente no link, liga limpo: {summary})")
    return True


# VERMELHO (a razao deste portao existir): a mesma forma exata do
# defeito real - main_smoke.cpp/consumer.cpp chamam para dentro de
# atom.cpp atraves de um header que RESOLVE normalmente (a includes-
# gate nao veria nada de errado aqui), mas a invocacao g++ nunca lista
# atom.cpp - "undefined reference to atom_value()" no ld real.
def selftest_missing_atom_reproves(scratch):
    summary, errors = _run_selftest_case(scratch, "missing-atom", include_atom_in_link=False)
    if not errors:
        print("selftest: VERMELHO FALHOU (atom.cpp ausente do link deveria ter reprovado)", file=sys.stderr)
        return False
    if summary["compile_ok"] != 0 or len(summary["failures"]) != 1:
        print(f"selftest: VERMELHO FALHOU (contagens erradas): {summary}", file=sys.stderr)
        return False
    failure_text = "\n".join(errors)
    if "atom_value" not in failure_text or "undefined reference" not in failure_text:
        print(f"selftest: VERMELHO FALHOU (nao cita o simbolo/erro real do ld): {errors}", file=sys.stderr)
        return False
    print(f"selftest: VERMELHO OK (atom.cpp ausente do link pego pelo ld real, simbolo citado): {errors}")
    return True


def selftest_empty_containerfile_reproves(scratch):
    root = os.path.join(scratch, "empty")
    context_dir = os.path.join(root, "tests", "container")
    staged_dir = os.path.join(context_dir, "_arch_ports_src")
    os.makedirs(staged_dir, exist_ok=True)
    containerfile = (
        "FROM fedora:44 AS arch-ports-builder\n"
        "RUN dnf -y install gcc-c++\n"
        "\n"
        "FROM fedora:44\n"
        "RUN echo runtime-stage-never-compiles-anything\n"
    )
    build_dir = tempfile.mkdtemp(prefix="glintfx-fixture-link-selftest-build-empty-", dir=scratch)
    try:
        summary, errors = run_link_check(containerfile, context_dir, staged_dir, build_dir)
    finally:
        shutil.rmtree(build_dir, ignore_errors=True)
    if not errors:
        print("selftest: EMPTY FALHOU (estagio sem g++/gcc deveria ter reprovado)", file=sys.stderr)
        return False
    if summary["compile_total"] != 0 or not any("varredura vazia" in e for e in errors):
        print(f"selftest: EMPTY FALHOU (contagens/mensagem erradas): {summary} {errors}", file=sys.stderr)
        return False
    if summary["dnf_skipped"] != 1:
        print(f"selftest: EMPTY FALHOU (dnf deveria ter sido pulado, nao executado): {summary}", file=sys.stderr)
        return False
    print(f"selftest: EMPTY OK (estagio sem nenhuma compilacao pego, dnf pulado sem tentar rodar): {errors}")
    return True


# Controle POSITIVO adicional: um segundo alvo, no MESMO Containerfile,
# que falta o atomo - prova que o portao acumula TODAS as falhas, nao
# so a primeira (o defeito real mordeu 16 alvos ao mesmo tempo; um
# portao que parasse no primeiro nunca provaria o tamanho real do dano).
def selftest_accumulates_multiple_failures(scratch):
    root = os.path.join(scratch, "multi")
    context_dir = os.path.join(root, "tests", "container")
    staged_dir = os.path.join(context_dir, "_arch_ports_src")
    _write(os.path.join(staged_dir, "src", "atom.hpp"), "#pragma once\nint atom_value();\n")
    _write(os.path.join(staged_dir, "src", "atom.cpp"), '#include "atom.hpp"\nint atom_value() { return 42; }\n')
    _write(os.path.join(staged_dir, "src", "consumer.hpp"), "#pragma once\nint consumer_value();\n")
    _write(
        os.path.join(staged_dir, "src", "consumer.cpp"),
        '#include "consumer.hpp"\n#include "atom.hpp"\nint consumer_value() { return atom_value() + 1; }\n',
    )
    _write(
        os.path.join(context_dir, "first_smoke.cpp"),
        '#include "consumer.hpp"\nint main() { return consumer_value(); }\n',
    )
    _write(
        os.path.join(context_dir, "second_smoke.cpp"),
        '#include "consumer.hpp"\nint main() { return consumer_value() - 1; }\n',
    )
    containerfile = (
        "FROM fedora:44 AS arch-ports-builder\n"
        "COPY _arch_ports_src /build/_arch_ports_src\n"
        "COPY first_smoke.cpp /build/first_smoke.cpp\n"
        "COPY second_smoke.cpp /build/second_smoke.cpp\n"
        "RUN g++ -std=c++23 -Wall -Wextra -Werror -I /build/_arch_ports_src/src \\\n"
        "        -o /build/first_smoke \\\n"
        "        /build/_arch_ports_src/src/consumer.cpp \\\n"
        "        /build/first_smoke.cpp \\\n"
        "    && g++ -std=c++23 -Wall -Wextra -Werror -I /build/_arch_ports_src/src \\\n"
        "        -o /build/second_smoke \\\n"
        "        /build/_arch_ports_src/src/consumer.cpp \\\n"
        "        /build/second_smoke.cpp\n"
        "\n"
        "FROM fedora:44\n"
        "RUN echo runtime-stage-never-compiles-anything\n"
    )
    build_dir = tempfile.mkdtemp(prefix="glintfx-fixture-link-selftest-build-multi-", dir=scratch)
    try:
        summary, errors = run_link_check(containerfile, context_dir, staged_dir, build_dir)
    finally:
        shutil.rmtree(build_dir, ignore_errors=True)
    if summary["compile_total"] != 2 or len(summary["failures"]) != 2:
        print(f"selftest: MULTI FALHOU (esperava 2 alvos e 2 falhas, veio {summary})", file=sys.stderr)
        return False
    if not any("first_smoke" in f["target"] for f in summary["failures"]):
        print(f"selftest: MULTI FALHOU (first_smoke nao citado nas falhas): {summary['failures']}", file=sys.stderr)
        return False
    if not any("second_smoke" in f["target"] for f in summary["failures"]):
        print(f"selftest: MULTI FALHOU (second_smoke nao citado nas falhas): {summary['failures']}", file=sys.stderr)
        return False
    print(f"selftest: MULTI OK (os DOIS alvos que faltam o atomo foram pegos, nenhum escondeu o outro)")
    return True


def selftest_main():
    scratch = _make_scratch()
    try:
        controls = [
            selftest_positive_control(scratch),
            selftest_missing_atom_reproves(scratch),
            selftest_empty_containerfile_reproves(scratch),
            selftest_accumulates_multiple_failures(scratch),
        ]
    finally:
        shutil.rmtree(scratch, ignore_errors=True)
    if not all(controls):
        print(f"{SCRIPT_NAME} --selftest: FALHOU (ver acima)", file=sys.stderr)
        sys.exit(1)
    print(f"{SCRIPT_NAME} --selftest: os {len(controls)} controles OK")


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
    elif args and args[0] == "--exec":
        real_main(args[1:])
    else:
        fail(
            "usage: check_container_fixture_link.py --exec "
            "<Containerfile> <context-dir> <staged-dir> <repo-root>  |  --selftest"
        )


if __name__ == "__main__":
    main()
