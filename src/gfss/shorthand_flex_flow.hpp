// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <vector>

#include <glintfx/gfss/token.hpp>

#include "shorthand_expand_result.hpp"
#include "shorthand_longhand_table.hpp"

// shorthand_flex_flow.hpp - GFSS-SHORTHAND (TODO.md wave W6, GODS_LAWS.
// md L-17/L-20/L-40, docs/plano-w6-folha-de-estilo.md D-W6-3): flex-flow
// - flex-direction (4 words, `entry.longhands[0]`) and flex-wrap (3
// words, `entry.longhands[1]`), 1 or 2 components, FREE ORDER, disjoint
// keyword sets (CSS Flexible Box Layout Module Level 1's own
// flex-flow shorthand grammar: `<flex-direction> || <flex-wrap>`). A
// component is classified by WHICH of the two longhands' own contract it
// satisfies - the SAME free-order-by-trial technique shorthand_border_
// parts.cpp uses for its own three types, with only two candidates here
// instead of three and no per-side replication.

namespace glintfx::style::detail {

[[nodiscard]] shorthand_expand_result expand_flex_flow(const std::vector<gltfx_gfss_token> &tokens,
                                                       const shorthand_longhand_entry &entry,
                                                       bool important);

} // namespace glintfx::style::detail
