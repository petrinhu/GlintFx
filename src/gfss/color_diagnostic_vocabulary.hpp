// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <string_view>

// color_diagnostic_vocabulary.hpp - GFSS-COLOR-PARSE, private helper
// (GODS_LAWS.md L-17/L-40, docs/api-conventions.md R7): the SINGLE
// authoritative list of every identifier color_parse.cpp's own
// glintfx::style::gltfx_gfss_diagnostic::expected (token.hpp) can hold
// when the FAILING call was gltfx_gfss_parse_color(), not the
// tokenizer.
//
// A SEPARATE FILE FROM diagnostic_vocabulary.hpp - DELIBERATE, NOT AN
// OVERSIGHT (GODS_LAWS.md L-27, marked INFERENCE: this split is a
// design choice made HERE, not a fact the service order for this
// slice states). diagnostic_vocabulary.hpp's own header comment calls
// itself "the SINGLE authoritative list of every identifier
// gltfx_gfss_diagnostic::expected can EVER hold" - reading that
// literally would mean adding this file's four identifiers there
// instead. Three reasons this file exists on its own instead:
//
//   1. THAT LIST HAS A CLOSED-BY-CONSTRUCTION PROOF THAT NAMES ITS OWN
//      PRODUCER (tests/gfss_tokenizer_test.cpp's own
//      diagnostic_vocabulary_is_enumerated_closed_and_every_identifier_
//      is_produced, GFSS-TOKEN's fatia, already closed and green):
//      `static_assert(k_expected_vocabulary_count == 4, ...)` plus a
//      hand-written "directed-production coverage" block that proves
//      EACH of the four identifiers by feeding malformed gfss TEXT to
//      the TOKENIZER specifically. That test lives outside this
//      fatia's own file list (GFSS-COLOR-PARSE's service order:
//      src/gfss/, tests/gfss_color_parse_test.cpp, tests/
//      CMakeLists.txt - NOT tests/gfss_tokenizer_test.cpp). Appending
//      rows to the shared list would either break that static_assert
//      (a closed, already-verified fatia's own gate, GODS_LAWS.md L-12:
//      "review adversarial... roda mutacao" already ran there once)
//      or require touching a file outside this fatia's scope to keep
//      it green - both worse than a second, equally-closed list.
//   2. THE PRODUCERS ARE GENUINELY DIFFERENT LAYERS. tokenizer.cpp
//      diagnoses SCANNING failures (an unterminated string, a
//      malformed url - CSS Syntax Module Level 3's own grammar, see
//      that file's header comment); color_parse.cpp diagnoses VALUE
//      failures ABOVE the token stream (a hex literal with the wrong
//      digit count, an rgb() with the wrong argument count) - the SAME
//      "who paid this call site" distinction GODS_LAWS.md L-17's own
//      "quem paga a proxima feature" question asks. A single shared
//      list that keeps growing every time a NEW layer starts producing
//      the SAME diagnostic type is the "switch that gains a case per
//      feature" shape L-17's own table of known monolith shapes names.
//   3. gltfx_gfss_diagnostic::expected is `std::string_view expected`,
//      not a closed enum - token.hpp's own header comment already
//      documents that no static_assert freezes this type's shape yet
//      (it congeals only at GFSS-API's dedicated review, TODO.md wave
//      W10). Two vocabularies feeding the SAME open string_view field
//      is not an ABI hazard the way two competing enums would be.
//
// SAME X-MACRO TECHNIQUE diagnostic_vocabulary.hpp AND token.hpp's OWN
// GLINTFX_GFSS_TOKEN_KIND_LIST ALREADY USE, for the SAME reason
// (GODS_LAWS.md L-40 achado 1 of 26/08/2026): one list, the named
// constant AND the enumerable array both generated from it, so they
// cannot drift apart. Private to this header - defined and #undef'd
// immediately below, never leaks past this translation unit's parse of
// this file (the same discipline both sibling files already follow).
//
// snake_case, English, never a sentence (docs/api-conventions.md R7):
// every call site in color_parse.cpp uses the NAMED constant below,
// never a literal string of its own.

namespace glintfx::style::detail {

#define GLINTFX_GFSS_COLOR_EXPECTED_LIST(X)                                                       \
    X(color_value)                                                                                \
    X(hex_digit)                                                                                  \
    X(valid_hex_length)                                                                           \
    X(known_color_keyword)                                                                        \
    X(known_color_function)                                                                       \
    X(shipped_color_notation)                                                                     \
    X(number_or_percentage)                                                                        \
    X(number)                                                                                     \
    X(percentage)                                                                                  \
    X(uniform_component_types)                                                                     \
    X(comma)                                                                                       \
    X(argument_count)                                                                              \
    X(closing_parenthesis)                                                                         \
    X(no_trailing_content)

// One named constexpr std::string_view per entry, spelled from the
// entry's own name via stringizing (#name) so the identifier and its
// string spelling can never drift apart - the same guarantee
// diagnostic_vocabulary.hpp's own k_expected_##name constants give,
// here for a DIFFERENT, private list.
#define GLINTFX_GFSS_COLOR_EXPECTED_CONSTANT(name)                                                 \
    inline constexpr std::string_view k_color_expected_##name{#name};
GLINTFX_GFSS_COLOR_EXPECTED_LIST(GLINTFX_GFSS_COLOR_EXPECTED_CONSTANT)
#undef GLINTFX_GFSS_COLOR_EXPECTED_CONSTANT

// The list's own cardinality, counted mechanically - never a
// hand-copied literal (GODS_LAWS.md L-40 achado 1).
inline constexpr std::size_t k_color_expected_vocabulary_count = [] {
    std::size_t count = 0;
#define GLINTFX_GFSS_COLOR_EXPECTED_COUNT_ONE(name) ++count;
    GLINTFX_GFSS_COLOR_EXPECTED_LIST(GLINTFX_GFSS_COLOR_EXPECTED_COUNT_ONE)
#undef GLINTFX_GFSS_COLOR_EXPECTED_COUNT_ONE
    return count;
}();

// Mechanically-built array for enumeration (GODS_LAWS.md L-40's "the
// space is small, enumerate it whole") - gfss_color_parse_test.cpp
// sweeps this, never a hand-picked subset, and a 15th identifier added
// to the list above appears here with no second edit.
inline constexpr std::array<std::string_view, k_color_expected_vocabulary_count>
    k_color_expected_vocabulary{
#define GLINTFX_GFSS_COLOR_EXPECTED_ARRAY_ONE(name) k_color_expected_##name,
        GLINTFX_GFSS_COLOR_EXPECTED_LIST(GLINTFX_GFSS_COLOR_EXPECTED_ARRAY_ONE)
#undef GLINTFX_GFSS_COLOR_EXPECTED_ARRAY_ONE
};

#undef GLINTFX_GFSS_COLOR_EXPECTED_LIST

} // namespace glintfx::style::detail
