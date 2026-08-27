// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstdint>
#include <print>
#include <string_view>
#include <vector>

#include <glintfx/gfss/token.hpp>
#include <glintfx/gfss/tokenizer.hpp>

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
    constexpr int k_expected_count = 25;
    static_assert(k_samples.size() == k_expected_count,
                  "GODS_LAWS.md L-40: this table IS the closed enumeration - one row per "
                  "CSS Syntax Module Level 3 token class, never a sampled subset");

    int checked = 0;
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

// --- a realistic multi-token slice, the shape a real gfss declaration
// will eventually be parsed from -------------------------------------

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
