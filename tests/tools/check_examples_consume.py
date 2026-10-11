#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_examples_consume.py - DEMO-1 D7 (D-W8-76): the gate that every example builds as a CONSUMER of an
# INSTALLED glintfx. It installs the build tree into a prefix of its own, configures examples/ on its own
# with CMAKE_PREFIX_PATH pointing only there (the find_package branch of examples/CMakeLists.txt,
# D-W8-86), and builds it. It never runs an example. Test tooling, not product code. Standard library only
# (L-07). Python, so the same gate runs on Linux and on Windows.
#
# The examples are a CLOSED enumeration: every examples/<name>/ with a CMakeLists.txt. Zero examples is a
# refusal (L-40). An example counts as built when its build directory holds an executable named like the
# example (<name>, or <name>.exe on Windows), directly or in the <config> subdirectory a multi-config generator
# uses: the convention of one main executable per example.
#
# Prints, always: `check_examples_consume: exemplos encontrados=<n> construidos=<m>`, and the exit code of
# each step. Exit 0 only when install, configure and build exit 0, n >= 1 and m == n.
#
# USAGE: check_examples_consume.py --cmake <cmake> --generator <generator> --compiler <c++ compiler>
#            --build <glintfx build dir> --examples <source>/examples --work <empty work dir> --config <config>
#        check_examples_consume.py --selftest

import argparse
import contextlib
import io
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

SCRIPT_NAME = "check_examples_consume.py"


def find_examples(examples_dir):
    """The closed enumeration: the names of the direct children of examples/ that hold a CMakeLists.txt."""
    root = Path(examples_dir)
    if not root.is_dir():
        return []
    return sorted(child.name for child in root.iterdir() if (child / "CMakeLists.txt").is_file())


def install_command(args, prefix):
    return [args.cmake, "--install", args.build, "--prefix", str(prefix), "--config", args.config]


def configure_command(args, prefix, build_dir):
    return [args.cmake, "-S", args.examples, "-B", str(build_dir), "-G", args.generator,
            f"-DCMAKE_PREFIX_PATH={prefix}", f"-DCMAKE_CXX_COMPILER={args.compiler}",
            f"-DCMAKE_BUILD_TYPE={args.config}"]


def build_command(args, build_dir):
    return [args.cmake, "--build", str(build_dir), "--config", args.config]


def executable_candidates(build_dir, name, config):
    """Where an example's executable lands: its build directory (single-config generators, Ninja and Make)
    or the <config> subdirectory of it (multi-config generators, Visual Studio), with or without .exe."""
    directory = Path(build_dir) / name
    return [folder / file for folder in (directory, directory / config) for file in (name, f"{name}.exe")]


def count_built(build_dir, names, config):
    """How many examples left an executable named like themselves in their build directory."""
    return sum(1 for name in names if any(path.is_file() for path in executable_candidates(build_dir, name, config)))


def consume_passes(codes, found, built):
    """The verdict: every step exited 0, at least one example, and every example built."""
    return all(code == 0 for code in codes) and found >= 1 and built == found


def run_process(command):
    completed = subprocess.run([str(part) for part in command], capture_output=True, text=True, check=False)
    return completed.returncode, completed.stdout + completed.stderr


def fresh_work(work):
    """The prefix and the build directory start empty; nothing else under work is touched (L-53)."""
    prefix, build_dir = Path(work) / "prefix", Path(work) / "build"
    for directory in (prefix, build_dir):
        shutil.rmtree(directory, ignore_errors=True)
    return prefix, build_dir


def run_steps(args, run, names):
    """Install, configure and build, in order; stops at the first step that fails. Returns the codes."""
    prefix, build_dir = fresh_work(args.work)
    codes = []
    for step, command in (("install", install_command(args, prefix)),
                          ("configure", configure_command(args, prefix, build_dir)),
                          ("build", build_command(args, build_dir))):
        code, output = run(command)
        print(f"{SCRIPT_NAME}: {step} rc={code}")
        if code != 0:
            sys.stdout.write(output if output.endswith("\n") or not output else output + "\n")
        codes.append(code)
        if code != 0:
            break
    return codes, build_dir


def check_consume(args, run=run_process):
    names = find_examples(args.examples)
    codes, build_dir = run_steps(args, run, names) if names else ([], Path(args.work) / "build")
    built = count_built(build_dir, names, args.config) if codes and all(code == 0 for code in codes) else 0
    print(f"check_examples_consume: exemplos encontrados={len(names)} construidos={built}")
    return consume_passes(codes, len(names), built)


def parse_args(argv):
    parser = argparse.ArgumentParser(prog=SCRIPT_NAME)
    for name in ("--cmake", "--generator", "--compiler", "--build", "--examples", "--work", "--config"):
        parser.add_argument(name, required=True)
    return parser.parse_args(argv)


# -- selftest ---------------------------------------------------------
CHECKS = []


def check(name, condition):
    CHECKS.append(name)
    if not condition:
        print(f"selftest: {name} FALHOU", file=sys.stderr)
        sys.exit(1)


def fake_args(root):
    return argparse.Namespace(cmake="CMAKE", generator="GEN", compiler="CXX", build="B",
                              examples=str(Path(root) / "examples"), work=str(Path(root) / "work"),
                              config="Release")


def write_example(root, name, executable=None):
    directory = Path(root) / "examples" / name
    directory.mkdir(parents=True, exist_ok=True)
    (directory / "CMakeLists.txt").write_text("add_executable(x x.cpp)\n")
    if executable:
        target = Path(root) / "work" / "build" / name / executable
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(b"x")


class build_double:
    """A run() double: records the commands, answers with `codes` in order, and, on the build step, leaves
    the executables `built` names in the build tree, as a real build would."""

    def __init__(self, codes, built=()):
        self.codes = list(codes)
        self.built = built
        self.calls = []

    def __call__(self, command):
        self.calls.append([str(part) for part in command])
        code = self.codes.pop(0)
        if command[1] == "--build" and code == 0:
            for name in self.built:
                (Path(command[2]) / name).mkdir(parents=True, exist_ok=True)
                (Path(command[2]) / name / name).write_bytes(b"x")
        return code, ""


def selftest_enumeration():
    with tempfile.TemporaryDirectory() as tmp:
        write_example(tmp, "a")
        (Path(tmp) / "examples" / "b").mkdir()
        (Path(tmp) / "examples" / "c.txt").write_text("x")
        check("a enumeracao pega so os filhos diretos de examples/ com CMakeLists.txt", find_examples(
            Path(tmp) / "examples") == ["a"])
        check("examples/ ausente da zero exemplos", find_examples(Path(tmp) / "nao-existe") == [])


def selftest_commands():
    args = fake_args("/r")
    prefix, build_dir = Path("/r/work/prefix"), Path("/r/work/build")
    check("a instalacao vai para o prefixo proprio, na configuracao pedida",
          install_command(args, prefix) == ["CMAKE", "--install", "B", "--prefix", "/r/work/prefix", "--config",
                                            "Release"])
    configure = configure_command(args, prefix, build_dir)
    check("a configuracao le examples/ sozinho, com CMAKE_PREFIX_PATH so no prefixo e o mesmo compilador",
          configure[:3] == ["CMAKE", "-S", str(Path("/r") / "examples")] and "-DCMAKE_PREFIX_PATH=/r/work/prefix"
          in configure and "-DCMAKE_CXX_COMPILER=CXX" in configure and configure[configure.index("-G") + 1] == "GEN")
    check("a construcao usa o diretorio proprio e a configuracao pedida",
          build_command(args, build_dir) == ["CMAKE", "--build", "/r/work/build", "--config", "Release"])


def selftest_counting():
    with tempfile.TemporaryDirectory() as tmp:
        write_example(tmp, "a", executable="a")
        write_example(tmp, "w", executable="w.exe")
        write_example(tmp, "v", executable="Release/v.exe")
        write_example(tmp, "z")
        write_example(tmp, "d", executable="Debug/d.exe")
        check("conta como construido o executavel com o nome do exemplo, com ou sem .exe, direto ou na pasta da "
              "configuracao pedida, e so ele", count_built(Path(tmp) / "work" / "build", ["a", "w", "v", "z", "d"],
                                                          "Release") == 3)


def selftest_verdict():
    check("tudo com rc 0, um exemplo, um construido: passa", consume_passes([0, 0, 0], 1, 1))
    check("zero exemplos reprova", not consume_passes([], 0, 0))
    check("um passo com rc diferente de 0 reprova", not consume_passes([0, 2, 0], 1, 1))
    check("menos construidos que encontrados reprova", not consume_passes([0, 0, 0], 2, 1))


def consume_with(root, run):
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        passed = check_consume(fake_args(root), run)
    return passed, out.getvalue()


def selftest_whole_run():
    with tempfile.TemporaryDirectory() as tmp:
        write_example(tmp, "first_window")
        passed, out = consume_with(tmp, build_double([0, 0, 0], built=("first_window",)))
        check("integracao: instala, configura e constroi, e imprime encontrados=1 construidos=1",
              passed and "check_examples_consume: exemplos encontrados=1 construidos=1" in out.splitlines())
        failed = build_double([0, 0, 1])
        passed, out = consume_with(tmp, failed)
        check("integracao: a construcao falha, reprova, e a linha diz construidos=0",
              not passed and "check_examples_consume: exemplos encontrados=1 construidos=0" in out.splitlines()
              and len(failed.calls) == 3)
        silent = build_double([0, 0, 0], built=())
        passed, out = consume_with(tmp, silent)
        check("integracao: a construcao sai 0 sem gerar o executavel, reprova, e a linha diz construidos=0",
              not passed and "check_examples_consume: exemplos encontrados=1 construidos=0" in out.splitlines())
    with tempfile.TemporaryDirectory() as tmp:
        (Path(tmp) / "examples").mkdir()
        run = build_double([])
        passed, out = consume_with(tmp, run)
        check("integracao: examples/ sem exemplo reprova sem rodar nada, e a linha diz encontrados=0",
              not passed and run.calls == []
              and "check_examples_consume: exemplos encontrados=0 construidos=0" in out.splitlines())


def selftest_process_level():
    proc = subprocess.run([sys.executable, str(Path(__file__).resolve())], capture_output=True, text=True,
                          check=False)
    check("processo: sem argumentos sai 2", proc.returncode == 2)


def selftest_main():
    selftest_enumeration()
    selftest_commands()
    selftest_counting()
    selftest_verdict()
    selftest_whole_run()
    selftest_process_level()
    print(f"selftest: {len(CHECKS)} controles OK")


def main(argv):
    if argv and argv[0] == "--selftest":
        selftest_main()
        return 0
    args = parse_args(argv)
    return 0 if check_consume(args) else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
