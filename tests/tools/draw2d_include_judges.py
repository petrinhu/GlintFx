# SPDX-License-Identifier: AGPL-3.0-or-later
#
# draw2d_include_judges.py - the judges of the draw2d direction gate (CONTRACT.md 5.3). Each judge takes what the disk
# and the build log gave it and returns violations and problems: an object judged by its closure, the probe as one more
# root, every header of draw2d placed in the union of the closures, and the generator that must write the log.
#
# Standard library only (L-07). Identifiers and comments in English (L-21); messages to the user in
# pt-br, without accents.

from draw2d_include_paths import display, is_forbidden, normalize_path

ALLOWED_GENERATORS = ("Ninja", "Ninja Multi-Config")
PROBE_LABEL = "SONDA"


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
