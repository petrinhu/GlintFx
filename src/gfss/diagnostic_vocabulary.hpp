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
// THREE OF THE FOUR ARE SPEC FACT MADE MACHINE-READABLE, ONE IS OUR OWN
// DEFECT SIGNAL (GODS_LAWS.md L-27, fact vs. inference): "closing_quote",
// "closing_parenthesis" and "escape_sequence" each name a CONDITION the
// CSS Syntax Module Level 3 grammar itself defines (see tokenizer.cpp's
// own header comment for the section-by-section mapping) - a
// machine-readable label for a spec condition is still this project's
// own INFERENCE (the spec never says "give this a token"), just one
// anchored to a real grammar rule. "internal_tokenizer_defect" names
// something the spec has no concept of at all: THIS library failing its
// own internal contract (token_progress_guard.hpp's
// token_made_forward_progress()) - see token_progress_recovery.hpp's
// own header comment for why it is glintfx, never the consumer's file,
// that this identifier blames.
//
// snake_case, English, never a sentence (docs/api-conventions.md R7,
// the SAME convention gltfx_err_fields()/gltfx_gfss_token_kind_name()
// already use project-wide): every call site in tokenizer.cpp and
// token_progress_recovery.hpp uses the NAMED constant below, never a
// literal string of its own - the constant IS the spelling, checked
// once, here.

namespace glintfx::style::detail {

#define GLINTFX_GFSS_DIAGNOSTIC_EXPECTED_LIST(X)                                                   \
    X(closing_quote)                                                                               \
    X(closing_parenthesis)                                                                         \
    X(escape_sequence)                                                                             \
    X(internal_tokenizer_defect)

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
