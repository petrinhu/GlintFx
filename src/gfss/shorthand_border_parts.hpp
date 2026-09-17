// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <vector>

#include <glintfx/gfss/token.hpp>

#include "shorthand_expand_result.hpp"
#include "shorthand_longhand_table.hpp"

// shorthand_border_parts.hpp - GFSS-SHORTHAND (TODO.md wave W6, GODS_
// LAWS.md L-17/L-20/L-40, docs/plano-w6-folha-de-estilo.md D-W6-3):
// border (12 longhands - 4 sides x {width, style, color}) and outline (3
// longhands - {width, style, color}, no per-side variant) - the SAME
// "1 to 3 components, each classified by WHAT it is, free order, each
// type at most once" shape.
//
// A SINGLE VALUE SETS EVERY SIDE THE SAME WAY (never per-side, unlike
// shorthand_four_sides.cpp's own border-width/border-style/border-color
// shorthands): `border: 2px solid red` sets ALL FOUR sides' own width to
// 2px, ALL FOUR sides' own style to solid, ALL FOUR sides' own color to
// red - never one side alone. The REPLICATE FACTOR (`entry.longhand_
// count / 3` - 4 for `border`, 1 for `outline`) is what turns THREE
// classified components into `entry.longhand_count` final declarations:
// `shorthand_longhand_table.hpp`'s own row comment names why `border`'s
// 12 longhands are grouped width-then-style-then-color, never
// interleaved by side - this arithmetic depends on that exact grouping.
//
// CLASSIFICATION IS BY TRIAL, NEVER BY TOKEN SHAPE ALONE: a component is
// "width" if it satisfies the family's own REPRESENTATIVE width
// longhand's contract (`entry.longhands[0]`), "style" if it satisfies
// the representative style longhand's (`entry.longhands[replicate]`),
// "color" if it satisfies the representative color longhand's
// (`entry.longhands[2 * replicate]`) - width/style/color/none are
// checked in that order, safe because this registry's own closed
// vocabulary (`docs/gfss-property-registry-v1.md` SS6) never lets a
// single token satisfy two of the three (a length/thin/medium/thick
// word is never `none`/`solid`, and neither is ever a real color name).

namespace glintfx::style::detail {

[[nodiscard]] shorthand_expand_result
expand_border_parts(const std::vector<gltfx_gfss_token> &tokens,
                    const shorthand_longhand_entry &entry, bool important);

} // namespace glintfx::style::detail
