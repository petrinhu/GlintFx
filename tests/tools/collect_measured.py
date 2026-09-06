#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# collect_measured.py - MEASURED-COLLECTOR (docs/plano-w6a-janela.md,
# achado do time-lead 06/09/2026): the reader for a fact this project
# already writes to disk on EVERY run, and never reads.
#
# THE DEFECT THIS SCRIPT CLOSES: every CI test step in this project
# runs `ctest ... --output-on-failure`, which only echoes a test's own
# stdout to the CONSOLE when that test REPROVES. `ctest` itself does
# not throw the passing output away, though - it writes the full
# stdout/stderr of EVERY test, passing ones included, into
# <builddir>/Testing/Temporary/LastTest.log on every run (the same
# fact PNC-FASES-SNAPSHOT, .github/workflows/ci.yml's own "Fases do
# portao de colisao de nome" step, already discovered and used for ONE
# script's own instrumentation). A "measured, not asserted" print
# statement inside a passing test - window_parity_test.cpp's own state-
# bits line, win32_window_close_request_test.cpp's own WM_SIZE
# counter, win32_runner_probe_test.cpp's own fifteen std::println()
# calls about the real windows-latest runner's environment - genuinely
# runs and genuinely writes its line into that file, every single
# time. Nobody was reading the file. This script is what reads it.
#
# WHY A FIXED TOKEN, NOT "ANY interesting-looking line": grepping for a
# vague pattern ("measured", a number, a colon) would either miss real
# measurements phrased differently or, far more likely, false-positive
# on ordinary prose that happens to contain a number - this project's
# own docs/comunicacao.md style already fills test output with
# sentences like "50 pumps ok" that are NOT meant to become a parity-
# table row. A measurement that wants to be COLLECTED says so
# explicitly, on its own line, in ONE fixed shape:
#
#   MEASURED <owner>.<key>=<value>
#
# `<owner>` is the test/fixture name that produced it (so two
# different fixtures can each own a `logical_width` key without
# colliding), `<key>` is a stable, English, snake_case identifier for
# what was measured (docs/api-conventions.md R7's own convention,
# applied here to a token instead of a diagnostic field), and
# `<value>` is whatever text the producer wants to report - never
# parsed further by this script, only compared byte-for-byte later by
# check_measured_parity.py (the reader that lives in the `parity` job,
# GODS_LAWS.md L-04). This is deliberately the SAME "closed vocabulary,
# one line, mechanically greppable" shape this project's own NOLINT-
# justification convention and parity_aliases.txt/parity_exceptions.txt
# already use - a line without the token is not a measurement, it is a
# log message, and this script does not guess otherwise.
#
# GODS_LAWS.md L-40 (piso de varredura nao-vazia), THE WHOLE REASON
# THIS SCRIPT EXISTS: zero MEASURED lines found across every log this
# script was given is NEVER "nothing to report" - by the time this
# script runs, at least win32_window_close_request_test.cpp and
# window_parity_test.cpp (or their Wayland-side twins) have already
# run and already ought to have written at least one line each. Zero
# means the instrumentation regressed, the snapshot this script was
# pointed at is stale/empty, or the token itself drifted - never a
# clean pass. --collect's own real_main() below reproves on zero,
# exactly like check_container_fixture_inventory.py's own --compare
# already does for its own piso.

import re
import sys
import tempfile
from pathlib import Path

SCRIPT_NAME = "collect_measured.py"

# One line, start to end - MEASURED, one space, owner.key=value with
# no further structure imposed on `value` (it may itself contain '='
# or spaces; only the FIRST '=' after the dot-separated owner.key
# matters, so a value like "org.glintfx.foo" or "a=b" round-trips
# intact).
MEASURED_LINE = re.compile(r"^MEASURED\s+([A-Za-z0-9_]+\.[A-Za-z0-9_]+)=(.*)$")


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


def extract_measured_lines(text):
    """Returns the list of MEASURED lines found in `text`, in order,
    exactly as written (untouched past stripping the line ending) -
    check_measured_parity.py is the one that parses owner/key/value
    out of them, this function only proves a line MATCHED the shape."""
    found = []
    for raw_line in text.splitlines():
        line = raw_line.strip()
        if MEASURED_LINE.match(line):
            found.append(line)
    return found


def real_main(args):
    if not args or args[0] != "--out":
        fail("usage: collect_measured.py --collect --out <arquivo-saida> <log> [<log> ...]  |  --selftest")
    if len(args) < 2:
        fail("--out exige um caminho de arquivo de saida")
    out_path = Path(args[1])
    log_paths = [Path(p) for p in args[2:]]
    if not log_paths:
        fail("nenhum arquivo de log dado - nada para varrer")

    all_lines = []
    for log_path in log_paths:
        if not log_path.is_file():
            fail(f"arquivo de log nao encontrado: {log_path}")
        text = log_path.read_text(encoding="utf-8", errors="replace")
        found = extract_measured_lines(text)
        print(f"{SCRIPT_NAME}: {len(found)} linha(s) MEASURED encontrada(s) em {log_path}")
        all_lines.extend(found)

    out_path.write_text("\n".join(all_lines) + ("\n" if all_lines else ""), encoding="utf-8")
    print(f"{SCRIPT_NAME}: {len(all_lines)} linha(s) MEASURED no total, gravadas em {out_path}")

    # GODS_LAWS.md L-40: zero e' sempre sinal de coleta quebrada aqui,
    # nunca de "nada para medir" - ver o header comment deste arquivo.
    if len(all_lines) == 0:
        fail(
            "0 linha(s) MEASURED encontrada(s) - varredura vazia (GODS_LAWS.md L-40): "
            "a instrumentacao sumiu, o snapshot de log esta errado/vazio, ou o token MEASURED "
            "mudou de forma sem este script acompanhar"
        )


# --- selftest -----------------------------------------------------

def _write_temp(content):
    with tempfile.NamedTemporaryFile(
        mode="w", suffix=".log", delete=False, encoding="utf-8"
    ) as handle:
        handle.write(content)
    return Path(handle.name)


def selftest_empty_log_reproves():
    """RED: a log with test chatter but NO MEASURED line must make
    real_main() exit 1 - the exact piso de varredura nao-vazia this
    script exists to enforce."""
    log = _write_temp(
        "1: some_test .......... Passed 0.01 sec\n"
        "1: connect_smoke: connected (is_open() == true)\n"
    )
    out = _write_temp("")
    try:
        try:
            real_main(["--out", str(out), str(log)])
        except SystemExit as exc:
            if exc.code != 1:
                print(f"selftest: log vazio de MEASURED devolveu codigo {exc.code}, esperado 1",
                      file=sys.stderr)
                return False
            print("selftest: log sem nenhuma linha MEASURED reprova (RED) - ok")
            return True
        print("selftest: log vazio de MEASURED NAO reprovou - esperado exit 1", file=sys.stderr)
        return False
    finally:
        log.unlink(missing_ok=True)
        out.unlink(missing_ok=True)


def selftest_populated_log_succeeds():
    """GREEN: a log carrying two real MEASURED lines (the exact two
    this fatia's own report names) must succeed and write BOTH out,
    untouched."""
    log = _write_temp(
        "1: window_parity_test ... Passed 0.03 sec\n"
        "window_parity_test: state right after open() - active=0 maximized=0 fullscreen=0 "
        "(measured, not asserted)\n"
        "MEASURED window_parity_test.active_after_open=0\n"
        "MEASURED window_parity_test.maximized_after_open=0\n"
        "MEASURED window_parity_test.fullscreen_after_open=0\n"
        "2: win32_window_close_request_test ... Passed 0.02 sec\n"
        "MEASURED win32_window_close_request_test.wm_size_during_create=1\n"
    )
    out = _write_temp("")
    try:
        try:
            real_main(["--out", str(out), str(log)])
        except SystemExit as exc:
            print(f"selftest: log populado reprovou inesperadamente (codigo {exc.code})",
                  file=sys.stderr)
            return False
        written = out.read_text(encoding="utf-8").splitlines()
        if len(written) != 4:
            print(f"selftest: esperava 4 linhas MEASURED gravadas, achei {len(written)}: {written}",
                  file=sys.stderr)
            return False
        print(f"selftest: log com 4 linhas MEASURED reais gravou as 4 (GREEN) - ok: {written}")
        return True
    finally:
        log.unlink(missing_ok=True)
        out.unlink(missing_ok=True)


def selftest_ignores_non_measured_lines():
    """A line that merely CONTAINS the word "measured" in prose (this
    project's own convention text, "measured, not asserted") must NEVER
    match - only a line that IS the fixed MEASURED token shape,
    starting the line, counts."""
    log = _write_temp(
        "seat_test: name=\"seat0\" pointer=0 keyboard=1 (measured, not asserted)\n"
        "MEASURED seat_test.pointer=0\n"
    )
    out = _write_temp("")
    try:
        try:
            real_main(["--out", str(out), str(log)])
        except SystemExit as exc:
            print(f"selftest: log misto reprovou inesperadamente (codigo {exc.code})",
                  file=sys.stderr)
            return False
        written = out.read_text(encoding="utf-8").splitlines()
        if written != ["MEASURED seat_test.pointer=0"]:
            print(f"selftest: prosa com a palavra 'measured' vazou para a colheita: {written}",
                  file=sys.stderr)
            return False
        print("selftest: prosa contendo 'measured' e' ignorada, so' o token real e' colhido - ok")
        return True
    finally:
        log.unlink(missing_ok=True)
        out.unlink(missing_ok=True)


def selftest_main():
    controls = [
        selftest_empty_log_reproves(),
        selftest_populated_log_succeeds(),
        selftest_ignores_non_measured_lines(),
    ]
    if not all(controls):
        print(f"{SCRIPT_NAME} --selftest: FALHOU (ver acima)", file=sys.stderr)
        sys.exit(1)
    print(f"{SCRIPT_NAME} --selftest: os {len(controls)} controles OK")


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
    elif args and args[0] == "--collect":
        real_main(args[1:])
    else:
        fail("usage: collect_measured.py --collect --out <arquivo-saida> <log> [<log> ...]  |  --selftest")


if __name__ == "__main__":
    main()
