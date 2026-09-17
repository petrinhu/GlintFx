// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

#include <glintfx/gfss/property.hpp>
#include <glintfx/gfss/token.hpp>

#include "declaration_ast.hpp"
#include "shorthand_longhand_table.hpp"

// shorthand_longhand_apply.hpp - GFSS-SHORTHAND, D-W6-3/D-W6-4 (TODO.md
// wave W6, GODS_LAWS.md L-17/L-33/L-40): the TWO atoms every family
// (shorthand_four_sides.cpp, shorthand_four_corners.cpp, shorthand_
// axis_pair.cpp, shorthand_border_parts.cpp, shorthand_flex_flow.cpp)
// needs and none of them owns alone - CONTRACT.md SS6.7's "regra de
// tres" triggered by the FIVE families sharing both, not the two real
// occurrences (declaration_parse.cpp's own apply_contract_value(),
// unchanged by this fatia) that justified leaving the SAME dispatch
// duplicated there (this file's own .cpp explains why it is not the
// SAME function, only the same shape).
//
// (1) "does this component span satisfy ONE longhand's own contract,
// and what gfss_declaration does it become?" - the SAME color/values
// dispatch build_known_property()'s own apply_contract_value()
// (declaration_parse.cpp) already does for a directly-authored
// declaration, reused here so a longhand written via a shorthand and one
// written directly produce the IDENTICAL representation (D-W6-3's own
// consistency requirement - GFSS-CASCADE, W10, must never be able to
// tell the two apart).
//
// (2) "what does this longhand become when the shorthand OMITS it?"
// (D-W6-4, the project leader's decision: the registry's own initial,
// never `is_shorthand`/absent) - see this file's own .cpp for why a
// color-typed longhand's initial (always a keyword, property_table.hpp's
// own rows) is resolved through the REAL color grammar instead of
// copied raw.

namespace glintfx::style::detail {

struct longhand_component_outcome {
    bool ok = false;
    gfss_declaration declaration{};     // valid iff ok
    gltfx_gfss_diagnostic diagnostic{}; // valid iff !ok
};

// `tokens[begin, end)` is one component's own span (shorthand_component_
// split.hpp's own declaration_span shape) or a whole family-decided
// sub-span (shorthand_four_sides.cpp's own per-side span, etc.) - never
// re-trimmed here, the SAME contract check_declaration_value() and
// read_declaration_color_value() already hold for their own callers.
[[nodiscard]] longhand_component_outcome
apply_longhand_component(const std::vector<gltfx_gfss_token> &tokens, std::size_t begin,
                         std::size_t end, gltfx_gfss_property property, bool important);

// The `property`'s own gfss_declaration when the shorthand's own value
// never named it - always succeeds (the registry's own initial is never
// sheet-authored text, it cannot fail the SAME grammar an author's own
// spelling of it would pass).
[[nodiscard]] gfss_declaration
longhand_declaration_with_registry_initial(gltfx_gfss_property property, bool important);

// A stable, space-separated (R7) list of `entry`'s own longhands' sheet
// names (property_table.hpp's own spelling, e.g. "margin-top margin-
// right margin-bottom margin-left") - the `detail` every family's own
// arity-violation diagnostic (one_to_four_side_values, one_to_four_
// corner_values, one_or_two_axis_values, flex_direction_or_flex_wrap_
// word) names. `entry` MUST be a reference into k_shorthand_longhand_
// table (shorthand_longhand_table.hpp's own find_shorthand_longhand_
// entry() is the only legitimate source) - the cache below is indexed by
// pointer arithmetic against that ONE array, never a second lookup.
[[nodiscard]] std::string_view longhand_names_detail(const shorthand_longhand_entry &entry);

} // namespace glintfx::style::detail
