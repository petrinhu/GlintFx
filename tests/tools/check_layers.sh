#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
# check_layers.sh - CI gate for GODS_LAWS.md L-19 ("a CI gate reproves
# the violation" instead of trusting the discipline of whoever writes
# the code).
#
# Verifies that the core layer (src/core/, include/glintfx/core/) does
# not include (a) a header from a layer above; today only
# glintfx/platform/, not created yet, but the pattern is ready for when
# it is born; nor (b) an operating system header. The pure core knows
# nothing about the OS.
#
# Usage:
#   check_layers.sh <source-root-directory>
#   check_layers.sh --selftest
#
# --selftest runs the three GODS_LAWS.md L-40 controls (positive,
# negative, empty-scan) against throwaway fixtures under mktemp, never
# against the real tracked tree - registered as ctest case
# `layers_selftest` (see tests/CMakeLists.txt).
#
# GODS_LAWS.md L-40, achado de revisao adversarial (25/08/2026):
# layers_test era o UNICO teste wired a este script, e sempre rodava
# contra a raiz real do repo (nunca vazia) - a "varredura vazia" que
# require_nonempty_scan (abaixo) existe para recusar nunca era
# exercitada. Confirmado por mutacao ao vivo: remover a chamada de
# require_nonempty_scan faz o script regredir exatamente para a forma
# que a L-40 nomeia pelo nome ("violations: 0 in 0 files scanned", exit
# 0), e nenhum teste da suite pegava isso. --selftest fecha essa
# lacuna chamando check_layers() - a MESMA funcao que real_main usa -
# contra uma fixture sem src/core nem include/glintfx/core.
#
# Each function below does one thing (GODS_LAWS.md L-17).

set -eu

# Layer above the core that does not exist yet in this slice (FUND-2);
# the pattern is ready for when it is born.
readonly UPPER_LAYER_PATTERN='glintfx/platform/'

# OS headers covered by this slice: Wayland, Win32, GL/EGL and the most
# common low-level POSIX calls.
readonly OS_HEADER_PATTERN='wayland|windows\.h|winuser|GL/|EGL/|<dlfcn|<unistd|<sys/|<fcntl'

fail() {
    echo "check_layers.sh: $1" >&2
    exit 1
}

core_source_dirs() {
    root="$1"
    for candidate in "$root/src/core" "$root/include/glintfx/core"; do
        [ -d "$candidate" ] && printf '%s\n' "$candidate"
    done
}

# One find(1) call per candidate directory, so a directory whose path
# contains whitespace is never split by word-splitting (unlike passing
# a newline-joined string straight to `find $dirs`).
core_source_files() {
    root="$1"
    core_source_dirs "$root" | while IFS= read -r dir; do
        find "$dir" -type f \
            \( -name '*.hpp' -o -name '*.cpp' -o -name '*.h' \
            -o -name '*.hh' -o -name '*.hxx' -o -name '*.cc' -o -name '*.cxx' \)
    done
}

forbidden_include_pattern() {
    printf '%s|%s' "$UPPER_LAYER_PATTERN" "$OS_HEADER_PATTERN"
}

violations_in_file() {
    grep -nE "$(forbidden_include_pattern)" "$1" 2>/dev/null | while IFS=: read -r line _; do
        printf '%s:%s\n' "$1" "$line"
    done
}

# GODS_LAWS.md L-40 (piso de varredura nao-vazia): zero arquivos
# encontrados sob src/core/ ou include/glintfx/core/ nao e "nada a
# reportar", e reprova. Returns 1 (not exit) so check_layers() below
# can be exercised in-process by --selftest without killing the shell.
require_nonempty_scan() {
    file_count="$1"
    if [ "$file_count" -eq 0 ]; then
        echo "check_layers.sh: varredura vazia (0 arquivos em src/core ou include/glintfx/core) - GODS_LAWS.md L-40" >&2
        return 1
    fi
}

# The actual gate logic, factored out of main() so --selftest exercises
# the EXACT same function real_main() calls - not a reimplementation
# that could drift from production, and not a subprocess re-invocation
# that a mutation on this function's own body would not necessarily
# reach the same way.
check_layers() {
    root="$1"

    files="$(core_source_files "$root")"
    file_count=0
    [ -n "$files" ] && file_count="$(printf '%s\n' "$files" | wc -l)"

    require_nonempty_scan "$file_count" || return 1

    violations=""
    if [ -n "$files" ]; then
        violations="$(printf '%s\n' "$files" | while IFS= read -r f; do violations_in_file "$f"; done)"
    fi

    if [ -n "$violations" ]; then
        echo "check_layers.sh: layer violations (GODS_LAWS.md L-19):" >&2
        echo "$violations" >&2
        return 1
    fi

    violation_count=0
    echo "check_layers.sh: violations: $violation_count in $file_count files scanned"
}

# --- real mode -------------------------------------------------------

require_root_dir_arg() {
    [ "$#" -eq 1 ] || fail "usage: check_layers.sh <source-root-directory>"
    [ -d "$1" ] || fail "directory not found: $1"
}

real_main() {
    require_root_dir_arg "$@"
    check_layers "$1" || fail "layer violation found (GODS_LAWS.md L-19; see message above)"
}

# --- fixtures and controls for --selftest -----------------------------

make_scratch_workdir() {
    mktemp -d "${TMPDIR:-/tmp}/glintfx-layers-selftest-XXXXXX"
}

# Minimal tree with one clean file under src/core/.
make_clean_fixture() {
    root="$1"
    mkdir -p "$root/src/core"
    printf '#include <cstdint>\n// clean core file, no OS or upper-layer header\n' \
        > "$root/src/core/clean.cpp"
}

# Positive control: clean fixture. Expected: passes.
selftest_positive_control() {
    scratch="$1"
    root="$scratch/positive"
    make_clean_fixture "$root"

    if output="$(check_layers "$root" 2>&1)"; then
        echo "selftest: controle POSITIVO OK (fixture limpa aprovada)"
        return 0
    fi
    echo "selftest: controle POSITIVO FALHOU (fixture limpa deveria ter sido aprovada)" >&2
    printf '%s\n' "$output" >&2
    return 1
}

# Negative control: plants a forbidden OS header inside
# include/glintfx/core/. Expected: reproves and cites the planted file.
selftest_negative_control() {
    scratch="$1"
    root="$scratch/negative"
    make_clean_fixture "$root"
    alvo="$root/include/glintfx/core/dirty.hpp"
    mkdir -p "$(dirname "$alvo")"
    printf '#include <wayland-client.h>\n' > "$alvo"

    if output="$(check_layers "$root" 2>&1)"; then
        echo "selftest: controle NEGATIVO FALHOU (header do SO em include/glintfx/core/ nao foi pego)" >&2
        return 1
    fi
    if ! printf '%s\n' "$output" | grep -qF "$alvo"; then
        echo "selftest: controle NEGATIVO FALHOU (reprovou, mas nao citou $alvo)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    echo "selftest: controle NEGATIVO OK (header do SO em include/glintfx/core/ pego e citado)"
    return 0
}

# Empty-scan floor: neither src/core/ nor include/glintfx/core/ exists.
# Expected: reproves with "varredura vazia" in the message - the exact
# case the revisor's mutation (removing require_nonempty_scan) makes
# regress to "violations: 0 in 0 files scanned", exit 0.
selftest_empty_scan_control() {
    scratch="$1"
    root="$scratch/empty"
    mkdir -p "$root"

    if output="$(check_layers "$root" 2>&1)"; then
        echo "selftest: controle de VARREDURA VAZIA FALHOU (raiz sem src/core nem include/glintfx/core deveria ter sido recusada, mas passou)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    if ! printf '%s\n' "$output" | grep -qF "varredura vazia"; then
        echo "selftest: controle de VARREDURA VAZIA FALHOU (recusou, mas nao disse 'varredura vazia')" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    echo "selftest: controle de VARREDURA VAZIA OK (raiz sem diretorio de core recusada)"
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
        echo "check_layers.sh --selftest: FALHOU (ver acima)" >&2
        exit 1
    fi
    echo "check_layers.sh --selftest: os tres controles OK"
}

main() {
    if [ "${1:-}" = "--selftest" ]; then
        selftest_main
    else
        real_main "$@"
    fi
}

main "$@"
