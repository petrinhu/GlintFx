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

// ENUMERATION, not a directed sample (GODS_LAWS.md L-40/L-27: "enumere
// o espaco pequeno quando ele for fechado" - token.hpp's own header
// comment names the CSS Syntax Module Level 3 source for this closed
// set of 25 classes). Every kind must round-trip through
// gltfx_gfss_token_kind_name() to a NON-EMPTY, space-free identifier
// (docs/api-conventions.md R7: vocabulary token, never a sentence),
// and no two kinds may share a name - a collision would silently make
// two different token classes indistinguishable in any log or message
// a consumer builds from the name alone.
GLINTFX_TEST(gltfx_gfss_token_kind_name_covers_every_class_with_a_distinct_token) {
    using glintfx::style::gltfx_gfss_token_kind;
    constexpr gltfx_gfss_token_kind k_all_kinds[] = {
        gltfx_gfss_token_kind::ident,       gltfx_gfss_token_kind::function,
        gltfx_gfss_token_kind::at_keyword,  gltfx_gfss_token_kind::hash,
        gltfx_gfss_token_kind::string,      gltfx_gfss_token_kind::bad_string,
        gltfx_gfss_token_kind::url,         gltfx_gfss_token_kind::bad_url,
        gltfx_gfss_token_kind::delim,       gltfx_gfss_token_kind::number,
        gltfx_gfss_token_kind::percentage,  gltfx_gfss_token_kind::dimension,
        gltfx_gfss_token_kind::whitespace,  gltfx_gfss_token_kind::cdo,
        gltfx_gfss_token_kind::cdc,         gltfx_gfss_token_kind::colon,
        gltfx_gfss_token_kind::semicolon,   gltfx_gfss_token_kind::comma,
        gltfx_gfss_token_kind::open_square, gltfx_gfss_token_kind::close_square,
        gltfx_gfss_token_kind::open_paren,  gltfx_gfss_token_kind::close_paren,
        gltfx_gfss_token_kind::open_curly,  gltfx_gfss_token_kind::close_curly,
        gltfx_gfss_token_kind::eof,
    };
    constexpr int k_expected_count = 25;
    static_assert(sizeof(k_all_kinds) / sizeof(k_all_kinds[0]) == k_expected_count,
                  "GODS_LAWS.md L-40: this array IS the closed enumeration - keep it in "
                  "sync with token.hpp's own enum, never sample a subset");

    int checked = 0;
    for (int outer = 0; outer < k_expected_count; ++outer) {
        const std::string_view name =
            glintfx::style::gltfx_gfss_token_kind_name(k_all_kinds[outer]);
        GLINTFX_CHECK(!name.empty());
        GLINTFX_CHECK(name.find(' ') == std::string_view::npos);
        for (int inner = outer + 1; inner < k_expected_count; ++inner) {
            GLINTFX_CHECK(name != glintfx::style::gltfx_gfss_token_kind_name(k_all_kinds[inner]));
        }
        ++checked;
    }
    // L-40: the count swept is printed even when everything passes.
    std::println("gltfx_gfss_token_kind_name_covers_every_class_with_a_distinct_token: {} "
                 "token class(es) checked",
                 checked);
}
