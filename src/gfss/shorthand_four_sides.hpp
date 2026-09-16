// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <vector>

#include <glintfx/gfss/token.hpp>

#include "shorthand_expand_result.hpp"
#include "shorthand_longhand_table.hpp"

// shorthand_four_sides.hpp - GFSS-SHORTHAND (TODO.md wave W6, GODS_LAWS.
// md L-17/L-20/L-40, docs/plano-w6-folha-de-estilo.md D-W6-3, dossie
// SS3.2): margin, padding, border-width, border-style, border-color -
// the CSS "1 to 4 values, clockwise from the top" shorthand shape. NEVER
// shared with shorthand_four_corners.cpp (border-radius's own distinct
// corner order, TL/TR/BR/BL) - two different distribution rules over
// the SAME "1 to 4 components" arity earn two files, not one with a
// mode flag (GODS_LAWS.md L-17).
//
// THE DISTRIBUTION, FROM THE SPEC (CSS Backgrounds and Borders Level 3's
// own shorthand grammar, the SAME rule every four-sides shorthand this
// registry has shares): 1 value -> all four sides; 2 -> (top+bottom),
// (left+right); 3 -> top, (left+right), bottom; 4 -> top, right, bottom,
// left, in that order - `entry.longhands` is ALREADY in that exact
// order (shorthand_longhand_table.hpp's own header comment), so this
// family only has to pick WHICH component index feeds which longhand
// index, never reorder the table itself.

namespace glintfx::style::detail {

[[nodiscard]] shorthand_expand_result expand_four_sides(const std::vector<gltfx_gfss_token> &tokens,
                                                        const shorthand_longhand_entry &entry,
                                                        bool important);

} // namespace glintfx::style::detail
