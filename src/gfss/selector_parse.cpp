// SPDX-License-Identifier: AGPL-3.0-or-later
#include "selector_parse.hpp"

#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <glintfx/gfss/tokenizer.hpp>

#include "diagnostic_vocabulary.hpp"
#include "selector_pseudo_vocabulary.hpp"

// selector_parse.cpp - GFSS-SEL-PARSE-CORE + GFSS-SEL-PARSE-PSEUDO-
// ELEMENT + GFSS-SEL-PARSE-ATTR (TODO.md, GODS_LAWS.md
// L-17/L-20/L-27/L-28/L-40): the algorithm behind selector_parse.hpp's
// own parse_selector_list() - see that file's own header comment for
// the design tensions (result shape, noexcept) this implementation
// inherits rather than re-decides.
//
// GFSS-SEL-PARSE-ATTR'S OWN GRAMMAR, READ UNDER GODS_LAWS.md L-29/L-43
// (CSS2.1's own "attrib" production, since CSS Syntax Module Level 3's
// generic tokenizer has no dedicated ATTRIB_MATCHOP token of its own -
// this file's own match_attribute_operator() below explains why each
// operator is instead TWO byte-adjacent delim tokens): "[" S* IDENT S*
// [ ATTRIB_MATCHOP S* [ IDENT | STRING ] S* ]? "]" - whitespace (S*)
// permitted around every part, an operator+value pair entirely OPTIONAL
// (the bare presence form). parse_attribute_selector() below is a
// FIXED, bounded sequence of at most five tokens (name, operator
// prefix, operator suffix, value, close bracket) with NO loop over
// content and NO recursion - unlike capture_functional_argument()'s own
// anti-DoS concern below, an attribute value is a single ident-or-
// string token by this grammar, never a nested sub-selector, so there
// is no unbounded-depth hazard to guard against here at all.
//
// LAYERED ON TOP OF GFSS-TOKEN's OWN TOKEN STREAM, NEVER RE-SCANNING
// BYTES ITSELF (the SAME relationship color_parse.cpp's own header
// comment already documents for GFSS-COLOR-PARSE): every helper below
// walks a flat std::vector<gltfx_gfss_token> that
// tokenizer.hpp's own gltfx_gfss_tokenize() built once, in
// parse_selector_list() itself - comments are already gone (silently
// skipped by the tokenizer, tokenizer.hpp's own contract) and every
// token's `lexeme` is still a view into the CALLER's own `text` buffer
// (tokenizer.hpp's own gltfx_gfss_cursor::source comment), which is
// exactly what lets capture_functional_argument() below compute a raw
// argument span by POINTER ARITHMETIC on two tokens' own lexemes,
// never by re-reading `text` a second time.
//
// ADJACENCY IS BYTE-POINTER EQUALITY, NOT "NO WHITESPACE TOKEN IN
// BETWEEN" (scope decision made HERE, GODS_LAWS.md L-27, marked
// INFERENCE): CSS Selectors Level 4's own grammar requires a class
// selector's '.' and a pseudo-class's ':' to be immediately followed
// by their identifier, with NOTHING between them - not even a comment.
// tokens_are_adjacent() below checks
// `first.lexeme.data() + first.lexeme.size() == second.lexeme.data()`,
// which is stricter than "no whitespace TOKEN separates them": a
// comment between the two (silently deleted by the tokenizer, so it
// never becomes a token of its own) still leaves a BYTE GAP between
// the two lexemes, which this check correctly rejects - the SAME
// "comments behave like whitespace" reading token.hpp's own header
// comment already establishes for this track's grammar.
//
// PAREN DEPTH IS COUNTED ON THE TOKEN STREAM, NEVER BY RECURSING INTO
// THE ARGUMENT (GODS_LAWS.md L-40's own "anti-DoS de folha hostil"
// concern, named explicitly for GFSS-SEL-PARSE-NOT's future recursive
// parse - TODO.md): capture_functional_argument() below is an
// ITERATIVE loop over the flat token vector, tracking depth with a
// plain `int`. A functional pseudo-class's own '(' is not a separate
// open_paren TOKEN (tokenizer.hpp's own consume_ident_like_token()
// consumes it as the LAST byte of the function-token's own lexeme,
// e.g. "nth-child("), so a NESTED function token (":not(:nth-child(2))"
// has one) must count as an opening paren too, exactly like a bare
// open_paren token - paren_depth_delta() below treats both the same
// way. Because this loop never calls itself, an arbitrarily deep chain
// of nested parentheses inside a raw argument costs stack space
// proportional to ZERO, only the token vector's own O(n) memory - this
// fatia's own service order names "aninhamento profundo" as a hostile
// case to prove exactly this.
//
// EVERY HELPER BELOW RETURNS A SMALL "ok/value/diagnostic" STRUCT, THE
// SAME SHAPE color_parse_result (color_parse.hpp) ALREADY ESTABLISHES
// for this track - never a bool-plus-out-param, and never throwing a
// glintfx-defined exception across an internal call (this file has no
// public boundary of its own to guard - GODS_LAWS.md L-22 governs the
// PUBLIC surface, not a private helper's own return shape).

namespace glintfx::style::detail {

namespace {

using token_vector = std::vector<gltfx_gfss_token>;

[[nodiscard]] gltfx_gfss_diagnostic make_diagnostic(const gltfx_gfss_token &at,
                                                    std::string_view expected) noexcept {
    return gltfx_gfss_diagnostic{.line = at.line, .column = at.column, .expected = expected};
}

// See this file's own header comment above on why byte-pointer
// equality, not "no whitespace token between them", is the correct
// adjacency test for this grammar.
[[nodiscard]] bool tokens_are_adjacent(const gltfx_gfss_token &first,
                                       const gltfx_gfss_token &second) noexcept {
    return first.lexeme.data() + first.lexeme.size() == second.lexeme.data();
}

// A function-token's own lexeme always ends with the '(' that started
// it (tokenizer.hpp's own consume_ident_like_token()/consume_url_or_
// function_token()) - this strips exactly that one trailing byte to
// recover the bare function NAME, e.g. "nth-child(" -> "nth-child".
[[nodiscard]] std::string_view function_name(std::string_view lexeme) noexcept {
    return lexeme.substr(0, lexeme.size() - 1);
}

// +1 for a token that OPENS a nesting level this scanner must later
// close (a bare '(' or a nested function-token, which carries its own
// '(' as the LAST byte of its lexeme - see this file's own header
// comment above), -1 for the ')' that closes one, 0 for anything else.
// Never recurses - see this file's own header comment above on why
// this predicate is the anti-DoS mechanism, not a depth limit.
[[nodiscard]] int paren_depth_delta(gltfx_gfss_token_kind kind) noexcept {
    if (kind == gltfx_gfss_token_kind::open_paren || kind == gltfx_gfss_token_kind::function) {
        return 1;
    }
    if (kind == gltfx_gfss_token_kind::close_paren) {
        return -1;
    }
    return 0;
}

// Advances `index` past every consecutive whitespace token starting
// there, returning whether at least one was skipped - the caller uses
// that to distinguish "there was a gap here" (a candidate descendant
// combinator) from "these two tokens are already touching".
bool skip_whitespace(const token_vector &tokens, std::size_t &index) noexcept {
    bool skipped = false;
    while (tokens[index].kind == gltfx_gfss_token_kind::whitespace) {
        ++index;
        skipped = true;
    }
    return skipped;
}

// The delim token matching `child`/`next_sibling`/`subsequent_sibling`
// in selector_ast.hpp's own gfss_combinator_table, or std::nullopt for
// anything else (including `descendant`'s own table row, which never
// matches: no delim token's lexeme is ever a literal space - see that
// table's own header comment). A single lookup here replaces what
// would otherwise be a hand-written if/else chain that could drift
// from the enum selector_ast.hpp itself defines.
[[nodiscard]] std::optional<gfss_combinator>
explicit_combinator_from(const gltfx_gfss_token &tok) noexcept {
    if (tok.kind != gltfx_gfss_token_kind::delim || tok.lexeme.size() != 1) {
        return std::nullopt;
    }
    for (const auto &entry : gfss_combinator_table) {
        if (entry.combinator != gfss_combinator::descendant &&
            entry.delimiter == tok.lexeme.front()) {
            return entry.combinator;
        }
    }
    return std::nullopt;
}

struct argument_capture_outcome {
    bool ok = false;
    std::string_view text;
    std::size_t next_index = 0;
    gltfx_gfss_diagnostic diagnostic{};
};

// Captures the RAW bytes of a functional pseudo-class's own argument -
// everything between its '(' (already consumed as part of
// `function_token`'s own lexeme) and the matching ')' - without
// analyzing a single byte of it (this fatia's own scope line: "guarda
// o argumento cru"). `start_index` is the token right after
// `function_token` in the stream. See this file's own header comment
// above for why this is a flat loop, never recursion.
[[nodiscard]] argument_capture_outcome
capture_functional_argument(const token_vector &tokens, std::size_t start_index,
                            const gltfx_gfss_token &function_token) noexcept {
    const char *begin = function_token.lexeme.data() + function_token.lexeme.size();
    int depth = 1;
    for (std::size_t idx = start_index; idx < tokens.size(); ++idx) {
        const gltfx_gfss_token &tok = tokens[idx];
        if (tok.kind == gltfx_gfss_token_kind::eof) {
            break;
        }
        depth += paren_depth_delta(tok.kind);
        if (depth == 0) {
            const char *end = tok.lexeme.data();
            return {.ok = true,
                    .text = std::string_view(begin, static_cast<std::size_t>(end - begin)),
                    .next_index = idx + 1,
                    .diagnostic = {}};
        }
    }
    return {.ok = false,
            .text = {},
            .next_index = 0,
            .diagnostic = make_diagnostic(function_token, k_expected_closing_parenthesis)};
}

struct simple_selector_outcome {
    bool ok = false;
    gfss_simple_selector selector{};
    gltfx_gfss_diagnostic diagnostic{};
};

// GFSS-SEL-PARSE-PSEUDO-ELEMENT (TODO.md, 05/09/2026, GODS_LAWS.md
// L-28 decision 11 of 26/08/2026): true iff `tokens[index]` is a ':'
// immediately followed, with NO gap, by a SECOND ':' - CSS Selectors
// Level 4's own "::" pseudo-element sigil is two adjacent colon
// TOKENS, never one (GFSS-TOKEN's own tokenizer.cpp has no "::" token
// kind of its own - token.hpp's own GLINTFX_GFSS_TOKEN_KIND_LIST names
// 25 classes, `colon` among them, singular). Byte-pointer adjacency,
// the SAME test tokens_are_adjacent() already is, so a comment between
// the two colons (silently deleted by the tokenizer, never becoming a
// token) still correctly fails this check.
[[nodiscard]] bool starts_pseudo_element(const token_vector &tokens, std::size_t index) noexcept {
    const std::size_t second_index = index + 1;
    return second_index < tokens.size() &&
           tokens[second_index].kind == gltfx_gfss_token_kind::colon &&
           tokens_are_adjacent(tokens[index], tokens[second_index]);
}

// `tokens[index]` is the FIRST ':' of a "::" pseudo-element - the
// caller (parse_one_simple_selector below) already confirmed, via
// starts_pseudo_element() above, that a second, adjacent ':' follows.
// Requires an ident IMMEDIATELY adjacent to that SECOND colon (the
// SAME adjacency rule parse_pseudo_selector() already applies to the
// single-colon forms), matching one of the two known pseudo-element
// names (selector_pseudo_vocabulary.hpp's own k_pseudo_element_names).
// NEVER the single-colon legacy CSS2.1 spelling (":before") - that
// text takes the OTHER branch of parse_one_simple_selector's own
// dispatch below, unchanged by this addition, and still fails there as
// an unknown simple pseudo-class (gfss_selector_parse_test.cpp's own
// regression proof). OUT OF SCOPE, ON PURPOSE (this fatia's own
// service order, and selector_ast.hpp's own header comment above):
// fabricating the box a pseudo-element denotes is LAYOUT-PSEUDO-BOXES'
// job, not this one's - this function only recognizes the GRAMMAR and
// records the bare name, the same "shape, not meaning" scope
// parse_pseudo_selector() already keeps for `pseudo_class`. Advances
// `index` past the whole "::name" on success.
[[nodiscard]] simple_selector_outcome parse_pseudo_element(const token_vector &tokens,
                                                           std::size_t &index) noexcept {
    const gltfx_gfss_token &second_colon = tokens[index + 1];
    const std::size_t name_index = index + 2;
    const bool has_adjacent_ident = name_index < tokens.size() &&
                                    tokens[name_index].kind == gltfx_gfss_token_kind::ident &&
                                    tokens_are_adjacent(second_colon, tokens[name_index]);
    if (!has_adjacent_ident) {
        return {.ok = false,
                .selector = {},
                .diagnostic =
                    make_diagnostic(second_colon, k_expected_identifier_after_double_colon)};
    }
    const gltfx_gfss_token &name = tokens[name_index];
    if (!is_known_pseudo_element(name.lexeme)) {
        return {.ok = false,
                .selector = {},
                .diagnostic = make_diagnostic(name, k_expected_known_pseudo_element)};
    }
    index = name_index + 1;
    return {.ok = true,
            .selector = gfss_simple_selector{.kind = gfss_simple_selector_kind::pseudo_element,
                                             .name = name.lexeme,
                                             .raw_argument = {},
                                             .attribute_value = {}},
            .diagnostic = {}};
}

// `tokens[index]` is a '.' delim; a class selector requires an ident
// token IMMEDIATELY adjacent to it (see this file's own header comment
// above on adjacency). Advances `index` past both tokens on success.
[[nodiscard]] simple_selector_outcome parse_class_selector(const token_vector &tokens,
                                                           std::size_t &index) noexcept {
    const gltfx_gfss_token &dot = tokens[index];
    const std::size_t name_index = index + 1;
    const bool has_adjacent_ident = name_index < tokens.size() &&
                                    tokens[name_index].kind == gltfx_gfss_token_kind::ident &&
                                    tokens_are_adjacent(dot, tokens[name_index]);
    if (!has_adjacent_ident) {
        return {.ok = false,
                .selector = {},
                .diagnostic = make_diagnostic(dot, k_expected_identifier_after_dot)};
    }
    index = name_index + 1;
    return {.ok = true,
            .selector = gfss_simple_selector{.kind = gfss_simple_selector_kind::class_selector,
                                             .name = tokens[name_index].lexeme,
                                             .raw_argument = {},
                                             .attribute_value = {}},
            .diagnostic = {}};
}

// `tokens[function_index]` is a function-token whose name is already
// known to be one of the five recognized functional pseudo-classes
// (the caller checks that before calling this). Captures the raw
// argument and advances `index` past the whole `:name(...)` on
// success.
[[nodiscard]] simple_selector_outcome parse_functional_pseudo(const token_vector &tokens,
                                                              std::size_t function_index,
                                                              std::size_t &index) noexcept {
    const gltfx_gfss_token &function_token = tokens[function_index];
    const std::string_view name = function_name(function_token.lexeme);
    const auto argument = capture_functional_argument(tokens, function_index + 1, function_token);
    if (!argument.ok) {
        return {.ok = false, .selector = {}, .diagnostic = argument.diagnostic};
    }
    index = argument.next_index;
    return {.ok = true,
            .selector = gfss_simple_selector{.kind = gfss_simple_selector_kind::pseudo_function,
                                             .name = name,
                                             .raw_argument = argument.text,
                                             .attribute_value = {}},
            .diagnostic = {}};
}

// `tokens[index]` is a ':' delim; a pseudo-class requires an ident (no
// argument) or a function-token (functional pseudo) IMMEDIATELY
// adjacent to it. Advances `index` past the whole pseudo-class on
// success.
[[nodiscard]] simple_selector_outcome parse_pseudo_selector(const token_vector &tokens,
                                                            std::size_t &index) noexcept {
    const gltfx_gfss_token &colon = tokens[index];
    const std::size_t next_index = index + 1;
    if (next_index >= tokens.size() || !tokens_are_adjacent(colon, tokens[next_index])) {
        return {.ok = false,
                .selector = {},
                .diagnostic = make_diagnostic(colon, k_expected_identifier_after_colon)};
    }
    const gltfx_gfss_token &next = tokens[next_index];
    if (next.kind == gltfx_gfss_token_kind::ident) {
        if (!is_known_simple_pseudo(next.lexeme)) {
            return {.ok = false,
                    .selector = {},
                    .diagnostic = make_diagnostic(next, k_expected_known_pseudo_class)};
        }
        index = next_index + 1;
        return {.ok = true,
                .selector = gfss_simple_selector{.kind = gfss_simple_selector_kind::pseudo_class,
                                                 .name = next.lexeme,
                                                 .raw_argument = {},
                                                 .attribute_value = {}},
                .diagnostic = {}};
    }
    if (next.kind == gltfx_gfss_token_kind::function) {
        if (!is_known_functional_pseudo(function_name(next.lexeme))) {
            return {.ok = false,
                    .selector = {},
                    .diagnostic = make_diagnostic(next, k_expected_known_pseudo_function)};
        }
        return parse_functional_pseudo(tokens, next_index, index);
    }
    return {.ok = false,
            .selector = {},
            .diagnostic = make_diagnostic(colon, k_expected_identifier_after_colon)};
}

struct attribute_operator_outcome {
    bool matched = false;
    gfss_attribute_operator op = gfss_attribute_operator::equals;
    std::size_t next_index = 0;
};

// `tokens[index]` is the first token right after an attribute name and
// any whitespace. Recognizes one of the six operator spellings
// selector_ast.hpp's own gfss_attribute_operator_table lists, by table
// lookup - never a hand-written if/else chain that could drift from
// that table (the SAME technique explicit_combinator_from() above uses
// for the four combinators). `matched` is false, not a diagnosed
// error, when nothing here looks like an operator start at all: this
// function alone cannot tell a legitimately absent operator (the
// caller has already ruled out the presence form's own "]" before
// calling this) from a genuinely invented one (e.g. "%=") - that
// distinction is the CALLER's job (parse_attribute_selector below),
// since only it knows this is the "operator or close" grammar
// position.
[[nodiscard]] attribute_operator_outcome match_attribute_operator(const token_vector &tokens,
                                                                  std::size_t index) noexcept {
    const gltfx_gfss_token &tok = tokens[index];
    if (tok.kind == gltfx_gfss_token_kind::delim && tok.lexeme == std::string_view{"="}) {
        return {.matched = true, .op = gfss_attribute_operator::equals, .next_index = index + 1};
    }
    if (tok.kind != gltfx_gfss_token_kind::delim || tok.lexeme.size() != 1) {
        return {.matched = false, .op = {}, .next_index = index};
    }
    const std::size_t equals_index = index + 1;
    const bool has_adjacent_equals = equals_index < tokens.size() &&
                                     tokens[equals_index].kind == gltfx_gfss_token_kind::delim &&
                                     tokens[equals_index].lexeme == std::string_view{"="} &&
                                     tokens_are_adjacent(tok, tokens[equals_index]);
    if (!has_adjacent_equals) {
        return {.matched = false, .op = {}, .next_index = index};
    }
    for (const auto &entry : gfss_attribute_operator_table) {
        if (entry.prefix == tok.lexeme.front()) {
            return {.matched = true, .op = entry.op, .next_index = equals_index + 1};
        }
    }
    return {.matched = false, .op = {}, .next_index = index};
}

struct attribute_value_outcome {
    bool ok = false;
    std::string_view text;
    std::size_t next_index = 0;
    gltfx_gfss_diagnostic diagnostic{};
};

// `tokens[index]` is the first token right after an attribute operator.
// Reads the value grammar this fatia's own service order registers as
// a default "para veto" (CSS's own convention, since gfss's own format
// documentation does not specify quoting here at all): an <ident-token>
// decodes to its own lexeme verbatim (the SAME "lexeme is the raw
// source span" rule every other ident-shaped simple selector already
// follows), a <string-token> strips EXACTLY its own one opening and one
// closing quote character (never resolving an escape inside it -
// selector_ast.hpp's own header comment on gfss_simple_selector::
// attribute_value explains why). A <bad-string-token>, or a
// <string-token> that already carries its OWN diagnostic (the
// EOF-without-closing-quote case tokenizer.cpp's own
// consume_string_token() leaves as kind::string, not kind::bad_string -
// that function's own header comment), PROPAGATES that token's own
// diagnostic upward unchanged - the SAME layering color_parse.cpp's own
// parse_color() already applies to bad_string/bad_url, reused here
// rather than re-invented, so "aspas nao fechadas" reports the
// tokenizer's own k_expected_closing_quote, never a selector-parser
// diagnostic pretending to know more than the lexical layer already
// determined.
[[nodiscard]] attribute_value_outcome parse_attribute_value(const token_vector &tokens,
                                                            std::size_t index) noexcept {
    const gltfx_gfss_token &tok = tokens[index];
    if (tok.kind == gltfx_gfss_token_kind::ident) {
        return {.ok = true, .text = tok.lexeme, .next_index = index + 1, .diagnostic = {}};
    }
    if (tok.kind == gltfx_gfss_token_kind::bad_string || !tok.diagnostic.expected.empty()) {
        return {.ok = false, .text = {}, .next_index = 0, .diagnostic = tok.diagnostic};
    }
    if (tok.kind == gltfx_gfss_token_kind::string) {
        return {.ok = true,
                .text = tok.lexeme.substr(1, tok.lexeme.size() - 2),
                .next_index = index + 1,
                .diagnostic = {}};
    }
    return {.ok = false,
            .text = {},
            .next_index = 0,
            .diagnostic = make_diagnostic(tok, k_expected_attribute_value)};
}

// `tokens[index]` is the "[" that opens an attribute selector
// (selector_ast.hpp's own gfss_simple_selector_kind::attribute) - this
// file's own top comment names the grammar and why it needs neither a
// loop nor recursion. Advances `index` past the whole "[...]" on
// success.
[[nodiscard]] simple_selector_outcome parse_attribute_selector(const token_vector &tokens,
                                                               std::size_t &index) noexcept {
    const gltfx_gfss_token &open_bracket = tokens[index];
    std::size_t cursor = index + 1;
    skip_whitespace(tokens, cursor);
    if (tokens[cursor].kind != gltfx_gfss_token_kind::ident) {
        return {.ok = false,
                .selector = {},
                .diagnostic = make_diagnostic(tokens[cursor], k_expected_attribute_name)};
    }
    const gltfx_gfss_token &name = tokens[cursor];
    ++cursor;
    skip_whitespace(tokens, cursor);

    if (tokens[cursor].kind == gltfx_gfss_token_kind::close_square) {
        index = cursor + 1;
        return {.ok = true,
                .selector = gfss_simple_selector{.kind = gfss_simple_selector_kind::attribute,
                                                 .name = name.lexeme,
                                                 .raw_argument = {},
                                                 .attribute_value = {}},
                .diagnostic = {}};
    }

    const attribute_operator_outcome op = match_attribute_operator(tokens, cursor);
    if (!op.matched) {
        return {.ok = false,
                .selector = {},
                .diagnostic =
                    make_diagnostic(tokens[cursor], k_expected_attribute_operator_or_close)};
    }
    cursor = op.next_index;
    skip_whitespace(tokens, cursor);

    const attribute_value_outcome value = parse_attribute_value(tokens, cursor);
    if (!value.ok) {
        return {.ok = false, .selector = {}, .diagnostic = value.diagnostic};
    }
    cursor = value.next_index;
    skip_whitespace(tokens, cursor);

    if (tokens[cursor].kind != gltfx_gfss_token_kind::close_square) {
        return {.ok = false,
                .selector = {},
                .diagnostic = make_diagnostic(open_bracket, k_expected_closing_square_bracket)};
    }
    index = cursor + 1;
    return {.ok = true,
            .selector = gfss_simple_selector{.kind = gfss_simple_selector_kind::attribute,
                                             .name = name.lexeme,
                                             .raw_argument = {},
                                             .attribute_operator = op.op,
                                             .has_attribute_value = true,
                                             .attribute_value = value.text},
            .diagnostic = {}};
}

struct compound_parse_outcome {
    bool ok = false;
    gfss_compound_selector compound{};
    gltfx_gfss_diagnostic diagnostic{};
};

// One simple selector, glued directly (no whitespace, no combinator)
// onto whatever `index` already points to - a single dispatch step of
// parse_compound_selector()'s own loop, split out so that loop stays
// under CONTRACT.md SS6.2's own line/nesting limits (GODS_LAWS.md
// L-17). Returns std::nullopt for a token kind that cannot start (or
// continue) a compound selector - the caller reads that as "the
// compound selector ends here", never as an error by itself.
[[nodiscard]] std::optional<simple_selector_outcome>
parse_one_simple_selector(const token_vector &tokens, std::size_t &index) noexcept {
    const gltfx_gfss_token &tok = tokens[index];
    if (tok.kind == gltfx_gfss_token_kind::ident) {
        const gfss_simple_selector selector{.kind = gfss_simple_selector_kind::type,
                                            .name = tok.lexeme,
                                            .raw_argument = {},
                                            .attribute_value = {}};
        ++index;
        return simple_selector_outcome{.ok = true, .selector = selector, .diagnostic = {}};
    }
    if (tok.kind == gltfx_gfss_token_kind::hash) {
        const gfss_simple_selector selector{.kind = gfss_simple_selector_kind::id_selector,
                                            .name = tok.lexeme.substr(1),
                                            .raw_argument = {},
                                            .attribute_value = {}};
        ++index;
        return simple_selector_outcome{.ok = true, .selector = selector, .diagnostic = {}};
    }
    if (tok.kind == gltfx_gfss_token_kind::delim && tok.lexeme == std::string_view{"*"}) {
        ++index;
        return simple_selector_outcome{
            .ok = true,
            .selector = gfss_simple_selector{.kind = gfss_simple_selector_kind::universal,
                                             .name = {},
                                             .raw_argument = {},
                                             .attribute_value = {}},
            .diagnostic = {}};
    }
    if (tok.kind == gltfx_gfss_token_kind::delim && tok.lexeme == std::string_view{"."}) {
        return parse_class_selector(tokens, index);
    }
    if (tok.kind == gltfx_gfss_token_kind::colon) {
        if (starts_pseudo_element(tokens, index)) {
            return parse_pseudo_element(tokens, index);
        }
        return parse_pseudo_selector(tokens, index);
    }
    if (tok.kind == gltfx_gfss_token_kind::open_square) {
        return parse_attribute_selector(tokens, index);
    }
    return std::nullopt;
}

// One compound selector: one or more simple selectors with nothing
// between them (selector_ast.hpp's own gfss_compound_selector). Fails
// ONLY when zero simple selectors are found at `index` - never after
// at least one has already been accepted (an unrecognized token there
// simply ends the compound, the SAME "stop the loop, do not fail"
// shape parse_complex_selector() below uses for its own combinator
// loop).
[[nodiscard]] compound_parse_outcome parse_compound_selector(const token_vector &tokens,
                                                             std::size_t &index) noexcept {
    gfss_compound_selector compound;
    for (;;) {
        auto one = parse_one_simple_selector(tokens, index);
        if (!one.has_value()) {
            break;
        }
        if (!one->ok) {
            return {.ok = false, .compound = {}, .diagnostic = one->diagnostic};
        }
        compound.simple_selectors.push_back(one->selector);
    }
    if (compound.simple_selectors.empty()) {
        return {.ok = false,
                .compound = {},
                .diagnostic = make_diagnostic(tokens[index], k_expected_simple_selector)};
    }
    return {.ok = true, .compound = std::move(compound), .diagnostic = {}};
}

struct complex_parse_outcome {
    bool ok = false;
    gfss_complex_selector complex_selector{};
    gltfx_gfss_diagnostic diagnostic{};
};

// One complex selector: a head compound selector, then zero or more
// (combinator, compound) pairs (selector_ast.hpp's own gfss_complex_
// selector). An explicit combinator delim ('>'/'+'/'~') always wins
// over the implicit descendant reading; a descendant combinator is
// only tried when whitespace was actually skipped AND what follows is
// not the end of this complex selector (a comma or eof) - trying it
// unconditionally would misreport trailing whitespace before a comma
// as "expected a simple selector".
[[nodiscard]] complex_parse_outcome parse_complex_selector(const token_vector &tokens,
                                                           std::size_t &index) noexcept {
    auto head = parse_compound_selector(tokens, index);
    if (!head.ok) {
        return {.ok = false, .complex_selector = {}, .diagnostic = head.diagnostic};
    }
    gfss_complex_selector complex_selector{.head = std::move(head.compound), .rest = {}};

    for (;;) {
        const bool skipped_whitespace = skip_whitespace(tokens, index);
        const std::optional<gfss_combinator> explicit_combinator =
            explicit_combinator_from(tokens[index]);
        const bool at_list_boundary = tokens[index].kind == gltfx_gfss_token_kind::comma ||
                                      tokens[index].kind == gltfx_gfss_token_kind::eof;

        if (!explicit_combinator.has_value() && (!skipped_whitespace || at_list_boundary)) {
            break;
        }
        const gfss_combinator combinator =
            explicit_combinator.value_or(gfss_combinator::descendant);
        if (explicit_combinator.has_value()) {
            ++index;
            skip_whitespace(tokens, index);
        }
        auto next = parse_compound_selector(tokens, index);
        if (!next.ok) {
            return {.ok = false, .complex_selector = {}, .diagnostic = next.diagnostic};
        }
        complex_selector.rest.push_back(
            gfss_combined_selector{.combinator = combinator, .compound = std::move(next.compound)});
    }
    return {.ok = true, .complex_selector = std::move(complex_selector), .diagnostic = {}};
}

} // namespace

selector_parse_result parse_selector_list(std::string_view text) {
    const token_vector tokens = gltfx_gfss_tokenize(text);
    std::size_t index = 0;
    skip_whitespace(tokens, index);

    gfss_selector_list list;
    for (;;) {
        auto complex_result = parse_complex_selector(tokens, index);
        if (!complex_result.ok) {
            return {.ok = false, .value = {}, .diagnostic = complex_result.diagnostic};
        }
        list.selectors.push_back(std::move(complex_result.complex_selector));

        skip_whitespace(tokens, index);
        const gltfx_gfss_token &tok = tokens[index];
        if (tok.kind == gltfx_gfss_token_kind::comma) {
            ++index;
            skip_whitespace(tokens, index);
            continue;
        }
        if (tok.kind == gltfx_gfss_token_kind::eof) {
            break;
        }
        return {.ok = false,
                .value = {},
                .diagnostic = make_diagnostic(tok, k_expected_comma_or_end_of_selector_list)};
    }
    return {.ok = true, .value = std::move(list), .diagnostic = {}};
}

} // namespace glintfx::style::detail
