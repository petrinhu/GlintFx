#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
# check_port_privacy.sh - CI gate for the ARCH-PORTS gatilho de parada
# (GODS_LAWS.md L-19/L-40): proves, mechanically, two of the three
# conditions the CTO plan named as the stop-and-report trigger for this
# fatia - nothing is INSTALLED (checked structurally: nothing here is
# ever considered, because this scan never touches an install prefix,
# only the source tree and the built .so) and nothing is EXPORTED as a
# dynamic symbol. The third condition ("nada global") is a property of
# the CODE ITSELF - no static/global adapter instance anywhere in
# src/platform/ - reviewed by the adversarial reviewer, the same way
# "an absence of a thing" is reviewed everywhere else in this project's
# gates (e.g. check_layers.sh's own header comment makes the same
# distinction for layer discipline).
#
# THREE SUB-CHECKS, each with its own GODS_LAWS.md L-40 non-empty-scan
# floor:
#   (a) at least one adapter header exists under src/platform/ - the
#       gate itself has something to check. A tree with ZERO adapters
#       found is the "varredura vazia sai verde" shape L-40 forbids,
#       not "nothing to report".
#   (b) EVERY *_adapter.hpp found anywhere in the repository (src/ and
#       tests/) names a CLASS already on the CLOSED list this script
#       hard-codes - an adapter added anywhere without updating that
#       list reproves, rather than passing unnoticed (the "enumeracao
#       fechada" leg of L-40 item 5).
#   (c) no header under include/glintfx/ (the PUBLIC surface) reaches
#       into src/platform/port/ or any src/platform/<backend>/ by
#       #include path - the port never becomes part of the installed,
#       public API surface.
#   (d) when a shared-library path is given (BUILD_SHARED_LIBS=ON), the
#       library's own dynamic symbol table (nm -D --defined-only)
#       contains ZERO symbol whose demangled name mentions any name on
#       the closed adapter/port list - proves opacity at the BINARY
#       level, not only by reading source. When BUILD_SHARED_LIBS=OFF
#       this sub-check is SKIPPED WITH A PRINTED REASON (a static
#       archive has no dynamic symbol table at all) - never silently.
#
# Usage:
#   check_port_privacy.sh <source-root-directory> <path-to-.so-or-NONE>
#   check_port_privacy.sh --selftest
#
# Each function below does one thing (GODS_LAWS.md L-17).

set -eu

# The CLOSED list (L-40 item 5, "enumeracao fechada por construcao"):
# every class this project's OWN code (production or test) is allowed
# to name as a display-connection adapter today. Adding a new adapter
# means adding its name HERE, in the same commit - the exact discipline
# tests/display_port_concept_test.cpp's own header comment already
# names as "a lista de adaptadores... vive num único lugar".
#
# win32_display_adapter added by WIN-DISPLAY (TODO.md, GODS_LAWS.md
# L-04 reabertura de ARCH-PORTS por paridade): this script scans
# src/platform/ and tests/ UNCONDITIONALLY, on every platform this
# gate runs on (this gate itself is if(UNIX)-only in tests/CMakeLists.txt,
# but the tree it reads is the same tree on every leg) - so the
# win32/ backend's own adapter must be on this list even though this
# script never runs a Windows build itself.
readonly KNOWN_ADAPTER_CLASSES="wayland_display_adapter fake_display_adapter win32_display_adapter"
readonly KNOWN_PORT_NAMES="display_connection_port display_connection"

fail() {
    echo "check_port_privacy.sh: $1" >&2
    exit 1
}

require_nonempty_scan() {
    what="$1"
    count="$2"
    if [ "$count" -eq 0 ]; then
        echo "check_port_privacy.sh: varredura vazia ($what) - GODS_LAWS.md L-40" >&2
        return 1
    fi
}

# --- (a) at least one adapter header exists under src/platform/ ------

production_adapter_headers() {
    root="$1"
    find "$root/src/platform" -type f -name '*_adapter.hpp' 2>/dev/null | sort
}

check_production_adapters_present() {
    root="$1"
    files="$(production_adapter_headers "$root")"
    count=0
    [ -n "$files" ] && count="$(printf '%s\n' "$files" | wc -l | tr -d ' ')"
    require_nonempty_scan "src/platform/*_adapter.hpp" "$count" || return 1
    echo "check_port_privacy.sh: (a) $count adapter header(s) found under src/platform/:"
    printf '%s\n' "$files"
}

# --- (b) every adapter class found anywhere is on the closed list ----

all_adapter_headers() {
    root="$1"
    find "$root/src" "$root/tests" -type f -name '*_adapter.hpp' 2>/dev/null | sort
}

# A file whose own name matches *_adapter.hpp defines a new adapter
# class ("class wayland_display_adapter { ... }") OR only ALIASES an
# already-known one (selected_display_adapter.hpp's own "using
# selected_display_adapter = wayland_display_adapter;" - GODS_LAWS.md
# L-19 item 2, the compile-time selection mechanism itself). Both
# shapes name a class this function extracts and the caller validates
# against KNOWN_ADAPTER_CLASSES; a file matching neither shape is
# genuinely unrecognized, not silently accepted.
#
# [a-z0-9_]+, NOT [a-z_]+ (WIN-DISPLAY correction): the original
# pattern had no digit in its character class, so it silently failed
# to extract "win32_display_adapter" - a false NEGATIVE that made this
# gate reprove a legitimately-named, closed-list adapter as "no class
# declaration found" (measured: port_privacy_test failing on a clean
# win32_display_adapter addition, GODS_LAWS.md L-40 - a gate that
# cannot even recognize a correctly-named class is not proving the
# closed list, it is proving its own regex). snake_case identifiers in
# this project's own convention (GODS_LAWS.md L-21) are lowercase
# letters, digits and underscores - the character class now matches
# that convention exactly, not a subset of it.
adapter_class_name_in_file() {
    class_decl="$(grep -oE 'class [a-z0-9_]+_adapter\b' "$1" 2>/dev/null | head -n 1 | awk '{ print $2 }')"
    if [ -n "$class_decl" ]; then
        printf '%s\n' "$class_decl"
        return 0
    fi
    grep -oE 'using[[:space:]]+[a-z0-9_]+[[:space:]]*=[[:space:]]*[a-z0-9_]+_adapter\b' "$1" 2>/dev/null \
        | head -n 1 | awk '{ print $NF }'
}

name_is_known_adapter() {
    candidate="$1"
    for known in $KNOWN_ADAPTER_CLASSES; do
        [ "$candidate" = "$known" ] && return 0
    done
    return 1
}

check_adapter_list_is_closed() {
    root="$1"
    files="$(all_adapter_headers "$root")"
    count=0
    [ -n "$files" ] && count="$(printf '%s\n' "$files" | wc -l | tr -d ' ')"
    require_nonempty_scan "src/**/*_adapter.hpp + tests/**/*_adapter.hpp" "$count" || return 1

    unknown=""
    printf '%s\n' "$files" | while IFS= read -r f; do
        class_name="$(adapter_class_name_in_file "$f")"
        if [ -z "$class_name" ]; then
            printf '%s: no "class *_adapter" declaration found\n' "$f"
            continue
        fi
        if ! name_is_known_adapter "$class_name"; then
            printf '%s: class %s is not on the closed adapter list (%s)\n' \
                "$f" "$class_name" "$KNOWN_ADAPTER_CLASSES"
        fi
    done > "${TMPDIR:-/tmp}/check_port_privacy_unknown.$$"
    unknown="$(cat "${TMPDIR:-/tmp}/check_port_privacy_unknown.$$")"
    rm -f "${TMPDIR:-/tmp}/check_port_privacy_unknown.$$"

    if [ -n "$unknown" ]; then
        echo "check_port_privacy.sh: adapter(s) outside the closed list (GODS_LAWS.md L-40):" >&2
        echo "$unknown" >&2
        return 1
    fi
    echo "check_port_privacy.sh: (b) $count adapter header(s) scanned, all on the closed list"
}

# --- (c) no public header reaches into src/platform/ -----------------

public_headers() {
    root="$1"
    find "$root/include/glintfx" -type f -name '*.hpp' 2>/dev/null | sort
}

file_leaks_platform_include() {
    grep -nE '#include[[:space:]]*["<]platform/' "$1" 2>/dev/null
}

check_public_headers_do_not_leak_platform() {
    root="$1"
    files="$(public_headers "$root")"
    count=0
    [ -n "$files" ] && count="$(printf '%s\n' "$files" | wc -l | tr -d ' ')"
    require_nonempty_scan "include/glintfx/**/*.hpp" "$count" || return 1

    leaks=""
    leaks="$(printf '%s\n' "$files" | while IFS= read -r f; do file_leaks_platform_include "$f" | sed "s#^#$f:#"; done)"
    if [ -n "$leaks" ]; then
        echo "check_port_privacy.sh: public header reaches into src/platform/ (GODS_LAWS.md L-19):" >&2
        echo "$leaks" >&2
        return 1
    fi
    echo "check_port_privacy.sh: (c) $count public header(s) scanned, none reach into src/platform/"
}

# --- (d) the built .so exports zero symbol naming a closed name ------

check_library_exports_no_port_symbol() {
    library_path="$1"

    if [ "$library_path" = "NONE" ]; then
        echo "check_port_privacy.sh: (d) skipped - BUILD_SHARED_LIBS=OFF, a static archive has no dynamic symbol table to inspect"
        return 0
    fi
    [ -f "$library_path" ] || fail "shared library not found: $library_path"
    command -v nm >/dev/null 2>&1 || fail "'nm' not found in PATH (no silent skip)"
    command -v c++filt >/dev/null 2>&1 || fail "'c++filt' not found in PATH (no silent skip)"

    symbols="$(nm -D --defined-only "$library_path" 2>/dev/null | awk '{ print $NF }')"
    total=0
    [ -n "$symbols" ] && total="$(printf '%s\n' "$symbols" | wc -l | tr -d ' ')"
    require_nonempty_scan "$library_path dynamic symbols" "$total" || return 1

    demangled="$(printf '%s\n' "$symbols" | c++filt)"

    hits=""
    for name in $KNOWN_ADAPTER_CLASSES $KNOWN_PORT_NAMES; do
        found="$(printf '%s\n' "$demangled" | grep -F "$name" || true)"
        [ -n "$found" ] && hits="$hits
$found"
    done

    if [ -n "$hits" ]; then
        echo "check_port_privacy.sh: port/adapter symbol found in the dynamic symbol table:" >&2
        echo "$hits" >&2
        return 1
    fi
    echo "check_port_privacy.sh: (d) $total dynamic symbol(s) scanned in $library_path, none name the port or an adapter"
}

check_port_privacy() {
    root="$1"
    library_path="$2"

    check_production_adapters_present "$root" || return 1
    check_adapter_list_is_closed "$root" || return 1
    check_public_headers_do_not_leak_platform "$root" || return 1
    check_library_exports_no_port_symbol "$library_path" || return 1
}

# --- real mode ---------------------------------------------------------

real_main() {
    [ "$#" -eq 2 ] || fail "usage: check_port_privacy.sh <source-root-directory> <path-to-.so-or-NONE>"
    [ -d "$1" ] || fail "directory not found: $1"
    check_port_privacy "$1" "$2" || fail "port privacy violation found (see message above)"
}

# --- fixtures and controls for --selftest -----------------------------

make_scratch_workdir() {
    mktemp -d "${TMPDIR:-/tmp}/glintfx-port-privacy-selftest-XXXXXX"
}

make_clean_fixture() {
    root="$1"
    mkdir -p "$root/src/platform/wayland" "$root/tests/fake" "$root/include/glintfx/core"
    printf 'class wayland_display_adapter {};\n' > "$root/src/platform/wayland/display_adapter.hpp"
    printf 'class fake_display_adapter {};\n' > "$root/tests/fake/fake_display_adapter.hpp"
    printf '#pragma once\n#include <cstdint>\n' > "$root/include/glintfx/core/err.hpp"
}

selftest_positive_control() {
    scratch="$1"
    root="$scratch/positive"
    make_clean_fixture "$root"

    if output="$(check_port_privacy "$root" "NONE" 2>&1)"; then
        echo "selftest: controle POSITIVO OK (fixture limpa aprovada)"
        return 0
    fi
    echo "selftest: controle POSITIVO FALHOU (fixture limpa deveria ter sido aprovada)" >&2
    printf '%s\n' "$output" >&2
    return 1
}

# Negative control (b): plants an adapter class NOT on the closed list.
selftest_negative_control_unknown_adapter() {
    scratch="$1"
    root="$scratch/negative-unknown-adapter"
    make_clean_fixture "$root"
    printf 'class evil_display_adapter {};\n' > "$root/src/platform/wayland/intruder_adapter.hpp"

    if output="$(check_port_privacy "$root" "NONE" 2>&1)"; then
        echo "selftest: controle NEGATIVO (adaptador desconhecido) FALHOU (deveria ter sido reprovado)" >&2
        return 1
    fi
    if ! printf '%s\n' "$output" | grep -qF "evil_display_adapter"; then
        echo "selftest: controle NEGATIVO (adaptador desconhecido) FALHOU (reprovou, mas nao citou evil_display_adapter)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    echo "selftest: controle NEGATIVO (adaptador desconhecido) OK (evil_display_adapter pego e citado)"
    return 0
}

# Negative control (c): plants a public header that reaches into
# src/platform/.
selftest_negative_control_public_leak() {
    scratch="$1"
    root="$scratch/negative-public-leak"
    make_clean_fixture "$root"
    printf '#pragma once\n#include "platform/port/display_connection_port.hpp"\n' \
        > "$root/include/glintfx/core/leaky.hpp"

    if output="$(check_port_privacy "$root" "NONE" 2>&1)"; then
        echo "selftest: controle NEGATIVO (vazamento publico) FALHOU (deveria ter sido reprovado)" >&2
        return 1
    fi
    if ! printf '%s\n' "$output" | grep -qF "leaky.hpp"; then
        echo "selftest: controle NEGATIVO (vazamento publico) FALHOU (reprovou, mas nao citou leaky.hpp)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    echo "selftest: controle NEGATIVO (vazamento publico) OK (leaky.hpp pego e citado)"
    return 0
}

# Empty-scan floor (a): src/platform/ exists but has no adapter header
# at all - the exact case a "0 encontrados, sai verde" regression would
# produce.
selftest_empty_scan_control() {
    scratch="$1"
    root="$scratch/empty"
    mkdir -p "$root/src/platform" "$root/tests" "$root/include/glintfx"

    if output="$(check_port_privacy "$root" "NONE" 2>&1)"; then
        echo "selftest: controle de VARREDURA VAZIA FALHOU (arvore sem adaptador deveria ter sido recusada, mas passou)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    if ! printf '%s\n' "$output" | grep -qF "varredura vazia"; then
        echo "selftest: controle de VARREDURA VAZIA FALHOU (recusou, mas nao disse 'varredura vazia')" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    echo "selftest: controle de VARREDURA VAZIA OK (arvore sem adaptador recusada)"
    return 0
}

selftest_main() {
    scratch="$(make_scratch_workdir)"
    trap 'rm -rf "$scratch"' EXIT

    overall=0
    selftest_positive_control "$scratch" || overall=1
    selftest_negative_control_unknown_adapter "$scratch" || overall=1
    selftest_negative_control_public_leak "$scratch" || overall=1
    selftest_empty_scan_control "$scratch" || overall=1

    if [ "$overall" -ne 0 ]; then
        echo "check_port_privacy.sh --selftest: FALHOU (ver acima)" >&2
        exit 1
    fi
    echo "check_port_privacy.sh --selftest: os quatro controles OK"
}

main() {
    if [ "${1:-}" = "--selftest" ]; then
        selftest_main
    else
        real_main "$@"
    fi
}

main "$@"
