#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_draw2d_include_direction.py - R2D-IMAGE-RESOLVER R2a (CONTRACT.md 5.3): the gate that draw2d never reaches
# glintfx/gfss, neither through a direct #include nor through a header of core or of any other module.
#
# This file is the command line only. The work is split by subject, and each module says what it does in its header:
#   draw2d_include_paths.py     one comparable form for every path, and the rule that names gfss;
#   draw2d_include_disk.py      the closed enumeration of draw2d on the disk;
#   draw2d_include_build_log.py the compile database and the `ninja -t deps` log, read by one process;
#   draw2d_include_judges.py    the judges: objects, probe, headers, generator;
#   draw2d_include_gate.py      the orchestration and the report;
#   draw2d_include_fixtures.py  and draw2d_include_selftest.py, the selftest and its controls.
#
# WHAT IT DOES NOT SEE. A branch of #if that this build did not take. The CI matrix covers the branches of each system.
#
# Standard library only (L-07). Identifiers and comments in English (L-21).
#
# USAGE: check_draw2d_include_direction.py --check --source <root> --build <build dir> --ninja <ninja>
#            --generator <generator> --probe <generated probe .cpp>
#        check_draw2d_include_direction.py --selftest

import argparse
import subprocess
import sys

from draw2d_include_gate import check_tree, print_report, verdict
from draw2d_include_selftest import run_selftest


def run_process(argv):
    """Run one command; returns (rc, stdout, stderr). A command that cannot start is rc 127."""
    try:
        done = subprocess.run(argv, capture_output=True, text=True, check=False)
    except OSError as error:
        return 127, "", str(error)
    return done.returncode, done.stdout, done.stderr


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
