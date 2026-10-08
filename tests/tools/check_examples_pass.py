#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_examples_pass.py - DEMO-1 D1-fix-b (D-W8-93) and D1-fix-c (D-W8-101,
# D-W8-102): exercises the examples pass (cmake/GlintfxExamples.cmake) on each
# planta of tests/examples_pass/plantas/, one fresh configure per planta,
# WITHOUT configuring the library, and then checks the TREATMENT through the
# File API model of the CMake (tests/tools/examples_pass_codemodel.py).
#
# For each planta, by its expectation (EXPECTATIONS):
#   - "pass": configure rc 0, and EXACTLY ONE output line equal to
#     "-- glintfx: exemplos diretorios=d alcancados=a executaveis_tratados=t"
#     (regex on the whole line, then equality with the formatted tuple: an
#     integer comparison would accept "00", D-W8-102), and no "CMake Error";
#   - "refuse": configure rc different from 0, EXACTLY ONE "CMake Error", and
#     the body of that error (between "(message):" and "Call Stack") equal,
#     after space normalization (D-W8-97), to the expected rule message;
#   - "refuse_prefix": the same, but the body must START with the text
#     (sem_examples carries the path in its message; declared, D-W8-102);
#   - "refuse_absolute": the same, but rule 4 prints an ABSOLUTE path, which
#     depends on the checkout: only its tail is fixed (R2).
# A planta that must pass also gets the MODEL check: every EXECUTABLE declared
# under its examples/ must be exactly the t of its expectation (per planta, D-W8-101),
# and must carry the compile tokens of the reference target
# and sit under examples/bin/. The calibration (the reference target must
# differ from the untreated one, D-W8-101) is checked on every model read: a
# ruler that does not tell them apart refuses the whole run.
#
# Piso (L-40): the plantas are enumerated FROM DISK. The script always prints
#   modelo: executaveis=<n> tratados=<k> calibracao=<ok|falhou>
#   plantas=<n> conferidas=<c> falharam=<k>
# and refuses (exit 1) when n is 0 or different from PLANT_COUNT, when the
# model finds 0 executables or a number different from the sum of the expected
# treated counts of the passing plantas, or when the calibration fails.
#
# About the return code (K3, declared): the rc is the only signal for a dead
# process (rc 128 or more, GODS_LAWS.md L-49), so it is checked, but in the
# plantas where the message already covers the case it adds nothing. No
# --selftest on purpose: this script registers nothing; the plantas and the
# calibration fixtures are its fixtures.
#
# Usage:
#   check_examples_pass.py --cmake <cmake> --generator <gen> \
#       --source <raiz-do-repo> --work <dir-de-trabalho>

import argparse
import os
import re
import shutil
import subprocess
import sys

import examples_pass_codemodel

PLANT_COUNT = 16
SCRIPT_NAME = "check_examples_pass.py"
PLANTAS_RELDIR = os.path.join("tests", "examples_pass", "plantas")
PROJECT_RELDIR = os.path.join("tests", "examples_pass")

PASS = "pass"
REFUSE = "refuse"
REFUSE_PREFIX = "refuse_prefix"
REFUSE_ABSOLUTE = "refuse_absolute"

RULE1_FIRST_LEVEL = (
    "glintfx: examples/foo/ tem CMakeLists.txt mas nao foi adicionado: "
    "acrescente add_subdirectory(foo) em examples/CMakeLists.txt"
)
RULE1_NESTED = (
    "glintfx: examples/misto/interno/ tem CMakeLists.txt mas nao foi adicionado: "
    "acrescente add_subdirectory(interno) em examples/misto/CMakeLists.txt"
)
RULE2 = "glintfx: examples/lib_only/ nao declara nenhum executavel"
RULE3 = (
    "glintfx: o executavel root_tool foi declarado em examples/CMakeLists.txt; "
    "cada exemplo mora no proprio diretorio"
)
RULE4 = (
    "glintfx: examples/CMakeLists.txt adicionou grupo/exemplo, que nao e um "
    "diretorio filho de examples/; cada exemplo mora em examples/<nome>/, e um "
    "agrupamento precisa do proprio CMakeLists.txt"
)
SEM_EXAMPLES = (
    "GLINTFX_BUILD_EXAMPLES is ON but examples/CMakeLists.txt does not exist"
)
# Rule 4 when the entry climbs out of examples/: the path is ABSOLUTE and
# depends on where the checkout lives, so only its tail is fixed (D-W8-98, R2).
ABS_PREFIX = "glintfx: examples/CMakeLists.txt adicionou "
ABS_TAIL = (
    ", que nao e um diretorio filho de examples/; cada exemplo mora em "
    "examples/<nome>/, e um agrupamento precisa do proprio CMakeLists.txt"
)

# name -> (kind, expected)
#   pass:           expected = (d, a, t), the tuple of the count line
#   refuse:         expected = the rule message (one CMake Error, equal body)
#   refuse_prefix:  expected = the start of the message
EXPECTATIONS = {
    "ok": (PASS, (1, 1, 1)),
    "dois_executaveis": (PASS, (1, 1, 2)),
    "vazio": (PASS, (0, 0, 0)),
    "aninhado": (PASS, (1, 2, 1)),
    "nao_alcancado": (REFUSE, RULE1_FIRST_LEVEL),
    "so_biblioteca": (REFUSE, RULE2),
    "executavel_na_raiz": (REFUSE, RULE3),
    "sem_examples": (REFUSE_PREFIX, SEM_EXAMPLES),
    "grupo_misto": (REFUSE, RULE4),
    "aninhado_mixto": (REFUSE, RULE1_NESTED),
    "ordem_1_4": (REFUSE, RULE1_FIRST_LEVEL),
    "ordem_4_2": (REFUSE, RULE4),
    "ordem_2_3": (REFUSE, RULE2),
    "fronteira_separador": (PASS, (1, 1, 1)),
    "cmakefiles_ignorado": (PASS, (1, 1, 1)),
    "fora_topo": (REFUSE_ABSOLUTE, "/fora_topo/fora"),
}

COUNT_LINE_RE = re.compile(
    r"^-- glintfx: exemplos diretorios=\d+ alcancados=\d+ executaveis_tratados=\d+$",
    re.MULTILINE,
)
COUNT_GROUPS_RE = re.compile(
    r"^-- glintfx: exemplos diretorios=(\d+) alcancados=(\d+) executaveis_tratados=(\d+)$"
)
CMAKE_ERROR = "CMake Error"
BODY_START = "(message):"
BODY_END = "Call Stack (most recent call first):"
CONFIGURE_INCOMPLETE = "-- Configuring incomplete"
MODEL_ERRORS = (
    examples_pass_codemodel.CodemodelError, OSError, ValueError,
    KeyError, IndexError, TypeError,
)


def normalize_space(text):
    """Collapses every run of whitespace to one space (D-W8-97).

    CMake breaks the text of a FATAL_ERROR over several lines, so a raw
    comparison would refuse a message that is correct. Applied to BOTH sides.
    """
    return " ".join(text.split())


def list_plantas(plantas_dir):
    """Names of the planta directories on disk, sorted."""
    if not os.path.isdir(plantas_dir):
        return []
    return sorted(
        name for name in os.listdir(plantas_dir)
        if os.path.isdir(os.path.join(plantas_dir, name))
    )


def configure_planta(args, planta, build_dir):
    """One fresh configure of the planta; returns (rc, combined output)."""
    shutil.rmtree(build_dir, ignore_errors=True)
    command = [
        args.cmake,
        "--fresh",
        "-S", os.path.join(args.source, PROJECT_RELDIR),
        "-B", build_dir,
        "-G", args.generator,
        "-DGLINTFX_SOURCE_DIR=" + args.source,
        "-DPLANTA=" + planta,
    ]
    completed = subprocess.run(command, capture_output=True, text=True, check=False)
    output = completed.stdout + completed.stderr
    return completed.returncode, output


def extract_error_body(output):
    """The text of the first CMake Error, normalized; empty when there is none."""
    start = output.find(BODY_START)
    if start < 0:
        return ""
    rest = output[start + len(BODY_START):]
    end = rest.find(BODY_END)
    if end < 0:
        end = rest.find(CONFIGURE_INCOMPLETE)
    if end < 0:
        end = len(rest)
    return normalize_space(rest[:end])


def count_line_problems(output, expected):
    """Exactly one whole-line match, equal to the formatted expected tuple."""
    matches = list(COUNT_LINE_RE.finditer(output))
    if len(matches) != 1:
        return ["linha de contagem: encontradas=%d, esperada=1" % len(matches)]
    d, a, t = expected
    expected_line = "-- glintfx: exemplos diretorios=%d alcancados=%d executaveis_tratados=%d" % (d, a, t)
    if matches[0].group(0) != expected_line:
        groups = COUNT_GROUPS_RE.match(matches[0].group(0)).groups()
        return ["contagem: esperado (%d,%d,%d), obtido (%s)" % (d, a, t, ",".join(groups))]
    return []


def pass_problems(rc, output, expected):
    reasons = []
    if rc != 0:
        reasons.append("rc=%d, esperado 0" % rc)
    reasons.extend(count_line_problems(output, expected))
    errors = output.count(CMAKE_ERROR)
    if errors != 0:
        reasons.append("erros do CMake=%d, esperado 0" % errors)
    return reasons


def absolute_matches(body, path_tail):
    """Rule 4 with an absolute path: the prefix and tail are fixed, the path ends with path_tail."""
    if not (body.startswith(ABS_PREFIX) and body.endswith(ABS_TAIL)):
        return False
    middle = body[len(ABS_PREFIX):len(body) - len(ABS_TAIL)]
    return (re.match(r"(/|[A-Za-z]:/)", middle) is not None
            and middle.endswith(path_tail) and " " not in middle)


# Autoteste da conferencia do caminho absoluto (CI vermelho da D1-fix-d: o
# caminho do Windows comeca pela letra da unidade). Roda sempre, no inicio do
# driver, e imprime cada caso; um caso que nao se comporta como escrito reprova.
ABSOLUTE_CASES = (
    ("caminho POSIX", "/home/x/plantas/fora_topo/fora", True),
    ("caminho com letra de unidade", "D:/a/x/plantas/fora_topo/fora", True),
    ("caminho relativo", "../fora", False),
    ("caminho sem raiz", "fora_topo/fora", False),
)


def absolute_selfcheck():
    """Prints each case of ABSOLUTE_CASES; returns how many did not behave as written."""
    failures = 0
    for label, path, accepted in ABSOLUTE_CASES:
        body = ABS_PREFIX + path + ABS_TAIL
        got = absolute_matches(body, "/fora_topo/fora")
        ok = got == accepted
        print("autoteste absolute_matches: %s (%s) %s" % (label, path, "OK" if ok else "FALHOU"))
        if not ok:
            failures += 1
    return failures


def body_matches(kind, body, expected_text):
    """The matcher of each refusal kind. The body is already normalized."""
    if kind == REFUSE_ABSOLUTE:
        return absolute_matches(body, expected_text)
    wanted = normalize_space(expected_text)
    if kind == REFUSE_PREFIX:
        return body.startswith(wanted)
    return body == wanted


def describe_expected(kind, expected_text):
    """The expectation as text, for the motive line."""
    if kind == REFUSE_ABSOLUTE:
        return ABS_PREFIX + "<caminho absoluto terminado em " + expected_text + ">" + ABS_TAIL
    return expected_text


def refuse_problems(rc, output, kind, expected_text):
    reasons = []
    if rc == 0:
        reasons.append("rc=0, esperado diferente de 0")
    errors = output.count(CMAKE_ERROR)
    if errors != 1:
        reasons.append("erros do CMake=%d, esperado 1" % errors)
    body = extract_error_body(output)
    if not body_matches(kind, body, expected_text):
        reasons.append("mensagem esperada ausente: " + describe_expected(kind, expected_text))
    return reasons


def expectation_problems(planta, rc, output):
    """The planta's own reasons (configure and count/message). Model not included."""
    if planta not in EXPECTATIONS:
        return ["planta sem expectativa escrita no " + SCRIPT_NAME]
    kind, expected = EXPECTATIONS[planta]
    if kind == PASS:
        return pass_problems(rc, output, expected)
    return refuse_problems(rc, output, kind, expected)


def planta_examples_dir(args, planta):
    return os.path.normpath(
        os.path.join(args.source, PROJECT_RELDIR, "plantas", planta, "examples"))


class ModelTally:
    """Sums of the model over the passing plantas, and the calibration verdict."""

    def __init__(self):
        self.executables = 0
        self.treated = 0
        self.calibration_problems = []
        self.reads = 0

    def add(self, model):
        self.reads += 1
        self.executables += len(model["executables"])
        self.treated += model["treated"]
        self.calibration_problems.extend(model["calibration"])

    def calibration_verdict(self):
        if self.reads == 0 or self.calibration_problems:
            return "falhou"
        return "ok"


def model_problems(args, planta, build_dir, tally, expected_t):
    """Reads the model of one passing planta; returns its reasons, and feeds the tally.

    The count is checked per planta (its own t, D-W8-101), so a planta that
    gains an extra executable under examples/ dies by its own name.
    """
    try:
        model = examples_pass_codemodel.check_model(build_dir, planta_examples_dir(args, planta))
    except MODEL_ERRORS as error:
        return ["modelo indisponivel: %s" % error]
    tally.add(model)
    reasons = list(model["problems"])
    found = len(model["executables"])
    if found != expected_t:
        reasons.append("modelo: executaveis=%d, esperado %d" % (found, expected_t))
    return reasons


def expected_treated_sum():
    """The executables the passing plantas declare: the t of each (d, a, t)."""
    return sum(expected[2] for kind, expected in EXPECTATIONS.values() if kind == PASS)


def run_planta(args, planta, tally):
    """Configures one planta, checks it, and returns (rc, reasons)."""
    build_dir = os.path.join(args.work, planta)
    rc, output = configure_planta(args, planta, build_dir)
    log_path = os.path.join(args.work, planta + ".log")
    with open(log_path, "w", encoding="utf-8") as log:
        log.write(output)
    reasons = expectation_problems(planta, rc, output)
    kind = EXPECTATIONS.get(planta, (None, None))[0]
    if kind == PASS:
        if rc == 0:
            reasons.extend(model_problems(args, planta, build_dir, tally, EXPECTATIONS[planta][1][2]))
        else:
            reasons.append("modelo nao lido: configure com rc diferente de 0")
    return rc, reasons, log_path


def report_planta(planta, rc, reasons, log_path):
    status = "OK" if not reasons else "FALHOU"
    print("planta=%s rc=%d resultado=%s log=%s" % (planta, rc, status, log_path))
    for reason in reasons:
        print("  motivo: " + reason)


def run_all(args):
    """Runs every planta. Returns (n, conferidas, falharam, tally)."""
    plantas = list_plantas(os.path.join(args.source, PLANTAS_RELDIR))
    tally = ModelTally()
    failures = 0
    checked = 0
    for planta in plantas:
        rc, reasons, log_path = run_planta(args, planta, tally)
        report_planta(planta, rc, reasons, log_path)
        checked += 1
        if reasons:
            failures += 1
    return len(plantas), checked, failures, tally


def parse_args(argv):
    parser = argparse.ArgumentParser(description=SCRIPT_NAME)
    parser.add_argument("--cmake", required=True)
    parser.add_argument("--generator", required=True)
    parser.add_argument("--source", required=True)
    parser.add_argument("--work", required=True)
    return parser.parse_args(argv)


def main(argv):
    args = parse_args(argv)
    if absolute_selfcheck():
        print(SCRIPT_NAME + ": FALHOU - autoteste de absolute_matches")
        return 1
    os.makedirs(args.work, exist_ok=True)
    n, checked, failures, tally = run_all(args)
    print("modelo: executaveis=%d tratados=%d calibracao=%s"
          % (tally.executables, tally.treated, tally.calibration_verdict()))
    for problem in tally.calibration_problems:
        print("  calibracao: " + problem)
    print("plantas=%d conferidas=%d falharam=%d" % (n, checked, failures))
    expected_sum = expected_treated_sum()
    if n == 0 or n != PLANT_COUNT:
        print(SCRIPT_NAME + ": FALHOU - plantas=%d, o esperado sao %d "
              "(tirar ou acrescentar uma planta muda esta contagem)" % (n, PLANT_COUNT))
        return 1
    if tally.executables == 0 or tally.executables != expected_sum:
        print(SCRIPT_NAME + ": FALHOU - modelo: executaveis=%d, a soma esperada das "
              "plantas verdes e %d" % (tally.executables, expected_sum))
        return 1
    if tally.calibration_verdict() != "ok":
        print(SCRIPT_NAME + ": FALHOU - calibracao falhou: a regua nao distingue o "
              "tratado do nao tratado")
        return 1
    if failures:
        print(SCRIPT_NAME + ": FALHOU - %d planta(s) fora do esperado" % failures)
        return 1
    print(SCRIPT_NAME + ": OK - %d plantas conferidas" % checked)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
