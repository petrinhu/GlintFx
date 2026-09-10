#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_lib_source_parity.py - TODO.md item GATE-LIB-SOURCE-PARITY
# (GODS_LAWS.md L-36/L-40), par declarado de LIB-NO-UNDEF.
#
# THE REAL DEFECT THIS CLOSES (measured 05/09/2026, run 33997698610):
# the Microsoft linker REFUSED to produce glintfx.dll/glintfx.lib
# (LNK2019/LNK1120, seat_capabilities::set_capability() unresolved).
# The root cause was a SOURCE asymmetry, not a symbol-table one: win32/
# seat_adapter.cpp (Y-1) was already wired into glintfx_library via
# win32/CMakeLists.txt's own target_sources(), and it calls
# set_capability() - but wayland/seat_adapter.cpp (S-B) was NOT wired
# into glintfx_library on the Linux side (only ever compiled a second
# time, inside tests/CMakeLists.txt's own test binaries), and
# seat_capabilities.cpp itself (the ONE shared definition both
# mechanisms need) had NO src/platform/input/CMakeLists.txt at all -
# also compiled only inside tests. Comparing what is on DISK against
# what each directory's own CMakeLists.txt hands to glintfx_library
# found exactly two files out of the whole platform layer: wayland/
# seat_adapter.cpp and input/seat_capabilities.cpp.
#
# THE DISTINCTION THAT IS THIS ITEM'S OWN REASON TO EXIST: tests/tools/
# check_no_undef_glintfx.py (LIB-NO-UNDEF) PASSED GREEN on the Linux
# side of this exact same defect, correctly - with no caller inside
# glintfx_library (wayland/seat_adapter.cpp not wired in either), there
# was no pending symbol for `nm -D` to find. LIB-NO-UNDEF audits
# SYMBOLS; this gate audits SOURCE PARITY. Neither subsumes the other:
# the defect above needed BOTH gaps closed to fully reproduce (a
# caller wired in on one side with the callee's definition missing on
# both), and it is entirely possible for a future defect to trip only
# one of the two.
#
# WHAT THIS PROVES: for every src/platform/<dir>/CMakeLists.txt, the
# set of .cpp files tracked on disk in that SAME directory equals the
# set of .cpp filenames that directory's own target_sources(
# glintfx_library ...) call hands to the library target - no source
# left compiled only inside tests, indefinitely, waiting for some
# OTHER platform's linker to notice.
#
# "NAS DUAS ARVORES DE CONFIGURACAO" (the item's own words, meaning
# this project's build/ vs build-static/ - CLAUDE.md's own "Casos de
# teste por modo (shared/estatico)"): target_sources() is PLAIN TEXT,
# read once from the CMakeLists.txt that both trees configure from -
# confirmed by grep across src/ for any $<$<CONFIG:...>>/
# BUILD_SHARED_LIBS conditional inside a target_sources() argument
# list (09/09/2026: none exist today). A single static parse therefore
# covers both trees BY CONSTRUCTION, not by assumption: if a future
# edit ever makes the source list depend on the configuration, that
# edit introduces a `$<...>` generator expression or a `${...}`
# variable into the argument list, and require_no_unresolvable_
# tokens() below refuses to silently half-parse it (GODS_LAWS.md L-40:
# a form that escapes extraction reproves, it does not pass quietly).
#
# WHAT THIS GATE DOES NOT CATCH (declared, GODS_LAWS.md L-40 applied to
# the next reader):
#   * A source that is legitimately test-only and never belongs in the
#     shipped library - it needs a form to say so (tests/
#     lib_source_exceptions.txt, one line per <dir>/<file.cpp>, a
#     reason and a TODO.md item - the SAME shape tests/parity_
#     exceptions.txt already established for check_test_parity.py, not
#     a new pattern invented here).
#   * Divergence WITHIN one directory BETWEEN THE TWO PLATFORMS (e.g.
#     wayland/ having a feature win32/ has not received yet) - that is
#     an entirely normal, expected shape of a project mid-port, and
#     comparing directory-by-directory (never wayland/'s set against
#     win32/'s set) is what keeps this gate silent about it, on
#     purpose. tests/tools/check_sibling_lists.py's own GLINTFX-
#     SIBLING-LIST marker is the tool for a list that two platforms
#     DECLARE must stay identical - this gate never infers that on its
#     own.
#
# Usage:
#   check_lib_source_parity.py --check <repo-root-directory> <exceptions.txt>
#   check_lib_source_parity.py --selftest
#
# Each function below does one thing (GODS_LAWS.md L-17).

import glob
import os
import re
import subprocess
import sys

SCRIPT_NAME = "check_lib_source_parity.py"

_TARGET_SOURCES_OPEN_RE = re.compile(r"target_sources\(\s*glintfx_library\b")
_CPP_SUFFIX = ".cpp"
_UNRESOLVABLE_NEEDLE_RE = re.compile(r"\$<|\$\{")


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


# --- CMakeLists.txt parsing --------------------------------------------


# Finds the matching ')' for the '(' that opens a target_sources(
# glintfx_library ...) call, by depth-counting parens from that '('
# onward. CMake generator expressions use '$<...>' (angle brackets),
# never parens, and this project's own filenames never contain a
# paren either (measured: `git ls-files -- '*.cpp' | grep -c '('`
# across the whole tree is 0) - a plain depth counter is therefore
# exact here, not an approximation that happens to work today.
def _find_matching_close_paren(text, open_paren_index):
    depth = 0
    for i in range(open_paren_index, len(text)):
        if text[i] == "(":
            depth += 1
        elif text[i] == ")":
            depth -= 1
            if depth == 0:
                return i
    return None


# Returns (cpp_filenames: set[str], malformed_reason: str | None). A
# non-None reason means the block contains a token this parser cannot
# statically resolve ('$<...>' generator expression, or a '${...}'
# CMake variable) - GODS_LAWS.md L-40: this is reproved by the CALLER,
# never silently treated as "zero extra sources, all good".
def parse_target_sources_cpp_files(cmake_text):
    filenames = set()
    search_from = 0
    while True:
        m = _TARGET_SOURCES_OPEN_RE.search(cmake_text, search_from)
        if not m:
            break
        open_paren_index = cmake_text.index("(", m.start())
        close_paren_index = _find_matching_close_paren(cmake_text, open_paren_index)
        if close_paren_index is None:
            return filenames, "target_sources(glintfx_library ...) sem ')' de fechamento (arquivo truncado ou malformado)"
        body = cmake_text[open_paren_index + 1 : close_paren_index]

        if _UNRESOLVABLE_NEEDLE_RE.search(body):
            return (
                filenames,
                "target_sources(glintfx_library ...) contem generator expression ($<...>) ou "
                "variavel (${...}) - forma nao resolvivel estaticamente, GODS_LAWS.md L-40",
            )

        for token in body.split():
            if token.endswith(_CPP_SUFFIX):
                filenames.add(os.path.basename(token))

        search_from = close_paren_index + 1
    return filenames, None


# --- exceptions parsing ----------------------------------------------


def _strip_pipe_fields(line, expected_fields, source_label):
    parts = [p.strip() for p in line.split("|")]
    if len(parts) != expected_fields:
        fail(
            f"{source_label}: linha malformada (esperava {expected_fields} campos separados "
            f"por '|', achou {len(parts)}): {line!r}"
        )
    return parts


# Format: <dir>/<filename.cpp>|motivo|item-TODO, one exception per
# line, '#' comments and blank lines skipped - same shape tests/
# parity_exceptions.txt already established for check_test_parity.py.
# Returns dict[dir] -> set(filenames).
def parse_exceptions_text(text, source_label="tests/lib_source_exceptions.txt"):
    by_dir = {}
    for raw_line in text.splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        dir_and_file, _motivo, _item = _strip_pipe_fields(line, 3, source_label)
        if "/" not in dir_and_file:
            fail(
                f"{source_label}: entrada sem '<dir>/<arquivo.cpp>' valido: {dir_and_file!r}"
            )
        dir_name, filename = dir_and_file.rsplit("/", 1)
        by_dir.setdefault(dir_name, set()).add(filename)
    return by_dir


# --- comparison --------------------------------------------------------


# directories: dict[dir_name] -> (disk_cpp_files: set[str],
#   declared_cpp_files: set[str], malformed_reason: str | None)
# exceptions: dict[dir_name] -> set[str]
def evaluate(directories, exceptions):
    errors = []

    if not directories:
        errors.append(
            "varredura vazia: nenhum diretorio de plataforma encontrado (nenhum "
            "src/platform/*/CMakeLists.txt) - GODS_LAWS.md L-40, sinal de coleta quebrada"
        )
        return errors

    total_disk = sum(len(disk) for disk, _declared, _malformed in directories.values())
    if total_disk == 0:
        errors.append(
            "varredura vazia: 0 arquivo(s) .cpp encontrado(s) em qualquer diretorio de "
            "plataforma - GODS_LAWS.md L-40"
        )
        return errors

    for dir_name in sorted(directories):
        disk_files, declared_files, malformed_reason = directories[dir_name]
        if malformed_reason is not None:
            errors.append(f"{dir_name}: {malformed_reason}")
            continue

        excepted = exceptions.get(dir_name, set())

        missing = sorted(disk_files - declared_files - excepted)
        if missing:
            errors.append(
                f"{dir_name}: fonte(s) no disco FORA de target_sources(glintfx_library ...): "
                f"{', '.join(missing)} - sem chamador na biblioteca, so o ligador de outra "
                "plataforma acha isso (GODS_LAWS.md L-36/L-40)"
            )

        stale_missing_from_disk = sorted(excepted - disk_files)
        if stale_missing_from_disk:
            errors.append(
                f"{dir_name}: excecao aponta para arquivo que nao existe mais no disco: "
                f"{', '.join(stale_missing_from_disk)} - apague a linha de "
                "tests/lib_source_exceptions.txt"
            )

        stale_now_delivered = sorted(excepted & declared_files)
        if stale_now_delivered:
            errors.append(
                f"{dir_name}: excecao para arquivo que ja foi entregue a biblioteca (nao "
                f"precisa mais dela): {', '.join(stale_now_delivered)} - apague a linha de "
                "tests/lib_source_exceptions.txt"
            )

    return errors


# --- real mode -------------------------------------------------------


def git_ls_files_cpp(root, rel_dir):
    proc = subprocess.run(
        ["git", "-C", root, "ls-files", "-z", "--", f"{rel_dir}/*.cpp"],
        capture_output=True,
        check=False,
    )
    if proc.returncode != 0:
        fail(
            f"'git -C {root} ls-files -- {rel_dir}/*.cpp' saiu com codigo {proc.returncode}: "
            f"{proc.stderr.decode('utf-8', errors='replace').strip()}"
        )
    raw = proc.stdout.split(b"\x00")
    return {os.path.basename(p.decode("utf-8", errors="surrogateescape")) for p in raw if p}


def _read_file(path):
    try:
        with open(path, "r", encoding="utf-8") as handle:
            return handle.read()
    except OSError as exc:
        fail(f"arquivo nao encontrado: {path} ({exc})")


def discover_platform_directories(root):
    pattern = os.path.join(root, "src", "platform", "*", "CMakeLists.txt")
    return sorted(glob.glob(pattern))


def real_main(args):
    if len(args) != 2:
        fail("usage: check_lib_source_parity.py --check <repo-root-directory> <exceptions.txt>")
    root, exceptions_path = args

    exceptions = parse_exceptions_text(_read_file(exceptions_path))

    directories = {}
    for cmake_path in discover_platform_directories(root):
        dir_abs = os.path.dirname(cmake_path)
        dir_name = os.path.basename(dir_abs)
        rel_dir = os.path.relpath(dir_abs, root)
        disk_files = git_ls_files_cpp(root, rel_dir)
        declared_files, malformed_reason = parse_target_sources_cpp_files(_read_file(cmake_path))
        directories[dir_name] = (disk_files, declared_files, malformed_reason)

    total_disk = sum(len(disk) for disk, _d, _m in directories.values())
    total_declared = sum(len(declared) for _disk, declared, _m in directories.values())
    print(
        f"{SCRIPT_NAME}: {len(directories)} diretorio(s) de plataforma escaneado(s), "
        f"{total_disk} fonte(s) .cpp no disco, {total_declared} entregue(s) a biblioteca, "
        f"{sum(len(v) for v in exceptions.values())} excecao(oes)"
    )

    errors = evaluate(directories, exceptions)
    if errors:
        print(f"{SCRIPT_NAME}: REPROVADO ({len(errors)} problema(s)):", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        sys.exit(1)

    print(f"{SCRIPT_NAME}: paridade OK - toda fonte de plataforma no disco entra na biblioteca")


# --- fixtures and controls for --selftest -----------------------------


def _dirs(**kwargs):
    """kwargs: dir_name=(disk_set, declared_set, malformed_reason)."""
    return dict(kwargs)


# Positive control: disk and declared sets match exactly across two
# directories. Expected: passes, zero errors.
def selftest_positive_control():
    directories = _dirs(
        wayland=({"a.cpp", "b.cpp"}, {"a.cpp", "b.cpp"}, None),
        win32=({"c.cpp"}, {"c.cpp"}, None),
    )
    errors = evaluate(directories, {})
    if errors:
        print(f"selftest: controle POSITIVO FALHOU (esperava zero erros, veio {errors})", file=sys.stderr)
        return False
    print("selftest: controle POSITIVO OK (disco e biblioteca batem em dois diretorios)")
    return True


# VERMELHO #1 - a forma exata do defeito real: seat_adapter.cpp on
# disk in wayland/, never entered into target_sources().
def selftest_missing_from_target_sources_reproves():
    directories = _dirs(
        wayland=({"display_adapter.cpp", "seat_adapter.cpp"}, {"display_adapter.cpp"}, None),
    )
    errors = evaluate(directories, {})
    if not errors:
        print("selftest: VERMELHO#1 FALHOU (fonte fora da biblioteca deveria ter reprovado)", file=sys.stderr)
        return False
    if not any("seat_adapter.cpp" in e for e in errors):
        print(f"selftest: VERMELHO#1 FALHOU (nao citou seat_adapter.cpp): {errors}", file=sys.stderr)
        return False
    print(f"selftest: VERMELHO#1 OK (fonte orfa pega): {errors}")
    return True


# VERMELHO #2 - a segunda metade do defeito real: um diretorio inteiro
# (input/, seat_capabilities.cpp) sem NENHUMA entrega - CMakeLists.txt
# existe mas com um target_sources() para outro alvo, ou nenhum.
def selftest_directory_with_zero_declared_reproves():
    directories = _dirs(
        input=({"seat_capabilities.cpp"}, set(), None),
    )
    errors = evaluate(directories, {})
    if not errors:
        print("selftest: VERMELHO#2 FALHOU (diretorio sem entrega alguma deveria ter reprovado)", file=sys.stderr)
        return False
    if not any("seat_capabilities.cpp" in e for e in errors):
        print(f"selftest: VERMELHO#2 FALHOU (nao citou seat_capabilities.cpp): {errors}", file=sys.stderr)
        return False
    print(f"selftest: VERMELHO#2 OK (diretorio sem entrega pego): {errors}")
    return True


# Controle: excecao legitima (fonte so-de-teste, declarada) nunca
# reprova, mesmo fora de target_sources().
def selftest_exception_suppresses_missing():
    directories = _dirs(
        wayland=({"display_adapter.cpp", "test_only_probe.cpp"}, {"display_adapter.cpp"}, None),
    )
    exceptions = {"wayland": {"test_only_probe.cpp"}}
    errors = evaluate(directories, exceptions)
    if errors:
        print(f"selftest: controle EXCECAO FALHOU (deveria ter passado): {errors}", file=sys.stderr)
        return False
    print("selftest: controle EXCECAO OK (fonte so-de-teste declarada nao reprova)")
    return True


# Controle: excecao APONTANDO para arquivo que nao existe mais no disco
# (higiene - mesma familia do "excecao para item concluido" de
# check_test_parity.py) reprova, mesmo sem nenhuma fonte orfa real.
def selftest_stale_exception_missing_from_disk_reproves():
    directories = _dirs(
        wayland=({"display_adapter.cpp"}, {"display_adapter.cpp"}, None),
    )
    exceptions = {"wayland": {"long_gone.cpp"}}
    errors = evaluate(directories, exceptions)
    if not errors:
        print("selftest: controle EXCECAO-OBSOLETA(disco) FALHOU (deveria ter reprovado)", file=sys.stderr)
        return False
    if not any("long_gone.cpp" in e for e in errors):
        print(f"selftest: controle EXCECAO-OBSOLETA(disco) FALHOU (nao citou o arquivo): {errors}", file=sys.stderr)
        return False
    print(f"selftest: controle EXCECAO-OBSOLETA(disco) OK: {errors}")
    return True


# Controle: excecao para arquivo que JA foi entregue a biblioteca (a
# excecao virou desnecessaria) reprova - a mesma disciplina de nao
# deixar excecao morta acumular.
def selftest_stale_exception_now_delivered_reproves():
    directories = _dirs(
        wayland=({"display_adapter.cpp"}, {"display_adapter.cpp"}, None),
    )
    exceptions = {"wayland": {"display_adapter.cpp"}}
    errors = evaluate(directories, exceptions)
    if not errors:
        print("selftest: controle EXCECAO-OBSOLETA(entregue) FALHOU (deveria ter reprovado)", file=sys.stderr)
        return False
    if not any("display_adapter.cpp" in e for e in errors):
        print(f"selftest: controle EXCECAO-OBSOLETA(entregue) FALHOU: {errors}", file=sys.stderr)
        return False
    print(f"selftest: controle EXCECAO-OBSOLETA(entregue) OK: {errors}")
    return True


# VERMELHO #3 (piso de varredura vazia, GODS_LAWS.md L-40): nenhum
# diretorio de plataforma encontrado.
def selftest_top_level_empty_scan_reproves():
    errors = evaluate({}, {})
    if not errors:
        print("selftest: VERMELHO#3 FALHOU (zero diretorios deveria ter reprovado)", file=sys.stderr)
        return False
    if not any("varredura vazia" in e for e in errors):
        print(f"selftest: VERMELHO#3 FALHOU (sem 'varredura vazia'): {errors}", file=sys.stderr)
        return False
    print(f"selftest: VERMELHO#3 OK: {errors}")
    return True


# VERMELHO #4 - malformed: um target_sources(glintfx_library ...)
# carrega um generator expression - nao pode virar "zero fontes extra,
# tudo bem" silencioso.
def selftest_unresolvable_generator_expression_reproves():
    # win32/ carries a normal, healthy pair (disk == declared) so
    # total_disk > 0 and the top-level empty-scan check (VERMELHO#3,
    # above) does NOT short-circuit this control - the assertion below
    # must land on the PER-DIRECTORY malformed branch, not on "varredura
    # vazia", or this control would silently test the wrong code path.
    directories = _dirs(
        wayland=(set(), set(), "target_sources(...) contem generator expression"),
        win32=({"display_adapter.cpp"}, {"display_adapter.cpp"}, None),
    )
    errors = evaluate(directories, {})
    if not errors:
        print("selftest: VERMELHO#4 FALHOU (forma nao resolvivel deveria ter reprovado)", file=sys.stderr)
        return False
    if not any("generator expression" in e for e in errors):
        print(f"selftest: VERMELHO#4 FALHOU (nao citou o motivo malformed): {errors}", file=sys.stderr)
        return False
    if any("varredura vazia" in e for e in errors):
        print(f"selftest: VERMELHO#4 FALHOU (caiu no piso de topo em vez do malformed por diretorio): {errors}", file=sys.stderr)
        return False
    print(f"selftest: VERMELHO#4 OK: {errors}")
    return True


# Prova de parsing real: target_sources(glintfx_library PRIVATE ...)
# multi-linha, com comentarios intercalados e um SEGUNDO target_
# sources() para OUTRO alvo no mesmo arquivo (nunca deve contaminar a
# extracao) - forma real deste projeto (src/platform/wayland/
# CMakeLists.txt tambem chama glintfx_add_wayland_xdg_shell_binding()
# e glintfx_add_egl(), nunca target_sources() de outro alvo, mas o
# parser precisa ignorar um alvo estranho se algum dia aparecer).
def selftest_parsing_multiline_and_other_target():
    cmake_text = (
        "# comentario\n"
        "some_other_macro(glintfx_library)\n"
        "target_sources(other_target PRIVATE\n"
        "    nao_deveria_entrar.cpp\n"
        ")\n"
        "target_sources(glintfx_library PRIVATE\n"
        "    display_adapter.cpp\n"
        "    # comentario no meio\n"
        "    seat_adapter.cpp\n"
        ")\n"
    )
    files, malformed = parse_target_sources_cpp_files(cmake_text)
    if malformed is not None:
        print(f"selftest: PARSING FALHOU (malformed inesperado): {malformed}", file=sys.stderr)
        return False
    expected = {"display_adapter.cpp", "seat_adapter.cpp"}
    if files != expected:
        print(f"selftest: PARSING FALHOU (esperava {expected}, veio {files})", file=sys.stderr)
        return False
    print("selftest: PARSING OK (multi-linha, comentario no meio, outro alvo ignorado)")
    return True


# Prova de parsing: generator expression dentro do bloco e' detectada
# e vira malformed_reason, nunca um conjunto vazio silencioso.
def selftest_parsing_detects_generator_expression():
    cmake_text = (
        "target_sources(glintfx_library PRIVATE\n"
        "    display_adapter.cpp\n"
        "    $<$<CONFIG:Debug>:debug_only.cpp>\n"
        ")\n"
    )
    files, malformed = parse_target_sources_cpp_files(cmake_text)
    if malformed is None:
        print(f"selftest: PARSING(generator) FALHOU (deveria ter marcado malformed, files={files})", file=sys.stderr)
        return False
    print(f"selftest: PARSING(generator) OK: {malformed}")
    return True


# Prova de parsing das excecoes: round-trip de texto real.
def selftest_exceptions_parsing_round_trip():
    text = (
        "# comentario\n"
        "\n"
        "wayland/test_only_probe.cpp|so entra em teste, nunca na biblioteca|ITEM-X\n"
    )
    parsed = parse_exceptions_text(text)
    if parsed != {"wayland": {"test_only_probe.cpp"}}:
        print(f"selftest: PARSING(excecoes) FALHOU: {parsed}", file=sys.stderr)
        return False
    print("selftest: PARSING(excecoes) OK")
    return True


def selftest_main():
    controls = [
        selftest_positive_control(),
        selftest_missing_from_target_sources_reproves(),
        selftest_directory_with_zero_declared_reproves(),
        selftest_exception_suppresses_missing(),
        selftest_stale_exception_missing_from_disk_reproves(),
        selftest_stale_exception_now_delivered_reproves(),
        selftest_top_level_empty_scan_reproves(),
        selftest_unresolvable_generator_expression_reproves(),
        selftest_parsing_multiline_and_other_target(),
        selftest_parsing_detects_generator_expression(),
        selftest_exceptions_parsing_round_trip(),
    ]
    if not all(controls):
        print(f"{SCRIPT_NAME} --selftest: FALHOU (ver acima)", file=sys.stderr)
        sys.exit(1)
    print(f"{SCRIPT_NAME} --selftest: os {len(controls)} controles OK")


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
    elif args and args[0] == "--check":
        real_main(args[1:])
    else:
        fail(
            "usage: check_lib_source_parity.py --check <repo-root-directory> "
            "<exceptions.txt>  |  --selftest"
        )


if __name__ == "__main__":
    main()
