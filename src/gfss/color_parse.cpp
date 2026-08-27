// SPDX-License-Identifier: AGPL-3.0-or-later
#include "color_parse.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <charconv>
#include <cmath>
#include <cstdint>

#include <glintfx/gfss/tokenizer.hpp>

#include "code_point.hpp"
#include "color_diagnostic_vocabulary.hpp"
#include "named_colors.hpp"

// color_parse.cpp - GFSS-COLOR-PARSE (TODO.md, GODS_LAWS.md L-17: each
// function below answers exactly one question of the grammar
// color_parse.hpp's own header comment scopes - hex literal, named
// keyword, or one of the four legacy functions, never more than one
// per function).

namespace glintfx::style::detail {

namespace {

// --- shared plumbing -------------------------------------------------

gltfx_gfss_diagnostic make_diagnostic(const gltfx_gfss_token &token,
                                      std::string_view expected) noexcept {
    return gltfx_gfss_diagnostic{.line = token.line, .column = token.column, .expected = expected};
}

color_parse_result fail(const gltfx_gfss_diagnostic &diagnostic) noexcept {
    return color_parse_result{.ok = false, .value = {}, .diagnostic = diagnostic};
}

color_parse_result fail_at(const gltfx_gfss_token &token, std::string_view expected) noexcept {
    return fail(make_diagnostic(token, expected));
}

color_parse_result succeed(gltfx_rgba8 encoded) noexcept {
    return color_parse_result{
        .ok = true, .value = glintfx::gltfx_rgba_from_srgb8(encoded), .diagnostic = {}};
}

// Reads the next token that is not <whitespace-token>, mirroring
// gltfx_gfss_next_token()'s own true/false convention (true while
// there is more; false the moment `out_token` IS the <EOF-token>) -
// color syntax has no construct where whitespace itself is
// significant (unlike, say, a future GFSS-VALUE-level shorthand),
// so every caller in this file wants exactly this skip.
bool next_significant_token(gltfx_gfss_cursor &cursor, gltfx_gfss_token &out_token) noexcept {
    bool more = true;
    do {
        more = gltfx_gfss_next_token(cursor, out_token);
    } while (more && out_token.kind == gltfx_gfss_token_kind::whitespace);
    return more;
}

// --- number decoding ---------------------------------------------------

// Converts a <number-token>/<percentage-token> lexeme (WITHOUT the
// trailing '%' for a percentage - callers strip it first) to a
// double. std::from_chars(..., chars_format::general) implements a
// SUPERSET of CSS Syntax Module Level 3's own "consume a number"
// grammar (4.3.12) for every lexeme THIS project's OWN tokenizer can
// produce, with exactly one gap: std::from_chars does not accept a
// leading '+' on the mantissa (only inside an exponent), while CSS
// numbers do ("+1" is valid gfss) - stripped here before the call, the
// ONE piece of "4.3.13 Convert a string to a number" this project
// writes itself (token.hpp's own header comment: 4.3.13 is
// deliberately NOT implemented in the tokenizer layer, GFSS-COLOR-PARSE
// does its own decoding).
//
// noexcept, INFALLIBLE for every lexeme gltfx_gfss_next_token()
// actually emits as a <number-token>/<percentage-token> - the grammar
// containment argument: every code point sequence consume_number()
// (lexical_rules.cpp) can produce is already a well-formed
// standard-library floating-point literal once a leading '+' is
// stripped, so from_chars succeeding is an INVARIANT of this project's
// own tokenizer, not user input to validate a second time - the
// assert below documents that invariant instead of inventing a dead
// error path color_diagnostic_vocabulary.hpp would have no test able
// to reach (GODS_LAWS.md L-40's own "isto e testado" corollary).
double decode_number_lexeme(std::string_view lexeme) noexcept {
    std::string_view digits = lexeme;
    if (!digits.empty() && digits.front() == '+') {
        digits.remove_prefix(1);
    }
    double value = 0.0;
    // [[maybe_unused]]: the ONLY consumer of `result` is the assert()
    // below, which itself compiles to nothing in a build with NDEBUG
    // defined (this project's own Release default) - without this
    // attribute, THAT build (not Debug) would warn "set but not used"
    // and fail this project's own -Werror gate (GODS_LAWS.md L-23).
    [[maybe_unused]] const std::from_chars_result result =
        std::from_chars(digits.data(), digits.data() + digits.size(), value);
    assert(result.ec == std::errc{} && result.ptr == digits.data() + digits.size() &&
           "decode_number_lexeme(): lexeme was not produced by this project's own tokenizer as a "
           "well-formed <number-token>/<percentage-token> body");
    return value;
}

double decode_percentage_lexeme(std::string_view lexeme) noexcept {
    return decode_number_lexeme(lexeme.substr(0, lexeme.size() - 1)); // drop trailing '%'
}

// --- byte quantization ---------------------------------------------

// A <number> color component (0-255 domain) - clamps first, CSS Color
// 4 SS4/13's "the used value MUST be clamped" rule (color_parse.hpp's
// own scope-cut 5), the SAME convention core/color.hpp's own
// gltfx_rgba_to_srgb8() already applies at its own boundary.
std::uint8_t clamp_0_255_to_byte(double value) noexcept {
    return static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0, 255.0)));
}

// A unit fraction (0-1 domain: a <percentage> color component already
// divided by 100, or an <alpha-value>) - clamped THEN scaled, same
// rule.
std::uint8_t clamp_unit_to_byte(double unit) noexcept {
    return static_cast<std::uint8_t>(std::lround(std::clamp(unit, 0.0, 1.0) * 255.0));
}

std::uint8_t rgb_component_to_byte(double value, bool is_percentage) noexcept {
    return is_percentage ? clamp_unit_to_byte(value / 100.0) : clamp_0_255_to_byte(value);
}

// <alpha-value> ::= <number> [0,1] | <percentage> [0%,100%] - UNLIKE
// an rgb() color component, a plain <number> alpha is ALREADY a 0-1
// unit fraction, never a 0-255 domain value (CSS Color 4 SS13.3's own
// alpha grammar) - this is why alpha does not reuse
// rgb_component_to_byte() above even though both branch on the same
// `is_percentage` flag.
std::uint8_t alpha_component_to_byte(double value, bool is_percentage) noexcept {
    return clamp_unit_to_byte(is_percentage ? value / 100.0 : value);
}

// --- hex literal -----------------------------------------------------

std::uint8_t expand_hex_nibble(std::uint8_t nibble) noexcept {
    return static_cast<std::uint8_t>(nibble * 17U); // 0x/hex digit duplicated: 0xf -> 0xff
}

std::uint8_t hex_byte(std::uint8_t high_nibble, std::uint8_t low_nibble) noexcept {
    return static_cast<std::uint8_t>((high_nibble << 4U) | low_nibble);
}

// Precondition: every character of `hex_digit` already passed
// code_point.hpp's own is_hex_digit() - callers of this function check
// that first, at the point where a per-character diagnostic (line +
// column of the OFFENDING digit, not the token) would matter; this
// helper only ever converts what is already known-valid.
std::uint8_t hex_nibble_value(char hex_digit) noexcept {
    if (hex_digit >= '0' && hex_digit <= '9') {
        return static_cast<std::uint8_t>(hex_digit - '0');
    }
    if (hex_digit >= 'a' && hex_digit <= 'f') {
        return static_cast<std::uint8_t>(hex_digit - 'a' + 10);
    }
    assert(hex_digit >= 'A' && hex_digit <= 'F' &&
           "hex_nibble_value(): caller must validate is_hex_digit() first");
    return static_cast<std::uint8_t>(hex_digit - 'A' + 10);
}

bool hex_body_has_valid_length(std::size_t size) noexcept {
    return size == 3 || size == 4 || size == 6 || size == 8;
}

// #RGB/#RGBA (each digit duplicated) or #RRGGBB/#RRGGBBAA - the FOUR
// forms color_parse.hpp's own header comment enumerates. Alpha
// defaults to fully opaque (255) when the body has no alpha digits
// (3 or 6).
gltfx_rgba8 decode_hex_body(std::string_view body) noexcept {
    std::array<std::uint8_t, 8> nibble{};
    for (std::size_t i = 0; i < body.size(); ++i) {
        nibble[i] = hex_nibble_value(body[i]);
    }
    if (body.size() <= 4) {
        const std::uint8_t alpha = body.size() == 4 ? expand_hex_nibble(nibble[3]) : 255U;
        return gltfx_rgba8{.red = expand_hex_nibble(nibble[0]),
                           .green = expand_hex_nibble(nibble[1]),
                           .blue = expand_hex_nibble(nibble[2]),
                           .alpha = alpha};
    }
    const std::uint8_t alpha = body.size() == 8 ? hex_byte(nibble[6], nibble[7]) : 255U;
    return gltfx_rgba8{.red = hex_byte(nibble[0], nibble[1]),
                       .green = hex_byte(nibble[2], nibble[3]),
                       .blue = hex_byte(nibble[4], nibble[5]),
                       .alpha = alpha};
}

color_parse_result parse_hex_color(const gltfx_gfss_token &token) noexcept {
    const std::string_view body = token.lexeme.substr(1); // strip leading '#'
    if (!hex_body_has_valid_length(body.size())) {
        return fail_at(token, k_color_expected_valid_hex_length);
    }
    for (const char ch : body) {
        if (!is_hex_digit(static_cast<int>(static_cast<unsigned char>(ch)))) {
            return fail_at(token, k_color_expected_hex_digit);
        }
    }
    return succeed(decode_hex_body(body));
}

// --- named keyword -----------------------------------------------------

color_parse_result parse_named_color(const gltfx_gfss_token &token) noexcept {
    gltfx_rgba8 value{};
    if (!lookup_named_color(token.lexeme, value)) {
        return fail_at(token, k_color_expected_known_color_keyword);
    }
    return succeed(value);
}

// --- hsl() conversion -------------------------------------------------

struct rgb_unit {
    double red = 0.0;
    double green = 0.0;
    double blue = 0.0;
};

// CSS Color 4's own reference "hsl to rgb" conversion (public, standard
// color math - https://www.w3.org/TR/css-color-4/#hsl-to-rgb, read
// under GODS_LAWS.md L-29, re-derived and checked by hand against
// hsl(0, 100%, 50%) == red before being trusted here, never copied
// CODE from any implementation of it). `hue_deg` is unrestricted (this
// function itself normalizes into [0,360)); `sat_unit`/`light_unit`
// are already-clamped [0,1] fractions - the caller (parse_hsl_color()
// below) owns turning a <percentage> token into that fraction.
rgb_unit hsl_to_rgb_unit(double hue_deg, double sat_unit, double light_unit) noexcept {
    double hue = std::fmod(hue_deg, 360.0);
    if (hue < 0.0) {
        hue += 360.0;
    }
    const double sat = std::clamp(sat_unit, 0.0, 1.0);
    const double light = std::clamp(light_unit, 0.0, 1.0);
    const double a = sat * std::min(light, 1.0 - light);

    auto channel = [&](double n) noexcept -> double {
        const double k = std::fmod(n + hue / 30.0, 12.0);
        return light - a * std::max(-1.0, std::min({k - 3.0, 9.0 - k, 1.0}));
    };
    return rgb_unit{.red = channel(0.0), .green = channel(8.0), .blue = channel(4.0)};
}

// --- rgb()/rgba()/hsl()/hsla() ----------------------------------------

enum class color_function_kind : std::uint8_t { rgb, rgba, hsl, hsla };

bool try_match_color_function_kind(std::string_view name, color_function_kind &out_kind) noexcept {
    struct entry {
        // Default member initializer, not a user-declared constructor
        // (would forfeit aggregate-init below) - the SAME cppcheck
        // uninitMemberVarNoCtor fix named_colors.hpp's own
        // named_color_entry already applies.
        std::string_view name;
        color_function_kind kind = color_function_kind::rgb;
    };
    constexpr std::array<entry, 4> k_functions{{
        {"rgb", color_function_kind::rgb},
        {"rgba", color_function_kind::rgba},
        {"hsl", color_function_kind::hsl},
        {"hsla", color_function_kind::hsla},
    }};
    for (const entry &candidate : k_functions) {
        if (ascii_case_insensitive_equal(candidate.name, name)) {
            out_kind = candidate.kind;
            return true;
        }
    }
    return false;
}

// ESCOPO.md SS4 decision 8 / color_parse.hpp's own scope-cut 6: these
// four are RECOGNIZED CSS color notations this version does not ship -
// oklch() has a dedicated future fatia (GFSS-OKLCH); lab()/lch()/
// oklab() stay out of the v1 scope entirely. Grouped as one predicate
// because color_parse.cpp treats all four identically today (one
// diagnostic identifier, k_color_expected_shipped_color_notation) -
// splitting them apart is GFSS-OKLCH's own decision to make once it
// exists.
bool is_deferred_color_notation(std::string_view name) noexcept {
    constexpr std::array<std::string_view, 4> k_deferred{{"oklch", "lab", "lch", "oklab"}};
    for (const std::string_view candidate : k_deferred) {
        if (ascii_case_insensitive_equal(candidate, name)) {
            return true;
        }
    }
    return false;
}

struct color_component {
    double value = 0.0;
    bool is_percentage = false;
};

// Which token kind(s) a given argument SLOT accepts - hue accepts only
// <number>, saturation/lightness accept only <percentage>, an rgb()
// color component or an alpha value accepts either. A PLAIN BOOLEAN
// pair ("accepts number" / "accepts percentage") would let a caller
// construct the nonsensical "accepts neither" by mistake; this enum
// makes that combination unrepresentable.
enum class component_kind_requirement : std::uint8_t {
    number_or_percentage,
    number_only,
    percentage_only
};

// One argument matching `requirement` - MUTATION-FOUND BUG, FIXED HERE
// (this project's own first real red, captured live before this fix:
// hsl(50%, 50%, 50%) - a PERCENTAGE in the HUE slot - was wrongly
// ACCEPTED, because an earlier version of this function only used its
// "which diagnostic to raise" parameter on the WRONG-TOKEN-KIND
// fallback branch, never to REJECT a token that matched a DIFFERENT,
// still-valid-looking kind. `requirement` is now consulted on EVERY
// branch, not only the last one - a <percentage-token> in a
// number_only slot, or a <number-token> in a percentage_only slot,
// both now fall through to the SAME "neither branch matched" failure
// path a genuinely wrong token kind already took.
//
// `out_token` receives the component token EVEN ON SUCCESS - the
// caller needs its line/column to build a k_color_expected_uniform_
// component_types diagnostic if a LATER check
// (rgb_component_type_matches_first()) fails, and by then this
// function has already moved on to the next argument's own token.
bool read_color_component(gltfx_gfss_cursor &cursor, component_kind_requirement requirement,
                          color_component &out_component, gltfx_gfss_token &out_token,
                          gltfx_gfss_diagnostic &out_failure) noexcept {
    if (!next_significant_token(cursor, out_token)) {
        out_failure = make_diagnostic(out_token, k_color_expected_argument_count);
        return false;
    }
    const bool accepts_number = requirement != component_kind_requirement::percentage_only;
    const bool accepts_percentage = requirement != component_kind_requirement::number_only;
    if (accepts_number && out_token.kind == gltfx_gfss_token_kind::number) {
        out_component = color_component{.value = decode_number_lexeme(out_token.lexeme),
                                        .is_percentage = false};
        return true;
    }
    if (accepts_percentage && out_token.kind == gltfx_gfss_token_kind::percentage) {
        out_component = color_component{.value = decode_percentage_lexeme(out_token.lexeme),
                                        .is_percentage = true};
        return true;
    }
    const std::string_view expected = requirement == component_kind_requirement::number_only
                                          ? k_color_expected_number
                                      : requirement == component_kind_requirement::percentage_only
                                          ? k_color_expected_percentage
                                          : k_color_expected_number_or_percentage;
    out_failure = make_diagnostic(out_token, expected);
    return false;
}

// The separator that must follow argument `index` (0-based) of
// `required_args` total: a comma for every argument but the last, a
// close-paren for the last. Returns false (out_failure set) on
// anything else, including distinguishing "one comma too many" (an
// extra argument) from "the ')' never came".
bool read_component_separator(gltfx_gfss_cursor &cursor, std::size_t index,
                              std::size_t required_args,
                              gltfx_gfss_diagnostic &out_failure) noexcept {
    const bool is_last_argument = index + 1 == required_args;
    gltfx_gfss_token token{};
    if (!next_significant_token(cursor, token)) {
        // Ran out of input: if we already have every required
        // component, the only thing missing is the ')' itself; if we
        // do not, this is the SAME "too few arguments" shape
        // read_color_component() reports when EOF arrives in ITS OWN
        // slot - both roads lead to the SAME diagnostic identifier for
        // the SAME real defect (arity), the paren case is the only
        // one that is not an arity question at all.
        out_failure = make_diagnostic(token, is_last_argument ? k_color_expected_closing_parenthesis
                                                              : k_color_expected_argument_count);
        return false;
    }
    if (is_last_argument) {
        if (token.kind == gltfx_gfss_token_kind::close_paren) {
            return true;
        }
        out_failure = make_diagnostic(token, token.kind == gltfx_gfss_token_kind::comma
                                                 ? k_color_expected_argument_count
                                                 : k_color_expected_closing_parenthesis);
        return false;
    }
    if (token.kind == gltfx_gfss_token_kind::comma) {
        return true;
    }
    out_failure = make_diagnostic(token, k_color_expected_comma);
    return false;
}

// rgb()/rgba()'s own extra rule CSS Color 3 states once for the whole
// function (color_parse.hpp's own scope-cut 3): the three COLOR
// components (index 0/1/2 - red/green/blue) must share ONE type; index
// 3, when present, is alpha and is DELIBERATELY excluded (scope-cut 3:
// "alpha is INDEPENDENT of that rule"). hsl()/hsla() never call this -
// hue is always <number>, saturation/lightness are always
// <percentage>, so there is no "uniform type" question there at all.
// Checks ONLY `components[index]` against `components[0]` - NOT every
// slot up to `index` at once - because a slot beyond `index` has not
// been read from the token stream yet and still holds its
// default-constructed (is_percentage == false) value, which would
// otherwise read as a FALSE type mismatch the moment `components[0]`
// happens to be a percentage.
bool rgb_component_type_matches_first(const std::array<color_component, 4> &components,
                                      std::size_t index) noexcept {
    return components[index].is_percentage == components[0].is_percentage;
}

gltfx_rgba8 rgb_components_to_rgba8(const std::array<color_component, 4> &components,
                                    bool has_alpha) noexcept {
    return gltfx_rgba8{
        .red = rgb_component_to_byte(components[0].value, components[0].is_percentage),
        .green = rgb_component_to_byte(components[1].value, components[1].is_percentage),
        .blue = rgb_component_to_byte(components[2].value, components[2].is_percentage),
        .alpha = has_alpha
                     ? alpha_component_to_byte(components[3].value, components[3].is_percentage)
                     : std::uint8_t{255},
    };
}

gltfx_rgba8 hsl_components_to_rgba8(const std::array<color_component, 4> &components,
                                    bool has_alpha) noexcept {
    const rgb_unit unit = hsl_to_rgb_unit(components[0].value, components[1].value / 100.0,
                                          components[2].value / 100.0);
    return gltfx_rgba8{
        .red = clamp_unit_to_byte(unit.red),
        .green = clamp_unit_to_byte(unit.green),
        .blue = clamp_unit_to_byte(unit.blue),
        .alpha = has_alpha
                     ? alpha_component_to_byte(components[3].value, components[3].is_percentage)
                     : std::uint8_t{255},
    };
}

// Reads every argument of an already-recognized rgb()/rgba()/hsl()/
// hsla() call - `cursor` is positioned right after the opening '(' the
// <function-token> itself consumed (tokenizer.hpp's own convention,
// same as consume_url_or_function_token in tokenizer.cpp). One
// function, one grammar shape (`<comp>, <comp>, <comp> [, <alpha>] )`)
// shared by all four names - the SLOT-specific pieces (which
// diagnostic a wrong token kind raises, whether the three color
// components must share a type, which formula turns them into bytes)
// are the only things that vary, and each is a parameter or a
// dedicated helper above, never a fourth copy of this loop.
color_parse_result parse_function_arguments(gltfx_gfss_cursor &cursor,
                                            color_function_kind kind) noexcept {
    const bool is_hsl_family =
        kind == color_function_kind::hsl || kind == color_function_kind::hsla;
    const bool has_alpha = kind == color_function_kind::rgba || kind == color_function_kind::hsla;
    const std::size_t required_args = has_alpha ? 4 : 3;

    std::array<color_component, 4> components{};
    for (std::size_t i = 0; i < required_args; ++i) {
        const bool is_hue = is_hsl_family && i == 0;
        const bool is_saturation_or_lightness = is_hsl_family && (i == 1 || i == 2);
        const component_kind_requirement requirement =
            is_hue                       ? component_kind_requirement::number_only
            : is_saturation_or_lightness ? component_kind_requirement::percentage_only
                                         : component_kind_requirement::number_or_percentage;

        gltfx_gfss_token component_token{};
        gltfx_gfss_diagnostic failure{};
        if (!read_color_component(cursor, requirement, components[i], component_token, failure)) {
            return fail(failure);
        }
        if (!is_hsl_family && i > 0 && i < 3 && !rgb_component_type_matches_first(components, i)) {
            return fail(make_diagnostic(component_token, k_color_expected_uniform_component_types));
        }
        if (!read_component_separator(cursor, i, required_args, failure)) {
            return fail(failure);
        }
    }

    return succeed(is_hsl_family ? hsl_components_to_rgba8(components, has_alpha)
                                 : rgb_components_to_rgba8(components, has_alpha));
}

color_parse_result parse_function_color(gltfx_gfss_cursor &cursor,
                                        const gltfx_gfss_token &function_token) noexcept {
    const std::string_view name = function_token.lexeme.substr(0, function_token.lexeme.size() - 1);

    color_function_kind kind{};
    if (!try_match_color_function_kind(name, kind)) {
        return fail_at(function_token, is_deferred_color_notation(name)
                                           ? k_color_expected_shipped_color_notation
                                           : k_color_expected_known_color_function);
    }
    return parse_function_arguments(cursor, kind);
}

} // namespace

color_parse_result parse_color(std::string_view text) noexcept {
    gltfx_gfss_cursor cursor{.source = text};
    gltfx_gfss_token token{};
    if (!next_significant_token(cursor, token)) {
        return fail_at(token, k_color_expected_color_value);
    }

    // A <bad-string-token>/<bad-url-token> already carries a diagnostic
    // from tokenizer.cpp's own vocabulary (diagnostic_vocabulary.hpp) -
    // propagated as-is rather than masked behind this file's generic
    // "not a color value" identifier, the same layering principle
    // token.hpp's own header comment documents for these two recovery
    // classes.
    if (token.kind == gltfx_gfss_token_kind::bad_string ||
        token.kind == gltfx_gfss_token_kind::bad_url) {
        return fail(token.diagnostic);
    }

    color_parse_result value{};
    if (token.kind == gltfx_gfss_token_kind::hash) {
        value = parse_hex_color(token);
    } else if (token.kind == gltfx_gfss_token_kind::ident) {
        value = parse_named_color(token);
    } else if (token.kind == gltfx_gfss_token_kind::function) {
        value = parse_function_color(cursor, token);
    } else {
        return fail_at(token, k_color_expected_color_value);
    }
    if (!value.ok) {
        return value;
    }

    gltfx_gfss_token trailing{};
    if (next_significant_token(cursor, trailing)) {
        return fail_at(trailing, k_color_expected_no_trailing_content);
    }
    return value;
}

} // namespace glintfx::style::detail
