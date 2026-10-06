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
#      failed, 2 = the tool could not vouch for the capture);
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
# --sabotage is the debut proof (L-36): the fixture draws a wrong frame on purpose and the verdict
# MUST be a rejection; this script does not invert it. Exit 0 only when every mode passed.
#
# WHAT THIS DOES NOT PROVE (declared, L-43): the alpha of the SCREEN capture (absent by
# construction, counted); ACTIVATION and FOCUS (the tool never reads them); DPI scaling other than
# 96 (the runner's); the composition on a real GPU (the runner draws with Mesa llvmpipe).

import contextlib
import io
import re
import shutil
import subprocess
import sys
import tempfile
from dataclasses import dataclass, field
from pathlib import Path

SCRIPT_NAME = "run_capture_known_color_win32.py"
TOOLS_DIR = Path(__file__).resolve().parent
REPO_ROOT = TOOLS_DIR.parent.parent
MODES = ("off", "on")
# The title the fixture gives its window. A copy of a fact the fixture owns, so a selftest control
# reads the fixture's source and fails when the two drift (feedback_copia_em_vez_de_fonte).
WINDOW_TITLE = "janela da fumaca de cor conhecida"
FIXTURE_SOURCES = (REPO_ROOT / "tests" / "parity" / "capture_known_color_smoke.cpp",
                   REPO_ROOT / "tests" / "container" / "capture_known_color_smoke.cpp")
ABSENCE_LINE = "AUSENCIA DECLARADA srgb_framebuffer=on"
ABSENCE_FIXTURE_EXIT = "fixture exit=77"
TOOL_FIXTURE_FAILED = 1
PROCESS_TIMEOUT_SECONDS = 180
MECHANISMS = ("printwindow", "bitblt")
COMPARATOR_LINE = re.compile(r"pixels=(\d+) differing=(\d+) alpha_not_compared=(\d+)")


@dataclass
class run_config:
    tool: str
    fixture: str
    out_dir: Path
    sabotage: str = ""
    probes_dir: Path = REPO_ROOT / "tests" / "fixtures"


@dataclass
class mode_result:
    mode: str
    status: str  # "ran", "absent" or "failed"
    checks: dict = field(default_factory=dict)  # name -> exit code
    alpha_compared: int = 0

    def passed(self):
        return self.status == "absent" or (self.status == "ran" and all(rc == 0 for rc in self.checks.values()))


def run_process(args):
    """The real executor: (exit code, stdout+stderr). A hung child is a failure, never a hang."""
    try:
        done = subprocess.run([str(part) for part in args], capture_output=True, text=True,
                              errors="replace", timeout=PROCESS_TIMEOUT_SECONDS, check=False)
    except subprocess.TimeoutExpired:
        return 124, f"timeout after {PROCESS_TIMEOUT_SECONDS} s: {args[0]}"
    except OSError as error:
        return 127, f"cannot run {args[0]}: {error}"
    return done.returncode, done.stdout + done.stderr


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
    directory = mode_dir(config, mode)
    fixture = [config.fixture, mode, directory / "readback"] + ([config.sabotage] if config.sabotage else [])
    return [config.tool, "--title", WINDOW_TITLE, "--out", directory / "capture", "--", *fixture]


def classify_tool_run(mode, code, output):
    """"ran" (the tool exited 0), "absent" (mode on, the fixture declared the sRGB absence and exited
    77: the tool then reports its own exit 1 for a fixture that never presented) or "failed"."""
    if code == 0:
        return "ran"
    declared = mode == "on" and code == TOOL_FIXTURE_FAILED and ABSENCE_LINE in output \
        and ABSENCE_FIXTURE_EXIT in output
    return "absent" if declared else "failed"


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


def judge_ran_mode(config, mode, run):
    """The host-side checks over what the tool left. Returns the mode's result."""
    result = mode_result(mode, "ran")
    result.checks["pairs"] = check_capture_pairs(mode_dir(config, mode) / "capture")
    result.checks["readback_png"] = convert_readback_png(config, mode, run)
    result.checks["probes"] = probe_readback_png(config, mode, run) if result.checks["readback_png"] == 0 else 1
    for mechanism in MECHANISMS:
        result.checks[f"compare_{mechanism}"], compared = compare_mechanism(config, mode, mechanism, run)
        result.alpha_compared += compared
        result.checks[f"png_{mechanism}"] = convert_capture_png(config, mode, mechanism, run)
    return result


def run_mode(config, mode, run):
    clean_mode_output(config, mode)
    (mode_dir(config, mode) / "readback").mkdir(parents=True)
    code, output = run(tool_command(config, mode))
    status = classify_tool_run(mode, code, output)
    print(f"{SCRIPT_NAME}: mode={mode} tool={code} status={status}")
    if status != "ran":
        return mode_result(mode, status, {"tool": code} if status == "failed" else {})
    return judge_ran_mode(config, mode, run)


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
        print(f"{SCRIPT_NAME}: verdict mode={result.mode} status={result.status} "
              f"checks={result.checks} {'OK' if result.passed() else 'REPROVADO'}")
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
    if len(args) != 3:
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
    absent_text = f"... {ABSENCE_LINE} ...\n{ABSENCE_FIXTURE_EXIT}\n"
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
    sources = [path for path in FIXTURE_SOURCES if path.is_file()]
    check("o fonte da fixture existe em um dos dois caminhos conhecidos (piso, L-40)", len(sources) >= 1)
    check("o titulo que o driver passa e o que a fixture da a janela",
          all(f'"{WINDOW_TITLE}"' in path.read_text(encoding="utf-8", errors="replace") for path in sources))


def selftest_tool_command():
    with tempfile.TemporaryDirectory() as tmp:
        config = make_config(tmp)
        command = tool_command(config, "off")
        check("comando da ferramenta: titulo, saida e a linha da fixture depois de --",
              command[0] == "TOOL" and command[command.index("--title") + 1] == WINDOW_TITLE
              and command[command.index("--out") + 1] == config.out_dir / "off" / "capture"
              and command[command.index("--") + 1:] == ["FIXTURE", "off", config.out_dir / "off" / "readback"])
        config.sabotage = "swap_red_blue"
        check("sabotagem vai por ultimo, para a fixture", tool_command(config, "on")[-1] == "swap_red_blue")


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


def selftest_main():
    selftest_classification()
    selftest_clean_output()
    selftest_exact_modes()
    selftest_measured_lines()
    selftest_title_matches_the_fixture()
    selftest_tool_command()
    selftest_count_pairs()
    selftest_wiring_all_clean()
    selftest_wiring_each_part_counts()
    selftest_real_helpers_end_to_end()
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
