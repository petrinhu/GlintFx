#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_draw2d_include_direction.py - R2D-IMAGE-RESOLVER R2a (CONTRACT.md 5.3): the gate that draw2d never reaches
# glintfx/gfss, neither through a direct #include nor through a header of core or of any other module.
#
# MECHANISM. The dependency closure of every object is the one the COMPILER wrote during the real build. It is read
# from the Ninja deps log by ONE process (`ninja -t deps`). The gate never runs a compiler: one compiler process per
# file would cost one process per item swept (GODS_LAWS.md global L-11, item 6; project L-45). Ninja stores the gcc
# depfile or the msvc /showIncludes output with the same flags, defines and include directories as the product, so
# the closure is the product's own. The format of `ninja -t deps` is not documented; the parser fails closed on any
# line it does not recognize. The gate requires the Ninja generator, the only one that writes that log.
#
# COVERAGE, closed by construction. Files are enumerated from the disk, not from git, so an untracked new file is
# seen. .cpp is a source; .hpp .h .hh .hxx .inl .ipp .inc are headers; CMakeLists.txt is ignored by name; any other
# extension is refused by name. Every source needs at least one object, every object needs a VALID closure with at
# least one dependency, and every header must sit in the union of the closures, so a header that no object reaches is
# refused, never skipped. The probe (a generated translation unit that includes every public header of draw2d) is one
# more root, for the public headers that no .cpp of draw2d includes.
#
# WHAT IT DOES NOT SEE. A branch of #if that this build did not take. The CI matrix covers the branches of each system.
#
# Standard library only (L-07). Identifiers and comments in English (L-21); messages to the user in pt-br, without
# accents, as check_examples_consume.py does.
#
# USAGE: check_draw2d_include_direction.py --check --source <root> --build <build dir> --ninja <ninja>
#            --generator <generator> --probe <generated probe .cpp>
#        check_draw2d_include_direction.py --selftest

import argparse
import contextlib
import io
import json
import os
import posixpath
import re
import subprocess
import sys
import tempfile
from collections import namedtuple
from selftest_controls import CHECKS, check

SCRIPT_NAME = "check_draw2d_include_direction"
INTERNAL_DIR = "src/draw2d"
PUBLIC_DIR = "include/glintfx/draw2d"
SOURCE_EXTENSIONS = (".cpp",)
HEADER_EXTENSIONS = (".hpp", ".h", ".hh", ".hxx", ".inl", ".ipp", ".inc")
IGNORED_NAMES = ("CMakeLists.txt",)
ALLOWED_GENERATORS = ("Ninja", "Ninja Multi-Config")
PROBE_LABEL = "SONDA"
DEPS_HEADER = re.compile(r"^(\S.*): #deps (\d+), deps mtime (-?\d+) \((VALID|STALE)\)$")
DEPS_ITEM = re.compile(r"^    (\S.*)$")
DRIVE_PREFIX = re.compile(r"^[A-Za-z]:/")
Report = namedtuple("Report", "generator sources objects probes headers dependencies violations problems")
Case = namedtuple("Case", "tree files text entries generator")


# ---- the judges ----

def normalize_path(path, base):
    """The comparable form of a path: forward slashes, joined to base when relative, normalized, casefolded."""
    unified = path.replace("\\", "/")
    if not unified.startswith("/") and not DRIVE_PREFIX.match(unified):
        unified = posixpath.join(base, unified)
    return posixpath.normpath(unified).casefold()


def display(path, root):
    """The path relative to root, for the report; the path itself when it lies outside root."""
    prefix = normalize_path(root, "/") + "/"
    return path[len(prefix):] if path.startswith(prefix) else path


def list_draw2d_files(source):
    """Every file under the two draw2d folders, recursive, read from the disk, as paths relative to source."""
    found = []
    for folder in (INTERNAL_DIR, PUBLIC_DIR):
        for current, _folders, names in os.walk(os.path.join(source, folder)):
            for name in names:
                full = os.path.join(current, name)
                found.append(os.path.relpath(full, source).replace(os.sep, "/"))
    return sorted(found)


def classify_file(rel):
    """The class of one file by the closed table: source, header, ignored by name, or unknown (refused)."""
    name = posixpath.basename(rel)
    extension = posixpath.splitext(name)[1]
    if name in IGNORED_NAMES:
        return "ignored"
    if extension in SOURCE_EXTENSIONS:
        return "source"
    if extension in HEADER_EXTENSIONS:
        return "header"
    return "unknown"


def classify_draw2d_files(source):
    """Sort every draw2d file into (sources, headers, unknown), as relative paths in order."""
    sources, headers, unknown = [], [], []
    for rel in list_draw2d_files(source):
        kind = classify_file(rel)
        if kind == "source":
            sources.append(rel)
        elif kind == "header":
            headers.append(rel)
        elif kind == "unknown":
            unknown.append(rel)
    return sources, headers, unknown


def read_compile_commands(build):
    """The entries of compile_commands.json; a missing or invalid file is a named problem."""
    path = os.path.join(build, "compile_commands.json")
    try:
        with open(path, encoding="utf-8") as handle:
            entries = json.load(handle)
    except (OSError, ValueError) as error:
        return [], f"compile_commands.json ausente ou invalido em {build}: {error}"
    if not isinstance(entries, list):
        return [], f"compile_commands.json nao e uma lista em {build}"
    return entries, None


def objects_by_source(entries):
    """Map each normalized source to its (normalized output, output as written) pairs. An entry without file or
    output is a named problem."""
    mapping, problems = {}, []
    for entry in entries:
        if not isinstance(entry, dict):
            problems.append("entrada do compile_commands nao e um objeto JSON")
            continue
        directory = entry.get("directory", "")
        source_name, output = entry.get("file"), entry.get("output")
        if not source_name or not output:
            problems.append(f"entrada do compile_commands sem output ou file: {source_name or '(sem file)'}")
            continue
        key = normalize_path(source_name, directory)
        mapping.setdefault(key, []).append((normalize_path(output, directory), output))
    return mapping, problems


def run_process(argv):
    """Run one command; returns (rc, stdout, stderr). A command that cannot start is rc 127."""
    try:
        done = subprocess.run(argv, capture_output=True, text=True, check=False)
    except OSError as error:
        return 127, "", str(error)
    return done.returncode, done.stdout, done.stderr


def run_ninja_deps(ninja, build, run):
    """ONE process: `ninja -C <build> -t deps`. Returns (text, problem); a non-zero exit is a named problem."""
    code, text, error = run([ninja, "-C", build, "-t", "deps"])
    if code != 0:
        first_line = (error.strip().splitlines() or ["sem saida de erro"])[0]
        return "", f"ninja -t deps falhou (rc={code}): {first_line}"
    return text, None


def open_target(match, build):
    """A new target line of the log, with the count it declares and no dependency read yet."""
    return {"target": normalize_path(match.group(1), build), "count": int(match.group(2)),
            "state": match.group(4), "deps": []}


def close_target(current, table, problems):
    """Store a finished target. A count that does not match the lines read, or a repeated target, is a problem."""
    if current is None:
        return None
    if len(current["deps"]) != current["count"]:
        problems.append(f"linha fora do formato do ninja: #deps {current['count']} de {current['target']} "
                        f"nao bate com {len(current['deps'])} linha(s) lida(s)")
    if current["target"] in table:
        problems.append(f"alvo repetido no log de dependencias: {current['target']}")
    table[current["target"]] = (current["state"], current["deps"])
    return None


def parse_ninja_deps(text, build):
    """Read `ninja -t deps` into {normalized target: (state, [normalized dependencies])}. A line outside the format
    is a named problem: the parser fails closed."""
    table, problems, current = {}, [], None
    for line in text.splitlines():
        header = DEPS_HEADER.match(line)
        item = DEPS_ITEM.match(line)
        if header:
            current = close_target(current, table, problems)
            current = open_target(header, build)
        elif item and current is not None:
            current["deps"].append(normalize_path(item.group(1), build))
        elif line == "":
            current = close_target(current, table, problems)
        else:
            problems.append(f"linha fora do formato do ninja: {line}")
    close_target(current, table, problems)
    return table, problems


def is_forbidden(dep, source):
    """True when a dependency reaches gfss: the public folder, installed or generated, or the internal src/gfss/."""
    path = normalize_path(dep, "/")
    internal = normalize_path(source + "/src/gfss", "/") + "/"
    return "/glintfx/gfss/" in path or path.startswith(internal)


def check_generator(generator):
    """Only Ninja writes the log this gate reads; any other generator would be green without looking."""
    if generator in ALLOWED_GENERATORS:
        return []
    return [f"o portao exige o gerador Ninja; este build usa {generator}"]


def judge_object(output, label, table, problems):
    """The closure of one object. It must be in the log, VALID, with at least one dependency; otherwise a problem
    is named and nothing is counted from it."""
    if output not in table:
        problems.append(f"objeto sem entrada no log de dependencias: {label}")
        return []
    state, deps = table[output]
    if state != "VALID":
        problems.append(f"entrada {state} no log de dependencias: {label}")
        return []
    if not deps:
        problems.append(f"entrada com zero dependencias no log de dependencias: {label}")
        return []
    return deps


def violations_of(source_label, object_label, deps, source):
    """The (source, object, dependency) triples whose dependency reaches gfss."""
    return [(source_label, object_label, dep) for dep in deps if is_forbidden(dep, source)]


def judge_objects(sources, by_source, table, source):
    """Judge every object compiled from a draw2d source. Returns (violations, problems, union, object count)."""
    violations, problems, union, count = [], [], set(), 0
    for src in sources:
        objects = by_source.get(src, [])
        if not objects:
            problems.append(f"fonte de draw2d sem objeto no compile_commands: {display(src, source)}")
        for output, label in objects:
            count += 1
            deps = judge_object(output, label, table, problems)
            union.update(deps)
            violations += violations_of(display(src, source), label, deps, source)
    return violations, problems, union, count


def judge_probe(probe_objects, table, source):
    """The probe is one more root. It must be in the compile database, each of its objects must judge clean, and its
    closure joins the union. Returns (violations, problems, closure dependencies)."""
    if not probe_objects:
        return [], ["sonda ausente do compile_commands: a unidade gerada nao foi compilada"], []
    violations, problems, deps_all = [], [], []
    for output, label in probe_objects:
        deps = judge_object(output, label, table, problems)
        deps_all += deps
        violations += violations_of(PROBE_LABEL, label, deps, source)
    return violations, problems, deps_all


def judge_headers(headers, union, source):
    """Every header of draw2d, public and internal, must sit in the closure of some object; otherwise it was never
    judged, and the gate says so by name."""
    problems = []
    for rel in headers:
        if normalize_path(rel, source) not in union:
            problems.append(f"cabecalho de draw2d fora do fecho de todo objeto de draw2d: {rel} "
                            "(inclua-o no .cpp que o implementa)")
    return problems


def scan_disk(source):
    """The disk side: the closed classification of draw2d, and the problems it finds (unknown extension, empty
    sweep). The sweep counts what it saw, and zero is a refusal (L-40)."""
    sources, headers, unknown = classify_draw2d_files(source)
    problems = [f"extensao fora da lista, arquivo nao julgavel: {rel}" for rel in unknown]
    if not sources and not headers:
        problems.append(f"varredura vazia, nenhum arquivo de draw2d em {source}")
    return sources, headers, problems


def read_log(args, run):
    """The compiler side: the compile database and the closures of `ninja -t deps`, read by one process."""
    entries, entries_problem = read_compile_commands(args.build)
    problems = [entries_problem] if entries_problem else []
    text, problem = run_ninja_deps(args.ninja, args.build, run)
    if problem:
        problems.append(problem)
    table, parse_problems = parse_ninja_deps(text, args.build)
    return entries, table, problems + parse_problems


def check_tree(args, run):
    """Join the judges: the disk is classified, the compiler's log is read once, and every object, the probe and
    every header are judged. Returns the Report."""
    sources, headers, problems = scan_disk(args.source)
    problems = check_generator(args.generator) + problems
    entries, table, log_problems = read_log(args, run)
    by_source, mapping_problems = objects_by_source(entries)
    source_norm = [normalize_path(rel, args.source) for rel in sources]
    violations, object_problems, union, object_count = judge_objects(source_norm, by_source, table, args.source)
    probe_objects = by_source.get(normalize_path(args.probe, "/"), [])
    probe_violations, probe_problems, probe_deps = judge_probe(probe_objects, table, args.source)
    union.update(probe_deps)
    header_problems = judge_headers(headers, union, args.source)
    problems += log_problems + mapping_problems + object_problems + probe_problems + header_problems
    return Report(args.generator, sources, object_count, len(probe_objects), headers, len(union),
                  violations + probe_violations, problems)


def print_report(report):
    """The count line always goes to stdout; each violation and each problem goes to stderr."""
    analyzed = len(report.sources) + len(report.headers)
    print(f"{SCRIPT_NAME}: gerador={report.generator} fontes={len(report.sources)} objetos={report.objects} "
          f"sonda={report.probes} cabecalhos={len(report.headers)} dependencias={report.dependencies} "
          f"analisados={analyzed} violacoes={len(report.violations)}")
    for source_label, object_label, dep in report.violations:
        print(f"{SCRIPT_NAME}: VIOLACAO (CONTRACT.md 5.3): {source_label} pelo objeto {object_label} alcanca {dep}",
              file=sys.stderr)
    for problem in report.problems:
        print(f"{SCRIPT_NAME}: FALHOU: {problem}", file=sys.stderr)


def verdict(report):
    """Exit 0 only with no problem, no violation, and at least one analyzed file (L-40)."""
    analyzed = len(report.sources) + len(report.headers)
    return 0 if not report.problems and not report.violations and analyzed > 0 else 1
FIXTURE_TEXT = "// fixture\n"
FIXTURE_FILES = ("src/draw2d/a.cpp", "src/draw2d/a.hpp", "include/glintfx/draw2d/pub.hpp", "src/draw2d/CMakeLists.txt")
SOURCE_OUTPUT = "src/CMakeFiles/glintfx.dir/draw2d/a.cpp.o"
PROBE_OUTPUT = "tests/CMakeFiles/draw2d_include_direction_probe.dir/draw2d_include_direction_probe.cpp.o"


def selftest_directions():
    """Controls 1 to 6: the clean tree passes, each shape of gfss reaching draw2d is refused, look-alikes pass."""
    rc, out, _ = in_fixture(base_case)
    check("positivo: arvore limpa passa com violacoes=0", rc == 0 and "analisados=3 violacoes=0" in out)
    rc, _, err = in_fixture(source_reaches(lambda root: [f"{root}/include/glintfx/gfss/token.hpp"]))
    check("direto: cabecalho de gfss no fecho de um objeto reprova",
          rc == 1 and "VIOLACAO (CONTRACT.md 5.3): src/draw2d/a.cpp pelo objeto" in err and "gfss/token.hpp" in err)
    rc, _, err = in_fixture(source_reaches(lambda root: [f"{root}/include/glintfx/core/rect.hpp",
                                                        f"{root}/include/glintfx/gfss/token.hpp"]))
    check("indireto: core/rect.hpp e depois gfss/token.hpp no fecho reprova",
          rc == 1 and "VIOLACAO (CONTRACT.md 5.3): src/draw2d/a.cpp" in err)
    rc, _, err = in_fixture(source_reaches(lambda root: [f"{root}/src/gfss/image_value_read.hpp"]))
    check("interno: src/gfss no fecho reprova", rc == 1 and "VIOLACAO (CONTRACT.md 5.3): src/draw2d/a.cpp" in err)
    windows_dep = normalize_path(r"D:\A\Include\GlintFX\GFSS\token.hpp", "/")
    table, problems = parse_ninja_deps(r"src\CMakeFiles\glintfx.dir\draw2d\a.cpp.obj: #deps 0, deps mtime 1 (VALID)"
                                       "\n\n", "D:/A/build")
    key = normalize_path("src/CMakeFiles/glintfx.dir/draw2d/a.cpp.obj", "D:/A/build")
    check("windows: barra invertida, letra de unidade e caixa alta reprovam",
          is_forbidden(windows_dep, "D:/A") and key in table and not problems)
    check("vizinho: glintfx/gfss_extra e glintfx/gfssx nao sao gfss",
          not is_forbidden("/x/include/glintfx/gfss_extra/token.hpp", "/x")
          and not is_forbidden("/x/include/glintfx/gfssx/token.hpp", "/x"))


def selftest_objects():
    """Controls 7 to 11: a source without object, an entry without output, and the three bad object states."""
    rc, _, err = in_fixture(lambda root: base_case(root)._replace(entries=[probe_entry(root)]))
    check("fonte de draw2d sem objeto reprova",
          rc == 1 and "fonte de draw2d sem objeto no compile_commands: src/draw2d/a.cpp" in err)
    rc, _, err = in_fixture(without_output)
    check("entrada do compile_commands sem output reprova", rc == 1 and "sem output" in err)
    rc, _, err = in_fixture(lambda root: base_case(root)._replace(
        text=deps_block(PROBE_OUTPUT, "VALID", pub_deps(root))))
    check("objeto sem entrada no log de dependencias reprova",
          rc == 1 and f"objeto sem entrada no log de dependencias: {SOURCE_OUTPUT}" in err)
    rc, _, err = in_fixture(lambda root: base_case(root)._replace(
        text=deps_block(SOURCE_OUTPUT, "STALE", src_deps(root)) + deps_block(PROBE_OUTPUT, "VALID", pub_deps(root))))
    check("entrada STALE reprova", rc == 1 and f"entrada STALE no log de dependencias: {SOURCE_OUTPUT}" in err)
    rc, _, err = in_fixture(lambda root: base_case(root)._replace(
        text=deps_block(SOURCE_OUTPUT, "VALID", []) + deps_block(PROBE_OUTPUT, "VALID", pub_deps(root))))
    check("entrada com zero dependencias reprova", rc == 1 and "entrada com zero dependencias" in err)


def selftest_headers_scan():
    """Controls 12 to 14: a header outside every closure, an empty sweep, and an extension outside the closed list."""
    rc, _, err = in_fixture(lambda root: base_case(root)._replace(
        text=canned_text(src_deps(root), [f"{root}/src/draw2d/a.cpp"])))
    check("cabecalho de draw2d fora do fecho reprova",
          rc == 1 and "include/glintfx/draw2d/pub.hpp (inclua-o no .cpp que o implementa)" in err)
    rc, out, err = in_fixture(lambda root: base_case(root)._replace(tree=False, text="", entries=[]))
    check("varredura vazia reprova", rc == 1 and "analisados=0" in out and "varredura vazia" in err)
    rc, _, err = in_fixture(lambda root: base_case(root)._replace(files=("src/draw2d/notas.txt",)))
    check("extensao fora da lista reprova",
          rc == 1 and "extensao fora da lista, arquivo nao julgavel: src/draw2d/notas.txt" in err)


def selftest_format_generator_probe():
    """Controls 15 to 17: the format of the log, the generator name, and the probe as a root."""
    rc_a, _, err_a = in_fixture(lambda root: base_case(root)._replace(
        text="sem cabecalho nem recuo\n" + canned_text(src_deps(root), pub_deps(root))))
    rc_b, _, err_b = in_fixture(short_count_case)
    check("linha fora do formato do ninja reprova",
          rc_a == 1 and "linha fora do formato do ninja: sem cabecalho" in err_a
          and rc_b == 1 and "linha fora do formato do ninja: #deps 2" in err_b)
    rc, _, err = in_fixture(lambda root: base_case(root)._replace(generator="Unix Makefiles"))
    check("gerador que nao e Ninja reprova nomeando o gerador",
          rc == 1 and "o portao exige o gerador Ninja; este build usa Unix Makefiles" in err)
    rc_c, _, err_c = in_fixture(lambda root: base_case(root)._replace(entries=[source_entry(root)]))
    rc_d, _, err_d = in_fixture(probe_reaches(lambda root: [f"{root}/include/glintfx/gfss/token.hpp"]))
    check("sonda sem objeto ou com gfss reprova",
          rc_c == 1 and "sonda ausente do compile_commands" in err_c
          and rc_d == 1 and f"VIOLACAO (CONTRACT.md 5.3): {PROBE_LABEL} pelo objeto" in err_d)


def run_selftest():
    """Run the four groups of controls; the count line is the one the ctest registration checks."""
    selftest_directions()
    selftest_objects()
    selftest_headers_scan()
    selftest_format_generator_probe()
    print(f"selftest: {len(CHECKS)} controles OK")
    return 0


# ---- fixture builders (selftest only) ----

def write_text(path, text):
    """Write a fixture file, creating its folders."""
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as handle:
        handle.write(text)


def build_of(root):
    """The build directory of a fixture tree."""
    return os.path.join(root, "build")


def probe_path(build):
    """The generated probe translation unit, as the CMake registration names it."""
    return os.path.join(build, "tests", "draw2d_include_direction_probe.cpp")


def source_entry(root):
    """The compile_commands entry of a.cpp, with its object."""
    return {"directory": build_of(root), "file": f"{root}/src/draw2d/a.cpp", "output": SOURCE_OUTPUT}


def probe_entry(root):
    """The compile_commands entry of the probe, with its object."""
    build = build_of(root)
    return {"directory": build, "file": probe_path(build), "output": PROBE_OUTPUT}


def src_deps(root, *extra):
    """The closure of a.cpp: the source itself, a.hpp, and any extra dependency."""
    return [f"{root}/src/draw2d/a.cpp", f"{root}/src/draw2d/a.hpp", *extra]


def pub_deps(root, *extra):
    """The closure of the probe: pub.hpp, and any extra dependency."""
    return [f"{root}/include/glintfx/draw2d/pub.hpp", *extra]


def deps_block(output, state, deps):
    """One target in the real format of `ninja -t deps`: header line, 4-space dependency lines, blank line."""
    lines = [f"{output}: #deps {len(deps)}, deps mtime 1 ({state})"]
    lines += [f"    {dep}" for dep in deps]
    return "\n".join(lines) + "\n\n"


def canned_text(source_deps, probe_deps):
    """The canned `ninja -t deps` text: the closure of a.cpp and the closure of the probe, both VALID."""
    return deps_block(SOURCE_OUTPUT, "VALID", source_deps) + deps_block(PROBE_OUTPUT, "VALID", probe_deps)


def base_case(root):
    """The clean fixture: a tree, one source object and the probe, both VALID, over clean headers."""
    return Case(tree=True, files=(), text=canned_text(src_deps(root), pub_deps(root)),
                entries=[source_entry(root), probe_entry(root)], generator="Ninja")


def source_reaches(make_extra):
    """A clean fixture whose a.cpp object also reaches the dependencies that make_extra(root) lists."""
    return lambda root: base_case(root)._replace(text=canned_text(src_deps(root, *make_extra(root)), pub_deps(root)))


def probe_reaches(make_extra):
    """A clean fixture whose probe also reaches the dependencies that make_extra(root) lists."""
    return lambda root: base_case(root)._replace(text=canned_text(src_deps(root), pub_deps(root, *make_extra(root))))


def without_output(root):
    """The clean fixture plus an entry for a.cpp that has no output."""
    broken = {"directory": build_of(root), "file": f"{root}/src/draw2d/a.cpp"}
    return base_case(root)._replace(entries=[source_entry(root), probe_entry(root), broken])


def short_count_case(root):
    """a.cpp declares two dependencies in the log and writes only one line of them."""
    short = f"{SOURCE_OUTPUT}: #deps 2, deps mtime 1 (VALID)\n    {root}/src/draw2d/a.cpp\n\n"
    return base_case(root)._replace(text=short + deps_block(PROBE_OUTPUT, "VALID", pub_deps(root)))


def canned_run(text):
    """Stands in for the one process of ninja: answers rc 0 with the canned text, and runs nothing."""
    def run(argv):
        return 0, text, ""
    return run


def run_case(case, root):
    """Materialize the fixture under root, judge it on the canned text, and return (rc, stdout, stderr)."""
    build = build_of(root)
    if case.tree:
        for rel in FIXTURE_FILES + case.files:
            write_text(os.path.join(root, rel), FIXTURE_TEXT)
    write_text(os.path.join(build, "compile_commands.json"), json.dumps(case.entries))
    args = argparse.Namespace(source=root, build=build, ninja="ninja", generator=case.generator,
                              probe=probe_path(build))
    report = check_tree(args, canned_run(case.text))
    return (verdict(report),) + render(report)


def in_fixture(make_case):
    """Run make_case(root) in a fresh temporary tree, so no file of one control leaks into the next."""
    with tempfile.TemporaryDirectory() as root:
        return run_case(make_case(root), root)


def render(report):
    """What print_report would write, captured as (stdout, stderr)."""
    out, err = io.StringIO(), io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
        print_report(report)
    return out.getvalue(), err.getvalue()


# ---- command line ----

def build_parser():
    """The command line: exactly one mode, --check or --selftest."""
    parser = argparse.ArgumentParser(description="Portao de direcao draw2d -> gfss pelo fecho do compilador.")
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--check", action="store_true", help="julga o build")
    mode.add_argument("--selftest", action="store_true", help="roda os controles do autoteste")
    parser.add_argument("--source", help="raiz da arvore-fonte")
    parser.add_argument("--build", help="diretorio de build (compile_commands.json e .ninja_deps)")
    parser.add_argument("--ninja", help="caminho do ninja")
    parser.add_argument("--generator", help="gerador do CMake (tem de ser Ninja)")
    parser.add_argument("--probe", help="caminho do .cpp gerado da sonda")
    return parser


def main(argv):
    """Exit 0 only when the gate is green; argparse exits 2 on bad usage."""
    parser = build_parser()
    args = parser.parse_args(argv)
    if args.selftest:
        return run_selftest()
    missing = [name for name in ("source", "build", "ninja", "generator", "probe") if not getattr(args, name)]
    if missing:
        parser.error("--check exige " + ", ".join("--" + name for name in missing))
    report = check_tree(args, run_process)
    print_report(report)
    return verdict(report)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
