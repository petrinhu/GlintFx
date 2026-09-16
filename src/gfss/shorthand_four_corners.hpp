// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <vector>

#include <glintfx/gfss/token.hpp>

#include "shorthand_expand_result.hpp"
#include "shorthand_longhand_table.hpp"

// shorthand_four_corners.hpp - GFSS-SHORTHAND (TODO.md wave W6, GODS_
// LAWS.md L-17/L-20/L-40, docs/plano-w6-folha-de-estilo.md D-W6-3/D-W6-9,
// dossie SS3.2): border-radius - ONE shorthand, its own family, NEVER
// sharing code with shorthand_four_sides.cpp. Two things make border-
// radius its own family, not a side of that one: (1) its own corner
// order is TL, TR, BR, BL (clockwise from top-left) - NOT the "top,
// right, bottom, left" order the other four-sides shorthands use; (2)
// this v1 registry gives every corner exactly ONE value (docs/gfss-
// property-registry-v1.md SS6) - elliptical radii (`border-radius: 10px
// / 5px`) are NOT representable, so a top-level `/` is refused with its
// own diagnostic (`corner_radius_without_slash`, D-W6-9) instead of
// falling through to a generic arity failure.

namespace glintfx::style::detail {

[[nodiscard]] shorthand_expand_result
expand_four_corners(const std::vector<gltfx_gfss_token> &tokens,
                    const shorthand_longhand_entry &entry, bool important);

} // namespace glintfx::style::detail
