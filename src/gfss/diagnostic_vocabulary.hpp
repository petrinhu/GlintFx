// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <string_view>

// diagnostic_vocabulary.hpp - GFSS-TOKEN, private helper (GODS_LAWS.md
// L-17/L-40, docs/api-conventions.md R7): the SINGLE authoritative list
// of every identifier gltfx_gfss_diagnostic::expected (token.hpp) can
// ever hold - answers exactly one question, "what is the closed set of
// tokens a diagnostic can name?", distinct from every file that
// PRODUCES a diagnostic (tokenizer.cpp, token_progress_recovery.hpp):
// this file only names the vocabulary, it never decides when one
// applies.
//
// SAME X-MACRO TECHNIQUE token.hpp's OWN GLINTFX_GFSS_TOKEN_KIND_LIST
// ALREADY USES, for the SAME reason (GODS_LAWS.md L-40 achado 1 of
// 26/08/2026, "a enumeracao fechada nao e fechada"): before this file
// existed, "closing_quote", "closing_parenthesis" and "escape_sequence"
// were THREE independently-typed string literals scattered across
// tokenizer.cpp, with nothing to stop a fourth call site from spelling
// one of them wrong, or a test from claiming to enumerate "the
// vocabulary" against a hand-copied count that could drift.
// GLINTFX_GFSS_DIAGNOSTIC_EXPECTED_LIST(X) below is the ONE list; every
// named constant AND the enumerable array further down are generated
// from it, so they cannot drift apart. Private to this header - defined
// and #undef'd immediately below, never leaks past this translation
// unit's parse of this file (the same discipline token.hpp's own list
// macro follows).
//
// THREE OF THE FIRST FOUR ARE SPEC FACT MADE MACHINE-READABLE, ONE IS
// OUR OWN DEFECT SIGNAL (GODS_LAWS.md L-27, fact vs. inference):
// "closing_quote", "closing_parenthesis" and "escape_sequence" each
// name a CONDITION the CSS Syntax Module Level 3 grammar itself
// defines (see tokenizer.cpp's own header comment for the
// section-by-section mapping) - a machine-readable label for a spec
// condition is still this project's own INFERENCE (the spec never
// says "give this a token"), just one anchored to a real grammar rule.
// "internal_tokenizer_defect" names something the spec has no concept
// of at all: THIS library failing its own internal contract
// (token_progress_guard.hpp's token_made_forward_progress()) - see
// token_progress_recovery.hpp's own header comment for why it is
// glintfx, never the consumer's file, that this identifier blames.
//
// ONE LIST FOR THE WHOLE gfss TRACK, DECIDED BY THE PROJECT LEADER ON
// 27/08/2026 - NOT PER PRODUCER LAYER (GODS_LAWS.md L-27, this
// decision reached the implementer through the orchestrator, not
// read from a spec or inferred here): this file used to be named, and
// justified, as "GFSS-TOKEN's own" list, with the expectation that a
// later layer (a value parser above the token stream, e.g. a color or
// a selector parser) would open ITS OWN separate list, mirroring the
// per-producer split GODS_LAWS.md L-17's own monolith table warns
// against for a switch, applied here to vocabulary files instead. A
// real, measured finding overturned that plan: the adversarial review
// of the sibling GFSS-COLOR-PARSE fatia found that its own separate
// list (color_diagnostic_vocabulary.hpp) and this one had ALREADY,
// independently, chosen the identical spelling "closing_parenthesis"
// for two DIFFERENT conditions (an unterminated CSS url()/string
// token here, versus an unterminated rgb()/hsl() function call
// there) - and NOTHING detected it, because each list only proved
// itself closed against ITSELF. Per-producer splitting was solving
// the wrong problem: it does prevent one list's own static_assert
// from being reopened by an unrelated fatia, but it does nothing
// against the SAME two-letter identifier meaning two different things
// project-wide - which is a real hazard for a consumer building a
// message catalog keyed by this string. The leader's decision: one
// list for the track, GLINTFX_GFSS_SIMPLE_PSEUDO/GLINTFX_GFSS_
// FUNCTIONAL_PSEUDO-adjacent additions included, so that ONE
// enumeration proves closure AND absence of collision at once - see
// tests/gfss_selector_parse_test.cpp's own diagnostic_vocabulary_has_
// no_duplicate_word test case, which sweeps k_expected_vocabulary
// below pairwise for exactly this. GFSS-SEL-PARSE-CORE (TODO.md) is
// the first fatia to add rows here under this policy; GFSS-COLOR-
// PARSE's own separate list is UNCHANGED by this file - its
// consolidation, if any, is that fatia's own author's work, not this
// one's (its review is reproved on other grounds and returns to them).
//
// GFSS-VALUE (TODO.md) IS THE SECOND FATIA TO ADD ROWS HERE UNDER THE
// 27/08/2026 POLICY ABOVE (component_value, known_length_unit) -
// value_parse.cpp is the producer. THE KNOWN COLLISION THIS POLICY
// EXISTS TO PREVENT (this file's own paragraph above) IS STILL OPEN,
// NOT RESOLVED BY THIS ADDITION (GODS_LAWS.md L-28's own order of
// service for GFSS-VALUE, explicitly: "esta fatia NAO a resolve... se
// voce tocar o vocabulario de diagnostico, registre que a colisao
// segue aberta"): color_diagnostic_vocabulary.hpp's own
// "closing_parenthesis" (an unterminated rgb()/hsl() call) still
// duplicates THIS file's own "closing_parenthesis" (an unterminated
// CSS url()/string token) under two different spellings-that-are-the-
// same-word, for two different conditions - GFSS-COLOR-PARSE's own
// consolidation into this shared list, if it ever happens, is that
// fatia's own author's work, unchanged by this one.
//
// snake_case, English, never a sentence (docs/api-conventions.md R7,
// the SAME convention gltfx_err_fields()/gltfx_gfss_token_kind_name()
// already use project-wide): every call site across this track's .cpp
// files uses the NAMED constant below, never a literal string of its
// own - the constant IS the spelling, checked once, here.

namespace glintfx::style::detail {

#define GLINTFX_GFSS_DIAGNOSTIC_EXPECTED_LIST(X)                                                   \
    X(closing_quote)                                                                               \
    X(closing_parenthesis)                                                                         \
    X(escape_sequence)                                                                             \
    X(internal_tokenizer_defect)                                                                   \
    X(simple_selector)                                                                             \
    X(identifier_after_dot)                                                                        \
    X(identifier_after_colon)                                                                      \
    X(known_pseudo_class)                                                                          \
    X(known_pseudo_function)                                                                       \
    X(comma_or_end_of_selector_list)                                                               \
    X(component_value)                                                                             \
    X(known_length_unit)

// One named constexpr std::string_view per entry, spelled from the
// entry's own name via stringizing (#name) so the identifier and its
// string spelling can never drift apart - the same guarantee
// err_code.cpp's table gives gltfx_err_code_name() by hand, here made
// structural instead.
#define GLINTFX_GFSS_DIAGNOSTIC_EXPECTED_CONSTANT(name)                                            \
    inline constexpr std::string_view k_expected_##name{#name};
GLINTFX_GFSS_DIAGNOSTIC_EXPECTED_LIST(GLINTFX_GFSS_DIAGNOSTIC_EXPECTED_CONSTANT)
#undef GLINTFX_GFSS_DIAGNOSTIC_EXPECTED_CONSTANT

// The list's own cardinality, counted mechanically - never a
// hand-copied literal (GODS_LAWS.md L-40 achado 1, the same technique
// token.hpp's own gltfx_gfss_token_kind_count already uses).
inline constexpr std::size_t k_expected_vocabulary_count = [] {
    std::size_t count = 0;
#define GLINTFX_GFSS_DIAGNOSTIC_EXPECTED_COUNT_ONE(name) ++count;
    GLINTFX_GFSS_DIAGNOSTIC_EXPECTED_LIST(GLINTFX_GFSS_DIAGNOSTIC_EXPECTED_COUNT_ONE)
#undef GLINTFX_GFSS_DIAGNOSTIC_EXPECTED_COUNT_ONE
    return count;
}();

// Mechanically-built array for enumeration (GODS_LAWS.md L-40's "the
// space is small, enumerate it whole") - a gate or a test sweeps this,
// never a hand-picked subset, and a fifth identifier added to the list
// above appears here with no second edit.
inline constexpr std::array<std::string_view, k_expected_vocabulary_count> k_expected_vocabulary{
#define GLINTFX_GFSS_DIAGNOSTIC_EXPECTED_ARRAY_ONE(name) k_expected_##name,
    GLINTFX_GFSS_DIAGNOSTIC_EXPECTED_LIST(GLINTFX_GFSS_DIAGNOSTIC_EXPECTED_ARRAY_ONE)
#undef GLINTFX_GFSS_DIAGNOSTIC_EXPECTED_ARRAY_ONE
};

#undef GLINTFX_GFSS_DIAGNOSTIC_EXPECTED_LIST

} // namespace glintfx::style::detail
