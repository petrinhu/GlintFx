// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <vector>

#include <glintfx/gfss/token.hpp>
#include <glintfx/gfss/value.hpp>

#include "property_value_contract.hpp"

// declaration_value_check.hpp - GFSS-DECL-PARSE, DP-6 (TODO.md wave
// W5, GODS_LAWS.md L-17/L-20/L-27/L-40, plan
// /var/tmp/glintfx-plan/gfss-decl-parse.md SS3): answers exactly one
// question - "does this span of value tokens satisfy `contract`, and if
// so, what decoded values does it hold?" - never resolves color
// (declaration_color_value.hpp's own job) and never touches a
// raw_composite or universal-keyword contract (declaration_parse.cpp's
// own job routes those away before this file is ever called).
//
// A DECLARED SIMPLIFICATION FOR COMMA-SEPARATED LISTS (GODS_LAWS.md
// L-27, marked INFERENCE - property_value_contract.hpp's own header
// comment names the same gap for `<time>#`): this file splits on
// TOP-LEVEL commas when `contract.comma_separated` is set, and decodes
// each comma-separated group as ONE value component - it does not
// cross-reference the list's own length against another property's
// value (e.g. `transition-property`'s own count), which is outside this
// registry's declared scope (docs/gfss-property-registry-v1.md SS4's
// own "o que NAO congela").

namespace glintfx::style::detail {

struct declaration_value_check_result {
    bool ok = false;
    std::vector<gltfx_gfss_value> values;
    gltfx_gfss_diagnostic diagnostic{};
};

// Checks `tokens[begin, end)` against `contract`. See this file's own
// header comment for what this function does NOT handle (color, raw,
// universal keywords - all routed elsewhere before this is called). NOT
// noexcept: the result holds a std::vector<gltfx_gfss_value> built with
// push_back (the same reason declaration_split.hpp's own split
// function is not either).
[[nodiscard]] declaration_value_check_result
check_declaration_value(const std::vector<gltfx_gfss_token> &tokens, std::size_t begin, std::size_t end,
                        const property_value_contract &contract);

} // namespace glintfx::style::detail
