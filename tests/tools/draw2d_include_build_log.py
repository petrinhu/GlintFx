# SPDX-License-Identifier: AGPL-3.0-or-later
#
# draw2d_include_build_log.py - the build side of the draw2d direction gate (CONTRACT.md 5.3). It reads what the
# compiler already wrote during the build: compile_commands.json gives each source its objects, and `ninja -t deps`
# gives, for each object, the closure the compiler computed. ONE process runs (never one per file, GODS_LAWS.md global
# L-11, item 6; project L-45), and the text is parsed fail-closed: a line outside the format is a named problem.
#
# Standard library only (L-07). Identifiers and comments in English (L-21); messages to the user in
# pt-br, without accents.

import json
import os
import re

from draw2d_include_paths import normalize_path

DEPS_HEADER = re.compile(r"^(\S.*): #deps (\d+), deps mtime (-?\d+) \((VALID|STALE)\)$")
DEPS_ITEM = re.compile(r"^    (\S.*)$")


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
