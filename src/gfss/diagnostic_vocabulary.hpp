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
// GFSS-SEL-PARSE-PSEUDO-ELEMENT AND GFSS-SEL-PARSE-NTH (TODO.md,
// 05/09/2026) ARE THE THIRD AND FOURTH FATIAS TO ADD ROWS HERE, BOTH
// UNDER THE SAME 27/08/2026 POLICY: identifier_after_double_colon and
// known_pseudo_element are selector_parse.cpp's OWN two new diagnostics
// (that file's own parse_pseudo_element(), GODS_LAWS.md L-28 decision
// 11) - same producer tag as GFSS-SEL-PARSE-CORE's own six, since both
// live in the SAME producer file. anb_expression/anb_offset/end_of_
// anb_expression are anb_parse.cpp's OWN three - a NEW producer file,
// hence the NEW gfss_diagnostic_producer::anb_parse tag below (this
// microparser is explicitly a STANDALONE utility, not yet wired into
// selector_parse.cpp's own AST - anb_parse.hpp's own header comment
// names why - so its diagnostics could not honestly claim the
// selector_parse producer tag; a tag names WHICH FILE emits the
// diagnostic, never which fatia's service order asked for it).
//
// GFSS-SEL-PARSE-ATTR (TODO.md, 05/09/2026) IS THE FIFTH FATIA TO ADD
// ROWS HERE, UNDER THE SAME 27/08/2026 POLICY: attribute_name,
// attribute_operator_or_close, attribute_value and closing_square_
// bracket are selector_parse.cpp's OWN four new diagnostics (that
// file's own parse_attribute_selector()) - same producer tag as GFSS-
// SEL-PARSE-CORE/GFSS-SEL-PARSE-PSEUDO-ELEMENT's own rows, since all
// three live in the SAME producer file. closing_quote (tokenizer-owned,
// already in this list) is REUSED for the "aspas nao fechadas" case,
// never re-spelled under selector_parse - the SAME reuse this fatia's
// own producer already applies to closing_parenthesis for an unclosed
// functional-pseudo argument.
//
// GFSS-SEL-PARSE-NOT (TODO.md, 05/09/2026) IS THE SIXTH FATIA TO ADD A
// ROW HERE, UNDER THE SAME 27/08/2026 POLICY: not_recursion_limit is
// selector_parse.cpp's OWN diagnostic for its own attach_not_argument()
// (same producer tag as every other row this file already assigns to
// that file) - the ONLY identifier in this whole list that does not name
// a spec-defined grammar condition at all (this file's own "THREE OF
// THE FIRST FOUR..." paragraph above already draws that same line for
// internal_tokenizer_defect): CSS Selectors Level 4 places no limit of
// its own on how deep `:not()` may nest, so a leaf that nests it past
// this LIBRARY's own bound is not violating the FORMAT's grammar - it is
// hitting THIS project's own anti-DoS policy (GODS_LAWS.md LEI ZERO,
// "base de consumidores aberta e desconhecida"), the same self-imposed
// category internal_tokenizer_defect already belongs to.
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

// GFSS-DECL-PARSE (TODO.md wave W5, plan
// /var/tmp/glintfx-plan/gfss-decl-parse.md SS4.1) IS THE SEVENTH FATIA
// TO ADD ROWS HERE, UNDER THE SAME 27/08/2026 POLICY: the twelve
// identifiers below are declaration_parse.cpp's OWN (a NEW producer
// file, hence the new gfss_diagnostic_producer::declaration_parse tag).
// `component_value` (value_parse) and `known_dimension_unit`
// (value_parse) are REUSED, never re-spelled under this new producer -
// an empty declaration value (`color: ;`) and a dimension whose unit
// this track's contract does not recognize are the SAME conditions
// value_parse.cpp already names, reached through a different caller.
//
// GFSS-SHORTHAND (TODO.md wave W6, docs/plano-w6-folha-de-estilo.md
// D-W6-3) IS THE EIGHTH FATIA TO ADD ROWS HERE: the seven identifiers
// below are shorthand_expand.cpp's OWN (its family helpers - shorthand_
// four_sides.cpp, shorthand_four_corners.cpp, shorthand_axis_pair.cpp,
// shorthand_border_parts.cpp, shorthand_flex_flow.cpp - all share the
// ONE `shorthand_expand` producer tag, since the entry point a consumer
// sees is always shorthand_expand.cpp's own expand_shorthand(), never
// one family file called directly). `universal_keyword_alone` is
// REUSED from declaration_parse - the SAME condition (a universal
// keyword mixed with other tokens), reached through the shorthand's own
// value instead of a known property's.
//
// GFSS-SHORTHAND, S-3b (TODO.md wave W6, docs/plano-w6-folha-de-
// estilo.md D-W6-14, decisao do lider D4, 15/09/2026) IS THE NINTH
// FATIA TO ADD A ROW HERE: `longhand_not_overridden_by_later_
// shorthand` is shorthand_reset_notice.cpp's OWN - the SAME
// `shorthand_expand` producer tag as the seven rows above (the diagnostic
// still names a fact about a shorthand's own expansion, just a NOTICE
// instead of a rejection - D-W6-13's own separate `notices` list is
// what carries the distinction, never a second producer tag here). Its
// own line/column point at the SHORTHAND that did the overriding, never
// at the longhand it silently reset.
//
// GFSS-SHORTHAND, WAVE W6 BATCH 2 FIX (GODS_LAWS.md L-36, finding #1 of
// the 16/09/2026 adversarial review) IS THE TENTH ROW HERE:
// `internal_shorthand_table_defect` is shorthand_expand.cpp's OWN -
// same `shorthand_expand` producer tag, same "internal defect" category
// `internal_tokenizer_defect` (tokenizer.cpp) already names (this file's
// own "THREE OF THE FIRST FOUR..." paragraph above draws that line: the
// CSS spec has no concept of this at all). Emitted ONLY when
// `find_shorthand_longhand_entry()` (shorthand_longhand_table.hpp)
// returns nullptr for a `shorthand.shorthand_name` this library's own
// static_assert there proves cannot happen for any caller inside this
// library's own sources - a declared, diagnosed refusal instead of a
// dereference, never reachable through this track's own accepted
// eleven names, kept anyway per LEI ZERO's own "base de consumidores
// aberta e desconhecida" (a defensive floor, not a spec condition).
//
// NOEXCEPT-ALLOC-B8 fatia F4 (/var/tmp/glintfx-plan/plano-conserto-
// noexcept.md sec. "F4", ESCOPO.md Decisao 11, 17/09/2026) IS THE
// ELEVENTH ROW HERE: `anb_expression_too_long` is anb_parse.cpp's OWN
// (same `anb_parse` producer tag as its existing three rows) - emitted
// when an argument needs more tokens than anb_parse.cpp's own fixed-
// capacity token buffer (k_max_anb_tokens) can hold. THE SAME "self-
// imposed defensive floor, not a spec condition" category `not_
// recursion_limit` (selector_parse.cpp) already belongs to, above:
// CSS Syntax's own <an+b> microsyntax places no limit of its own on an
// argument's length, so refusing one that is merely LONG is this
// LIBRARY's own anti-DoS/anti-unbounded-allocation policy (LEI ZERO,
// "base de consumidores aberta e desconhecida"), never a grammar
// violation - which is exactly why this could not honestly reuse
// `end_of_anb_expression` (that one means "a value already finished
// parsing has trailing garbage after it", a real grammar fact; this
// one means "this parser refused to even try, because reading further
// would have meant allocating").
//
// GFSS-SEL-REJECT-ORPHAN (TODO.md wave W6, GODS_LAWS.md L-20/L-40,
// decision of the leader in ESCOPO.md's own "As seis decisoes da manha
// de 02/09" SS1, verbatim "Recusar na leitura da folha") IS THE
// TWELFTH ROW HERE: `answerable_pseudo_class` is selector_parse.cpp's
// OWN (same `selector_parse` producer tag as its existing rows, since
// it is emitted by the SAME parse_pseudo_selector() that already emits
// `known_pseudo_class`) - a DIFFERENT question from that sibling
// diagnostic, never a re-spelling of it: `known_pseudo_class` fires
// when the name is not pseudo-class VOCABULARY at all
// (selector_pseudo_vocabulary.hpp's own GLINTFX_GFSS_SIMPLE_PSEUDO_LIST
// does not name it); `answerable_pseudo_class` fires when the name IS
// known vocabulary but is also a member of that SAME file's own new
// GLINTFX_GFSS_ORPHAN_SIMPLE_PSEUDO_LIST - a name the eight-fact node
// contract (docs/node-view-and-matching.md's own "gap 1") can never
// answer. Before this fatia, such a selector was silently ACCEPTED and
// would simply never match; this diagnostic is what makes that failure
// visible at parse time instead. Does not touch the eight-fact contract
// itself (the leader's own order for this fatia, verbatim in TODO.md:
// "Nao mexe no contrato dos oito").

namespace glintfx::style::detail {

// Which producer file EMITS a diagnostic carrying this identifier - see
// this header's own GFSS-VOCAB-BIND paragraph above for why this exists
// and gfss_diagnostic_entry/k_expected_entries/count_owned_by() below
// for how a producer's own test proves it against this tag.
enum class gfss_diagnostic_producer : std::uint8_t {
    tokenizer,         // src/gfss/tokenizer.cpp (GFSS-TOKEN)
    selector_parse,    // src/gfss/selector_parse.cpp (GFSS-SEL-PARSE-CORE +
                       // GFSS-SEL-PARSE-PSEUDO-ELEMENT)
    value_parse,       // src/gfss/value_parse.cpp (GFSS-VALUE)
    anb_parse,         // src/gfss/anb_parse.cpp (GFSS-SEL-PARSE-NTH)
    declaration_parse, // src/gfss/declaration_parse.cpp (GFSS-DECL-PARSE)
    shorthand_expand,  // src/gfss/shorthand_expand.cpp + its five family files (GFSS-SHORTHAND)
    sheet_parse,       // src/gfss/sheet_parse.cpp (GFSS-SHEET-PARSE)
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
    X(identifier_after_double_colon, selector_parse)                                               \
    X(known_pseudo_element, selector_parse)                                                        \
    X(attribute_name, selector_parse)                                                              \
    X(attribute_operator_or_close, selector_parse)                                                 \
    X(attribute_value, selector_parse)                                                             \
    X(closing_square_bracket, selector_parse)                                                      \
    X(not_recursion_limit, selector_parse)                                                         \
    X(answerable_pseudo_class, selector_parse)                                                     \
    X(component_value, value_parse)                                                                \
    X(known_dimension_unit, value_parse)                                                           \
    X(anb_expression, anb_parse)                                                                   \
    X(anb_offset, anb_parse)                                                                       \
    X(end_of_anb_expression, anb_parse)                                                            \
    X(anb_expression_too_long, anb_parse)                                                          \
    X(property_name, declaration_parse)                                                            \
    X(known_property_name, declaration_parse)                                                      \
    X(longhand_property_names, declaration_parse)                                                  \
    X(renamed_property_name, declaration_parse)                                                    \
    X(colon_after_property_name, declaration_parse)                                                \
    X(important_flag_at_end_of_value, declaration_parse)                                           \
    X(universal_keyword_alone, declaration_parse)                                                  \
    X(keyword_for_property, declaration_parse)                                                     \
    X(value_nature_for_property, declaration_parse)                                                \
    X(value_count_for_property, declaration_parse)                                                 \
    X(value_in_range_for_property, declaration_parse)                                              \
    X(semicolon_or_end_of_declaration, declaration_parse)                                          \
    X(one_to_four_side_values, shorthand_expand)                                                   \
    X(one_to_four_corner_values, shorthand_expand)                                                 \
    X(corner_radius_without_slash, shorthand_expand)                                               \
    X(one_or_two_axis_values, shorthand_expand)                                                    \
    X(border_part_width_style_or_color, shorthand_expand)                                          \
    X(each_border_part_at_most_once, shorthand_expand)                                             \
    X(flex_direction_or_flex_wrap_word, shorthand_expand)                                          \
    X(longhand_not_overridden_by_later_shorthand, shorthand_expand)                                \
    X(internal_shorthand_table_defect, shorthand_expand)                                           \
    X(style_rule_or_at_rule, sheet_parse)                                                          \
    X(opening_curly_brace_after_selector, sheet_parse)                                             \
    X(opening_curly_brace_after_at_rule, sheet_parse)                                              \
    X(closing_curly_brace, sheet_parse)                                                            \
    X(keyframes_name, sheet_parse)                                                                 \
    X(supported_at_rule, sheet_parse)                                                              \
    X(at_rule_media_is_outside_v1, sheet_parse)

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
