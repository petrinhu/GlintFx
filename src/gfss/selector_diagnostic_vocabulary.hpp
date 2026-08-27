// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <string_view>

// selector_diagnostic_vocabulary.hpp - GFSS-SEL-PARSE-CORE, private
// helper (GODS_LAWS.md L-17/L-40, docs/api-conventions.md R7): the
// SINGLE authoritative list of every identifier selector_parse.cpp's
// own glintfx::style::gltfx_gfss_diagnostic::expected (token.hpp) can
// hold when the FAILING call was parse_selector_list(), not the
// tokenizer nor GFSS-COLOR-PARSE.
//
// THIS IS THE THIRD SEPARATE DIAGNOSTIC VOCABULARY IN THIS TRACK, AND
// THAT IS KNOWN AND DELIBERATE (GODS_LAWS.md L-27, marked INFERENCE -
// this split is a design choice made HERE, following the SAME
// precedent color_diagnostic_vocabulary.hpp's own header comment
// already set, not a fact this fatia's service order states). The
// project already has diagnostic_vocabulary.hpp (GFSS-TOKEN's own
// scanning-layer list, 4 identifiers) and color_diagnostic_vocabulary.
// hpp (GFSS-COLOR-PARSE's own value-layer list, 14 identifiers) - this
// file is a THIRD list for a THIRD producer, not a second copy of an
// existing one. The SAME three reasons color_diagnostic_vocabulary.hpp
// gives for splitting off from diagnostic_vocabulary.hpp apply here,
// word for word, against BOTH existing lists:
//
//   1. EACH EXISTING LIST HAS A CLOSED-BY-CONSTRUCTION PROOF THAT NAMES
//      ITS OWN PRODUCER. tests/gfss_tokenizer_test.cpp's own
//      diagnostic_vocabulary_is_enumerated_closed_and_every_identifier_
//      is_produced static_asserts k_expected_vocabulary_count == 4 and
//      feeds malformed gfss TEXT to the TOKENIZER specifically;
//      tests/gfss_color_parse_test.cpp's own analogous case
//      static_asserts k_color_expected_vocabulary_count == 14 against
//      GFSS-COLOR-PARSE specifically. Both fatias are already closed
//      and green (GODS_LAWS.md L-12: review adversarial already ran on
//      each). Appending selector-layer rows to either list would
//      either break that static_assert or require touching a file
//      outside THIS fatia's own scope (src/gfss/, tests/gfss_selector_
//      parse_test.cpp, tests/CMakeLists.txt) to keep it green - both
//      worse than a third, equally-closed list.
//   2. THE PRODUCERS ARE GENUINELY DIFFERENT LAYERS. tokenizer.cpp
//      diagnoses SCANNING failures (CSS Syntax Module Level 3's own
//      grammar); color_parse.cpp diagnoses COLOR VALUE failures ABOVE
//      the token stream; selector_parse.cpp diagnoses SELECTOR GRAMMAR
//      failures ABOVE the SAME token stream but answering a completely
//      different question (is this a well-formed tag/.class/#id/*,
//      compound, combinator chain, or pseudo-class - never "is this a
//      well-formed color") - the SAME "quem paga a proxima feature"
//      distinction GODS_LAWS.md L-17 asks. A single shared list that
//      keeps growing every time a NEW layer starts producing
//      diagnostics is the "switch that gains a case per feature" shape
//      L-17's own table of known monolith shapes names.
//   3. gltfx_gfss_diagnostic::expected IS AN OPEN std::string_view, NOT
//      A CLOSED ENUM (token.hpp's own header comment: no static_assert
//      freezes this type's shape yet - it congeals only at GFSS-API's
//      dedicated review, TODO.md wave W10). Three vocabularies feeding
//      the SAME open string_view field is not an ABI hazard the way
//      three competing enums would be.
//
// SAME X-MACRO TECHNIQUE THE OTHER TWO VOCABULARIES ALREADY USE, for
// the SAME reason (GODS_LAWS.md L-40 achado 1 of 26/08/2026): one
// list, the named constant AND the enumerable array both generated
// from it, so they cannot drift apart. Private to this header - defined
// and #undef'd immediately below.
//
// A STRING VALUE CAN REPEAT ACROSS THE THREE VOCABULARIES ON PURPOSE:
// k_selector_expected_closing_parenthesis below spells the SAME
// "closing_parenthesis" string diagnostic_vocabulary.hpp's own
// k_expected_closing_parenthesis and color_diagnostic_vocabulary.hpp's
// own k_color_expected_closing_parenthesis already spell - all three
// name the SAME concept (an opening delimiter with no matching close)
// from three different producer layers, which a consumer's own
// message-building code is free to treat identically. The vocabularies
// are separate to avoid COUPLING three unrelated producers to one
// growing enum, not to invent a new word for an old idea.
//
// snake_case, English, never a sentence (docs/api-conventions.md R7,
// the SAME convention every other vocabulary in this project already
// uses): every call site in selector_parse.cpp uses the NAMED constant
// below, never a literal string of its own.

namespace glintfx::style::detail {

#define GLINTFX_GFSS_SELECTOR_EXPECTED_LIST(X)                                                     \
    X(simple_selector)                                                                             \
    X(identifier_after_dot)                                                                        \
    X(identifier_after_colon)                                                                      \
    X(known_pseudo_class)                                                                          \
    X(known_pseudo_function)                                                                       \
    X(closing_parenthesis)                                                                         \
    X(comma_or_end_of_selector_list)

// One named constexpr std::string_view per entry, spelled from the
// entry's own name via stringizing (#name) so the identifier and its
// string spelling can never drift apart - the same guarantee the
// other two vocabularies' own named constants already give.
#define GLINTFX_GFSS_SELECTOR_EXPECTED_CONSTANT(name)                                              \
    inline constexpr std::string_view k_selector_expected_##name{#name};
GLINTFX_GFSS_SELECTOR_EXPECTED_LIST(GLINTFX_GFSS_SELECTOR_EXPECTED_CONSTANT)
#undef GLINTFX_GFSS_SELECTOR_EXPECTED_CONSTANT

// The list's own cardinality, counted mechanically - never a
// hand-copied literal (GODS_LAWS.md L-40 achado 1).
inline constexpr std::size_t k_selector_expected_vocabulary_count = [] {
    std::size_t count = 0;
#define GLINTFX_GFSS_SELECTOR_EXPECTED_COUNT_ONE(name) ++count;
    GLINTFX_GFSS_SELECTOR_EXPECTED_LIST(GLINTFX_GFSS_SELECTOR_EXPECTED_COUNT_ONE)
#undef GLINTFX_GFSS_SELECTOR_EXPECTED_COUNT_ONE
    return count;
}();

// Mechanically-built array for enumeration (GODS_LAWS.md L-40's "the
// space is small, enumerate it whole") - gfss_selector_parse_test.cpp
// sweeps this, never a hand-picked subset, and an 8th identifier added
// to the list above appears here with no second edit.
inline constexpr std::array<std::string_view, k_selector_expected_vocabulary_count>
    k_selector_expected_vocabulary{
#define GLINTFX_GFSS_SELECTOR_EXPECTED_ARRAY_ONE(name) k_selector_expected_##name,
        GLINTFX_GFSS_SELECTOR_EXPECTED_LIST(GLINTFX_GFSS_SELECTOR_EXPECTED_ARRAY_ONE)
#undef GLINTFX_GFSS_SELECTOR_EXPECTED_ARRAY_ONE
    };

#undef GLINTFX_GFSS_SELECTOR_EXPECTED_LIST

} // namespace glintfx::style::detail
