// SPDX-License-Identifier: AGPL-3.0-or-later
#include "numeric_lexeme.hpp"

#include <algorithm>
#include <cassert>
#include <charconv>
#include <limits>

// numeric_lexeme.cpp - see numeric_lexeme.hpp's own header comment for
// why this file exists and why every line below is a PURE MOVE from
// color_parse.cpp (GODS_LAWS.md L-17/L-27), not a rewrite. The
// anonymous namespace below is the SAME supporting machinery that used
// to live inside color_parse.cpp's own anonymous namespace - still
// private to this one translation unit, since nothing outside this
// file ever needs most_significant_digit_place()/split_lexeme_at_
// exponent_marker()/parse_saturating_exponent()/
// saturate_out_of_range_number() on their own; only the two functions
// numeric_lexeme.hpp declares (decode_number_lexeme()/decode_
// percentage_lexeme()) widen from anonymous-namespace linkage to named
// (still hidden, still not GLINTFX_API) linkage, because THOSE are what
// value_parse.cpp now needs to call too.

namespace glintfx::style::detail {

namespace {

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
        // clamp_0_255_to_byte()/clamp_unit_to_byte() already reach
        // through std::clamp() for an in-range-but-too-large double.
        // lowest()/max(), never +-infinity: hsl_to_rgb_unit()'s own
        // std::fmod(hue_deg, 360.0) is a DOMAIN ERROR (NaN) for an
        // infinite hue_deg, but well-defined and FINITE for the
        // largest finite double - measured with this project's own
        // probe before being trusted here.
        return is_negative ? std::numeric_limits<double>::lowest()
                           : std::numeric_limits<double>::max();
    }
    return is_negative ? -0.0 : 0.0; // underflow: rounds toward zero, never the extreme.
}

} // namespace

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
// deliberately NOT implemented in the tokenizer layer, every consumer
// above it does its own decoding).
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

} // namespace glintfx::style::detail
