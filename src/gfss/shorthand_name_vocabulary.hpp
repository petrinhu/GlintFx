// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <string_view>

// shorthand_name_vocabulary.hpp - GFSS-DECL-PARSE, DP-4/D-DP-5
// (TODO.md wave W5, GODS_LAWS.md L-17/L-40, docs/gfss-property-
// registry-v1.md SS6 "As onze abreviacoes", plan
// /var/tmp/glintfx-plan/gfss-decl-parse.md SS2 D-DP-5): the CLOSED,
// eleven-name list of shorthand properties this fatia RECOGNIZES and
// accepts CRUDE (form == raw, D-DP-5) - never expands. Answers exactly
// one question - "is this sheet name one of the eleven accepted
// shorthands?" - never what it expands to (GFSS-SHORTHAND's own job,
// TODO.md wave W6) nor which OTHER CSS shorthand exists and is refused
// instead (refused_property_names.hpp's own job).
//
// SAME X-MACRO/MECHANICAL-COUNT DISCIPLINE AS keyword.hpp (GODS_LAWS.md
// L-40 achado 1): the array and its count both derive from ONE list, so
// a twelfth name added here without updating the plan's own D-DP-5
// count fails the test's own static_assert instead of silently
// changing "onze" to "doze".

namespace glintfx::style::detail {

#define GLINTFX_GFSS_SHORTHAND_NAME_LIST(X)                                                       \
    X("margin")                                                                                   \
    X("padding")                                                                                  \
    X("border-width")                                                                             \
    X("border-style")                                                                              \
    X("border-color")                                                                              \
    X("border")                                                                                   \
    X("border-radius")                                                                             \
    X("gap")                                                                                       \
    X("overflow")                                                                                  \
    X("outline")                                                                                   \
    X("flex-flow")

inline constexpr std::size_t k_shorthand_name_count = [] {
    std::size_t count = 0;
#define GLINTFX_GFSS_SHORTHAND_NAME_COUNT_ONE(name) ++count;
    GLINTFX_GFSS_SHORTHAND_NAME_LIST(GLINTFX_GFSS_SHORTHAND_NAME_COUNT_ONE)
#undef GLINTFX_GFSS_SHORTHAND_NAME_COUNT_ONE
    return count;
}();

inline constexpr std::array<std::string_view, k_shorthand_name_count> k_shorthand_names{
#define GLINTFX_GFSS_SHORTHAND_NAME_ARRAY_ONE(name) std::string_view{name},
    GLINTFX_GFSS_SHORTHAND_NAME_LIST(GLINTFX_GFSS_SHORTHAND_NAME_ARRAY_ONE)
#undef GLINTFX_GFSS_SHORTHAND_NAME_ARRAY_ONE
};

#undef GLINTFX_GFSS_SHORTHAND_NAME_LIST

} // namespace glintfx::style::detail
