// SPDX-License-Identifier: AGPL-3.0-or-later
#include "shorthand_axis_pair.hpp"

#include "diagnostic_vocabulary.hpp"
#include "shorthand_component_split.hpp"
#include "shorthand_longhand_apply.hpp"

// shorthand_axis_pair.cpp - GFSS-SHORTHAND (GODS_LAWS.md L-17: this
// file's own function answers exactly one question of shorthand_axis_
// pair.hpp's own header comment scope).

namespace glintfx::style::detail {

shorthand_expand_result expand_axis_pair(const std::vector<gltfx_gfss_token> &tokens,
                                         const shorthand_longhand_entry &entry, bool important) {
    const std::vector<declaration_span> components = split_shorthand_components(tokens);
    if (components.empty() || components.size() > 2) {
        return shorthand_expand_result{.ok = false,
                                       .longhands = {},
                                       .diagnostic = gltfx_gfss_diagnostic{
                                           .line = tokens.empty() ? 0 : tokens.front().line,
                                           .column = tokens.empty() ? 0 : tokens.front().column,
                                           .expected = k_expected_one_or_two_axis_values,
                                           .detail = longhand_names_detail(entry)}};
    }

    shorthand_expand_result result;
    result.longhands.reserve(entry.longhand_count);
    for (std::size_t axis = 0; axis < entry.longhand_count; ++axis) {
        const std::size_t component_index = (components.size() == 1) ? 0 : axis;
        const declaration_span &component = components[component_index];
        const longhand_component_outcome outcome = apply_longhand_component(
            tokens, component.begin, component.end, entry.longhands[axis], important);
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
