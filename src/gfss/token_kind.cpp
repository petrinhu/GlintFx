// SPDX-License-Identifier: AGPL-3.0-or-later
#include <glintfx/gfss/token.hpp>

#include <array>

// token_kind.cpp - GFSS-TOKEN (TODO.md, GODS_LAWS.md L-17): name and
// value live in ONE table, the same technique core/err_code.cpp
// already established for gltfx_err_code_name(). This file answers
// exactly one question - "what identifier names this token class?" -
// nothing else.

namespace glintfx::style {

namespace {

struct token_kind_entry {
    gltfx_gfss_token_kind kind = gltfx_gfss_token_kind::eof;
    std::string_view name;
};

// THE table - 25 rows, one per CSS Syntax Module Level 3 token class
// (see token.hpp's own header comment for the source). Order matches
// the enum declaration; tests/gfss_token_kind_enumeration_test.cpp
// proves this table's row count against token.hpp's own enumerator
// count with a closed enumeration (GODS_LAWS.md L-40), not by eye.
constexpr std::array<token_kind_entry, 25> k_table{{
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
}};

} // namespace

std::string_view gltfx_gfss_token_kind_name(gltfx_gfss_token_kind kind) noexcept {
    for (const token_kind_entry &entry : k_table) {
        if (entry.kind == kind) {
            return entry.name;
        }
    }
    // A raw value this build's table does not recognize (docs/api-
    // conventions.md R4's graceful-degradation convention, same as
    // gltfx_err_code_name()): never undefined behavior.
    return "unknown";
}

} // namespace glintfx::style
