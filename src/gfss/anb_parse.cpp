// SPDX-License-Identifier: AGPL-3.0-or-later
#include "anb_parse.hpp"

#include <charconv>
#include <cstddef>
#include <limits>
#include <optional>
#include <string_view>
#include <system_error>
#include <vector>

#include <glintfx/gfss/tokenizer.hpp>

#include "ascii_case.hpp"
#include "diagnostic_vocabulary.hpp"

// anb_parse.cpp - GFSS-SEL-PARSE-NTH (TODO.md, GODS_LAWS.md
// L-17/L-20/L-27/L-40; source read under L-29/L-43): the algorithm
// behind anb_parse.hpp's own parse_anb() - see that file's own header
// comment for the design tensions (result shape, noexcept) this
// implementation inherits rather than re-decides, and this comment for
// the GRAMMAR itself.
//
// THE 16 PRODUCTIONS, READ FROM CSS SYNTAX MODULE LEVEL 3 SS6.2 "THE
// <AN+B> TYPE" (https://www.w3.org/TR/css-syntax-3/#anb-microsyntax,
// GODS_LAWS.md L-29/L-43 - read to LEARN the grammar, never to copy an
// implementation of it):
//
//   1. odd                                   6. -n
//   2. even                                   7. <ndashdigit-dimension>
//   3. <integer>                              8. '+'? <ndashdigit-ident>
//   4. <n-dimension>                          9. <dashndashdigit-ident>
//   5. '+'? n                                10. <n-dimension> <signed-integer>
//  11. '+'? n <signed-integer>               14. '+'? n- <signless-integer>
//  12. -n <signed-integer>                   15. -n- <signless-integer>
//  13. <ndash-dimension> <signless-integer>  16. <n-dimension> ['+'|'-'] <signless-integer>
//                                                 (and its '+'? n twin)
//
// Cross-checked against CSS Syntax's own SS6.1 "Informal Syntax
// Description" (same URL) for the prose rules this file's own tests
// enumerate by name: "the 1 may be omitted" (bare "n"/"-n" mean
// coefficient +-1), "the +B part may be omitted... unless the A part
// is already omitted" (bare "3n" means offset 0; a totally empty
// argument is NEVER valid - no production is the empty string), "When
// B is negative, its minus sign replaces the + sign" (an ndashdigit
// form's own embedded digits are ALWAYS subtracted, never added), and
// "whitespace is permitted on either side of the + or -" separating An
// and B when both are present. MDN's own :nth-child() page supplied
// the worked examples this file's test suite reuses as directed
// samples, one per production.
//
// LAYERED ON TOP OF GFSS-TOKEN'S OWN TOKEN STREAM - see anb_parse.hpp's
// own header comment for why a second, hand-rolled character scanner
// has no reason to exist alongside gltfx_gfss_tokenize().
//
// numeric_prefix_length()/split_dimension_lexeme() BELOW ARE A
// DELIBERATE, DOCUMENTED DUPLICATE OF value_parse.cpp's OWN
// split_dimension_lexeme() (GODS_LAWS.md L-27, marked INFERENCE, NOT a
// DRY violation left unexamined): that file's own version re-runs
// lexical_rules.hpp's own consume_number() on a throwaway cursor,
// which is the MORE PRINCIPLED technique (it can never disagree with
// how the tokenizer itself found the boundary) - but doing the SAME
// here would make gfss_selector_parse_test.cpp's own test executable
// need a SECOND internal .cpp compiled in alongside this one
// (lexical_rules.cpp, tests/CMakeLists.txt's own established pattern
// for value_parse.cpp/numeric_lexeme.cpp), and tests/CMakeLists.txt is
// explicitly outside this fatia's own file list (the orchestrator owns
// that wiring). A dimension token's own numeric prefix is a SMALL,
// well-understood shape (CSS Syntax Module Level 3 4.3.12's own
// grammar: optional sign, digits, optional '.'+digits, optional
// exponent) - re-walking it by hand here, rather than pulling in a
// second producer file's own private helper, is the smaller footprint
// for a fatia that does not own the test target's own source list.
// number_lexeme_is_integer()/decode_anb_integer() below mirror
// value_parse.cpp's own number_lexeme_is_integer()/decode_integer_
// lexeme() the SAME way, for the SAME reason - see this comment.

namespace glintfx::style::detail {

namespace {

using token_vector = std::vector<gltfx_gfss_token>;

[[nodiscard]] gltfx_gfss_diagnostic make_diagnostic(const gltfx_gfss_token &at,
                                                    std::string_view expected) noexcept {
    return gltfx_gfss_diagnostic{.line = at.line, .column = at.column, .expected = expected};
}

// The SAME byte-pointer adjacency test selector_parse.cpp's own
// tokens_are_adjacent() already is (that file's own header comment
// explains why byte-pointer equality, not "no whitespace token
// between them", is the correct test for this track's grammar) -
// duplicated here rather than shared, the SAME reasoning this file's
// own top comment already gives for numeric_prefix_length().
[[nodiscard]] bool tokens_are_adjacent(const gltfx_gfss_token &first,
                                       const gltfx_gfss_token &second) noexcept {
    return first.lexeme.data() + first.lexeme.size() == second.lexeme.data();
}

void skip_whitespace(const token_vector &tokens, std::size_t &index) noexcept {
    while (tokens[index].kind == gltfx_gfss_token_kind::whitespace) {
        ++index;
    }
}

// CSS Syntax Module Level 3 4.3.12's own "type" flag: a <number-token>
// is "integer" unless its lexeme contains a decimal point or an
// exponent marker - the SAME reading value_parse.cpp's own number_
// lexeme_is_integer() already gives (this file's own top comment on
// why this is a deliberate duplicate, not an unexamined one).
[[nodiscard]] bool number_lexeme_is_integer(std::string_view lexeme) noexcept {
    return lexeme.find_first_of(".eE") == std::string_view::npos;
}

// Parses a lexeme already known to hold no '.'/exponent (number_
// lexeme_is_integer() above) into a signed integer, saturating on
// overflow rather than failing - the SAME "the library never aborts
// the consumer's process on hostile input" principle anb.hpp's own
// header comment already names for this fatia, at the integer domain,
// the SAME technique value_parse.cpp's own decode_integer_lexeme()
// already uses (std::from_chars does not accept a leading '+', so one
// is stripped by hand first).
[[nodiscard]] long long decode_anb_integer(std::string_view lexeme) noexcept {
    std::string_view digits = lexeme;
    if (!digits.empty() && digits.front() == '+') {
        digits.remove_prefix(1);
    }
    long long magnitude = 0;
    const std::from_chars_result result =
        std::from_chars(digits.data(), digits.data() + digits.size(), magnitude);
    if (result.ec == std::errc::result_out_of_range) {
        const bool is_negative = !digits.empty() && digits.front() == '-';
        return is_negative ? std::numeric_limits<long long>::lowest()
                           : std::numeric_limits<long long>::max();
    }
    return magnitude;
}

[[nodiscard]] bool lexeme_has_explicit_sign(std::string_view lexeme) noexcept {
    return !lexeme.empty() && (lexeme.front() == '+' || lexeme.front() == '-');
}

// Returns the digit run's own value (always later SUBTRACTED at the
// call site - "n-5" means "-5", never "+5", CSS Syntax's own "When B
// is negative, its minus sign replaces the + sign") iff `text` is one
// or more ASCII digits and nothing else - never a partial match
// ("n-5x" is not an ndashdigit ident at all, and this returns
// std::nullopt for its own "5x" tail).
[[nodiscard]] std::optional<long long> parse_digit_run(std::string_view text) noexcept {
    if (text.empty()) {
        return std::nullopt;
    }
    for (const char ch : text) {
        if (ch < '0' || ch > '9') {
            return std::nullopt;
        }
    }
    return decode_anb_integer(text);
}

[[nodiscard]] bool ascii_case_insensitive_starts_with(std::string_view text,
                                                      std::string_view prefix) noexcept {
    return text.size() >= prefix.size() &&
           ascii_case_insensitive_equal(text.substr(0, prefix.size()), prefix);
}

// See this file's own top comment: a hand-walked re-derivation of CSS
// Syntax Module Level 3 4.3.12's own "consume a number" grammar over
// just a <dimension-token>'s own lexeme, never the caller's whole
// source buffer - an optional leading sign, one or more digits, an
// optional '.' followed by one or more digits, an optional 'e'/'E'
// exponent with its own optional sign and digits. The first byte that
// does not fit this shape is where the unit begins.
[[nodiscard]] std::size_t numeric_prefix_length(std::string_view lexeme) noexcept {
    std::size_t i = 0;
    const std::size_t n = lexeme.size();
    if (i < n && (lexeme[i] == '+' || lexeme[i] == '-')) {
        ++i;
    }
    while (i < n && lexeme[i] >= '0' && lexeme[i] <= '9') {
        ++i;
    }
    if (i < n && lexeme[i] == '.' && i + 1 < n && lexeme[i + 1] >= '0' && lexeme[i + 1] <= '9') {
        i += 2;
        while (i < n && lexeme[i] >= '0' && lexeme[i] <= '9') {
            ++i;
        }
    }
    const std::size_t after_mantissa = i;
    std::size_t j = i;
    if (j < n && (lexeme[j] == 'e' || lexeme[j] == 'E')) {
        ++j;
        if (j < n && (lexeme[j] == '+' || lexeme[j] == '-')) {
            ++j;
        }
        if (j < n && lexeme[j] >= '0' && lexeme[j] <= '9') {
            while (j < n && lexeme[j] >= '0' && lexeme[j] <= '9') {
                ++j;
            }
            return j;
        }
    }
    return after_mantissa;
}

struct dimension_split {
    std::string_view number_part;
    std::string_view unit_part;
};

[[nodiscard]] dimension_split split_dimension_lexeme(std::string_view lexeme) noexcept {
    const std::size_t boundary = numeric_prefix_length(lexeme);
    return {.number_part = lexeme.substr(0, boundary), .unit_part = lexeme.substr(boundary)};
}

// The result of recognizing an "An" (coefficient) shape - `a` is
// always set when `ok`; `embeds_b` marks the ndashdigit-ident/
// ndashdigit-dimension/dashndashdigit-ident forms (productions 7/8/9),
// whose OWN token already carries the complete B too
// (`embedded_b`); `requires_trailing_offset` marks the ndash-ident/
// ndash-dimension forms (productions 13/14/15), which MUST be followed
// by a signless integer - never optional, unlike the bare "n"/"-n"/
// <n-dimension> forms (productions 4/5/6), which stand alone with b
// implicitly 0 when nothing follows.
struct an_shape {
    bool ok = false;
    long long a = 0;
    bool embeds_b = false;
    long long embedded_b = 0;
    bool requires_trailing_offset = false;
};

// Productions 4-6/13-15: the "An" part spelled as a bare IDENT -
// "n"/"-n" (coefficient +-1, the 1 omitted per CSS Syntax's own SS6.1),
// "n-"/"-n-" (coefficient +-1, REQUIRING a trailing signless integer),
// or "n-<digits>"/"-n-<digits>" (coefficient +-1, with B already
// embedded, always negative - "n-5" means a=1, b=-5).
[[nodiscard]] an_shape match_an_ident(std::string_view name) noexcept {
    if (ascii_case_insensitive_equal(name, "n")) {
        return {.ok = true, .a = 1};
    }
    if (ascii_case_insensitive_equal(name, "-n")) {
        return {.ok = true, .a = -1};
    }
    if (ascii_case_insensitive_equal(name, "n-")) {
        return {.ok = true, .a = 1, .requires_trailing_offset = true};
    }
    if (ascii_case_insensitive_equal(name, "-n-")) {
        return {.ok = true, .a = -1, .requires_trailing_offset = true};
    }
    if (ascii_case_insensitive_starts_with(name, "n-")) {
        if (const auto digits = parse_digit_run(name.substr(2))) {
            return {.ok = true, .a = 1, .embeds_b = true, .embedded_b = -*digits};
        }
    }
    if (ascii_case_insensitive_starts_with(name, "-n-")) {
        if (const auto digits = parse_digit_run(name.substr(3))) {
            return {.ok = true, .a = -1, .embeds_b = true, .embedded_b = -*digits};
        }
    }
    return {};
}

// Productions 4/7/13: the "An" part spelled as a <dimension-token> -
// "<int>n" (coefficient <int>, CSS Syntax's own "type flag" must be
// integer - a fractional magnitude like "3.5n" is NOT a valid
// <n-dimension> at all), "<int>n-" (REQUIRING a trailing signless
// integer), or "<int>n-<digits>" (B already embedded, always
// negative).
[[nodiscard]] an_shape match_an_dimension(std::string_view number_part,
                                          std::string_view unit_part) noexcept {
    if (!number_lexeme_is_integer(number_part)) {
        return {};
    }
    const long long a = decode_anb_integer(number_part);
    if (ascii_case_insensitive_equal(unit_part, "n")) {
        return {.ok = true, .a = a};
    }
    if (ascii_case_insensitive_equal(unit_part, "n-")) {
        return {.ok = true, .a = a, .requires_trailing_offset = true};
    }
    if (ascii_case_insensitive_starts_with(unit_part, "n-")) {
        if (const auto digits = parse_digit_run(unit_part.substr(2))) {
            return {.ok = true, .a = a, .embeds_b = true, .embedded_b = -*digits};
        }
    }
    return {};
}

struct offset_outcome {
    bool ok = false;
    long long b = 0;
    std::size_t next_index = 0;
    gltfx_gfss_diagnostic diagnostic{};
};

// `tokens[sign_index]` is a lone '+'/'-' delim already committed to as
// the start of an offset (productions 11/12/14/15/16's own SEPARATED
// sign, e.g. "n+ 1", "n + 1", "n - 1") - whitespace is permitted on
// either side of it (CSS Syntax's own SS6.1). Whatever follows MUST be
// a SIGNLESS integer (no sign character of its own - "n+ +1" is not a
// production this grammar has), or this reproves with anb_offset,
// never silently falling back to "no offset" (unlike match_optional_
// offset() below: once a sign delim is seen, an offset is no longer
// optional).
[[nodiscard]] offset_outcome match_signless_offset_after_sign(const token_vector &tokens,
                                                              std::size_t sign_index,
                                                              char sign) noexcept {
    std::size_t idx = sign_index + 1;
    skip_whitespace(tokens, idx);
    const gltfx_gfss_token &tok = tokens[idx];
    const bool is_signless_integer = tok.kind == gltfx_gfss_token_kind::number &&
                                     !lexeme_has_explicit_sign(tok.lexeme) &&
                                     number_lexeme_is_integer(tok.lexeme);
    if (!is_signless_integer) {
        return {.ok = false,
                .b = 0,
                .next_index = 0,
                .diagnostic = make_diagnostic(tokens[sign_index], k_expected_anb_offset)};
    }
    const long long magnitude = decode_anb_integer(tok.lexeme);
    return {.ok = true, .b = (sign == '+') ? magnitude : -magnitude, .next_index = idx + 1};
}

// Productions 5/6/10/11/12/16's own OPTIONAL offset - `index` already
// points PAST any whitespace after the "An" part. A bare "n"/"-n"/
// <n-dimension> may stand alone (b implicitly 0, `next_index` left
// UNCHANGED at `index` so the caller's own require_end() decides
// whether what follows is legitimately nothing or trailing garbage);
// a <signed-integer> number token (e.g. "n+1", the sign merged into
// the digits by the tokenizer itself) or a bare '+'/'-' delim (e.g.
// "n + 1", handed to match_signless_offset_after_sign() above) both
// commit to an offset.
[[nodiscard]] offset_outcome match_optional_offset(const token_vector &tokens,
                                                   std::size_t index) noexcept {
    const gltfx_gfss_token &tok = tokens[index];
    if (tok.kind == gltfx_gfss_token_kind::number && lexeme_has_explicit_sign(tok.lexeme)) {
        // A signed number token here has already COMMITTED to being an
        // offset (its own '+'/'-' is proof of intent) - a fractional
        // one, e.g. "n+1.5", is a malformed OFFSET, never merely
        // trailing garbage to leave for require_end() to name instead.
        if (!number_lexeme_is_integer(tok.lexeme)) {
            return {.ok = false,
                    .b = 0,
                    .next_index = 0,
                    .diagnostic = make_diagnostic(tok, k_expected_anb_offset)};
        }
        return {.ok = true, .b = decode_anb_integer(tok.lexeme), .next_index = index + 1};
    }
    if (tok.kind == gltfx_gfss_token_kind::delim && tok.lexeme.size() == 1 &&
        (tok.lexeme.front() == '+' || tok.lexeme.front() == '-')) {
        return match_signless_offset_after_sign(tokens, index, tok.lexeme.front());
    }
    return {.ok = true, .b = 0, .next_index = index};
}

// Productions 13/14/15's own MANDATORY trailing signless integer - the
// "An" part already ended in a bare "-" ("n-"/"-n-"/"<int>n-"), so an
// offset here is NEVER optional (unlike match_optional_offset()
// above): a "-" with nothing valid after it is always malformed,
// reported at `an_token` itself (the offset's own sign was already
// spent inside the An part's own lexeme, so there is no separate sign
// token to point at).
[[nodiscard]] offset_outcome
match_mandatory_trailing_offset(const token_vector &tokens, std::size_t index,
                                const gltfx_gfss_token &an_token) noexcept {
    std::size_t idx = index;
    skip_whitespace(tokens, idx);
    const gltfx_gfss_token &tok = tokens[idx];
    const bool is_signless_integer = tok.kind == gltfx_gfss_token_kind::number &&
                                     !lexeme_has_explicit_sign(tok.lexeme) &&
                                     number_lexeme_is_integer(tok.lexeme);
    if (!is_signless_integer) {
        return {.ok = false,
                .b = 0,
                .next_index = 0,
                .diagnostic = make_diagnostic(an_token, k_expected_anb_offset)};
    }
    return {.ok = true, .b = -decode_anb_integer(tok.lexeme), .next_index = idx + 1};
}

[[nodiscard]] anb_parse_result finish_ok(gfss_anb value) noexcept {
    return {.ok = true, .value = value, .diagnostic = {}};
}

[[nodiscard]] anb_parse_result finish_fail(const gltfx_gfss_diagnostic &diagnostic) noexcept {
    return {.ok = false, .value = {}, .diagnostic = diagnostic};
}

// A complete An+B value must consume the WHOLE of the caller's own
// text - anything left over past `index` (after skipping whitespace)
// is trailing garbage, never silently ignored (anb_parse.hpp's own
// header comment).
[[nodiscard]] std::optional<gltfx_gfss_diagnostic> require_end(const token_vector &tokens,
                                                               std::size_t index) noexcept {
    std::size_t idx = index;
    skip_whitespace(tokens, idx);
    if (tokens[idx].kind != gltfx_gfss_token_kind::eof) {
        return make_diagnostic(tokens[idx], k_expected_end_of_anb_expression);
    }
    return std::nullopt;
}

// Shared tail for every "An" shape (bare ident, dimension, or the
// '+'-prefixed ident below) - dispatches on which of the three mutually
// exclusive continuations this file's own an_shape documents (embedded
// B, mandatory trailing offset, or optional offset), then requires the
// whole argument to be consumed.
[[nodiscard]] anb_parse_result finish_an_shape(const token_vector &tokens, std::size_t an_index,
                                               const gltfx_gfss_token &an_token,
                                               const an_shape &shape) noexcept {
    if (shape.embeds_b) {
        if (const auto trailing = require_end(tokens, an_index + 1)) {
            return finish_fail(*trailing);
        }
        return finish_ok({.a = shape.a, .b = shape.embedded_b});
    }
    if (shape.requires_trailing_offset) {
        const offset_outcome offset =
            match_mandatory_trailing_offset(tokens, an_index + 1, an_token);
        if (!offset.ok) {
            return finish_fail(offset.diagnostic);
        }
        if (const auto trailing = require_end(tokens, offset.next_index)) {
            return finish_fail(*trailing);
        }
        return finish_ok({.a = shape.a, .b = offset.b});
    }
    std::size_t offset_index = an_index + 1;
    skip_whitespace(tokens, offset_index);
    const offset_outcome offset = match_optional_offset(tokens, offset_index);
    if (!offset.ok) {
        return finish_fail(offset.diagnostic);
    }
    if (const auto trailing = require_end(tokens, offset.next_index)) {
        return finish_fail(*trailing);
    }
    return finish_ok({.a = shape.a, .b = offset.b});
}

// Productions 5/8/14's own OPTIONAL leading '+' (CSS Syntax's own
// "'+'? n", "'+'? <ndashdigit-ident>", "'+'? n- <signless-integer>") -
// `plus` is that delim; requires an ident IMMEDIATELY adjacent (no gap
// - the SAME adjacency rule every other sigil in this track already
// applies), spelling one of the POSITIVE "n"/"n-"/"n-<digits>" shapes
// only - the grammar has no "+-n" form, so a name that matches but
// yields a NEGATIVE coefficient (an_shape::a < 0, i.e. "-n"/"-n-...")
// is rejected here, not silently accepted.
[[nodiscard]] anb_parse_result
parse_optional_plus_then_ident(const token_vector &tokens, std::size_t index,
                               const gltfx_gfss_token &plus) noexcept {
    const std::size_t name_index = index + 1;
    const bool has_adjacent_ident = name_index < tokens.size() &&
                                    tokens[name_index].kind == gltfx_gfss_token_kind::ident &&
                                    tokens_are_adjacent(plus, tokens[name_index]);
    if (!has_adjacent_ident) {
        return finish_fail(make_diagnostic(plus, k_expected_anb_expression));
    }
    const gltfx_gfss_token &name = tokens[name_index];
    const an_shape shape = match_an_ident(name.lexeme);
    if (!shape.ok || shape.a < 0) {
        return finish_fail(make_diagnostic(name, k_expected_anb_expression));
    }
    return finish_an_shape(tokens, name_index, name, shape);
}

// Dispatches the "An" part starting at `tok` - a leading '+' (the
// optional sigil above), a bare ident ("n"/"-n"/"n-"/"-n-"/their own
// ndashdigit forms), or a <dimension-token> ("<int>n" and its own two
// ndash forms). Anything else is not a valid An+B leading shape at
// all.
[[nodiscard]] anb_parse_result parse_an_and_offset(const token_vector &tokens, std::size_t index,
                                                   const gltfx_gfss_token &tok) noexcept {
    if (tok.kind == gltfx_gfss_token_kind::delim && tok.lexeme == std::string_view{"+"}) {
        return parse_optional_plus_then_ident(tokens, index, tok);
    }
    if (tok.kind == gltfx_gfss_token_kind::ident) {
        const an_shape shape = match_an_ident(tok.lexeme);
        if (!shape.ok) {
            return finish_fail(make_diagnostic(tok, k_expected_anb_expression));
        }
        return finish_an_shape(tokens, index, tok, shape);
    }
    if (tok.kind == gltfx_gfss_token_kind::dimension) {
        const dimension_split split = split_dimension_lexeme(tok.lexeme);
        const an_shape shape = match_an_dimension(split.number_part, split.unit_part);
        if (!shape.ok) {
            return finish_fail(make_diagnostic(tok, k_expected_anb_expression));
        }
        return finish_an_shape(tokens, index, tok, shape);
    }
    return finish_fail(make_diagnostic(tok, k_expected_anb_expression));
}

} // namespace

anb_parse_result parse_anb(std::string_view text) noexcept {
    const token_vector tokens = gltfx_gfss_tokenize(text);
    std::size_t index = 0;
    skip_whitespace(tokens, index);
    const gltfx_gfss_token &tok = tokens[index];

    // Productions 1/2: the two keywords (CSS Syntax's own "the same
    // meaning as 2n and 2n+1" - checked BEFORE the general ident
    // dispatch below, since "odd"/"even" are not An-part shapes at
    // all).
    if (tok.kind == gltfx_gfss_token_kind::ident &&
        ascii_case_insensitive_equal(tok.lexeme, "odd")) {
        if (const auto trailing = require_end(tokens, index + 1)) {
            return finish_fail(*trailing);
        }
        return finish_ok({.a = 0, .b = 1});
    }
    if (tok.kind == gltfx_gfss_token_kind::ident &&
        ascii_case_insensitive_equal(tok.lexeme, "even")) {
        if (const auto trailing = require_end(tokens, index + 1)) {
            return finish_fail(*trailing);
        }
        return finish_ok({.a = 2, .b = 0});
    }
    // Production 3: a bare <integer> - a=0, "every 0th element plus B"
    // collapses to "only the B-th".
    if (tok.kind == gltfx_gfss_token_kind::number) {
        if (!number_lexeme_is_integer(tok.lexeme)) {
            return finish_fail(make_diagnostic(tok, k_expected_anb_expression));
        }
        const long long b = decode_anb_integer(tok.lexeme);
        if (const auto trailing = require_end(tokens, index + 1)) {
            return finish_fail(*trailing);
        }
        return finish_ok({.a = 0, .b = b});
    }
    return parse_an_and_offset(tokens, index, tok);
}

} // namespace glintfx::style::detail
