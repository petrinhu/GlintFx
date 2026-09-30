#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# preci_compare_times.py - infra trail C3 (30/09/2026): compares the
# per-test durations of the local preci run against the last GREEN run of
# the same mode, both read from the JUnit that `ctest --output-junit`
# writes. A test that got more than FACTOR times slower AND ended above
# FLOOR_S seconds reproves, naming the test and both numbers. The floor
# keeps a 0.1 s -> 0.5 s jitter from reproving; the factor keeps a slow
# test that merely drifted from reproving.
#
# The scope line is ALWAYS printed (GODS_LAWS.md L-40), zero included:
#   comparacao: comparados N, sem base M (testes novos), reprovados K
# No base file is declared out loud ("sem base"), never silence. A base
# that exists but shares NO test name with the current run reproves: that
# is a broken comparison, not a clean one.
#
# CACHE STATE (cto-review, 30/09/2026): the same nested-build tests take
# 44.7 s with a hot ccache and 147.4 s with it off or cold (3.3x), which a
# 3x limit would call a regression. With --base-state/--now-state (the
# "estado=<k> acerto=<N> chamadas=<n>" strings preci.sh records) only two
# runs of COMPATIBLE state are compared: both "desligado", or both
# "ligado-quente". Anything else is declared out loud, counted on the
# scope line, and neither reproves nor passes silently:
#   comparacao: SUSPENSA (cache agora: <estado>, base: <estado>), comparados 0, reprovados 0
#
# Usage:
#   preci_compare_times.py --base <junit> --now <junit> [--factor 3] [--floor 5]
#                          [--base-state <s> --now-state <s>]
#   preci_compare_times.py --selftest

import os
import sys
import tempfile
import xml.etree.ElementTree as ElementTree

SCRIPT_NAME = "preci_compare_times.py"
DEFAULT_FACTOR = 3.0
DEFAULT_FLOOR_S = 5.0


def read_durations(junit_path):
    """({name: seconds}, error|None). A name appearing twice keeps the larger time."""
    try:
        root = ElementTree.parse(junit_path).getroot()
    except (OSError, ElementTree.ParseError) as exc:
        return None, f"{junit_path} ilegivel ({exc})"
    durations = {}
    for case in root.iter("testcase"):
        try:
            seconds = float(case.get("time", "0"))
        except ValueError:
            continue
        name = case.get("name", "?")
        durations[name] = max(seconds, durations.get(name, 0.0))
    return durations, None


def compare(base, now, factor, floor_s):
    """(comparados, sem_base, reprovados[(name, base_s, now_s)])."""
    compared = 0
    without_base = 0
    offenders = []
    for name, now_s in sorted(now.items()):
        if name not in base:
            without_base += 1
            continue
        compared += 1
        base_s = base[name]
        if now_s > floor_s and now_s > factor * base_s:
            offenders.append((name, base_s, now_s))
    return compared, without_base, offenders


COMPARABLE_STATES = ("desligado", "ligado-quente")


def _state_key(state):
    for token in (state or "").split():
        if token.startswith("estado="):
            return token[len("estado="):]
    return "desconhecido"


def states_comparable(base_state, now_state):
    """True when no state was given (old behaviour) or both runs share a comparable one."""
    if base_state is None and now_state is None:
        return True
    base_key, now_key = _state_key(base_state), _state_key(now_state)
    return base_key == now_key and base_key in COMPARABLE_STATES


def run(base_path, now_path, factor, floor_s, base_state=None, now_state=None):
    now, error = read_durations(now_path)
    if error:
        print(f"{SCRIPT_NAME}: {error}", file=sys.stderr)
        return 1
    if not now:
        print(f"{SCRIPT_NAME}: {now_path} sem nenhum teste (varredura vazia)", file=sys.stderr)
        return 1
    if not os.path.exists(base_path):
        print(f"comparacao: comparados 0, sem base {len(now)} (nenhuma rodada verde anterior deste modo em {base_path}), reprovados 0")
        return 0
    base, error = read_durations(base_path)
    if error:
        print(f"{SCRIPT_NAME}: base {error}", file=sys.stderr)
        return 1
    if not states_comparable(base_state, now_state):
        print(
            f"comparacao: SUSPENSA (cache agora: {now_state}, base: {base_state}), "
            f"comparados 0, reprovados 0"
        )
        return 0
    compared, without_base, offenders = compare(base, now, factor, floor_s)
    print(f"comparacao: comparados {compared}, sem base {without_base} (testes novos), reprovados {len(offenders)}")
    if base and compared == 0:
        print(f"{SCRIPT_NAME}: a base tem {len(base)} teste(s) e nenhum nome casa com a rodada atual: comparacao quebrada", file=sys.stderr)
        return 1
    for name, base_s, now_s in offenders:
        print(
            f"{SCRIPT_NAME}: {name} ficou {now_s / base_s if base_s else float('inf'):.1f}x mais lento "
            f"(base {base_s:.2f} s, agora {now_s:.2f} s; limite {factor:g}x e acima de {floor_s:g} s)",
            file=sys.stderr,
        )
    return 1 if offenders else 0


# --- selftest -----------------------------------------------------------


def _write_junit(path, cases):
    root = ElementTree.Element("testsuite")
    for name, seconds in cases.items():
        ElementTree.SubElement(root, "testcase", {"name": name, "time": repr(seconds)})
    ElementTree.ElementTree(root).write(path, encoding="utf-8", xml_declaration=True)


def _case(scratch, label, base_cases, now_cases, expect_rc, expect_text, expect_absent=(), states=(None, None)):
    import contextlib
    import io

    base_path = os.path.join(scratch, f"{label}-base.xml")
    now_path = os.path.join(scratch, f"{label}-now.xml")
    if base_cases is not None:
        _write_junit(base_path, base_cases)
    _write_junit(now_path, now_cases)
    buffer = io.StringIO()
    with contextlib.redirect_stdout(buffer), contextlib.redirect_stderr(buffer):
        rc = run(base_path, now_path, DEFAULT_FACTOR, DEFAULT_FLOOR_S, states[0], states[1])
    text = buffer.getvalue()
    ok = rc == expect_rc and all(needle in text for needle in expect_text) and not any(
        needle in text for needle in expect_absent
    )
    if not ok:
        print(f"selftest: {label} FALHOU (rc {rc}, esperado {expect_rc}; saida: {text.strip()})", file=sys.stderr)
        return False
    print(f"selftest: {label} OK")
    return True


def selftest_main():
    scratch = tempfile.mkdtemp(prefix="glintfx-preci-compare-selftest-")
    results = [
        _case(scratch, "sem mudanca", {"a": 1.0, "b": 2.0, "c": 6.0}, {"a": 1.1, "b": 2.0, "c": 6.5},
              0, ["comparados 3, sem base 0 (testes novos), reprovados 0"]),
        # The fake test sleeps 4x longer than in the base, above the floor.
        _case(scratch, "teste de mentira 4x mais lento", {"dorminhoco": 2.0, "ok": 1.0}, {"dorminhoco": 8.0, "ok": 1.0},
              1, ["comparados 2", "reprovados 1", "dorminhoco ficou 4.0x mais lento", "base 2.00 s, agora 8.00 s"]),
        # The real case of this trail: the noexcept_alloc gate went from 0.51 s to 21 s.
        _case(scratch, "noexcept_alloc 0,51 s para 21 s", {"noexcept_alloc_test": 0.51}, {"noexcept_alloc_test": 21.0},
              1, ["noexcept_alloc_test ficou 41.2x mais lento", "base 0.51 s, agora 21.00 s"]),
        _case(scratch, "4x mas abaixo do piso de 5 s", {"rapido": 0.5}, {"rapido": 2.0},
              0, ["reprovados 0"]),
        _case(scratch, "acima do piso mas so 2x", {"medio": 3.0}, {"medio": 6.0},
              0, ["reprovados 0"]),
        _case(scratch, "sem base declarada", None, {"a": 1.0, "b": 2.0},
              0, ["comparados 0, sem base 2", "nenhuma rodada verde anterior"]),
        _case(scratch, "teste novo nao reprova e e contado", {"a": 1.0}, {"a": 1.0, "novo": 30.0},
              0, ["comparados 1, sem base 1 (testes novos), reprovados 0"]),
        _case(scratch, "base sem nenhum nome em comum reprova", {"x": 1.0}, {"y": 1.0},
              1, ["comparados 0", "comparacao quebrada"]),
        # Cache state (cto-review): a hot base against a cold run is SUSPENDED, never a false regression.
        _case(scratch, "base quente contra rodada fria: suspensa", {"aninhado": 44.7}, {"aninhado": 147.4},
              0, ["comparacao: SUSPENSA", "comparados 0, reprovados 0"], states=("estado=ligado-quente acerto=97 chamadas=4263", "estado=ligado-frio acerto=12 chamadas=1421")),
        _case(scratch, "base quente contra quente 4x mais lenta: reprova", {"aninhado": 44.7}, {"aninhado": 178.8},
              1, ["aninhado ficou 4.0x mais lento"], states=("estado=ligado-quente acerto=97 chamadas=4263", "estado=ligado-quente acerto=97 chamadas=4263")),
        _case(scratch, "desligado contra desligado 4x mais lento: reprova", {"aninhado": 40.0}, {"aninhado": 160.0},
              1, ["aninhado ficou 4.0x mais lento"], states=("estado=desligado acerto=na chamadas=0", "estado=desligado acerto=na chamadas=0")),
        _case(scratch, "desligado contra quente: suspensa", {"aninhado": 147.4}, {"aninhado": 44.7},
              0, ["comparacao: SUSPENSA"], states=("estado=desligado acerto=na chamadas=0", "estado=ligado-quente acerto=97 chamadas=4263")),
        _case(scratch, "fria contra fria: suspensa", {"aninhado": 100.0}, {"aninhado": 400.0},
              0, ["comparacao: SUSPENSA"], states=("estado=ligado-frio acerto=12 chamadas=1421", "estado=ligado-frio acerto=12 chamadas=1421")),
        _case(scratch, "base sem estado registrado: suspensa", {"aninhado": 44.7}, {"aninhado": 178.8},
              0, ["comparacao: SUSPENSA", "estado=desconhecido"], states=("estado=desconhecido", "estado=ligado-quente acerto=97 chamadas=4263")),
    ]
    if not all(results):
        print(f"{SCRIPT_NAME} --selftest: FALHOU (ver acima)", file=sys.stderr)
        sys.exit(1)
    print(f"{SCRIPT_NAME} --selftest: os {len(results)} controles OK")


def _usage():
    print(f"uso: {SCRIPT_NAME} --base <junit> --now <junit> [--factor N] [--floor S] [--base-state S --now-state S]  |  --selftest", file=sys.stderr)
    sys.exit(2)


def main():
    args = sys.argv[1:]
    if args == ["--selftest"]:
        selftest_main()
        return
    options = {"--factor": DEFAULT_FACTOR, "--floor": DEFAULT_FLOOR_S}
    paths = {}
    states = {}
    index = 0
    while index < len(args):
        key = args[index]
        if key not in ("--base", "--now", "--factor", "--floor", "--base-state", "--now-state") or index + 1 >= len(args):
            _usage()
        value = args[index + 1]
        if key in options:
            try:
                options[key] = float(value)
            except ValueError:
                _usage()
        elif key in ("--base-state", "--now-state"):
            states[key] = value
        else:
            paths[key] = value
        index += 2
    if "--base" not in paths or "--now" not in paths:
        _usage()
    if ("--base-state" in states) != ("--now-state" in states):
        _usage()
    sys.exit(
        run(paths["--base"], paths["--now"], options["--factor"], options["--floor"],
            states.get("--base-state"), states.get("--now-state"))
    )


if __name__ == "__main__":
    main()
