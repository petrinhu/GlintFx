# SPDX-License-Identifier: AGPL-3.0-or-later
#
# win32_capture_protocol.py - the protocol that the Windows capture tool speaks, shared by every
# driver that launches it (tests/tools/win32_window_capture.cpp). It holds the table of verdict
# exit codes (a copy of tests/tools/window_capture_rules.hpp, kept equal by check_sibling_lists.py),
# how the tool's veredito line is read, the capture mechanisms and their pairs, and the count of
# capture pairs. How the tool is run (its deadline and its process tree) is in the sibling module
# win32_capture_process.py. Test tooling, not product code. Standard library only (L-07). Its users
# are run_capture_known_color_win32.py and run_example_capture_win32.py, the two drivers that launch
# the tool. Nothing runs when this file is executed: it only defines names for an importer.

import re
import sys
from pathlib import Path


# THE TABLE OF VERDICT CODES (D-W8-45): a COPY of tests/tools/window_capture_rules.hpp's
# k_verdict_code_table, the source, which check_sibling_lists.py keeps equal token by token.
# GLINTFX-SIBLING-LIST:window-capture-verdict-codes:START
VERDICT_CODE_TABLE = ("10=JANELAS", "11=INVISIVEL", "12=ICONICA", "13=FORA_DA_TELA", "14=OCLUIDA",
                      "15=CAPTURA_RECUSADA")
# GLINTFX-SIBLING-LIST:window-capture-verdict-codes:END
VERDICT_NAME_BY_CODE = {int(entry.split("=")[0]): entry.split("=")[1] for entry in VERDICT_CODE_TABLE}
VERDICT_LINE = re.compile(r"veredito: (.*)$", re.MULTILINE)
PLAIN_FAILURE_CODES = (1, 2, 124, 127)  # fixture, tool, timeout, impossible run
MECHANISMS = ("printwindow", "bitblt")


def verdict_name(code):
    return VERDICT_NAME_BY_CODE.get(code)


def verdict_line_prefix(name):
    """How the tool writes a verdict on its `veredito:` line: JANELAS is `janelas=<n>`, the others
    are the table name with spaces."""
    return "janelas=" if name == "JANELAS" else name.replace("_", " ")


def line_agrees_with_code(code, output):
    """The LAST `veredito:` line of the tool starts with the words of the verdict the code names."""
    lines = VERDICT_LINE.findall(output)
    return bool(lines) and lines[-1].startswith(verdict_line_prefix(VERDICT_NAME_BY_CODE[code]))


def count_pairs(directory):
    """How many .raw files the directory holds, each with its .meta beside it; -1 when a .raw has none."""
    raws = sorted(Path(directory).glob("*.raw")) if Path(directory).is_dir() else []
    return len(raws) if all(raw.with_suffix(".meta").is_file() for raw in raws) else -1


def check_capture_pairs(directory, who):
    """0 when each mechanism's directory holds exactly one complete pair."""
    for mechanism in MECHANISMS:
        pairs = count_pairs(Path(directory) / mechanism)
        if pairs != 1:
            print(f"{who}: FAIL {mechanism}: {pairs} pair(s) in {Path(directory) / mechanism}, exactly 1 expected",
                  file=sys.stderr)
            return 1
    return 0
