# SPDX-License-Identifier: AGPL-3.0-or-later
#
# run_capture_known_color_win32_selftest.py - the controls of run_capture_known_color_win32.py, loaded
# only by that driver's
# own --selftest (CAPTURE-DRIVER-SPLIT, D-W8-178, D-W8-185: a driver's controls live beside it). A pure move: the same
# names, the same count. Standard library only (L-07).

from pathlib import Path
from run_capture_known_color_win32 import *  # the driver's names, unqualified, as before the move

DRIVER_SCRIPT = Path(__file__).resolve().parent / "run_capture_known_color_win32.py"


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
        check("comando da ferramenta: titulo, saida e os dois orcamentos dela antes de --, "
              "a linha da fixture depois, com o orcamento de espera",
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
    options_text = OPTIONS_SOURCE.read_text(encoding="utf-8", errors="replace")
    check("a gramatica que o driver usa e a que a ferramenta aceita: --present-budget-ms e --exit-budget-ms",
          '"--present-budget-ms"' in options_text and '"--exit-budget-ms"' in options_text)


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
    check("I-2: apresentacao de 0, 1000 e 5000 ms reprova (abaixo do piso: teto medido de 1,21 s do teste "
          "inteiro, com margem)",
          not any(budgets_are_coherent(replace(real, present_ms=value)) for value in (0, 1000, 5000)))
    check("I-2: saida de 0, 1000 e 5000 ms reprova (abaixo do piso: teto medido de 1,21 s do teste "
          "inteiro, com margem)",
          not any(budgets_are_coherent(replace(real, exit_ms=value)) for value in (0, 1000, 5000)))
    check("I-2: folga de morte de 0 s reprova (sem tempo para o pipe fechar depois do kill)",
          not budgets_are_coherent(replace(real, kill_grace_s=0)))
    check("I-2: o piso de 1 deixa os valores baixos passarem, e por isso os controles acima os matam",
          budgets_are_coherent(replace(real, present_ms=0, present_floor_ms=0))
          and budgets_are_coherent(replace(real, exit_ms=0, exit_floor_ms=0))
          and budgets_are_coherent(replace(real, kill_grace_s=0, grace_floor_s=0)))
    check("I-2: cada piso e' inclusivo (o valor igual ao piso passa, um a menos reprova)",
          budgets_are_coherent(replace(real, present_ms=real.present_floor_ms))
          and not budgets_are_coherent(replace(real, present_ms=real.present_floor_ms - 1))
          and budgets_are_coherent(replace(real, exit_ms=real.exit_floor_ms))
          and not budgets_are_coherent(replace(real, exit_ms=real.exit_floor_ms - 1))
          and budgets_are_coherent(replace(real, kill_grace_s=real.grace_floor_s))
          and not budgets_are_coherent(replace(real, kill_grace_s=real.grace_floor_s - 1)))
    # The timeout is in whole seconds, so the exact fit is the sum rounded UP to a second: it holds
    # whatever the millisecond budgets are, even one that is not a multiple of 1000 (C-5 of the review).
    total_ms = real.present_ms + real.hold_ms + real.exit_ms + real.kill_grace_s * 1000
    fits_s = -(-total_ms // 1000)
    check("a soma que fecha no prazo (arredondada para cima ao segundo) passa, um segundo a menos reprova",
          budgets_are_coherent(replace(real, timeout_s=fits_s))
          and not budgets_are_coherent(replace(real, timeout_s=fits_s - 1)))
    check("o arredondamento vale para um orcamento que nao e' multiplo de 1000 (30500 ms)",
          budgets_are_coherent(replace(real, present_ms=30500, timeout_s=-(-(total_ms + 500) // 1000)))
          and not budgets_are_coherent(replace(real, present_ms=30500, timeout_s=-(-(total_ms + 500) // 1000) - 1)))


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


def selftest_check_capture_pairs_names_the_caller():
    # An empty directory holds no pair, so the first mechanism is a FAIL. The check must say who
    # asked: every FAIL line starts with the caller's name, and at least one line exists (a check
    # that printed nothing would pass this comparison for free).
    with tempfile.TemporaryDirectory() as tmp:
        stderr = io.StringIO()
        with contextlib.redirect_stderr(stderr):
            code = check_capture_pairs(Path(tmp), "quem_chama_teste")
        lines = [line for line in stderr.getvalue().splitlines() if line]
        check("check_capture_pairs cita quem chamou em cada linha de FAIL",
              code == 1 and len(lines) >= 1
              and all(line.startswith("quem_chama_teste: FAIL") for line in lines))


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
        key = Path(str(command[1])).name + (
            " " + str(command[2])
            if len(command) > 2 and str(command[2]).startswith("--") and "raw_to_png" in str(command[1])
            else ""
        )
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
        proc = subprocess.run([sys.executable, str(DRIVER_SCRIPT), *missing],
                              capture_output=True, text=True, check=False)
        check("processo: ferramenta inexistente sai 1 e imprime o resumo com os modos",
              proc.returncode == 1 and "modos=off on reprovados=2" in proc.stdout)
    proc = subprocess.run([sys.executable, str(DRIVER_SCRIPT)], capture_output=True, text=True, check=False)
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
    evidence = ("win32_window_capture: janelas=1 visivel=0 iconico=0\n"
                "win32_window_capture: oclusao: OCLUIDA por classe=Progman\n")
    with tempfile.TemporaryDirectory() as tmp:
        captured = io.StringIO()
        with contextlib.redirect_stdout(captured):
            run_mode(make_config(tmp), "off", scripted_run(tool_reply=(2, evidence)))
        check("a saida da ferramenta (janelas=, visivel=, oclusao:) chega ao stdout do driver",
              "janelas=1 visivel=0 iconico=0" in captured.getvalue()
              and "OCLUIDA por classe=Progman" in captured.getvalue())
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


# The decided table (D-W8-45), written here by hand and NOT read from the copy in win32_capture_protocol.py,
# so that a swapped pair in the copy is caught; the C++ table is the source and check_sibling_lists.py
# keeps the copy equal to it.
DECIDED_VERDICTS = ((10, "janelas=0", "JANELAS"), (11, "INVISIVEL", "INVISIVEL"), (12, "ICONICA", "ICONICA"),
                    (13, "FORA DA TELA", "FORA_DA_TELA"), (14, "OCLUIDA por classe=Progman", "OCLUIDA"),
                    (15, "CAPTURA RECUSADA mecanismo=bitblt erro=5", "CAPTURA_RECUSADA"))


def selftest_verdict_codes():
    check("a copia da tabela do modulo e a decidida, codigo por codigo",
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
    check("run_process: filho que nao termina vira 124 dentro do prazo",
          code == 124 and time.monotonic() - started < 10)
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
    selftest_check_capture_pairs_names_the_caller()
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
