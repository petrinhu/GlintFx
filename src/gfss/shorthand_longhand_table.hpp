// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <glintfx/gfss/property.hpp>

#include "shorthand_name_vocabulary.hpp"

// shorthand_longhand_table.hpp - GFSS-SHORTHAND, D-W6-3/D-W6-10 (TODO.md
// wave W6, GODS_LAWS.md L-17/L-40, docs/plano-w6-folha-de-estilo.md S-3):
// the CLOSED table mapping each of the eleven accepted shorthand names
// (shorthand_name_vocabulary.hpp) to its FAMILY (which expansion rule
// applies) and the exact, ORDERED list of longhand properties it expands
// into. Answers exactly one question - "given a shorthand's own sheet
// spelling, which family and which longhands?" - never HOW a family
// expands a value (that is each shorthand_four_sides.hpp/shorthand_
// four_corners.hpp/shorthand_axis_pair.hpp/shorthand_border_parts.hpp/
// shorthand_flex_flow.hpp's own job) nor WHERE in the declaration list
// the expansion lands (shorthand_expand.hpp's own job, called from
// declaration_list_parse.cpp's fold_into_result).
//
// ORDER IS THE OUTPUT ORDER (D-W6-3's own invariant, "a posicao e
// preservada por construcao"): shorthand_expand.cpp appends longhands[i]
// for i in [0, longhand_count) verbatim, at the shorthand's own index in
// the declaration list - this array's own order therefore IS the order
// a consumer sees. Every row below matches property.hpp's own id order
// for that property group (property_value_contract.hpp's own table
// already groups them the same way), so reading this table against
// property.hpp is a mechanical, line-for-line comparison, never free
// interpretation.
//
// BORDER-RADIUS' OWN CORNER ORDER (dossie SS3.2, D-W6-3's plan): TL, TR,
// BR, BL - NEVER the "sentido horario a partir do topo" order the other
// four-value families use for sides (T, R, B, L). This is why border-
// radius gets its own family (shorthand_four_corners), never sharing
// code with shorthand_four_sides.
//
// `border`'S OWN 12 LONGHANDS ARE GROUPED BY SUB-PROPERTY, NOT BY SIDE
// (property_value_contract.hpp's own table order, ids 38-49): every
// *-width first, then every *-style, then every *-color - shorthand_
// border_parts.cpp's own REPLICATE-FACTOR arithmetic (longhand_count / 3
// = 4 for `border`, 1 for `outline`) depends on this exact grouping.

namespace glintfx::style::detail {

enum class shorthand_family : std::uint8_t {
    four_sides,   // margin, padding, border-width, border-style, border-color
    four_corners, // border-radius (TL, TR, BR, BL - never shares code with four_sides)
    axis_pair,    // gap (row, column), overflow (x, y)
    border_parts, // border (12 longhands), outline (3 longhands) - width/style/color, free order
    flex_flow,    // flex-direction, flex-wrap - free order, disjoint keyword sets
};

// 12 is the ceiling this whole track needs (`border`'s own 12 longhands,
// F8) - every other family uses fewer slots, left at their own default-
// initialized `gltfx_gfss_property::display` (never read past
// `longhand_count`).
struct shorthand_longhand_entry {
    std::string_view name; // sheet spelling, e.g. "margin" - matches k_shorthand_names verbatim
    shorthand_family family = shorthand_family::four_sides;
    std::array<gltfx_gfss_property, 12> longhands{};
    std::uint8_t longhand_count = 0;
};

inline constexpr std::array<shorthand_longhand_entry, k_shorthand_name_count>
    k_shorthand_longhand_table{{
        {"margin",
         shorthand_family::four_sides,
         {gltfx_gfss_property::margin_top, gltfx_gfss_property::margin_right,
          gltfx_gfss_property::margin_bottom, gltfx_gfss_property::margin_left},
         4},
        {"padding",
         shorthand_family::four_sides,
         {gltfx_gfss_property::padding_top, gltfx_gfss_property::padding_right,
          gltfx_gfss_property::padding_bottom, gltfx_gfss_property::padding_left},
         4},
        {"border-width",
         shorthand_family::four_sides,
         {gltfx_gfss_property::border_top_width, gltfx_gfss_property::border_right_width,
          gltfx_gfss_property::border_bottom_width, gltfx_gfss_property::border_left_width},
         4},
        {"border-style",
         shorthand_family::four_sides,
         {gltfx_gfss_property::border_top_style, gltfx_gfss_property::border_right_style,
          gltfx_gfss_property::border_bottom_style, gltfx_gfss_property::border_left_style},
         4},
        {"border-color",
         shorthand_family::four_sides,
         {gltfx_gfss_property::border_top_color, gltfx_gfss_property::border_right_color,
          gltfx_gfss_property::border_bottom_color, gltfx_gfss_property::border_left_color},
         4},
        {"border-radius",
         shorthand_family::four_corners,
         {gltfx_gfss_property::border_top_left_radius, gltfx_gfss_property::border_top_right_radius,
          gltfx_gfss_property::border_bottom_right_radius,
          gltfx_gfss_property::border_bottom_left_radius},
         4},
        {"gap",
         shorthand_family::axis_pair,
         {gltfx_gfss_property::row_gap, gltfx_gfss_property::column_gap},
         2},
        {"overflow",
         shorthand_family::axis_pair,
         {gltfx_gfss_property::overflow_x, gltfx_gfss_property::overflow_y},
         2},
        {"border",
         shorthand_family::border_parts,
         {gltfx_gfss_property::border_top_width, gltfx_gfss_property::border_right_width,
          gltfx_gfss_property::border_bottom_width, gltfx_gfss_property::border_left_width,
          gltfx_gfss_property::border_top_style, gltfx_gfss_property::border_right_style,
          gltfx_gfss_property::border_bottom_style, gltfx_gfss_property::border_left_style,
          gltfx_gfss_property::border_top_color, gltfx_gfss_property::border_right_color,
          gltfx_gfss_property::border_bottom_color, gltfx_gfss_property::border_left_color},
         12},
        {"outline",
         shorthand_family::border_parts,
         {gltfx_gfss_property::outline_width, gltfx_gfss_property::outline_style,
          gltfx_gfss_property::outline_color},
         3},
        {"flex-flow",
         shorthand_family::flex_flow,
         {gltfx_gfss_property::flex_direction, gltfx_gfss_property::flex_wrap},
         2},
    }};

// GODS_LAWS.md L-36, finding #1 of the 16/09/2026 adversarial review
// (wave W6, batch 2): `k_shorthand_longhand_table.size() ==
// k_shorthand_name_count` can NEVER fail - `.size()` of a
// `std::array<T, k_shorthand_name_count>` IS `k_shorthand_name_count`
// BY DECLARATION, no matter how many rows the `{{...}}` initializer
// above actually writes. A twelfth name added to shorthand_name_
// vocabulary.hpp with no matching row here grows this array to twelve
// SILENTLY (the twelfth entry zero-initialized, `name` empty) while
// that dead assert stayed green - the review proved this in practice:
// a clean build, and `find_shorthand_longhand_entry` returning nullptr
// for the new name. The function below walks `k_shorthand_names` (the
// VOCABULARY, never this array) and PROVES each name actually has a row
// here whose `.name` matches - the zero-init twelfth entry matches no
// name at all, so the function returns false and the static_assert
// fails the build. This is the real guard; the old one only ever proved
// a number equal to itself.
[[nodiscard]] constexpr bool every_vocabulary_shorthand_has_a_table_row() {
    for (const std::string_view vocabulary_name : k_shorthand_names) {
        bool found = false;
        for (const shorthand_longhand_entry &entry : k_shorthand_longhand_table) {
            if (entry.name == vocabulary_name) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

static_assert(
    every_vocabulary_shorthand_has_a_table_row(),
    "GODS_LAWS.md L-40: every shorthand_name_vocabulary.hpp entry needs a matching row here (by "
    "NAME, not by array size) - a twelfth shorthand added there without a matching row here must "
    "not compile silently");

// Finds the table row for `shorthand_name` (the sheet spelling GFSS-
// DECL-PARSE's own property_name_lookup.cpp already resolved as one of
// the eleven accepted shorthands) - the static_assert right above PROVES
// this is never nullptr for a caller that only ever passes a
// `gfss_declaration::shorthand_name` value coming from `k_shorthand_
// names` in the first place (declaration_parse.cpp's own build_
// shorthand()), THIS library's own sources being the only place that
// call ever originates today. That proof is about THIS library's own
// closed table, never about an arbitrary `std::string_view` a future
// caller might pass - shorthand_expand.cpp's own expand_shorthand()
// still checks this return for nullptr and returns a diagnosed refusal
// instead of dereferencing it (GODS_LAWS.md LEI ZERO/L-22: no UB crosses
// a function this library exposes past its own translation unit,
// whatever a caller this static_assert cannot see about does).
[[nodiscard]] inline const shorthand_longhand_entry *
find_shorthand_longhand_entry(std::string_view shorthand_name) noexcept {
    for (const shorthand_longhand_entry &entry : k_shorthand_longhand_table) {
        if (entry.name == shorthand_name) {
            return &entry;
        }
    }
    return nullptr;
}

} // namespace glintfx::style::detail
