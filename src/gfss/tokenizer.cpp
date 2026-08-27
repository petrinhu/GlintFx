// SPDX-License-Identifier: AGPL-3.0-or-later
#include <glintfx/gfss/tokenizer.hpp>

#include <array>
#include <cassert>
#include <cstddef>
#include <string_view>

#include "code_point.hpp"
#include "cursor_ops.hpp"
#include "lexical_rules.hpp"
#include "token_progress_guard.hpp"

// tokenizer.cpp - GFSS-TOKEN (TODO.md, GODS_LAWS.md L-17/L-19/L-20/
// L-22/L-28): the CSS Syntax Module Level 3 "consume a token"
// dispatch (section 4.3.1) and every per-token-class production it
// calls (4.3.2 through 4.3.6) - see token.hpp/tokenizer.hpp's own
// header comments for the design decisions this file executes
// (closed vocabulary, raw-lexeme scope, why this never returns
// gltfx_rslt<T>).
//
// EVERY "this is a parse error" IN THE SPEC BECOMES ONE OF FOUR
// gltfx_gfss_diagnostic::expected TOKENS (design choice made HERE,
// GODS_LAWS.md L-27, marked INFERENCE - the spec names the CONDITION,
// never a machine-readable label for it): "closing_quote" (an
// unterminated string, 4.3.5's newline and EOF branches),
// "closing_parenthesis" (every dead end inside consume_url_token,
// 4.3.6), "escape_sequence" (a lone backslash not starting a valid
// escape, 4.3.1's own U+005C branch) - each is attached to the SAME
// token the spec already says to return at that point, never invented
// on a token the grammar itself does not produce. Unterminated
// comments (4.3.2's own EOF branch) do NOT get a diagnostic in this
// slice - the grammar produces no token of any kind for a comment, so
// there is nothing to attach one to without inventing a mechanism the
// service order for this fatia did not ask for (scope cut, declared,
// not silent).

namespace glintfx::style {

namespace {

using detail::advance_code_point;
using detail::at_end;
using detail::consume_escaped_code_point;
using detail::consume_ident_sequence;
using detail::consume_number;
using detail::consume_remnants_of_bad_url;
using detail::is_digit;
using detail::is_ident_continue;
using detail::is_ident_start;
using detail::is_non_printable;
using detail::is_valid_escape;
using detail::is_whitespace;
using detail::peek;
using detail::token_made_forward_progress;
using detail::would_start_ident_sequence;
using detail::would_start_number;

gltfx_gfss_diagnostic make_diagnostic(const gltfx_gfss_cursor &cursor,
                                      std::string_view expected) noexcept {
    return gltfx_gfss_diagnostic{
        .line = cursor.line, .column = cursor.column, .expected = expected};
}

// The nine punctuation classes that are exactly one code point wide
// and never branch on what follows - ONE table, not a switch growing
// a case per feature (GODS_LAWS.md L-17, the same technique
// token_kind.cpp's own name table already uses).
struct single_char_entry {
    char ch;
    gltfx_gfss_token_kind kind;
};

constexpr std::array<single_char_entry, 9> k_single_char_tokens{{
    {'(', gltfx_gfss_token_kind::open_paren},
    {')', gltfx_gfss_token_kind::close_paren},
    {',', gltfx_gfss_token_kind::comma},
    {':', gltfx_gfss_token_kind::colon},
    {';', gltfx_gfss_token_kind::semicolon},
    {'[', gltfx_gfss_token_kind::open_square},
    {']', gltfx_gfss_token_kind::close_square},
    {'{', gltfx_gfss_token_kind::open_curly},
    {'}', gltfx_gfss_token_kind::close_curly},
}};

bool try_single_char_token(int current, gltfx_gfss_token_kind &out_kind) noexcept {
    for (const single_char_entry &entry : k_single_char_tokens) {
        if (entry.ch == current) {
            out_kind = entry.kind;
            return true;
        }
    }
    return false;
}

void consume_whitespace(gltfx_gfss_cursor &cursor) noexcept {
    while (is_whitespace(peek(cursor))) {
        advance_code_point(cursor);
    }
}

// 4.3.2 "Consume comments". No diagnostic on the EOF branch - see this
// file's own header comment on why that scope cut is declared, not
// silent.
void skip_comments(gltfx_gfss_cursor &cursor) noexcept {
    while (peek(cursor) == '/' && peek(cursor, 1) == '*') {
        advance_code_point(cursor);
        advance_code_point(cursor);
        while (!at_end(cursor) && !(peek(cursor) == '*' && peek(cursor, 1) == '/')) {
            advance_code_point(cursor);
        }
        if (at_end(cursor)) {
            return;
        }
        advance_code_point(cursor); // '*'
        advance_code_point(cursor); // '/'
    }
}

// 4.3.1 U+0023 branch.
gltfx_gfss_token_kind consume_hash_or_delim(gltfx_gfss_cursor &cursor) noexcept {
    advance_code_point(cursor); // '#'
    if (is_ident_continue(peek(cursor)) || is_valid_escape(cursor)) {
        consume_ident_sequence(cursor);
        return gltfx_gfss_token_kind::hash;
    }
    return gltfx_gfss_token_kind::delim;
}

// 4.3.3 "Consume a numeric token".
gltfx_gfss_token_kind consume_numeric_token(gltfx_gfss_cursor &cursor) noexcept {
    consume_number(cursor);
    if (would_start_ident_sequence(cursor)) {
        consume_ident_sequence(cursor);
        return gltfx_gfss_token_kind::dimension;
    }
    if (peek(cursor) == '%') {
        advance_code_point(cursor);
        return gltfx_gfss_token_kind::percentage;
    }
    return gltfx_gfss_token_kind::number;
}

// 4.3.1 U+002B/U+002E branches: shared shape ("if a number starts
// here, it is a numeric token; otherwise it is a lone delimiter").
gltfx_gfss_token_kind consume_plus_or_dot_led_token(gltfx_gfss_cursor &cursor) noexcept {
    if (would_start_number(cursor)) {
        return consume_numeric_token(cursor);
    }
    advance_code_point(cursor);
    return gltfx_gfss_token_kind::delim;
}

bool ident_text_is_url_keyword(std::string_view text) noexcept {
    if (text.size() != 3) {
        return false;
    }
    return (text[0] == 'u' || text[0] == 'U') && (text[1] == 'r' || text[1] == 'R') &&
           (text[2] == 'l' || text[2] == 'L');
}

bool next_two_are_whitespace(const gltfx_gfss_cursor &cursor) noexcept {
    return is_whitespace(peek(cursor)) && is_whitespace(peek(cursor, 1));
}

// 4.3.6's own U+005C branch: true (loop continues) on a valid escape
// consumed; false (caller finishes as a <bad-url-token>) otherwise.
bool consume_url_escape_or_flag_bad(gltfx_gfss_cursor &cursor,
                                    gltfx_gfss_diagnostic &diagnostic) noexcept {
    if (is_valid_escape(cursor)) {
        advance_code_point(cursor); // the backslash
        consume_escaped_code_point(cursor);
        return true;
    }
    diagnostic = make_diagnostic(cursor, "closing_parenthesis");
    consume_remnants_of_bad_url(cursor);
    return false;
}

// 4.3.6's own "whitespace" branch, factored out so consume_url_token
// itself never nests an `if` inside an `if` (GODS_LAWS.md L-17).
gltfx_gfss_token_kind consume_url_trailing_whitespace(gltfx_gfss_cursor &cursor,
                                                      gltfx_gfss_diagnostic &diagnostic) noexcept {
    consume_whitespace(cursor);
    const int current = peek(cursor);
    if (current == ')') {
        advance_code_point(cursor);
        return gltfx_gfss_token_kind::url;
    }
    if (current == -1) {
        diagnostic = make_diagnostic(cursor, "closing_parenthesis");
        return gltfx_gfss_token_kind::url;
    }
    diagnostic = make_diagnostic(cursor, "closing_parenthesis");
    consume_remnants_of_bad_url(cursor);
    return gltfx_gfss_token_kind::bad_url;
}

// 4.3.6 "Consume a url token" - assumes the leading "url(" (and any
// immediately-following whitespace consume_url_or_function_token below
// already stepped past) has already been consumed.
gltfx_gfss_token_kind consume_url_token(gltfx_gfss_cursor &cursor,
                                        gltfx_gfss_diagnostic &diagnostic) noexcept {
    consume_whitespace(cursor);
    while (true) {
        const int current = peek(cursor);
        if (current == ')') {
            advance_code_point(cursor);
            return gltfx_gfss_token_kind::url;
        }
        if (current == -1) {
            diagnostic = make_diagnostic(cursor, "closing_parenthesis");
            return gltfx_gfss_token_kind::url;
        }
        if (is_whitespace(current)) {
            return consume_url_trailing_whitespace(cursor, diagnostic);
        }
        if (current == '"' || current == '\'' || current == '(' || is_non_printable(current)) {
            diagnostic = make_diagnostic(cursor, "closing_parenthesis");
            consume_remnants_of_bad_url(cursor);
            return gltfx_gfss_token_kind::bad_url;
        }
        if (current == '\\') {
            if (consume_url_escape_or_flag_bad(cursor, diagnostic)) {
                continue;
            }
            return gltfx_gfss_token_kind::bad_url;
        }
        advance_code_point(cursor);
    }
}

// 4.3.4's own "url(" sub-algorithm: decides function-token vs.
// url-token, per the exact "while next two are whitespace, consume
// one" rule the spec's own text spells out (see this file's own
// citation in the code review notes / service-order report - the
// single-consume-per-iteration shape is verbatim spec text, not a
// simplification).
gltfx_gfss_token_kind consume_url_or_function_token(gltfx_gfss_cursor &cursor,
                                                    gltfx_gfss_diagnostic &diagnostic) noexcept {
    advance_code_point(cursor); // '('
    while (next_two_are_whitespace(cursor)) {
        advance_code_point(cursor);
    }
    const int first = peek(cursor);
    const int second = peek(cursor, 1);
    const bool starts_with_quote = first == '"' || first == '\'';
    const bool whitespace_then_quote = is_whitespace(first) && (second == '"' || second == '\'');
    if (starts_with_quote || whitespace_then_quote) {
        return gltfx_gfss_token_kind::function;
    }
    return consume_url_token(cursor, diagnostic);
}

// 4.3.4 "Consume an ident-like token".
gltfx_gfss_token_kind consume_ident_like_token(gltfx_gfss_cursor &cursor,
                                               gltfx_gfss_diagnostic &diagnostic) noexcept {
    const std::size_t ident_start = cursor.byte_offset;
    consume_ident_sequence(cursor);
    const std::string_view ident_text =
        cursor.source.substr(ident_start, cursor.byte_offset - ident_start);

    if (ident_text_is_url_keyword(ident_text) && peek(cursor) == '(') {
        return consume_url_or_function_token(cursor, diagnostic);
    }
    if (peek(cursor) == '(') {
        advance_code_point(cursor);
        return gltfx_gfss_token_kind::function;
    }
    return gltfx_gfss_token_kind::ident;
}

// 4.3.1 U+002D branch.
gltfx_gfss_token_kind consume_hyphen_led_token(gltfx_gfss_cursor &cursor,
                                               gltfx_gfss_diagnostic &diagnostic) noexcept {
    if (would_start_number(cursor)) {
        return consume_numeric_token(cursor);
    }
    if (peek(cursor, 1) == '-' && peek(cursor, 2) == '>') {
        advance_code_point(cursor);
        advance_code_point(cursor);
        advance_code_point(cursor);
        return gltfx_gfss_token_kind::cdc;
    }
    if (would_start_ident_sequence(cursor)) {
        return consume_ident_like_token(cursor, diagnostic);
    }
    advance_code_point(cursor);
    return gltfx_gfss_token_kind::delim;
}

// 4.3.1 U+003C branch.
gltfx_gfss_token_kind consume_less_than_led_token(gltfx_gfss_cursor &cursor) noexcept {
    if (peek(cursor, 1) == '!' && peek(cursor, 2) == '-' && peek(cursor, 3) == '-') {
        advance_code_point(cursor);
        advance_code_point(cursor);
        advance_code_point(cursor);
        advance_code_point(cursor);
        return gltfx_gfss_token_kind::cdo;
    }
    advance_code_point(cursor);
    return gltfx_gfss_token_kind::delim;
}

// 4.3.1 U+0040 branch.
gltfx_gfss_token_kind consume_at_keyword_or_delim(gltfx_gfss_cursor &cursor) noexcept {
    advance_code_point(cursor); // '@'
    if (would_start_ident_sequence(cursor)) {
        consume_ident_sequence(cursor);
        return gltfx_gfss_token_kind::at_keyword;
    }
    return gltfx_gfss_token_kind::delim;
}

// 4.3.1 U+005C branch.
gltfx_gfss_token_kind consume_backslash_led_token(gltfx_gfss_cursor &cursor,
                                                  gltfx_gfss_diagnostic &diagnostic) noexcept {
    if (is_valid_escape(cursor)) {
        return consume_ident_like_token(cursor, diagnostic);
    }
    diagnostic = make_diagnostic(cursor, "escape_sequence");
    advance_code_point(cursor); // the lone backslash
    return gltfx_gfss_token_kind::delim;
}

// 4.3.5's own U+005C branch.
void consume_string_escape(gltfx_gfss_cursor &cursor) noexcept {
    advance_code_point(cursor); // the backslash
    if (at_end(cursor)) {
        return;
    }
    if (detail::is_newline(peek(cursor))) {
        advance_code_point(cursor);
        return;
    }
    consume_escaped_code_point(cursor);
}

// 4.3.5 "Consume a string token" - the ending code point is whichever
// quote opened the string (this grammar never calls it with a
// different one, unlike the general algorithm the spec describes).
gltfx_gfss_token_kind consume_string_token(gltfx_gfss_cursor &cursor,
                                           gltfx_gfss_diagnostic &diagnostic) noexcept {
    const int ending = peek(cursor);
    advance_code_point(cursor); // opening quote
    while (true) {
        const int current = peek(cursor);
        if (current == ending) {
            advance_code_point(cursor);
            return gltfx_gfss_token_kind::string;
        }
        if (current == -1) {
            diagnostic = make_diagnostic(cursor, "closing_quote");
            return gltfx_gfss_token_kind::string;
        }
        if (detail::is_newline(current)) {
            diagnostic = make_diagnostic(cursor, "closing_quote");
            return gltfx_gfss_token_kind::bad_string; // reconsume: do NOT advance past the newline
        }
        if (current == '\\') {
            consume_string_escape(cursor);
            continue;
        }
        advance_code_point(cursor);
    }
}

// 4.3.1 "Consume a token" - the top-level dispatch every branch above
// serves.
gltfx_gfss_token_kind dispatch_token(gltfx_gfss_cursor &cursor,
                                     gltfx_gfss_diagnostic &diagnostic) noexcept {
    const int current = peek(cursor);

    gltfx_gfss_token_kind single_char_kind{};
    if (try_single_char_token(current, single_char_kind)) {
        advance_code_point(cursor);
        return single_char_kind;
    }
    if (current == -1) {
        return gltfx_gfss_token_kind::eof;
    }
    if (is_whitespace(current)) {
        consume_whitespace(cursor);
        return gltfx_gfss_token_kind::whitespace;
    }
    if (current == '"' || current == '\'') {
        return consume_string_token(cursor, diagnostic);
    }
    if (current == '#') {
        return consume_hash_or_delim(cursor);
    }
    if (current == '+' || current == '.') {
        return consume_plus_or_dot_led_token(cursor);
    }
    if (current == '-') {
        return consume_hyphen_led_token(cursor, diagnostic);
    }
    if (current == '<') {
        return consume_less_than_led_token(cursor);
    }
    if (current == '@') {
        return consume_at_keyword_or_delim(cursor);
    }
    if (current == '\\') {
        return consume_backslash_led_token(cursor, diagnostic);
    }
    if (is_digit(current)) {
        return consume_numeric_token(cursor);
    }
    if (is_ident_start(current)) {
        return consume_ident_like_token(cursor, diagnostic);
    }
    advance_code_point(cursor);
    return gltfx_gfss_token_kind::delim;
}

} // namespace

bool gltfx_gfss_next_token(gltfx_gfss_cursor &cursor, gltfx_gfss_token &out_token) noexcept {
    skip_comments(cursor);

    const std::uint32_t start_line = cursor.line;
    const std::uint32_t start_column = cursor.column;
    const std::size_t start_offset = cursor.byte_offset;

    gltfx_gfss_diagnostic diagnostic{};
    const gltfx_gfss_token_kind kind = dispatch_token(cursor, diagnostic);

    // ZERO-PROGRESS GUARD, found by this fatia's own mutation-testing
    // pass (GODS_LAWS.md L-20): every dispatch_token() branch above
    // guarantees at least one code point of progress through a
    // DIFFERENT mechanism per branch (the opening quote/bracket/at-
    // sign/backslash is consumed unconditionally before any further
    // check, or would_start_number()/would_start_ident_sequence()
    // having said yes forces consume_number()/consume_ident_sequence()
    // to consume something) - there is no SINGLE centralized check
    // INSIDE dispatch_token() that this holds, so a future one-line
    // regression in any ONE of them (this was measured LIVE:
    // neutralizing consume_optional_sign() alone makes a leading
    // "-3.5e-2" produce a ZERO-LENGTH <number-token>) turns into
    // gltfx_gfss_tokenize()'s while loop spinning forever - a HANG, not
    // a crash, on the CONSUMER's own process. token_progress_guard.hpp's
    // own header comment has the full rationale for why the check
    // itself is a separate, testable predicate.
    //
    // TWO REACTIONS, CORRECTED 26/08/2026 (GODS_LAWS.md L-40 achado 2:
    // "a guarda de progresso so protege em depuracao"). The FIRST
    // version of this guard was assert() alone, on the SAME convention
    // gltfx_rslt<T>'s precondition guard uses (docs/api-conventions.md
    // R1) - but that convention fits a CALLER mistake (the consumer
    // broke a precondition; UB in Release is the accepted, documented
    // cost, same as std::optional::value()). This guard protects
    // against something else: an INTERNAL glintfx defect that hangs a
    // CORRECTLY-BEHAVING consumer's process, and assert() alone gave
    // that protection ONLY in a Debug build (NDEBUG undefined) - a
    // build this project's OWN tooling never uses: tools/preci.sh's
    // stage_sanitizer (the ASan/UBSan portao) and every job of
    // .github/workflows/ci.yml configure with
    // -DCMAKE_BUILD_TYPE=Release, so this assert has NEVER fired in any
    // of glintfx's own gates, not even the sanitizer one - measured
    // live while fixing this achado, not assumed. The decision (see
    // this fatia's own service order/commit message for the three
    // options weighed): TURN THE GUARD INTO ONE THAT SURVIVES
    // OPTIMIZATION, kept alongside the assert() rather than replacing
    // it - the assert() still gives a developer doing a genuine manual
    // -DCMAKE_BUILD_TYPE=Debug build the exact, named diagnostic; the
    // `if` below is what actually holds the promise LEI ZERO's unknown
    // external consumer gets in the build glintfx ships (Release):
    // malformed input, or a future regression here, degrades to a
    // wrong-but-terminating token stream, never a hung process. The
    // cost is one integer comparison already computed for the assert's
    // own condition, unconditionally, on the hot per-token path - not a
    // product-policy call (no error code invented, no public signature
    // touched, no behavior change for any currently-correct input).
    const bool made_progress = token_made_forward_progress(kind, start_offset, cursor.byte_offset);
    assert(made_progress &&
           "gltfx_gfss_next_token(): a token production consumed zero code points - internal "
           "contract violation, would spin the caller's while loop forever");
    if (!made_progress) {
        advance_code_point(cursor); // RELEASE SAFETY NET - see this block's own comment above.
    }

    out_token.kind = kind;
    out_token.lexeme = cursor.source.substr(start_offset, cursor.byte_offset - start_offset);
    out_token.line = start_line;
    out_token.column = start_column;
    out_token.diagnostic = diagnostic;
    return kind != gltfx_gfss_token_kind::eof;
}

} // namespace glintfx::style
