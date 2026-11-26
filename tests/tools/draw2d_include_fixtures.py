# SPDX-License-Identifier: AGPL-3.0-or-later
#
# draw2d_include_fixtures.py - the fixtures of the draw2d direction gate's selftest (CONTRACT.md 5.3): small
# trees written
# to a temporary folder, the compile database that goes with them, and canned `ninja -t deps` texts in the real format.
# No compiler and no ninja run here: the judges run on the fixtures through check_tree with a canned `run`.
#
# Standard library only (L-07). Identifiers and comments in English (L-21).

import argparse
import contextlib
import io
import json
import os
import tempfile
from collections import namedtuple

from draw2d_include_gate import check_tree, print_report, verdict

Case = namedtuple("Case", "tree files text entries generator")
FIXTURE_TEXT = "// fixture\n"
FIXTURE_FILES = ("src/draw2d/a.cpp", "src/draw2d/a.hpp", "include/glintfx/draw2d/pub.hpp", "src/draw2d/CMakeLists.txt")
SOURCE_OUTPUT = "src/CMakeFiles/glintfx.dir/draw2d/a.cpp.o"
PROBE_OUTPUT = "tests/CMakeFiles/draw2d_include_direction_probe.dir/draw2d_include_direction_probe.cpp.o"


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


def render(report):
    """What print_report would write, captured as (stdout, stderr)."""
    out, err = io.StringIO(), io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
        print_report(report)
    return out.getvalue(), err.getvalue()


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
