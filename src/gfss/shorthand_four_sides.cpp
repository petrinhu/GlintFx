// SPDX-License-Identifier: AGPL-3.0-or-later
#include "shorthand_four_sides.hpp"

#include "diagnostic_vocabulary.hpp"
#include "shorthand_component_split.hpp"
#include "shorthand_longhand_apply.hpp"

// shorthand_four_sides.cpp - GFSS-SHORTHAND (GODS_LAWS.md L-17: this
// file's own function answers exactly one question of shorthand_four_
// sides.hpp's own header comment scope).

namespace glintfx::style::detail {

namespace {

// Component index feeding longhand index `side` (0=top, 1=right,
// 2=bottom, 3=left), for a value written with `component_count`
// components - the spec table this file's own header comment spells in
// prose, made data.
[[nodiscard]] std::size_t component_index_for_side(std::size_t component_count,
                                                   std::size_t side) noexcept {
    switch (component_count) {
    case 1:
        return 0;
    case 2:
        return (side == 0 || side == 2) ? 0 : 1; // top/bottom=0, right/left=1
    case 3:
        if (side == 0) {
            return 0; // top
        }
        if (side == 2) {
            return 2; // bottom
        }
        return 1; // right/left
    default:      // 4
        return side;
    }
}

} // namespace

shorthand_expand_result expand_four_sides(const std::vector<gltfx_gfss_token> &tokens,
                                          const shorthand_longhand_entry &entry, bool important) {
    const std::vector<declaration_span> components = split_shorthand_components(tokens);
    if (components.empty() || components.size() > 4) {
        return shorthand_expand_result{.ok = false,
                                       .longhands = {},
                                       .diagnostic = gltfx_gfss_diagnostic{
                                           .line = tokens.empty() ? 0 : tokens.front().line,
                                           .column = tokens.empty() ? 0 : tokens.front().column,
                                           .expected = k_expected_one_to_four_side_values,
                                           .detail = longhand_names_detail(entry)}};
    }

    shorthand_expand_result result;
    result.longhands.reserve(entry.longhand_count);
    for (std::size_t side = 0; side < entry.longhand_count; ++side) {
        const declaration_span &component =
            components[component_index_for_side(components.size(), side)];
        const longhand_component_outcome outcome = apply_longhand_component(
            tokens, component.begin, component.end, entry.longhands[side], important);
        if (!outcome.ok) {
            return shorthand_expand_result{
                .ok = false, .longhands = {}, .diagnostic = outcome.diagnostic};
        }
        result.longhands.push_back(outcome.declaration);
    }
    result.ok = true;
    return result;
}

} // namespace glintfx::style::detail
