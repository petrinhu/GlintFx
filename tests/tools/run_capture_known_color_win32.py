#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# run_capture_known_color_win32.py - QA-SCREEN-CAPTURE C2b-3 (D-W8-36, D-W8-37, D-W8-39, D-W8-40 of
# /var/tmp/cto-w8/decisao-c2b.md): the DRIVER of the Windows capture proof, the counterpart of
# tests/container/run_capture_known_color.sh. Test tooling, not product code. Standard library only
# (L-07). Python, not sh: the Windows CI has no sh (the same reason the driver's selftest is
# UNIX-only there).
#
# For each srgb_framebuffer mode (off, on) it runs tests/tools/win32_window_capture.cpp's tool, which
# launches the fixture (capture_known_color_smoke), reads the window by PrintWindow and by BitBlt of
# the screen and leaves one capture pair per mechanism, then judges on the host:
#   1. the tool's exit code (0 only when the fixture AND the capture were good; 1 = the fixture
#      failed, 2 = the tool itself failed, 10 to 15 = one CAPTURE VERDICT each (D-W8-45: 10 janelas,
#      11 INVISIVEL, 12 ICONICA, 13 FORA DA TELA, 14 OCLUIDA, 15 CAPTURA RECUSADA); the code and the
#      tool's `veredito:` line must AGREE, or the mode is rejected as INCOERENTE; a code outside the
#      table is CODIGO_DESCONHECIDO; the driver prints `motivo=<name> codigo=<n>` for every mode);
#   2. exactly ONE captured pair in <out>/<mode>/capture/printwindow and in .../bitblt;
#   3. raw_to_png.py --readback turns the fixture's OWN glReadPixels (the INTERNAL reading, alpha
#      included) into a PNG, and image_probe.py probes it with the same probe file the Linux driver
#      uses (tests/fixtures/capture_known_color_probes_<mode>.txt): the library's alpha is proven
#      HERE, because the screen capture carries none (D-W8-39);
#   4. capture_vs_readback.py --alpha-absent-declared: each capture equals the internal reading in
#      R, G and B, pixel by pixel; it prints alpha_not_compared=<n>, and this driver prints it as a
#      MEASURED line (capture_alpha_compared) so the absence is COUNTED, never silent;
#   5. raw_to_png.py turns each capture into a PNG (the artifact a human looks at).
# MODES: `off` runs. `on` is an ABSENCE declared by the fixture when the runner's Mesa refuses
# srgb_framebuffer=on (exit 77 and the line "AUSENCIA DECLARADA srgb_framebuffer=on", D-SRGB2-13): it
# is counted (MEASURED capture_known_color_smoke.srgb_on_absent=1), not skipped silently. If `on`
# runs after all, it is judged like `off` with the `on` probe file. The mode set must be EXACTLY
# {off, on} (a loop `off off` has two entries and never covers `on`).
#
# USAGE: run_capture_known_color_win32.py [--sabotage NAME] <win32_window_capture.exe>
#            <capture_known_color_smoke.exe> <out_dir>
#        run_capture_known_color_win32.py --selftest
# --sabotage is the debut proof (L-36): swap_red_blue, alpha_half and corrupt_readback make the fixture
# draw a wrong frame on purpose; `occlude` (D-W8-42) makes the TOOL cover the window with one of its own
# (`--sabotage-occlude`), and the verdict MUST be OCLUIDA (code 14). The verdict MUST be a rejection;
# this script does not invert it. Exit 0 only when every mode passed.
#
# WHAT THIS DOES NOT PROVE (declared, L-43): the alpha of the SCREEN capture (absent by
# construction, counted); ACTIVATION and FOCUS (the tool never reads them); DPI scaling other than
# 96 (the runner's); the composition on a real GPU (the runner draws with Mesa llvmpipe).

import contextlib
import io
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass, field, replace
from pathlib import Path

from win32_capture_protocol import (
    MECHANISMS,
    PLAIN_FAILURE_CODES,
    VERDICT_NAME_BY_CODE,
    check_capture_pairs,
    count_pairs,
    line_agrees_with_code,
    verdict_name,
)
from win32_capture_process import (
    PROCESS_TIMEOUT_SECONDS,
    KILL_GRACE_SECONDS,
    run_process,
)

SCRIPT_NAME = "run_capture_known_color_win32.py"
TOOLS_DIR = Path(__file__).resolve().parent
REPO_ROOT = TOOLS_DIR.parent.parent
MODES = ("off", "on")
# The title the fixture gives its window. A copy of a fact the fixture owns, so a selftest control
# reads the fixture's source and fails when the two drift (feedback_copia_em_vez_de_fonte).
WINDOW_TITLE = "janela da fumaca de cor conhecida"
FIXTURE_SOURCE = REPO_ROOT / "tests" / "parity" / "capture_known_color_smoke.cpp"
OPTIONS_SOURCE = TOOLS_DIR / "window_capture_options.cpp"
# THE BUDGETS (D-W8-61): this driver OWNS all three and hands them to the tool and the fixture, so
# no copy of a default of the tool hides here. What is proven is their RELATION (budgets_are_coherent),
# not a number: a budget fixed alone would let 1 ms and 200000 ms through.
# How long the tool waits for the fixture's "presented" line, and then for the fixture to exit by itself
# after the WM_CLOSE (the tool's --present-budget-ms and --exit-budget-ms).
PRESENT_BUDGET_MS = 30000
EXIT_BUDGET_MS = 10000
# How long the fixture keeps its window alive and answering messages after it presented (its
# --hold-until-close budget): it covers the capture phase, between "presented" and the WM_CLOSE.
HOLD_UNTIL_CLOSE_MS = 120000
# The least the hold may be: the capture phase measured 1.21 s whole on the Windows runner (run
# 37558358855 and CI 37611152408), so 60 s is a margin of about 50 times.
CAPTURE_PHASE_FLOOR_MS = 60000
# THE OTHER FLOORS (I-2 of the C2b-5 review: a budget without a floor let 0 ms through). What was
# MEASURED is only the whole test on the Windows runner, 1.21 s (the same two runs), so that figure
# is a CEILING for each phase (present, hold-to-close and exit all fit in it), never a per-phase
# measurement: each floor is that ceiling times a declared margin.
#  - present: 10 s, about 8 times 1.21 s (the wait for the fixture's "presented" line, a window on a
#    cold runner).
#  - exit: 6 s, about 5 times 1.21 s (the fixture answers the WM_CLOSE and leaves).
#  - kill grace: 2 s. NOT MEASURED (taskkill /T is not measured on Windows either, see kill_tree in
#    win32_capture_process.py): it is
#    the declared minimum for the pipe to reach EOF after the kill, and 0 would make the wait for it
#    expire at once.
PRESENT_BUDGET_FLOOR_MS = 10000
EXIT_BUDGET_FLOOR_MS = 6000
KILL_GRACE_FLOOR_SECONDS = 2
# THE SABOTAGES. The three of the fixture travel on the fixture's command line; `occlude` is a mode
# of the TOOL (D-W8-42: it covers the window from outside) and is never handed to the fixture.
FIXTURE_SABOTAGES = ("swap_red_blue", "alpha_half", "corrupt_readback")
TOOL_SABOTAGES = {"occlude": "--sabotage-occlude"}
ABSENCE_LINE = "AUSENCIA DECLARADA srgb_framebuffer=on"
ABSENCE_FIXTURE_EXIT = re.compile(r"fixture exit=77\s*$", re.MULTILINE)
TOOL_FIXTURE_FAILED = 1
COMPARATOR_LINE = re.compile(r"pixels=(\d+) differing=(\d+) alpha_not_compared=(\d+)")


@dataclass
class run_config:
    tool: str
    fixture: str
    out_dir: Path
    sabotage: str = ""
    probes_dir: Path = REPO_ROOT / "tests" / "fixtures"


@dataclass(frozen=True)
class budget_set:
    """The numbers whose relation D-W8-61 and I-2 prove (milliseconds, except those named `_s`): the six
    budgets, the floor of the hold and the three floors of present, exit and kill grace."""
    present_ms: int
    hold_ms: int
    exit_ms: int
    kill_grace_s: int
    timeout_s: int
    floor_ms: int
    present_floor_ms: int
    exit_floor_ms: int
    grace_floor_s: int


def current_budgets():
    return budget_set(PRESENT_BUDGET_MS, HOLD_UNTIL_CLOSE_MS, EXIT_BUDGET_MS, KILL_GRACE_SECONDS,
                      PROCESS_TIMEOUT_SECONDS, CAPTURE_PHASE_FLOOR_MS, PRESENT_BUDGET_FLOOR_MS,
                      EXIT_BUDGET_FLOOR_MS, KILL_GRACE_FLOOR_SECONDS)


def budgets_are_coherent(budgets):
    """True when the hold covers the capture phase (hold >= floor), the present wait, the exit wait and
    the kill grace each reach their own floor (I-2), AND the orphan fixture goes away by itself before the
    driver kills the tree, even if `taskkill /T` (not measured on Windows) fails: the present wait, the
    hold, the exit wait and the kill grace fit in the process timeout."""
    covers_capture = budgets.hold_ms >= budgets.floor_ms
    reaches_floors = (budgets.present_ms >= budgets.present_floor_ms
                      and budgets.exit_ms >= budgets.exit_floor_ms
                      and budgets.kill_grace_s >= budgets.grace_floor_s)
    total_ms = budgets.present_ms + budgets.hold_ms + budgets.exit_ms + budgets.kill_grace_s * 1000
    return covers_capture and reaches_floors and total_ms <= budgets.timeout_s * 1000


@dataclass
class mode_result:
    mode: str
    # "ran", "absent", "refused" (a capture verdict whose code and line agree), "incoherent" (a known
    # code whose line is missing or says another verdict), "unknown" (a code outside the table) or "failed"
    status: str
    checks: dict = field(default_factory=dict)  # name -> exit code
    alpha_compared: int = 0
    motivo: str = ""
    codigo: int = 0

    def passed(self):
        return self.status == "absent" or (self.status == "ran" and all(rc == 0 for rc in self.checks.values()))


def helper(script, *args):
    return [sys.executable, str(TOOLS_DIR / script), *args]


def mode_dir(config, mode):
    return config.out_dir / mode


def clean_mode_output(config, mode):
    """A reused out directory would hand the judge the PREVIOUS run's captures (a fixture that does
    nothing would then read as a pass): remove this mode's directory first. Refuses an empty or
    foreign mode, and a directory that is not inside out_dir (GODS_LAWS.md L-53)."""
    if mode not in MODES or not str(config.out_dir):
        raise ValueError(f"refusing to clean mode={mode!r} under out_dir={str(config.out_dir)!r}")
    target = mode_dir(config, mode).resolve()
    if config.out_dir.resolve() not in target.parents:
        raise ValueError(f"{target} is not inside {config.out_dir}")
    shutil.rmtree(target, ignore_errors=True)


def tool_command(config, mode):
    """The tool's command line: its own options (the title, the output, the present and exit budgets of
    D-W8-61, and the sabotage `occlude`, the only one that is the tool's), then `--` and the fixture's: mode, readback directory, the hold budget
    (always: PrintWindow needs the owner of the window answering messages) and the fixture's sabotage."""
    directory = mode_dir(config, mode)
    fixture = [config.fixture, mode, directory / "readback", "--hold-until-close", str(HOLD_UNTIL_CLOSE_MS)]
    if config.sabotage in FIXTURE_SABOTAGES:
        fixture.append(config.sabotage)
    tool_flags = [TOOL_SABOTAGES[config.sabotage]] if config.sabotage in TOOL_SABOTAGES else []
    budgets = ["--present-budget-ms", str(PRESENT_BUDGET_MS), "--exit-budget-ms", str(EXIT_BUDGET_MS)]
    return [config.tool, "--title", WINDOW_TITLE, "--out", directory / "capture", *budgets, *tool_flags, "--", *fixture]


def classify_tool_run(mode, code, output):
    """"ran" (exit 0); "refused" (exit 10 to 15, a capture verdict, and the tool's `veredito:` line says
    the same verdict); "incoherent" (a known code with a missing or divergent line); "absent" (mode on,
    the fixture declared the sRGB absence and exited 77: the tool then reports its own exit 1 for a
    fixture that never presented); "failed" (1, 2, 124 or 127); "unknown" (any other code). The tool's
    exit 2 is NEVER a verdict."""
    if code == 0:
        return "ran"
    if code in VERDICT_NAME_BY_CODE:
        return "refused" if line_agrees_with_code(code, output) else "incoherent"
    declared = mode == "on" and code == TOOL_FIXTURE_FAILED and ABSENCE_LINE in output \
        and ABSENCE_FIXTURE_EXIT.search(output) is not None
    if declared:
        return "absent"
    return "failed" if code in PLAIN_FAILURE_CODES else "unknown"


MOTIVO_BY_STATUS = {"ran": "PRONTA", "absent": "AUSENCIA_DECLARADA", "failed": "FALHA_DE_FIXTURE_OU_FERRAMENTA",
                    "incoherent": "INCOERENTE", "unknown": "CODIGO_DESCONHECIDO"}


def motivo_of(status, code):
    return verdict_name(code) if status == "refused" else MOTIVO_BY_STATUS[status]


def report(output):
    sys.stdout.write(output if output.endswith("\n") or not output else output + "\n")


def convert_readback_png(config, mode, run):
    directory = mode_dir(config, mode)
    code, output = run(helper("raw_to_png.py", "--readback", directory / "readback" / f"readback_{mode}.raw",
                              directory / "readback" / f"readback_{mode}.meta", directory / "readback.png"))
    report(output)
    return code


def probe_readback_png(config, mode, run):
    probes = config.probes_dir / f"capture_known_color_probes_{mode}.txt"
    code, output = run(helper("image_probe.py", mode_dir(config, mode) / "readback.png", probes))
    report(output)
    return code


def compare_mechanism(config, mode, mechanism, run):
    """(exit code, pixels whose alpha WAS compared). An exit 0 whose summary line was not printed is
    not understood, and so it is a failure."""
    directory = mode_dir(config, mode)
    code, output = run(helper("capture_vs_readback.py", "--alpha-absent-declared", directory / "capture" / mechanism,
                              directory / "readback" / f"readback_{mode}.raw",
                              directory / "readback" / f"readback_{mode}.meta"))
    report(output)
    summary = COMPARATOR_LINE.search(output)
    if code != 0 or summary is None:
        return (code or 1), 0
    return 0, int(summary.group(1)) - int(summary.group(3))


def convert_capture_png(config, mode, mechanism, run):
    directory = mode_dir(config, mode)
    code, output = run(helper("raw_to_png.py", directory / "capture" / mechanism, directory / "png" / mechanism))
    report(output)
    return code


def judge_internal_reading(config, mode, run, result):
    """The fixture's own glReadPixels, probed (alpha included). It is judged on its own so that a
    failing tool still leaves the ruler able to tell 'the internal reading is wrong' from 'the frame
    did not reach the system' (D-W8-36)."""
    result.checks["readback_png"] = convert_readback_png(config, mode, run)
    result.checks["probes"] = probe_readback_png(config, mode, run) if result.checks["readback_png"] == 0 else 1


def judge_failed_tool(config, mode, run, code):
    """The tool did not vouch for the capture. The fixture writes its readback BEFORE it presents,
    so the internal reading is judged whenever it exists; the mode stays rejected by the tool."""
    result = mode_result(mode, "failed", {"tool": code})
    if (mode_dir(config, mode) / "readback" / f"readback_{mode}.raw").is_file():
        judge_internal_reading(config, mode, run, result)
    return result


def judge_refused_capture(config, mode, run, code):
    """The tool printed a capture verdict (exit 10 to 15): the mode is rejected for it, and nothing else
    is hidden. The internal reading is judged (the library drew right or not), and the PrintWindow
    pair, when it exists, is compared and RECORDED as it came. It only exists in the case CAPTURA
    RECUSADA of the BitBlt (PrintWindow runs first, on a window that was READY): the tool captures no
    window that is not ready (D-W8-44), so the pair is never the measurement of a window the library
    never showed. The screen read was not taken."""
    result = mode_result(mode, "refused", {"tool": code})
    has_readback = (mode_dir(config, mode) / "readback" / f"readback_{mode}.raw").is_file()
    if has_readback:
        judge_internal_reading(config, mode, run, result)
    if count_pairs(mode_dir(config, mode) / "capture" / "printwindow") == 1:
        result.checks["compare_printwindow"], _ = compare_mechanism(config, mode, "printwindow", run)
    return result


def judge_ran_mode(config, mode, run):
    """The host-side checks over what the tool left. Returns the mode's result."""
    result = mode_result(mode, "ran")
    result.checks["pairs"] = check_capture_pairs(mode_dir(config, mode) / "capture", SCRIPT_NAME)
    judge_internal_reading(config, mode, run, result)
    for mechanism in MECHANISMS:
        result.checks[f"compare_{mechanism}"], compared = compare_mechanism(config, mode, mechanism, run)
        result.alpha_compared += compared
        result.checks[f"png_{mechanism}"] = convert_capture_png(config, mode, mechanism, run)
    return result


def run_mode(config, mode, run):
    clean_mode_output(config, mode)
    (mode_dir(config, mode) / "readback").mkdir(parents=True)
    code, output = run(tool_command(config, mode))
    report(output)  # the evidence the red is READ from: janelas=, visivel=, oclusao:, the absence line
    status = classify_tool_run(mode, code, output)
    print(f"{SCRIPT_NAME}: mode={mode} tool={code} status={status}")
    result = judge_by_status(config, mode, run, (status, code))
    result.motivo, result.codigo = motivo_of(status, code), code
    return result


def judge_by_status(config, mode, run, outcome):
    """What the host judges for each way the tool can have ended."""
    status, code = outcome
    if status == "absent":
        return mode_result(mode, status)
    if status == "refused":
        return judge_refused_capture(config, mode, run, code)
    if status == "ran":
        return judge_ran_mode(config, mode, run)
    result = judge_failed_tool(config, mode, run, code)
    result.status = status
    return result


def modes_are_exact(modes_seen):
    return sorted(modes_seen) == sorted(MODES)


def measured_lines(results):
    alpha = sum(result.alpha_compared for result in results if result.status == "ran")
    absent = int(any(result.mode == "on" and result.status == "absent" for result in results))
    return [f"MEASURED capture_known_color_smoke.capture_alpha_compared={alpha}",
            f"MEASURED capture_known_color_smoke.srgb_on_absent={absent}"]


def run_all(config, run=run_process):
    results = [run_mode(config, mode, run) for mode in MODES]
    for result in results:
        print(f"{SCRIPT_NAME}: verdict mode={result.mode} motivo={result.motivo} codigo={result.codigo} "
              f"status={result.status} checks={result.checks} {'OK' if result.passed() else 'REPROVADO'}")
    for line in measured_lines(results):
        print(line)
    failed = [result.mode for result in results if not result.passed()]
    print(f"{SCRIPT_NAME}: modos={' '.join(result.mode for result in results)} reprovados={len(failed)} "
          f"sabotagem={config.sabotage or 'nenhuma'}")
    return modes_are_exact([result.mode for result in results]) and not failed


def parse_args(args):
    sabotage = ""
    if len(args) >= 2 and args[0] == "--sabotage":
        sabotage, args = args[1], args[2:]
    if len(args) != 3 or (sabotage and sabotage not in FIXTURE_SABOTAGES + tuple(TOOL_SABOTAGES)):
        return None
    return run_config(tool=args[0], fixture=args[1], out_dir=Path(args[2]), sabotage=sabotage)


def real_main(args):
    config = parse_args(args)
    if config is None:
        print(f"usage: {SCRIPT_NAME} [--sabotage NAME] <win32_window_capture.exe> "
              f"<capture_known_color_smoke.exe> <out_dir>  |  --selftest", file=sys.stderr)
        return 2
    config.out_dir.mkdir(parents=True, exist_ok=True)
    return 0 if run_all(config) else 1


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        # The controls live beside the driver (CAPTURE-DRIVER-SPLIT, D-W8-178), loaded only here.
        from run_capture_known_color_win32_selftest import selftest_main
        selftest_main()
    else:
        sys.exit(real_main(args))


if __name__ == "__main__":
    main()
