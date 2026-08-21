#!/usr/bin/env sh
# check_exports.sh - fails if the glintfx dynamic library exports a
# symbol outside the glintfx:: namespace or the minimal runtime
# allowlist (FUND-1, item 8 of the service order).
#
# Usage: check_exports.sh <path-to-.so>
#
# Each function below does one thing (GODS_LAWS.md L-17).

set -eu

readonly EXPECTED_MANGLED_PREFIX="_ZN7glintfx"
readonly ALLOWED_RUNTIME_SYMBOLS="_init _fini _edata _end __bss_start"

fail() {
    echo "check_exports.sh: $1" >&2
    exit 1
}

require_nm_present() {
    command -v nm >/dev/null 2>&1 || fail "'nm' not found in PATH (no silent skip)"
}

require_library_path_arg() {
    [ "$#" -eq 1 ] || fail "usage: check_exports.sh <path-to-.so>"
    [ -f "$1" ] || fail "file not found: $1"
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
    echo "check_exports.sh: dynamic symbols defined in $library_path:"
    defined_dynamic_symbol_names "$library_path"

    intruders="$(collect_intruder_symbols "$library_path")"
    if [ -n "$intruders" ]; then
        echo "check_exports.sh: symbols exported outside the contract:" >&2
        echo "$intruders" >&2
        exit 1
    fi

    echo "ok: no symbol outside the contract."
}

main "$@"
