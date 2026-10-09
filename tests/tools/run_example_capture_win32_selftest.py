# SPDX-License-Identifier: AGPL-3.0-or-later
#
# run_example_capture_win32_selftest.py - the controls of run_example_capture_win32.py, loaded only by that driver's
# own --selftest (CAPTURE-DRIVER-SPLIT, D-W8-178, D-W8-185: a driver's controls live beside it). A pure move: the same
# names, the same count. Standard library only (L-07).

from pathlib import Path
from run_example_capture_win32 import *  # the driver's names, unqualified, as before the move

DRIVER_SCRIPT = Path(__file__).resolve().parent / "run_example_capture_win32.py"


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
        stale_png = Path(tmp) / "out" / "first_window" / "png" / "printwindow" / "velho.png"
        stale_png.parent.mkdir(parents=True)
        stale_png.write_bytes(b"x")
        neighbour = Path(tmp) / "out" / "outro" / "fica.txt"
        neighbour.parent.mkdir(parents=True)
        neighbour.write_text("x")
        with captured_output():
            run_example(config, double_run(None, FAILED_REPLY))
        check("a saida velha de first_window/ (capture/ e png/) e apagada antes da ferramenta, e o vizinho em "
              "<out>/ fica", not stale.exists() and not stale_png.exists() and neighbour.exists())


def selftest_budgets_fit():
    check("os dois orcamentos da ferramenta e a folga de morte cabem no prazo do processo",
          PRESENT_BUDGET_MS + EXIT_BUDGET_MS + KILL_GRACE_SECONDS * 1000 <= PROCESS_TIMEOUT_SECONDS * 1000)


def selftest_process_level():
    script = str(DRIVER_SCRIPT)
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


def selftest_decided_scene_constants():
    check("as constantes decididas da cena: 12 sondas estaticas e captura de 640x480 (D-W8-72)",
          PROBE_COUNT == 12 and CAPTURE_SIZE == (640, 480))


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
    selftest_decided_scene_constants()
    print(f"selftest: {len(CHECKS)} controles OK")
