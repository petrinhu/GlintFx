// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
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
// 27/08/2026 POLICY ABOVE (component_value, known_dimension_unit) -
// value_parse.cpp is the producer. known_dimension_unit was named
// known_length_unit until GFSS-VALUE-2 (28/08/2026): back then
// value_parse.cpp's own dimension-token branch only ever tried ONE
// closed unit family (length), so naming the diagnostic after that
// family was accurate. GFSS-VALUE-2 made the SAME branch try THREE
// closed families in turn (length, then angle, then time - value.hpp's
// own achado-6 comment on why they stay separate enums) - a dimension
// whose unit text matches NONE of the three no longer implies the
// author meant "length" specifically, so keeping the old, narrower
// name would have been a diagnostic that CLAIMS to know more than the
// parser actually determined (GODS_LAWS.md L-27). Renamed, not left
// pointing at a stale name: nothing outside this track's own three
// producer files (value_parse.hpp/.cpp, gfss_value_test.cpp) ever
// referenced the old spelling (grepped before the rename), and this
// pre-1.0 window has no external consumer to break (ESCOPO.md SS3's
// own "SOVERSION 0, nada de estabilidade prometida"). THE KNOWN
// COLLISION THIS POLICY EXISTS TO PREVENT (this file's own paragraph
// above) IS STILL OPEN, NOT RESOLVED BY THIS ADDITION (GODS_LAWS.md
// L-28's own order of service for GFSS-VALUE, explicitly: "esta fatia
// NAO a resolve... se voce tocar o vocabulario de diagnostico,
// registre que a colisao segue aberta"): color_diagnostic_
// vocabulary.hpp's own
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
//
// GFSS-VOCAB-BIND (TODO.md, GODS_LAWS.md L-40, achado IMPORTANTE-baixo
// of the 27/08/2026 re-review): k_expected_vocabulary_count above was
// already mechanical - counted from this SAME list, never a hand-copied
// literal - but nothing tied that count to "every identifier has a
// producer that actually EMITS it". A real experiment proved the gap:
// adding a 5th identifier here and bumping a TEST's own hand-written
// literal (a static_assert of the form "count == 12") compiled clean
// and printed a green "swept" count, with no production anywhere for
// the new one - the test only proved FORMAT (snake_case, non-empty),
// never that a producer's own code path ever attaches it to a real
// gltfx_gfss_diagnostic. EACH ENTRY NOW NAMES THE PRODUCER THAT OWNS IT
// (gfss_diagnostic_producer, the SAME split the project leader's
// 27/08/2026 "one list for the track" decision already documents in
// prose above - now load-bearing, not just narrated): tokenizer.cpp
// (this track's own GFSS-TOKEN layer), selector_parse.cpp
// (GFSS-SEL-PARSE-CORE) and value_parse.cpp (GFSS-VALUE).
// count_owned_by() below is counted from THIS list, the same technique
// gltfx_gfss_token_kind_count (token.hpp) already uses for the token
// vocabulary itself - each producer's own test file (gfss_tokenizer_
// test.cpp, gfss_selector_parse_test.cpp, gfss_value_test.cpp) ties its
// own directed-production table's size to count_owned_by(that
// producer) via static_assert, so an identifier added here under an
// EXISTING producer with no matching production row in that producer's
// own test file now FAILS TO COMPILE instead of passing a stale count.

namespace glintfx::style::detail {

// Which producer file EMITS a diagnostic carrying this identifier - see
// this header's own GFSS-VOCAB-BIND paragraph above for why this exists
// and gfss_diagnostic_entry/k_expected_entries/count_owned_by() below
// for how a producer's own test proves it against this tag.
enum class gfss_diagnostic_producer : std::uint8_t {
    tokenizer,      // src/gfss/tokenizer.cpp (GFSS-TOKEN)
    selector_parse, // src/gfss/selector_parse.cpp (GFSS-SEL-PARSE-CORE)
    value_parse,    // src/gfss/value_parse.cpp (GFSS-VALUE)
};

#define GLINTFX_GFSS_DIAGNOSTIC_EXPECTED_LIST(X)                                                   \
    X(closing_quote, tokenizer)                                                                    \
    X(closing_parenthesis, tokenizer)                                                              \
    X(escape_sequence, tokenizer)                                                                  \
    X(internal_tokenizer_defect, tokenizer)                                                        \
    X(closing_comment, tokenizer)                                                                  \
    X(simple_selector, selector_parse)                                                             \
    X(identifier_after_dot, selector_parse)                                                        \
    X(identifier_after_colon, selector_parse)                                                      \
    X(known_pseudo_class, selector_parse)                                                          \
    X(known_pseudo_function, selector_parse)                                                       \
    X(comma_or_end_of_selector_list, selector_parse)                                               \
    X(component_value, value_parse)                                                                \
    X(known_dimension_unit, value_parse)

// One named constexpr std::string_view per entry, spelled from the
// entry's own name via stringizing (#name) so the identifier and its
// string spelling can never drift apart - the same guarantee
// err_code.cpp's table gives gltfx_err_code_name() by hand, here made
// structural instead. The producer tag is unused BY THIS macro - it
// only spells the constant; gfss_diagnostic_entry/count_owned_by()
// below are what read the producer.
#define GLINTFX_GFSS_DIAGNOSTIC_EXPECTED_CONSTANT(name, producer)                                  \
    inline constexpr std::string_view k_expected_##name{#name};
GLINTFX_GFSS_DIAGNOSTIC_EXPECTED_LIST(GLINTFX_GFSS_DIAGNOSTIC_EXPECTED_CONSTANT)
#undef GLINTFX_GFSS_DIAGNOSTIC_EXPECTED_CONSTANT

// The list's own cardinality, counted mechanically - never a
// hand-copied literal (GODS_LAWS.md L-40 achado 1, the same technique
// token.hpp's own gltfx_gfss_token_kind_count already uses).
inline constexpr std::size_t k_expected_vocabulary_count = [] {
    std::size_t count = 0;
#define GLINTFX_GFSS_DIAGNOSTIC_EXPECTED_COUNT_ONE(name, producer) ++count;
    GLINTFX_GFSS_DIAGNOSTIC_EXPECTED_LIST(GLINTFX_GFSS_DIAGNOSTIC_EXPECTED_COUNT_ONE)
#undef GLINTFX_GFSS_DIAGNOSTIC_EXPECTED_COUNT_ONE
    return count;
}();

// Mechanically-built array for enumeration (GODS_LAWS.md L-40's "the
// space is small, enumerate it whole") - a gate or a test sweeps this,
// never a hand-picked subset, and a fifth identifier added to the list
// above appears here with no second edit.
inline constexpr std::array<std::string_view, k_expected_vocabulary_count> k_expected_vocabulary{
#define GLINTFX_GFSS_DIAGNOSTIC_EXPECTED_ARRAY_ONE(name, producer) k_expected_##name,
    GLINTFX_GFSS_DIAGNOSTIC_EXPECTED_LIST(GLINTFX_GFSS_DIAGNOSTIC_EXPECTED_ARRAY_ONE)
#undef GLINTFX_GFSS_DIAGNOSTIC_EXPECTED_ARRAY_ONE
};

// One identifier/producer PAIR per list entry - the structural link
// count_owned_by() below walks. Mechanically built the SAME way as
// k_expected_vocabulary above (same X-macro pass, same source list), so
// a row can never exist in one array and not the other.
struct gfss_diagnostic_entry {
    // Both members carry a default initializer so a default-constructed
    // entry is never indeterminate. Every row of k_expected_entries
    // below sets both explicitly, so this default is unreachable there -
    // it exists so that the type itself cannot produce a garbage
    // producer value if anyone ever default-constructs one.
    std::string_view identifier{};
    gfss_diagnostic_producer producer{};
};

inline constexpr std::array<gfss_diagnostic_entry, k_expected_vocabulary_count> k_expected_entries{
#define GLINTFX_GFSS_DIAGNOSTIC_EXPECTED_ENTRY_ONE(name, producer)                                 \
    gfss_diagnostic_entry{k_expected_##name, gfss_diagnostic_producer::producer},
    GLINTFX_GFSS_DIAGNOSTIC_EXPECTED_LIST(GLINTFX_GFSS_DIAGNOSTIC_EXPECTED_ENTRY_ONE)
#undef GLINTFX_GFSS_DIAGNOSTIC_EXPECTED_ENTRY_ONE
};

#undef GLINTFX_GFSS_DIAGNOSTIC_EXPECTED_LIST

// How many identifiers this list assigns to `producer` - counted
// MECHANICALLY from k_expected_entries above, never a hand-copied
// literal (this header's own GFSS-VOCAB-BIND paragraph). Each
// producer's own test file static_asserts its directed-production
// table's size against this call, so a new identifier added under an
// EXISTING producer with no matching production row now fails to
// compile in THAT producer's own test binary, instead of silently
// passing a stale hand-written count.
[[nodiscard]] constexpr std::size_t count_owned_by(gfss_diagnostic_producer producer) noexcept {
    std::size_t count = 0;
    for (const gfss_diagnostic_entry &entry : k_expected_entries) {
        if (entry.producer == producer) {
            ++count;
        }
    }
    return count;
}

} // namespace glintfx::style::detail
