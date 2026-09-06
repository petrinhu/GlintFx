#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_readme_volatile_numbers.py - GODS_LAWS.md L-40 gate, decision of
# the CTO (fatia that follows WIN-WINDOW's own link fix). README.md's
# own "On Linux,"/"On Windows," paragraphs went stale TWICE in two days:
# once caught by check_readme_test_count.py's own single sentence (the
# "has N registered cases..." phrase), and once NOT caught by anything -
# the surrounding parity NARRATIVE ("Linux's 90", "88", "ten renamed
# pairs", "forty controls", an invented program message) drifted away
# from the tree with zero gate watching it, because prose has no fixed
# anchor to check a free-form number against.
#
# The fix this fatia makes is NOT a smarter parser of the narrative -
# the CTO rejected that (it would need one anchor per number, and nine
# anchors in a paragraph make the prose a hostage of the gate). Instead:
# the narrative stops citing volatile numbers AT ALL, and this gate
# reproves any digit that sneaks back into it. Precedent cited by the
# CTO for "check a fixed token, never free prose": the Rust ecosystem's
# version-sync tooling, which checks version STRINGS across files by
# byte-for-byte comparison, never by parsing English about what a
# version means.
#
# THE ONLY EXEMPTIONS (this is the complete list, nothing else is
# stripped before the digit scan):
#   1. The exact trigger phrase check_readme_test_count.py's own gate
#      already owns: "has N registered cases in shared mode and M in
#      static mode". Its numbers are that gate's job, not this one's.
#   2. Any span between backticks (`` `...` ``) - code, file names,
#      command lines. A digit inside a code span is not narrative
#      prose; check_hygiene_coverage.py-style gates own THAT kind of
#      claim if it ever needs one.
#   3. Any `L-NN` law citation (GODS_LAWS.md reference) - a law NUMBER
#      is an identifier into a table, not a fact this repository's own
#      tree can drift out from under.
#
# Every digit that survives all three removals is a FAILURE - the exact
# shape of digit GODS_LAWS.md L-40 exists to catch: a piece of prose
# that "looks read" (states a number) without anything behind it that
# would notice the number going stale.
#
# Usage:
#   check_readme_volatile_numbers.py <readme-path>
#   check_readme_volatile_numbers.py --selftest
#
# Wired into tests/CMakeLists.txt as readme_volatile_numbers_test (real
# mode) and readme_volatile_numbers_selftest (the four controls below),
# UNGUARDED (GODS_LAWS.md L-04, same shape check_readme_test_count.py
# and check_spdx.py already use): this gate reads text, it does not
# build anything, so there is no reason for it to run on only one OS.
#
# Each function below does one thing (GODS_LAWS.md L-17).

import re
import sys

SCRIPT_NAME = "check_readme_volatile_numbers.py"

# Anchors each platform paragraph as ONE LINE (GODS_LAWS.md L-40 sibling
# gate check_readme_test_count.py's own _PLATFORM_PATTERNS comment: `.`
# does not match a newline here on purpose - README.md's own paragraphs
# are each authored as a single long line, and matching across a break
# would risk pulling text from an unrelated paragraph into the scan).
#
# REQUIRES the sibling gate's own trigger phrase to appear on the SAME
# line, exactly like check_readme_test_count.py's own _PLATFORM_PATTERNS
# does - a plain `^\*\*On Windows\*\*,` anchor is not enough: README.md
# also has a "**On Windows**, prepare the MSVC build environment..."
# paragraph (the "Building from source" section) that starts with the
# identical bold phrase and legitimately cites version/architecture
# digits (Visual Studio 2022, x64) that are product facts, not the
# volatile parity narrative this gate exists to police. Found live
# proving this gate red for the first time (GODS_LAWS.md L-36): without
# this requirement the scan reported 3 paragraphs, not 2, and flagged
# that unrelated paragraph's own digits alongside the real ones.
_PARAGRAPH_PATTERN = re.compile(
    r"^\*\*On (?:Linux|Windows)\*\*,.*\bhas \d+ registered cases in shared mode "
    r"and \d+ in static mode.*$",
    re.MULTILINE,
)

# Exemption 1: check_readme_test_count.py's own trigger phrase.
_TRIGGER_PHRASE_PATTERN = re.compile(
    r"\bhas \d+ registered cases in shared mode and \d+ in static mode\b"
)

# Exemption 2: backtick-quoted spans (code, file names, commands).
_BACKTICK_SPAN_PATTERN = re.compile(r"`[^`]*`")

# Exemption 3: GODS_LAWS.md citations (L-04, L-40, ...).
_LAW_CITATION_PATTERN = re.compile(r"\bL-\d+\b")

_DIGIT_PATTERN = re.compile(r"\d")

# A whole token (run of non-space characters) that still contains at
# least one digit, for a human-readable "trecho" in the failure message
# - reporting the bare digit alone ("9") is useless; reporting "90:"
# or "1024x768" tells the reader what actually leaked through.
_DIGIT_TOKEN_PATTERN = re.compile(r"\S*\d\S*")


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


# Strips the three exemptions, in order, from ONE paragraph's text.
# Returns (cleaned_text, spans_removed) so the caller can report both
# the survivors and how much was legitimately exempted - GODS_LAWS.md
# L-40's own "the count appears in the output even when it passes"
# requirement, applied to the exemptions too.
def strip_exemptions(paragraph_text):
    spans_removed = 0

    cleaned, count = _TRIGGER_PHRASE_PATTERN.subn(" ", paragraph_text)
    spans_removed += count

    cleaned, count = _BACKTICK_SPAN_PATTERN.subn(" ", cleaned)
    spans_removed += count

    cleaned, count = _LAW_CITATION_PATTERN.subn(" ", cleaned)
    spans_removed += count

    return cleaned, spans_removed


# Finds every remaining digit-bearing token in the cleaned text. Returns
# the list of tokens (possibly empty) - the caller decides pass/fail and
# how to report it.
def find_surviving_digit_tokens(cleaned_text):
    return _DIGIT_TOKEN_PATTERN.findall(cleaned_text)


# The comparison logic itself, factored out so --selftest exercises the
# EXACT function real_main() calls, with hand-picked README text instead
# of the real file.
def check_readme_volatile_numbers(readme_text):
    paragraphs = _PARAGRAPH_PATTERN.findall(readme_text)

    if len(paragraphs) < 2:
        print(
            f"{SCRIPT_NAME}: varredura recusada ({len(paragraphs)} paragrafo(s) "
            '"**On Linux**,"/"**On Windows**," encontrado(s), esperado pelo menos 2) '
            "- GODS_LAWS.md L-40, nunca presumido vazio nem aprovado por engano",
            file=sys.stderr,
        )
        return False

    total_spans_removed = 0
    total_digits_remaining = 0
    any_failure = False

    for paragraph in paragraphs:
        cleaned, spans_removed = strip_exemptions(paragraph)
        total_spans_removed += spans_removed

        surviving_tokens = find_surviving_digit_tokens(cleaned)
        digit_count = sum(len(_DIGIT_PATTERN.findall(token)) for token in surviving_tokens)
        total_digits_remaining += digit_count

        if surviving_tokens:
            any_failure = True
            print(
                f"{SCRIPT_NAME}: digito(s) fora das isencoes na narrativa de paridade:\n"
                f"  paragrafo: {paragraph}\n"
                f"  trecho(s): {', '.join(surviving_tokens)}\n"
                f"  digitos neste paragrafo: {digit_count}",
                file=sys.stderr,
            )

    # Printed unconditionally (GODS_LAWS.md L-40: the count appears in
    # the output even when the gate passes, so "passou" and "nao olhou"
    # are never indistinguishable).
    print(
        f"{SCRIPT_NAME}: varreu {len(paragraphs)} paragrafo(s), "
        f"{total_spans_removed} trecho(s) isento(s) removido(s), "
        f"{total_digits_remaining} digito(s) restante(s)"
    )

    return not any_failure


# --- fixtures and controls for --selftest -----------------------------

_SELFTEST_README_CLEAN = (
    "**On Linux**, the test suite currently has 102 registered cases in shared mode and 100 in "
    "static mode. Everything below is checked by `readme_test_count_test` and the L-04 parity job, "
    "never restated here as a number.\n"
    "**On Windows**, the test suite currently has 104 registered cases in shared mode and 104 in "
    "static mode. See `tests/parity_aliases.txt` and `tests/parity_exceptions.txt` (L-40).\n"
)

_SELFTEST_README_LOOSE_DIGIT = (
    "**On Linux**, the test suite currently has 102 registered cases in shared mode and 100 in "
    "static mode. Shared mode is at parity with Linux's 90.\n"
    "**On Windows**, the test suite currently has 104 registered cases in shared mode and 104 in "
    "static mode.\n"
)

_SELFTEST_README_EMPTY = "This README no longer states a platform paragraph anywhere.\n"

_SELFTEST_README_TRIGGER_ONLY = (
    "**On Linux**, the test suite currently has 34 registered cases in shared mode and 33 in "
    "static mode.\n"
    "**On Windows**, the test suite currently has 20 registered cases in shared mode and 19 in "
    "static mode.\n"
)


# Positive control: two clean paragraphs (digits only inside the
# exempted trigger phrase, backticks, or an L-NN citation). Expected:
# passes.
def selftest_positive_control():
    if check_readme_volatile_numbers(_SELFTEST_README_CLEAN):
        print("selftest: controle POSITIVO OK (paragrafos limpos passam)")
        return True
    print("selftest: controle POSITIVO FALHOU (paragrafos limpos deveriam ter passado)", file=sys.stderr)
    return False


# Negative control: a loose "90" outside every exemption. Expected:
# reproves, citing the offending token.
def selftest_negative_control():
    import contextlib
    import io

    buffer = io.StringIO()
    with contextlib.redirect_stderr(buffer):
        passed = check_readme_volatile_numbers(_SELFTEST_README_LOOSE_DIGIT)
    output = buffer.getvalue()

    if passed:
        print('selftest: controle NEGATIVO FALHOU ("Linux\'s 90" solto deveria ter reprovado)', file=sys.stderr)
        return False
    if "90" not in output:
        print('selftest: controle NEGATIVO FALHOU (reprovou, mas nao citou o "90")', file=sys.stderr)
        print(output, file=sys.stderr)
        return False
    print('selftest: controle NEGATIVO OK ("Linux\'s 90" solto pego e citado)')
    return True


# Empty-scan floor: fewer than two platform paragraphs found. Expected:
# reproves with "varredura recusada" in the message, never a silent
# pass.
def selftest_empty_scan_control():
    import contextlib
    import io

    buffer = io.StringIO()
    with contextlib.redirect_stderr(buffer):
        passed = check_readme_volatile_numbers(_SELFTEST_README_EMPTY)
    output = buffer.getvalue()

    if passed:
        print("selftest: controle de VARREDURA VAZIA FALHOU (README sem paragrafo deveria ter sido recusado)", file=sys.stderr)
        return False
    if "varredura recusada" not in output:
        print("selftest: controle de VARREDURA VAZIA FALHOU (recusou, mas nao disse 'varredura recusada')", file=sys.stderr)
        print(output, file=sys.stderr)
        return False
    print("selftest: controle de VARREDURA VAZIA OK (README sem paragrafo recusado)")
    return True


# Fourth control the CTO named explicitly: the trigger phrase, WITH its
# own numbers, keeps PASSING - this gate must never re-reprove what
# check_readme_test_count.py already owns.
def selftest_trigger_phrase_exempt_control():
    if check_readme_volatile_numbers(_SELFTEST_README_TRIGGER_ONLY):
        print("selftest: controle de FRASE GATILHADA OK (34/33 e 20/19 da frase de contagem continuam passando)")
        return True
    print(
        "selftest: controle de FRASE GATILHADA FALHOU (a frase 'has N registered cases...' "
        "nao deveria ter sido reprovada por este portao)",
        file=sys.stderr,
    )
    return False


def selftest_main():
    controls = [
        selftest_positive_control(),
        selftest_negative_control(),
        selftest_empty_scan_control(),
        selftest_trigger_phrase_exempt_control(),
    ]
    if not all(controls):
        print(f"{SCRIPT_NAME} --selftest: FALHOU (ver acima)", file=sys.stderr)
        sys.exit(1)
    print(f"{SCRIPT_NAME} --selftest: os {len(controls)} controles OK")


def real_main(args):
    if len(args) != 1:
        fail("usage: check_readme_volatile_numbers.py <readme-path>")
    (readme_file,) = args

    try:
        with open(readme_file, "r", encoding="utf-8") as handle:
            readme_text = handle.read()
    except OSError as exc:
        fail(f"arquivo nao encontrado: {readme_file} ({exc})")

    if not check_readme_volatile_numbers(readme_text):
        fail("digito(s) fora das isencoes na narrativa de paridade (ver mensagens acima)")


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
    else:
        real_main(args)


if __name__ == "__main__":
    main()
