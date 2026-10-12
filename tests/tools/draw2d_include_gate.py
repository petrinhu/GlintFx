# SPDX-License-Identifier: AGPL-3.0-or-later
#
# draw2d_include_gate.py - the orchestration and the report of the draw2d direction gate (CONTRACT.md 5.3). check_tree
# joins the disk, the build log and the judges; print_report and verdict turn the result into the lines the ctest reads
# and the exit code. The command line lives in check_draw2d_include_direction.py.
#
# The count `dependencias_unicas` is the number of DISTINCT dependency paths in the union of every closure. It is not
# a sum over the objects: a header included by many objects counts once.
#
# Standard library only (L-07). Identifiers and comments in English (L-21); messages to the user in
# pt-br, without accents.

import sys
from collections import namedtuple

from draw2d_include_build_log import objects_by_source, parse_ninja_deps, read_compile_commands, run_ninja_deps
from draw2d_include_disk import scan_disk
from draw2d_include_judges import check_generator, judge_headers, judge_objects, judge_probe
from draw2d_include_paths import normalize_path

SCRIPT_NAME = "check_draw2d_include_direction"
Report = namedtuple("Report", "generator sources objects probes headers dependencies violations problems")


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
    """Join the judges: the disk is classified, the compiler's log is read once, and every object, the probe and every
    header are judged. Returns the Report."""
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
          f"sonda={report.probes} cabecalhos={len(report.headers)} dependencias_unicas={report.dependencies} "
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
