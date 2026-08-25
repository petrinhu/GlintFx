#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
# check_nodiscard_rslt.sh - CE-4 of CORE-ERROR (TODO.md): proves the
# [[nodiscard]] on glintfx::gltfx_rslt<T> is not decoration. The class
# template being tagged is a claim - "a caller cannot silently drop a
# fallible call's result" - and a claim about a COMPILER DIAGNOSTIC is
# only proven by making the compiler actually emit one, the same way
# every other compile-time contract in this suite (check_layers.sh,
# check_no_x11.sh) is proven by running the real tool against real
# input, not by reading the source and trusting it.
#
# METHOD, mirroring the DISCARDED-VALUE half of GODS_LAWS.md L-20's own
# vermelho/verde discipline, at the level of a compiler diagnostic
# instead of a runtime assertion: two fixtures, generated here (not
# vendored under tests/), compiled against the REAL project headers
# with THIS PROJECT'S OWN warning flags (-Wall -Wextra -Werror, the
# same set GlintfxCompileOptions.cmake applies to every target):
#   - discard.cpp calls a gltfx_rslt<int>-returning AND a
#     gltfx_rslt<void>-returning example fallible function and drops
#     BOTH results outright (no (void) cast, no assignment) - MUST
#     fail to compile, and the failure text MUST mention the
#     diagnostic by name, not just "compile failed for some reason".
#   - consume.cpp calls the same two functions and USES both results -
#     MUST compile cleanly.
# Compilation only (-c, no link): gltfx_err's trivial constructor is
# inline (err.hpp), so building a code-only gltfx_err needs no symbol
# from libglintfx at all; the copy constructor and destructor ARE
# out-of-line (GLINTFX_API, err.cpp), but `-c` stops before the link
# step ever resolves them, so no library artifact is needed here.
#
# Usage: check_nodiscard_rslt.sh <include-dir> <generated-include-dir> <cxx-compiler>
#
# Each function below does one thing (GODS_LAWS.md L-17).

set -eu

fail() {
    echo "check_nodiscard_rslt.sh: $1" >&2
    exit 1
}

require_args() {
    [ "$#" -eq 3 ] || fail "usage: check_nodiscard_rslt.sh <include-dir> <generated-include-dir> <cxx-compiler>"
    [ -d "$1" ] || fail "include dir not found: $1"
    [ -d "$2" ] || fail "generated include dir not found: $2"
}

make_scratch_workdir() {
    mktemp -d "${TMPDIR:-/tmp}/glintfx-nodiscard-XXXXXX"
}

# The two example fallible functions, shared verbatim by both fixtures
# below - only main() differs (discards vs. consumes the results). Not
# part of glintfx's public API: illustrative only, for this gate.
example_functions_source() {
    cat <<'CXX'
#include <glintfx/core/err.hpp>

#include <string_view>

glintfx::gltfx_rslt<int> parse_positive_int(std::string_view text) noexcept {
    if (text.empty()) {
        return glintfx::gltfx_rslt<int>::err(glintfx::gltfx_err(glintfx::gltfx_err_code::invalid_argument));
    }
    int value = 0;
    for (const char c : text) {
        if (c < '0' || c > '9') {
            return glintfx::gltfx_rslt<int>::err(glintfx::gltfx_err(glintfx::gltfx_err_code::parse_failure));
        }
        value = (value * 10) + (c - '0');
    }
    return glintfx::gltfx_rslt<int>::ok(value);
}

glintfx::gltfx_rslt<void> validate_non_empty(std::string_view text) noexcept {
    if (text.empty()) {
        return glintfx::gltfx_rslt<void>::err(glintfx::gltfx_err(glintfx::gltfx_err_code::invalid_argument));
    }
    return glintfx::gltfx_rslt<void>::ok();
}
CXX
}

write_discard_fixture() {
    out="$1"
    {
        example_functions_source
        cat <<'CXX'

int main() {
    parse_positive_int("42");
    validate_non_empty("not empty");
    return 0;
}
CXX
    } > "$out"
}

write_consume_fixture() {
    out="$1"
    {
        example_functions_source
        cat <<'CXX'

int main() {
    const auto parsed = parse_positive_int("42");
    const auto validated = validate_non_empty("not empty");
    return (parsed.has_value() ? 0 : 1) + (validated.has_value() ? 0 : 1);
}
CXX
    } > "$out"
}

compile_fixture() {
    src="$1"
    includedir="$2"
    generated_includedir="$3"
    cxx="$4"
    "$cxx" -std=c++23 -Wall -Wextra -Werror \
        -I "$includedir" -I "$generated_includedir" \
        -c "$src" -o "$src.o" 2>"$src.stderr"
}

assert_discard_fixture_fails_naming_nodiscard() {
    src="$1"
    includedir="$2"
    generated_includedir="$3"
    cxx="$4"

    if compile_fixture "$src" "$includedir" "$generated_includedir" "$cxx"; then
        fail "discard fixture compiled CLEANLY - [[nodiscard]] on gltfx_rslt<T> is not enforced: $(cat "$src.stderr")"
    fi

    if ! grep -qi "nodiscard\|discard\|unused" "$src.stderr"; then
        fail "discard fixture failed to compile, but not for a nodiscard reason - stderr: $(cat "$src.stderr")"
    fi

    echo "check_nodiscard_rslt.sh: discard fixture correctly REFUSED to compile ([[nodiscard]] fired)"
}

assert_consume_fixture_compiles_cleanly() {
    src="$1"
    includedir="$2"
    generated_includedir="$3"
    cxx="$4"

    if ! compile_fixture "$src" "$includedir" "$generated_includedir" "$cxx"; then
        fail "consume fixture (result actually used) FAILED to compile: $(cat "$src.stderr")"
    fi

    echo "check_nodiscard_rslt.sh: consume fixture (result actually used) compiled cleanly"
}

main() {
    require_args "$@"
    includedir="$1"
    generated_includedir="$2"
    cxx="$3"

    scratch="$(make_scratch_workdir)"
    trap 'rm -rf "$scratch"' EXIT

    discard_src="$scratch/discard.cpp"
    consume_src="$scratch/consume.cpp"
    write_discard_fixture "$discard_src"
    write_consume_fixture "$consume_src"

    assert_discard_fixture_fails_naming_nodiscard "$discard_src" "$includedir" "$generated_includedir" "$cxx"
    assert_consume_fixture_compiles_cleanly "$consume_src" "$includedir" "$generated_includedir" "$cxx"

    echo "ok: gltfx_rslt<T>'s [[nodiscard]] is a real compiler diagnostic, proven against both gltfx_rslt<int> and gltfx_rslt<void>, not decoration."
}

main "$@"
