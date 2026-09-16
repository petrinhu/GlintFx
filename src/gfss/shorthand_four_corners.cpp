// SPDX-License-Identifier: AGPL-3.0-or-later
#include "shorthand_four_corners.hpp"

#include "diagnostic_vocabulary.hpp"
#include "shorthand_component_split.hpp"
#include "shorthand_longhand_apply.hpp"

// shorthand_four_corners.cpp - GFSS-SHORTHAND (GODS_LAWS.md L-17: each
// function below answers exactly one question of shorthand_four_
// corners.hpp's own header comment scope).

namespace glintfx::style::detail {

namespace {

// Component index feeding longhand index `corner` (0=TL, 1=TR, 2=BR,
// 3=BL), for a value written with `component_count` components - CSS
// Backgrounds and Borders Level 3's own border-radius distribution,
// spelled in this family's own header comment, made data.
[[nodiscard]] std::size_t component_index_for_corner(std::size_t component_count,
                                                     std::size_t corner) noexcept {
    switch (component_count) {
    case 1:
        return 0;
    case 2:
        return (corner == 0 || corner == 2) ? 0 : 1; // TL/BR=0, TR/BL=1
    case 3:
        if (corner == 0) {
            return 0; // TL
        }
        if (corner == 2) {
            return 2; // BR
        }
        return 1; // TR/BL
    default:      // 4
        return corner;
    }
}

// D-W6-9: a top-level `/` anywhere in the shorthand's own raw value
// means the author wrote the elliptical-radius syntax this v1 registry
// does not represent (one value per corner, never a horizontal/vertical
// pair) - refused with its own diagnostic, checked BEFORE component
// splitting so both "10px / 5px" (three components, the middle one a
// lone `/`) and "10px/5px" (one component containing a `/` token
// between two dimensions) are caught the same way.
[[nodiscard]] const gltfx_gfss_token *
find_top_level_slash(const std::vector<gltfx_gfss_token> &tokens) noexcept {
    for (const gltfx_gfss_token &token : tokens) {
        if (token.kind == gltfx_gfss_token_kind::delim && token.lexeme == "/") {
            return &token;
        }
    }
    return nullptr;
}

} // namespace

shorthand_expand_result expand_four_corners(const std::vector<gltfx_gfss_token> &tokens,
                                            const shorthand_longhand_entry &entry, bool important) {
    if (const gltfx_gfss_token *slash = find_top_level_slash(tokens)) {
        return shorthand_expand_result{
            .ok = false,
            .longhands = {},
            .diagnostic = gltfx_gfss_diagnostic{.line = slash->line,
                                                .column = slash->column,
                                                .expected = k_expected_corner_radius_without_slash,
                                                .detail = longhand_names_detail(entry)}};
    }

    const std::vector<declaration_span> components = split_shorthand_components(tokens);
    if (components.empty() || components.size() > 4) {
        return shorthand_expand_result{.ok = false,
                                       .longhands = {},
                                       .diagnostic = gltfx_gfss_diagnostic{
                                           .line = tokens.empty() ? 0 : tokens.front().line,
                                           .column = tokens.empty() ? 0 : tokens.front().column,
                                           .expected = k_expected_one_to_four_corner_values,
                                           .detail = longhand_names_detail(entry)}};
    }

    shorthand_expand_result result;
    result.longhands.reserve(entry.longhand_count);
    for (std::size_t corner = 0; corner < entry.longhand_count; ++corner) {
        const declaration_span &component =
            components[component_index_for_corner(components.size(), corner)];
        const longhand_component_outcome outcome = apply_longhand_component(
            tokens, component.begin, component.end, entry.longhands[corner], important);
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
