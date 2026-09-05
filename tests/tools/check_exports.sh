#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
# check_exports.sh - fails if the glintfx dynamic library exports a
# symbol outside the glintfx:: namespace or the minimal runtime
# allowlist (FUND-1, item 8 of the service order).
#
# Usage: check_exports.sh <path-to-.so>
#
# Each function below does one thing (GODS_LAWS.md L-17).

set -eu

# CORE-ERROR CE-3 finding (25/08/2026): the mangled Itanium prefix for
# a namespace-scoped symbol is "_ZN7glintfx..." ONLY for a free
# function or a NON-const member function. A CONST member function
# mangles with a "K" cv-qualifier infixed between "_ZN" and the
# nested-name ("_ZNK7glintfx..."), per the Itanium C++ ABI. Every
# symbol exported before CE-3 (runtime_version, version_string,
# gltfx_err_code_name, gltfx_err's constructors/destructor) happened to
# be a free function or a non-const special member, so this gate never
# exercised the "K" branch until gltfx_err's first const accessor
# (path(), line(), ...) - this is a latent bug in the PATTERN, not a
# real intruder symbol, closed here rather than worked around in the
# type being checked.
#
# DECLARED LIMITATION: only "K" (const) is covered, because that is
# the only cv/ref-qualifier this codebase uses on an exported member
# function today. A future exported `volatile`- or ref-qualified
# member ("V", "O", "R" infixes) would need the same treatment; if this
# gate ever rejects a real, correctly glintfx-scoped symbol again,
# check the infix letter between "_ZN" and the namespace length first.
readonly EXPECTED_MANGLED_NAMESPACE="7glintfx"
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
        _ZN"${EXPECTED_MANGLED_NAMESPACE}"*) return 0 ;; # free function / non-const member
        _ZNK"${EXPECTED_MANGLED_NAMESPACE}"*) return 0 ;; # const member function
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

# CORE-ERROR CE-6 finding (25/08/2026, GODS_LAWS.md L-40 applied
# literally to this gate, as asked): a library that somehow exported
# ZERO dynamic symbols would fall through collect_intruder_symbols()
# with an empty intruder list and print "ok" - the exact "varredura
# vazia sai verde" shape L-40 names by name. A library with nothing
# exported is not a passing state for a SHARED library meant to be
# consumed; it means nm read the wrong file or the build produced a
# stripped/empty artifact.
count_lines() {
    if [ -z "$1" ]; then
        echo 0
        return
    fi
    printf '%s\n' "$1" | wc -l | tr -d ' '
}

require_nonempty_scan() {
    total="$1"
    [ "$total" -gt 0 ] || fail "varredura vazia (0 simbolos dinamicos definidos) - biblioteca nao exporta nada, ou nm leu o arquivo errado"
}

real_main() {
    require_nm_present
    require_library_path_arg "$@"

    library_path="$1"
    symbols="$(defined_dynamic_symbol_names "$library_path")"
    total_count="$(count_lines "$symbols")"

    echo "check_exports.sh: dynamic symbols defined in $library_path:"
    printf '%s\n' "$symbols"

    require_nonempty_scan "$total_count"

    intruders="$(collect_intruder_symbols "$library_path")"
    if [ -n "$intruders" ]; then
        echo "check_exports.sh: symbols exported outside the contract:" >&2
        echo "$intruders" >&2
        exit 1
    fi

    # L-40, literal: the count decides nothing by itself here (every
    # one of the $total_count symbols already passed symbol_is_allowed
    # above, or main() would have exited already) - it is printed so a
    # reviewer sees the SIZE of what was actually scanned, on the
    # passing run too, not just inferred from a wall of names above.
    echo "ok: no symbol outside the contract ($total_count dynamic symbol(s) scanned, all allowed)."
}

# --- --selftest (PARITY-GATE, TODO.md, GODS_LAWS.md L-04/L-36): this
# script never had a --selftest of its own, while its Windows twin
# (tools/ci/check-exports-win.ps1) already does - the exact asymmetry
# the parity gate (tests/tools/check_test_parity.py) found and could
# not except (the item that would cover it, EXPORTS-PARITY-WIN, is
# concluded - a "concluded without a pair" is precisely what that gate
# exists to catch). The four controls below mirror check-exports-
# win.ps1's Invoke-SelfTest EXACTLY (same shape, same four cases),
# feeding synthetic mangled names straight to symbol_is_allowed() -
# no real .so, no real `nm`, because what needs proving here is the
# PARSER/ALLOWLIST logic, not a real binary (the real binary is what
# visibility_test itself already exercises). Registered as
# visibility_selftest in tests/CMakeLists.txt, same if(BUILD_SHARED_
# LIBS AND UNIX) guard as visibility_test - Itanium mangling only
# means anything on an ELF/Unix toolchain. ---

selftest_positive_control() {
    # Free function, non-const member, const member - the same three
    # shapes check-exports-win.ps1's Get-SyntheticCleanDumpbinExports
    # uses, proving the "K" cv-qualifier branch (symbol_has_glintfx_
    # prefix) alongside the plain one.
    for symbol in \
        "_ZN7glintfx15runtime_versionEv" \
        "_ZN7glintfx9gltfx_errC1Ev" \
        "_ZNK7glintfx9gltfx_err4pathEv"
    do
        if ! symbol_is_allowed "$symbol"; then
            echo "selftest: controle POSITIVO FALHOU (deveria ter sido aprovado): $symbol" >&2
            return 1
        fi
    done
    echo "selftest: controle POSITIVO OK (3 exports sinteticos - free function, membro nao-const, membro const - todos reconhecidos como glintfx::)"
    return 0
}

selftest_negative_control_stdlib_leak() {
    symbol="_ZSt19__throw_length_errorPKc"
    if symbol_is_allowed "$symbol"; then
        echo "selftest: controle NEGATIVO (stdlib leak) FALHOU ($symbol nao deveria casar como glintfx::)" >&2
        return 1
    fi
    echo "selftest: controle NEGATIVO (stdlib leak) OK (std::__throw_length_error pego, nao reconhecido como glintfx::)"
    return 0
}

selftest_negative_control_bare_c_symbol() {
    symbol="SomeStrayCFunction"
    if symbol_is_allowed "$symbol"; then
        echo "selftest: controle NEGATIVO (simbolo C solto) FALHOU (SomeStrayCFunction nao deveria casar como glintfx::)" >&2
        return 1
    fi
    echo "selftest: controle NEGATIVO (simbolo C solto) OK (SomeStrayCFunction pego, nao reconhecido como glintfx::)"
    return 0
}

selftest_empty_scan_control() {
    total="$(count_lines "")"
    if [ "$total" -ne 0 ]; then
        echo "selftest: controle de VARREDURA VAZIA FALHOU (count_lines de string vazia deveria ser 0, veio $total)" >&2
        return 1
    fi
    # require_nonempty_scan chama fail(), que da exit - roda em
    # subshell para o exit derrubar so o subshell, nunca este script
    # (mesmo padrao de "$(...)" que check_port_privacy.sh ja usa).
    if (require_nonempty_scan "$total") 2>/dev/null; then
        echo "selftest: controle de VARREDURA VAZIA FALHOU (require_nonempty_scan 0 deveria ter reprovado e passou)" >&2
        return 1
    fi
    echo "selftest: controle de VARREDURA VAZIA OK (0 exports extraidos - a chamada real trataria isto como reprovacao, GODS_LAWS.md L-40)"
    return 0
}

selftest_main() {
    overall=0
    selftest_positive_control || overall=1
    selftest_negative_control_stdlib_leak || overall=1
    selftest_negative_control_bare_c_symbol || overall=1
    selftest_empty_scan_control || overall=1

    if [ "$overall" -ne 0 ]; then
        echo "check_exports.sh --selftest: FALHOU (ver acima)" >&2
        exit 1
    fi
    echo "check_exports.sh --selftest: os quatro controles OK"
}

main() {
    if [ "${1:-}" = "--selftest" ]; then
        selftest_main
    else
        real_main "$@"
    fi
}

main "$@"
