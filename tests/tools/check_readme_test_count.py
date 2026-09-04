#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_readme_test_count.py - GODS_LAWS.md L-40 gate for the exact
# defect an adversarial review found on 2026-08-26: README.md's own
# "Building from source" section stated a stale ctest total. This file
# is a PORT of the former tests/tools/check_readme_test_count.sh
# (POSIX sh), retired in the same fatia that wrote this one
# (GODS_LAWS.md L-04, decisao do lider de 02/09/2026: "O comportamento
# deve ser igual em qualquer OS" - the sh version's `if(UNIX)` guard in
# tests/CMakeLists.txt meant it never ran on the windows-latest CI job).
# This port is registered UNGUARDED, the same shape check_spdx.py and
# check_hygiene_coverage.py already proved for their own gates.
#
# README-COUNT-PARITY (TODO.md, measured 02/09/2026): the sh version
# only ever checked ONE stated number against the LOCAL platform's
# ctest total, because it only ran on Linux. Windows registers a
# genuinely SMALLER total - several gates in tests/CMakeLists.txt are
# still POSIX-shell-only and sit inside `if(UNIX)` (readme_test_count
# itself among them, before this port), so they never enter the
# Windows ctest run at all. That is not measurement noise to average
# away: it is a structural difference between the two platforms'
# actual gate coverage, and README.md's own prose needs to say so
# rather than state a single number that is only ever true on one of
# the five CI targets. This gate therefore holds TWO independent
# sentences in README.md to account - one anchored "On Linux," one
# anchored "On Windows," - and checks whichever one matches the
# platform this ctest run is actually executing on. A CI job that only
# ever runs on Linux can never accidentally prove the Windows sentence
# right by comparing it against a Linux number, and vice versa - see
# selftest_cross_platform_control() below, which fixes exactly that
# failure mode with hand-picked numbers before it ever reaches a real
# build.
#
# Usage:
#   check_readme_test_count.py <readme-path> <build-dir> <shared|static>
#   check_readme_test_count.py --selftest
#
# Wired into tests/CMakeLists.txt as readme_test_count_test (real mode)
# and readme_test_count_selftest (the four controls below), on all
# five platforms - unlike the sh version it replaces.
#
# ENV-DRIFT (27/08/2026, GODS_LAWS.md L-40, carried over from the sh
# version): the Linux total is not a property of the repository - it
# is a property of the machine that ran `cmake configure`.
# `preci_selftest` (tests/CMakeLists.txt) is the one test in this whole
# suite gated by find_program(): it registers only when clang-format,
# clang-tidy AND cppcheck are all found, and it lives entirely inside
# an `if(UNIX)` block, so it can only ever inflate the LINUX total,
# never the Windows one. count_optional_tooling_tests() below only
# looks for it when the current platform is Linux; on Windows it is
# always zero, by construction, not by omission.
#
# Each function below does one thing (GODS_LAWS.md L-17).

import platform
import re
import subprocess
import sys

SCRIPT_NAME = "check_readme_test_count.py"

# One regex per platform label, sharing the same tail shape as the sh
# version's sed pattern ("has N registered cases in shared mode and M
# in static mode"), anchored on the BOLD "**On <Platform>**," markdown
# phrase specifically - README.md also has an earlier, unrelated
# plain-text "On Linux, glintfx targets **Wayland only**" sentence in
# the "Supported platforms" section (no test count in it), and a loose
# "On Linux," anchor without the bold markers matches THAT sentence
# first, then fails to cross the paragraph break with the actual count
# (`.` does not match a newline here on purpose - the two anchors stay
# each on their own single-line paragraph, and matching across a break
# would risk pulling in text from an unrelated paragraph instead of
# correctly reporting an empty scan).
_PLATFORM_PATTERNS = {
    "Linux": re.compile(
        r"\*\*On Linux\*\*,.*?\bhas (\d+) registered cases in shared mode and (\d+) in static mode"
    ),
    "Windows": re.compile(
        r"\*\*On Windows\*\*,.*?\bhas (\d+) registered cases in shared mode and (\d+) in static mode"
    ),
}


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


def detect_current_platform_label():
    """"Linux" or "Windows" - the two labels this gate knows about. Any
    other platform.system() value is a genuine gap (macOS is not a CI
    target of this project, GODS_LAWS.md L-04) and fails loudly rather
    than silently picking one of the two sentences to compare against.
    """
    system = platform.system()
    if system == "Windows":
        return "Windows"
    if system == "Linux":
        return "Linux"
    fail(f"plataforma nao coberta por este portao: {system!r} (Linux e Windows sao os dois alvos conhecidos)")


# Extracts the (shared, static) pair for ONE platform label from
# README.md's own text. Returns None when that platform's sentence is
# not found - the empty-scan case GODS_LAWS.md L-40 exists to reprove,
# never to pass silently.
def extract_readme_counts(readme_text, platform_label):
    pattern = _PLATFORM_PATTERNS[platform_label]
    match = pattern.search(readme_text)
    if match is None:
        return None
    return int(match.group(1)), int(match.group(2))


# The comparison logic itself, factored out so --selftest exercises the
# EXACT function real_main() calls, with hand-picked numbers instead of
# a real build tree.
def check_readme_test_count(stated_pair, actual, mode, platform_label):
    if stated_pair is None:
        print(
            f'{SCRIPT_NAME}: varredura vazia (a frase "On {platform_label}, ... has N registered '
            f'cases in shared mode and M in static mode" nao foi encontrada em README.md) - '
            "GODS_LAWS.md L-40",
            file=sys.stderr,
        )
        return False

    stated_shared, stated_static = stated_pair
    if mode == "shared":
        stated = stated_shared
    elif mode == "static":
        stated = stated_static
    else:
        fail(f"modo desconhecido: {mode} (esperado shared ou static)")

    if stated != actual:
        print(
            f"{SCRIPT_NAME}: README.md (secao 'On {platform_label}') diz {stated_shared} shared / "
            f"{stated_static} static, ctest mediu {actual} para o modo {mode} - atualize a frase em "
            "README.md",
            file=sys.stderr,
        )
        return False

    print(
        f"{SCRIPT_NAME}: README.md (secao 'On {platform_label}': {stated_shared} shared / "
        f"{stated_static} static) confere com ctest ({actual}, modo {mode})"
    )
    return True


# --- real mode -------------------------------------------------------


# Raw `ctest -N` total, untouched. Kept apart from the ENV-DRIFT
# adjustment below on purpose: the empty-scan floor (GODS_LAWS.md L-40)
# has to fire on "ctest printed no 'Total Tests:' line at all", never
# on "the adjusted count happens to come out to zero".
def get_ctest_raw_total(build_dir):
    result = subprocess.run(
        ["ctest", "--test-dir", build_dir, "-N"],
        capture_output=True,
        text=True,
        check=False,
    )
    match = re.search(r"^Total Tests: (\d+)$", result.stdout, re.MULTILINE)
    return int(match.group(1)) if match else None


# How much of the raw total is `preci_selftest` - the one test in
# tests/CMakeLists.txt gated by find_program(clang-format/clang-tidy/
# cppcheck) AND by `if(UNIX)`, so it can only ever appear on Linux (see
# the ENV-DRIFT header comment above). Never looked for on Windows: the
# subtraction there is always zero, by construction.
def count_optional_tooling_tests(build_dir, platform_label):
    if platform_label != "Linux":
        return 0
    result = subprocess.run(
        ["ctest", "--test-dir", build_dir, "-N"],
        capture_output=True,
        text=True,
        check=False,
    )
    return len(re.findall(r": preci_selftest$", result.stdout, re.MULTILINE))


def real_main(args):
    if len(args) != 3:
        fail("usage: check_readme_test_count.py <readme-path> <build-dir> <shared|static>")
    readme_file, build_dir, mode = args

    try:
        with open(readme_file, "r", encoding="utf-8") as handle:
            readme_text = handle.read()
    except OSError as exc:
        fail(f"arquivo nao encontrado: {readme_file} ({exc})")

    platform_label = detect_current_platform_label()

    raw_total = get_ctest_raw_total(build_dir)
    if raw_total is None:
        fail(f"ctest --test-dir {build_dir} -N nao devolveu 'Total Tests: N' - GODS_LAWS.md L-40, varredura vazia")

    optional_count = count_optional_tooling_tests(build_dir, platform_label)
    actual = raw_total - optional_count

    stated_pair = extract_readme_counts(readme_text, platform_label)
    if not check_readme_test_count(stated_pair, actual, mode, platform_label):
        fail(
            "contagem de testes divergente (ver mensagem acima) - total bruto do ctest: "
            f"{raw_total}, testes de ferramenta opcional descontados: {optional_count}"
        )


# --- fixtures and controls for --selftest -----------------------------

_SELFTEST_README = (
    "**On Linux**, the test suite currently has 34 registered cases in shared mode and 33 in static mode.\n"
    "**On Windows**, the test suite currently has 20 registered cases in shared mode and 19 in static mode.\n"
)

_SELFTEST_README_EMPTY = "This README no longer states a test count anywhere.\n"


# Positive control: fixture states 34/33 for Linux, actual is 34 for
# Linux/shared. Expected: passes.
def selftest_positive_control():
    stated_pair = extract_readme_counts(_SELFTEST_README, "Linux")
    if check_readme_test_count(stated_pair, 34, "shared", "Linux"):
        print("selftest: controle POSITIVO OK (34 shared declarado para Linux bate com 34 medido)")
        return True
    print("selftest: controle POSITIVO FALHOU (34 shared declarado para Linux deveria ter batido com 34 medido)", file=sys.stderr)
    return False


# Negative control: fixture states 34/33 for Linux, actual is 40 for
# Linux/shared. Expected: reproves, citing BOTH numbers.
def selftest_negative_control():
    import io
    import contextlib

    stated_pair = extract_readme_counts(_SELFTEST_README, "Linux")
    buffer = io.StringIO()
    with contextlib.redirect_stderr(buffer):
        passed = check_readme_test_count(stated_pair, 40, "shared", "Linux")
    output = buffer.getvalue()

    if passed:
        print("selftest: controle NEGATIVO FALHOU (34 declarado x 40 medido deveria ter reprovado)", file=sys.stderr)
        return False
    if "ctest mediu 40" not in output:
        print("selftest: controle NEGATIVO FALHOU (reprovou, mas nao citou os dois numeros)", file=sys.stderr)
        print(output, file=sys.stderr)
        return False
    print("selftest: controle NEGATIVO OK (divergencia 34 declarado x 40 medido pega e citada)")
    return True


# Empty-scan floor: fixture README has no matching sentence at all.
# Expected: reproves with "varredura vazia" in the message.
def selftest_empty_scan_control():
    import io
    import contextlib

    stated_pair = extract_readme_counts(_SELFTEST_README_EMPTY, "Linux")
    buffer = io.StringIO()
    with contextlib.redirect_stderr(buffer):
        passed = check_readme_test_count(stated_pair, 34, "shared", "Linux")
    output = buffer.getvalue()

    if passed:
        print("selftest: controle de VARREDURA VAZIA FALHOU (README sem a frase deveria ter sido recusado, mas passou)", file=sys.stderr)
        return False
    if "varredura vazia" not in output:
        print("selftest: controle de VARREDURA VAZIA FALHOU (recusou, mas nao disse 'varredura vazia')", file=sys.stderr)
        print(output, file=sys.stderr)
        return False
    print("selftest: controle de VARREDURA VAZIA OK (README sem a frase recusado)")
    return True


# Cross-platform control (README-COUNT-PARITY, this port's own reason
# to exist): the fixture states DIFFERENT numbers for Linux (34/33) and
# Windows (20/19). Proves the exact failure mode a naive single-regex
# gate would have: comparing a Windows-measured total against the
# Linux sentence (or vice versa) either false-fails a correct Windows
# build, or - worse - false-passes a wrong one if the two totals ever
# happened to collide. Checks BOTH directions explicitly.
def selftest_cross_platform_control():
    linux_pair = extract_readme_counts(_SELFTEST_README, "Linux")
    windows_pair = extract_readme_counts(_SELFTEST_README, "Windows")

    if linux_pair != (34, 33):
        print(f"selftest: controle CROSS-PLATFORM FALHOU (extracao Linux deveria ser (34, 33), veio {linux_pair})", file=sys.stderr)
        return False
    if windows_pair != (20, 19):
        print(f"selftest: controle CROSS-PLATFORM FALHOU (extracao Windows deveria ser (20, 19), veio {windows_pair})", file=sys.stderr)
        return False

    if not check_readme_test_count(windows_pair, 20, "shared", "Windows"):
        print("selftest: controle CROSS-PLATFORM FALHOU (20 shared medido no Windows deveria bater com o par do Windows)", file=sys.stderr)
        return False
    if check_readme_test_count(linux_pair, 20, "shared", "Windows"):
        print("selftest: controle CROSS-PLATFORM FALHOU (20 medido no Windows NAO deveria bater com o par do Linux, 34/33)", file=sys.stderr)
        return False

    print("selftest: controle CROSS-PLATFORM OK (par do Windows nunca confundido com o par do Linux)")
    return True


def selftest_main():
    controls = [
        selftest_positive_control(),
        selftest_negative_control(),
        selftest_empty_scan_control(),
        selftest_cross_platform_control(),
    ]
    if not all(controls):
        print("check_readme_test_count.py --selftest: FALHOU (ver acima)", file=sys.stderr)
        sys.exit(1)
    print(f"check_readme_test_count.py --selftest: os {len(controls)} controles OK")


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
    else:
        real_main(args)


if __name__ == "__main__":
    main()
