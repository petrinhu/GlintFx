#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_readme_volatile_numbers.py - GODS_LAWS.md L-40 gate, decision of
# the CTO (fatia that follows WIN-WINDOW's own link fix). README.md's
# own "On Linux,"/"On Windows," paragraphs went stale TWICE in two days:
# once caught by the sibling gate this file used to share the README
# with (check_readme_test_count.py, its own single sentence "has N
# registered cases..."), and once NOT caught by anything - the
# surrounding parity NARRATIVE ("Linux's 90", "88", "ten renamed
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
# 06/09/2026 - the sibling gate itself is RETIRED (GODS_LAWS.md L-67:
# revoked is deleted, not archived), and its own trigger-phrase
# exemption goes with it: the "has N registered cases..." sentence that
# gate owned went stale a THIRD time in three commits (the Windows
# figure was always deduced, never measured, on a machine with no MSVC
# toolchain) - the number itself is gone from README.md now, not just
# re-guarded, so this gate's own vocabulary of exemptions shrinks from
# three to two.
#
# THE ONLY EXEMPTIONS LEFT (this is the complete list, nothing else is
# stripped before the digit scan):
#   1. Any span between backticks (`` `...` ``) - code, file names,
#      command lines. A digit inside a code span is not narrative
#      prose; check_hygiene_coverage.py-style gates own THAT kind of
#      claim if it ever needs one.
#   2. Any `L-NN` law citation (GODS_LAWS.md reference) - a law NUMBER
#      is an identifier into a table, not a fact this repository's own
#      tree can drift out from under.
#
# Every digit that survives both removals is a FAILURE - the exact
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

# Anchors each platform paragraph as ONE LINE (`.` does not match a
# newline here on purpose - README.md's own paragraphs are each
# authored as a single long line, and matching across a break would
# risk pulling text from an unrelated paragraph into the scan).
#
# ANCHORED on "the test suite registers" (06/09/2026, since the retired
# sibling gate's own numeric trigger phrase no longer exists to anchor
# on) - a plain `^\*\*On Windows\*\*,` anchor is not enough: README.md
# also has a "**On Windows**, prepare the MSVC build environment..."
# paragraph (the "Building from source" section) that starts with the
# identical bold phrase and legitimately cites version/architecture
# digits (Visual Studio 2022, x64) that are product facts, not the
# volatile parity narrative this gate exists to police. Found live
# proving this gate red for the first time (GODS_LAWS.md L-36): without
# this requirement the scan reported 3 paragraphs, not 2, and flagged
# that unrelated paragraph's own digits alongside the real ones.
_PARAGRAPH_PATTERN = re.compile(
    r"^\*\*On (?:Linux|Windows)\*\*, the test suite registers\b.*$",
    re.MULTILINE,
)

# Exemption 1: backtick-quoted spans (code, file names, commands).
_BACKTICK_SPAN_PATTERN = re.compile(r"`[^`]*`")

# Exemption 2: GODS_LAWS.md citations (L-04, L-40, ...).
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


# Strips the two exemptions, in order, from ONE paragraph's text.
# Returns (cleaned_text, spans_removed) so the caller can report both
# the survivors and how much was legitimately exempted - GODS_LAWS.md
# L-40's own "the count appears in the output even when it passes"
# requirement, applied to the exemptions too.
def strip_exemptions(paragraph_text):
    spans_removed = 0

    cleaned, count = _BACKTICK_SPAN_PATTERN.subn(" ", paragraph_text)
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
    "**On Linux**, the test suite registers more cases in shared mode than in static mode. "
    "Everything is checked by the `parity` job (GODS_LAWS.md L-04), never restated here as a number.\n"
    "**On Windows**, the test suite registers the same cases in shared and in static mode. "
    "See `tests/parity_aliases.txt` and `tests/parity_exceptions.txt` (L-40).\n"
)

_SELFTEST_README_LOOSE_DIGIT = (
    "**On Linux**, the test suite registers more cases in shared mode than in static mode. "
    "Shared mode is at parity with Linux's 90.\n"
    "**On Windows**, the test suite registers the same cases in shared and in static mode.\n"
)

_SELFTEST_README_EMPTY = "This README no longer states a platform paragraph anywhere.\n"

# 06/09/2026 - the exact shape this gate now exists to prevent a
# RETURN of: the old numbered sentence the retired sibling gate used to
# police, sneaking back into an otherwise-clean paragraph.
_SELFTEST_README_REGRESSION = (
    "**On Linux**, the test suite registers more cases in shared mode than in static mode: "
    "it has 116 registered cases in shared mode and 114 in static mode.\n"
    "**On Windows**, the test suite registers the same cases in shared and in static mode.\n"
)

# 06/09/2026 - the exact "isca" this gate's own anchor has to survive:
# a THIRD paragraph that starts with the identical bold "**On
# Windows**," phrase but is not the parity narrative at all (README.md's
# real "Building from source" section has one just like it) - the scan
# must report 2 paragraphs, never 3.
_SELFTEST_README_BAIT = (
    "**On Linux**, the test suite registers more cases in shared mode than in static mode.\n"
    "**On Windows**, the test suite registers the same cases in shared and in static mode.\n"
    "**On Windows**, prepare the MSVC build environment for Visual Studio 2022 before building.\n"
)


# Positive control: two clean paragraphs (digits only inside backticks
# or an L-NN citation). Expected: passes.
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


# Regression control (o oposto do antigo controle de FRASE GATILHADA,
# que passava a numeracao antiga porque ela tinha isencao propria):
# agora que a isencao 1 morreu junto com o gate irmao, a MESMA frase
# numerada tem de REPROVAR se algum dia voltar a aparecer.
def selftest_regression_control():
    import contextlib
    import io

    buffer = io.StringIO()
    with contextlib.redirect_stderr(buffer):
        passed = check_readme_volatile_numbers(_SELFTEST_README_REGRESSION)
    output = buffer.getvalue()

    if passed:
        print(
            "selftest: controle de REGRESSAO FALHOU (a frase numerada antiga voltou e deveria ter reprovado)",
            file=sys.stderr,
        )
        return False
    if "116" not in output:
        print('selftest: controle de REGRESSAO FALHOU (reprovou, mas nao citou "116")', file=sys.stderr)
        print(output, file=sys.stderr)
        return False
    print('selftest: controle de REGRESSAO OK (frase numerada antiga pega e citada, "116")')
    return True


# Isca control: um terceiro paragrafo comecando pelo mesmo "**On
# Windows**," (o paragrafo real de MSVC do README) nao pode ser contado
# como paragrafo de paridade - a varredura tem de dizer 2, nunca 3.
def selftest_bait_paragraph_control():
    import contextlib
    import io

    buffer = io.StringIO()
    with contextlib.redirect_stdout(buffer):
        passed = check_readme_volatile_numbers(_SELFTEST_README_BAIT)
    output = buffer.getvalue()

    if not passed:
        print(f"selftest: controle de ISCA FALHOU (deveria ter passado): {output}", file=sys.stderr)
        return False
    if "varreu 2 paragrafo(s)" not in output:
        print(f"selftest: controle de ISCA FALHOU (deveria ter varrido exatamente 2): {output}", file=sys.stderr)
        return False
    print(f"selftest: controle de ISCA OK (paragrafo de MSVC nao contado como narrativa de paridade): {output.strip()}")
    return True


def selftest_main():
    controls = [
        selftest_positive_control(),
        selftest_negative_control(),
        selftest_empty_scan_control(),
        selftest_regression_control(),
        selftest_bait_paragraph_control(),
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
