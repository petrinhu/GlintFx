// SPDX-License-Identifier: AGPL-3.0-or-later
#include "color_parse.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>

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

// SECOND CORRECTION HERE (GODS_LAWS.md L-27/L-40 - an ADVERSARIAL
// review against the FIRST fix below, in decode_number_lexeme()'s own
// comment, found this one): that fix's own comment claimed the shared
// bound below "never changes which SIDE the sum lands on" - MEASURED
// FALSE (this project's own probe, GODS_LAWS.md L-27). A mantissa
// digit run long enough to clamp most_significant_digit_place()'s
// return value to EXACTLY the bound's negative extreme, summed with an
// exponent digit run clamped by parse_saturating_exponent() to EXACTLY
// the SAME bound's positive extreme, landed the sum on EXACTLY ZERO -
// the TRUE, un-clamped sum was nowhere near zero (a genuine, deep
// underflow), but clamping BOTH summands to the SAME bound BEFORE
// adding them threw that fact away, and the sign check
// (saturate_out_of_range_number()'s own `>= 0`) then picked the WRONG
// side. Three real reproductions of exactly this collision are this
// fatia's own gfss_color_parse_test.cpp.
//
// THE FIX IS TWO BOUNDS, NOT A BIGGER SINGLE ONE - GODS_LAWS.md L-40's
// own review instruction ("compare as magnitudes verdadeiras, ou some
// numa aritmetica que nao perca a informacao") ruled out the naive
// fix: a bigger SHARED bound is attackable by a proportionally bigger
// digit run on BOTH sides at once, the SAME collision at a new size.
// Decoupling the two bounds by six orders of magnitude closes the
// collision BY CONSTRUCTION - neither side's REALISTICALLY achievable
// range can ever equal the other's, so they can never land on the
// same magnitude with opposite sign no matter how long an attacker
// makes either digit run:
//
//   - k_mantissa_place_bound only guards most_significant_digit_place()
//     below against signed-integer wraparound converting
//     std::string_view::size() (unsigned) to long long. It is NOT
//     meant to ever be reached by a real mantissa: `mantissa` is a
//     substring of ONE gfss color value's source text, bounded by
//     whatever fits in this process's own address space - many orders
//     of magnitude below this bound. For every input this project can
//     actually receive, most_significant_digit_place() below returns
//     the mantissa's TRUE, un-clamped place value, never an
//     artificially narrowed one.
//   - k_exponent_overflow_magnitude is the sentinel
//     parse_saturating_exponent() below substitutes ONLY when the
//     EXPONENT's OWN digit run overflows long long on ITS OWN
//     std::from_chars() call (roughly 19+ significant digits - reachable
//     with a few dozen characters of TEXT, unlike the mantissa side).
//     Chosen with a margin wide enough above k_mantissa_place_bound
//     that adding the two, at EITHER one's own worst case, can never
//     cross zero from the wrong side, and can never overflow long long
//     either (k_mantissa_place_bound + k_exponent_overflow_magnitude
//     stays well inside long long's own range) - the sentinel therefore
//     ALWAYS dominates the sign of the sum whenever it fires.
constexpr long long k_mantissa_place_bound = std::numeric_limits<long long>::max() / 4;
constexpr long long k_exponent_overflow_magnitude = std::numeric_limits<long long>::max() / 2;

// The place value (power of ten) of the first nonzero digit in
// `mantissa` (a digit run with at most one '.', no sign, no exponent
// letter - split_lexeme_at_exponent_marker()'s own mantissa half).
// "120.045" -> 2 (the '1' sits in the hundreds place); "0.0045" -> -3.
// A mantissa that is entirely zero digits never reaches this function
// in practice (std::from_chars() already succeeds trivially for an
// exact zero, so decode_number_lexeme() below never takes the
// out-of-range branch for one) - the fallback below only guards a case
// this project's own grammar cannot produce, and picks the SAFE
// direction (underflow, never a fabricated overflow) if it somehow did.
//
// Returns the TRUE place value for every mantissa this project can
// actually receive (k_mantissa_place_bound's own header comment above:
// the clamp below exists ONLY to guard the size_t-to-long long cast
// against wraparound, and sits so far above any realistic digit run
// that it never fires in practice) - deliberately NOT the same bound
// parse_saturating_exponent() below saturates to, per this file's own
// SECOND CORRECTION comment above.
long long most_significant_digit_place(std::string_view mantissa) noexcept {
    const std::size_t dot = mantissa.find('.');
    const std::size_t whole_digit_count = dot == std::string_view::npos ? mantissa.size() : dot;
    for (std::size_t i = 0; i < mantissa.size(); ++i) {
        if (mantissa[i] == '.' || mantissa[i] == '0') {
            continue;
        }
        const long long place =
            i < whole_digit_count
                ? static_cast<long long>(whole_digit_count - 1 - i)
                : static_cast<long long>(whole_digit_count) - static_cast<long long>(i);
        return std::clamp(place, -k_mantissa_place_bound, k_mantissa_place_bound);
    }
    return -k_mantissa_place_bound;
}

// One <number-token>/<percentage-token> body, split at its own "e"/"E"
// marker (lexical_rules.cpp's own consume_optional_exponent()) into a
// mantissa (keeps its own optional '.') and an exponent (digits only,
// sign lifted into `exponent_is_negative` - std::from_chars's INTEGER
// overload has the SAME "no leading +" restriction the double overload
// does, so the sign cannot simply be left in front of the digits).
struct mantissa_and_exponent {
    std::string_view mantissa;
    std::string_view exponent_digits; // empty: no explicit "e..." suffix
    bool exponent_is_negative = false;
};

mantissa_and_exponent split_lexeme_at_exponent_marker(std::string_view unsigned_lexeme) noexcept {
    const std::size_t marker = unsigned_lexeme.find_first_of("eE");
    if (marker == std::string_view::npos) {
        return mantissa_and_exponent{
            .mantissa = unsigned_lexeme, .exponent_digits = {}, .exponent_is_negative = false};
    }
    std::string_view rest = unsigned_lexeme.substr(marker + 1);
    const bool is_negative = !rest.empty() && rest.front() == '-';
    if (!rest.empty() && (rest.front() == '+' || rest.front() == '-')) {
        rest.remove_prefix(1);
    }
    return mantissa_and_exponent{.mantissa = unsigned_lexeme.substr(0, marker),
                                 .exponent_digits = rest,
                                 .exponent_is_negative = is_negative};
}

// Parses `digits` (already known all-ASCII-digit by this project's own
// tokenizer grammar) into a signed magnitude, saturating instead of
// failing. UNLIKE most_significant_digit_place() above, this function's
// own saturation bound (k_exponent_overflow_magnitude) is chosen to be
// far LARGER than any mantissa place value this project can actually
// produce - per this file's own SECOND CORRECTION comment above, that
// asymmetry is the fix: it means this function's own saturated result
// ALWAYS dominates the sign of the sum saturate_out_of_range_number()
// below computes, rather than landing on the SAME bound
// most_significant_digit_place() might independently saturate to.
long long parse_saturating_exponent(std::string_view digits, bool is_negative) noexcept {
    long long magnitude = 0;
    const std::from_chars_result result =
        std::from_chars(digits.data(), digits.data() + digits.size(), magnitude);
    if (result.ec == std::errc::result_out_of_range) {
        magnitude = k_exponent_overflow_magnitude;
    }
    magnitude = std::clamp(magnitude, 0LL, k_exponent_overflow_magnitude);
    return is_negative ? -magnitude : magnitude;
}

// std::from_chars() rejected the DOUBLE conversion of `unsigned_lexeme`
// (decode_number_lexeme() below already stripped any leading '+'/'-')
// as errc::result_out_of_range - true for BOTH directions this
// project's own tokenizer's unbounded exponent digit run
// (lexical_rules.cpp's own consume_optional_exponent()) can produce:
// "1e400" (too big) and "1e-400" (too small, rounds to zero) - VERIFIED
// against this project's own libstdc++ to return the SAME ec for both
// (GODS_LAWS.md L-27: measured, not assumed). Distinguished the SAME
// way scientific notation itself is - by the SIGN of the combined
// decimal exponent (the mantissa's own most-significant-digit place,
// PLUS any explicit "e..." suffix) - never by inspecting `value`,
// which std::from_chars left untouched.
//
// The sign check below (`>= 0`) is only as trustworthy as the two
// summands feeding it - this file's own SECOND CORRECTION comment
// above (k_mantissa_place_bound / k_exponent_overflow_magnitude)
// exists SPECIFICALLY so this addition can never land on a false zero:
// mantissa_place is the mantissa's TRUE place value for every input
// this project can actually receive, and explicit_exponent, whenever
// it IS saturated, saturates to a sentinel far too large for
// mantissa_place to ever cancel.
double saturate_out_of_range_number(std::string_view unsigned_lexeme, bool is_negative) noexcept {
    const mantissa_and_exponent split = split_lexeme_at_exponent_marker(unsigned_lexeme);
    const long long mantissa_place = most_significant_digit_place(split.mantissa);
    const long long explicit_exponent =
        split.exponent_digits.empty()
            ? 0
            : parse_saturating_exponent(split.exponent_digits, split.exponent_is_negative);
    const bool magnitude_is_at_least_one = mantissa_place + explicit_exponent >= 0;
    if (magnitude_is_at_least_one) {
        // CSS Color 4 SS4/13's own "the used value... MUST be clamped"
        // rule (color_parse.hpp's own scope-cut 5) extended to a value
        // from_chars declined to produce at all - the SAME extreme
        // clamp_0_255_to_byte()/clamp_unit_to_byte() below already
        // reach through std::clamp() for an in-range-but-too-large
        // double. lowest()/max(), never +-infinity:
        // hsl_to_rgb_unit()'s own std::fmod(hue_deg, 360.0) is a
        // DOMAIN ERROR (NaN) for an infinite hue_deg, but well-defined
        // and FINITE for the largest finite double - measured with
        // this project's own probe before being trusted here.
        return is_negative ? std::numeric_limits<double>::lowest()
                           : std::numeric_limits<double>::max();
    }
    return is_negative ? -0.0 : 0.0; // underflow: rounds toward zero, never the extreme.
}

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
// CORRECTED HERE (GODS_LAWS.md L-27/L-40 - a CRITICAL review finding
// against an EARLIER version of this function): that version claimed
// from_chars SUCCEEDING was an invariant of this project's own
// tokenizer - true for the SYNTAX (every lexeme this file's own
// tokenizer produces IS a well-formed floating-point literal), false
// for the VALUE: lexical_rules.cpp's own consume_optional_exponent()
// does not cap the exponent digit run, so "1e400" is syntactically
// valid gfss that overflows double, and from_chars reports that
// truthfully as errc::result_out_of_range instead of fabricating a
// value. `assert()`-ing on that branch would abort the CONSUMER's
// process on ordinary external text - this project's own "the library
// never aborts the consumer's process" principle (ESCOPO.md SS2,
// CORE-ERROR decision 1) extends past out-of-memory to hostile
// external text just as directly. The branch below saturates instead,
// per color_parse.hpp's own scope-cut 5 ("out-of-range components are
// clamped, never rejected") - the SAME rule that already governs an
// in-range-but-too-large value like rgb(300, -10, 0).
double decode_number_lexeme(std::string_view lexeme) noexcept {
    std::string_view digits = lexeme;
    if (!digits.empty() && digits.front() == '+') {
        digits.remove_prefix(1);
    }
    double value = 0.0;
    const std::from_chars_result result =
        std::from_chars(digits.data(), digits.data() + digits.size(), value);
    if (result.ec == std::errc::result_out_of_range) {
        const bool is_negative = !digits.empty() && digits.front() == '-';
        return saturate_out_of_range_number(is_negative ? digits.substr(1) : digits, is_negative);
    }
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

// The three OUT parameters read_color_component() below fills on
// every call, success or failure, grouped into one struct -
// CONTRACT.md SS6.2's own 4-parameter ceiling (GODS_LAWS.md L-17): the
// bare function used to take `cursor` and `requirement` PLUS three
// more out-parameters, five in total. `component` and `token` are
// BOTH needed on a SUCCESSFUL call (the caller's own
// rgb_component_type_matches_first() check needs the token's own
// line/column to build a k_color_expected_uniform_component_types
// diagnostic if a LATER argument fails the uniform-type check, and by
// then read_color_component() has already moved on to that later
// argument's own token) - `failure` is the only field that only
// matters on the false path.
struct color_component_read {
    color_component component{};
    gltfx_gfss_token token{};
    gltfx_gfss_diagnostic failure{};
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
bool read_color_component(gltfx_gfss_cursor &cursor, component_kind_requirement requirement,
                          color_component_read &out) noexcept {
    if (!next_significant_token(cursor, out.token)) {
        out.failure = make_diagnostic(out.token, k_color_expected_argument_count);
        return false;
    }
    const bool accepts_number = requirement != component_kind_requirement::percentage_only;
    const bool accepts_percentage = requirement != component_kind_requirement::number_only;
    if (accepts_number && out.token.kind == gltfx_gfss_token_kind::number) {
        out.component = color_component{.value = decode_number_lexeme(out.token.lexeme),
                                        .is_percentage = false};
        return true;
    }
    if (accepts_percentage && out.token.kind == gltfx_gfss_token_kind::percentage) {
        out.component = color_component{.value = decode_percentage_lexeme(out.token.lexeme),
                                        .is_percentage = true};
        return true;
    }
    const std::string_view expected = requirement == component_kind_requirement::number_only
                                          ? k_color_expected_number
                                      : requirement == component_kind_requirement::percentage_only
                                          ? k_color_expected_percentage
                                          : k_color_expected_number_or_percentage;
    out.failure = make_diagnostic(out.token, expected);
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

        color_component_read read{};
        if (!read_color_component(cursor, requirement, read)) {
            return fail(read.failure);
        }
        components[i] = read.component;
        if (!is_hsl_family && i > 0 && i < 3 && !rgb_component_type_matches_first(components, i)) {
            return fail(make_diagnostic(read.token, k_color_expected_uniform_component_types));
        }
        gltfx_gfss_diagnostic separator_failure{};
        if (!read_component_separator(cursor, i, required_args, separator_failure)) {
            return fail(separator_failure);
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
