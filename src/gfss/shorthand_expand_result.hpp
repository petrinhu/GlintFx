// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <vector>

#include <glintfx/gfss/token.hpp>

#include "declaration_ast.hpp"

// shorthand_expand_result.hpp - GFSS-SHORTHAND (TODO.md wave W6, GODS_
// LAWS.md L-17/L-34): the ONE result shape every family expander
// (shorthand_four_sides.hpp, shorthand_four_corners.hpp, shorthand_
// axis_pair.hpp, shorthand_border_parts.hpp, shorthand_flex_flow.hpp)
// and the dispatcher (shorthand_expand.hpp) return - a header of its
// own, never folded into shorthand_expand.hpp itself, so a family header
// never has to include the dispatcher that calls it.

namespace glintfx::style::detail {

struct shorthand_expand_result {
    bool ok = false;
    std::vector<gfss_declaration> longhands; // valid iff ok, in the family's own output order
    gltfx_gfss_diagnostic diagnostic{};      // valid iff !ok
};

} // namespace glintfx::style::detail
