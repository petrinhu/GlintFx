// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

// selector_ast.hpp - GFSS-SEL-PARSE-CORE (TODO.md, GODS_LAWS.md
// L-17/L-19/L-20/L-28): the closed vocabulary of selector GRAMMAR
// shapes selector_parse.cpp's parser can produce - the tag/.class/
// #id/*, compound selector, comma-separated list, the four
// combinators, and every pseudo-class this fatia's own service order
// (TODO.md GFSS-SEL-PARSE-CORE row) enumerates: the 14 argument-less
// ones matched by name (selector_pseudo_vocabulary.hpp), plus the 5
// functional ones RECOGNIZED by name with their argument left
// UNANALYZED (see gfss_simple_selector_kind::pseudo_function below).
//
// GFSS-SEL-PARSE-PSEUDO-ELEMENT (TODO.md, 05/09/2026) ADDS A 7TH SHAPE
// (fact vs. inference, GODS_LAWS.md L-27): the project leader's own
// decision of 26/08/2026 (GODS_LAWS.md L-28 decision 11) names
// "::before"/"::after" for the v1 - CSS Selectors Level 4's OWN
// double-colon pseudo-element sigil, distinct from the single-colon
// pseudo-CLASS forms this file already enumerates above. See
// gfss_simple_selector_kind::pseudo_element below for the shape, and
// this fatia's own scope line (selector_parse.cpp's own header
// comment on parse_pseudo_element()) for why FABRICATING the box a
// pseudo-element denotes is explicitly OUT of scope here (LAYOUT-
// PSEUDO-BOXES, a different fatia entirely) - this file only names the
// GRAMMAR shape and the bare identifier it carries, never a box.
//
// GFSS-SEL-PARSE-ATTR (TODO.md, 05/09/2026) ADDS AN 8TH SHAPE: the
// attribute selector, "[foo]" (bare presence) or "[foo<op>value]" (one
// of six comparison operators). READ THE ROW BEFORE TRUSTING ITS OWN
// COUNT (GODS_LAWS.md L-27/L-18, fact vs. inference, reported to the
// orchestrator rather than resolved here): TODO.md's own service order
// for this fatia says "os 7 operadores" and then names exactly SIX
// (equals, includes ~=, dash_match |=, prefix_match ^=, suffix_match
// $=, substring_match *=), with bare presence named SEPARATELY in the
// same sentence. gfss_attribute_operator below holds exactly those SIX
// named operators - presence is not folded in as a fabricated seventh
// member (see gfss_simple_selector::has_attribute_value below for how
// presence is actually represented). The value grammar this fatia
// implements ("[foo=bar]" unquoted, or "[foo=\"bar\"]" quoted) is a
// DEFAULT the service order itself registers as "para veto (nao
// bloqueia)": CSS's own convention (an <ident-token> or a <string-
// token>), because the gfss format's own documentation does not specify
// quoting rules for this value at all.
//
// SEPARATE FILE FROM selector_parse.hpp/.cpp - deliberate (GODS_LAWS.md
// L-17: "arquivo e atomo de assunto"): this file answers "what SHAPE
// can a gfss selector take", never "how do I read gfss TEXT into that
// shape" - the SAME split token.hpp/tokenizer.hpp already establish
// for this track (token.hpp: the vocabulary; tokenizer.hpp: the
// algorithm that produces it).
//
// INTERNAL IN THIS SLICE, ON PURPOSE (GODS_LAWS.md L-19, the SAME
// reasoning color_parse.hpp's own header comment already gives for
// this track): lives under src/gfss/, not include/glintfx/ - GFSS-API
// (TODO.md, wave W10) is the dedicated review that decides the PUBLIC
// shape. Nothing here is ABI-frozen.
//
// STRING_VIEW FIELDS ARE NON-OWNING, SAME LIFETIME RULE AS
// gltfx_gfss_cursor::source (tokenizer.hpp): every std::string_view
// below is a view into the CALLER's own gfss source buffer, which must
// outlive every value this file's types hold - exactly the discipline
// gltfx_gfss_token::lexeme already documents.

namespace glintfx::style::detail {

// The four combinators the CSS Selectors Level 4 grammar defines
// (read under GODS_LAWS.md L-29) - descendant is the WHITESPACE
// combinator, the only one with no delimiter character of its own;
// the other three each spell their own single-character delim token
// ('>' / '+' / '~').
//
// GLINTFX_GFSS_COMBINATOR_LIST(X) - the SINGLE authoritative list: the
// enum, its own mechanically-derived count AND the (combinator,
// delimiter) lookup table below are all generated from this one list,
// so they cannot drift apart (GODS_LAWS.md L-40 achado 1 of
// 26/08/2026, "a enumeracao fechada nao e fechada" - the SAME
// technique token.hpp's own GLINTFX_GFSS_TOKEN_KIND_LIST already uses,
// applied here even though four is small and stable, because the
// service order for this fatia asks for the count to be DERIVED, not
// asserted against a hand-typed literal). descendant's own delimiter
// entry is ' ' - documentary, never matched against a real delim
// token's lexeme (no CSS delim token ever holds a literal space
// character; whitespace is its own, distinct token kind, token.hpp's
// own gltfx_gfss_token_kind::whitespace) - selector_parse.cpp's own
// explicit_combinator_from() skips this entry by construction. Private
// to this header - defined and #undef'd immediately below.
#define GLINTFX_GFSS_COMBINATOR_LIST(X)                                                            \
    X(descendant, ' ')                                                                             \
    X(child, '>')                                                                                  \
    X(next_sibling, '+')                                                                           \
    X(subsequent_sibling, '~')

enum class gfss_combinator : std::uint8_t {
#define GLINTFX_GFSS_COMBINATOR_ENUMERATOR(name, delim_char) name,
    GLINTFX_GFSS_COMBINATOR_LIST(GLINTFX_GFSS_COMBINATOR_ENUMERATOR)
#undef GLINTFX_GFSS_COMBINATOR_ENUMERATOR
};

// The list's own cardinality, counted mechanically - never a
// hand-copied literal (GODS_LAWS.md L-40 achado 1).
inline constexpr std::size_t gfss_combinator_count = [] {
    std::size_t count = 0;
#define GLINTFX_GFSS_COMBINATOR_COUNT_ONE(name, delim_char) ++count;
    GLINTFX_GFSS_COMBINATOR_LIST(GLINTFX_GFSS_COMBINATOR_COUNT_ONE)
#undef GLINTFX_GFSS_COMBINATOR_COUNT_ONE
    return count;
}();

struct gfss_combinator_entry {
    gfss_combinator combinator = gfss_combinator::descendant;
    char delimiter = ' ';
};

// Mechanically-built lookup table (GODS_LAWS.md L-40's "the space is
// small, enumerate it whole") - gfss_selector_parse_test.cpp sweeps
// this to prove all four combinators, and selector_parse.cpp's own
// explicit_combinator_from() consults it instead of a hand-written
// if/else chain that could drift from the enum above.
inline constexpr std::array<gfss_combinator_entry, gfss_combinator_count> gfss_combinator_table{
#define GLINTFX_GFSS_COMBINATOR_ARRAY_ONE(name, delim_char)                                        \
    gfss_combinator_entry{.combinator = gfss_combinator::name, .delimiter = (delim_char)},
    GLINTFX_GFSS_COMBINATOR_LIST(GLINTFX_GFSS_COMBINATOR_ARRAY_ONE)
#undef GLINTFX_GFSS_COMBINATOR_ARRAY_ONE
};

#undef GLINTFX_GFSS_COMBINATOR_LIST

// Attribute selector match operator - CSS Selectors' own family of six
// operators for "[name<op>value]" (read under GODS_LAWS.md L-29: MDN's
// own "Attribute selectors" page and the CSS2.1 grammar it still
// documents for these ATTRIB_MATCHOP forms). CSS Syntax Module Level
// 3's generic tokenizer, unlike CSS2.1's own dedicated grammar, has no
// combined token for any of these: each is spelled as ONE delim token
// (bare "=") or TWO byte-adjacent delim tokens (a prefix character
// immediately followed by "=", no whitespace and no comment between
// them - the SAME adjacency rule selector_parse.cpp's own
// tokens_are_adjacent() already applies to "." + ident and ":" + ":").
//
// GLINTFX_GFSS_ATTR_OPERATOR_LIST(X) - the SAME closed-enumeration
// technique gfss_combinator above already uses (GODS_LAWS.md L-40
// achado 1, "a enumeracao fechada nao e fechada"): the enum, its own
// mechanically-derived count and the lookup table below are all
// generated from this ONE list, so a operator added to the grammar
// cannot silently fall out of sync with the table selector_parse.cpp's
// own match_attribute_operator() consults. Private to this header -
// defined and #undef'd immediately below.
//
// PRESENCE IS NOT A SEVENTH MEMBER HERE (this file's own header comment
// above, GFSS-SEL-PARSE-ATTR): bare "[foo]" has no operator token at
// all, so folding it into this enum would need a fabricated sentinel
// value standing in for "absent" - gfss_simple_selector::has_
// attribute_value below is the actual discriminator, the SAME role a
// separate bool already plays wherever this track's own "empty/default
// means absent" convention (token.hpp's own R4) cannot use the field's
// own type to carry that meaning.
#define GLINTFX_GFSS_ATTR_OPERATOR_LIST(X)                                                        \
    X(equals, '\0')                                                                               \
    X(includes, '~')                                                                              \
    X(dash_match, '|')                                                                            \
    X(prefix_match, '^')                                                                          \
    X(suffix_match, '$')                                                                          \
    X(substring_match, '*')

enum class gfss_attribute_operator : std::uint8_t {
#define GLINTFX_GFSS_ATTR_OPERATOR_ENUMERATOR(name, prefix_char) name,
    GLINTFX_GFSS_ATTR_OPERATOR_LIST(GLINTFX_GFSS_ATTR_OPERATOR_ENUMERATOR)
#undef GLINTFX_GFSS_ATTR_OPERATOR_ENUMERATOR
};

// The list's own cardinality, counted mechanically - never a
// hand-copied literal (GODS_LAWS.md L-40 achado 1).
inline constexpr std::size_t gfss_attribute_operator_count = [] {
    std::size_t count = 0;
#define GLINTFX_GFSS_ATTR_OPERATOR_COUNT_ONE(name, prefix_char) ++count;
    GLINTFX_GFSS_ATTR_OPERATOR_LIST(GLINTFX_GFSS_ATTR_OPERATOR_COUNT_ONE)
#undef GLINTFX_GFSS_ATTR_OPERATOR_COUNT_ONE
    return count;
}();

// `prefix` is '\0' for the bare "=" form (equals has no leading
// character of its own); every other row's own prefix is the ONE
// character that, immediately adjacent to a trailing '=' delim, spells
// that operator (e.g. '~' + "=" adjacent = includes).
struct gfss_attribute_operator_entry {
    gfss_attribute_operator op = gfss_attribute_operator::equals;
    char prefix = '\0';
};

// Mechanically-built lookup table (GODS_LAWS.md L-40's "the space is
// small, enumerate it whole") - gfss_selector_parse_test.cpp sweeps
// this to prove all six operators, and selector_parse.cpp's own
// match_attribute_operator() consults it instead of a hand-written
// if/else chain that could drift from the enum above.
inline constexpr std::array<gfss_attribute_operator_entry, gfss_attribute_operator_count>
    gfss_attribute_operator_table{
#define GLINTFX_GFSS_ATTR_OPERATOR_ARRAY_ONE(name, prefix_char)                                   \
    gfss_attribute_operator_entry{.op = gfss_attribute_operator::name, .prefix = (prefix_char)},
        GLINTFX_GFSS_ATTR_OPERATOR_LIST(GLINTFX_GFSS_ATTR_OPERATOR_ARRAY_ONE)
#undef GLINTFX_GFSS_ATTR_OPERATOR_ARRAY_ONE
    };

#undef GLINTFX_GFSS_ATTR_OPERATOR_LIST

// Every simple-selector shape this fatia's own service order lists.
// This enum is NOT X-macro'd (unlike gfss_combinator above): its six
// values are fixed by the grammar's own STRUCTURE (four distinct
// PARSING SHAPES plus the two pseudo-class forms), not an open catalog
// of NAMES the way pseudo-class identifiers are (selector_pseudo_
// vocabulary.hpp's own two lists, which ARE X-macro'd, because a name
// can be added there and forgotten) - there is no comparable "a 7th
// simple-selector shape was added and the count forgot to follow"
// risk class here.
//
// FUNCTIONAL PSEUDO-CLASSES ARE RECOGNIZED, NOT PARSED (the fatia's
// own scope line, TODO.md GFSS-SEL-PARSE-CORE: "guarda o argumento cru
// para as duas fatias seguintes") - pseudo_function below is the ONE
// kind whose meaning this fatia does not fully resolve: it names WHICH
// of the five functional pseudo-classes matched
// (gfss_simple_selector::name) and hands the UNANALYZED argument text
// to gfss_simple_selector::raw_argument, byte for byte, for
// GFSS-SEL-PARSE-NTH/GFSS-SEL-PARSE-NOT (TODO.md, wave W4/W5) to read.
enum class gfss_simple_selector_kind : std::uint8_t {
    type,            // tag
    universal,       // *
    class_selector,  // .foo
    id_selector,     // #foo
    pseudo_class,    // :hover, :first-child, ... (no argument)
    pseudo_function, // :nth-child(...), :not(...), ... (raw argument)
    pseudo_element,  // ::before, ::after (no argument)
    attribute,       // [foo], [foo=bar], [foo~="bar"], ... (GFSS-SEL-PARSE-ATTR)
};

// One simple selector. `name` holds the tag name / class name (without
// the leading '.') / id name (without the leading '#') / pseudo-class
// name (without the leading ':') / pseudo-element name (without the
// leading "::") for every kind except `universal`, which names nothing
// and leaves `name` empty. `raw_argument` is populated ONLY for
// `pseudo_function` - the exact source bytes between the function's
// own '(' and its matching ')', unanalyzed (see this file's own header
// comment above) - and stays empty for every other kind, INCLUDING
// `pseudo_element` (which carries no argument of its own, the same
// shape as `pseudo_class`), the SAME "empty means absent" convention
// token.hpp's own gltfx_gfss_diagnostic already establishes
// (docs/api-conventions.md R4).
struct gfss_simple_selector {
    gfss_simple_selector_kind kind = gfss_simple_selector_kind::universal;
    std::string_view name;
    std::string_view raw_argument;

    // GFSS-SEL-PARSE-ATTR (TODO.md, 05/09/2026): the THREE fields below
    // are populated ONLY for `kind == attribute` - the SAME "empty/
    // default means absent for every other kind" convention this
    // struct's own comment above already establishes for
    // `raw_argument`. The attribute's own NAME (e.g. "foo" for "[foo]")
    // is `name` above, reused rather than duplicated in a fourth field
    // (CONTRACT.md SS6.7's own "duplicacao real" test: it would be the
    // SAME data wearing a second name).
    //
    // `has_attribute_value` is the DISCRIMINATOR between the bare
    // presence form "[foo]" (false - `attribute_operator` and
    // `attribute_value` are then both meaningless and never read) and
    // every comparison form "[foo<op>value]" (true) - see selector_ast.
    // hpp's own header comment above on gfss_attribute_operator for why
    // presence is a separate bool here, never a fabricated seventh
    // operator value.
    gfss_attribute_operator attribute_operator = gfss_attribute_operator::equals;
    bool has_attribute_value = false;

    // The DECODED value text: an unquoted ident's own lexeme verbatim,
    // or a quoted string's own lexeme with EXACTLY its one opening and
    // one closing quote character stripped (selector_parse.cpp's own
    // parse_attribute_value() does the stripping). An escape sequence
    // INSIDE the string is left UNRESOLVED - the same "lexeme is the
    // raw source span" scope token.hpp's own header comment fixes for
    // this whole track; resolving it would need this struct to OWN a
    // std::string, which no sibling kind does and which gfss_simple_
    // selector::raw_argument's own precedent above already rules out.
    std::string_view attribute_value;
};

// A compound selector: simple selectors glued with NO combinator
// between them (CSS Selectors Level 4's own definition, read under
// GODS_LAWS.md L-29 - e.g. "button.primary#ok" is ONE compound
// selector, three simple selectors). Never empty in a value this
// fatia's parser returns successfully - an EMPTY compound is exactly
// the "simple_selector" diagnostic diagnostic_vocabulary.hpp names
// instead of ever being produced (see selector_parse.cpp's own header
// comment).
struct gfss_compound_selector {
    std::vector<gfss_simple_selector> simple_selectors;
};

// One combinator and the compound selector it introduces - the
// building block of gfss_complex_selector::rest below.
struct gfss_combined_selector {
    gfss_combinator combinator = gfss_combinator::descendant;
    gfss_compound_selector compound;
};

// A complex selector: one or more compound selectors joined by
// combinators, e.g. "button.primary #ok" (a descendant combinator
// between two compounds). `head` is ALWAYS the first compound - no
// combinator precedes it, so this shape has no "ignored field" the
// way a single flat vector of (combinator, compound) pairs would carry
// for its own first element (GODS_LAWS.md L-17's "a frase sem e" test:
// a field that means something only sometimes is two fields wearing
// one name).
struct gfss_complex_selector {
    gfss_compound_selector head;
    std::vector<gfss_combined_selector> rest;
};

// A full gfss selector: the comma-separated LIST of complex selectors
// this fatia's own service order requires ("Lista por virgula") - a
// single bare selector with no comma is a list of exactly one.
struct gfss_selector_list {
    std::vector<gfss_complex_selector> selectors;
};

} // namespace glintfx::style::detail
