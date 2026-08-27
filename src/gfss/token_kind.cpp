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
// the enum declaration.
//
// CORRECTION of 26/08/2026 (GODS_LAWS.md L-40 achado 1): this comment
// used to claim a file, tests/gfss_token_kind_enumeration_test.cpp,
// that never existed - the row count below was compared against
// nothing, and a 26th enum value added by hand compiled clean with
// this table silently one row short. The static_assert right after the
// table is the actual proof now: it fails to COMPILE the moment this
// table's size and token.hpp's own gltfx_gfss_token_kind_count (itself
// mechanically counted from GLINTFX_GFSS_TOKEN_KIND_LIST, never a
// hand-copied literal) disagree.
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

// GODS_LAWS.md L-40 achado 1: the enumeration this table claims to
// close is now closed BY CONSTRUCTION, not by eye - a class added to
// GLINTFX_GFSS_TOKEN_KIND_LIST (token.hpp) without a matching row here
// fails to compile this file.
static_assert(k_table.size() == gltfx_gfss_token_kind_count,
              "GODS_LAWS.md L-40: k_table's row count must track token.hpp's own "
              "gltfx_gfss_token_kind_count - a class added to the enum without a name row here "
              "must not compile silently");

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
