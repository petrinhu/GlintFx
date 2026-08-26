#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_hygiene_coverage.sh - mechanical coverage gate for
# tests/header_hygiene_test.cpp (HDR-HYGIENE-FIX-2). Turns the "every
# new public header enters this translation unit" comment-only
# contract that used to sit at the top of header_hygiene_test.cpp into
# a portal that fails the build when a public header is added and NOT
# wired into that hygiene test's include list. The previous, text-only
# version of the contract was never applied by any script - that is
# the finding this gate exists to close.
#
# METHOD: ENUMERATION, not directed search. Directed search ("grep for
# headers used somewhere") finds what you already suspect; enumeration
# finds what you did not know to suspect - house rule of this project
# (see the "enumere o espaco pequeno" lesson referenced from
# GODS_LAWS.md L-27's lineage). Every *.hpp found under <include_dir>
# is listed, and each one must have a literal `#include <path>` line
# (relative to <include_dir> itself) inside <tu_file>.
#
# LIMITATION, declared here on purpose, not discovered by a future
# reviewer the hard way: headers GENERATED at configure or build time
# (export.hpp, version_macros.hpp - see cmake/GlintfxLibrary.cmake,
# glintfx_generate_export_header) are never under <include_dir> in the
# SOURCE tree; they are written under the build directory's generated
# include path instead. This gate enumerates what is COMMITTED. It
# treats generated headers as covered by transitivity: glintfx never
# ships without generating them, and header_hygiene_test.cpp already
# includes glintfx/version_macros.hpp directly.
#
# Usage:
#   check_hygiene_coverage.sh <include_dir> <tu_file>
#   check_hygiene_coverage.sh --selftest
#
# The real invocation (see tests/CMakeLists.txt) passes
# "${PROJECT_SOURCE_DIR}/include" as <include_dir>, so a header at
# include/glintfx/core/version.hpp enumerates as
# "glintfx/core/version.hpp" and the required line is literally
# `#include <glintfx/core/version.hpp>`.
#
# --selftest runs three controls (positive, negative, empty-scan
# floor) against a throwaway fixture tree under mktemp, never against
# the real <include_dir>/<tu_file> - see selftest_main() below.
#
# Each function below does one thing (GODS_LAWS.md L-17).

set -eu

fail() {
    echo "check_hygiene_coverage.sh: $1" >&2
    exit 1
}

# --- enumeration -------------------------------------------------------

# Prints one path per line, relative to <include_dir>, for every
# public header found under it - e.g. "glintfx/core/version.hpp" when
# <include_dir> is the project's top-level include/. Empty output (not
# an error by itself here) means the caller found nothing;
# require_nonempty_scan below is the floor that turns that into a hard
# failure.
#
# Enumerates *.hpp, *.h, *.hh and *.hxx (achado F6 reproduzido em
# 26/08/2026: a versao anterior so olhava *.hpp, e um legacy.h ao lado
# de um .hpp coberto fazia o portao imprimir "1 header(s) publico(s)
# cobertos" com saida 0 sem citar o arquivo ignorado em lugar nenhum -
# incoerente com check_layers.sh:51, que ja trata .h como extensao
# legitima deste repositorio).
enumerate_public_headers() {
    include_dir="$1"
    find "$include_dir" -type f \
        \( -name '*.hpp' -o -name '*.h' -o -name '*.hh' -o -name '*.hxx' \) \
        2>/dev/null | while IFS= read -r f; do
        printf '%s\n' "${f#"$include_dir"/}"
    done | sort
}

# Non-fatal on its own (returns, does not exit): --selftest's
# empty-scan control needs to observe this failure and keep running to
# check the MESSAGE, so exiting here would break that caller. The real
# check_coverage below turns this into a hard failure itself.
require_nonempty_scan() {
    headers="$1"
    if [ -z "$headers" ]; then
        echo "check_hygiene_coverage.sh: varredura vazia (0 headers)" >&2
        return 1
    fi
}

count_lines() {
    if [ -z "$1" ]; then
        echo 0
        return
    fi
    printf '%s\n' "$1" | wc -l | tr -d ' '
}

# Achado F6 (parte 2, reproduzido em 26/08/2026): o casamento antigo
# era literal e cego a comentario (`grep -qF`), entao
# "// #include <glintfx/...>" contava como coberto - um header cuja
# inclusao hostil foi desligada num refactor passava como coberto sem
# exercitar nada. Filtra fora as linhas comentadas (comecam, apos
# espaco em branco opcional, com "//") ANTES do grep literal; uma
# inclusao real na mesma TU ainda casa normalmente.
header_is_included_in_tu() {
    header="$1"
    tu_file="$2"
    grep -vE '^[[:space:]]*//' "$tu_file" | grep -qF "#include <${header}>"
}

missing_headers() {
    headers="$1"
    tu_file="$2"
    printf '%s\n' "$headers" | while IFS= read -r header; do
        [ -n "$header" ] || continue
        header_is_included_in_tu "$header" "$tu_file" || printf '%s\n' "$header"
    done
}

# --- coverage check ------------------------------------------------------
#
# Real implementation (HDR-HYGIENE-FIX-2): a stub that always approved
# ran here first, red, to prove the negative and empty-scan --selftest
# controls actually catch a gate that rubber-stamps everything - see
# the commit history of this file for that red run's captured output.

check_coverage() {
    include_dir="$1"
    tu_file="$2"

    headers="$(enumerate_public_headers "$include_dir")"
    require_nonempty_scan "$headers" || return 1

    header_count="$(count_lines "$headers")"
    missing="$(missing_headers "$headers" "$tu_file")"

    if [ -n "$missing" ]; then
        echo "check_hygiene_coverage.sh: header(s) publico(s) sem cobertura em $tu_file:" >&2
        printf '%s\n' "$missing" >&2
        return 1
    fi

    echo "check_hygiene_coverage.sh: $header_count header(s) publico(s) cobertos em $tu_file"
}

# --- real mode -----------------------------------------------------------

require_real_args() {
    [ "$#" -eq 2 ] || fail "usage: check_hygiene_coverage.sh <include_dir> <tu_file>"
    [ -d "$1" ] || fail "include dir not found: $1"
    [ -f "$2" ] || fail "tu file not found: $2"
}

real_main() {
    require_real_args "$@"
    check_coverage "$1" "$2" || fail "cobertura de higiene de header incompleta (ver mensagem acima)"
}

# --- selftest fixtures and controls ---------------------------------------

make_scratch_workdir() {
    mktemp -d "${TMPDIR:-/tmp}/glintfx-hygiene-coverage-XXXXXX"
}

# Positive control: one header, and the TU includes it. Expected: pass.
selftest_positive_control() {
    scratch="$1"
    include_dir="$scratch/positive/include"
    mkdir -p "$include_dir/pkg"
    printf '// fixture header\n' > "$include_dir/pkg/foo.hpp"
    tu_file="$scratch/positive/tu.cpp"
    printf '#include <pkg/foo.hpp>\n' > "$tu_file"

    if output="$(check_coverage "$include_dir" "$tu_file" 2>&1)"; then
        echo "selftest: controle POSITIVO OK (fixture completa aprovada)"
        return 0
    fi
    echo "selftest: controle POSITIVO FALHOU (fixture completa deveria ter sido aprovada)" >&2
    printf '%s\n' "$output" >&2
    return 1
}

# Negative control: two headers, TU includes only one. Expected: fail,
# naming the missing header (pkg/bar.hpp) in the message.
selftest_negative_control() {
    scratch="$1"
    include_dir="$scratch/negative/include"
    mkdir -p "$include_dir/pkg"
    printf '// fixture header\n' > "$include_dir/pkg/foo.hpp"
    printf '// fixture header, deliberately NOT included below\n' > "$include_dir/pkg/bar.hpp"
    tu_file="$scratch/negative/tu.cpp"
    printf '#include <pkg/foo.hpp>\n' > "$tu_file"

    if output="$(check_coverage "$include_dir" "$tu_file" 2>&1)"; then
        echo "selftest: controle NEGATIVO FALHOU (deveria acusar pkg/bar.hpp faltando, mas passou)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    if ! printf '%s\n' "$output" | grep -qF "pkg/bar.hpp"; then
        echo "selftest: controle NEGATIVO FALHOU (acusou algo, mas nao citou pkg/bar.hpp na mensagem)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    echo "selftest: controle NEGATIVO OK (acusou pkg/bar.hpp corretamente)"
    return 0
}

# Empty-scan floor: directory with zero headers. Expected: fail with
# "varredura vazia" in the message - a portal that scans nothing and
# prints green is the defect this project is explicitly building this
# gate to never ship again.
selftest_empty_scan_control() {
    scratch="$1"
    include_dir="$scratch/empty/include"
    mkdir -p "$include_dir"
    tu_file="$scratch/empty/tu.cpp"
    printf '// nada a incluir\n' > "$tu_file"

    if output="$(check_coverage "$include_dir" "$tu_file" 2>&1)"; then
        echo "selftest: controle de VARREDURA VAZIA FALHOU (deveria recusar diretorio sem headers, mas passou)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    if ! printf '%s\n' "$output" | grep -qF "varredura vazia"; then
        echo "selftest: controle de VARREDURA VAZIA FALHOU (recusou, mas nao disse 'varredura vazia')" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    echo "selftest: controle de VARREDURA VAZIA OK (recusou diretorio sem headers)"
    return 0
}

# Achado F6 (parte 1, reproduzido em 26/08/2026): um header .h nao
# incluido, ao lado de um .hpp coberto, tem de aparecer como faltando -
# antes do conserto ele ficava INVISIVEL (a enumeracao nem o olhava), e
# a fixture toda passava citando so o .hpp. Nomeia legacy.h na mensagem
# igual ao controle negativo acima nomeia pkg/bar.hpp.
selftest_extension_h_control() {
    scratch="$1"
    include_dir="$scratch/extension_h/include"
    mkdir -p "$include_dir/pkg"
    printf '// fixture .hpp, coberta\n' > "$include_dir/pkg/foo.hpp"
    printf '// fixture .h, deliberadamente NAO incluida abaixo\n' > "$include_dir/pkg/legacy.h"
    tu_file="$scratch/extension_h/tu.cpp"
    printf '#include <pkg/foo.hpp>\n' > "$tu_file"

    if output="$(check_coverage "$include_dir" "$tu_file" 2>&1)"; then
        echo "selftest: controle de EXTENSAO .h FALHOU (deveria acusar pkg/legacy.h faltando, mas passou - o .h ficou invisivel)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    if ! printf '%s\n' "$output" | grep -qF "pkg/legacy.h"; then
        echo "selftest: controle de EXTENSAO .h FALHOU (reprovou, mas nao citou pkg/legacy.h na mensagem)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    echo "selftest: controle de EXTENSAO .h OK (pkg/legacy.h enumerado e acusado como faltando)"
    return 0
}

# Achado F6 (parte 2, reproduzido em 26/08/2026): uma inclusao
# COMENTADA ("// #include <...>") nao pode contar como cobertura - um
# header cuja inclusao hostil foi desligada num refactor tem de
# reprovar, nao passar sem exercitar nada.
selftest_commented_include_control() {
    scratch="$1"
    include_dir="$scratch/commented/include"
    mkdir -p "$include_dir/pkg"
    printf '// fixture header, cuja inclusao abaixo esta comentada\n' > "$include_dir/pkg/disabled.hpp"
    tu_file="$scratch/commented/tu.cpp"
    printf '// #include <pkg/disabled.hpp>\n' > "$tu_file"

    if output="$(check_coverage "$include_dir" "$tu_file" 2>&1)"; then
        echo "selftest: controle de INCLUSAO COMENTADA FALHOU (deveria acusar pkg/disabled.hpp faltando, mas passou - grep literal casou dentro do comentario)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    if ! printf '%s\n' "$output" | grep -qF "pkg/disabled.hpp"; then
        echo "selftest: controle de INCLUSAO COMENTADA FALHOU (reprovou, mas nao citou pkg/disabled.hpp na mensagem)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    echo "selftest: controle de INCLUSAO COMENTADA OK (inclusao comentada nao contou como cobertura)"
    return 0
}

selftest_main() {
    scratch="$(make_scratch_workdir)"
    trap 'rm -rf "$scratch"' EXIT

    overall=0
    selftest_positive_control "$scratch" || overall=1
    selftest_negative_control "$scratch" || overall=1
    selftest_empty_scan_control "$scratch" || overall=1
    selftest_extension_h_control "$scratch" || overall=1
    selftest_commented_include_control "$scratch" || overall=1

    if [ "$overall" -ne 0 ]; then
        echo "check_hygiene_coverage.sh --selftest: FALHOU (ver acima)" >&2
        exit 1
    fi
    echo "check_hygiene_coverage.sh --selftest: os cinco controles OK"
}

main() {
    if [ "${1:-}" = "--selftest" ]; then
        selftest_main
    else
        real_main "$@"
    fi
}

main "$@"
