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
import signal
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass, field, replace
from pathlib import Path

SCRIPT_NAME = "run_capture_known_color_win32.py"
TOOLS_DIR = Path(__file__).resolve().parent
REPO_ROOT = TOOLS_DIR.parent.parent
MODES = ("off", "on")
# The title the fixture gives its window. A copy of a fact the fixture owns, so a selftest control
# reads the fixture's source and fails when the two drift (feedback_copia_em_vez_de_fonte).
WINDOW_TITLE = "janela da fumaca de cor conhecida"
FIXTURE_SOURCE = REPO_ROOT / "tests" / "parity" / "capture_known_color_smoke.cpp"
TOOL_SOURCE = TOOLS_DIR / "win32_window_capture.cpp"
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
# THE SABOTAGES. The three of the fixture travel on the fixture's command line; `occlude` is a mode
# of the TOOL (D-W8-42: it covers the window from outside) and is never handed to the fixture.
FIXTURE_SABOTAGES = ("swap_red_blue", "alpha_half", "corrupt_readback")
TOOL_SABOTAGES = {"occlude": "--sabotage-occlude"}
ABSENCE_LINE = "AUSENCIA DECLARADA srgb_framebuffer=on"
ABSENCE_FIXTURE_EXIT = re.compile(r"fixture exit=77\s*$", re.MULTILINE)
TOOL_FIXTURE_FAILED = 1
# THE TABLE OF VERDICT CODES (D-W8-45): a COPY of tests/tools/window_capture_rules.hpp's
# k_verdict_code_table, the source, which check_sibling_lists.py keeps equal token by token.
# GLINTFX-SIBLING-LIST:window-capture-verdict-codes:START
VERDICT_CODE_TABLE = ("10=JANELAS", "11=INVISIVEL", "12=ICONICA", "13=FORA_DA_TELA", "14=OCLUIDA",
                      "15=CAPTURA_RECUSADA")
# GLINTFX-SIBLING-LIST:window-capture-verdict-codes:END
VERDICT_NAME_BY_CODE = {int(entry.split("=")[0]): entry.split("=")[1] for entry in VERDICT_CODE_TABLE}
VERDICT_LINE = re.compile(r"veredito: (.*)$", re.MULTILINE)
PLAIN_FAILURE_CODES = (1, 2, 124, 127)  # fixture, tool, timeout, impossible run
PROCESS_TIMEOUT_SECONDS = 180
KILL_GRACE_SECONDS = 5
MECHANISMS = ("printwindow", "bitblt")
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
    """The six numbers whose relation D-W8-61 proves (milliseconds, except the two in seconds)."""
    present_ms: int
    hold_ms: int
    exit_ms: int
    kill_grace_s: int
    timeout_s: int
    floor_ms: int


def current_budgets():
    return budget_set(PRESENT_BUDGET_MS, HOLD_UNTIL_CLOSE_MS, EXIT_BUDGET_MS, KILL_GRACE_SECONDS,
                      PROCESS_TIMEOUT_SECONDS, CAPTURE_PHASE_FLOOR_MS)


def budgets_are_coherent(budgets):
    """True when the hold covers the capture phase (hold >= floor) AND the orphan fixture goes away by
    itself before the driver kills the tree, even if `taskkill /T` (not measured on Windows) fails: the
    present wait, the hold, the exit wait and the kill grace fit in the process timeout."""
    covers_capture = budgets.hold_ms >= budgets.floor_ms
    total_ms = budgets.present_ms + budgets.hold_ms + budgets.exit_ms + budgets.kill_grace_s * 1000
    return covers_capture and total_ms <= budgets.timeout_s * 1000


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


def kill_tree(process):
    """Kills the child AND everything it started. The tool launches the fixture, and the fixture
    inherits nothing it should not (the tool restricts the handles), but a tree left alive after a
    timeout would hold the pipe: on Windows subprocess.run then waits for the pipe's EOF after the
    kill, which is exactly the hang the timeout exists to prevent. POSIX: the whole process group;
    Windows: taskkill /T. NOT MEASURED on the Windows runner (declared): the mitigation is the known
    one, the proof of it there is the first red run."""
    if sys.platform == "win32":
        subprocess.run(["taskkill", "/F", "/T", "/PID", str(process.pid)], capture_output=True, check=False)
    else:
        with contextlib.suppress(ProcessLookupError, PermissionError):
            os.killpg(process.pid, signal.SIGKILL)
    with contextlib.suppress(OSError):
        process.kill()


def run_process(args, timeout=PROCESS_TIMEOUT_SECONDS):
    """The real executor: (exit code, stdout+stderr). A hung child is a failure, never a hang."""
    try:
        process = subprocess.Popen([str(part) for part in args], stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                   text=True, errors="replace", close_fds=True,
                                   start_new_session=sys.platform != "win32")
    except OSError as error:
        return 127, f"cannot run {args[0]}: {error}"
    try:
        output, _ = process.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        kill_tree(process)
        with contextlib.suppress(subprocess.TimeoutExpired):
            process.communicate(timeout=KILL_GRACE_SECONDS)
        return 124, f"timeout after {timeout} s: {args[0]}"
    return process.returncode, output


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


def count_pairs(directory):
    """How many .raw files the directory holds, each with its .meta beside it; -1 when a .raw has none."""
    raws = sorted(Path(directory).glob("*.raw")) if Path(directory).is_dir() else []
    return len(raws) if all(raw.with_suffix(".meta").is_file() for raw in raws) else -1


def report(output):
    sys.stdout.write(output if output.endswith("\n") or not output else output + "\n")


def check_capture_pairs(directory):
    """0 when each mechanism's directory holds exactly one complete pair."""
    for mechanism in MECHANISMS:
        pairs = count_pairs(Path(directory) / mechanism)
        if pairs != 1:
            print(f"{SCRIPT_NAME}: FAIL {mechanism}: {pairs} pair(s) in {Path(directory) / mechanism}, exactly 1 expected",
                  file=sys.stderr)
            return 1
    return 0


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
    result.checks["pairs"] = check_capture_pairs(mode_dir(config, mode) / "capture")
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


# -- selftest ---------------------------------------------------------
CHECKS = []


def check(name, condition):
    CHECKS.append(name)
    if not condition:
        print(f"selftest: {name} FALHOU", file=sys.stderr)
        sys.exit(1)


def selftest_classification():
    check("off com saida 0 roda", classify_tool_run("off", 0, "...") == "ran")
    check("off com saida 1 falha", classify_tool_run("off", 1, "...") == "failed")
    absent_text = f"... {ABSENCE_LINE} ...\nfixture exit=77\n"
    check("on com a linha de ausencia e a saida 77 da fixture e ausencia declarada",
          classify_tool_run("on", TOOL_FIXTURE_FAILED, absent_text) == "absent")
    check("off com a linha de ausencia NAO e ausencia: so o on declara",
          classify_tool_run("off", TOOL_FIXTURE_FAILED, absent_text) == "failed")
    check("on com saida 1 mas SEM a linha de ausencia falha",
          classify_tool_run("on", TOOL_FIXTURE_FAILED, "fixture exit=1\n") == "failed")
    check("on com a linha de ausencia mas fixture que saiu com outro codigo falha",
          classify_tool_run("on", TOOL_FIXTURE_FAILED, f"{ABSENCE_LINE}\nfixture exit=3\n") == "failed")
    check("on com saida 2 da ferramenta falha mesmo com a linha", classify_tool_run("on", 2, absent_text) == "failed")
    check("on que roda de verdade (saida 0) roda", classify_tool_run("on", 0, "ok") == "ran")


def make_config(root):
    return run_config(tool="TOOL", fixture="FIXTURE", out_dir=Path(root) / "out", probes_dir=Path(root) / "probes")


def selftest_clean_output():
    with tempfile.TemporaryDirectory() as tmp:
        config = make_config(tmp)
        (config.out_dir / "off" / "capture").mkdir(parents=True)
        (config.out_dir / "off" / "capture" / "stale.raw").write_bytes(b"x")
        (config.out_dir / "on").mkdir()
        clean_mode_output(config, "off")
        check("saida velha do modo e apagada, a do outro modo fica",
              not (config.out_dir / "off").exists() and (config.out_dir / "on").exists())
        for bad in ("", "..", "../x", "dev"):
            try:
                clean_mode_output(config, bad)
            except ValueError:
                continue
            check(f"modo {bad!r} fora do conjunto e recusado antes de apagar", False)
        check("modo recusado nao apagou nada", (config.out_dir / "on").exists())


def selftest_exact_modes():
    check("conjunto exato {off, on} passa em qualquer ordem",
          modes_are_exact(["off", "on"]) and modes_are_exact(["on", "off"]))
    check("conjunto errado reprova (off off, so off, on on, tres)",
          not modes_are_exact(["off", "off"]) and not modes_are_exact(["off"])
          and not modes_are_exact(["on", "on"]) and not modes_are_exact(["off", "on", "on"])
          and not modes_are_exact([]))


def selftest_measured_lines():
    absent = [mode_result("off", "ran", alpha_compared=0), mode_result("on", "absent")]
    check("linhas MEASURED: alfa comparado 0 e sRGB ligado ausente 1",
          measured_lines(absent) == ["MEASURED capture_known_color_smoke.capture_alpha_compared=0",
                                     "MEASURED capture_known_color_smoke.srgb_on_absent=1"])
    ran = [mode_result("off", "ran", alpha_compared=76800), mode_result("on", "ran", alpha_compared=76800)]
    check("linhas MEASURED: se o on rodou, srgb_on_absent=0 e o alfa e a soma",
          measured_lines(ran) == ["MEASURED capture_known_color_smoke.capture_alpha_compared=153600",
                                  "MEASURED capture_known_color_smoke.srgb_on_absent=0"])


def selftest_title_matches_the_fixture():
    check("o fonte da fixture existe (piso, L-40): um so, em tests/parity, para os dois sistemas",
          FIXTURE_SOURCE.is_file())
    text = FIXTURE_SOURCE.read_text(encoding="utf-8", errors="replace")
    check("o titulo que o driver passa e o que a fixture da a janela", f'"{WINDOW_TITLE}"' in text)
    check("a linha de ausencia que o driver procura e a que a fixture imprime, e a saida dela e 77",
          f'"{ABSENCE_LINE}' in text and "k_exit_declared_absence = 77" in text)
    check("a gramatica que o driver usa e a que a fixture aceita: --hold-until-close <ms>",
          '"--hold-until-close"' in text and "--hold-until-close" in " ".join(
              str(part) for part in tool_command(make_config("."), "off")))
    check("as sabotagens que o driver entrega a fixture sao exatamente as que a fixture conhece, e occlude nao",
          all(f'"{name}"' in text for name in FIXTURE_SABOTAGES) and '"occlude"' not in text)


def selftest_tool_command():
    with tempfile.TemporaryDirectory() as tmp:
        config = make_config(tmp)
        command = tool_command(config, "off")
        check("comando da ferramenta: titulo, saida e os dois orcamentos dela antes de --, a linha da fixture depois, com o orcamento de espera",
              command[0] == "TOOL" and command[command.index("--title") + 1] == WINDOW_TITLE
              and command[command.index("--out") + 1] == config.out_dir / "off" / "capture"
              and command[command.index("--present-budget-ms") + 1] == str(PRESENT_BUDGET_MS)
              and command[command.index("--exit-budget-ms") + 1] == str(EXIT_BUDGET_MS)
              and command.index("--exit-budget-ms") < command.index("--")
              and command[command.index("--") + 1:] == ["FIXTURE", "off", config.out_dir / "off" / "readback",
                                                        "--hold-until-close", str(HOLD_UNTIL_CLOSE_MS)])
        config.sabotage = "swap_red_blue"
        check("sabotagem da fixture vai por ultimo, para a fixture, depois do orcamento de espera",
              tool_command(config, "on")[-1] == "swap_red_blue"
              and tool_command(config, "on")[-3] == "--hold-until-close")


def selftest_tool_receives_the_driver_budgets():
    """D-W8-61: the driver OWNS the present and exit budgets and hands them to the tool, before `--`
    (they are the tool's options, never the fixture's). The grammar is also read from the tool's source."""
    command = tool_command(make_config("."), "off")
    separator = command.index("--")
    check("o comando da ferramenta passa --present-budget-ms com o valor do driver, antes do --",
          "--present-budget-ms" in command[:separator]
          and command[command.index("--present-budget-ms") + 1] == str(PRESENT_BUDGET_MS))
    check("o comando da ferramenta passa --exit-budget-ms com o valor do driver, antes do --",
          "--exit-budget-ms" in command[:separator]
          and command[command.index("--exit-budget-ms") + 1] == str(EXIT_BUDGET_MS))
    check("os orcamentos da ferramenta NAO chegam a fixture (depois do --)",
          "--present-budget-ms" not in command[separator + 1:] and "--exit-budget-ms" not in command[separator + 1:])
    tool_text = TOOL_SOURCE.read_text(encoding="utf-8", errors="replace")
    check("a gramatica que o driver usa e a que a ferramenta aceita: --present-budget-ms e --exit-budget-ms",
          '"--present-budget-ms"' in tool_text and '"--exit-budget-ms"' in tool_text)


def selftest_budgets_are_coherent():
    """D-W8-61: the RELATION between the budgets, not a number. Every mutant is the real set with ONE
    field changed, so a mutated constant of the file (H2 hold 1 ms, H3 hold 200000, H4 timeout 100,
    H5 floor 1) is caught by the control that holds the real values."""
    real = current_budgets()
    check("a relacao vale com as constantes reais do driver", budgets_are_coherent(real))
    check("H2: hold de 1 ms reprova (a janela nao cobre a fase de captura)",
          not budgets_are_coherent(replace(real, hold_ms=1)))
    check("H3: hold de 200000 ms reprova (a fixture orfa sobreviveria ao prazo do processo)",
          not budgets_are_coherent(replace(real, hold_ms=200000)))
    check("H4: prazo do processo de 100 s reprova (a soma dos orcamentos nao cabe)",
          not budgets_are_coherent(replace(real, timeout_s=100)))
    check("H5: piso de 1 ms deixa o hold de 1 ms passar, e por isso o controle H2 o mata",
          budgets_are_coherent(replace(real, hold_ms=1, floor_ms=1))
          and not budgets_are_coherent(replace(real, hold_ms=1)))
    check("o orcamento de apresentacao, o de saida e a folga de morte entram na soma",
          not budgets_are_coherent(replace(real, present_ms=real.timeout_s * 1000))
          and not budgets_are_coherent(replace(real, exit_ms=real.timeout_s * 1000))
          and not budgets_are_coherent(replace(real, kill_grace_s=real.timeout_s)))
    check("a soma que fecha exatamente no prazo passa, um milissegundo a mais reprova",
          budgets_are_coherent(replace(real, timeout_s=(real.present_ms + real.hold_ms + real.exit_ms) // 1000
                                       + real.kill_grace_s))
          and not budgets_are_coherent(replace(real, timeout_s=(real.present_ms + real.hold_ms + real.exit_ms) // 1000
                                               + real.kill_grace_s - 1)))


def selftest_occlude_reaches_only_the_tool():
    with tempfile.TemporaryDirectory() as tmp:
        config = make_config(tmp)
        config.sabotage = "occlude"
        command = tool_command(config, "off")
        separator = command.index("--")
        check("occlude chega a ferramenta como --sabotage-occlude, antes do --",
              "--sabotage-occlude" in command[:separator])
        check("occlude NAO chega a fixture (depois do --)",
              "occlude" not in command[separator + 1:] and "--sabotage-occlude" not in command[separator + 1:])
        for other in FIXTURE_SABOTAGES + ("",):
            config.sabotage = other
            check(f"a sabotagem {other!r} nao liga o oclusor da ferramenta",
                  "--sabotage-occlude" not in tool_command(config, "off"))
        check("argumentos: occlude e as tres da fixture sao aceitos, um nome desconhecido e recusado",
              all(parse_args(["--sabotage", name, "T", "F", "o"]) is not None
                  for name in ("occlude",) + FIXTURE_SABOTAGES)
              and parse_args(["--sabotage", "occlude_typo", "T", "F", "o"]) is None
              and parse_args(["T", "F", "o"]) is not None)


def selftest_count_pairs():
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        check("diretorio inexistente conta zero pares", count_pairs(root / "nope") == 0)
        (root / "conn1_surface1.raw").write_bytes(b"x")
        check("raw sem meta conta -1", count_pairs(root) == -1)
        (root / "conn1_surface1.meta").write_text("m")
        check("um par conta 1", count_pairs(root) == 1)
        (root / "conn2_surface1.raw").write_bytes(b"x")
        (root / "conn2_surface1.meta").write_text("m")
        check("dois pares contam 2", count_pairs(root) == 2)


class scripted_run:
    """A run() double: records every command, answers each helper by script name from `codes`, and
    plays the capture tool by calling `tool_effect(command)` (which writes what the tool would)."""

    def __init__(self, codes=None, tool_effect=None, tool_reply=(0, "tool ok\n"), outputs=None):
        self.codes = codes or {}
        self.tool_effect = tool_effect or (lambda command: None)
        self.tool_reply = tool_reply
        self.outputs = outputs or {}
        self.calls = []

    def __call__(self, command):
        self.calls.append([str(part) for part in command])
        if command[0] == "TOOL":
            self.tool_effect(command)
            return self.tool_reply
        key = Path(str(command[1])).name + (" " + str(command[2]) if len(command) > 2 and str(command[2]).startswith("--") and "raw_to_png" in str(command[1]) else "")
        return self.codes.get(key, 0), self.outputs.get(key, "")

    def calls_of(self, script):
        return [call for call in self.calls if call[0] != "TOOL" and Path(call[1]).name == script]


def write_pairs(directory, count):
    directory.mkdir(parents=True, exist_ok=True)
    for index in range(count):
        (directory / f"conn1_surface{index + 1}.raw").write_bytes(b"x")
        (directory / f"conn1_surface{index + 1}.meta").write_text("m")


def tool_writing(pairs_by_mechanism):
    def effect(command):
        out = Path(command[command.index("--out") + 1])
        for mechanism, count in pairs_by_mechanism.items():
            write_pairs(out / mechanism, count)
    return effect


GOOD_COMPARE = "capture_vs_readback.py: pixels=76800 differing=0 alpha_not_compared=76800\n"


def silent():
    """The drivers print MEASURED lines; a selftest must never print them (the collector reads every
    test's log, and a selftest's value would collide with the real run's)."""
    return contextlib.redirect_stdout(io.StringIO())


def judge_with(codes, pairs, outputs=None):
    with tempfile.TemporaryDirectory() as tmp, silent():
        config = make_config(tmp)
        run = scripted_run(codes, tool_writing(pairs), outputs=outputs or {"capture_vs_readback.py": GOOD_COMPARE})
        return run_mode(config, "off", run), run, config


def selftest_wiring_all_clean():
    result, run, config = judge_with({}, {"printwindow": 1, "bitblt": 1})
    check("fiacao: tudo limpo passa o modo e o alfa comparado e pixels menos alpha_not_compared (0)",
          result.status == "ran" and result.passed() and result.alpha_compared == 0)
    compares = run.calls_of("capture_vs_readback.py")
    check("fiacao: o comparador roda UMA vez por mecanismo, sempre com a declaracao de alfa ausente",
          len(compares) == 2 and all(call[2] == "--alpha-absent-declared" for call in compares)
          and compares[0][3].endswith("printwindow") and compares[1][3].endswith("bitblt"))
    probes = run.calls_of("image_probe.py")
    check("fiacao: as sondas rodam sobre o PNG da LEITURA INTERNA com o arquivo de sondas do modo",
          len(probes) == 1 and probes[0][2].endswith("readback.png")
          and probes[0][3].endswith("capture_known_color_probes_off.txt"))
    readbacks = [call for call in run.calls_of("raw_to_png.py") if call[2] == "--readback"]
    check("fiacao: a leitura interna vira PNG pelo modo --readback, com o raw e o meta do modo",
          len(readbacks) == 1 and readbacks[0][3].endswith("readback_off.raw")
          and readbacks[0][4].endswith("readback_off.meta"))
    check("fiacao: cada captura vira PNG para o olho humano",
          len([call for call in run.calls_of("raw_to_png.py") if call[2] != "--readback"]) == 2)


def selftest_wiring_each_part_counts():
    both = {"printwindow": 1, "bitblt": 1}
    check("fiacao: ferramenta com saida 2 reprova o modo e nao chama nenhum auxiliar",
          not scripted_mode({}, both, tool_reply=(2, "x"))[0].passed()
          and scripted_mode({}, both, tool_reply=(2, "x"))[1].calls_of("capture_vs_readback.py") == [])
    for script in ("capture_vs_readback.py", "image_probe.py"):
        check(f"fiacao: {script} saindo 1 reprova o modo",
              not judge_with({script: 1}, both)[0].passed())
    check("fiacao: leitura interna que nao vira PNG reprova (e as sondas nao rodam)",
          not judge_with({"raw_to_png.py --readback": 1}, both)[0].passed()
          and judge_with({"raw_to_png.py --readback": 1}, both)[1].calls_of("image_probe.py") == [])
    check("fiacao: zero pares em um mecanismo reprova",
          not judge_with({}, {"printwindow": 1, "bitblt": 0})[0].passed())
    check("fiacao: dois pares em um mecanismo reprova (ambiguo)",
          not judge_with({}, {"printwindow": 2, "bitblt": 1})[0].passed())
    check("fiacao: comparador que sai 0 mas nao imprime o resumo reprova (saida nao entendida)",
          not judge_with({}, both, {"capture_vs_readback.py": "silencio"})[0].passed())


def scripted_mode(codes, pairs, tool_reply=(0, "tool ok\n")):
    with tempfile.TemporaryDirectory() as tmp, silent():
        config = make_config(tmp)
        run = scripted_run(codes, tool_writing(pairs), tool_reply, {"capture_vs_readback.py": GOOD_COMPARE})
        return run_mode(config, "off", run), run


# A coherent 2x2 frame, written by a tool double and judged by the REAL helpers. Top row
# (1,2,3,4)(5,6,7,8), bottom row (9,10,11,12)(13,14,15,16) as R,G,B,A; the readback holds the BOTTOM
# row first; the screen capture is XRGB8888 (B,G,R,X), top row first, tightly packed.
READBACK_RAW = bytes(range(9, 17)) + bytes(range(1, 9))
READBACK_META = "width=2\nheight=2\norigin=bottom_left\norder=rgba\n"
CAPTURE_META = "width=2\nheight=2\nstride=8\nformat=1\n"
CAPTURE_RAW = bytes([3, 2, 1, 0, 7, 6, 5, 0, 11, 10, 9, 0, 15, 14, 13, 0])
PROBES = "0 0 01020304\n1 1 0D0E0F10\n"


def fixture_arguments(command):
    """What follows `--` in the tool command: fixture, mode, readback directory, optional sabotage."""
    return command[command.index("--") + 1:]


def real_frame_tool(bitblt_raw=CAPTURE_RAW, readback_raw=READBACK_RAW):
    def effect(command):
        out = Path(command[command.index("--out") + 1])
        _, mode, readback = fixture_arguments(command)[:3]
        Path(readback).mkdir(parents=True, exist_ok=True)
        (Path(readback) / f"readback_{mode}.raw").write_bytes(readback_raw)
        (Path(readback) / f"readback_{mode}.meta").write_text(READBACK_META)
        for mechanism, raw in (("printwindow", CAPTURE_RAW), ("bitblt", bitblt_raw)):
            (out / mechanism).mkdir(parents=True, exist_ok=True)
            (out / mechanism / "conn1_surface1.raw").write_bytes(raw)
            (out / mechanism / "conn1_surface1.meta").write_text(CAPTURE_META)
    return effect


ABSENT_REPLY = (1, f"{ABSENCE_LINE}\nfixture exit=77\n")


class mode_aware_run:
    """off: the coherent tool double, real helpers. on: the fixture's declared absence."""

    def __init__(self, effect):
        self.effect = effect

    def __call__(self, command):
        if command[0] != "TOOL":
            return run_process(command)
        if fixture_arguments(command)[1] == "on":
            return ABSENT_REPLY
        self.effect(command)
        return 0, "tool ok\n"


def run_all_with_real_helpers(effect):
    with tempfile.TemporaryDirectory() as tmp, silent():
        config = make_config(tmp)
        config.probes_dir.mkdir()
        (config.probes_dir / "capture_known_color_probes_off.txt").write_text(PROBES)
        return run_all(config, mode_aware_run(effect))


def selftest_real_helpers_end_to_end():
    check("integracao: quadro coerente, ferramenta duble, auxiliares REAIS: passa (off roda, on ausente)",
          run_all_with_real_helpers(real_frame_tool()))
    altered = bytearray(CAPTURE_RAW)
    altered[2] ^= 0xFF  # the red byte of the top-left pixel of the BitBlt capture only
    check("integracao: um byte de RGB diferente na captura do BitBlt reprova",
          not run_all_with_real_helpers(real_frame_tool(bitblt_raw=bytes(altered))))
    drifted = bytearray(READBACK_RAW)
    drifted[7] ^= 0xFF  # the alpha of the bottom-right pixel (memory: bottom row first), probe (1,1)
    check("integracao: a leitura interna com alfa errado reprova nas sondas (o alfa da lib e provado AQUI)",
          not run_all_with_real_helpers(real_frame_tool(readback_raw=bytes(drifted))))


def selftest_process_level():
    with tempfile.TemporaryDirectory() as tmp:
        missing = [str(Path(tmp) / "no-such-tool.exe"), str(Path(tmp) / "no-such-fixture.exe"), str(Path(tmp) / "out")]
        proc = subprocess.run([sys.executable, str(Path(__file__).resolve()), *missing],
                              capture_output=True, text=True, check=False)
        check("processo: ferramenta inexistente sai 1 e imprime o resumo com os modos",
              proc.returncode == 1 and "modos=off on reprovados=2" in proc.stdout)
    proc = subprocess.run([sys.executable, str(Path(__file__).resolve())], capture_output=True, text=True, check=False)
    check("processo: sem argumentos sai 2", proc.returncode == 2)


def tool_writing_readback(mode="off"):
    def effect(command):
        _, _, readback = fixture_arguments(command)[:3]
        Path(readback).mkdir(parents=True, exist_ok=True)
        (Path(readback) / f"readback_{mode}.raw").write_bytes(READBACK_RAW)
        (Path(readback) / f"readback_{mode}.meta").write_text(READBACK_META)
    return effect


def selftest_tool_evidence_reaches_the_log():
    """The red of the plan is READ from the log: janelas=, visivel=, oclusao: and the absence line
    must reach the driver's own stdout, never stay in a variable."""
    evidence = "win32_window_capture: janelas=1 visivel=0 iconico=0\nwin32_window_capture: oclusao: OCLUIDA por classe=Progman\n"
    with tempfile.TemporaryDirectory() as tmp:
        captured = io.StringIO()
        with contextlib.redirect_stdout(captured):
            run_mode(make_config(tmp), "off", scripted_run(tool_reply=(2, evidence)))
        check("a saida da ferramenta (janelas=, visivel=, oclusao:) chega ao stdout do driver",
              "janelas=1 visivel=0 iconico=0" in captured.getvalue() and "OCLUIDA por classe=Progman" in captured.getvalue())
        captured = io.StringIO()
        with contextlib.redirect_stdout(captured):
            run_mode(make_config(tmp), "on", scripted_run(tool_reply=ABSENT_REPLY))
        check("a linha de ausencia declarada tambem chega ao stdout",
              ABSENCE_LINE in captured.getvalue())


def selftest_internal_reading_is_judged_even_when_the_tool_fails():
    """D-W8-36: the ruler must separate 'the internal reading is wrong' from 'the frame did not
    reach the system'. With the tool failing (exit 2: occluded window) the fixture has still written
    its readback BEFORE presenting, so the probes still run."""
    with tempfile.TemporaryDirectory() as tmp, silent():
        run = scripted_run(tool_effect=tool_writing_readback(), tool_reply=(2, "oclusao: OCLUIDA\n"))
        result = run_mode(make_config(tmp), "off", run)
        check("ferramenta saindo 2: o modo reprova, mas a leitura interna E julgada (sondas rodam)",
              not result.passed() and result.checks.get("tool") == 2 and result.checks.get("probes") == 0
              and len(run.calls_of("image_probe.py")) == 1 and run.calls_of("capture_vs_readback.py") == [])
    with tempfile.TemporaryDirectory() as tmp, silent():
        run = scripted_run(codes={"image_probe.py": 1}, tool_effect=tool_writing_readback(), tool_reply=(2, "x"))
        result = run_mode(make_config(tmp), "off", run)
        check("ferramenta saindo 2 e leitura interna errada: as duas causas aparecem separadas",
              result.checks.get("tool") == 2 and result.checks.get("probes") == 1)
    with tempfile.TemporaryDirectory() as tmp, silent():
        run = scripted_run(tool_reply=(1, "FIXTURE saiu antes de apresentar\n"))
        result = run_mode(make_config(tmp), "off", run)
        check("sem leitura interna gravada (a fixture nem chegou la): nada a julgar, so a ferramenta",
              result.checks == {"tool": 1} and run.calls_of("image_probe.py") == [])
    with tempfile.TemporaryDirectory() as tmp, silent():
        run = scripted_run(tool_effect=tool_writing_readback("on"), tool_reply=ABSENT_REPLY)
        result = run_mode(make_config(tmp), "on", run)
        check("modo on com ausencia declarada nao julga leitura nenhuma", result.status == "absent"
              and run.calls_of("image_probe.py") == [])


# The decided table (D-W8-45), written here by hand and NOT read from the driver's own copy, so that a
# swapped pair in the copy is caught; the C++ table is the source and check_sibling_lists.py keeps the
# copy equal to it.
DECIDED_VERDICTS = ((10, "janelas=0", "JANELAS"), (11, "INVISIVEL", "INVISIVEL"), (12, "ICONICA", "ICONICA"),
                    (13, "FORA DA TELA", "FORA_DA_TELA"), (14, "OCLUIDA por classe=Progman", "OCLUIDA"),
                    (15, "CAPTURA RECUSADA mecanismo=bitblt erro=5", "CAPTURA_RECUSADA"))


def selftest_verdict_codes():
    check("a copia da tabela do driver e a decidida, codigo por codigo",
          VERDICT_NAME_BY_CODE == {code: name for code, _, name in DECIDED_VERDICTS})
    for code, line, name in DECIDED_VERDICTS:
        check(f"codigo {code} com a linha 'veredito: {line}' e o veredito {name}",
              classify_tool_run("off", code, f"win32_window_capture: veredito: {line}\n") == "refused"
              and verdict_name(code) == name)
    check("o mesmo codigo vale no modo on",
          classify_tool_run("on", 11, "veredito: INVISIVEL\n") == "refused")


def selftest_code_and_line_must_agree():
    check("codigo conhecido com a linha de OUTRO veredito e incoerencia, nunca aceito",
          classify_tool_run("off", 11, "veredito: OCLUIDA por classe=Progman\n") == "incoherent"
          and classify_tool_run("off", 14, "veredito: INVISIVEL\n") == "incoherent")
    check("codigo conhecido SEM linha veredito e incoerencia",
          classify_tool_run("off", 11, "janelas=1 visivel=0\n") == "incoherent")
    check("vale a ULTIMA linha veredito (a primeira concordando nao salva)",
          classify_tool_run("off", 11, "veredito: INVISIVEL\nveredito: ICONICA\n") == "incoherent")
    check("o codigo 13 com a linha FORA_DA_TELA (com sublinhado) tambem e incoerencia: a linha tem espacos",
          classify_tool_run("off", 13, "veredito: FORA_DA_TELA\n") == "incoherent")


def selftest_unknown_and_reserved_codes():
    check("codigo fora da tabela e desconhecido (3, 9, 16, 255), nunca veredito",
          all(classify_tool_run("off", code, "veredito: INVISIVEL\n") == "unknown" for code in (3, 9, 16, 255)))
    check("2 nunca e aceito como veredito, mesmo com a linha",
          classify_tool_run("off", 2, "veredito: INVISIVEL\n") == "failed")
    check("1, 124 e 127 seguem sendo falha",
          all(classify_tool_run("off", code, "veredito: INVISIVEL\n") == "failed" for code in (1, 124, 127)))


def selftest_motivo_and_codigo_are_printed():
    with tempfile.TemporaryDirectory() as tmp:
        captured = io.StringIO()
        with contextlib.redirect_stdout(captured):
            run_all(make_config(tmp), scripted_run(tool_reply=(11, "veredito: INVISIVEL\n")))
        text = captured.getvalue()
        check("o driver imprime motivo=INVISIVEL codigo=11 na linha de veredito do modo",
              "verdict mode=off motivo=INVISIVEL codigo=11" in text)
        check("incoerencia imprime motivo=INCOERENTE com o codigo",
              "motivo=INCOERENTE codigo=11" in _run_all_text(tool_reply=(11, "veredito: ICONICA\n")))
        check("codigo desconhecido imprime motivo=CODIGO_DESCONHECIDO",
              "motivo=CODIGO_DESCONHECIDO codigo=3" in _run_all_text(tool_reply=(3, "x\n")))


def selftest_incoherent_and_unknown_reject_the_driver():
    """R3-1: a status that is not a pass must make run_all itself answer False, not only be printed."""
    with tempfile.TemporaryDirectory() as tmp, silent():
        incoherent = run_all(make_config(tmp), scripted_run(tool_reply=(11, "veredito: ICONICA\n")))
    with tempfile.TemporaryDirectory() as tmp, silent():
        unknown = run_all(make_config(tmp), scripted_run(tool_reply=(3, "veredito: INVISIVEL\n")))
    with tempfile.TemporaryDirectory() as tmp, silent():
        refused = run_all(make_config(tmp), scripted_run(tool_reply=(11, "veredito: INVISIVEL\n")))
    check("run_all reprova codigo e linha discordando (INCOERENTE), codigo desconhecido e veredito de captura",
          incoherent is False and unknown is False and refused is False)
    for status in ("incoherent", "unknown", "refused", "failed"):
        check(f"passed() e falso para o status {status}, mesmo com todas as verificacoes em zero",
              not mode_result("off", status, {"tool": 0}).passed())


def _run_all_text(tool_reply):
    with tempfile.TemporaryDirectory() as tmp:
        captured = io.StringIO()
        with contextlib.redirect_stdout(captured):
            run_all(make_config(tmp), scripted_run(tool_reply=tool_reply))
        return captured.getvalue()


def selftest_capture_verdict_exit():
    """D-W8-45: a capture verdict leaves with ITS OWN code (10 to 15), never the tool's own failure
    (2), the fixture's (1), a timeout (124) or an impossible run (127)."""
    with tempfile.TemporaryDirectory() as tmp, silent():
        pairs = tool_writing({"printwindow": 1})

        def effect(command):
            tool_writing_readback()(command)
            pairs(command)
        run = scripted_run(tool_effect=effect, tool_reply=(15, "veredito: CAPTURA RECUSADA mecanismo=bitblt erro=5\n"),
                           outputs={"capture_vs_readback.py": GOOD_COMPARE})
        result = run_mode(make_config(tmp), "off", run)
        compares = run.calls_of("capture_vs_readback.py")
        check("veredito de captura: o modo reprova, a leitura interna e julgada e o PrintWindow e comparado",
              result.status == "refused" and not result.passed() and result.checks.get("tool") == 15
              and result.checks.get("probes") == 0 and result.checks.get("compare_printwindow") == 0
              and len(compares) == 1 and compares[0][3].endswith("printwindow")
              and (result.motivo, result.codigo) == ("CAPTURA_RECUSADA", 15))
    with tempfile.TemporaryDirectory() as tmp, silent():
        run = scripted_run(tool_effect=tool_writing_readback(), tool_reply=(12, "veredito: ICONICA\n"))
        result = run_mode(make_config(tmp), "off", run)
        check("veredito de captura sem par de PrintWindow (iconica): nada a comparar, so a leitura interna",
              result.status == "refused" and "compare_printwindow" not in result.checks
              and run.calls_of("capture_vs_readback.py") == [])


def process_alive(pid):
    """Without ever signalling it: os.kill(pid, 0) TERMINATES the process on Windows."""
    if sys.platform == "win32":
        listing = subprocess.run(["tasklist", "/FI", f"PID eq {pid}"], capture_output=True, text=True, check=False)
        return str(pid) in listing.stdout
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        return True
    try:  # a zombie answers kill(0) but is dead; the process can also vanish between the two reads
        state = Path(f"/proc/{pid}/stat").read_text().split(") ")[-1].split()[0]
    except OSError:
        return False
    return state != "Z"


SLEEPER = "import time; time.sleep(20)"
def selftest_run_process():
    code, output = run_process([sys.executable, "-c", "import sys; print('hello'); sys.exit(3)"])
    check("run_process: devolve o codigo e a saida do filho", code == 3 and "hello" in output)
    started = time.monotonic()
    code, output = run_process([sys.executable, "-c", SLEEPER], timeout=0.5)
    check("run_process: filho que nao termina vira 124 dentro do prazo", code == 124 and time.monotonic() - started < 10)
    check("run_process: a mensagem do 124 nomeia o prazo", "timeout after 0.5 s" in output)
    code, output = run_process(["/nonexistent/no-such-tool.exe"])
    check("run_process: programa inexistente vira 127 (nunca 0, nunca excecao)", code == 127 and "cannot run" in output)
    with tempfile.TemporaryDirectory() as tmp:
        pid_file = Path(tmp) / "grandchild.pid"
        parent = ("import subprocess, sys, time; "
                  f"g = subprocess.Popen([sys.executable, '-c', {SLEEPER!r}]); "
                  f"open({str(pid_file)!r}, 'w').write(str(g.pid)); time.sleep(20)")
        # The risk the review named (A4): the child's own child inherits the pipe, so killing only the child
        # leaves the pipe open and a plain Windows subprocess.run keeps waiting for its EOF. The tree must die.
        started = time.monotonic()
        code, _ = run_process([sys.executable, "-c", parent], timeout=1.5)
        elapsed = time.monotonic() - started
        check(f"run_process: neto que segura o tubo nao pendura o executor (124 em {elapsed:.1f} s, teto 12 s)",
              code == 124 and elapsed < 12)
        grandchild = int(pid_file.read_text()) if pid_file.is_file() else 0
        deadline = time.monotonic() + 5
        while grandchild and process_alive(grandchild) and time.monotonic() < deadline:
            time.sleep(0.1)
        check("run_process: o prazo estourado mata a ARVORE (o neto nao fica vivo)",
              grandchild != 0 and not process_alive(grandchild))


class real_helpers_run:
    """Real helpers, and the tool double playing `effect` for any mode (exit 0)."""

    def __init__(self, effect):
        self.effect = effect

    def __call__(self, command):
        if command[0] != "TOOL":
            return run_process(command)
        self.effect(command)
        return 0, "tool ok\n"


def selftest_absence_needs_the_declared_line_and_an_exact_exit():
    only_exit = "fixture exit=77\n"
    check("on com fixture exit=77 mas SEM a linha de ausencia declarada falha",
          classify_tool_run("on", TOOL_FIXTURE_FAILED, only_exit) == "failed")
    check("on com a linha mas com fixture exit=7712 (prefixo de 77) falha",
          classify_tool_run("on", TOOL_FIXTURE_FAILED, f"{ABSENCE_LINE}\nfixture exit=7712\n") == "failed")
    check("on com a linha e o fim de linha certo, com prefixo da ferramenta, e ausencia",
          classify_tool_run("on", TOOL_FIXTURE_FAILED,
                            f"{ABSENCE_LINE}\nwin32_window_capture: fixture exit=77\n") == "absent")


def selftest_each_mode_uses_its_own_probe_file():
    """off's file is WRONG for this frame and on's is right: only a run that reads the file of ITS
    mode can pass (a driver that always read the off file would reject a good `on`)."""
    with tempfile.TemporaryDirectory() as tmp, silent():
        config = make_config(tmp)
        config.probes_dir.mkdir()
        (config.probes_dir / "capture_known_color_probes_off.txt").write_text("0 0 FFFFFFFF\n")
        (config.probes_dir / "capture_known_color_probes_on.txt").write_text(PROBES)
        result = run_mode(config, "on", real_helpers_run(real_frame_tool()))
        check("modo on que roda e julgado com o arquivo de sondas do on (e passa)",
              result.status == "ran" and result.passed())
        result = run_mode(config, "off", real_helpers_run(real_frame_tool()))
        check("modo off que roda e julgado com o arquivo do off (e reprova com ele errado)",
              result.status == "ran" and result.checks.get("probes") == 1)


def selftest_main():
    selftest_classification()
    selftest_clean_output()
    selftest_exact_modes()
    selftest_measured_lines()
    selftest_title_matches_the_fixture()
    selftest_tool_command()
    selftest_tool_receives_the_driver_budgets()
    selftest_budgets_are_coherent()
    selftest_occlude_reaches_only_the_tool()
    selftest_count_pairs()
    selftest_wiring_all_clean()
    selftest_wiring_each_part_counts()
    selftest_real_helpers_end_to_end()
    selftest_tool_evidence_reaches_the_log()
    selftest_internal_reading_is_judged_even_when_the_tool_fails()
    selftest_capture_verdict_exit()
    selftest_verdict_codes()
    selftest_code_and_line_must_agree()
    selftest_unknown_and_reserved_codes()
    selftest_motivo_and_codigo_are_printed()
    selftest_incoherent_and_unknown_reject_the_driver()
    selftest_run_process()
    selftest_absence_needs_the_declared_line_and_an_exact_exit()
    selftest_each_mode_uses_its_own_probe_file()
    selftest_process_level()
    print(f"selftest: {len(CHECKS)} controles OK")


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
        selftest_main()
    else:
        sys.exit(real_main(args))


if __name__ == "__main__":
    main()
