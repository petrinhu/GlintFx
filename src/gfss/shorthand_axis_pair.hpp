// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <vector>

#include <glintfx/gfss/token.hpp>

#include "shorthand_expand_result.hpp"
#include "shorthand_longhand_table.hpp"

// shorthand_axis_pair.hpp - GFSS-SHORTHAND (TODO.md wave W6, GODS_LAWS.
// md L-17/L-20/L-40, docs/plano-w6-folha-de-estilo.md D-W6-3): gap
// (row-gap, column-gap) and overflow (overflow-x, overflow-y) - the
// SAME "one value for both axes, or one value per axis" shape, shared
// by these two and no other of the eleven shorthands. `entry.longhands`
// is `{first-axis, second-axis}` (row before column, x before y -
// shorthand_longhand_table.hpp's own row order).

namespace glintfx::style::detail {

[[nodiscard]] shorthand_expand_result expand_axis_pair(const std::vector<gltfx_gfss_token> &tokens,
                                                       const shorthand_longhand_entry &entry,
                                                       bool important);

} // namespace glintfx::style::detail
