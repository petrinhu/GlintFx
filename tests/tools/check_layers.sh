#!/usr/bin/env sh
# check_layers.sh — gate de CI da GODS_LAWS.md L-19 ("um gate de CI
# reprova a violação" em vez de confiar na disciplina de quem escreve).
#
# Verifica que a camada núcleo (src/core/, include/glintfx/core/) não
# inclui (a) header de camada acima — hoje só glintfx/platform/, ainda
# não criada, mas o padrão já fica pronto para quando nascer — nem
# (b) header de sistema operacional. Núcleo puro não conhece o SO.
#
# Uso: check_layers.sh <diretório-raiz-do-source>
#
# Cada função abaixo faz uma coisa (GODS_LAWS.md L-17).

set -eu

# Camada acima do núcleo que ainda não existe nesta fatia (FUND-2); o
# padrão fica pronto para quando ela nascer.
readonly UPPER_LAYER_PATTERN='glintfx/platform/'

# Cabeçalhos de SO cobertos por esta fatia: Wayland, Win32, GL/EGL e as
# chamadas POSIX de baixo nível mais comuns.
readonly OS_HEADER_PATTERN='wayland|windows\.h|winuser|GL/|EGL/|<dlfcn|<unistd|<sys/|<fcntl'

fail() {
    echo "check_layers.sh: $1" >&2
    exit 1
}

require_root_dir_arg() {
    [ "$#" -eq 1 ] || fail "uso: check_layers.sh <diretório-raiz-do-source>"
    [ -d "$1" ] || fail "diretório não encontrado: $1"
}

core_source_dirs() {
    root="$1"
    for candidate in "$root/src/core" "$root/include/glintfx/core"; do
        [ -d "$candidate" ] && printf '%s\n' "$candidate"
    done
}

core_source_files() {
    dirs="$(core_source_dirs "$1")"
    [ -z "$dirs" ] && return 0
    # shellcheck disable=SC2086 # $dirs é lista de diretórios, split intencional.
    find $dirs -type f \
        \( -name '*.hpp' -o -name '*.cpp' -o -name '*.h' \
        -o -name '*.hh' -o -name '*.hxx' -o -name '*.cc' -o -name '*.cxx' \)
}

forbidden_include_pattern() {
    printf '%s|%s' "$UPPER_LAYER_PATTERN" "$OS_HEADER_PATTERN"
}

violations_in_file() {
    grep -nE "$(forbidden_include_pattern)" "$1" 2>/dev/null | while IFS=: read -r line _; do
        printf '%s:%s\n' "$1" "$line"
    done
}

main() {
    require_root_dir_arg "$@"

    files="$(core_source_files "$1")"
    file_count=0
    [ -n "$files" ] && file_count="$(printf '%s\n' "$files" | wc -l)"

    violations=""
    if [ -n "$files" ]; then
        violations="$(printf '%s\n' "$files" | while IFS= read -r f; do violations_in_file "$f"; done)"
    fi

    if [ -n "$violations" ]; then
        echo "check_layers.sh: violações de camada (GODS_LAWS.md L-19):" >&2
        echo "$violations" >&2
        exit 1
    fi

    violation_count=0
    [ -n "$violations" ] && violation_count="$(printf '%s\n' "$violations" | wc -l)"
    echo "check_layers.sh: violacoes: $violation_count em $file_count arquivos varridos"
}

main "$@"
