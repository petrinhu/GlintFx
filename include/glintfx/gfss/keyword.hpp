// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <glintfx/export.hpp>

// keyword.hpp - GFSS-PROP-REGISTRY (TODO.md, GODS_LAWS.md L-17/L-19/
// L-20/L-21/L-26/L-27/L-40, docs/gfss-property-registry-v1.md SS4/E2):
// the closed enumeration of every SIMPLE value word a gfss property's
// own accepted grammar names as a bare alternative (`display: block |
// flex | none`, `position: static | relative | absolute`, and so on) -
// [PMU], same append-only freeze discipline as property.hpp's own
// gltfx_gfss_property (SS4: "o valor numerico do enumerador e o id
// publico ... e e append-only").
//
// WHAT IS DELIBERATELY NOT HERE (docs/gfss-property-registry-v1.md
// SS4's own "o que NAO congela", read together with its E3/D3/D8
// paragraphs): a color name (`black`, `transparent`), a font alias
// (`sans-serif`), an easing name (`ease`), an animation/`@keyframes`
// name, or any other identifier from an OPEN or separately-owned
// vocabulary is carried as raw text in a gltfx_gfss_value's own
// `keyword_text` field (value.hpp's own "keyword carries raw text, no
// special-casing" rule) - it is NEVER a member of this enum. This
// enum only closes the SIMPLE, single-token alternatives the property
// registry's own SS6 table spells out with `|` between backticks
// (plus the handful of bare single-word alternatives mixed into a
// longer grammar cell, e.g. `border-image-repeat`'s own `stretch`).
// The composite grammars SS4 itself declares out of scope
// (<shadow>, <transform-list>, <easing>, <filter-function-list>,
// <image>, <position>) keep their own open vocabulary outside this
// file, decided by the fatia that reads them.
//
// E2 - THE RESERVED-WORD SUFFIX IS A RULE, NOT A LIST OF TWO (docs/
// gfss-property-registry-v1.md SS1, E2, CTO decision, 05/09/2026):
// "o identificador em codigo de um valor da folha e a propria palavra
// em snake_case ... quando a palavra e reservada em C++ (ou e token
// alternativo), o identificador leva o sufixo `_keyword`". The GENERAL
// rule this file's own sibling test (gfss_property_registry_test.cpp,
// case keyword_reserved_word_suffix_rule_is_general_not_a_hardcoded_
// pair_of_two) enumerates the FULL closed C++23 reserved-word list
// (keywords AND alternative tokens - "and"/"or"/"not"/"xor"/"bitand"/
// "bitor"/"compl"/"and_eq"/"or_eq"/"xor_eq"/"not_eq") against every
// enumerator below - the two members actually needing the suffix
// today (`auto_keyword`, folha spelling "auto"; `static_keyword`,
// folha spelling "static") are a MEASURED RESULT of applying the rule
// to this v1's own 56-word vocabulary, never a hardcoded pair a future
// word could silently miss. gltfx_gfss_keyword_name() below always
// returns the FOLHA spelling, without the suffix - the suffix exists
// only to keep the C++ identifier itself legal, never to change what
// the author of a gfss sheet writes.
//
// SAME X-MACRO / MECHANICAL-COUNT DISCIPLINE AS value.hpp (GODS_LAWS.
// md L-40 achado 1): the enum and its count both derive from ONE list
// below, so a word added to it without a matching name-table row in
// keyword_kind.cpp fails to COMPILE (that file's own static_assert),
// never silently returns "unknown" at runtime for a real member.

namespace glintfx::style {

// GLINTFX_GFSS_KEYWORD_LIST(X) - the closed set of 56 SIMPLE value
// words v1 recognizes (docs/gfss-property-registry-v1.md SS6, every
// backtick-delimited `|`-alternative cell, mechanically extracted and
// de-duplicated - see this file's own header comment for what is
// deliberately excluded). Alphabetical, NOT SS6 row order: unlike
// property.hpp's own gltfx_gfss_property, a keyword's numeric value
// carries no append-only contract of its own (nothing in the registry
// exposes a keyword's numeric id publicly) - alphabetical is simply
// the most reviewable order for a flat word list this size.
#define GLINTFX_GFSS_KEYWORD_LIST(X)                                                               \
    X(absolute)                                                                                    \
    X(all)                                                                                         \
    X(alternate)                                                                                   \
    X(alternate_reverse)                                                                           \
    X(auto_keyword)                                                                                \
    X(backwards)                                                                                   \
    X(baseline)                                                                                    \
    X(block)                                                                                       \
    X(border_box)                                                                                  \
    X(both)                                                                                        \
    X(center)                                                                                      \
    X(clip)                                                                                        \
    X(column)                                                                                      \
    X(column_reverse)                                                                              \
    X(contain)                                                                                     \
    X(content_box)                                                                                 \
    X(cover)                                                                                       \
    X(ellipsis)                                                                                    \
    X(fill)                                                                                        \
    X(flex)                                                                                        \
    X(flex_end)                                                                                    \
    X(flex_start)                                                                                  \
    X(forwards)                                                                                    \
    X(hidden)                                                                                      \
    X(infinite)                                                                                    \
    X(left)                                                                                        \
    X(medium)                                                                                      \
    X(multiply)                                                                                    \
    X(no_repeat)                                                                                   \
    X(none)                                                                                        \
    X(normal)                                                                                      \
    X(nowrap)                                                                                      \
    X(paused)                                                                                      \
    X(pixelated)                                                                                   \
    X(plus_lighter)                                                                                \
    X(relative)                                                                                    \
    X(repeat)                                                                                      \
    X(repeat_x)                                                                                    \
    X(repeat_y)                                                                                    \
    X(reverse)                                                                                     \
    X(right)                                                                                       \
    X(row)                                                                                         \
    X(row_reverse)                                                                                 \
    X(running)                                                                                     \
    X(screen)                                                                                      \
    X(solid)                                                                                       \
    X(space_around)                                                                                \
    X(space_between)                                                                               \
    X(space_evenly)                                                                                \
    X(static_keyword)                                                                              \
    X(stretch)                                                                                     \
    X(thick)                                                                                       \
    X(thin)                                                                                        \
    X(visible)                                                                                     \
    X(wrap)                                                                                        \
    X(wrap_reverse)

// std::uint16_t is docs/gfss-property-registry-v1.md's own SS4
// verbatim base type ("enum class gltfx_gfss_keyword, base
// std::uint16_t") - NOT the smallest type that fits today's 56
// members (clang-tidy's own performance-enum-size would suggest
// std::uint8_t, the base value.hpp's own smaller closed enums use).
// Left wider because the CTO's own ratified spec names it explicitly,
// the same way property.hpp's own gltfx_gfss_property is - an
// implementer narrowing it to satisfy a lint warning would be
// reopening a decision that is not this fatia's to reopen (GODS_
// LAWS.md's own "lei das leis").
enum class gltfx_gfss_keyword : std::uint16_t { // NOLINT(performance-enum-size) reason: see the
                                                // paragraph above
#define GLINTFX_GFSS_KEYWORD_ENUMERATOR(name) name,
    GLINTFX_GFSS_KEYWORD_LIST(GLINTFX_GFSS_KEYWORD_ENUMERATOR)
#undef GLINTFX_GFSS_KEYWORD_ENUMERATOR
};

// Mechanically counted from GLINTFX_GFSS_KEYWORD_LIST above - never a
// hand-copied literal (same reasoning as value.hpp's own gltfx_gfss_
// value_kind_count).
inline constexpr std::size_t gltfx_gfss_keyword_count = [] {
    std::size_t count = 0;
#define GLINTFX_GFSS_KEYWORD_COUNT_ONE(name) ++count;
    GLINTFX_GFSS_KEYWORD_LIST(GLINTFX_GFSS_KEYWORD_COUNT_ONE)
#undef GLINTFX_GFSS_KEYWORD_COUNT_ONE
    return count;
}();

#undef GLINTFX_GFSS_KEYWORD_LIST

// Returns the FOLHA spelling of `keyword` (e.g. "auto" for
// gltfx_gfss_keyword::auto_keyword, "space-between" for ::space_
// between) - never the C++ identifier, and never a sentence (docs/
// api-conventions.md R7). Defined in keyword_kind.cpp, same table/
// static_assert technique as value.hpp's own gltfx_gfss_value_kind_
// name(). noexcept, never undefined behavior: a `keyword` outside the
// table returns "unknown" (docs/api-conventions.md R4).
[[nodiscard]] GLINTFX_API std::string_view
gltfx_gfss_keyword_name(gltfx_gfss_keyword keyword) noexcept;

} // namespace glintfx::style
