// SPDX-License-Identifier: AGPL-3.0-or-later
#include "shorthand_border_parts.hpp"

#include <array>
#include <optional>

#include "diagnostic_vocabulary.hpp"
#include "shorthand_component_split.hpp"
#include "shorthand_longhand_apply.hpp"

// shorthand_border_parts.cpp - GFSS-SHORTHAND (GODS_LAWS.md L-17: each
// function below answers exactly one question of shorthand_border_
// parts.hpp's own header comment scope).

namespace glintfx::style::detail {

namespace {

enum class border_part_type : std::uint8_t { width, style, color };

[[nodiscard]] std::string_view border_part_type_name(border_part_type type) noexcept {
    switch (type) {
    case border_part_type::width:
        return "width";
    case border_part_type::style:
        return "style";
    case border_part_type::color:
        return "color";
    }
    return "width";
}

struct border_part_slots {
    std::array<std::optional<declaration_span>, 3> spans{}; // width, style, color
};

// The property whose own contract this family tries FIRST/SECOND/THIRD
// to classify a component - always the type's own first side (`border`)
// or the type's own single longhand (`outline`), the SAME index
// arithmetic expand_border_parts() below uses to explode a matched slot
// back into every side.
[[nodiscard]] gltfx_gfss_property representative_property(const shorthand_longhand_entry &entry,
                                                          border_part_type type,
                                                          std::uint8_t replicate) noexcept {
    return entry.longhands[static_cast<std::size_t>(static_cast<std::uint8_t>(type) * replicate)];
}

} // namespace

shorthand_expand_result expand_border_parts(const std::vector<gltfx_gfss_token> &tokens,
                                            const shorthand_longhand_entry &entry, bool important) {
    const std::uint8_t replicate = static_cast<std::uint8_t>(entry.longhand_count / 3);

    const std::vector<declaration_span> components = split_shorthand_components(tokens);
    if (components.empty() || components.size() > 3) {
        return shorthand_expand_result{.ok = false,
                                       .longhands = {},
                                       .diagnostic = gltfx_gfss_diagnostic{
                                           .line = tokens.empty() ? 0 : tokens.front().line,
                                           .column = tokens.empty() ? 0 : tokens.front().column,
                                           .expected = k_expected_border_part_width_style_or_color,
                                           .detail = longhand_names_detail(entry)}};
    }

    border_part_slots slots;
    for (const declaration_span &component : components) {
        static constexpr std::array<border_part_type, 3> k_types_in_trial_order{
            border_part_type::width, border_part_type::style, border_part_type::color};

        bool matched = false;
        for (const border_part_type type : k_types_in_trial_order) {
            const gltfx_gfss_property candidate = representative_property(entry, type, replicate);
            const longhand_component_outcome outcome = apply_longhand_component(
                tokens, component.begin, component.end, candidate, important);
            if (!outcome.ok) {
                continue;
            }
            std::optional<declaration_span> &slot = slots.spans[static_cast<std::uint8_t>(type)];
            if (slot.has_value()) {
                return shorthand_expand_result{
                    .ok = false,
                    .longhands = {},
                    .diagnostic =
                        gltfx_gfss_diagnostic{.line = tokens[component.begin].line,
                                              .column = tokens[component.begin].column,
                                              .expected = k_expected_each_border_part_at_most_once,
                                              .detail = border_part_type_name(type)}};
            }
            slot = component;
            matched = true;
            break;
        }
        if (!matched) {
            return shorthand_expand_result{
                .ok = false,
                .longhands = {},
                .diagnostic =
                    gltfx_gfss_diagnostic{.line = tokens[component.begin].line,
                                          .column = tokens[component.begin].column,
                                          .expected = k_expected_border_part_width_style_or_color,
                                          .detail = longhand_names_detail(entry)}};
        }
    }

    shorthand_expand_result result;
    result.longhands.reserve(entry.longhand_count);
    for (std::uint8_t i = 0; i < entry.longhand_count; ++i) {
        const std::uint8_t type_index = static_cast<std::uint8_t>(i / replicate);
        const gltfx_gfss_property property = entry.longhands[i];
        const std::optional<declaration_span> &slot = slots.spans[type_index];
        if (!slot.has_value()) {
            result.longhands.push_back(
                longhand_declaration_with_registry_initial(property, important));
            continue;
        }
        const longhand_component_outcome outcome =
            apply_longhand_component(tokens, slot->begin, slot->end, property, important);
        if (!outcome.ok) {
            // Unreachable in practice - every side within one type shares
            // the IDENTICAL contract shape (property_value_contract.hpp's
            // own table), so a span already validated against the type's
            // representative property always validates against every
            // other side of the same type. Kept as a real branch, never
            // asserted away, so a future registry change that broke that
            // identity would surface as THIS diagnostic instead of
            // undefined behavior.
            return shorthand_expand_result{
                .ok = false, .longhands = {}, .diagnostic = outcome.diagnostic};
        }
        result.longhands.push_back(outcome.declaration);
    }
    result.ok = true;
    return result;
}

} // namespace glintfx::style::detail
