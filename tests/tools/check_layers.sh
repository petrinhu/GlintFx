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
# Usage: check_layers.sh <source-root-directory>
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

require_root_dir_arg() {
    [ "$#" -eq 1 ] || fail "usage: check_layers.sh <source-root-directory>"
    [ -d "$1" ] || fail "directory not found: $1"
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
# reportar", e reprova. Antes deste conserto, um diretorio renomeado ou
# apagado fazia file_count cair para 0 e o portao imprimia "violations:
# 0 in 0 files scanned" com saida 0 - exatamente a forma que a L-40
# proibe pelo nome, medida ao vivo com `check_layers.sh <raiz sem
# src/core nem include/glintfx/core>`.
require_nonempty_scan() {
    file_count="$1"
    [ "$file_count" -gt 0 ] \
        || fail "varredura vazia (0 arquivos em src/core ou include/glintfx/core) - GODS_LAWS.md L-40"
}

main() {
    require_root_dir_arg "$@"

    files="$(core_source_files "$1")"
    file_count=0
    [ -n "$files" ] && file_count="$(printf '%s\n' "$files" | wc -l)"

    require_nonempty_scan "$file_count"

    violations=""
    if [ -n "$files" ]; then
        violations="$(printf '%s\n' "$files" | while IFS= read -r f; do violations_in_file "$f"; done)"
    fi

    if [ -n "$violations" ]; then
        echo "check_layers.sh: layer violations (GODS_LAWS.md L-19):" >&2
        echo "$violations" >&2
        exit 1
    fi

    violation_count=0
    [ -n "$violations" ] && violation_count="$(printf '%s\n' "$violations" | wc -l)"
    echo "check_layers.sh: violations: $violation_count in $file_count files scanned"
}

main "$@"
