#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_public_name_collision.sh - CI gate for docs/api-conventions.md
# R6 ("Nome publico nunca colide com macro de sistema nem com nome ja
# usado pela biblioteca padrao"). Replaces the HAND-TYPED name list
# that R6's own "Auditoria de colisao" table used to be with a
# MECHANICAL scan of two real things: the public headers this build
# actually ships (include/), and the SYSTEM headers the compiler
# actually searches - not a curated /usr/include grep.
#
# ORIGIN (TODO.md, "Desvios", 25/08/2026): the CORE-ERROR adversarial
# review swept /usr/include for real and found a collision candidate
# ("value", a p11-kit PKCS11 macro) the hand-typed list never had a
# chance to name, because nobody thought to type "value" into it. The
# achado's own words: "o valor do item nao e aquele nome, e o metodo:
# lista curada envelhece e depende de alguem lembrar, enquanto
# varredura real do sistema enumera" (GODS_LAWS.md L-40). This script
# is that automation, built before the next API review (ASSET-LOAD,
# ARCH-PORTS, CORE-COLOR, ...) has to repeat the same hand audit.
#
# WHAT "OUR NAMES" MEANS HERE, DECLARED (policy decision 1 of 3, see
# the ordem de servico this script was built against): every *.hpp/
# *.h/*.hh/*.hxx under <include_dir> is enumerated - not the .cpp
# implementation files. Only include/ is text the CONSUMER's
# preprocessor ever sees; a name colliding inside a .cpp is this
# project's OWN build failing immediately in CI, self-catching,
# already covered by the fact this whole test suite would not have
# compiled - it needs no dedicated gate. Within each public header,
# four categories are extracted MECHANICALLY (enumerate_names.awk,
# written to scratch below): type names (class/struct/enum class),
# enumerator names, function/method names (the FIRST identifier
# immediately before an opening paren on a line that looks like a
# declaration - GLINTFX_API, noexcept, [[nodiscard]], inline, static or
# explicit somewhere on it - deliberately the FIRST match only, so a
# constructor's member-init-list ("m_code(code)") or a one-line
# getter's body call ("m_storage.index()") are never mistaken for a
# name THIS library declared), and data-member names (a bare
# "TYPE name;" line with no parens, excluding statement keywords like
# return/if/for so a body line inside an inline function is not
# mistaken for a field). Declared limitation, not silently dropped:
# function PARAMETER names are NOT scanned - the same four categories
# docs/api-conventions.md's own "Auditoria de colisao" table already
# promised to cover, automated rather than reinvented with a wider
# net.
#
# WHY std::-INHERITED METHOD NAMES ARE DELIBERATELY OUT OF SCOPE
# (policy decision 1, continued - measured live, not assumed): an
# earlier version of enumerate_names.awk captured EVERY identifier
# before a paren on a declaration line, not just the first, and it
# found a REAL system collision this way - std::variant::index(),
# called inside gltfx_rslt's one-line has_value()/has_error() getters,
# collides with a `#define index(s,c) ...` in the legacy X-Window
# system's own "Xos dot h" header (measured live on this machine; the
# path is spelled out in prose here, not as a literal path string, on
# purpose - tests/tools/check_no_x11.sh greps this whole repository for
# that system's header paths, GODS_LAWS.md L-05, and a literal mention
# here would trip a false positive on a comment that only explains a
# scoping decision, the same reason version.hpp's own header comment
# gives for the identical choice). That name is not ours to rename:
# glintfx does not choose std::variant's method names, and a consumer
# who separately pulls in an X11 header already carries that risk with
# every other use of std::variant in their own program, independent of
# glintfx. Scanning it here would fail commits over a name this
# project cannot fix by renaming - noise the achado explicitly warns
# against ("reprovar isso gera ruido"). The FIRST-match-only rule
# above closes this cleanly: it happens to also be the correct
# heuristic for "the name being DECLARED", not "every name being USED
# on this line".
#
# WHAT COUNTS AS A COLLISION, DECLARED (policy decision 2): a system
# header #define-ing one of our names is a FAILURE, UNLESS one of TWO
# neutralizing shapes applies. First (the original 25/08/2026 finding):
# that same file also #undef's the same name later (grep on that one
# file) - the exact shape the achado's own finding was ("o header o
# define e o desfaz no mesmo arquivo... inofensivo na pratica"). A name
# defined and undefined within one file is gone from the preprocessor's
# active state by the time that file's own #include finishes; the only
# way it could still bite a consumer is if OUR header were transitively
# #include'd by that SAME system header between the #define and the
# #undef line, which no system header does for a third-party library it
# does not know exists. Second (added 26/08/2026, measured live in Arch
# and CachyOS containers against attr's own error_context.h): the
# #define is textually nested inside "#ifdef GUARD"/"#if defined(GUARD)"
# for a symbol nothing in a NORMAL include chain ever defines, so the
# macro never becomes active in default preprocessing - proved by
# asking the SAME real compiler (macro_active_under_default_preprocessing
# below), not a hand-rolled nested-#ifdef parser, echoing policy
# decision 3's own "ask the compiler, never assume". Neither shape is
# silently dropped - classify_matches() below tags EACH neutralized line
# with WHICH of the two reasons applied - they print in a separate,
# clearly labelled section so the count, and the reason, are never
# hidden (GODS_LAWS.md L-40's "a contagem aparece na saida, mesmo quando
# passa").
#
# WHERE THIS SCANS, DECLARED (policy decision 3): the compiler's OWN
# system include search path, discovered live via
# `${cxx} -E -Wp,-v -xc++ -` (the same "#include <...> search starts
# here:" / "End of search list." markers every GCC frontend prints,
# and every compiler configured for this project's four Linux CI legs
# is a GCC frontend - see .github/workflows/ci.yml's `instalar:`
# lines). This is NOT a hardcoded /usr/include: it also reaches the
# compiler's OWN bundled C++ standard library headers
# (.../include/c++/16, on this machine), which /usr/include never
# contains, and it recuses cleanly on a distro where /usr/include is
# thin or absent (a minimal container image) because it asks the
# compiler, never assumes a path. If discovery yields ZERO directories
# - compiler missing, or a broken toolchain - this is a HARD FAILURE
# (require_nonempty_scan below), never a silent skip: GODS_LAWS.md
# L-40 names "recusar alto e melhor que aprovar em silencio" as the
# rule, and it applies here exactly as written.
#
# PORTABILITY, DECLARED (not silently claimed): POSIX sh, and the
# `${cxx} -E -Wp,-v -xc++ -` discovery trick is a GCC-frontend
# convention. This gate runs where `sh`, `grep`, `awk`, `sed`, `find`
# and a GCC-family C++ compiler are all present - the four Linux CI
# legs (Fedora primary, Ubuntu, Arch, CachyOS), the same declared
# downgrade every other tests/tools/*.sh gate in this file carries
# (check_layers.sh, check_no_x11.sh, check_hygiene_coverage.sh, ...).
# It does NOT run on Windows: no `sh` by default, and the Windows SDK
# macro-collision risk (min/max/ERROR/DELETE/IN/OUT/CONST/VOID/TRUE/
# FALSE/interface/small/near/far/STRICT) is covered by a DIFFERENT,
# pre-existing mechanism that this gate does not replace -
# tests/header_hygiene_test.cpp's
# core_error_use_sites_survive_hostile_system_headers, which compiles
# every CORE-ERROR public identifier as a real expression under
# windows.h on the Windows CI leg (#ifdef _WIN32 already selects that
# path). This gate complements that one for the FOUR LINUX LEGS with a
# REAL system-header sweep instead of the hand-typed Windows macro list
# docs/api-conventions.md's own "Limite desta auditoria" section
# already declares as a documented gap (enumeration against a
# documented list, not a live compile) - this script does not close
# that Windows gap; it closes the LINUX side of the SAME achado.
#
# Usage:
#   check_public_name_collision.sh <include_dir> <cxx-compiler>
#   check_public_name_collision.sh --selftest [cxx-compiler]
#
# --selftest runs seven controls against throwaway fixtures under
# mktemp, never against the real include_dir or the real machine's
# system headers (GODS_LAWS.md L-40's three mandatory controls -
# positive, negative, empty-scan - plus FOUR specific to this gate:
# the same-file #undef neutralization from policy decision 2; the
# guard-inactive and guard-active pair added 26/08/2026 for policy
# decision 2's second neutralizing shape (a macro alive only under an
# #ifdef nothing normally defines does not reprove, but the SAME macro
# with its guard symbol actually defined still does - the regression
# guard against loosening this too far); and a SECOND empty-scan floor
# for the system-header side, independent of the "our names" side).
# See selftest_main() below.
#
# Each function below does one thing (GODS_LAWS.md L-17).

set -eu

fail() {
    echo "check_public_name_collision.sh: $1" >&2
    exit 1
}

count_lines() {
    if [ -z "$1" ]; then
        echo 0
        return
    fi
    printf '%s\n' "$1" | wc -l | tr -d ' '
}

# --- our names: enumeration from include_dir -------------------------

# Same four extensions check_hygiene_coverage.sh already enumerates
# (F6 finding, 26/08/2026): *.hpp/*.h/*.hh/*.hxx, so a legacy .h
# public header is never invisible here either.
enumerate_public_headers() {
    include_dir="$1"
    find "$include_dir" -type f \
        \( -name '*.hpp' -o -name '*.h' -o -name '*.hh' -o -name '*.hxx' \) \
        2>/dev/null
}

# Writes the name-extraction awk program to <dest>. A separate file,
# not an inline `awk '...'` one-liner, because the program has five
# distinct extraction rules (GODS_LAWS.md L-17: this function's ONE job
# is placing that text on disk, not deciding what it says).
write_enumerate_names_awk() {
    dest="$1"
    cat > "$dest" <<'AWK_PROGRAM'
BEGIN { in_enum = 0 }

/^[[:space:]]*$/ { next }

# enum class NAME [: underlying] { ... enumerators ... };
/^[[:space:]]*enum[[:space:]]+class[[:space:]]+[A-Za-z_][A-Za-z0-9_]*/ {
    line = $0
    sub(/^[[:space:]]*enum[[:space:]]+class[[:space:]]+/, "", line)
    match(line, /^[A-Za-z_][A-Za-z0-9_]*/)
    print substr(line, RSTART, RLENGTH)
    if (line ~ /\{/) { in_enum = 1 }
    next
}

in_enum && /^[[:space:]]*\}/ { in_enum = 0; next }

in_enum {
    line = $0
    gsub(/^[[:space:]]+/, "", line)
    if (match(line, /^[A-Za-z_][A-Za-z0-9_]*/)) {
        print substr(line, RSTART, RLENGTH)
    }
    next
}

# class NAME / class [[nodiscard]] NAME / struct NAME (declaration,
# definition or forward-declaration alike - all three are literal text
# in a public header a hostile macro can still mangle).
/(^|[[:space:]])(class|struct)[[:space:]]+(\[\[nodiscard\]\][[:space:]]+)?[A-Za-z_][A-Za-z0-9_]*/ {
    line = $0
    if (match(line, /(class|struct)[[:space:]]+(\[\[nodiscard\]\][[:space:]]+)?[A-Za-z_][A-Za-z0-9_]*/)) {
        seg = substr(line, RSTART, RLENGTH)
        match(seg, /[A-Za-z_][A-Za-z0-9_]*$/)
        print substr(seg, RSTART, RLENGTH)
    }
}

# Function/method declaration or definition: the FIRST identifier
# immediately before an opening paren, on a line carrying at least one
# marker this codebase's declarations always show somewhere on the
# line (GLINTFX_API, noexcept, [[nodiscard]], inline, static,
# explicit). FIRST match only - see this script's own header comment,
# "WHY std::-INHERITED METHOD NAMES ARE DELIBERATELY OUT OF SCOPE",
# for why capturing every match instead of just the first one is a
# real, measured mistake, not a hypothetical one.
/\(/ {
    line = $0
    is_decl = 0
    if (line ~ /noexcept/) is_decl = 1
    if (line ~ /GLINTFX_API/) is_decl = 1
    if (line ~ /\[\[nodiscard\]\]/) is_decl = 1
    if (line ~ /(^|[[:space:]])inline([[:space:]]|$)/) is_decl = 1
    if (line ~ /(^|[[:space:]])static([[:space:]]|$)/) is_decl = 1
    if (line ~ /(^|[[:space:]])explicit([[:space:]]|$)/) is_decl = 1
    if (is_decl && match(line, /[A-Za-z_][A-Za-z0-9_]*[[:space:]]*\(/)) {
        seg = substr(line, RSTART, RLENGTH)
        gsub(/[[:space:]]*\($/, "", seg)
        candidate = seg
        if (candidate != "if" && candidate != "noexcept" && candidate != "sizeof" \
            && candidate != "static_assert" && candidate != "explicit") {
            print candidate
        }
    }
}

# Data member: a bare "TYPE name;" line with no parens at all (a
# function declaration always has one). Excludes using/typedef/
# namespace/template lines, and excludes statement-keyword lines
# (return/break/continue/if/for/while/else) so a body statement inside
# an inline function ("return *this;") is never mistaken for a field.
!/\(/ && /;[[:space:]]*$/ {
    line = $0
    gsub(/=.*;/, ";", line)
    if (line !~ /^[[:space:]]*(using|typedef|namespace|template|return|break|continue|if|for|while|else)([[:space:]]|;|\*)/ && \
        match(line, /[A-Za-z_][A-Za-z0-9_]*[[:space:]]*;[[:space:]]*$/)) {
        seg = substr(line, RSTART, RLENGTH)
        match(seg, /[A-Za-z_][A-Za-z0-9_]*/)
        print substr(seg, RSTART, RLENGTH)
    }
}
AWK_PROGRAM
}

# Strips a trailing "// ..." comment from every line before handing the
# file to the awk program above - without this, a NOLINT comment's own
# prose could contain a word matching an extraction pattern by
# accident. No /* */ block comments exist in this project's public
# headers today (grep -n '/\*' include/glintfx/core/*.hpp confirmed
# empty on 26/08/2026); if one is ever added, this line stripping alone
# would not hide it, and that is a real, not-yet-covered gap - see
# "What this gate does NOT cover" in this script's own report output.
strip_line_comments() {
    sed -E 's~//.*~~' "$1"
}

enumerate_our_names() {
    include_dir="$1"
    awk_file="$2"
    headers="$(enumerate_public_headers "$include_dir")"
    [ -n "$headers" ] || return 0
    printf '%s\n' "$headers" | while IFS= read -r h; do
        [ -n "$h" ] || continue
        strip_line_comments "$h" | awk -f "$awk_file"
    done | sort -u
}

# --- system side: where to look, and what is defined there -----------

# GCC-frontend convention (see this script's own header, policy
# decision 3): the search path a real compile would use, never a
# hardcoded /usr/include.
discover_system_include_dirs() {
    cxx="$1"
    : | "$cxx" -E -Wp,-v -xc++ - 2>&1 | awk '
        /#include <\.\.\.> search starts here:/ { on=1; next }
        /End of search list\./ { on=0 }
        on { sub(/^ /, ""); print }
    '
}

list_files_under_dirs() {
    dirs="$1"
    [ -n "$dirs" ] || return 0
    printf '%s\n' "$dirs" | while IFS= read -r d; do
        [ -n "$d" ] && [ -d "$d" ] && find "$d" -type f 2>/dev/null
    done
}

build_alternation() {
    names="$1"
    printf '%s\n' "$names" | awk '
        BEGIN { first = 1 }
        { if ($0 != "") { if (!first) printf "|"; printf "%s", $0; first = 0 } }
        END { print "" }
    '
}

# Every "#define NAME..." (object-like or function-like: the \b after
# NAME matches both "#define NAME value" and "#define NAME(args) ...")
# in any of <dirs>, for any of <names>. Raw grep -rn output, one match
# per line, "file:line:content" - classify_matches() below turns that
# into REAL vs NEUTRALIZED.
scan_defines_in_dirs() {
    dirs="$1"
    names="$2"
    alt="$(build_alternation "$names")"
    [ -n "$alt" ] || return 0
    pattern="^[[:space:]]*#[[:space:]]*define[[:space:]]+(${alt})\\b"
    printf '%s\n' "$dirs" | while IFS= read -r d; do
        # grep's exit 1 ("no matches", not an error) would otherwise be
        # the LAST command of this loop body's "&&" chain and trip
        # `set -e` under the subshell a command substitution runs in -
        # measured live: the positive selftest control aborted here
        # silently before this "|| true" was added, because its
        # fixture system header has zero matches by design.
        [ -n "$d" ] && [ -d "$d" ] && { grep -rnE "$pattern" "$d" 2>/dev/null || true; }
    done
}

# A #define is NEUTRALIZED (policy decision 2) if the SAME file also
# #undef's the SAME name, anywhere in the file.
name_is_undef_in_same_file() {
    file="$1"
    name="$2"
    grep -qE "^[[:space:]]*#[[:space:]]*undef[[:space:]]+${name}\\b" "$file" 2>/dev/null
}

# A #define is NEUTRALIZED under a SECOND, independent reason (policy
# decision 2, extended 26/08/2026 - the achado measured live in Arch
# and CachyOS containers, attr's own error_context.h): the line is
# textually nested inside "#ifdef GUARD" / "#if defined(GUARD)" for a
# symbol nothing in a NORMAL include chain ever defines, so the macro
# never becomes active. This does NOT hand-parse nested #ifdef/#elif/
# #endif in awk - that breaks on the first "#if defined(A) && !defined
# (B)" a real system header writes. It asks the SAME compiler this
# whole gate already trusts for the search path (policy decision 3) to
# preprocess that ONE file with ZERO extra flags (`-E -dM`, dumping
# every macro alive at end-of-file) and checks whether NAME is in that
# dump. If the compiler cannot even preprocess the file standalone
# (missing, unreadable, or a header that needs context this gate does
# not have), the function answers "active" - GODS_LAWS.md L-40's
# "recusar alto e melhor que aprovar em silencio" applied here: an
# unprovable case must never quietly turn into a neutralization.
macro_active_under_default_preprocessing() {
    file="$1"
    name="$2"
    cxx="$3"
    dm_output="$("$cxx" -E -dM -xc++ "$file" 2>/dev/null)"
    dm_status="$?"
    if [ "$dm_status" -ne 0 ] || [ -z "$dm_output" ]; then
        return 0
    fi
    printf '%s\n' "$dm_output" | grep -qE "^#define[[:space:]]+${name}\\b"
}

# Splits raw "file:line:content" matches into REAL and NEUTRALIZED,
# prefixed per-line so the two callers below (real_collisions,
# neutralized_collisions) can filter with one grep each. NEUTRALIZED
# lines carry a third field naming WHICH of the two reasons applied,
# so the report stays auditable (GODS_LAWS.md L-40 "contagem nunca
# escondida" applies to WHY, not just to the count).
classify_matches() {
    matches="$1"
    cxx="$2"
    printf '%s\n' "$matches" | while IFS= read -r rawline; do
        [ -n "$rawline" ] || continue
        file="$(printf '%s' "$rawline" | cut -d: -f1)"
        lineno="$(printf '%s' "$rawline" | cut -d: -f2)"
        content="$(printf '%s' "$rawline" | cut -d: -f3-)"
        name="$(printf '%s' "$content" | sed -E 's/^[[:space:]]*#[[:space:]]*define[[:space:]]+([A-Za-z_][A-Za-z0-9_]*).*/\1/')"
        if name_is_undef_in_same_file "$file" "$name"; then
            printf 'NEUTRALIZED %s:%s:%s:undef-mesmo-arquivo\n' "$file" "$lineno" "$name"
        elif ! macro_active_under_default_preprocessing "$file" "$name" "$cxx"; then
            printf 'NEUTRALIZED %s:%s:%s:guarda-inativa-por-padrao\n' "$file" "$lineno" "$name"
        else
            printf 'REAL %s:%s:%s\n' "$file" "$lineno" "$name"
        fi
    done
}

real_collisions() {
    printf '%s\n' "$1" | grep '^REAL ' | sed 's/^REAL //' || true
}

neutralized_collisions() {
    printf '%s\n' "$1" | grep '^NEUTRALIZED ' | sed 's/^NEUTRALIZED //' || true
}

# --- L-40 floor --------------------------------------------------------

require_nonempty_scan() {
    value="$1"
    what="$2"
    if [ -z "$value" ]; then
        echo "check_public_name_collision.sh: varredura vazia ($what)" >&2
        return 1
    fi
}

# --- the check itself ----------------------------------------------------

check_public_name_collision() {
    include_dir="$1"
    cxx="$2"
    scratch="$3"

    # This function OWNS writing into $scratch (the awk program below,
    # plus whatever future scratch file a caller adds) - it creates
    # its own workspace instead of trusting every caller to have done
    # it. Missing on purpose before 26/08/2026: CI on Ubuntu's dash
    # /bin/sh hit this directly (public_name_collision_selftest,
    # "cannot create .../empty-our-names-work/enumerate_names.awk:
    # Directory nonexistent") because dash aborts the WHOLE script on
    # a redirection failure even without set -e, unlike bash, which
    # merely fails that one command under set -e and let the selftest
    # limp on to a coincidentally-matching "varredura vazia" message -
    # masking the real bug on every developer machine that answers to
    # "sh" with bash (this one included, and Fedora/CachyOS/Arch's own
    # /bin/sh too, measured 26/08/2026).
    mkdir -p "$scratch"

    awk_file="$scratch/enumerate_names.awk"
    write_enumerate_names_awk "$awk_file"

    names="$(enumerate_our_names "$include_dir" "$awk_file")"
    require_nonempty_scan "$names" "0 nomes publicos enumerados em $include_dir" || return 1
    name_count="$(count_lines "$names")"

    sys_dirs="$(discover_system_include_dirs "$cxx")"
    require_nonempty_scan "$sys_dirs" "0 diretorios de sistema descobertos via '$cxx -E -Wp,-v -xc++ -' (compilador ausente ou toolchain quebrada)" || return 1
    dir_count="$(count_lines "$sys_dirs")"

    sys_files="$(list_files_under_dirs "$sys_dirs")"
    require_nonempty_scan "$sys_files" "0 arquivos de sistema varridos sob $dir_count diretorio(s) descoberto(s)" || return 1
    file_count="$(count_lines "$sys_files")"

    matches="$(scan_defines_in_dirs "$sys_dirs" "$names")"
    classified="$(classify_matches "$matches" "$cxx")"
    real="$(real_collisions "$classified")"
    neutralized="$(neutralized_collisions "$classified")"
    neutralized_count="$(count_lines "$neutralized")"
    [ -n "$neutralized" ] || neutralized_count=0

    if [ -n "$neutralized" ]; then
        echo "check_public_name_collision.sh: $neutralized_count colisao(oes) NEUTRALIZADA(S) (define+undef no mesmo arquivo, ou define ativo so sob guarda de simbolo que a inclusao normal nao define - motivo por linha abaixo, GODS_LAWS.md L-40 nao esconde a contagem nem o motivo):"
        printf '%s\n' "$neutralized"
    fi

    if [ -n "$real" ]; then
        echo "check_public_name_collision.sh: COLISAO (docs/api-conventions.md R6):" >&2
        printf '%s\n' "$real" >&2
        return 1
    fi

    echo "check_public_name_collision.sh: $name_count nome(s) publico(s) verificados contra $file_count arquivo(s) de sistema (${dir_count} diretorio(s)), 0 colisao real"
}

# --- real mode -----------------------------------------------------------

make_scratch_workdir() {
    mktemp -d "${TMPDIR:-/tmp}/glintfx-name-collision-XXXXXX"
}

require_real_args() {
    [ "$#" -eq 2 ] || fail "usage: check_public_name_collision.sh <include_dir> <cxx-compiler>"
    [ -d "$1" ] || fail "include dir not found: $1"
    command -v "$2" >/dev/null 2>&1 || fail "compiler not found in PATH: $2"
}

real_main() {
    require_real_args "$@"
    include_dir="$1"
    cxx="$2"

    scratch="$(make_scratch_workdir)"
    trap 'rm -rf "$scratch"' EXIT

    check_public_name_collision "$include_dir" "$cxx" "$scratch" \
        || fail "colisao de nome publico encontrada (ver mensagem acima)"
}

# --- selftest fixtures and controls ---------------------------------------

# A fixture "system" tree the tests below plant macros into. Kept
# separate from discover_system_include_dirs() entirely - selftest
# NEVER touches this machine's real /usr/include or compiler (L-40:
# "os tres controles da casa sao obrigatorios no autoteste... contra
# uma fixture descartavel, nunca contra a arvore rastreada real").
make_fixture_include_dir() {
    scratch="$1"
    label="$2"
    dir="$scratch/$label/include/pkg"
    mkdir -p "$dir"
    printf '%s\n' "$dir"
}

make_fixture_system_dir() {
    scratch="$1"
    label="$2"
    dir="$scratch/$label/system"
    mkdir -p "$dir"
    printf '%s\n' "$dir"
}

# Positive control: a clean public header (one type, one method), and
# a system header that defines something unrelated. Expected: passes,
# names the counts.
selftest_positive_control() {
    scratch="$1"
    include_dir="$(make_fixture_include_dir "$scratch" positive)"
    printf 'class widget {\n  public:\n    [[nodiscard]] int size() const noexcept;\n};\n' \
        > "$include_dir/widget.hpp"
    sys_dir="$(make_fixture_system_dir "$scratch" positive)"
    printf '#define UNRELATED_MACRO 1\n' > "$sys_dir/unrelated.h"

    awk_file="$scratch/positive-enumerate.awk"
    write_enumerate_names_awk "$awk_file"
    names="$(enumerate_our_names "$include_dir" "$awk_file")"
    if output="$(scan_defines_in_dirs "$sys_dir" "$names" 2>&1)" && [ -z "$output" ]; then
        echo "selftest: controle POSITIVO OK (header limpo, sem colisao)"
        return 0
    fi
    echo "selftest: controle POSITIVO FALHOU (esperava zero colisoes, achou algo)" >&2
    printf '%s\n' "$output" >&2
    return 1
}

# Negative control: a public header declaring `planted_collision_name`,
# and a system header defining exactly that, WITHOUT undef. Expected:
# the full check FAILS, and the message names both the file and the
# name (GODS_LAWS.md L-40: "nunca um sucesso silencioso").
selftest_negative_control() {
    scratch="$1"
    include_dir="$(make_fixture_include_dir "$scratch" negative)"
    printf 'class widget {\n  public:\n    [[nodiscard]] int planted_collision_name() const noexcept;\n};\n' \
        > "$include_dir/widget.hpp"
    sys_dir="$(make_fixture_system_dir "$scratch" negative)"
    hostile="$sys_dir/hostile.h"
    printf '#define planted_collision_name 1\n' > "$hostile"

    # Deliberately NOT routed through check_public_name_collision()'s
    # own discover_system_include_dirs(): that step asks THIS machine's
    # real compiler for the real search path, which selftest must never
    # touch (L-40's "nunca contra a arvore rastreada real", applied here
    # to the machine's own system headers, not just the repo tree).
    # Exercising scan_defines_in_dirs()/classify_matches() directly
    # against the fixture sys_dir proves the same logic without that
    # dependency.
    awk_file="$scratch/negative-enumerate.awk"
    write_enumerate_names_awk "$awk_file"
    names="$(enumerate_our_names "$include_dir" "$awk_file")"
    matches="$(scan_defines_in_dirs "$sys_dir" "$names")"
    classified="$(classify_matches "$matches" "${CXX_FOR_SELFTEST:-c++}")"
    real="$(real_collisions "$classified")"
    if [ -z "$real" ]; then
        echo "selftest: controle NEGATIVO FALHOU (scan_defines_in_dirs nao achou planted_collision_name em $hostile)" >&2
        return 1
    fi
    if ! printf '%s\n' "$real" | grep -qF "$hostile"; then
        echo "selftest: controle NEGATIVO FALHOU (achou colisao, mas nao citou $hostile)" >&2
        printf '%s\n' "$real" >&2
        return 1
    fi
    if ! printf '%s\n' "$real" | grep -qF "planted_collision_name"; then
        echo "selftest: controle NEGATIVO FALHOU (achou colisao, mas nao citou o nome)" >&2
        printf '%s\n' "$real" >&2
        return 1
    fi
    echo "selftest: controle NEGATIVO OK (planted_collision_name reprovado, citando arquivo e nome)"
    return 0
}

# Specific to this gate's own policy decision 2: same name, same
# fixture, but the hostile header ALSO #undef's it before EOF.
# Expected: classify_matches() puts it in NEUTRALIZED, not REAL - the
# achado's own precedent case ("o header o define e o desfaz no mesmo
# arquivo... inofensivo").
selftest_undef_neutralizes_control() {
    scratch="$1"
    include_dir="$(make_fixture_include_dir "$scratch" neutralize)"
    printf 'class widget {\n  public:\n    [[nodiscard]] int planted_collision_name() const noexcept;\n};\n' \
        > "$include_dir/widget.hpp"
    sys_dir="$(make_fixture_system_dir "$scratch" neutralize)"
    hostile="$sys_dir/hostile.h"
    printf '#define planted_collision_name 1\n/* uses it here */\n#undef planted_collision_name\n' > "$hostile"

    awk_file="$scratch/neutralize-enumerate.awk"
    write_enumerate_names_awk "$awk_file"
    names="$(enumerate_our_names "$include_dir" "$awk_file")"
    matches="$(scan_defines_in_dirs "$sys_dir" "$names")"
    classified="$(classify_matches "$matches" "${CXX_FOR_SELFTEST:-c++}")"
    real="$(real_collisions "$classified")"
    neutralized="$(neutralized_collisions "$classified")"

    if [ -n "$real" ]; then
        echo "selftest: controle de NEUTRALIZACAO FALHOU (define+undef no mesmo arquivo deveria ser NEUTRALIZADO, apareceu como REAL)" >&2
        printf '%s\n' "$real" >&2
        return 1
    fi
    if [ -z "$neutralized" ] || ! printf '%s\n' "$neutralized" | grep -qF "planted_collision_name"; then
        echo "selftest: controle de NEUTRALIZACAO FALHOU (nao apareceu na lista NEUTRALIZADA, ou nao citou o nome - GODS_LAWS.md L-40 exige que a contagem nunca fique escondida)" >&2
        return 1
    fi
    echo "selftest: controle de NEUTRALIZACAO OK (define+undef no mesmo arquivo nao reprova, mas aparece na contagem)"
    return 0
}

# Specific to the 26/08/2026 achado (measured live in Arch/CachyOS
# containers, attr's own /usr/include/attr/error_context.h): a #define
# textually nested inside "#ifdef GUARD" / "#if defined(GUARD)", where
# GUARD is never defined anywhere a normal include chain reaches, is
# NOT a real collision - the achado's own words, "uma macro que so
# existe se alguem definir ERROR_CONTEXT_MACROS nao colide numa
# inclusao normal". Fixture reproduces that exact shape: guard, define,
# no #undef. Expected: classify_matches() puts it in NEUTRALIZED (a
# SECOND reason, distinct from policy decision 2's same-file #undef),
# never REAL - and the reason string says which kind, so the report
# stays auditable (GODS_LAWS.md L-40: contagem nunca escondida).
selftest_guard_inactive_control() {
    scratch="$1"
    include_dir="$(make_fixture_include_dir "$scratch" guard_inactive)"
    printf 'class widget {\n  public:\n    [[nodiscard]] int planted_collision_name() const noexcept;\n};\n' \
        > "$include_dir/widget.hpp"
    sys_dir="$(make_fixture_system_dir "$scratch" guard_inactive)"
    hostile="$sys_dir/hostile.h"
    printf '#ifdef GUARD_NEVER_DEFINED\n#define planted_collision_name 1\n#endif\n' > "$hostile"

    awk_file="$scratch/guard-inactive-enumerate.awk"
    write_enumerate_names_awk "$awk_file"
    names="$(enumerate_our_names "$include_dir" "$awk_file")"
    matches="$(scan_defines_in_dirs "$sys_dir" "$names")"
    classified="$(classify_matches "$matches" "${CXX_FOR_SELFTEST:-c++}")"
    real="$(real_collisions "$classified")"
    neutralized="$(neutralized_collisions "$classified")"

    if [ -n "$real" ]; then
        echo "selftest: controle de GUARDA INATIVA FALHOU (macro so existe sob #ifdef de simbolo nunca definido deveria ser NEUTRALIZADA, apareceu como REAL)" >&2
        printf '%s\n' "$real" >&2
        return 1
    fi
    if [ -z "$neutralized" ] || ! printf '%s\n' "$neutralized" | grep -qF "planted_collision_name"; then
        echo "selftest: controle de GUARDA INATIVA FALHOU (nao apareceu na lista NEUTRALIZADA, ou nao citou o nome)" >&2
        return 1
    fi
    echo "selftest: controle de GUARDA INATIVA OK (macro sob #ifdef de simbolo nunca definido nao reprova, mas aparece na contagem)"
    return 0
}

# The middle case the same achado's ordem de servico demands explicitly
# ("o caso do meio: macro guardada por simbolo que ESTA definido no
# contexto deve reprovar"): same shape as above, but the guard symbol
# IS defined earlier in the same fixture header - so a real translation
# unit including it WOULD see the macro active. Expected: still REAL.
# This is the regression guard against loosening the check too far -
# GODS_LAWS.md L-40's own warning in this ordem de servico, "falso
# negativo em portao e exatamente a familia que esta sessao passou o
# dia inteiro fechando".
selftest_guard_active_control() {
    scratch="$1"
    include_dir="$(make_fixture_include_dir "$scratch" guard_active)"
    printf 'class widget {\n  public:\n    [[nodiscard]] int planted_collision_name() const noexcept;\n};\n' \
        > "$include_dir/widget.hpp"
    sys_dir="$(make_fixture_system_dir "$scratch" guard_active)"
    hostile="$sys_dir/hostile.h"
    printf '#define GUARD_ALWAYS_DEFINED_HERE 1\n#ifdef GUARD_ALWAYS_DEFINED_HERE\n#define planted_collision_name 1\n#endif\n' \
        > "$hostile"

    awk_file="$scratch/guard-active-enumerate.awk"
    write_enumerate_names_awk "$awk_file"
    names="$(enumerate_our_names "$include_dir" "$awk_file")"
    matches="$(scan_defines_in_dirs "$sys_dir" "$names")"
    classified="$(classify_matches "$matches" "${CXX_FOR_SELFTEST:-c++}")"
    real="$(real_collisions "$classified")"
    if [ -z "$real" ] || ! printf '%s\n' "$real" | grep -qF "planted_collision_name"; then
        echo "selftest: controle de GUARDA ATIVA FALHOU (macro cujo simbolo-guarda ESTA definido no proprio arquivo deveria reprovar como REAL, nao reprovou)" >&2
        printf '%s\n' "$real" >&2
        return 1
    fi
    echo "selftest: controle de GUARDA ATIVA OK (macro cujo simbolo-guarda esta definido no contexto continua REAL)"
    return 0
}

# Empty-scan floor, side 1: zero public headers under include_dir.
# Expected: fail, message names "0 nomes publicos".
selftest_empty_our_names_control() {
    scratch="$1"
    include_dir="$scratch/empty_our_names/include"
    mkdir -p "$include_dir"

    if output="$(check_public_name_collision "$include_dir" "${CXX_FOR_SELFTEST:-c++}" "$scratch/empty-our-names-work" 2>&1)"; then
        echo "selftest: controle de VARREDURA VAZIA (nomes) FALHOU (deveria recusar include_dir sem headers, mas passou)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    if ! printf '%s\n' "$output" | grep -qF "varredura vazia"; then
        echo "selftest: controle de VARREDURA VAZIA (nomes) FALHOU (recusou, mas nao disse 'varredura vazia')" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    echo "selftest: controle de VARREDURA VAZIA (nomes) OK (include_dir sem headers recusado)"
    return 0
}

# Empty-scan floor, side 2: the system-header side. Exercised directly
# against list_files_under_dirs()/require_nonempty_scan() with a
# directory that does not exist - the same shape a broken or
# not-yet-installed toolchain would produce from
# discover_system_include_dirs(), without needing to actually break
# this machine's compiler to prove it.
selftest_empty_system_dirs_control() {
    scratch="$1"
    nonexistent="$scratch/empty_system/does_not_exist"

    files="$(list_files_under_dirs "$nonexistent")"
    if output="$(require_nonempty_scan "$files" "0 arquivos de sistema varridos" 2>&1)"; then
        echo "selftest: controle de VARREDURA VAZIA (sistema) FALHOU (deveria recusar diretorio de sistema inexistente, mas passou)" >&2
        return 1
    fi
    if ! printf '%s\n' "$output" | grep -qF "varredura vazia"; then
        echo "selftest: controle de VARREDURA VAZIA (sistema) FALHOU (recusou, mas nao disse 'varredura vazia')" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    echo "selftest: controle de VARREDURA VAZIA (sistema) OK (diretorio de sistema inexistente recusado)"
    return 0
}

selftest_main() {
    # Optional positional compiler override (CI passes
    # CMAKE_CXX_COMPILER explicitly, since a bare "c++" symlink is not
    # guaranteed on every distro that only installs a versioned
    # g++-14-style package - see tests/CMakeLists.txt's own wiring of
    # this gate). CXX_FOR_SELFTEST env var stays as a second fallback
    # for local/manual runs; "c++" is the last resort.
    CXX_FOR_SELFTEST="${1:-${CXX_FOR_SELFTEST:-c++}}"
    export CXX_FOR_SELFTEST

    scratch="$(make_scratch_workdir)"
    trap 'rm -rf "$scratch"' EXIT

    command -v "$CXX_FOR_SELFTEST" >/dev/null 2>&1 \
        || fail "selftest precisa de um compilador C++ em PATH ($CXX_FOR_SELFTEST), nao encontrado"

    overall=0
    selftest_positive_control "$scratch" || overall=1
    selftest_negative_control "$scratch" || overall=1
    selftest_undef_neutralizes_control "$scratch" || overall=1
    selftest_guard_inactive_control "$scratch" || overall=1
    selftest_guard_active_control "$scratch" || overall=1
    selftest_empty_our_names_control "$scratch" || overall=1
    selftest_empty_system_dirs_control "$scratch" || overall=1

    if [ "$overall" -ne 0 ]; then
        echo "check_public_name_collision.sh --selftest: FALHOU (ver acima)" >&2
        exit 1
    fi
    echo "check_public_name_collision.sh --selftest: os sete controles OK"
}

main() {
    if [ "${1:-}" = "--selftest" ]; then
        shift
        selftest_main "$@"
    else
        real_main "$@"
    fi
}

main "$@"
