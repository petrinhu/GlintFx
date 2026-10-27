# SPDX-License-Identifier: AGPL-3.0-or-later
#
# run_example_capture_win32_selftest.py - the controls of run_example_capture_win32.py, loaded only by that driver's
# own --selftest (CAPTURE-DRIVER-SPLIT, D-W8-178, D-W8-185: a driver's controls live beside it). Standard library only
# (L-07). R2D-SHAPES S6 (D-W8-1081): the table of examples, its probe files, its sources and the command line of the
# second example have controls here.

from pathlib import Path
import ast
from run_example_capture_win32 import *  # the driver's names, unqualified, as before the move
from image_probe import ProbeError, parse_probes
from selftest_controls import CHECKS, check

DRIVER_SCRIPT = Path(__file__).resolve().parent / "run_example_capture_win32.py"

# Each example's source: the control below reads its title and its ready line there (D-W8-127).
def example_source(name):
    return REPO_ROOT / "examples" / name / f"{name}.cpp"


# -- selftest ---------------------------------------------------------





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


def make_config(root, probes=PROBES, probe_count=2, name="first_window"):
    """A config of the table's `name` over a temporary root, with the 2x2 frame's size; probes None leaves the file
    absent."""
    probes_file = Path(root) / "probes.txt"
    if probes is not None:
        probes_file.write_text(probes)
    facts = EXAMPLES[name]
    return run_config(name=name, tool="TOOL", example="EXAMPLE", out_dir=Path(root) / "out",
                      window_title=facts.window_title, ready_line=facts.ready_line, probes_file=probes_file,
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
        shapes = tool_command(make_config(tmp, name="shapes"))
        check("comando do shapes: titulo 'GlintFx example: shapes', --ready-line 'shapes: first frame presented' e a "
              "saida <out>/shapes/capture, pela linha da tabela",
              shapes[shapes.index("--title") + 1] == "GlintFx example: shapes"
              and shapes[shapes.index("--ready-line") + 1] == "shapes: first frame presented"
              and shapes[shapes.index("--out") + 1] == Path(tmp) / "out" / "shapes" / "capture")


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
        missing = [str(Path(tmp) / "no-such-tool.exe"), "first_window", str(Path(tmp) / "no-such-example.exe"),
                   str(Path(tmp) / "out")]
        proc = subprocess.run([sys.executable, script, *missing], capture_output=True, text=True, check=False)
        check("processo: ferramenta inexistente sai 1, com motivo=FALHA_DE_EXEMPLO_OU_FERRAMENTA codigo=127 e a "
              "linha MEASURED uma vez",
              proc.returncode == 1 and "motivo=FALHA_DE_EXEMPLO_OU_FERRAMENTA codigo=127 " in proc.stdout
              and proc.stdout.splitlines().count(MEASURED_TEXT) == 1)
    no_args = subprocess.run([sys.executable, script], capture_output=True, text=True, check=False)
    empty_arg = subprocess.run([sys.executable, script, "T", "first_window", "E", ""], capture_output=True, text=True,
                               check=False)
    check("processo: sem argumentos, ou com um argumento vazio, sai 2",
          no_args.returncode == 2 and empty_arg.returncode == 2)
    unknown = subprocess.run([sys.executable, script, "T", "first_windows", "E", "O"], capture_output=True, text=True,
                             check=False)
    old_form = subprocess.run([sys.executable, script, "T", "E", "O"], capture_output=True, text=True, check=False)
    check("processo: um exemplo fora da tabela, ou a forma antiga de tres argumentos, sai 2 com a lista dos exemplos",
          unknown.returncode == 2 and old_form.returncode == 2
          and "(examples: first_window, shapes)" in unknown.stderr)


def selftest_decided_scene_constants():
    check("a tabela decidida: first_window com 12 sondas estaticas (D-W8-72), shapes com 62 (D-W8-1083), os dois "
          "arquivos em tests/fixtures/, e captura de 640x480",
          sorted(EXAMPLES) == ["first_window", "shapes"] and CAPTURE_SIZE == (640, 480)
          and EXAMPLES["first_window"].probe_count == 12 and EXAMPLES["shapes"].probe_count == 62
          and EXAMPLES["first_window"].probes_file == REPO_ROOT / "tests" / "fixtures" / "first_window_probes.txt"
          and EXAMPLES["shapes"].probes_file == REPO_ROOT / "tests" / "fixtures" / "shapes_probes.txt")


def probes_in(path):
    """How many probes the REAL reader (image_probe.parse_probes) finds in the file; None when it cannot read it."""
    try:
        return len(parse_probes(Path(path).read_text(encoding="utf-8")))
    except (OSError, ProbeError, UnicodeDecodeError):
        return None


def selftest_table_probe_files_hold_their_counts():
    found = {name: probes_in(facts.probes_file) for name, facts in EXAMPLES.items()}
    check("cada arquivo de sondas da tabela existe e tem, pelo leitor real, exatamente a contagem da tabela: "
          + ", ".join(f"{name}={found[name]}/{EXAMPLES[name].probe_count}" for name in EXAMPLES),
          len(found) == 2 and all(found[name] == EXAMPLES[name].probe_count for name in EXAMPLES))


def selftest_example_source_matches():
    for name, facts in EXAMPLES.items():
        source = example_source(name)
        text = source.read_text(encoding="utf-8", errors="replace") if source.is_file() else ""
        check(f"o fonte de {name} existe e da a janela o titulo que o driver passa a ferramenta, e imprime a linha de "
              "prontidao que o driver espera (D-W8-127)",
              f'"{facts.window_title}"' in text and f'"{facts.ready_line}"' in text)




CONTROL_NAME_PREFIX = "selftest_"

# The binding constructs of the language, from The Python Language Reference, section 4.2.1 "Binding of names"
# (https://docs.python.org/3/reference/executionmodel.html). Type parameter lists exist since Python 3.12; the
# names are looked up so this module still imports on an older interpreter, where parsing the table's rows written
# in that syntax raises SyntaxError and stops the selftest loudly (L-40).
TYPE_PARAMETER_NODES = tuple(getattr(ast, name) for name in ("TypeVar", "ParamSpec", "TypeVarTuple")
                             if hasattr(ast, name))


def bound_names(node):
    """The names one AST node binds, for every construct of section 4.2.1: formal parameters, class and function
    definitions, assignment targets (assignment, augmented and annotated assignment, assignment expression, for,
    with, comprehension and type statement all reach here as a Name in store context through ast.walk), the except
    and except* clause, the as-pattern and capture patterns, import, import *, type parameter lists and del; plus
    two forms that bind outside the plain name: an attribute in store context, and global or nonlocal."""
    if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef)):
        return [node.name]
    if isinstance(node, (ast.Name, ast.Attribute)) and isinstance(node.ctx, (ast.Store, ast.Del)):
        return [node.id if isinstance(node, ast.Name) else node.attr]
    if isinstance(node, ast.arg):
        return [node.arg]
    if isinstance(node, (ast.Import, ast.ImportFrom)):
        return [alias.asname or alias.name.split(".")[0] for alias in node.names]
    if isinstance(node, (ast.Global, ast.Nonlocal)):
        return list(node.names)
    if isinstance(node, (ast.ExceptHandler, ast.MatchAs, ast.MatchStar) + TYPE_PARAMETER_NODES) and node.name:
        return [node.name]
    if isinstance(node, ast.MatchMapping) and node.rest:
        return [node.rest]
    return []


def is_dispatch(node, sibling):
    """The one sanctioned binding (D-W8-185): `from <this module> import selftest_main`, no alias."""
    return (isinstance(node, ast.ImportFrom) and node.module == sibling and len(node.names) == 1
            and node.names[0].name == "selftest_main" and node.names[0].asname is None)


def is_control_name(name):
    """A control name (D-W8-137, D-W8-206): CHECKS, selftest_*, or `*`, which binds every name of a module."""
    return name in ("CHECKS", "*") or name.startswith(CONTROL_NAME_PREFIX)


def driver_control_bindings(source, sibling):
    """(controls, dispatches) of a driver's source: every binding with a control name, anywhere in the tree
    (ast.walk, so nesting hides nothing), apart from the sanctioned dispatch, which is counted on its own. Not seen,
    declared: names made at run time (setattr, globals(), exec), which no static reading can see."""
    controls, dispatches = [], 0
    for node in ast.walk(ast.parse(source)):
        if is_dispatch(node, sibling):
            dispatches += 1
            continue
        controls += [name for name in bound_names(node) if is_control_name(name)]
    return sorted(controls), dispatches


# One row per construct of section 4.2.1, each naming the item it covers, with the result fixed before the run
# (D-W8-346). A construct the reference lists and this table lacks is a defect; a binding the reference does not
# list is outside the rule.
BINDING_FORMS = [
    ("def f(selftest_p): pass", ["selftest_p"], 0, "formal parameters"),
    ("def f(*CHECKS): pass", ["CHECKS"], 0, "formal parameters"),
    ("f = lambda selftest_q: 0", ["selftest_q"], 0, "formal parameters"),
    ("class selftest_g: pass", ["selftest_g"], 0, "class definitions"),
    ("def selftest_a(): pass", ["selftest_a"], 0, "function definitions"),
    ("async def selftest_z(): pass", ["selftest_z"], 0, "function definitions"),
    ("if True:\n    def selftest_b(): pass", ["selftest_b"], 0, "function definitions"),
    ("(selftest_e := 1)", ["selftest_e"], 0, "assignment expressions"),
    ("CHECKS = []", ["CHECKS"], 0, "targets: assignment"),
    ("CHECKS, _u = [], 0", ["CHECKS"], 0, "targets: assignment"),
    ("*CHECKS, = [1]", ["CHECKS"], 0, "targets: assignment"),
    ("CHECKS += []", ["CHECKS"], 0, "targets: assignment"),
    ("CHECKS: list = []", ["CHECKS"], 0, "targets: assignment"),
    ("for selftest_c in []: pass", ["selftest_c"], 0, "targets: for loop header"),
    ("[0 for selftest_h in []]", ["selftest_h"], 0, "targets: for loop header"),
    ("with open(__file__) as selftest_d: pass", ["selftest_d"], 0, "targets: as in a with statement"),
    ("try:\n    pass\nexcept Exception as selftest_j:\n    pass", ["selftest_j"], 0, "targets: except clause"),
    ("try:\n    pass\nexcept* Exception as selftest_k:\n    pass", ["selftest_k"], 0, "targets: except* clause"),
    ("match 1:\n    case 1 as selftest_l:\n        pass", ["selftest_l"], 0, "targets: as-pattern"),
    ("match 1:\n    case selftest_i:\n        pass", ["selftest_i"], 0, "targets: capture pattern"),
    ("match []:\n    case [*selftest_m]:\n        pass", ["selftest_m"], 0, "targets: capture pattern"),
    ("match {}:\n    case {**selftest_n}:\n        pass", ["selftest_n"], 0, "targets: capture pattern"),
    ("import os as selftest_f", ["selftest_f"], 0, "import statements"),
    ("import selftest_r.sub", ["selftest_r"], 0, "import statements"),
    ("from os import path as CHECKS", ["CHECKS"], 0, "import statements"),
    ("from OTHER import selftest_main", ["selftest_main"], 0, "import statements"),
    ("from SIBLING import selftest_main as selftest_main", ["selftest_main"], 0, "import statements"),
    ("from SIBLING import selftest_main, CHECKS", ["CHECKS", "selftest_main"], 0, "import statements"),
    ("from SIBLING import *", ["*"], 0, "import *"),
    ("from OTHER import *", ["*"], 0, "import *"),
    ("from . import CHECKS", ["CHECKS"], 0, "import statements"),
    ("from .raw import *", ["*"], 0, "import *"),
    ("type CHECKS = int", ["CHECKS"], 0, "type statements"),
    ("def f[selftest_t](): pass", ["selftest_t"], 0, "type parameter lists"),
    ("class C[*selftest_u]: pass", ["selftest_u"], 0, "type parameter lists"),
    ("def g[**selftest_v](): pass", ["selftest_v"], 0, "type parameter lists"),
    ("CHECKS = 1\ndel CHECKS", ["CHECKS", "CHECKS"], 0, "del target"),
    ("def f():\n    global CHECKS\n    CHECKS = []", ["CHECKS", "CHECKS"], 0, "global (outside the list)"),
    ("import sys\nsys.modules[__name__].CHECKS = []", ["CHECKS"], 0, "attribute target (outside the list)"),
    ("from SIBLING import selftest_main", [], 1, "the sanctioned dispatch"),
    ("", [], 0, "empty source"),
    ("x = 1", [], 0, "no control"),
]


def driver_layout_ok(controls, dispatches):
    """The decided layout (D-W8-137, D-W8-185, D-W8-206): no control in the driver, and the dispatch exactly once.
    Zero dispatches is also how an empty or unread driver looks, so it fails too (L-40: a scan of nothing)."""
    return not controls and dispatches == 1


# The verdict over (controls, dispatches), fixed before the run.
LAYOUT_CASES = [(([], 1), True), (([], 0), False), ((["CHECKS"], 1), False), (([], 2), False)]


def selftest_driver_keeps_no_controls():
    sibling = Path(__file__).stem
    forms_wrong = [f"{item}: {source}" for source, controls, dispatches, item in BINDING_FORMS
                   if driver_control_bindings(source.replace("SIBLING", sibling), sibling) != (controls, dispatches)]
    layout_wrong = [str(case) for case, want in LAYOUT_CASES if driver_layout_ok(*case) != want]
    check(f"as formas de ligacao da secao 4.2.1 e o veredito: formas={len(BINDING_FORMS)} casos={len(LAYOUT_CASES)}, "
          "erradas: " + (" | ".join(forms_wrong + layout_wrong) or "nenhuma"),
          len(BINDING_FORMS) == 42 and len(LAYOUT_CASES) == 4 and not forms_wrong and not layout_wrong)
    controls, dispatches = driver_control_bindings(DRIVER_SCRIPT.read_text(encoding="utf-8"), sibling)
    check(f"o driver nao define controles e despacha uma vez (D-W8-137, D-W8-206): controles="
          f"{', '.join(controls) or 'nenhum'} despachos={dispatches}", driver_layout_ok(controls, dispatches))


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
    selftest_table_probe_files_hold_their_counts()
    selftest_example_source_matches()
    selftest_driver_keeps_no_controls()
    print(f"selftest: {len(CHECKS)} controles OK")
