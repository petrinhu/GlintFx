#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# run_example_capture_win32.py - DEMO-1 D5b (D-W8-75, and D-W8-125 to D-W8-128 of
# /var/tmp/cto-w8/plano-d5b.md): the DRIVER of the Windows capture of the example first_window. Test
# tooling, not product code. Standard library only (L-07). Python, not sh: the Windows CI has no sh.
#
# It runs tests/tools/win32_window_capture.cpp's tool, which launches the example, waits for the
# example's ready line, reads the window by PrintWindow and by BitBlt of the screen and leaves one
# capture pair per mechanism. Then it judges on the host:
#   1. the tool's exit code AND its last `veredito:` line must agree (win32_capture_protocol.py). Exit 0
#      with a rejection line, or with no line, is INCOERENTE; exit 10 to 15 is the verdict it names, or
#      INCOERENTE when the line says another; 1, 2, 124 and 127 are failures; any other code is
#      CODIGO_DESCONHECIDO. Only exit 0 with an agreeing line goes on to the captures;
#   2. exactly ONE pair per mechanism under <out>/first_window/capture/;
#   3. per mechanism: the size read from the pair's .meta (by raw_to_png.py's own parser) must be 640x480
#      (D-W8-72); raw_to_png.py turns the pair into a PNG under <out>/first_window/png/ (the artifact a
#      human looks at); ONLY with the right size, image_probe.py probes that PNG with the static probe
#      file and must report exactly 12 probes, none failed;
#   4. the alpha: the screen capture is XRGB8888 and carries none; raw_to_png.py gives it 255, so the
#      probes' FF passes without measuring anything. The driver prints
#      `MEASURED first_window.capture_alpha_compared=0` on EVERY run, so the absence is counted.
# The window title and the ready line are facts the EXAMPLE owns (examples/first_window/first_window.cpp,
# born in D6-red); the selftest control that reads them in the example's source arrives with it.
#
# USAGE: run_example_capture_win32.py <win32_window_capture.exe> <first_window.exe> <out_dir>
#        run_example_capture_win32.py --selftest
# Exit 0 only when the capture passed.
#
# WHAT THIS DOES NOT PROVE (declared, L-43): the alpha of the screen capture (absent, counted); that the
# two captures show the same frame (the green square moves between them, so they are never compared);
# DPI scaling other than 96; the composition on a real GPU (the runner draws with Mesa llvmpipe).

import contextlib
import io
import re
import shutil
import subprocess
import sys
import tempfile
from dataclasses import dataclass, field
from pathlib import Path

from raw_to_png import CaptureError, parse_meta
from win32_capture_protocol import (
    MECHANISMS,
    PLAIN_FAILURE_CODES,
    VERDICT_LINE,
    VERDICT_NAME_BY_CODE,
    check_capture_pairs,
    line_agrees_with_code,
    verdict_line_prefix,
    verdict_name,
)
from win32_capture_process import KILL_GRACE_SECONDS, PROCESS_TIMEOUT_SECONDS, run_process

SCRIPT_NAME = "run_example_capture_win32.py"
TOOLS_DIR = Path(__file__).resolve().parent
REPO_ROOT = TOOLS_DIR.parent.parent
# The example's name: the directory its captures go to (D-W8-90) and the owner of its MEASURED line.
EXAMPLE_NAME = "first_window"
# Facts the example owns (D-W8-72, D-W8-73): its window title and the line it prints when its first frame
# is presented. The control that reads them in the example's source comes with the example (D6-red).
WINDOW_TITLE = "GlintFx example: first window"
READY_LINE = "first_window: first frame presented"
# THE BUDGETS: this driver owns them and hands them to the tool, so no default of the tool hides here.
PRESENT_BUDGET_MS = 30000
EXIT_BUDGET_MS = 10000
# The scene, pre-registered (D-W8-72): the capture is 640x480 and the static probe file holds 12 probes.
CAPTURE_SIZE = (640, 480)
PROBE_COUNT = 12
PROBES_FILE = REPO_ROOT / "tests" / "fixtures" / "first_window_probes.txt"
OPTIONS_SOURCE = TOOLS_DIR / "window_capture_options.cpp"
PROBE_SUMMARY = re.compile(r"^image_probe\.py: probes=(\d+) failed=(\d+)$", re.MULTILINE)
MEASURED_ALPHA_LINE = f"MEASURED {EXAMPLE_NAME}.capture_alpha_compared=0"
MOTIVO_BY_STATUS = {"ran": "PRONTA", "failed": "FALHA_DE_EXEMPLO_OU_FERRAMENTA", "incoherent": "INCOERENTE",
                    "unknown": "CODIGO_DESCONHECIDO"}


@dataclass
class run_config:
    tool: str
    example: str
    out_dir: Path
    probes_file: Path = PROBES_FILE
    probe_count: int = PROBE_COUNT
    capture_size: tuple = CAPTURE_SIZE


@dataclass
class example_result:
    status: str
    code: int
    motivo: str
    checks: dict = field(default_factory=dict)  # name -> exit code; filled only when status is "ran"

    def passed(self):
        return bool(self.checks) and all(rc == 0 for rc in self.checks.values())


def helper(script, *args):
    return [sys.executable, str(TOOLS_DIR / script), *args]


def report(output):
    sys.stdout.write(output if output.endswith("\n") or not output else output + "\n")


def example_dir(config):
    return config.out_dir / EXAMPLE_NAME


def clean_example_output(config):
    """A reused out directory would hand the judge the PREVIOUS run's captures: first_window/ is removed
    before the tool runs, and nothing else under out_dir is touched (GODS_LAWS.md L-53)."""
    shutil.rmtree(example_dir(config), ignore_errors=True)


def tool_command(config):
    """The tool's own options (title, output, the two budgets, the ready line), then `--` and the example
    ALONE: no --frames, the example runs until the tool's WM_CLOSE (D-W8-73, D-W8-75)."""
    budgets = ["--present-budget-ms", str(PRESENT_BUDGET_MS), "--exit-budget-ms", str(EXIT_BUDGET_MS)]
    return [config.tool, "--title", WINDOW_TITLE, "--out", example_dir(config) / "capture", *budgets,
            "--ready-line", READY_LINE, "--", config.example]


def success_line_agrees(output):
    """Exit 0 agrees with the tool's output when its LAST `veredito:` line exists and names no verdict of
    the table: a ready window leaves its readiness line, and a rejection line under exit 0 is incoherent."""
    lines = VERDICT_LINE.findall(output)
    return bool(lines) and not any(lines[-1].startswith(verdict_line_prefix(name))
                                   for name in VERDICT_NAME_BY_CODE.values())


def classify_tool_run(code, output):
    """"ran" (exit 0, agreeing line); "refused" (exit 10 to 15 whose line names the same verdict);
    "incoherent" (a code whose line disagrees); "failed" (1, 2, 124, 127); "unknown" (anything else)."""
    if code == 0:
        return "ran" if success_line_agrees(output) else "incoherent"
    if code in VERDICT_NAME_BY_CODE:
        return "refused" if line_agrees_with_code(code, output) else "incoherent"
    return "failed" if code in PLAIN_FAILURE_CODES else "unknown"


def motivo_of(status, code):
    return verdict_name(code) if status == "refused" else MOTIVO_BY_STATUS[status]


def read_capture_size(capture_dir):
    """(width, height) of the one pair in capture_dir, from its .meta by raw_to_png.py's own parser; None
    when it cannot be read. The pair count was checked before."""
    try:
        raw = next(Path(capture_dir).glob("*.raw"))
        meta = parse_meta(raw.with_suffix(".meta").read_text())
    except (CaptureError, OSError, StopIteration, ValueError):
        return None
    return meta["width"], meta["height"]


def probe_verdict(code, output, expected):
    """0 only when image_probe.py exited 0 AND reported exactly `expected` probes, none failed: the count is
    the one D-W8-72 fixed, so a shorter or a longer probe file is a failure, never a pass."""
    summary = PROBE_SUMMARY.search(output)
    if code != 0 or summary is None:
        return code or 1
    probes, failed = int(summary.group(1)), int(summary.group(2))
    return 0 if probes == expected and failed == 0 else 1


def probe_capture(config, mechanism, run):
    """image_probe.py over the PNG of the mechanism's one pair, judged by probe_verdict."""
    capture_dir = example_dir(config) / "capture" / mechanism
    png = example_dir(config) / "png" / mechanism / f"{next(capture_dir.glob('*.raw')).stem}.png"
    code, output = run(helper("image_probe.py", png, config.probes_file))
    report(output)
    return probe_verdict(code, output, config.probe_count)


def judge_mechanism(config, mechanism, run, checks):
    """The size from the .meta, the PNG for a human, and the probes ONLY when the size is right and the PNG
    exists (D-W8-72: a capture of the wrong size is refused before any probe)."""
    capture_dir = example_dir(config) / "capture" / mechanism
    size = read_capture_size(capture_dir)
    shown = "ilegivel" if size is None else f"{size[0]}x{size[1]}"
    print(f"{SCRIPT_NAME}: {mechanism} tamanho={shown} esperado={config.capture_size[0]}x{config.capture_size[1]}")
    checks[f"size_{mechanism}"] = 0 if size == config.capture_size else 1
    code, output = run(helper("raw_to_png.py", capture_dir, example_dir(config) / "png" / mechanism))
    report(output)
    checks[f"png_{mechanism}"] = code
    if checks[f"size_{mechanism}"] != 0 or code != 0:
        return
    checks[f"probes_{mechanism}"] = probe_capture(config, mechanism, run)


def judge_captures(config, run, checks):
    """Exactly one pair per mechanism, then each mechanism judged on its own."""
    checks["pairs"] = check_capture_pairs(example_dir(config) / "capture", SCRIPT_NAME)
    if checks["pairs"] != 0:
        return
    for mechanism in MECHANISMS:
        judge_mechanism(config, mechanism, run, checks)


def run_example(config, run=run_process):
    """The whole run: clean, the tool, the classification, the captures (only when the tool vouched for
    them), then the verdict line and the MEASURED line, both printed on EVERY run."""
    clean_example_output(config)
    code, output = run(tool_command(config))
    report(output)
    status = classify_tool_run(code, output)
    result = example_result(status, code, motivo_of(status, code))
    if status == "ran":
        judge_captures(config, run, result.checks)
    print(f"{SCRIPT_NAME}: verdict exemplo={EXAMPLE_NAME} motivo={result.motivo} codigo={code} status={status} "
          f"checks={result.checks} {'OK' if result.passed() else 'REPROVADO'}")
    print(MEASURED_ALPHA_LINE)
    return result.passed()


def parse_args(args):
    if len(args) != 3 or not all(args):
        return None
    return run_config(tool=args[0], example=args[1], out_dir=Path(args[2]))


def real_main(args):
    config = parse_args(args)
    if config is None:
        print(f"usage: {SCRIPT_NAME} <win32_window_capture.exe> <first_window.exe> <out_dir>  |  --selftest",
              file=sys.stderr)
        return 2
    config.out_dir.mkdir(parents=True, exist_ok=True)
    return 0 if run_example(config) else 1


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        # The controls live beside the driver (CAPTURE-DRIVER-SPLIT, D-W8-178), loaded only here.
        from run_example_capture_win32_selftest import selftest_main
        selftest_main()
    else:
        sys.exit(real_main(args))


if __name__ == "__main__":
    main()
