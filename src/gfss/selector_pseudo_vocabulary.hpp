// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <string_view>

#include "named_colors.hpp"

// selector_pseudo_vocabulary.hpp - GFSS-SEL-PARSE-CORE, private helper
// (GODS_LAWS.md L-17/L-27/L-40, ESCOPO.md SS4 decision 3 "seletor
// completo na v1"): the closed lists of pseudo-class NAMES this
// fatia's own service order enumerates - the 14 argument-less ones
// (GLINTFX_GFSS_SIMPLE_PSEUDO_LIST) and the 5 this fatia recognizes BY
// NAME without analyzing their argument
// (GLINTFX_GFSS_FUNCTIONAL_PSEUDO_LIST) - answers exactly one
// question, "is this name a KNOWN pseudo-class of its own kind",
// distinct from selector_ast.hpp (what the MATCHED shape looks like)
// and selector_parse.cpp (WHEN this lookup applies to a token stream).
//
// SAME X-MACRO TECHNIQUE token.hpp/diagnostic_vocabulary.hpp/
// color_diagnostic_vocabulary.hpp ALREADY USE, for the SAME reason
// (GODS_LAWS.md L-40 achado 1 of 26/08/2026): the count gfss_selector_
// parse_test.cpp claims to sweep is DERIVED from these two lists, not
// a hand-copied literal - a 15th simple pseudo-class or 6th functional
// one added here with no matching test row makes that test's own
// static_assert fail to COMPILE, never silently pass 15/14 or 6/5.
// Private to this header - defined and #undef'd immediately below.
//
// EACH LIST TAKES ONE STRING LITERAL PER ENTRY, NOT AN (identifier,
// string) PAIR THE WAY gfss_combinator_table (selector_ast.hpp) DOES:
// most of these 19 names contain a hyphen ("focus-visible",
// "nth-last-of-type"), which cannot spell a C++ identifier - and
// nothing here needs a per-entry NAMED constant the way
// color_diagnostic_vocabulary.hpp's own k_color_expected_##name
// constants do (each of THOSE is referenced by name at an individual
// call site in color_parse.cpp; a pseudo-class name is only ever
// checked by MEMBERSHIP in the whole list, is_known_simple_pseudo()/
// is_known_functional_pseudo() below, never by its own identifier) -
// so the simpler, single-string-literal-per-entry form is the correct
// one here, not a shortcut.
//
// THE LIST IS A FACT, READ FROM THE SERVICE ORDER THAT OPENED THIS
// FATIA (GODS_LAWS.md L-27): TODO.md's own GFSS-SEL-PARSE-CORE row
// names all 14 simple pseudo-classes and the 5 functional ones by name
// (cross-referenced by GFSS-SEL-PARSE-NTH/GFSS-SEL-PARSE-NOT's own
// rows, which consume the raw argument this fatia only guards) - this
// file is that enumeration made machine-readable, not a fresh reading
// of the CSS Selectors Level 4 spec.
//
// ASCII CASE-INSENSITIVE, SAME RULE named_colors.hpp's OWN
// ascii_case_insensitive_equal() ALREADY ESTABLISHES for THIS track's
// keyword matching (CSS Syntax Module Level 3's own <ident-token>
// match rule, the same one gfss's own function-name matching already
// relies on) - is_known_simple_pseudo()/is_known_functional_pseudo()
// below reuse that SAME function: a THIRD real call site, past
// CONTRACT.md SS6's "three occurrences" bar for a shared helper (the
// first two are named_colors.cpp's own table lookup and
// color_parse.cpp's function-name match).

namespace glintfx::style::detail {

#define GLINTFX_GFSS_SIMPLE_PSEUDO_LIST(X)                                                         \
    X("hover")                                                                                     \
    X("active")                                                                                    \
    X("focus")                                                                                     \
    X("focus-visible")                                                                             \
    X("checked")                                                                                   \
    X("first-child")                                                                               \
    X("last-child")                                                                                \
    X("only-child")                                                                                \
    X("first-of-type")                                                                             \
    X("last-of-type")                                                                              \
    X("only-of-type")                                                                              \
    X("empty")                                                                                     \
    X("placeholder-shown")                                                                         \
    X("scope")

// "not" itself cannot name a C++ identifier in this file's macro
// arguments the way GLINTFX_GFSS_COMBINATOR_LIST's entries do
// (selector_ast.hpp) - it is a C++ KEYWORD (an alternative token for
// `!`, [lex.key]), not merely a reserved library name - which is
// exactly why this list (unlike that one) takes a bare string literal
// per entry and never an identifier at all.
#define GLINTFX_GFSS_FUNCTIONAL_PSEUDO_LIST(X)                                                     \
    X("nth-child")                                                                                 \
    X("nth-last-child")                                                                            \
    X("nth-of-type")                                                                               \
    X("nth-last-of-type")                                                                          \
    X("not")

inline constexpr std::size_t k_simple_pseudo_count = [] {
    std::size_t count = 0;
#define GLINTFX_GFSS_SIMPLE_PSEUDO_COUNT_ONE(text) ++count;
    GLINTFX_GFSS_SIMPLE_PSEUDO_LIST(GLINTFX_GFSS_SIMPLE_PSEUDO_COUNT_ONE)
#undef GLINTFX_GFSS_SIMPLE_PSEUDO_COUNT_ONE
    return count;
}();

inline constexpr std::array<std::string_view, k_simple_pseudo_count> k_simple_pseudo_names{
#define GLINTFX_GFSS_SIMPLE_PSEUDO_ARRAY_ONE(text) std::string_view{text},
    GLINTFX_GFSS_SIMPLE_PSEUDO_LIST(GLINTFX_GFSS_SIMPLE_PSEUDO_ARRAY_ONE)
#undef GLINTFX_GFSS_SIMPLE_PSEUDO_ARRAY_ONE
};

#undef GLINTFX_GFSS_SIMPLE_PSEUDO_LIST

inline constexpr std::size_t k_functional_pseudo_count = [] {
    std::size_t count = 0;
#define GLINTFX_GFSS_FUNCTIONAL_PSEUDO_COUNT_ONE(text) ++count;
    GLINTFX_GFSS_FUNCTIONAL_PSEUDO_LIST(GLINTFX_GFSS_FUNCTIONAL_PSEUDO_COUNT_ONE)
#undef GLINTFX_GFSS_FUNCTIONAL_PSEUDO_COUNT_ONE
    return count;
}();

inline constexpr std::array<std::string_view, k_functional_pseudo_count> k_functional_pseudo_names{
#define GLINTFX_GFSS_FUNCTIONAL_PSEUDO_ARRAY_ONE(text) std::string_view{text},
    GLINTFX_GFSS_FUNCTIONAL_PSEUDO_LIST(GLINTFX_GFSS_FUNCTIONAL_PSEUDO_ARRAY_ONE)
#undef GLINTFX_GFSS_FUNCTIONAL_PSEUDO_ARRAY_ONE
};

#undef GLINTFX_GFSS_FUNCTIONAL_PSEUDO_LIST

[[nodiscard]] constexpr bool is_known_simple_pseudo(std::string_view name) noexcept {
    for (const auto &candidate : k_simple_pseudo_names) {
        if (ascii_case_insensitive_equal(name, candidate)) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] constexpr bool is_known_functional_pseudo(std::string_view name) noexcept {
    for (const auto &candidate : k_functional_pseudo_names) {
        if (ascii_case_insensitive_equal(name, candidate)) {
            return true;
        }
    }
    return false;
}

} // namespace glintfx::style::detail
