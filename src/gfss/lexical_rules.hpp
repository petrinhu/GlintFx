// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>

#include <glintfx/gfss/tokenizer.hpp>

#include "code_point.hpp"
#include "cursor_ops.hpp"

// lexical_rules.hpp - GFSS-TOKEN, private helper (GODS_LAWS.md L-17):
// the CSS Syntax Module Level 3 "check" and "consume" algorithms
// shared by more than one top-level token production
// (tokenizer.cpp's own consume_*_token functions) - distinct subject
// from cursor_ops.hpp (single code point stepping) and code_point.hpp
// (character classification): this file answers "what does the
// GRAMMAR say about escapes, ident sequences and numbers?", the
// others answer "how do I read one code point?" and "what class is
// it?". Not installed, not part of glintfx's public surface.
//
// LOOKAHEAD-ONLY FUNCTIONS ARE inline HERE; CONSUMING ONES ARE
// DECLARED HERE, DEFINED IN lexical_rules.cpp: the three "check"
// algorithms below (sections 4.3.8/4.3.9/4.3.10) never mutate the
// cursor ("Does not consume additional code points" - the spec's own
// words, repeated verbatim in each one), so each is small enough to
// stay a one-line inline predicate; the four "consume" algorithms
// (4.3.7/4.3.11/4.3.12/4.3.14) mutate the cursor and are each a small
// state machine worth its own out-of-line definition.

namespace glintfx::style::detail {

// 4.3.8 "Check if two code points are a valid escape": true iff the
// code point at `lookahead` is U+005C REVERSE SOLIDUS and the one
// right after it is NOT a newline. EOF right after the backslash
// counts as "not newline" here, exactly as the spec's own definition
// says - consume_escaped_code_point (lexical_rules.cpp) is what turns
// that EOF into its own parse error when actually consumed.
[[nodiscard]] constexpr bool is_valid_escape(const gltfx_gfss_cursor &cursor,
                                             std::size_t lookahead = 0) noexcept {
    return peek(cursor, lookahead) == '\\' && !is_newline(peek(cursor, lookahead + 1));
}

// 4.3.9 "Check if three code points would start an ident sequence".
[[nodiscard]] constexpr bool would_start_ident_sequence(const gltfx_gfss_cursor &cursor) noexcept {
    const int first = peek(cursor);
    if (first == '-') {
        return is_ident_start(peek(cursor, 1)) || peek(cursor, 1) == '-' ||
               is_valid_escape(cursor, 1);
    }
    if (first == '\\') {
        return is_valid_escape(cursor);
    }
    return is_ident_start(first);
}

// 4.3.10 "Check if three code points would start a number".
[[nodiscard]] constexpr bool would_start_number(const gltfx_gfss_cursor &cursor) noexcept {
    const int first = peek(cursor);
    if (first == '+' || first == '-') {
        return is_digit(peek(cursor, 1)) || (peek(cursor, 1) == '.' && is_digit(peek(cursor, 2)));
    }
    if (first == '.') {
        return is_digit(peek(cursor, 1));
    }
    return is_digit(first);
}

// 4.3.7 "Consume an escaped code point". Assumes the leading U+005C
// has ALREADY been consumed by the caller and that is_valid_escape()
// was already true at that position - advances `cursor` past the
// escape body (1-6 hex digits plus one optional trailing whitespace,
// or exactly one other code point, or nothing at EOF). This module
// never needs the DECODED value (token.hpp's own "LEXEME IS THE RAW
// SOURCE SPAN" scope decision) - only correct byte advancement so the
// caller's lexeme boundary lands in the right place.
void consume_escaped_code_point(gltfx_gfss_cursor &cursor) noexcept;

// 4.3.11 "Consume an ident sequence": advances past every ident code
// point and every valid escape, stopping (without consuming) at the
// first code point that is neither.
void consume_ident_sequence(gltfx_gfss_cursor &cursor) noexcept;

// 4.3.12 "Consume a number": advances past an optional sign, digits,
// an optional decimal point with more digits, and an optional
// exponent - the NUMBER'S OWN SPAN, not the unit/percent sign that
// might follow it (that is consume_numeric_token's own job in
// tokenizer.cpp, section 4.3.3). This module never converts the span
// to a numeric value (4.3.13, "Convert a string to a number", is
// deliberately NOT implemented here - see token.hpp's own "LEXEME IS
// THE RAW SOURCE SPAN" scope decision).
void consume_number(gltfx_gfss_cursor &cursor) noexcept;

// 4.3.14 "Consume the remnants of a bad url": advances until the
// matching U+0029 RIGHT PARENTHESIS or EOF, treating an embedded valid
// escape as one unit (so an escaped ")" inside the remnants does not
// end the scan early).
void consume_remnants_of_bad_url(gltfx_gfss_cursor &cursor) noexcept;

} // namespace glintfx::style::detail
