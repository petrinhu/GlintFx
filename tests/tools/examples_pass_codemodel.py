# SPDX-License-Identifier: AGPL-3.0-or-later
#
# examples_pass_codemodel.py - DEMO-1 D1-fix-c (D-W8-101): the model check of
# the examples pass. It reads the File API codemodel-v2 that a configure leaves
# in <build>/.cmake/api/v1/reply/ (the CMake's own model, independent of the
# pass's walk and of the generator) and answers, for one planta, three
# questions about every EXECUTABLE declared under plantas/<p>/examples/:
#   - is it there (the count the driver compares with the expected sum);
#   - did it get the compile options of the reference target (the tokens of
#     the reference must all be among the tokens of the target);
#   - does its artifact sit under examples/bin/ (the output directory).
#
# CALIBRATION, on every read (the ruler must tell the two apart): the
# reference must carry tokens; the untreated target must NOT contain all of
# them; the untreated artifact must NOT sit under examples/bin/. A failed
# calibration is reported as problems, and the driver refuses the run.
#
# Standard library only (GODS_LAWS.md L-07). No selftest of its own: the
# driver is the only caller, and the reference and untreated targets are its
# calibration fixtures (they exist in every planta configure).
#
# Facts this module relies on, measured in DEMO-1 D1-fix-c passo 0 (probe,
# CMake 4.3, Ninja): paths.source of a target is RELATIVE to the top-level
# source directory of the codemodel ("plantas/ok/examples/foo", "." for the
# project's own targets); artifacts[0].path is RELATIVE to the build dir;
# the compile tokens come from compileGroups[].compileCommandFragments, and
# the standard from compileGroups[].languageStandard.standard.

import glob
import json
import os

REFERENCE_NAME = "glintfx_examples_pass_reference"
UNTREATED_NAME = "glintfx_examples_pass_untreated"
BIN_PREFIX = "examples/bin/"


class CodemodelError(Exception):
    """The model could not be read: the driver reports it as a planta motive."""


def reply_dir(build_dir):
    return os.path.join(build_dir, ".cmake", "api", "v1", "reply")


def load_json(path):
    with open(path, encoding="utf-8") as handle:
        return json.load(handle)


def codemodel_path(build_dir):
    """The codemodel reply, located through reply/index-*.json."""
    directory = reply_dir(build_dir)
    indexes = sorted(glob.glob(os.path.join(directory, "index-*.json")))
    if not indexes:
        raise CodemodelError("reply/index-*.json ausente em " + directory)
    entry = load_json(indexes[-1]).get("reply", {}).get("codemodel-v2")
    if not entry:
        raise CodemodelError("codemodel-v2 ausente no index de " + directory)
    return os.path.join(directory, entry["jsonFile"])


def compile_tokens(raw_target):
    """Every compile token of the CXX groups: fragments split by space, plus std:<n>."""
    tokens = set()
    for group in raw_target.get("compileGroups", []):
        if group.get("language") != "CXX":
            continue
        for fragment in group.get("compileCommandFragments", []):
            tokens.update(fragment.get("fragment", "").split())
        standard = (group.get("languageStandard") or {}).get("standard")
        if standard:
            tokens.add("std:" + str(standard))
    return frozenset(tokens)


def describe_target(raw_target):
    artifacts = raw_target.get("artifacts") or []
    return {
        "name": raw_target["name"],
        "type": raw_target.get("type", ""),
        "source": raw_target.get("paths", {}).get("source", "."),
        "artifact": artifacts[0]["path"] if artifacts else "",
        "tokens": compile_tokens(raw_target),
    }


def read_model(build_dir):
    """Returns (top_source, targets) from the codemodel of one configure."""
    model = load_json(codemodel_path(build_dir))
    top_source = model["paths"]["source"]
    configurations = model.get("configurations") or []
    if not configurations:
        raise CodemodelError("codemodel sem configuracao")
    targets = []
    for reference in configurations[0].get("targets", []):
        raw = load_json(os.path.join(reply_dir(build_dir), reference["jsonFile"]))
        targets.append(describe_target(raw))
    return top_source, targets


def absolute_source(top_source, target):
    return os.path.normpath(os.path.join(top_source, target["source"]))


def is_under(path, directory):
    """Separator-bounded: /x/examples2 is NOT under /x/examples."""
    return path == directory or path.startswith(directory + os.sep)


def named(targets, name):
    matches = [target for target in targets if target["name"] == name]
    if len(matches) != 1:
        raise CodemodelError("alvo %s aparece %d vez(es), esperado 1" % (name, len(matches)))
    return matches[0]


def calibration_problems(targets):
    """The ruler's own checks. Empty list means the ruler tells the two apart."""
    problems = []
    reference = named(targets, REFERENCE_NAME)
    untreated = named(targets, UNTREATED_NAME)
    if not reference["tokens"]:
        problems.append("referencia sem tokens de opcao: a regua nao mede nada")
    elif reference["tokens"] <= untreated["tokens"]:
        problems.append("untreated contem todos os tokens da referencia: a regua nao distingue")
    if untreated["artifact"].startswith(BIN_PREFIX):
        problems.append("untreated caiu em examples/bin/: a regua nao distingue a saida")
    return problems


def target_problems(target, reference_tokens):
    """What an example executable lacks. Empty list means it was treated."""
    problems = []
    missing = sorted(reference_tokens - target["tokens"])
    if missing:
        problems.append("alvo %s sem opcoes da referencia: %s" % (target["name"], " ".join(missing)))
    if not target["artifact"].startswith(BIN_PREFIX):
        problems.append("alvo %s com artefato fora de examples/bin/: %s"
                        % (target["name"], target["artifact"] or "(vazio)"))
    return problems


def check_model(build_dir, planta_examples_dir):
    """Reads one configure and answers the model questions for one planta.

    Returns a dict:
      executables: names of the EXECUTABLE targets under planta_examples_dir;
      treated:     how many of them passed the option and output checks;
      problems:    one line per example target that failed (its own planta);
      calibration: the ruler's problems (global, not one planta's).
    """
    top_source, targets = read_model(build_dir)
    reference = named(targets, REFERENCE_NAME)
    examples = [
        target for target in targets
        if target["type"] == "EXECUTABLE"
        and is_under(absolute_source(top_source, target), planta_examples_dir)
    ]
    problems = []
    treated = 0
    for target in examples:
        found = target_problems(target, reference["tokens"])
        if found:
            problems.extend(found)
        else:
            treated += 1
    return {
        "executables": [target["name"] for target in examples],
        "treated": treated,
        "problems": problems,
        "calibration": calibration_problems(targets),
    }
