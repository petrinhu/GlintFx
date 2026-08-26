#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_readme_test_count.sh - GODS_LAWS.md L-40 gate for the exact
# defect an adversarial review found on 2026-08-26: README.md's own
# "Building from source" section stated "33 registered cases in
# shared mode and 32 in static mode" while `ctest -N` already measured
# 34/33, three commits earlier - the documentation commits simply
# predated the CORE-COLOR commits that landed on top of them, and
# nothing compared the two numbers automatically. This script is that
# comparison, run as an ordinary ctest case against the SAME build it
# ships in.
#
# Usage:
#   check_readme_test_count.sh <readme-path> <build-dir> <shared|static>
#   check_readme_test_count.sh --selftest
#
# Wired into tests/CMakeLists.txt as readme_test_count_test (real mode)
# and readme_test_count_selftest (the three controls below), UNIX only,
# same declared downgrade as every other gate in that file (Windows has
# no `sh` by default). Registering this gate itself adds two ctest
# cases to the very total it checks - README.md's own sentence already
# accounts for the +2 (GODS_LAWS.md L-40: a gate that forgets to count
# its own footprint is exactly the kind of gap this lei exists to name).
#
# --selftest runs the three GODS_LAWS.md L-40 controls (positive,
# negative, empty-scan) against throwaway fixture README files under
# mktemp, with hand-picked "actual count" numbers - never a real
# `ctest -N` invocation, so the three controls stay fast and need no
# built tree (the one real I/O sliver, get_ctest_total(), is kept
# separate from the comparison logic on purpose, GODS_LAWS.md L-17).
#
# Each function below does one thing (GODS_LAWS.md L-17).

set -eu

fail() {
    echo "check_readme_test_count.sh: $1" >&2
    exit 1
}

# Extracts the two numbers from README.md's own sentence. Prints
# "<shared> <static>" on a match; prints nothing when the sentence is
# not found - the empty-scan case GODS_LAWS.md L-40 exists to reprove,
# never to pass silently.
extract_readme_counts() {
    readme_file="$1"
    sed -n \
        's/.*has \([0-9][0-9]*\) registered cases in shared mode and \([0-9][0-9]*\) in static mode.*/\1 \2/p' \
        "$readme_file" | head -n 1
}

# The comparison logic itself, factored out so --selftest exercises the
# EXACT function real_main() calls, with hand-picked numbers instead of
# a real build tree.
check_readme_test_count() {
    stated_pair="$1"
    actual="$2"
    mode="$3"

    if [ -z "$stated_pair" ]; then
        echo "check_readme_test_count.sh: varredura vazia (a frase \"N registered cases in shared mode and M in static mode\" nao foi encontrada em README.md) - GODS_LAWS.md L-40" >&2
        return 1
    fi

    stated_shared="$(printf '%s' "$stated_pair" | cut -d' ' -f1)"
    stated_static="$(printf '%s' "$stated_pair" | cut -d' ' -f2)"

    case "$mode" in
        shared) stated="$stated_shared" ;;
        static) stated="$stated_static" ;;
        *) fail "modo desconhecido: $mode (esperado shared ou static)" ;;
    esac

    if [ "$stated" != "$actual" ]; then
        echo "check_readme_test_count.sh: README.md diz $stated_shared shared / $stated_static static, ctest mediu $actual para o modo $mode - atualize a frase em README.md" >&2
        return 1
    fi

    echo "check_readme_test_count.sh: README.md ($stated_shared shared / $stated_static static) confere com ctest ($actual, modo $mode)"
}

# --- real mode -------------------------------------------------------

get_ctest_total() {
    build_dir="$1"
    ctest --test-dir "$build_dir" -N 2>/dev/null | sed -n 's/^Total Tests: \([0-9][0-9]*\)$/\1/p'
}

real_main() {
    [ "$#" -eq 3 ] || fail "usage: check_readme_test_count.sh <readme-path> <build-dir> <shared|static>"
    readme_file="$1"
    build_dir="$2"
    mode="$3"

    [ -f "$readme_file" ] || fail "arquivo nao encontrado: $readme_file"
    [ -d "$build_dir" ] || fail "diretorio de build nao encontrado: $build_dir"

    actual="$(get_ctest_total "$build_dir")"
    [ -n "$actual" ] || fail "ctest --test-dir $build_dir -N nao devolveu 'Total Tests: N' - GODS_LAWS.md L-40, varredura vazia"

    stated_pair="$(extract_readme_counts "$readme_file")"
    check_readme_test_count "$stated_pair" "$actual" "$mode" || fail "contagem de testes divergente (ver mensagem acima)"
}

# --- fixtures and controls for --selftest -----------------------------

make_scratch_workdir() {
    mktemp -d "${TMPDIR:-/tmp}/glintfx-readme-count-selftest-XXXXXX"
}

# Positive control: fixture states 34/33, actual is 34 for shared.
# Expected: passes.
selftest_positive_control() {
    scratch="$1"
    readme="$scratch/positive-README.md"
    printf 'The test suite currently has 34 registered cases in shared mode and 33 in static mode.\n' > "$readme"
    stated_pair="$(extract_readme_counts "$readme")"

    if output="$(check_readme_test_count "$stated_pair" "34" "shared" 2>&1)"; then
        echo "selftest: controle POSITIVO OK (34 shared declarado bate com 34 medido)"
        return 0
    fi
    echo "selftest: controle POSITIVO FALHOU (34 shared declarado deveria ter batido com 34 medido)" >&2
    printf '%s\n' "$output" >&2
    return 1
}

# Negative control: fixture states 34/33, actual is 40 for shared.
# Expected: reproves, citing BOTH numbers.
selftest_negative_control() {
    scratch="$1"
    readme="$scratch/negative-README.md"
    printf 'The test suite currently has 34 registered cases in shared mode and 33 in static mode.\n' > "$readme"
    stated_pair="$(extract_readme_counts "$readme")"

    if output="$(check_readme_test_count "$stated_pair" "40" "shared" 2>&1)"; then
        echo "selftest: controle NEGATIVO FALHOU (34 declarado x 40 medido deveria ter reprovado)" >&2
        return 1
    fi
    if ! printf '%s\n' "$output" | grep -qF "ctest mediu 40"; then
        echo "selftest: controle NEGATIVO FALHOU (reprovou, mas nao citou os dois numeros)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    echo "selftest: controle NEGATIVO OK (divergencia 34 declarado x 40 medido pega e citada)"
    return 0
}

# Empty-scan floor: fixture README has no matching sentence at all.
# Expected: reproves with "varredura vazia" in the message - the exact
# regression a rewritten sentence (different wording, moved section)
# would cause if this control did not exist.
selftest_empty_scan_control() {
    scratch="$1"
    readme="$scratch/empty-README.md"
    printf 'This README no longer states a test count anywhere.\n' > "$readme"
    stated_pair="$(extract_readme_counts "$readme")"

    if output="$(check_readme_test_count "$stated_pair" "34" "shared" 2>&1)"; then
        echo "selftest: controle de VARREDURA VAZIA FALHOU (README sem a frase deveria ter sido recusado, mas passou)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    if ! printf '%s\n' "$output" | grep -qF "varredura vazia"; then
        echo "selftest: controle de VARREDURA VAZIA FALHOU (recusou, mas nao disse 'varredura vazia')" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    echo "selftest: controle de VARREDURA VAZIA OK (README sem a frase recusado)"
    return 0
}

selftest_main() {
    scratch="$(make_scratch_workdir)"
    trap 'rm -rf "$scratch"' EXIT

    overall=0
    selftest_positive_control "$scratch" || overall=1
    selftest_negative_control "$scratch" || overall=1
    selftest_empty_scan_control "$scratch" || overall=1

    if [ "$overall" -ne 0 ]; then
        echo "check_readme_test_count.sh --selftest: FALHOU (ver acima)" >&2
        exit 1
    fi
    echo "check_readme_test_count.sh --selftest: os tres controles OK"
}

main() {
    if [ "${1:-}" = "--selftest" ]; then
        selftest_main
    else
        real_main "$@"
    fi
}

main "$@"
