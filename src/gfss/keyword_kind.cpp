// SPDX-License-Identifier: AGPL-3.0-or-later
#include <glintfx/gfss/keyword.hpp>

#include <array>

// keyword_kind.cpp - GFSS-PROP-REGISTRY (TODO.md wave W4, GODS_LAWS.md
// L-17/L-40): name and value live in ONE table, the SAME technique
// value_kind.cpp/token_kind.cpp already established - this file
// answers exactly one question, "what SHEET spelling names this
// keyword?", nothing else. keyword.hpp's own header comment carries
// the vocabulary's own provenance (extracted mechanically from docs/
// gfss-property-registry-v1.md SS6, cross-checked against the
// ratified document) and the E2 reserved-word-suffix reasoning; not
// repeated here.
//
// THE static_assert BELOW IS THIS FATIA'S OWN "ADD WITHOUT
// REGISTERING IT, WATCH IT FAIL TO COMPILE" PROOF (GODS_LAWS.md L-40,
// same discipline as value_kind.cpp's own two tables): the array's own
// <..., 56> size is a HAND-WRITTEN literal, not gltfx_gfss_keyword_
// count itself - a word added to keyword.hpp's own X-macro list
// without a matching row here fails to build instead of silently
// returning "unknown" for a real enumerator.

namespace glintfx::style {

namespace {

struct keyword_entry {
    gltfx_gfss_keyword keyword = gltfx_gfss_keyword::none;
    std::string_view name;
};

// 56 rows, alphabetical by SHEET spelling (see keyword.hpp's own
// header comment for why alphabetical, not SS6 row order - unlike
// gltfx_gfss_property, no numeric append-only contract applies here).
constexpr std::array<keyword_entry, 56> k_keyword_table{{
    {gltfx_gfss_keyword::absolute, "absolute"},
    {gltfx_gfss_keyword::all, "all"},
    {gltfx_gfss_keyword::alternate, "alternate"},
    {gltfx_gfss_keyword::alternate_reverse, "alternate-reverse"},
    {gltfx_gfss_keyword::auto_keyword, "auto"},
    {gltfx_gfss_keyword::backwards, "backwards"},
    {gltfx_gfss_keyword::baseline, "baseline"},
    {gltfx_gfss_keyword::block, "block"},
    {gltfx_gfss_keyword::border_box, "border-box"},
    {gltfx_gfss_keyword::both, "both"},
    {gltfx_gfss_keyword::center, "center"},
    {gltfx_gfss_keyword::clip, "clip"},
    {gltfx_gfss_keyword::column, "column"},
    {gltfx_gfss_keyword::column_reverse, "column-reverse"},
    {gltfx_gfss_keyword::contain, "contain"},
    {gltfx_gfss_keyword::content_box, "content-box"},
    {gltfx_gfss_keyword::cover, "cover"},
    {gltfx_gfss_keyword::ellipsis, "ellipsis"},
    {gltfx_gfss_keyword::fill, "fill"},
    {gltfx_gfss_keyword::flex, "flex"},
    {gltfx_gfss_keyword::flex_end, "flex-end"},
    {gltfx_gfss_keyword::flex_start, "flex-start"},
    {gltfx_gfss_keyword::forwards, "forwards"},
    {gltfx_gfss_keyword::hidden, "hidden"},
    {gltfx_gfss_keyword::infinite, "infinite"},
    {gltfx_gfss_keyword::left, "left"},
    {gltfx_gfss_keyword::medium, "medium"},
    {gltfx_gfss_keyword::multiply, "multiply"},
    {gltfx_gfss_keyword::no_repeat, "no-repeat"},
    {gltfx_gfss_keyword::none, "none"},
    {gltfx_gfss_keyword::normal, "normal"},
    {gltfx_gfss_keyword::nowrap, "nowrap"},
    {gltfx_gfss_keyword::paused, "paused"},
    {gltfx_gfss_keyword::pixelated, "pixelated"},
    {gltfx_gfss_keyword::plus_lighter, "plus-lighter"},
    {gltfx_gfss_keyword::relative, "relative"},
    {gltfx_gfss_keyword::repeat, "repeat"},
    {gltfx_gfss_keyword::repeat_x, "repeat-x"},
    {gltfx_gfss_keyword::repeat_y, "repeat-y"},
    {gltfx_gfss_keyword::reverse, "reverse"},
    {gltfx_gfss_keyword::right, "right"},
    {gltfx_gfss_keyword::row, "row"},
    {gltfx_gfss_keyword::row_reverse, "row-reverse"},
    {gltfx_gfss_keyword::running, "running"},
    {gltfx_gfss_keyword::screen, "screen"},
    {gltfx_gfss_keyword::solid, "solid"},
    {gltfx_gfss_keyword::space_around, "space-around"},
    {gltfx_gfss_keyword::space_between, "space-between"},
    {gltfx_gfss_keyword::space_evenly, "space-evenly"},
    {gltfx_gfss_keyword::static_keyword, "static"},
    {gltfx_gfss_keyword::stretch, "stretch"},
    {gltfx_gfss_keyword::thick, "thick"},
    {gltfx_gfss_keyword::thin, "thin"},
    {gltfx_gfss_keyword::visible, "visible"},
    {gltfx_gfss_keyword::wrap, "wrap"},
    {gltfx_gfss_keyword::wrap_reverse, "wrap-reverse"},
}};

static_assert(k_keyword_table.size() == gltfx_gfss_keyword_count,
              "GODS_LAWS.md L-40: k_keyword_table's row count must track keyword.hpp's own "
              "gltfx_gfss_keyword_count - a word added to the X-macro list without a matching "
              "name row here must not compile silently");

} // namespace

std::string_view gltfx_gfss_keyword_name(gltfx_gfss_keyword keyword) noexcept {
    for (const keyword_entry &entry : k_keyword_table) {
        if (entry.keyword == keyword) {
            return entry.name;
        }
    }
    // docs/api-conventions.md R4: never undefined behavior.
    return "unknown";
}

} // namespace glintfx::style
