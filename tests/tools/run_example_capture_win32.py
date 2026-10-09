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


# -- selftest ---------------------------------------------------------
CHECKS = []


def check(name, condition):
    CHECKS.append(name)
    if not condition:
        print(f"selftest: {name} FALHOU", file=sys.stderr)
        sys.exit(1)


# A coherent 2x2 frame as the tool writes it: XRGB8888 (B, G, R, X), top row first, tightly packed, the X
# byte 0. Pixels (R, G, B): (1,2,3) (5,6,7) / (9,10,11) (13,14,15). raw_to_png.py gives them alpha 255.
CAPTURE_RAW = bytes([3, 2, 1, 0, 7, 6, 5, 0, 11, 10, 9, 0, 15, 14, 13, 0])
CAPTURE_META = "width=2\nheight=2\nstride=8\nformat=1\n"
WIDE_META = "width=3\nheight=2\nstride=12\nformat=1\n"
TALL_META = "width=2\nheight=3\nstride=8\nformat=1\n"
PROBES = "0 0 010203FF\n1 1 0D0E0FFF\n"
MEASURED_TEXT = "MEASURED first_window.capture_alpha_compared=0"
READY_REPLY = (0, "win32_window_capture: veredito: 5 pontos da propria janela\nwin32_window_capture: exit=0\n")
FAILED_REPLY = (2, "win32_window_capture: RECUSADO sem janela\nwin32_window_capture: exit=2\n")


@contextlib.contextmanager
def captured_output():
    """The driver's stdout and stderr, captured: a selftest never prints MEASURED (collect_measured.py reads
    every test's log, and a selftest's line would pass for a real measurement)."""
    out, err = io.StringIO(), io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
        yield out, err


class double_run:
    """A run() double: plays the tool by calling `tool_effect(command)` (which writes what the tool would)
    and answering `tool_reply`; runs the REAL helpers (raw_to_png.py, image_probe.py) by run_process.
    Records every command."""

    def __init__(self, tool_effect=None, tool_reply=READY_REPLY):
        self.tool_effect = tool_effect or (lambda command: None)
        self.tool_reply = tool_reply
        self.calls = []

    def __call__(self, command):
        self.calls.append([str(part) for part in command])
        if command[0] == "TOOL":
            self.tool_effect(command)
            return self.tool_reply
        return run_process(command)

    def calls_of(self, script):
        return [call for call in self.calls if call[0] != "TOOL" and Path(call[1]).name == script]


def frame_tool(raws=None, metas=None, counts=None):
    """A tool double that writes, under --out, counts[m] pairs (default 1) per mechanism m, each with
    raws[m] and metas[m] (default: the coherent 2x2 frame)."""
    def effect(command):
        out = Path(command[command.index("--out") + 1])
        for mechanism in MECHANISMS:
            directory = out / mechanism
            directory.mkdir(parents=True, exist_ok=True)
            for index in range((counts or {}).get(mechanism, 1)):
                (directory / f"conn1_surface{index + 1}.raw").write_bytes((raws or {}).get(mechanism, CAPTURE_RAW))
                (directory / f"conn1_surface{index + 1}.meta").write_text((metas or {}).get(mechanism, CAPTURE_META))
    return effect


def altered_raw():
    """The coherent frame with the red byte of the top-left pixel flipped."""
    altered = bytearray(CAPTURE_RAW)
    altered[2] ^= 0xFF
    return bytes(altered)


def make_config(root, probes=PROBES, probe_count=2):
    """A config over a temporary root, with the 2x2 frame's size; probes None leaves the file absent."""
    probes_file = Path(root) / "probes.txt"
    if probes is not None:
        probes_file.write_text(probes)
    return run_config(tool="TOOL", example="EXAMPLE", out_dir=Path(root) / "out", probes_file=probes_file,
                      probe_count=probe_count, capture_size=(2, 2))


@dataclass
class judged:
    passed: bool
    run: double_run
    out: str
    err: str


def judge(tool_effect, tool_reply=READY_REPLY, probes=PROBES, probe_count=2):
    """One whole run of the driver over a tool double and the REAL helpers, its output captured."""
    with tempfile.TemporaryDirectory() as tmp, captured_output() as (out, err):
        run = double_run(tool_effect, tool_reply)
        passed = run_example(make_config(tmp, probes, probe_count), run)
    return judged(passed, run, out.getvalue(), err.getvalue())


def verdict_line_is(text, start, end):
    """True when some line of `text` starts with `start` and ends with `end`."""
    return any(line.startswith(start) and line.endswith(end) for line in text.splitlines())


def selftest_tool_command():
    with tempfile.TemporaryDirectory() as tmp:
        command = tool_command(make_config(tmp))
        separator = command.index("--")
        check("comando: titulo 'GlintFx example: first window' e --ready-line 'first_window: first frame "
              "presented', cada um uma vez, antes do --",
              command[0] == "TOOL" and command.count("--title") == 1 and command.count("--ready-line") == 1
              and command[command.index("--title") + 1] == "GlintFx example: first window"
              and command[command.index("--ready-line") + 1] == "first_window: first frame presented"
              and command.index("--title") < separator and command.index("--ready-line") < separator)
        check("comando: depois do -- vai so o exemplo, sem --frames (ele roda ate o WM_CLOSE da ferramenta)",
              command[separator + 1:] == ["EXAMPLE"])
        check("comando: a saida e <out>/first_window/capture e os orcamentos 30000 e 10000 vao antes do --",
              command[command.index("--out") + 1] == Path(tmp) / "out" / "first_window" / "capture"
              and command[command.index("--present-budget-ms") + 1] == "30000"
              and command[command.index("--exit-budget-ms") + 1] == "10000"
              and command.index("--present-budget-ms") < separator
              and command.index("--exit-budget-ms") < separator)


def selftest_grammar_text():
    text = OPTIONS_SOURCE.read_text(encoding="utf-8", errors="replace")
    check("o texto das cinco opcoes que o driver passa existe no fonte da gramatica da ferramenta (presenca "
          "de texto; o uso real so a D6 prova, no Windows)",
          all(f'"{option}"' in text for option in ("--title", "--out", "--present-budget-ms",
                                                    "--exit-budget-ms", "--ready-line")))


def selftest_classification():
    ready = "win32_window_capture: veredito: 5 pontos da propria janela\n"
    check("saida 0 com a linha de prontidao da ferramenta roda", classify_tool_run(0, ready) == "ran")
    check("saida 0 cuja ULTIMA linha veredito e uma recusa e INCOERENTE",
          classify_tool_run(0, ready + "win32_window_capture: veredito: OCLUIDA por classe=X\n") == "incoherent")
    check("saida 0 sem nenhuma linha veredito e INCOERENTE",
          classify_tool_run(0, "win32_window_capture: exit=0\n") == "incoherent")
    check("saida 14 com a linha OCLUIDA e recusa; saida 14 com a linha INVISIVEL e INCOERENTE",
          classify_tool_run(14, "win32_window_capture: veredito: OCLUIDA pontos=5\n") == "refused"
          and classify_tool_run(14, "win32_window_capture: veredito: INVISIVEL\n") == "incoherent")
    check("saidas 1, 2, 124 e 127 sao falha; 3 e 99 sao codigo desconhecido",
          all(classify_tool_run(code, "") == "failed" for code in (1, 2, 124, 127))
          and classify_tool_run(3, "") == "unknown" and classify_tool_run(99, "") == "unknown")
    check("motivo: PRONTA, o nome do veredito na recusa, INCOERENTE, FALHA_DE_EXEMPLO_OU_FERRAMENTA e "
          "CODIGO_DESCONHECIDO",
          motivo_of("ran", 0) == "PRONTA" and motivo_of("refused", 14) == "OCLUIDA"
          and motivo_of("incoherent", 0) == "INCOERENTE"
          and motivo_of("failed", 2) == "FALHA_DE_EXEMPLO_OU_FERRAMENTA"
          and motivo_of("unknown", 3) == "CODIGO_DESCONHECIDO")


def selftest_coherent_frame():
    good = judge(frame_tool())
    check("integracao: quadro XRGB coerente, ferramenta duble, auxiliares REAIS: passa (raw_to_png e "
          "image_probe encadeados dao alfa 255 com o byte X em 0)", good.passed)
    check("a linha MEASURED do alfa sai uma vez quando passa", good.out.splitlines().count(MEASURED_TEXT) == 1)
    check("o veredito do driver sai quando passa: exemplo=first_window motivo=PRONTA codigo=0, e OK",
          verdict_line_is(good.out, "run_example_capture_win32.py: verdict exemplo=first_window motivo=PRONTA "
                          "codigo=0 status=ran", " OK"))
    converted = good.run.calls_of("raw_to_png.py")
    check("grava em <out>/first_window/: le capture/<mecanismo>, escreve png/<mecanismo>, PrintWindow e depois "
          "BitBlt, e as sondas rodam nos dois",
          [Path(call[2]).parts[-3:] for call in converted]
          == [("first_window", "capture", "printwindow"), ("first_window", "capture", "bitblt")]
          and [Path(call[3]).parts[-3:] for call in converted]
          == [("first_window", "png", "printwindow"), ("first_window", "png", "bitblt")]
          and len(good.run.calls_of("image_probe.py")) == 2)


def selftest_alpha_and_colors():
    check("integracao: a mesma sonda com alfa 00 reprova (o alfa FF do quadro coerente nao passa de graca)",
          not judge(frame_tool(), probes="0 0 01020300\n1 1 0D0E0F00\n").passed)
    check("integracao: um byte de cor errado so na captura do PrintWindow reprova",
          not judge(frame_tool(raws={"printwindow": altered_raw()})).passed)
    check("integracao: um byte de cor errado so na captura do BitBlt reprova",
          not judge(frame_tool(raws={"bitblt": altered_raw()})).passed)


def selftest_pairs():
    two = judge(frame_tool(counts={"bitblt": 2}))
    check("dois pares no BitBlt reprovam, e nenhum auxiliar roda",
          not two.passed and two.run.calls_of("raw_to_png.py") == [] and two.run.calls_of("image_probe.py") == [])
    check("o FAIL de pares cita quem chamou: run_example_capture_win32.py: FAIL bitblt",
          any(line.startswith("run_example_capture_win32.py: FAIL bitblt") for line in two.err.splitlines()))
    check("zero pares no PrintWindow reprova", not judge(frame_tool(counts={"printwindow": 0})).passed)


def selftest_size_before_probes():
    wide = judge(frame_tool(raws={"bitblt": bytes(24)}, metas={"bitblt": WIDE_META}))
    probed = wide.run.calls_of("image_probe.py")
    check("tamanho 3x2 no BitBlt reprova ANTES de sonda: o image_probe roda so para o PrintWindow",
          not wide.passed and len(probed) == 1 and "printwindow" in Path(probed[0][2]).parts)
    tall = judge(frame_tool(raws={"bitblt": bytes(24)}, metas={"bitblt": TALL_META}))
    check("tamanho 2x3 no BitBlt tambem reprova antes de sonda (largura E altura)",
          not tall.passed and len(tall.run.calls_of("image_probe.py")) == 1)


def selftest_probe_file():
    check("arquivo de sondas ausente reprova", not judge(frame_tool(), probes=None).passed)
    check("arquivo de sondas sem nenhuma sonda reprova", not judge(frame_tool(), probes="# nenhuma\n").passed)
    check("menos sondas que as fixadas reprova (2 no arquivo, 3 esperadas)",
          not judge(frame_tool(), probe_count=3).passed)
    check("mais sondas que as fixadas reprova (2 no arquivo, 1 esperada)",
          not judge(frame_tool(), probe_count=1).passed)


def selftest_failures_still_report():
    failed = judge(None, tool_reply=FAILED_REPLY)
    check("ferramenta que falha (saida 2) reprova, e a linha MEASURED do alfa sai uma vez mesmo assim",
          not failed.passed and failed.out.splitlines().count(MEASURED_TEXT) == 1)
    check("o veredito do driver sai quando falha: motivo=FALHA_DE_EXEMPLO_OU_FERRAMENTA codigo=2, e REPROVADO",
          verdict_line_is(failed.out, "run_example_capture_win32.py: verdict exemplo=first_window "
                          "motivo=FALHA_DE_EXEMPLO_OU_FERRAMENTA codigo=2 status=failed", " REPROVADO"))
    incoherent = judge(frame_tool(), tool_reply=(0, "win32_window_capture: veredito: OCLUIDA por classe=X\n"))
    check("saida 0 com linha de recusa reprova o driver mesmo com as capturas perfeitas, sem rodar auxiliar",
          not incoherent.passed and incoherent.run.calls_of("raw_to_png.py") == [])


def selftest_stale_output():
    with tempfile.TemporaryDirectory() as tmp:
        config = make_config(tmp)
        stale = Path(tmp) / "out" / "first_window" / "capture" / "printwindow" / "conn9_surface1.raw"
        stale.parent.mkdir(parents=True)
        stale.write_bytes(b"x")
        neighbour = Path(tmp) / "out" / "outro" / "fica.txt"
        neighbour.parent.mkdir(parents=True)
        neighbour.write_text("x")
        with captured_output():
            run_example(config, double_run(None, FAILED_REPLY))
        check("a saida velha de first_window/ e apagada antes da ferramenta, e o vizinho em <out>/ fica",
              not stale.exists() and neighbour.exists())


def selftest_budgets_fit():
    check("os dois orcamentos da ferramenta e a folga de morte cabem no prazo do processo",
          PRESENT_BUDGET_MS + EXIT_BUDGET_MS + KILL_GRACE_SECONDS * 1000 <= PROCESS_TIMEOUT_SECONDS * 1000)


def selftest_process_level():
    script = str(Path(__file__).resolve())
    with tempfile.TemporaryDirectory() as tmp:
        missing = [str(Path(tmp) / "no-such-tool.exe"), str(Path(tmp) / "no-such-example.exe"), str(Path(tmp) / "out")]
        proc = subprocess.run([sys.executable, script, *missing], capture_output=True, text=True, check=False)
        check("processo: ferramenta inexistente sai 1, com motivo=FALHA_DE_EXEMPLO_OU_FERRAMENTA codigo=127 e a "
              "linha MEASURED uma vez",
              proc.returncode == 1 and "motivo=FALHA_DE_EXEMPLO_OU_FERRAMENTA codigo=127 " in proc.stdout
              and proc.stdout.splitlines().count(MEASURED_TEXT) == 1)
    no_args = subprocess.run([sys.executable, script], capture_output=True, text=True, check=False)
    empty_arg = subprocess.run([sys.executable, script, "T", "E", ""], capture_output=True, text=True, check=False)
    check("processo: sem argumentos, ou com um argumento vazio, sai 2",
          no_args.returncode == 2 and empty_arg.returncode == 2)


def selftest_main():
    selftest_tool_command()
    selftest_grammar_text()
    selftest_classification()
    selftest_coherent_frame()
    selftest_alpha_and_colors()
    selftest_pairs()
    selftest_size_before_probes()
    selftest_probe_file()
    selftest_failures_still_report()
    selftest_stale_output()
    selftest_budgets_fit()
    selftest_process_level()
    print(f"selftest: {len(CHECKS)} controles OK")


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
        selftest_main()
    else:
        sys.exit(real_main(args))


if __name__ == "__main__":
    main()
