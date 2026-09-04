// SPDX-License-Identifier: AGPL-3.0-or-later
#include <glintfx/gfss/tokenizer.hpp>

#include <array>
#include <cassert>
#include <cstddef>
#include <string_view>

#include "code_point.hpp"
#include "cursor_ops.hpp"
#include "diagnostic_vocabulary.hpp"
#include "lexical_rules.hpp"
#include "token_progress_guard.hpp"
#include "token_progress_recovery.hpp"

// tokenizer.cpp - GFSS-TOKEN (TODO.md, GODS_LAWS.md L-17/L-19/L-20/
// L-22/L-28): the CSS Syntax Module Level 3 "consume a token"
// dispatch (section 4.3.1) and every per-token-class production it
// calls (4.3.2 through 4.3.6) - see token.hpp/tokenizer.hpp's own
// header comments for the design decisions this file executes
// (closed vocabulary, raw-lexeme scope, why this never returns
// gltfx_rslt<T>).
//
// EVERY "this is a parse error" IN THE SPEC BECOMES ONE OF FOUR
// SPEC-DRIVEN gltfx_gfss_diagnostic::expected TOKENS (design choice
// made HERE, GODS_LAWS.md L-27, marked INFERENCE - the spec names the
// CONDITION, never a machine-readable label for it), each spelled from
// diagnostic_vocabulary.hpp's own single list, never a literal of its
// own: "closing_quote" (an unterminated string, 4.3.5's newline and
// EOF branches), "closing_parenthesis" (every dead end inside
// consume_url_token, 4.3.6), "escape_sequence" (a lone backslash not
// starting a valid escape, 4.3.1's own U+005C branch) - each is
// attached to the SAME token the spec already says to return at that
// point, never invented on a token the grammar itself does not
// produce.
//
// UNTERMINATED COMMENTS ARE THE FOURTH, ADDED BY GFSS-COMMENT-DIAG
// (TODO.md, GODS_LAWS.md L-28's own "linha aceita em silencio e o
// defeito que o lider mandou eliminar") - UNLIKE THE OTHER THREE, THE
// GRAMMAR PRODUCES NO TOKEN OF ANY KIND FOR A COMMENT AT ALL (4.3.2's
// own EOF branch, section 2.2's own "the parser attempts to recover
// gracefully" philosophy this file's own header comment already cites
// for strings/urls does not extend to comments the same way - a
// comment simply vanishes, well-formed or not). This slice used to cut
// that case on purpose, declared not silent (see the git history of
// this comment) - the leader's order of service reopened it: a
// comment that runs into EOF before its own "*/" now ends the WHOLE
// token stream with a single diagnosed <EOF-token>, the SAME shape
// token_progress_recovery.hpp's own recover_from_forward_progress_
// violation() already uses for an internal defect - here for a
// genuine CONSUMER input error instead, "closing_comment" naming what
// this identifier's own condition is, never blaming glintfx for the
// consumer's own unclosed comment. See skip_comments() below for the
// position rule (the comment's own opening "/*", never EOF).
//
// A FOURTH TOKEN, "internal_tokenizer_defect", NAMES SOMETHING THE
// SPEC HAS NO CONCEPT OF: THIS LIBRARY FAILING ITS OWN INTERNAL
// CONTRACT, NEVER A MALFORMED CONSUMER SOURCE (GODS_LAWS.md L-40, fix
// for the CRITICO that reproved commit 95c0f20 - see
// token_progress_recovery.hpp's own header comment, and this file's
// own gltfx_gfss_next_token() comment further down, for the full
// rationale and why it ends the token stream instead of continuing).

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
using detail::k_expected_closing_comment;
using detail::k_expected_closing_parenthesis;
using detail::k_expected_closing_quote;
using detail::k_expected_escape_sequence;
using detail::peek;
using detail::recover_from_forward_progress_violation;
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

// 4.3.2 "Consume comments". Returns true when every comment in the run
// (zero, one, or several in a row) closed cleanly; false, with
// `diagnostic` filled, the moment ONE of them runs into EOF before its
// own "*/" - GFSS-COMMENT-DIAG (TODO.md, this file's own header comment
// above). `diagnostic` is positioned at THAT comment's OWN opening
// "/*" (comment_line/comment_column below, captured BEFORE advancing
// past it), never at EOF - a consumer needs to know WHERE it forgot to
// close the comment, not where the file happened to end. Captured
// fresh on every loop iteration (not just the first), so several
// well-formed comments followed by an unterminated one point at THAT
// LAST one, never the first.
bool skip_comments(gltfx_gfss_cursor &cursor, gltfx_gfss_diagnostic &diagnostic) noexcept {
    while (peek(cursor) == '/' && peek(cursor, 1) == '*') {
        const std::uint32_t comment_line = cursor.line;
        const std::uint32_t comment_column = cursor.column;
        advance_code_point(cursor);
        advance_code_point(cursor);
        while (!at_end(cursor) && !(peek(cursor) == '*' && peek(cursor, 1) == '/')) {
            advance_code_point(cursor);
        }
        if (at_end(cursor)) {
            diagnostic = gltfx_gfss_diagnostic{.line = comment_line,
                                               .column = comment_column,
                                               .expected = k_expected_closing_comment};
            return false;
        }
        advance_code_point(cursor); // '*'
        advance_code_point(cursor); // '/'
    }
    return true;
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
    diagnostic = make_diagnostic(cursor, k_expected_closing_parenthesis);
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
        diagnostic = make_diagnostic(cursor, k_expected_closing_parenthesis);
        return gltfx_gfss_token_kind::url;
    }
    diagnostic = make_diagnostic(cursor, k_expected_closing_parenthesis);
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
            diagnostic = make_diagnostic(cursor, k_expected_closing_parenthesis);
            return gltfx_gfss_token_kind::url;
        }
        if (is_whitespace(current)) {
            return consume_url_trailing_whitespace(cursor, diagnostic);
        }
        if (current == '"' || current == '\'' || current == '(' || is_non_printable(current)) {
            diagnostic = make_diagnostic(cursor, k_expected_closing_parenthesis);
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
    diagnostic = make_diagnostic(cursor, k_expected_escape_sequence);
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
            diagnostic = make_diagnostic(cursor, k_expected_closing_quote);
            return gltfx_gfss_token_kind::string;
        }
        if (detail::is_newline(current)) {
            diagnostic = make_diagnostic(cursor, k_expected_closing_quote);
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
    gltfx_gfss_diagnostic comment_diagnostic{};
    if (!skip_comments(cursor, comment_diagnostic)) {
        // GFSS-COMMENT-DIAG: a comment ran into EOF before its own
        // "*/" - end the WHOLE stream with a single diagnosed
        // <EOF-token>, positioned at the comment's own opening "/*"
        // (comment_diagnostic already carries that position, never
        // EOF's) - the SAME shape recover_from_forward_progress_
        // violation() uses for an internal defect, here for a genuine
        // consumer input error instead. skip_comments()'s own inner
        // loop already advanced `cursor` to source.size() on this path
        // (the "!at_end(cursor)" guard is what stopped it), so every
        // SUBSEQUENT call observes genuine EOF structurally, the same
        // guarantee the internal-defect recovery path documents below.
        out_token = gltfx_gfss_token{
            .kind = gltfx_gfss_token_kind::eof,
            .lexeme = std::string_view{},
            .line = comment_diagnostic.line,
            .column = comment_diagnostic.column,
            .diagnostic = comment_diagnostic,
        };
        return false;
    }

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
    // TWO REACTIONS. The assert() below still gives a developer doing a
    // genuine manual -DCMAKE_BUILD_TYPE=Debug build the exact, named
    // diagnostic - this half has not changed since 26/08/2026. What
    // runs in the build glintfx actually SHIPS (Release, -DNDEBUG,
    // where the assert compiles out) has now changed TWICE:
    //
    // 26/08/2026 (GODS_LAWS.md L-40 achado 2, "a guarda de progresso so
    // protege em depuracao"): the FIRST version of this guard was
    // assert() alone, on the SAME convention gltfx_rslt<T>'s
    // precondition guard uses (docs/api-conventions.md R1) - but that
    // convention fits a CALLER mistake (UB in Release is the accepted,
    // documented cost, same as std::optional::value()), and this guard
    // protects against something else: an INTERNAL glintfx defect that
    // hangs a CORRECTLY-BEHAVING consumer's process. Measured live:
    // tools/preci.sh's stage_sanitizer and every job of
    // .github/workflows/ci.yml configure with
    // -DCMAKE_BUILD_TYPE=Release, so the assert had NEVER fired in any
    // of glintfx's own gates, not even the sanitizer one. The fix
    // committed then (95c0f20) forced one code point of advance on
    // violation, so gltfx_gfss_tokenize()'s loop would always
    // terminate.
    //
    // 27/08/2026 (this fatia, GODS_LAWS.md L-40 - a NEW CRITICO the
    // adversarial review of 95c0f20 found): forcing an advance
    // terminates the LOOP, but manufactures a token stream that LOOKS
    // like real output - measured live, neutralizing
    // detail::consume_optional_sign() and tokenizing a leading
    // "-3.5e-2" through 95c0f20's own code produced
    // kind=number/lexeme="-" then kind=number/lexeme="3.5e-2", both
    // with diagnostic.expected EMPTY. docs/api-conventions.md's own R4
    // fixes what an empty `expected` means project-wide: "no
    // diagnostic was attached", i.e. "this token is fine". Handing a
    // consumer that false "fine" for OUR bug blames their
    // correctly-formed source for a defect that is entirely ours. The
    // decision (project leader, executed here, not reopened - see this
    // fatia's own commit message): on violation, do NOT advance and
    // keep producing tokens - PIN the cursor at source.size() and
    // return a single <EOF-token> carrying diagnostic_vocabulary.hpp's
    // own "internal_tokenizer_defect" identifier
    // (token_progress_recovery.hpp has the full rationale for why EOF,
    // why pinned to the END rather than past the violation, and why
    // this identifier names glintfx rather than the consumer's file).
    // Every subsequent call on the same cursor then observes genuine
    // EOF STRUCTURALLY (dispatch_token()'s own `current == -1` branch),
    // for ANY shape of caller loop, not just the canonical `while` this
    // file's own test happens to write. The assert() stays exactly as
    // it was: the Debug-build diagnostic; the `if` below is the
    // Release-build promise.
    const bool made_progress = token_made_forward_progress(kind, start_offset, cursor.byte_offset);
    assert(made_progress &&
           "gltfx_gfss_next_token(): a token production consumed zero code points - internal "
           "contract violation, would spin the caller's while loop forever");
    if (!made_progress) {
        out_token = recover_from_forward_progress_violation(cursor, start_line, start_column);
        return false;
    }

    out_token.kind = kind;
    out_token.lexeme = cursor.source.substr(start_offset, cursor.byte_offset - start_offset);
    out_token.line = start_line;
    out_token.column = start_column;
    out_token.diagnostic = diagnostic;
    return kind != gltfx_gfss_token_kind::eof;
}

} // namespace glintfx::style
