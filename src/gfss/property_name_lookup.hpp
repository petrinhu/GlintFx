// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <string_view>

#include <glintfx/gfss/property.hpp>
#include <glintfx/gfss/token.hpp>

// property_name_lookup.hpp - GFSS-DECL-PARSE, DP-4 (TODO.md wave W5,
// GODS_LAWS.md L-17/L-20/L-27/L-40, plan
// /var/tmp/glintfx-plan/gfss-decl-parse.md SS2 D-DP-5): answers exactly
// one question - "what does this sheet spelling of a property name
// resolve to?" - a real registry property, an accepted shorthand
// (crude, D-DP-5), a recognized-and-refused name (with the `detail` to
// write instead), or none of the three (an honest typo).

namespace glintfx::style::detail {

enum class property_name_lookup_kind : std::uint8_t {
    known_property, // resolves to a k_property_table row - `property` is valid
    shorthand,      // one of the eleven accepted shorthand names - `shorthand_name` is valid
    refused,        // a recognized CSS name this registry refuses - `diagnostic.detail` names the
                    // longhands
    unknown,        // not a property, not a shorthand, not a refused name at all
};

struct property_name_lookup_result {
    property_name_lookup_kind kind = property_name_lookup_kind::unknown;
    gltfx_gfss_property property = gltfx_gfss_property::display; // valid iff kind == known_property
    std::string_view shorthand_name{};                           // valid iff kind == shorthand
    gltfx_gfss_diagnostic diagnostic{}; // valid iff kind == refused or unknown
};

// Resolves `name` (a sheet spelling, e.g. from an <ident-token>'s own
// lexeme) ASCII case-insensitively (CSS 2.1 SS4.1.3: "all CSS syntax is
// case-insensitive within the ASCII range") against the 104-property
// registry, the eleven accepted shorthands, and the ten refused names,
// in that order. `position`/`column` supply the diagnostic's own
// location for the `refused`/`unknown` cases - the caller's own name
// token, since this function never sees a gltfx_gfss_token directly
// (only its lexeme).
[[nodiscard]] property_name_lookup_result
lookup_property_name(std::string_view name, std::uint32_t line, std::uint32_t column) noexcept;

} // namespace glintfx::style::detail
