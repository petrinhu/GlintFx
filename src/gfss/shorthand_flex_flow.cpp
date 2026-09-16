// SPDX-License-Identifier: AGPL-3.0-or-later
#include "shorthand_flex_flow.hpp"

#include <optional>

#include "diagnostic_vocabulary.hpp"
#include "shorthand_component_split.hpp"
#include "shorthand_longhand_apply.hpp"

// shorthand_flex_flow.cpp - GFSS-SHORTHAND (GODS_LAWS.md L-17: this
// file's own function answers exactly one question of shorthand_flex_
// flow.hpp's own header comment scope).

namespace glintfx::style::detail {

shorthand_expand_result expand_flex_flow(const std::vector<gltfx_gfss_token> &tokens,
                                         const shorthand_longhand_entry &entry, bool important) {
    const std::vector<declaration_span> components = split_shorthand_components(tokens);
    if (components.empty() || components.size() > 2) {
        return shorthand_expand_result{.ok = false,
                                       .longhands = {},
                                       .diagnostic = gltfx_gfss_diagnostic{
                                           .line = tokens.empty() ? 0 : tokens.front().line,
                                           .column = tokens.empty() ? 0 : tokens.front().column,
                                           .expected = k_expected_flex_direction_or_flex_wrap_word,
                                           .detail = longhand_names_detail(entry)}};
    }

    std::optional<gfss_declaration> direction;
    std::optional<gfss_declaration> wrap;

    for (const declaration_span &component : components) {
        const longhand_component_outcome as_direction = apply_longhand_component(
            tokens, component.begin, component.end, entry.longhands[0], important);
        if (as_direction.ok) {
            if (direction.has_value()) {
                return shorthand_expand_result{
                    .ok = false,
                    .longhands = {},
                    .diagnostic = gltfx_gfss_diagnostic{
                        .line = tokens[component.begin].line,
                        .column = tokens[component.begin].column,
                        .expected = k_expected_flex_direction_or_flex_wrap_word,
                        .detail = longhand_names_detail(entry)}};
            }
            direction = as_direction.declaration;
            continue;
        }

        const longhand_component_outcome as_wrap = apply_longhand_component(
            tokens, component.begin, component.end, entry.longhands[1], important);
        if (as_wrap.ok) {
            if (wrap.has_value()) {
                return shorthand_expand_result{
                    .ok = false,
                    .longhands = {},
                    .diagnostic = gltfx_gfss_diagnostic{
                        .line = tokens[component.begin].line,
                        .column = tokens[component.begin].column,
                        .expected = k_expected_flex_direction_or_flex_wrap_word,
                        .detail = longhand_names_detail(entry)}};
            }
            wrap = as_wrap.declaration;
            continue;
        }

        return shorthand_expand_result{.ok = false,
                                       .longhands = {},
                                       .diagnostic = gltfx_gfss_diagnostic{
                                           .line = tokens[component.begin].line,
                                           .column = tokens[component.begin].column,
                                           .expected = k_expected_flex_direction_or_flex_wrap_word,
                                           .detail = longhand_names_detail(entry)}};
    }

    shorthand_expand_result result;
    result.ok = true;
    result.longhands.push_back(direction.has_value() ? *direction
                                                     : longhand_declaration_with_registry_initial(
                                                           entry.longhands[0], important));
    result.longhands.push_back(wrap.has_value() ? *wrap
                                                : longhand_declaration_with_registry_initial(
                                                      entry.longhands[1], important));
    return result;
}

} // namespace glintfx::style::detail
