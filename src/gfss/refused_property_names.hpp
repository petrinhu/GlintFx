// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <string_view>

// refused_property_names.hpp - GFSS-DECL-PARSE, DP-4/D-DP-5 (TODO.md
// wave W5, GODS_LAWS.md L-17/L-40, docs/api-conventions.md R7, plan
// /var/tmp/glintfx-plan/gfss-decl-parse.md SS2 D-DP-5, TODO.md's own
// E3 amendment of 05/09/2026): the CLOSED, ten-name list of sheet names
// this fatia RECOGNIZES as real CSS but REFUSES - each with the
// `detail` (token.hpp's own R4/R7 field, D-DP-3) naming what to write
// instead, so a declaration this project's own scope cut is never
// "typo silence" (E1's own guiding complaint, `docs/gfss-property-
// registry-v1.md` and the stylelint/ValidateHTML dor-da-comunidade
// finding the plan's own SS1.2 cites).
//
// NINE SHORTHANDS E3 NAMES BY NAME, PLUS `white-space` (A5) - TEN
// TOTAL: the nine are `transition`, `animation`, `background`,
// `border-image`, `place-content`, `place-items`, `place-self`
// (E3-refused shorthands), plus `flex` and `inset` (project leader's
// own order of 26/08/2026, cited verbatim in the plan's own D-DP-5).
// `white-space` is not a shorthand at all - it is CSS's OWN name for
// what this registry calls `text-wrap-mode` (docs/gfss-property-
// registry-v1.md SS7's own A5: "sem marcacao, texto e literal" - the
// registry only kept the wrapping half of CSS's own `white-space`,
// under its Level 4 replacement name) - it earns a row here on the
// SAME "recognized-and-refused, never silent" policy, its own `detail`
// naming the one longhand this registry actually has.
//
// A GAP THIS FILE DECLARES RATHER THAN HIDES (GODS_LAWS.md L-27): CSS's
// real `place-items`/`place-self` expand to `align-items`+`justify-
// items` and `align-self`+`justify-self` respectively - this registry
// (docs/gfss-property-registry-v1.md SS6) has NO `justify-items`/
// `justify-self` row at all (only `justify-content` exists, and only as
// its own property, id 31). Naming a longhand that this library does
// not itself recognize would send a consumer to fix an error by writing
// ANOTHER error. `detail` for these two rows names only the ONE
// longhand this registry actually ships (`align-items`/`align-self`) -
// an intentional narrowing, flagged here for the plan's own author
// to confirm or grow the registry, never silently "completed" by
// inventing a property name.

namespace glintfx::style::detail {

struct refused_property_name_entry {
    std::string_view name;
    std::string_view detail; // space-separated identifiers (R7) - never a sentence
};

inline constexpr std::array<refused_property_name_entry, 10> k_refused_property_names{{
    {"transition",
     "transition-property transition-duration transition-timing-function transition-delay"},
    {"animation", "animation-name animation-duration animation-timing-function animation-delay "
                  "animation-iteration-count animation-direction animation-fill-mode "
                  "animation-play-state"},
    {"background", "background-color background-image background-repeat background-size "
                   "background-position"},
    {"border-image", "border-image-source border-image-slice border-image-width "
                     "border-image-outset border-image-repeat"},
    {"place-content", "align-content justify-content"},
    // See this header's own top comment "A GAP THIS FILE DECLARES..." -
    // justify-items does not exist in this registry.
    {"place-items", "align-items"},
    // Same gap, for justify-self.
    {"place-self", "align-self"},
    {"flex", "flex-grow flex-shrink flex-basis"},
    {"inset", "top right bottom left"},
    {"white-space", "text-wrap-mode"},
}};

} // namespace glintfx::style::detail
