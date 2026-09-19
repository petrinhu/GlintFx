#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_container_smokes_in_compile_db.py - LINT-CONTAINER-SMOKES
# (TODO.md, GODS_LAWS.md L-09/L-40): tests/container/*.cpp are the
# Wayland/EGL fixtures the wayland-container CI job compiles with raw
# g++ invocations INSIDE tests/container/Containerfile and runs INSIDE
# a container (GODS_LAWS.md L-09 - never in the host session). Because
# they never entered this project's OWN CMake build on the host, they
# never entered compile_commands.json either, and run-clang-tidy (see
# tools/preci.sh's own stage_tidy) only ever analyzes what is IN that
# bank - it silently skips a file that is not there, success and all
# (the exact "varredura em lote que pode pular no meio ESCONDE
# cobertura perdida" shape GODS_LAWS.md L-36/L-40 name). Measured
# 19/09/2026, before tests/container/CMakeLists.txt existed: 20 .cpp
# tracked under tests/container/, only 1 (alloc_counter_classify.cpp,
# compiled into the REAL, RUNNING alloc_counter_classify_test - see
# tests/CMakeLists.txt's own comment above that target) in the bank.
#
# This gate is the ORACLE for that gap: it enumerates tests/container/
# *.cpp by `git ls-files` (never by os.walk - a file only staged, or a
# stray untracked one left over from a scratch edit, must not silently
# count) and cross-checks each one against compile_commands.json,
# matching by FULL NORMALIZED PATH, never by leaf name. Leaf-name
# matching is the exact defect tools/preci.sh's own stage_tidy still
# carries (grep -qF "/${base}" against a flat basename list) - this
# project has six pairs of same-leaf-name files living in different
# platform directories (src/platform/wayland/ vs src/platform/win32/,
# mirrored test fixtures), and the orchestrator who briefed this fatia
# measured 93313 false "found" hits from matching this tree's own
# compile_commands.json by basename alone. A leaf-name match here
# would report a missing fixture as already covered whenever some
# OTHER file anywhere in the bank happens to share its basename.
# Full-path comparison cannot make that mistake - proved by
# selftest_leaf_name_collision_control() below.
#
# It does NOT itself run clang-tidy or add the fixtures to the build -
# tests/container/CMakeLists.txt (an OBJECT library, GODS_LAWS.md L-09:
# COMPILED, never linked, never executed on the host) does that. This
# script only PROVES the fixtures landed in the bank once that library
# exists, and stays red for as long as any one of them has not.
#
# Usage:
#   check_container_smokes_in_compile_db.py <repo_root> <compile_commands_json>
#   check_container_smokes_in_compile_db.py --selftest
#
# --selftest runs four controls (positive, negative-names-the-file,
# empty-scan floor, leaf-name-collision) against throwaway git
# repositories under a real temp directory - never against the real
# tree. See selftest_main() below.
#
# Each function below does one thing (GODS_LAWS.md L-17).

import json
import os
import subprocess
import sys
import tempfile

SCRIPT_NAME = "check_container_smokes_in_compile_db.py"

CONTAINER_CPP_PATHSPEC = "tests/container/*.cpp"


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


# --- enumeration -------------------------------------------------------


def git_ls_files_z(root, extra_args):
    """Runs `git -C root ls-files -z <extra_args>`, decoded tolerantly
    (surrogateescape, same round-trip os.fsdecode() uses for raw OS
    path bytes). Returns (paths, ok); ok is False only when git itself
    could not run (missing, or root is not a git repository) - the
    caller turns that into an explicit, named failure, never a silent
    empty result. Same shape check_spdx.py's own git_ls_files_z uses.
    """
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


def tracked_container_cpp_files(root):
    """Every tests/container/*.cpp `git ls-files` reports, relative to
    root, forward-slash separated. Returns (paths, ok).
    """
    return git_ls_files_z(root, ["--", CONTAINER_CPP_PATHSPEC])


# --- compile_commands.json reading --------------------------------------


def normalized_path(path):
    """Absolute, normalized (no '..'/'.' segments, OS-native
    separator) - the comparison key. Never touches the filesystem (no
    realpath/symlink resolution), so it works identically against a
    real build tree and a --selftest fixture that names files which do
    not actually exist on disk.
    """
    return os.path.normpath(os.path.abspath(path))


def compiled_file_paths(compile_db_path):
    """The set of normalized 'file' entries compile_commands.json
    lists, resolved against each entry's own 'directory' when 'file'
    is not already absolute (the Compilation Database Format allows
    either - see clang.llvm.org/docs/JSONCompilationDatabase.html).
    Returns (paths_set, ok); ok is False when the file is missing or
    is not valid JSON - a gate that cannot read its own oracle refuses
    rather than silently treating that as "nothing compiled".
    """
    try:
        with open(compile_db_path, "r", encoding="utf-8") as handle:
            entries = json.load(handle)
    except (OSError, json.JSONDecodeError):
        return set(), False

    paths = set()
    for entry in entries:
        file_field = entry.get("file", "")
        if not file_field:
            continue
        if os.path.isabs(file_field):
            resolved = file_field
        else:
            directory = entry.get("directory", "")
            resolved = os.path.join(directory, file_field)
        paths.add(normalized_path(resolved))
    return paths, True


# --- check ---------------------------------------------------------------


def check_container_smokes_in_compile_db(root, compile_db_path):
    """Returns (ok, encontrados, no_banco, missing_relative_paths).
    missing_relative_paths is always the CONTAINER_CPP_PATHSPEC-
    relative form (root-independent), so the message reads the same on
    every machine/container that runs this gate.
    """
    tracked, git_ok = tracked_container_cpp_files(root)
    if not git_ok:
        print(
            f"{SCRIPT_NAME}: 'git ls-files' falhou em '{root}' (nao e "
            "repositorio git, ou git indisponivel) - varredura recusada, "
            "nunca presumida vazia",
            file=sys.stderr,
        )
        return False, 0, 0, []

    encontrados = len(tracked)
    if encontrados == 0:
        print(f"{SCRIPT_NAME}: varredura vazia (0 arquivos {CONTAINER_CPP_PATHSPEC})", file=sys.stderr)
        return False, 0, 0, []

    compiled, db_ok = compiled_file_paths(compile_db_path)
    if not db_ok:
        print(f"{SCRIPT_NAME}: nao foi possivel ler '{compile_db_path}' como JSON", file=sys.stderr)
        return False, encontrados, 0, []

    missing = []
    no_banco = 0
    for relative in sorted(tracked):
        absolute = normalized_path(os.path.join(root, *relative.split("/")))
        if absolute in compiled:
            no_banco += 1
        else:
            missing.append(relative)

    return len(missing) == 0, encontrados, no_banco, missing


# --- real mode -----------------------------------------------------------


def real_main(args):
    if len(args) != 2:
        fail("usage: check_container_smokes_in_compile_db.py <repo_root> <compile_commands_json>")
    root, compile_db_path = args
    if not os.path.isdir(root):
        fail(f"repo root not found: {root}")

    ok, encontrados, no_banco, missing = check_container_smokes_in_compile_db(root, compile_db_path)
    faltando = encontrados - no_banco
    # GODS_LAWS.md L-36/L-40: esta linha imprime SEMPRE, passando ou
    # reprovando - piso de varredura nao-vazia e cobertura real, nunca
    # so' o veredito.
    print(f"{SCRIPT_NAME}: encontrados={encontrados} no_banco={no_banco} faltando={faltando}")

    if not ok:
        if missing:
            print(f"{SCRIPT_NAME}: fora do banco de compilacao (nunca analisado por run-clang-tidy):", file=sys.stderr)
            for relative in missing:
                print(relative, file=sys.stderr)
        fail("tests/container/*.cpp incompleto no banco de compilacao (ver acima)")


# --- selftest fixtures and controls ---------------------------------------


def make_scratch_workdir():
    return tempfile.mkdtemp(prefix="glintfx-container-smokes-db-")


def write_file(path, content):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as handle:
        handle.write(content)


def write_compile_db(path, entries):
    write_file(path, json.dumps(entries))


def git_init_with(root):
    subprocess.run(["git", "-C", root, "init", "-q"], check=True)
    subprocess.run(["git", "-C", root, "config", "user.email", "selftest@check-container-smokes.invalid"], check=True)
    subprocess.run(["git", "-C", root, "config", "user.name", "check_container_smokes_in_compile_db selftest"], check=True)
    subprocess.run(["git", "-C", root, "add", "-A"], check=True)
    subprocess.run(["git", "-C", root, "commit", "-q", "-m", "fixture"], check=True)


class _Captured:
    __slots__ = ("ok", "encontrados", "no_banco", "missing", "text")

    def __init__(self, ok, encontrados, no_banco, missing, text):
        self.ok = ok
        self.encontrados = encontrados
        self.no_banco = no_banco
        self.missing = missing
        self.text = text


def _capture(root, compile_db_path):
    import contextlib
    import io

    buffer = io.StringIO()
    with contextlib.redirect_stdout(buffer), contextlib.redirect_stderr(buffer):
        ok, encontrados, no_banco, missing = check_container_smokes_in_compile_db(root, compile_db_path)
    return _Captured(ok, encontrados, no_banco, missing, buffer.getvalue())


# Positive control: two .cpp tracked under tests/container/, both
# listed in compile_commands.json by full absolute path. Expected:
# pass, encontrados=2, no_banco=2.
def selftest_positive_control(scratch):
    root = os.path.join(scratch, "positive")
    write_file(os.path.join(root, "tests", "container", "a.cpp"), "// fixture\n")
    write_file(os.path.join(root, "tests", "container", "b.cpp"), "// fixture\n")
    git_init_with(root)

    compile_db = os.path.join(root, "build", "compile_commands.json")
    write_compile_db(compile_db, [
        {"directory": root, "file": os.path.join(root, "tests", "container", "a.cpp"), "command": "c++ -c a.cpp"},
        {"directory": root, "file": os.path.join(root, "tests", "container", "b.cpp"), "command": "c++ -c b.cpp"},
    ])

    result = _capture(root, compile_db)
    if not (result.ok and result.encontrados == 2 and result.no_banco == 2):
        print("selftest: controle POSITIVO FALHOU (fixture completa deveria ter sido aprovada)", file=sys.stderr)
        print(result.text, file=sys.stderr)
        return False
    print("selftest: controle POSITIVO OK (2/2 no banco)")
    return True


# Negative control: two .cpp tracked, only one in the bank. Expected:
# fail, naming the missing one (tests/container/b.cpp) explicitly.
def selftest_negative_control(scratch):
    root = os.path.join(scratch, "negative")
    write_file(os.path.join(root, "tests", "container", "a.cpp"), "// fixture\n")
    write_file(os.path.join(root, "tests", "container", "b.cpp"), "// fixture, deliberately NOT in the compile db\n")
    git_init_with(root)

    compile_db = os.path.join(root, "build", "compile_commands.json")
    write_compile_db(compile_db, [
        {"directory": root, "file": os.path.join(root, "tests", "container", "a.cpp"), "command": "c++ -c a.cpp"},
    ])

    result = _capture(root, compile_db)
    if result.ok:
        print("selftest: controle NEGATIVO FALHOU (deveria acusar tests/container/b.cpp faltando, mas passou)", file=sys.stderr)
        print(result.text, file=sys.stderr)
        return False
    if result.missing != ["tests/container/b.cpp"] or result.encontrados != 2 or result.no_banco != 1:
        print("selftest: controle NEGATIVO FALHOU (numeros ou lista de ausentes incorretos)", file=sys.stderr)
        print(f"missing={result.missing} encontrados={result.encontrados} no_banco={result.no_banco}", file=sys.stderr)
        return False
    print("selftest: controle NEGATIVO OK (acusou tests/container/b.cpp corretamente, encontrados=2 no_banco=1)")
    return True


# Empty-scan floor: zero .cpp tracked under tests/container/. Expected:
# fail with "varredura vazia" in the message (GODS_LAWS.md L-40).
def selftest_empty_scan_control(scratch):
    root = os.path.join(scratch, "empty")
    write_file(os.path.join(root, "README.md"), "# nada em tests/container/\n")
    git_init_with(root)

    compile_db = os.path.join(root, "build", "compile_commands.json")
    write_compile_db(compile_db, [])

    result = _capture(root, compile_db)
    if result.ok:
        print("selftest: controle de VARREDURA VAZIA FALHOU (deveria recusar arvore sem tests/container/*.cpp)", file=sys.stderr)
        print(result.text, file=sys.stderr)
        return False
    if "varredura vazia" not in result.text:
        print("selftest: controle de VARREDURA VAZIA FALHOU (recusou, mas nao disse 'varredura vazia')", file=sys.stderr)
        print(result.text, file=sys.stderr)
        return False
    print("selftest: controle de VARREDURA VAZIA OK (recusou arvore sem tests/container/*.cpp)")
    return True


# Leaf-name-collision control: this gate's own reason to exist over a
# basename match (header comment above) - tests/container/x.cpp is
# tracked and MISSING from the bank, but the bank DOES contain a
# DIFFERENT x.cpp living under another directory, same leaf name. A
# basename-matching engine reports this as covered; full-path matching
# must not.
def selftest_leaf_name_collision_control(scratch):
    root = os.path.join(scratch, "collision")
    write_file(os.path.join(root, "tests", "container", "x.cpp"), "// fixture, deliberately NOT the one in the bank\n")
    write_file(os.path.join(root, "src", "platform", "other", "x.cpp"), "// unrelated fixture, SAME leaf name\n")
    git_init_with(root)

    compile_db = os.path.join(root, "build", "compile_commands.json")
    write_compile_db(compile_db, [
        {"directory": root, "file": os.path.join(root, "src", "platform", "other", "x.cpp"), "command": "c++ -c x.cpp"},
    ])

    result = _capture(root, compile_db)
    if result.ok:
        print("selftest: controle de COLISAO DE NOME-FOLHA FALHOU (basename bateu com x.cpp errado, deveria ter reprovado)", file=sys.stderr)
        print(result.text, file=sys.stderr)
        return False
    if result.missing != ["tests/container/x.cpp"]:
        print("selftest: controle de COLISAO DE NOME-FOLHA FALHOU (nao acusou tests/container/x.cpp)", file=sys.stderr)
        print(f"missing={result.missing}", file=sys.stderr)
        return False
    print("selftest: controle de COLISAO DE NOME-FOLHA OK (nao confundiu com o x.cpp de outro diretorio)")
    return True


def selftest_main():
    scratch = make_scratch_workdir()
    try:
        controls = [
            selftest_positive_control(scratch),
            selftest_negative_control(scratch),
            selftest_empty_scan_control(scratch),
            selftest_leaf_name_collision_control(scratch),
        ]
        if not all(controls):
            print("check_container_smokes_in_compile_db.py --selftest: FALHOU (ver acima)", file=sys.stderr)
            sys.exit(1)
        print(f"check_container_smokes_in_compile_db.py --selftest: os {len(controls)} controles OK")
    finally:
        import shutil

        shutil.rmtree(scratch, ignore_errors=True)


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
    else:
        real_main(args)


if __name__ == "__main__":
    main()
