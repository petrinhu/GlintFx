// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstdint>
#include <print>
#include <string_view>

#include <glintfx/gfss/token.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// gfss_token_test.cpp - GFSS-TOKEN (TODO.md, GODS_LAWS.md L-20): the
// TDD red/green witness for glintfx::style::gltfx_gfss_token_kind,
// gltfx_gfss_diagnostic and gltfx_gfss_token - see token.hpp's own
// header comment for the design rationale each check below proves.

GLINTFX_TEST(gltfx_gfss_diagnostic_default_reads_back_as_absent) {
    // R4 convention (docs/api-conventions.md): a default-constructed
    // diagnostic has no line/column/expected - empty/zero means
    // "never attached", not undefined behavior.
    constexpr glintfx::style::gltfx_gfss_diagnostic diagnostic;
    GLINTFX_CHECK_EQ(diagnostic.line, static_cast<std::uint32_t>(0));
    GLINTFX_CHECK_EQ(diagnostic.column, static_cast<std::uint32_t>(0));
    GLINTFX_CHECK(diagnostic.expected.empty());
}

GLINTFX_TEST(gltfx_gfss_diagnostic_aggregate_init_reads_back_the_same_three_fields) {
    constexpr glintfx::style::gltfx_gfss_diagnostic diagnostic{
        .line = 3, .column = 7, .expected = "closing_quote"};
    GLINTFX_CHECK_EQ(diagnostic.line, static_cast<std::uint32_t>(3));
    GLINTFX_CHECK_EQ(diagnostic.column, static_cast<std::uint32_t>(7));
    GLINTFX_CHECK(diagnostic.expected == std::string_view{"closing_quote"});
}

GLINTFX_TEST(gltfx_gfss_token_default_is_eof_with_no_diagnostic) {
    constexpr glintfx::style::gltfx_gfss_token token;
    GLINTFX_CHECK(token.kind == glintfx::style::gltfx_gfss_token_kind::eof);
    GLINTFX_CHECK(token.lexeme.empty());
    GLINTFX_CHECK(token.diagnostic.expected.empty());
}

GLINTFX_TEST(gltfx_gfss_token_aggregate_init_reads_back_every_field) {
    constexpr glintfx::style::gltfx_gfss_token token{
        .kind = glintfx::style::gltfx_gfss_token_kind::ident,
        .lexeme = "foo",
        .line = 2,
        .column = 5,
        .diagnostic = {},
    };
    GLINTFX_CHECK(token.kind == glintfx::style::gltfx_gfss_token_kind::ident);
    GLINTFX_CHECK(token.lexeme == std::string_view{"foo"});
    GLINTFX_CHECK_EQ(token.line, static_cast<std::uint32_t>(2));
    GLINTFX_CHECK_EQ(token.column, static_cast<std::uint32_t>(5));
    GLINTFX_CHECK(token.diagnostic.expected.empty());
}

namespace {

struct kind_name_sample {
    glintfx::style::gltfx_gfss_token_kind kind = glintfx::style::gltfx_gfss_token_kind::eof;
    std::string_view expected_name;
};

} // namespace

// ENUMERATION, not a directed sample (GODS_LAWS.md L-40/L-27: "enumere
// o espaco pequeno quando ele for fechado" - token.hpp's own header
// comment names the CSS Syntax Module Level 3 source for this closed
// set of 25 classes). Every kind must round-trip through
// gltfx_gfss_token_kind_name() to its EXACT expected identifier - not
// merely "non-empty and distinct from its neighbors" (a mutation-
// testing pass on this same fatia found that a weaker check like that
// survives swapping two names in token_kind.cpp's own table: both
// stay non-empty and mutually distinct, so only checking the EXACT
// string per kind catches it).
GLINTFX_TEST(gltfx_gfss_token_kind_name_covers_every_class_with_its_exact_identifier) {
    using glintfx::style::gltfx_gfss_token_kind;
    constexpr kind_name_sample k_samples[] = {
        {gltfx_gfss_token_kind::ident, "ident"},
        {gltfx_gfss_token_kind::function, "function"},
        {gltfx_gfss_token_kind::at_keyword, "at_keyword"},
        {gltfx_gfss_token_kind::hash, "hash"},
        {gltfx_gfss_token_kind::string, "string"},
        {gltfx_gfss_token_kind::bad_string, "bad_string"},
        {gltfx_gfss_token_kind::url, "url"},
        {gltfx_gfss_token_kind::bad_url, "bad_url"},
        {gltfx_gfss_token_kind::delim, "delim"},
        {gltfx_gfss_token_kind::number, "number"},
        {gltfx_gfss_token_kind::percentage, "percentage"},
        {gltfx_gfss_token_kind::dimension, "dimension"},
        {gltfx_gfss_token_kind::whitespace, "whitespace"},
        {gltfx_gfss_token_kind::cdo, "cdo"},
        {gltfx_gfss_token_kind::cdc, "cdc"},
        {gltfx_gfss_token_kind::colon, "colon"},
        {gltfx_gfss_token_kind::semicolon, "semicolon"},
        {gltfx_gfss_token_kind::comma, "comma"},
        {gltfx_gfss_token_kind::open_square, "open_square"},
        {gltfx_gfss_token_kind::close_square, "close_square"},
        {gltfx_gfss_token_kind::open_paren, "open_paren"},
        {gltfx_gfss_token_kind::close_paren, "close_paren"},
        {gltfx_gfss_token_kind::open_curly, "open_curly"},
        {gltfx_gfss_token_kind::close_curly, "close_curly"},
        {gltfx_gfss_token_kind::eof, "eof"},
    };
    // GODS_LAWS.md L-40 achado 1 of 26/08/2026: this used to compare
    // against a hand-copied literal (25) that had no link to
    // token.hpp's own enum - a 26th value added there compiled clean
    // and this static_assert kept passing. gltfx_gfss_token_kind_count
    // is mechanically counted from the SAME list the enum itself is
    // generated from (token.hpp's GLINTFX_GFSS_TOKEN_KIND_LIST), so a
    // class added to the enum without a row added HERE now fails to
    // compile instead of passing silently.
    static_assert(sizeof(k_samples) / sizeof(k_samples[0]) ==
                      glintfx::style::gltfx_gfss_token_kind_count,
                  "GODS_LAWS.md L-40: this table IS the closed enumeration - keep it in "
                  "sync with token.hpp's own enum, never sample a subset");

    int checked = 0;
    for (const kind_name_sample &sample : k_samples) {
        const std::string_view name = glintfx::style::gltfx_gfss_token_kind_name(sample.kind);
        GLINTFX_CHECK(name == sample.expected_name);
        GLINTFX_CHECK(name.find(' ') == std::string_view::npos);
        ++checked;
    }
    // L-40: the count swept is printed even when everything passes.
    std::println("gltfx_gfss_token_kind_name_covers_every_class_with_its_exact_identifier: {} "
                 "token class(es) checked",
                 checked);
}
