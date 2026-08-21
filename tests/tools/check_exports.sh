#!/usr/bin/env sh
# check_exports.sh — falha se a biblioteca dinâmica da glintfx exportar
# símbolo fora do namespace glintfx:: ou da allowlist mínima de runtime
# (FUND-1, item 8 da ordem de serviço).
#
# Uso: check_exports.sh <caminho-da-.so>
#
# Cada função abaixo faz uma coisa (GODS_LAWS.md L-17).

set -eu

readonly EXPECTED_MANGLED_PREFIX="_ZN7glintfx"
readonly ALLOWED_RUNTIME_SYMBOLS="_init _fini _edata _end __bss_start"

fail() {
    echo "check_exports.sh: $1" >&2
    exit 1
}

require_nm_present() {
    command -v nm >/dev/null 2>&1 || fail "'nm' não encontrado no PATH (sem skip silencioso)"
}

require_library_path_arg() {
    [ "$#" -eq 1 ] || fail "uso: check_exports.sh <caminho-da-.so>"
    [ -f "$1" ] || fail "arquivo não encontrado: $1"
}

symbol_has_glintfx_prefix() {
    case "$1" in
        "${EXPECTED_MANGLED_PREFIX}"*) return 0 ;;
        *) return 1 ;;
    esac
}

symbol_is_in_runtime_allowlist() {
    for allowed in $ALLOWED_RUNTIME_SYMBOLS; do
        [ "$1" = "$allowed" ] && return 0
    done
    return 1
}

symbol_is_allowed() {
    symbol_has_glintfx_prefix "$1" || symbol_is_in_runtime_allowlist "$1"
}

defined_dynamic_symbol_names() {
    nm -D --defined-only "$1" | awk '{ print $NF }'
}

collect_intruder_symbols() {
    library_path="$1"
    defined_dynamic_symbol_names "$library_path" | while IFS= read -r symbol; do
        symbol_is_allowed "$symbol" || printf '%s\n' "$symbol"
    done
}

main() {
    require_nm_present
    require_library_path_arg "$@"

    library_path="$1"
    echo "check_exports.sh: símbolos dinâmicos definidos em $library_path:"
    defined_dynamic_symbol_names "$library_path"

    intruders="$(collect_intruder_symbols "$library_path")"
    if [ -n "$intruders" ]; then
        echo "check_exports.sh: símbolos fora do contrato exportados:" >&2
        echo "$intruders" >&2
        exit 1
    fi

    echo "ok: nenhum símbolo fora do contrato."
}

main "$@"
