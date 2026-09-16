// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <vector>

#include <glintfx/gfss/token.hpp>

#include "declaration_split.hpp"

// shorthand_component_split.hpp - GFSS-SHORTHAND (TODO.md wave W6,
// GODS_LAWS.md L-17/L-20/L-33/L-40, docs/plano-w6-folha-de-estilo.md
// D-W6-3): answers exactly one question - "where does each SPACE-
// SEPARATED component of a shorthand's own raw value begin and end?" -
// never whether a component is a VALID value for any particular longhand
// (each family's own job, shorthand_four_sides.cpp and friends).
//
// REUSES declaration_span (declaration_split.hpp's own `[begin, end)`
// type), never a new one of its own: a "component of a shorthand value"
// and "a declaration inside a block" are the SAME shape - a token span
// bounded by a top-level delimiter - only the delimiter differs
// (whitespace here, `;` there). CONTRACT.md SS6.7's own "regra de tres"
// is not yet triggered (this is only the SECOND real occurrence of the
// nesting-depth-aware split algorithm itself, declaration_split.cpp's
// own loop being the first) - the loop body stays its own, small,
// duplicated function (shorthand_component_split.cpp's own header
// comment says why), but the SPAN TYPE it produces is shared, since
// nothing about `{begin, end}` is specific to either delimiter.
//
// WHY WHITESPACE, NEVER COMMA: none of the eleven shorthands this track
// accepts (shorthand_name_vocabulary.hpp) use a comma-separated grammar
// - `border`/`outline`'s own up-to-three parts, `flex-flow`'s own
// up-to-two words, and every four-sides/four-corners/axis-pair family
// are ALL space-separated (docs/gfss-property-registry-v1.md SS6). A
// future shorthand with a comma-separated component (none exists in the
// v1 registry) would need its own split, not a mode flag bolted onto
// this one.

namespace glintfx::style::detail {

// Splits `tokens` (a shorthand's own `raw_tokens`, ALREADY trimmed of
// leading/trailing whitespace by declaration_parse.cpp's own
// build_shorthand() - never re-trimmed here) into one span per
// top-level, whitespace-separated component. A construct that can hide
// whitespace of its own (a parenthesized/bracketed/braced span, or a
// <function-token>'s own argument list, e.g. `rgb( 1 , 2 , 3 )`) stays
// ONE component - nesting_depth_delta() (nesting_depth.hpp) is the SAME
// depth-tracking atom declaration_split.cpp's own top-level-`;` search
// and declaration_value_check.cpp's own top-level-`,` search already
// use, extended here to a THIRD delimiter (whitespace) instead of a
// fourth copy of the whole loop with only the trigger condition changed.
[[nodiscard]] std::vector<declaration_span>
split_shorthand_components(const std::vector<gltfx_gfss_token> &tokens);

} // namespace glintfx::style::detail
