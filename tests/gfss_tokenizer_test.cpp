// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <cstdint>
#include <print>
#include <string_view>
#include <vector>

#include <glintfx/gfss/token.hpp>
#include <glintfx/gfss/tokenizer.hpp>

#include "gfss/diagnostic_vocabulary.hpp"
#include "gfss/token_progress_guard.hpp"
#include "gfss/token_progress_recovery.hpp"
#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// gfss_tokenizer_test.cpp - GFSS-TOKEN (TODO.md, GODS_LAWS.md L-20):
// the TDD red/green witness for glintfx::style::gltfx_gfss_next_token()/
// gltfx_gfss_tokenize() - see tokenizer.hpp's own header comment for
// the design (two-layer API, why this never returns gltfx_rslt<T>) and
// token.hpp's for the CSS Syntax Module Level 3 source of the 25-class
// vocabulary this file enumerates CLOSED (GODS_LAWS.md L-40).

namespace {

using glintfx::style::gltfx_gfss_token;
using glintfx::style::gltfx_gfss_token_kind;
using glintfx::style::gltfx_gfss_tokenize;

// --- per-token-class production slice --------------------------------

struct kind_sample {
    // Default member initializers, not a user-declared constructor
    // (which would forfeit aggregate-init at every row of k_samples
    // below) - the same cppcheck uninitMemberVarNoCtor fix
    // src/core/err_code.cpp's own k_table already applies: the
    // checker cannot see that aggregate init already sets every field
    // at every use site.
    std::string_view source;
    gltfx_gfss_token_kind expected_kind = gltfx_gfss_token_kind::eof;
    std::string_view expected_lexeme;
};

// ENUMERATION, not a directed sample (GODS_LAWS.md L-27/L-40): one row
// per CSS Syntax Module Level 3 token class (see token.hpp's own
// header comment for the source), in the SAME order token.hpp declares
// the enum, so a class added there without a row here is easy to
// spot on review. std::array size is checked against the enum's own
// known cardinality (25) below, not assumed.
constexpr std::array<kind_sample, 25> k_samples{{
    {"foo", gltfx_gfss_token_kind::ident, "foo"},
    {"foo(", gltfx_gfss_token_kind::function, "foo("},
    {"@media", gltfx_gfss_token_kind::at_keyword, "@media"},
    {"#a1", gltfx_gfss_token_kind::hash, "#a1"},
    {"'abc'", gltfx_gfss_token_kind::string, "'abc'"},
    {"'abc\n", gltfx_gfss_token_kind::bad_string, "'abc"},
    {"url(foo.png)", gltfx_gfss_token_kind::url, "url(foo.png)"},
    {"url(foo bar)", gltfx_gfss_token_kind::bad_url, "url(foo bar)"},
    {"^", gltfx_gfss_token_kind::delim, "^"},
    {"42", gltfx_gfss_token_kind::number, "42"},
    {"42%", gltfx_gfss_token_kind::percentage, "42%"},
    {"42px", gltfx_gfss_token_kind::dimension, "42px"},
    {"   ", gltfx_gfss_token_kind::whitespace, "   "},
    {"<!--", gltfx_gfss_token_kind::cdo, "<!--"},
    {"-->", gltfx_gfss_token_kind::cdc, "-->"},
    {":", gltfx_gfss_token_kind::colon, ":"},
    {";", gltfx_gfss_token_kind::semicolon, ";"},
    {",", gltfx_gfss_token_kind::comma, ","},
    {"[", gltfx_gfss_token_kind::open_square, "["},
    {"]", gltfx_gfss_token_kind::close_square, "]"},
    {"(", gltfx_gfss_token_kind::open_paren, "("},
    {")", gltfx_gfss_token_kind::close_paren, ")"},
    {"{", gltfx_gfss_token_kind::open_curly, "{"},
    {"}", gltfx_gfss_token_kind::close_curly, "}"},
    {"", gltfx_gfss_token_kind::eof, ""},
}};

} // namespace

GLINTFX_TEST(gltfx_gfss_tokenize_produces_every_one_of_the_25_closed_token_classes) {
    // GODS_LAWS.md L-40 achado 1 of 26/08/2026: this used to compare
    // against a hand-copied literal (25) that had no link to
    // token.hpp's own enum - a 26th value added there compiled clean
    // and this static_assert kept passing. gltfx_gfss_token_kind_count
    // is mechanically counted from the SAME list the enum itself is
    // generated from (token.hpp's GLINTFX_GFSS_TOKEN_KIND_LIST), so a
    // class added to the enum without a row added HERE now fails to
    // compile instead of passing silently.
    constexpr std::size_t k_expected_count = glintfx::style::gltfx_gfss_token_kind_count;
    static_assert(k_samples.size() == k_expected_count,
                  "GODS_LAWS.md L-40: this table IS the closed enumeration - one row per "
                  "CSS Syntax Module Level 3 token class, never a sampled subset");

    std::size_t checked = 0;
    for (const kind_sample &sample : k_samples) {
        const std::vector<gltfx_gfss_token> tokens = gltfx_gfss_tokenize(sample.source);
        GLINTFX_CHECK(!tokens.empty());
        if (!tokens.empty()) {
            GLINTFX_CHECK(tokens.front().kind == sample.expected_kind);
            GLINTFX_CHECK(tokens.front().lexeme == sample.expected_lexeme);
        }
        ++checked;
    }
    // GODS_LAWS.md L-40: zero checked would be a floor violation, not
    // a pass - the count is printed even when every case is green.
    GLINTFX_CHECK_EQ(checked, k_expected_count);
    std::println("gltfx_gfss_tokenize_produces_every_one_of_the_25_closed_token_classes: {} "
                 "token class(es) checked",
                 checked);
}

// --- the third diagnostic field: line, column, "what was expected" ---

GLINTFX_TEST(gltfx_gfss_tokenize_reports_no_diagnostic_on_a_well_formed_token) {
    const std::vector<gltfx_gfss_token> tokens = gltfx_gfss_tokenize("foo");
    GLINTFX_CHECK(!tokens.empty());
    if (!tokens.empty()) {
        GLINTFX_CHECK(tokens.front().diagnostic.expected.empty());
        GLINTFX_CHECK_EQ(tokens.front().diagnostic.line, static_cast<std::uint32_t>(0));
        GLINTFX_CHECK_EQ(tokens.front().diagnostic.column, static_cast<std::uint32_t>(0));
    }
}

GLINTFX_TEST(gltfx_gfss_tokenize_bad_string_from_embedded_newline_expects_closing_quote) {
    const std::vector<gltfx_gfss_token> tokens = gltfx_gfss_tokenize("'abc\n");
    GLINTFX_CHECK(!tokens.empty());
    if (!tokens.empty()) {
        GLINTFX_CHECK(tokens.front().kind == gltfx_gfss_token_kind::bad_string);
        GLINTFX_CHECK(tokens.front().diagnostic.expected == std::string_view{"closing_quote"});
    }
}

// The SAME missing-closing-quote condition, but hit via EOF instead of
// an embedded newline - CSS Syntax Module Level 3 section 4.3.5 keeps
// the token kind as <string-token> here (NOT <bad-string-token>,
// unlike the newline case above), while still calling it a parse
// error. Proves this module's diagnostic is attached on BOTH paths
// even though only one of them changes the token's own kind.
GLINTFX_TEST(
    gltfx_gfss_tokenize_unterminated_string_at_eof_stays_string_but_expects_closing_quote) {
    const std::vector<gltfx_gfss_token> tokens = gltfx_gfss_tokenize("'abc");
    GLINTFX_CHECK(!tokens.empty());
    if (!tokens.empty()) {
        GLINTFX_CHECK(tokens.front().kind == gltfx_gfss_token_kind::string);
        GLINTFX_CHECK(tokens.front().diagnostic.expected == std::string_view{"closing_quote"});
    }
}

GLINTFX_TEST(gltfx_gfss_tokenize_bad_url_expects_closing_parenthesis) {
    const std::vector<gltfx_gfss_token> tokens = gltfx_gfss_tokenize("url(foo bar)");
    GLINTFX_CHECK(!tokens.empty());
    if (!tokens.empty()) {
        GLINTFX_CHECK(tokens.front().kind == gltfx_gfss_token_kind::bad_url);
        GLINTFX_CHECK(tokens.front().diagnostic.expected ==
                      std::string_view{"closing_parenthesis"});
    }
}

// 4.3.1's own U+005C branch: a lone backslash immediately followed by
// a newline is NOT a valid escape (4.3.8: "if the second code point is
// a newline, return false") - this is a <delim-token> carrying its own
// diagnostic, and the newline itself is left for the NEXT call to
// read as its own <whitespace-token> ("reconsume", never double-count).
GLINTFX_TEST(gltfx_gfss_tokenize_lone_backslash_before_newline_is_delim_expecting_escape_sequence) {
    const std::vector<gltfx_gfss_token> tokens = gltfx_gfss_tokenize("\\\nfoo");
    GLINTFX_CHECK(tokens.size() >= 3);
    if (tokens.size() >= 3) {
        GLINTFX_CHECK(tokens[0].kind == gltfx_gfss_token_kind::delim);
        GLINTFX_CHECK(tokens[0].lexeme == std::string_view{"\\"});
        GLINTFX_CHECK(tokens[0].diagnostic.expected == std::string_view{"escape_sequence"});
        GLINTFX_CHECK(tokens[1].kind == gltfx_gfss_token_kind::whitespace);
        GLINTFX_CHECK(tokens[2].kind == gltfx_gfss_token_kind::ident);
    }
}

// --- comments are skipped, never their own token kind -----------------

GLINTFX_TEST(gltfx_gfss_tokenize_skips_comments_between_real_tokens) {
    const std::vector<gltfx_gfss_token> tokens = gltfx_gfss_tokenize("/* a comment */foo");
    GLINTFX_CHECK(tokens.size() == 2); // ident "foo", then EOF - the comment produces NOTHING
    if (tokens.size() == 2) {
        GLINTFX_CHECK(tokens[0].kind == gltfx_gfss_token_kind::ident);
        GLINTFX_CHECK(tokens[0].lexeme == std::string_view{"foo"});
        GLINTFX_CHECK(tokens[1].kind == gltfx_gfss_token_kind::eof);
    }
}

// --- position tracking: line/column, and a code-point-aware ident ----

GLINTFX_TEST(gltfx_gfss_tokenize_tracks_line_and_column_across_a_newline) {
    const std::vector<gltfx_gfss_token> tokens = gltfx_gfss_tokenize("a\nb");
    GLINTFX_CHECK(tokens.size() >= 3);
    if (tokens.size() >= 3) {
        GLINTFX_CHECK_EQ(tokens[0].line, static_cast<std::uint32_t>(1));
        GLINTFX_CHECK_EQ(tokens[0].column, static_cast<std::uint32_t>(1));
        GLINTFX_CHECK_EQ(tokens[1].line,
                         static_cast<std::uint32_t>(1)); // the newline STARTS on line 1
        GLINTFX_CHECK_EQ(tokens[1].column, static_cast<std::uint32_t>(2));
        GLINTFX_CHECK_EQ(tokens[2].line, static_cast<std::uint32_t>(2)); // "b" is on the NEXT line
        GLINTFX_CHECK_EQ(tokens[2].column, static_cast<std::uint32_t>(1));
    }
}

// A two-byte UTF-8 ident-start code point (U+00E9 LATIN SMALL LETTER E
// WITH ACUTE, "e" with an accent) is still ONE ident code point per
// CSS Syntax Module Level 3's "non-ASCII code point" definition
// (section 4.2: any value >= U+0080) - proves cursor_ops.hpp's
// multi-byte advance keeps the WHOLE ident sequence (and the token
// after it) inside one contiguous lexeme instead of splitting mid
// code point.
GLINTFX_TEST(gltfx_gfss_tokenize_treats_a_multibyte_utf8_code_point_as_one_ident_code_point) {
    const std::vector<gltfx_gfss_token> tokens = gltfx_gfss_tokenize("caf\xC3\xA9 42");
    GLINTFX_CHECK(tokens.size() >= 4);
    if (tokens.size() >= 4) {
        GLINTFX_CHECK(tokens[0].kind == gltfx_gfss_token_kind::ident);
        GLINTFX_CHECK(tokens[0].lexeme == std::string_view{"caf\xC3\xA9"});
        GLINTFX_CHECK(tokens[1].kind == gltfx_gfss_token_kind::whitespace);
        GLINTFX_CHECK(tokens[2].kind == gltfx_gfss_token_kind::number);
    }
}

// --- 4.3.12 "Consume a number": sign, fraction and exponent ----------
//
// Found MISSING by this fatia's own mutation-testing pass (GODS_LAWS.md
// L-40's "prove the mutant reaches the code" discipline turned outward
// on the test suite itself, not just on ONE function): the closed-
// enumeration table above only ever fed consume_number() a bare "42",
// which never exercises consume_optional_sign()/consume_optional_
// fraction()/consume_optional_exponent() at all - a mutant that no-ops
// any of the three (lexical_rules.cpp) would have passed the WHOLE
// suite silently before this test existed.
GLINTFX_TEST(gltfx_gfss_tokenize_number_with_sign_fraction_and_exponent) {
    const std::vector<gltfx_gfss_token> tokens = gltfx_gfss_tokenize("-3.5e-2");
    GLINTFX_CHECK(!tokens.empty());
    if (!tokens.empty()) {
        GLINTFX_CHECK(tokens.front().kind == gltfx_gfss_token_kind::number);
        GLINTFX_CHECK(tokens.front().lexeme == std::string_view{"-3.5e-2"});
    }
}

// --- the zero-progress guard's own predicate (GODS_LAWS.md L-40
// achado 2 of 26/08/2026) --------------------------------------------
//
// token_progress_guard.hpp's own header comment has the full
// rationale: this exercises the VIOLATION directly, with hand-picked
// offsets, instead of needing a genuinely broken dispatch_token()
// committed anywhere in the tree. The three rows below are the three
// shapes gltfx_gfss_next_token() actually calls this predicate with:
// a normal non-eof production that advanced (true), the <EOF-token>
// case where "no advance" is CORRECT, not a violation (true), and the
// one case this guard exists to catch - a non-eof kind reported at the
// SAME offset it started (false).
GLINTFX_TEST(token_made_forward_progress_true_when_a_non_eof_token_advances_the_cursor) {
    using glintfx::style::detail::token_made_forward_progress;
    GLINTFX_CHECK(token_made_forward_progress(gltfx_gfss_token_kind::number, 5, 6));
}

GLINTFX_TEST(token_made_forward_progress_true_at_eof_even_with_zero_advance) {
    using glintfx::style::detail::token_made_forward_progress;
    GLINTFX_CHECK(token_made_forward_progress(gltfx_gfss_token_kind::eof, 5, 5));
}

GLINTFX_TEST(token_made_forward_progress_false_on_a_non_eof_token_at_the_same_offset) {
    using glintfx::style::detail::token_made_forward_progress;
    GLINTFX_CHECK(!token_made_forward_progress(gltfx_gfss_token_kind::number, 5, 5));
}

// --- a realistic multi-token slice, the shape a real gfss declaration
// will eventually be parsed from -------------------------------------

// --- GFSS-TOKEN, GODS_LAWS.md L-40 CRITICO that reproved commit
// 95c0f20: the consumer must receive a SIGNAL, never a plausible-but-
// false token, on internal defect ------------------------------------
//
// T1: the recovery atom itself (src/gfss/token_progress_recovery.hpp),
// exercised with a HAND-PICKED violation on a REAL cursor over REAL
// source - offsets chosen so the cursor sits mid-buffer (as it would
// after a real dispatch_token() zero-progress violation), not a
// throwaway empty string. Asserts the WHOLE token: kind, diagnostic
// (both non-empty AND equal to the one true identifier), position,
// empty lexeme, and the cursor pinned at source.size(). Then calls the
// REAL gltfx_gfss_next_token() on that now-pinned cursor and proves
// the flow STAYS terminated - genuine <EOF-token>, false - which is
// the guarantee that has to hold for ANY shape of caller loop, not
// just the one this test happens to write.
GLINTFX_TEST(recover_from_forward_progress_violation_signals_the_consumer_not_a_plausible_token) {
    using glintfx::style::detail::k_expected_internal_tokenizer_defect;
    using glintfx::style::detail::recover_from_forward_progress_violation;
    using glintfx::style::gltfx_gfss_cursor;

    constexpr std::string_view source{"-3.5e-2"};
    constexpr std::uint32_t violation_line = 1;
    constexpr std::uint32_t violation_column = 3;
    gltfx_gfss_cursor cursor{
        .source = source, .byte_offset = 2, .line = violation_line, .column = violation_column};

    const gltfx_gfss_token recovered =
        recover_from_forward_progress_violation(cursor, violation_line, violation_column);

    GLINTFX_CHECK(recovered.kind == gltfx_gfss_token_kind::eof);
    GLINTFX_CHECK(!recovered.diagnostic.expected.empty());
    GLINTFX_CHECK(recovered.diagnostic.expected == k_expected_internal_tokenizer_defect);
    GLINTFX_CHECK(recovered.diagnostic.expected == std::string_view{"internal_tokenizer_defect"});
    GLINTFX_CHECK_EQ(recovered.diagnostic.line, violation_line);
    GLINTFX_CHECK_EQ(recovered.diagnostic.column, violation_column);
    GLINTFX_CHECK(recovered.lexeme.empty());
    GLINTFX_CHECK_EQ(cursor.byte_offset, source.size());

    gltfx_gfss_token next{};
    const bool has_more = glintfx::style::gltfx_gfss_next_token(cursor, next);
    GLINTFX_CHECK(!has_more);
    GLINTFX_CHECK(next.kind == gltfx_gfss_token_kind::eof);
}

// T2: the vocabulary is enumerated CLOSED (GODS_LAWS.md L-40's "the
// space is small, enumerate it whole", not a search of what call sites
// happen to use), every identifier obeys R7 (docs/api-conventions.md:
// snake_case, no space, never a sentence), and every one of the four
// is PRODUCED for real - the three spec-driven ones via directed
// malformed input, the internal one via the recovery atom.
GLINTFX_TEST(diagnostic_vocabulary_is_enumerated_closed_and_every_identifier_is_produced) {
    using glintfx::style::detail::k_expected_vocabulary;
    using glintfx::style::detail::k_expected_vocabulary_count;
    using glintfx::style::detail::recover_from_forward_progress_violation;

    // GODS_LAWS.md L-40: this table IS the closed enumeration - a
    // fifth identifier added to diagnostic_vocabulary.hpp's own list
    // without a matching row added to the directed-production block
    // below fails to compile instead of passing silently.
    static_assert(k_expected_vocabulary_count == 4,
                  "GODS_LAWS.md L-40: diagnostic_vocabulary.hpp's list changed - update the "
                  "directed-production coverage below to match");

    std::size_t swept = 0;
    for (const std::string_view identifier : k_expected_vocabulary) {
        GLINTFX_CHECK(!identifier.empty());
        bool is_snake_case = true;
        for (const char ch : identifier) {
            const bool is_lower = ch >= 'a' && ch <= 'z';
            const bool is_digit_char = ch >= '0' && ch <= '9';
            const bool is_underscore = ch == '_';
            if (!is_lower && !is_digit_char && !is_underscore) {
                is_snake_case = false;
                break;
            }
        }
        GLINTFX_CHECK(is_snake_case);
        GLINTFX_CHECK(identifier.find(' ') == std::string_view::npos);
        ++swept;
    }
    // GODS_LAWS.md L-40: zero swept is a floor violation, never a pass.
    GLINTFX_CHECK(swept > 0);
    GLINTFX_CHECK_EQ(swept, k_expected_vocabulary_count);
    std::println("diagnostic_vocabulary_is_enumerated_closed_and_every_identifier_is_produced: "
                 "{} identifier(s) swept",
                 swept);

    {
        const std::vector<gltfx_gfss_token> tokens = gltfx_gfss_tokenize("'abc\n");
        GLINTFX_CHECK(!tokens.empty());
        if (!tokens.empty()) {
            GLINTFX_CHECK(tokens.front().diagnostic.expected == std::string_view{"closing_quote"});
        }
    }
    {
        const std::vector<gltfx_gfss_token> tokens = gltfx_gfss_tokenize("url(foo bar)");
        GLINTFX_CHECK(!tokens.empty());
        if (!tokens.empty()) {
            GLINTFX_CHECK(tokens.front().diagnostic.expected ==
                          std::string_view{"closing_parenthesis"});
        }
    }
    {
        const std::vector<gltfx_gfss_token> tokens = gltfx_gfss_tokenize("\\\nfoo");
        GLINTFX_CHECK(!tokens.empty());
        if (!tokens.empty()) {
            GLINTFX_CHECK(tokens.front().diagnostic.expected ==
                          std::string_view{"escape_sequence"});
        }
    }
    {
        glintfx::style::gltfx_gfss_cursor cursor{.source = "x", .byte_offset = 0, .line = 1, .column = 1};
        const gltfx_gfss_token recovered = recover_from_forward_progress_violation(cursor, 1, 1);
        GLINTFX_CHECK(recovered.diagnostic.expected ==
                      std::string_view{"internal_tokenizer_defect"});
    }
}

GLINTFX_TEST(gltfx_gfss_tokenize_a_short_declaration_like_slice) {
    const std::vector<gltfx_gfss_token> tokens = gltfx_gfss_tokenize("color: red;");
    // ident "color", colon, whitespace, ident "red", semicolon, EOF.
    GLINTFX_CHECK(tokens.size() == 6);
    if (tokens.size() == 6) {
        GLINTFX_CHECK(tokens[0].kind == gltfx_gfss_token_kind::ident);
        GLINTFX_CHECK(tokens[0].lexeme == std::string_view{"color"});
        GLINTFX_CHECK(tokens[1].kind == gltfx_gfss_token_kind::colon);
        GLINTFX_CHECK(tokens[2].kind == gltfx_gfss_token_kind::whitespace);
        GLINTFX_CHECK(tokens[3].kind == gltfx_gfss_token_kind::ident);
        GLINTFX_CHECK(tokens[3].lexeme == std::string_view{"red"});
        GLINTFX_CHECK(tokens[4].kind == gltfx_gfss_token_kind::semicolon);
        GLINTFX_CHECK(tokens[5].kind == gltfx_gfss_token_kind::eof);
    }
}
