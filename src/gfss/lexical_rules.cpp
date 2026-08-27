// SPDX-License-Identifier: AGPL-3.0-or-later
#include "lexical_rules.hpp"

// lexical_rules.cpp - GFSS-TOKEN (GODS_LAWS.md L-17): the four
// "consume" algorithms lexical_rules.hpp declares - see that header's
// own comment for why these are out-of-line while the three "check"
// predicates stay inline there.

namespace glintfx::style::detail {

namespace {

// Shared by consume_number below - not declared in the header because
// nothing outside this file needs a bare digit run on its own.
void consume_digits(gltfx_gfss_cursor &cursor) noexcept {
    while (is_digit(peek(cursor))) {
        advance_code_point(cursor);
    }
}

void consume_optional_sign(gltfx_gfss_cursor &cursor) noexcept {
    if (peek(cursor) == '+' || peek(cursor) == '-') {
        advance_code_point(cursor);
    }
}

// 4.3.12 step 4: "." followed by a digit.
void consume_optional_fraction(gltfx_gfss_cursor &cursor) noexcept {
    if (peek(cursor) != '.' || !is_digit(peek(cursor, 1))) {
        return;
    }
    advance_code_point(cursor); // '.'
    consume_digits(cursor);
}

// Look-only half of 4.3.12 step 5: does an exponent genuinely start
// here, and if so, how wide is its optional sign? Kept separate from
// consume_optional_exponent below so THAT function never nests an
// `if` inside an `if` (GODS_LAWS.md L-17/CONTRACT.md SS6.2).
bool exponent_starts_here(const gltfx_gfss_cursor &cursor, std::size_t &out_sign_width) noexcept {
    const int marker = peek(cursor);
    if (marker != 'e' && marker != 'E') {
        return false;
    }
    out_sign_width = (peek(cursor, 1) == '+' || peek(cursor, 1) == '-') ? 1 : 0;
    return is_digit(peek(cursor, 1 + out_sign_width));
}

// 4.3.12 step 5: ("e"|"E") ("+"|"-")? digit.
void consume_optional_exponent(gltfx_gfss_cursor &cursor) noexcept {
    std::size_t sign_width = 0;
    if (!exponent_starts_here(cursor, sign_width)) {
        return;
    }
    advance_code_point(cursor); // 'e'/'E'
    if (sign_width == 1) {
        advance_code_point(cursor);
    }
    consume_digits(cursor);
}

} // namespace

void consume_escaped_code_point(gltfx_gfss_cursor &cursor) noexcept {
    if (at_end(cursor)) {
        return; // 4.3.7 EOF branch: nothing left to advance past.
    }
    const int first = peek(cursor);
    advance_code_point(cursor);
    if (!is_hex_digit(first)) {
        return; // 4.3.7 "anything else": the one code point already consumed above IS the escape.
    }

    // 4.3.7 hex digit branch: up to 5 MORE hex digits (6 total,
    // counting `first`), then one optional trailing whitespace code
    // point.
    int hex_digits_consumed = 1;
    while (hex_digits_consumed < 6 && is_hex_digit(peek(cursor))) {
        advance_code_point(cursor);
        ++hex_digits_consumed;
    }
    if (is_whitespace(peek(cursor))) {
        advance_code_point(cursor);
    }
}

void consume_ident_sequence(gltfx_gfss_cursor &cursor) noexcept {
    while (true) {
        if (is_ident_continue(peek(cursor))) {
            advance_code_point(cursor);
            continue;
        }
        if (is_valid_escape(cursor)) {
            advance_code_point(cursor); // the backslash itself
            consume_escaped_code_point(cursor);
            continue;
        }
        return; // 4.3.11 "anything else": reconsume, stop here.
    }
}

void consume_number(gltfx_gfss_cursor &cursor) noexcept {
    consume_optional_sign(cursor);
    consume_digits(cursor);
    consume_optional_fraction(cursor);
    consume_optional_exponent(cursor);
}

void consume_remnants_of_bad_url(gltfx_gfss_cursor &cursor) noexcept {
    while (!at_end(cursor) && peek(cursor) != ')') {
        if (is_valid_escape(cursor)) {
            advance_code_point(cursor); // the backslash itself
            consume_escaped_code_point(cursor);
            continue;
        }
        advance_code_point(cursor);
    }
    if (peek(cursor) == ')') {
        advance_code_point(cursor); // consume the closing paren too, 4.3.14's own "return" step
    }
}

} // namespace glintfx::style::detail
