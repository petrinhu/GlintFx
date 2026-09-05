// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstddef>
#include <cstdint>
#include <print>
#include <string>
#include <string_view>

#include <glintfx/core/color.hpp>

#include "gfss/color_diagnostic_vocabulary.hpp"
#include "gfss/color_parse.hpp"
#include "gfss/diagnostic_vocabulary.hpp"
#include "gfss/named_colors.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// gfss_color_parse_test.cpp - GFSS-COLOR-PARSE (TODO.md, GODS_LAWS.md
// L-20/L-40): the TDD red/green witness for
// glintfx::style::detail::parse_color() (color_parse.hpp), the 149-row
// named color table (named_colors.hpp), and this fatia's OWN closed
// diagnostic vocabulary (color_diagnostic_vocabulary.hpp) - see each of
// those three files' own header comment for the design rationale each
// check below proves.
//
// ROUND-TRIP CHECK TECHNIQUE, not a hardcoded linear-space float per
// case (the SAME method color_test.cpp's own
// gltfx_rgba_srgb8_round_trip_covers_every_byte_value already proves
// exact for every one of the 256 byte values): every successful
// parse_color() result is converted BACK to gltfx_rgba8 via
// glintfx::gltfx_rgba_to_srgb8() and compared byte-for-byte against
// the EXPECTED sRGB8 value - never a hand-computed linear-light float
// literal, which would silently re-derive whatever
// gltfx_rgba_from_srgb8() itself does instead of checking it.

namespace {

using glintfx::style::detail::color_parse_result;
using glintfx::style::detail::parse_color;

// The one shared assertion every successful-parse test needs: `result`
// is ok, and round-tripping its value back to sRGB8 reproduces
// `expected` exactly.
void check_color_result(const color_parse_result &result, glintfx::gltfx_rgba8 expected) {
    GLINTFX_CHECK(result.ok);
    if (!result.ok) {
        return;
    }
    const glintfx::gltfx_rgba8 back = glintfx::gltfx_rgba_to_srgb8(result.value);
    GLINTFX_CHECK_EQ(back.red, expected.red);
    GLINTFX_CHECK_EQ(back.green, expected.green);
    GLINTFX_CHECK_EQ(back.blue, expected.blue);
    GLINTFX_CHECK_EQ(back.alpha, expected.alpha);
}

// The shared assertion every hostile-input test needs: `result`
// failed, with EXACTLY `expected_identifier` as the diagnostic - never
// merely "some error", the same "exact string, not merely non-empty"
// discipline gfss_token_test.cpp's own
// gltfx_gfss_token_kind_name_covers_every_class_with_its_exact_identifier
// already applies (that test's own comment: a weaker check survives a
// mutation swapping two identifiers).
void check_color_failure(const color_parse_result &result, std::string_view expected_identifier) {
    GLINTFX_CHECK(!result.ok);
    GLINTFX_CHECK(result.diagnostic.expected == expected_identifier);
}

// --- helpers for the SECOND critical finding (GODS_LAWS.md L-27/L-40):
// color_parse.cpp's own saturate_out_of_range_number() decided
// overflow-vs-underflow by summing TWO separately-clamped magnitudes
// (a mantissa's own most-significant-digit place, and an explicit "e"
// exponent) - clamping BOTH to the SAME shared bound before adding
// them let an adversarial digit run push one to +bound and the other
// to -bound, summing to EXACTLY ZERO even though the TRUE (unclamped)
// sum was nowhere near zero - a genuine underflow read back as an
// overflow (color_parse.cpp's own SECOND CORRECTION comment has the
// full derivation). The two builders below construct the EXACT lexeme
// shapes that reproduction needs, never a hardcoded ~2,000,000-
// character literal in this file's own source.

// "0." + `leading_zero_count` zeros + "1" + "e" + `explicit_exponent` -
// a fractional mantissa whose own most-significant-digit place is
// EXACTLY -(leading_zero_count + 1) (color_parse.cpp's own
// most_significant_digit_place(), verified by hand against this same
// shape), combined with an explicit exponent SHORT enough in TEXT to
// never overflow its own std::from_chars() parse (every magnitude this
// file's own test cases pass here is well under 19 digits) - so both
// halves reach saturate_out_of_range_number() as the mantissa/exponent
// pair's OWN TRUE values, never a value this helper itself invented.
std::string make_underflow_leaning_lexeme(std::size_t leading_zero_count,
                                          long long explicit_exponent) {
    std::string text = "0.";
    text.append(leading_zero_count, '0');
    text.push_back('1');
    text += "e";
    text += std::to_string(explicit_exponent);
    return text;
}

// The SAME "0." + zeros + "1" shape, but with the EXPONENT's OWN digit
// run long enough (all '9's) to overflow std::from_chars() on ITS OWN
// parse - parse_saturating_exponent()'s OTHER saturation path
// (color_parse.cpp), reached with only a few dozen characters of TEXT
// despite the MAGNITUDE it stands for being far larger than anything
// representable in a long long (2^63-ish) - the "as duas ordens de
// grandeza invertidas" half of this fatia's own boundary matrix below:
// here the EXPONENT is the side that saturates, not the mantissa.
std::string make_exponent_digit_overflow_lexeme(std::size_t leading_zero_count,
                                                bool exponent_is_negative,
                                                std::size_t exponent_nine_count) {
    std::string text = "0.";
    text.append(leading_zero_count, '0');
    text.push_back('1');
    text += "e";
    if (exponent_is_negative) {
        text.push_back('-');
    }
    text.append(exponent_nine_count, '9');
    return text;
}

// COLOR-INTPART-COV (TODO.md, achado da re-revisao de 27/08/2026): every
// attack string above shares the SAME "0." + zeros + "1" shape - the
// first nonzero digit always sits AFTER the '.', so
// most_significant_digit_place() (numeric_lexeme.cpp) always takes its
// FRACTIONAL branch (`i >= whole_digit_count`). This helper exercises
// the OTHER branch instead: "1" followed by `trailing_zero_count`
// zeros, WITHOUT any '.' at all - the exact shape ("numero escrito sem
// ponto decimal") of the ORIGINAL first critical finding this file's
// own rgb_number_component_overflow_saturates_to_the_extreme test
// above already covers for a SIMPLE overflow ("1e400"), but never
// exercised in combination with a canceling exponent the way the
// fractional-form matrix below does. With no '.', `dot` is
// std::string_view::npos, so `whole_digit_count` is the WHOLE mantissa
// length and the leading '1' (index 0) is always `i < whole_digit_count`
// - the integer-part branch, by construction, for every
// `trailing_zero_count`.
std::string make_overflow_leaning_lexeme_without_decimal_point(std::size_t trailing_zero_count,
                                                                long long explicit_exponent) {
    std::string text = "1";
    text.append(trailing_zero_count, '0');
    text += "e";
    text += std::to_string(explicit_exponent);
    return text;
}

} // namespace

// --- the FIRST assertion (GODS_LAWS.md L-20's own "vermelho antes de
// verde"): a plain named color round-trips through the whole pipeline.

GLINTFX_TEST(gltfx_gfss_parse_color_resolves_the_named_keyword_red) {
    const color_parse_result result = parse_color("red");
    check_color_result(result,
                       glintfx::gltfx_rgba8{.red = 255, .green = 0, .blue = 0, .alpha = 255});
}

// --- named colors: ENUMERATED CLOSED, not a directed sample
// (GODS_LAWS.md L-40/L-27, "enumere o espaco pequeno quando ele for
// fechado" - named_colors.hpp's own header comment names the two
// independently cross-checked sources for this 149-row table). Every
// entry is round-tripped through parse_color() ITSELF, not merely
// lookup_named_color() directly - this is what proves the ANALYZER,
// not only the table, reaches every one of the 149 keywords.

GLINTFX_TEST(gltfx_gfss_parse_color_resolves_every_named_color_keyword) {
    using glintfx::style::detail::named_color_at;
    using glintfx::style::detail::named_color_count;

    const std::size_t total = named_color_count();
    GLINTFX_CHECK(total > 0); // GODS_LAWS.md L-40: zero swept is a floor violation, never a pass
    GLINTFX_CHECK_EQ(total, static_cast<std::size_t>(149));

    std::size_t checked = 0;
    for (std::size_t i = 0; i < total; ++i) {
        const auto &entry = named_color_at(i);
        const color_parse_result result = parse_color(entry.name);
        check_color_result(result, entry.value);
        ++checked;
    }
    GLINTFX_CHECK_EQ(checked, total);
    // L-40: the count swept is printed even when everything passes.
    std::println("gltfx_gfss_parse_color_resolves_every_named_color_keyword: {} keyword(s) checked",
                 checked);
}

GLINTFX_TEST(gltfx_gfss_parse_color_named_keyword_lookup_is_ascii_case_insensitive) {
    check_color_result(parse_color("RED"),
                       glintfx::gltfx_rgba8{.red = 255, .green = 0, .blue = 0, .alpha = 255});
    check_color_result(parse_color("ReBeCcApUrPlE"),
                       glintfx::gltfx_rgba8{.red = 102, .green = 51, .blue = 153, .alpha = 255});
}

// --- hex literals: the FOUR forms color_parse.hpp's own header
// comment enumerates, closed.

GLINTFX_TEST(gltfx_gfss_parse_color_hex_covers_every_one_of_the_four_forms) {
    struct hex_case {
        // Default member initializer, not a user-declared
        // constructor (would forfeit aggregate-init below) - the SAME
        // cppcheck uninitMemberVarNoCtor fix named_colors.hpp's own
        // named_color_entry already applies.
        std::string_view text;
        glintfx::gltfx_rgba8 expected{};
    };
    constexpr hex_case k_cases[] = {
        // #RGB - each nibble duplicated: 0xf -> 0xff, 0x0 -> 0x00, alpha defaults opaque.
        {"#f0a", glintfx::gltfx_rgba8{.red = 255, .green = 0, .blue = 170, .alpha = 255}},
        // #RGBA - same duplication, PLUS an explicit alpha nibble.
        {"#f0a8", glintfx::gltfx_rgba8{.red = 255, .green = 0, .blue = 170, .alpha = 136}},
        // #RRGGBB - byte pairs, alpha defaults opaque.
        {"#ff00aa", glintfx::gltfx_rgba8{.red = 255, .green = 0, .blue = 170, .alpha = 255}},
        // #RRGGBBAA - byte pairs, explicit alpha byte.
        {"#ff00aa80", glintfx::gltfx_rgba8{.red = 255, .green = 0, .blue = 170, .alpha = 128}},
    };
    std::size_t checked = 0;
    for (const hex_case &c : k_cases) {
        check_color_result(parse_color(c.text), c.expected);
        ++checked;
    }
    GLINTFX_CHECK_EQ(checked, static_cast<std::size_t>(4));
    std::println(
        "gltfx_gfss_parse_color_hex_covers_every_one_of_the_four_forms: {} form(s) checked",
        checked);
}

GLINTFX_TEST(gltfx_gfss_parse_color_hex_accepts_uppercase_digits) {
    check_color_result(parse_color("#FF00AA"),
                       glintfx::gltfx_rgba8{.red = 255, .green = 0, .blue = 170, .alpha = 255});
}

// --- rgb()/rgba()/hsl()/hsla(): the FOUR function names
// color_parse.hpp's own header comment enumerates, closed.

GLINTFX_TEST(gltfx_gfss_parse_color_functions_cover_every_one_of_the_four_names) {
    struct function_case {
        // Same cppcheck fix as hex_case above.
        std::string_view text;
        glintfx::gltfx_rgba8 expected{};
    };
    constexpr function_case k_cases[] = {
        {"rgb(255, 0, 0)", glintfx::gltfx_rgba8{.red = 255, .green = 0, .blue = 0, .alpha = 255}},
        {"rgba(255, 0, 0, 0.5)",
         glintfx::gltfx_rgba8{.red = 255, .green = 0, .blue = 0, .alpha = 128}},
        // hue 0, saturation 100%, lightness 50% is pure red - hand
        // re-derived from the CSS Color 4 reference formula
        // (color_parse.cpp's own header comment), not merely trusted.
        {"hsl(0, 100%, 50%)",
         glintfx::gltfx_rgba8{.red = 255, .green = 0, .blue = 0, .alpha = 255}},
        {"hsla(0, 100%, 50%, 50%)",
         glintfx::gltfx_rgba8{.red = 255, .green = 0, .blue = 0, .alpha = 128}},
    };
    std::size_t checked = 0;
    for (const function_case &c : k_cases) {
        check_color_result(parse_color(c.text), c.expected);
        ++checked;
    }
    GLINTFX_CHECK_EQ(checked, static_cast<std::size_t>(4));
    std::println("gltfx_gfss_parse_color_functions_cover_every_one_of_the_four_names: {} "
                 "function(s) checked",
                 checked);
}

GLINTFX_TEST(gltfx_gfss_parse_color_rgb_accepts_percentage_components) {
    check_color_result(parse_color("rgb(100%, 0%, 50%)"),
                       glintfx::gltfx_rgba8{.red = 255, .green = 0, .blue = 128, .alpha = 255});
}

GLINTFX_TEST(
    gltfx_gfss_parse_color_functions_are_ascii_case_insensitive_and_ignore_inner_whitespace) {
    check_color_result(parse_color("RGB( 255 , 0 , 0 )"),
                       glintfx::gltfx_rgba8{.red = 255, .green = 0, .blue = 0, .alpha = 255});
    check_color_result(parse_color("HsL(0, 100%, 50%)"),
                       glintfx::gltfx_rgba8{.red = 255, .green = 0, .blue = 0, .alpha = 255});
}

// --- hue wraps modulo 360, both directions - the reference formula's
// own fmod()/negative-wrap branch (color_parse.cpp's own
// hsl_to_rgb_unit()).

GLINTFX_TEST(gltfx_gfss_parse_color_hsl_hue_wraps_outside_zero_to_360) {
    check_color_result(parse_color("hsl(360, 100%, 50%)"), // 360 wraps to 0
                       glintfx::gltfx_rgba8{.red = 255, .green = 0, .blue = 0, .alpha = 255});
    check_color_result(parse_color("hsl(-360, 100%, 50%)"), // -360 wraps to 0 too
                       glintfx::gltfx_rgba8{.red = 255, .green = 0, .blue = 0, .alpha = 255});
}

// --- clamping, never rejection (color_parse.hpp's own scope-cut 5) -
// out-of-range components are a SUCCESS with a clamped value, not a
// parse error.

GLINTFX_TEST(gltfx_gfss_parse_color_clamps_out_of_range_rgb_numbers_instead_of_rejecting) {
    check_color_result(parse_color("rgb(300, -10, 0)"),
                       glintfx::gltfx_rgba8{.red = 255, .green = 0, .blue = 0, .alpha = 255});
}

GLINTFX_TEST(gltfx_gfss_parse_color_clamps_out_of_range_hsl_saturation_instead_of_rejecting) {
    // 150% saturation clamps to 100% BEFORE the reference formula runs
    // - still resolves to pure red at hue 0/lightness 50%, not an
    // error.
    check_color_result(parse_color("hsl(0, 150%, 50%)"),
                       glintfx::gltfx_rgba8{.red = 255, .green = 0, .blue = 0, .alpha = 255});
}

// --- the layering hand-off to GFSS-TOKEN's own vocabulary: a
// malformed STRING (never valid gfss color syntax to begin with)
// propagates the TOKENIZER's own diagnostic verbatim, not this file's
// generic "not a color value" - proves color_parse.cpp's own
// bad_string/bad_url branch (parse_color()'s header comment).

GLINTFX_TEST(gltfx_gfss_parse_color_propagates_the_tokenizers_own_diagnostic_for_bad_string) {
    using glintfx::style::detail::k_expected_closing_quote;
    // A string cut off by EOF alone (no embedded newline) is still a
    // <string-token> per the spec's own lenient EOF handling
    // (tokenizer.cpp's own consume_string_token()) - only a literal
    // NEWLINE inside the string produces the <bad-string-token>
    // recovery class this test means to exercise, so the embedded
    // '\n' below is load-bearing, not incidental.
    const color_parse_result result = parse_color("'unterminated\nrest");
    check_color_failure(result, k_expected_closing_quote);
}

// --- the control negativo: oklch()/lab()/lch()/oklab() are RECOGNIZED
// and DEFERRED (ESCOPO.md SS4 decision 8, GFSS-OKLCH, TODO.md wave W4)
// - a DIFFERENT diagnostic from a totally unknown function name.

GLINTFX_TEST(gltfx_gfss_parse_color_oklch_is_recognized_and_deferred_not_treated_as_unknown) {
    using glintfx::style::detail::k_color_expected_shipped_color_notation;
    check_color_failure(parse_color("oklch(0.6 0.15 30)"), k_color_expected_shipped_color_notation);
    check_color_failure(parse_color("lab(50 40 30)"), k_color_expected_shipped_color_notation);
    check_color_failure(parse_color("lch(50 40 30)"), k_color_expected_shipped_color_notation);
    check_color_failure(parse_color("oklab(0.6 0.1 0.1)"), k_color_expected_shipped_color_notation);
}

GLINTFX_TEST(gltfx_gfss_parse_color_a_truly_unknown_function_name_is_a_different_diagnostic) {
    using glintfx::style::detail::k_color_expected_known_color_function;
    check_color_failure(parse_color("foo(1, 2, 3)"), k_color_expected_known_color_function);
}

// --- hostile input, taken seriously (this fatia's own service order):
// wrong arity, out-of-range value ALREADY covered above as a SUCCESS
// (clamped), malformed number, garbage, empty string, invalid hex
// length, percentage where a plain number is expected. "Validate
// before converting" (the service order's own warning): every one of
// these is caught BEFORE decode_number_lexeme()/hex decoding ever
// runs, never a converted-then-discarded value.

GLINTFX_TEST(gltfx_gfss_parse_color_empty_input_is_a_color_value_diagnostic) {
    using glintfx::style::detail::k_color_expected_color_value;
    check_color_failure(parse_color(""), k_color_expected_color_value);
}

GLINTFX_TEST(gltfx_gfss_parse_color_garbage_that_is_not_a_hash_ident_or_function_is_rejected) {
    using glintfx::style::detail::k_color_expected_color_value;
    check_color_failure(parse_color("123"), k_color_expected_color_value);
}

GLINTFX_TEST(gltfx_gfss_parse_color_unknown_keyword_is_rejected) {
    using glintfx::style::detail::k_color_expected_known_color_keyword;
    check_color_failure(parse_color("notacolor"), k_color_expected_known_color_keyword);
}

GLINTFX_TEST(gltfx_gfss_parse_color_hex_with_invalid_length_is_rejected) {
    using glintfx::style::detail::k_color_expected_valid_hex_length;
    check_color_failure(parse_color("#ab"), k_color_expected_valid_hex_length);
    check_color_failure(parse_color("#abcde"), k_color_expected_valid_hex_length);
}

GLINTFX_TEST(gltfx_gfss_parse_color_hex_with_invalid_digit_is_rejected) {
    using glintfx::style::detail::k_color_expected_hex_digit;
    check_color_failure(parse_color("#ggg"), k_color_expected_hex_digit);
}

GLINTFX_TEST(
    gltfx_gfss_parse_color_rgb_component_that_is_neither_number_nor_percentage_is_rejected) {
    using glintfx::style::detail::k_color_expected_number_or_percentage;
    check_color_failure(parse_color("rgb(red, 0, 0)"), k_color_expected_number_or_percentage);
}

GLINTFX_TEST(gltfx_gfss_parse_color_hsl_hue_as_percentage_is_rejected) {
    // The service order's own "percentual onde se espera numero":
    // hsl()'s hue slot wants a bare <number>, never a <percentage>.
    using glintfx::style::detail::k_color_expected_number;
    check_color_failure(parse_color("hsl(50%, 50%, 50%)"), k_color_expected_number);
}

GLINTFX_TEST(gltfx_gfss_parse_color_hsl_saturation_as_plain_number_is_rejected) {
    using glintfx::style::detail::k_color_expected_percentage;
    check_color_failure(parse_color("hsl(0, 50, 50%)"), k_color_expected_percentage);
}

GLINTFX_TEST(gltfx_gfss_parse_color_rgb_mixed_number_and_percentage_components_are_rejected) {
    // CSS Color 3's own "all values must be of the same type" rule
    // (color_parse.hpp's own scope-cut 3) - the SECOND way the service
    // order's "percentual onde se espera numero" shows up: not a
    // wrong TOKEN KIND at the grammar level, a wrong TYPE MIX at the
    // rgb()-specific level.
    using glintfx::style::detail::k_color_expected_uniform_component_types;
    check_color_failure(parse_color("rgb(255, 50%, 0)"), k_color_expected_uniform_component_types);
}

GLINTFX_TEST(gltfx_gfss_parse_color_missing_comma_between_rgb_arguments_is_rejected) {
    using glintfx::style::detail::k_color_expected_comma;
    check_color_failure(parse_color("rgb(255 0 0)"), k_color_expected_comma);
}

GLINTFX_TEST(gltfx_gfss_parse_color_one_argument_too_many_is_rejected) {
    using glintfx::style::detail::k_color_expected_argument_count;
    check_color_failure(parse_color("rgb(255, 0, 0, 0)"), k_color_expected_argument_count);
}

GLINTFX_TEST(gltfx_gfss_parse_color_running_out_of_input_before_enough_arguments_is_rejected) {
    using glintfx::style::detail::k_color_expected_argument_count;
    check_color_failure(parse_color("rgb(255, 0,"), k_color_expected_argument_count);
}

GLINTFX_TEST(gltfx_gfss_parse_color_wrong_closing_token_after_the_last_argument_is_rejected) {
    using glintfx::style::detail::k_color_expected_closing_parenthesis;
    check_color_failure(parse_color("rgb(255, 0, 0]"), k_color_expected_closing_parenthesis);
}

GLINTFX_TEST(gltfx_gfss_parse_color_missing_closing_parenthesis_at_end_of_input_is_rejected) {
    using glintfx::style::detail::k_color_expected_closing_parenthesis;
    check_color_failure(parse_color("rgb(255, 0, 0"), k_color_expected_closing_parenthesis);
}

GLINTFX_TEST(gltfx_gfss_parse_color_trailing_content_after_a_complete_value_is_rejected) {
    using glintfx::style::detail::k_color_expected_no_trailing_content;
    check_color_failure(parse_color("red extra"), k_color_expected_no_trailing_content);
    check_color_failure(parse_color("#fff garbage"), k_color_expected_no_trailing_content);
}

// --- diagnostic position: line/column are real, not left at their
// default-constructed zero (token.hpp's own R4 convention: absent
// means never attached, so a REAL diagnostic must read back non-zero
// here to be distinguishable from one that was never produced at all).

GLINTFX_TEST(gltfx_gfss_parse_color_diagnostic_carries_a_real_line_and_column) {
    const color_parse_result result = parse_color("#ab");
    GLINTFX_CHECK(!result.ok);
    GLINTFX_CHECK_EQ(result.diagnostic.line, static_cast<std::uint32_t>(1));
    GLINTFX_CHECK_EQ(result.diagnostic.column, static_cast<std::uint32_t>(1));

    // Same failure, later in the line - the column must MOVE with it,
    // not stay pinned at the start of input (would survive a mutant
    // that always reports column 1, which is exactly the class of bug
    // a single-position test cannot catch on its own).
    const color_parse_result later = parse_color("red   #ab");
    GLINTFX_CHECK(!later.ok);
    // "red   " is not a valid color value on its own, so this actually
    // fails earlier, at the trailing-content check right after "red" -
    // proves the SAME thing (a moving column), from a different
    // diagnostic.
    GLINTFX_CHECK(later.diagnostic.column > static_cast<std::uint32_t>(1));
}

// --- CRITICAL finding, GFSS-COLOR-PARSE review (GODS_LAWS.md L-27/
// L-40): decode_number_lexeme()'s own PRIOR version asserted
// std::from_chars() always succeeds for a lexeme this project's own
// tokenizer produces - true for the SYNTAX, false for the VALUE, since
// lexical_rules.cpp's own consume_optional_exponent() does not cap the
// exponent digit run ("1e400" is syntactically valid gfss that
// overflows double). The boundary matrix below is enumerated
// component by component, not sampled - GODS_LAWS.md L-40's own
// "enumerate the small space" - covering the SAME decode path
// rgb()/rgba()/hsl()/hsla() all four share (color_parse.cpp's own
// parse_function_arguments()), never just one function name.

GLINTFX_TEST(gltfx_gfss_parse_color_rgb_number_component_overflow_saturates_to_the_extreme) {
    struct boundary_case {
        std::string_view text;
        glintfx::gltfx_rgba8 expected{};
    };
    constexpr boundary_case k_cases[] = {
        // Positive overflow ("estouro por cima") - the CRITICAL
        // finding's own first reproduction: MUST saturate to the
        // MAXIMUM byte, never to zero.
        {"rgb(1e400, 0, 0)", glintfx::gltfx_rgba8{.red = 255, .green = 0, .blue = 0, .alpha = 255}},
        {"rgb(0, 1e309, 0)", glintfx::gltfx_rgba8{.red = 0, .green = 255, .blue = 0, .alpha = 255}},
        {"rgb(0, 0, 1e400)", glintfx::gltfx_rgba8{.red = 0, .green = 0, .blue = 255, .alpha = 255}},
        // Negative overflow ("estouro por baixo, valor negativo
        // gigante") - saturates to the MINIMUM byte.
        {"rgb(-1e400, 0, 0)", glintfx::gltfx_rgba8{.red = 0, .green = 0, .blue = 0, .alpha = 255}},
        // Underflow towards zero ("estouro por baixo, magnitude
        // minuscula") - a DIFFERENT libstdc++ code path than overflow
        // (both report errc::result_out_of_range, this project's own
        // measured probe) but the SAME correct outcome here: a
        // component this close to zero clamps to zero either way.
        {"rgb(1e-400, 0, 0)", glintfx::gltfx_rgba8{.red = 0, .green = 0, .blue = 0, .alpha = 255}},
        {"rgb(-1e-400, 0, 0)", glintfx::gltfx_rgba8{.red = 0, .green = 0, .blue = 0, .alpha = 255}},
        // The largest FINITE representable double - std::from_chars()
        // itself SUCCEEDS for this one (not the out-of-range branch at
        // all): the pre-existing in-range clamp_0_255_to_byte() path,
        // included so the boundary table is not silently missing the
        // one row that never touches this fatia's new code.
        {"rgb(1.7976931348623157e308, 0, 0)",
         glintfx::gltfx_rgba8{.red = 255, .green = 0, .blue = 0, .alpha = 255}},
        // A <percentage-token> overflow - decode_percentage_lexeme()
        // shares decode_number_lexeme() below, but the byte comes from
        // clamp_unit_to_byte() (a DIFFERENT downstream formula than
        // clamp_0_255_to_byte() above), so this is its own row, not
        // redundant with the plain-number case.
        {"rgb(1e400%, 0%, 0%)",
         glintfx::gltfx_rgba8{.red = 255, .green = 0, .blue = 0, .alpha = 255}},
    };
    std::size_t checked = 0;
    for (const boundary_case &c : k_cases) {
        check_color_result(parse_color(c.text), c.expected);
        ++checked;
    }
    GLINTFX_CHECK_EQ(checked, static_cast<std::size_t>(8));
    std::println(
        "gltfx_gfss_parse_color_rgb_number_component_overflow_saturates_to_the_extreme: {} "
        "case(s) checked",
        checked);
}

// --- the alpha-specific half of the SAME finding: the reviewer's own
// reproduction showed rgba(255,0,0,1e400) reading back FULLY
// TRANSPARENT (alpha=0) - the extreme OPPOSITE of the intended
// saturation - and rgba(...,1e-400) (underflow) needing the opposite
// answer again (also transparent, but for the RIGHT reason: alpha near
// zero really is near zero).

GLINTFX_TEST(gltfx_gfss_parse_color_rgba_alpha_overflow_saturates_to_the_correct_extreme) {
    struct boundary_case {
        std::string_view text;
        glintfx::gltfx_rgba8 expected{};
    };
    constexpr boundary_case k_cases[] = {
        // Positive overflow: alpha MUST saturate to fully OPAQUE
        // (255), never to fully transparent - this is the exact case
        // color_parse.cpp's OWN prior silent bug ("valor absurdamente
        // grande virou ZERO") got backwards.
        {"rgba(255, 0, 0, 1e400)",
         glintfx::gltfx_rgba8{.red = 255, .green = 0, .blue = 0, .alpha = 255}},
        // Negative overflow: saturates to fully TRANSPARENT (0).
        {"rgba(255, 0, 0, -1e400)",
         glintfx::gltfx_rgba8{.red = 255, .green = 0, .blue = 0, .alpha = 0}},
        // Underflow towards zero: also fully transparent, but for the
        // opposite reason from the positive-overflow row above (a
        // near-zero alpha really is near zero) - the row that proves
        // this fatia's fix does not confuse "big" with "small".
        {"rgba(255, 0, 0, 1e-400)",
         glintfx::gltfx_rgba8{.red = 255, .green = 0, .blue = 0, .alpha = 0}},
    };
    std::size_t checked = 0;
    for (const boundary_case &c : k_cases) {
        check_color_result(parse_color(c.text), c.expected);
        ++checked;
    }
    GLINTFX_CHECK_EQ(checked, static_cast<std::size_t>(3));
    std::println("gltfx_gfss_parse_color_rgba_alpha_overflow_saturates_to_the_correct_extreme: {} "
                 "case(s) checked",
                 checked);
}

// --- hsl()/hsla()'s saturation and lightness slots share the SAME
// decode_number_lexeme() overflow path (they are <percentage-token>s,
// component_kind_requirement::percentage_only) but a DIFFERENT
// downstream formula (hsl_to_rgb_unit()'s own std::clamp() on
// sat_unit/light_unit, not clamp_0_255_to_byte()/
// clamp_unit_to_byte()) - saturation=100% and lightness=0%/100% are
// the three hue-INDEPENDENT extremes (light=100% is always white,
// light=0%/sat=0% both zero the `a` term in the reference formula and
// so read back as whatever `light` alone is), so these rows are
// hand-derivable from the formula, unlike the hue case below.

GLINTFX_TEST(
    gltfx_gfss_parse_color_hsl_saturation_and_lightness_overflow_saturates_to_the_extreme) {
    struct boundary_case {
        std::string_view text;
        glintfx::gltfx_rgba8 expected{};
    };
    constexpr boundary_case k_cases[] = {
        // Lightness overflow saturates to 100% - white, at ANY hue.
        {"hsl(123, 100%, 1e400%)",
         glintfx::gltfx_rgba8{.red = 255, .green = 255, .blue = 255, .alpha = 255}},
        // Lightness AND saturation both underflow to 0% - black.
        {"hsl(123, 1e-400%, 1e-400%)",
         glintfx::gltfx_rgba8{.red = 0, .green = 0, .blue = 0, .alpha = 255}},
        // Saturation underflows to 0% alone: `a` is zero regardless of
        // lightness's own value, so the result is the GRAY the
        // reference formula's own channel() collapses to (channel(n)
        // == light for every n when a == 0) - light=0% keeps this row
        // analytically clean (black), not a rounded mid-gray.
        {"hsl(123, 1e-400%, 0%)",
         glintfx::gltfx_rgba8{.red = 0, .green = 0, .blue = 0, .alpha = 255}},
    };
    std::size_t checked = 0;
    for (const boundary_case &c : k_cases) {
        check_color_result(parse_color(c.text), c.expected);
        ++checked;
    }
    GLINTFX_CHECK_EQ(checked, static_cast<std::size_t>(3));
    std::println(
        "gltfx_gfss_parse_color_hsl_saturation_and_lightness_overflow_saturates_to_the_extreme: "
        "{} case(s) checked",
        checked);
}

// --- the hue slot is the one component_kind_requirement::number_only
// case (never a percentage), and hsl_to_rgb_unit()'s own
// std::fmod(hue_deg, 360.0) is a DOMAIN ERROR (produces NaN) for an
// INFINITE hue_deg, even though it is well-defined and FINITE for the
// largest finite double - this is exactly why
// saturate_out_of_range_number() (color_parse.cpp) returns
// std::numeric_limits<double>::max()/lowest(), never +-infinity.
// UNLIKE the rows above, CSS defines no meaning for a hue this
// absurd, so this test does NOT hardcode a hand-derived RGB triple
// (that would silently re-derive whatever std::fmod() happens to
// produce on this platform instead of checking the property that
// actually matters) - it proves the two things the finding cares
// about instead: the call SUCCEEDS (no crash, no NaN reaching
// std::lround()/static_cast<std::uint8_t> - THAT cast on a NaN would
// be undefined behavior), and it is DETERMINISTIC (calling twice with
// the SAME hostile text yields byte-identical output - a NaN or an
// uninitialized read reaching the final byte would not reliably do
// that).

GLINTFX_TEST(gltfx_gfss_parse_color_hsl_hue_overflow_never_reaches_nan_and_stays_deterministic) {
    constexpr std::string_view k_cases[] = {
        "hsl(1e400, 100%, 50%)",
        "hsl(-1e400, 100%, 50%)",
        "hsla(1e400, 100%, 50%, 50%)",
    };
    std::size_t checked = 0;
    for (const std::string_view text : k_cases) {
        const color_parse_result first = parse_color(text);
        const color_parse_result second = parse_color(text);
        GLINTFX_CHECK(first.ok);
        GLINTFX_CHECK(second.ok);
        if (!first.ok || !second.ok) {
            continue;
        }
        const glintfx::gltfx_rgba8 back_first = glintfx::gltfx_rgba_to_srgb8(first.value);
        const glintfx::gltfx_rgba8 back_second = glintfx::gltfx_rgba_to_srgb8(second.value);
        GLINTFX_CHECK_EQ(back_first.red, back_second.red);
        GLINTFX_CHECK_EQ(back_first.green, back_second.green);
        GLINTFX_CHECK_EQ(back_first.blue, back_second.blue);
        GLINTFX_CHECK_EQ(back_first.alpha, back_second.alpha);
        ++checked;
    }
    GLINTFX_CHECK_EQ(checked, static_cast<std::size_t>(3));
    std::println(
        "gltfx_gfss_parse_color_hsl_hue_overflow_never_reaches_nan_and_stays_deterministic: {} "
        "case(s) checked",
        checked);
}

// --- SECOND CRITICAL finding, GFSS-COLOR-PARSE adversarial re-review
// (GODS_LAWS.md L-27/L-40): the FIRST fix above (decode_number_lexeme's
// own comment, and the boundary matrix just above this one) closed the
// "does an out-of-range component saturate to the right extreme"
// question. This second review found that saturate_out_of_range_number()
// itself (color_parse.cpp) could pick the WRONG extreme when a mantissa
// digit run and an explicit exponent digit run, both individually
// clamped to the SAME shared bound before being ADDED, landed on
// EXACTLY ZERO - discarding which one was actually larger. The three
// rows below are the reviewer's OWN three reproductions (red channel,
// alpha channel, hsl lightness), each with the ATTACK STRING that
// triggered it and the CORRECT (post-fix) answer - GODS_LAWS.md L-20's
// own "vermelho antes de verde": every one of these three FAILED
// (wrong color) against the pre-fix source, verified live before this
// file's own fix (color_parse.cpp's own SECOND CORRECTION comment) was
// applied - not merely asserted here.

GLINTFX_TEST(
    gltfx_gfss_parse_color_rgb_component_survives_a_mantissa_exponent_collision_at_the_old_shared_bound) {
    // leading_zero_count = 1,999,999 -> mantissa place = -2,000,000
    // (this project's own historical shared bound was 1,000,000, so
    // this digit run is roughly TWICE that, deliberately past it -
    // "corrida de dígitos muito longa" the finding names). explicit
    // exponent = +1,000,000, opposite sign, SHORT text ("e1000000",
    // 8 characters) - the finding's own "expoente curto de sinal
    // oposto". TRUE combined exponent: -2,000,000 + 1,000,000 =
    // -1,000,000 (a genuine, deep underflow - the component's real
    // value is astronomically close to zero) - the PRE-FIX code
    // independently clamped both summands to +-1,000,000 first, summed
    // to EXACTLY ZERO, and read that as an OVERFLOW instead, saturating
    // to the byte 255 the reviewer's own report table names.
    const std::string attack = make_underflow_leaning_lexeme(1'999'999, 1'000'000);
    check_color_result(parse_color("rgb(" + attack + ", 0, 0)"),
                       glintfx::gltfx_rgba8{.red = 0, .green = 0, .blue = 0, .alpha = 255});
}

GLINTFX_TEST(
    gltfx_gfss_parse_color_rgba_alpha_survives_a_mantissa_exponent_collision_at_the_old_shared_bound) {
    // SAME attack shape as the red-channel row above, in the ALPHA
    // slot instead - the reviewer's own second reproduction, reading
    // back fully OPAQUE (255) pre-fix where the true value is a genuine
    // underflow (alpha component this close to zero clamps to fully
    // TRANSPARENT, byte 0 - clamp_unit_to_byte()'s own convention,
    // color_parse.cpp).
    const std::string attack = make_underflow_leaning_lexeme(1'999'999, 1'000'000);
    check_color_result(parse_color("rgba(255, 0, 0, " + attack + ")"),
                       glintfx::gltfx_rgba8{.red = 255, .green = 0, .blue = 0, .alpha = 0});
}

GLINTFX_TEST(
    gltfx_gfss_parse_color_hsl_lightness_survives_a_mantissa_exponent_collision_at_the_old_shared_bound) {
    // SAME attack shape again, in the hsl() LIGHTNESS slot (a
    // <percentage-token>, so the '%' is appended AFTER the exponent
    // digits - decode_percentage_lexeme() strips it before this file's
    // own decode_number_lexeme() ever sees the lexeme) - the reviewer's
    // own third reproduction, reading back WHITE pre-fix (lightness
    // misread as saturating toward 100%) where the true value is a
    // genuine underflow (lightness this close to 0% is BLACK at any
    // hue - hsl_to_rgb_unit()'s own reference formula, color_parse.cpp).
    const std::string attack = make_underflow_leaning_lexeme(1'999'999, 1'000'000);
    check_color_result(parse_color("hsl(123, 100%, " + attack + "%)"),
                       glintfx::gltfx_rgba8{.red = 0, .green = 0, .blue = 0, .alpha = 255});
}

// --- the fronteira around the empate itself, enumerated rather than
// sampled at one point (GODS_LAWS.md L-40's own "não teste um ponto
// só"): the mantissa's OWN digit run walked through the three
// positions relative to this project's HISTORICAL shared bound
// (1,000,000) - "logo abaixo", "exatamente no teto", "logo acima" -
// crossed with the explicit exponent's own magnitude relative to that
// SAME historical bound - "menor", "igual", "maior" - plus two rows
// where the ORDER OF MAGNITUDE is inverted (the EXPONENT is the side
// that saturates - via a digit run long enough to overflow its OWN
// std::from_chars() parse - and the mantissa stays small), never the
// reverse of the first seven rows' own shape. Every row's `expected`
// byte below was hand-derived from the TRUE, un-clamped combined
// decimal exponent (place + exponent) BEFORE this fatia's own fix
// existed - never copied from whatever the fixed code happens to
// output - and two candidate rows (mantissa place and exponent exactly
// canceling in TRUE arithmetic) are DELIBERATELY ABSENT: a literal
// whose true combined exponent is 0 represents a magnitude near 1,
// which is NOT out of double's own representable range at all, so
// std::from_chars() succeeds directly and this file's own
// saturate_out_of_range_number() is never even reached for it -
// verified against this project's own libstdc++ before being trusted
// here (GODS_LAWS.md L-27), never assumed.

GLINTFX_TEST(gltfx_gfss_parse_color_underflow_overflow_direction_survives_the_boundary_matrix) {
    struct boundary_case {
        // Default member initializers, not a user-declared constructor
        // (would forfeit aggregate-init below) - the SAME cppcheck
        // uninitMemberVarNoCtor fix named_colors.hpp's own
        // named_color_entry already applies.
        std::size_t leading_zero_count = 0; // mantissa place = -(this + 1)
        long long explicit_exponent = 0;    // opposite sign, always SHORT text
        bool byte_is_saturated_max = false; // true: expect 255; false: expect 0
        std::string_view note;
    };
    // clang-format off
    const boundary_case k_cases[] = {
        // -- mantissa dominant, exponent relative to the historical
        // bound (1,000,000): "abaixo"/"no teto"/"acima do teto".
        {500'000,     1'000'000, true,  "abaixo do teto, expoente igual ao teto: real +499999"},
        {500'000,     1'500'000, true,  "abaixo do teto, expoente maior que o teto: real +999999"},
        {999'999,       500'000, false, "no teto, expoente menor que o teto: real -500000"},
        {999'999,     1'500'000, true,  "no teto, expoente maior que o teto: real +500000"},
        {1'999'999,     500'000, false, "acima do teto, expoente menor: real -1500000"},
        // the two rows that actually collided pre-fix (old-clamped sum
        // == 0 while the true sum was deeply negative) - the SAME
        // shape as the three reproductions above, kept here too so the
        // matrix is self-contained without cross-referencing them.
        {1'999'999,   1'000'000, false, "acima do teto, expoente igual: real -1000000 (empate antigo)"},
        {1'999'999,   1'500'000, false, "acima do teto, expoente maior: real -500000 (empate antigo)"},
    };
    // clang-format on
    std::size_t checked = 0;
    for (const boundary_case &c : k_cases) {
        const std::string attack =
            make_underflow_leaning_lexeme(c.leading_zero_count, c.explicit_exponent);
        const std::uint8_t expected_byte = c.byte_is_saturated_max ? 255 : 0;
        check_color_result(
            parse_color("rgb(" + attack + ", 0, 0)"),
            glintfx::gltfx_rgba8{.red = expected_byte, .green = 0, .blue = 0, .alpha = 255});
        ++checked;
    }
    GLINTFX_CHECK_EQ(checked, static_cast<std::size_t>(7));
    std::println(
        "gltfx_gfss_parse_color_underflow_overflow_direction_survives_the_boundary_matrix: {} "
        "case(s) checked",
        checked);
}

GLINTFX_TEST(
    gltfx_gfss_parse_color_underflow_overflow_direction_survives_with_the_inverted_order_of_magnitude) {
    // "as duas ordens de grandeza invertidas": the SEVEN rows above all
    // have the MANTISSA as the side whose digit run is long; these two
    // rows invert that - the EXPONENT's own digit run is what overflows
    // (25 '9' digits, representing a magnitude around 10^25, far beyond
    // any long long), while the mantissa stays modest (leading_zero_
    // count = 500,000, comfortably below the historical bound on
    // either side of the fix). Both rows are CONTROLS in the sense that
    // even the pre-fix code got them right (the exponent's own
    // saturated magnitude, 1,000,000 pre-fix, still dominated a mantissa
    // this small) - kept here because the finding's own report named
    // this direction explicitly, and a fix that broke it silently would
    // be exactly the kind of regression this matrix exists to catch.
    struct inverted_case {
        // Same cppcheck fix as boundary_case above.
        bool exponent_is_negative = false;
        bool byte_is_saturated_max = false;
        std::string_view note;
    };
    const inverted_case k_cases[] = {
        {false, true, "expoente positivo com estouro proprio: real ~+1e25"},
        {true, false, "expoente negativo com estouro proprio: real ~-1e25"},
    };
    std::size_t checked = 0;
    for (const inverted_case &c : k_cases) {
        const std::string attack =
            make_exponent_digit_overflow_lexeme(500'000, c.exponent_is_negative, 25);
        const std::uint8_t expected_byte = c.byte_is_saturated_max ? 255 : 0;
        check_color_result(
            parse_color("rgb(" + attack + ", 0, 0)"),
            glintfx::gltfx_rgba8{.red = expected_byte, .green = 0, .blue = 0, .alpha = 255});
        ++checked;
    }
    GLINTFX_CHECK_EQ(checked, static_cast<std::size_t>(2));
    std::println("gltfx_gfss_parse_color_underflow_overflow_direction_survives_with_the_inverted_"
                 "order_of_magnitude: {} case(s) checked",
                 checked);
}

// --- COLOR-INTPART-COV (TODO.md): the mirror of the boundary matrix
// above, THROUGH THE INTEGER-PART BRANCH the fractional "0."+zeros+"1"
// shape above never reaches. Mantissa place is now POSITIVE and grows
// with `trailing_zero_count` (make_overflow_leaning_lexeme_without_
// decimal_point()'s own header comment above), so a NEGATIVE exponent
// of comparable magnitude is what creates the same "does the sign of
// the sum survive" tension the fractional matrix tests with a positive
// exponent - same historical magnitudes (500'000/999'999/1'999'999),
// same three-way "abaixo do teto"/"no teto"/"acima do teto" x
// "expoente menor/igual/maior" shape, sign of every combination
// verified by hand against most_significant_digit_place()'s own
// formula (place = trailing_zero_count for this shape) before being
// trusted here (GODS_LAWS.md L-27).

GLINTFX_TEST(gltfx_gfss_parse_color_integer_mantissa_collision_survives_the_boundary_matrix) {
    struct boundary_case {
        std::size_t trailing_zero_count = 0; // mantissa place = this (no decimal point)
        long long explicit_exponent = 0;     // negative, opposite sign, always SHORT text
        bool byte_is_saturated_max = false;  // true: expect 255; false: expect 0
        std::string_view note;
    };
    // clang-format off
    const boundary_case k_cases[] = {
        // -- mantissa dominant, exponent relative to the historical
        // bound (1,000,000): "abaixo"/"no teto"/"acima do teto".
        {500'000,     -1'000'000, false, "abaixo do teto, expoente igual ao teto: real -500000"},
        {500'000,     -1'500'000, false, "abaixo do teto, expoente maior que o teto: real -1000000"},
        {999'999,       -500'000, true,  "no teto, expoente menor que o teto: real +499999"},
        {999'999,     -1'500'000, false, "no teto, expoente maior que o teto: real -500001"},
        {1'999'999,     -500'000, true,  "acima do teto, expoente menor: real +1499999"},
        {1'999'999,   -1'000'000, true,  "acima do teto, expoente igual: real +999999"},
        {1'999'999,   -1'500'000, true,  "acima do teto, expoente maior: real +499999"},
    };
    // clang-format on
    std::size_t checked = 0;
    for (const boundary_case &c : k_cases) {
        const std::string attack = make_overflow_leaning_lexeme_without_decimal_point(
            c.trailing_zero_count, c.explicit_exponent);
        const std::uint8_t expected_byte = c.byte_is_saturated_max ? 255 : 0;
        check_color_result(
            parse_color("rgb(" + attack + ", 0, 0)"),
            glintfx::gltfx_rgba8{.red = expected_byte, .green = 0, .blue = 0, .alpha = 255});
        ++checked;
    }
    GLINTFX_CHECK_EQ(checked, static_cast<std::size_t>(7));
    std::println(
        "gltfx_gfss_parse_color_integer_mantissa_collision_survives_the_boundary_matrix: {} "
        "case(s) checked",
        checked);
}

// --- color_diagnostic_vocabulary.hpp's own closed enumeration
// (GODS_LAWS.md L-40): every identifier this fatia can ever attach to
// gltfx_gfss_diagnostic::expected, swept whole, R7-checked
// (snake_case, no space, never a sentence), and PRODUCED for real -
// mirrors gfss_tokenizer_test.cpp's own
// diagnostic_vocabulary_is_enumerated_closed_and_every_identifier_is_produced,
// for THIS fatia's OWN, separate vocabulary
// (color_diagnostic_vocabulary.hpp's own header comment explains why
// it is a second list, not an extension of the tokenizer's).

GLINTFX_TEST(color_diagnostic_vocabulary_is_enumerated_closed_and_every_identifier_is_produced) {
    using glintfx::style::detail::k_color_expected_vocabulary;
    using glintfx::style::detail::k_color_expected_vocabulary_count;

    // GODS_LAWS.md L-40: this table IS the closed enumeration - a 15th
    // identifier added to color_diagnostic_vocabulary.hpp's own list
    // without both a matching row here AND a directed-production case
    // above fails to compile instead of passing silently.
    static_assert(k_color_expected_vocabulary_count == 14,
                  "GODS_LAWS.md L-40: color_diagnostic_vocabulary.hpp's list changed - update "
                  "the directed-production coverage in this file to match");

    std::size_t swept = 0;
    for (const std::string_view identifier : k_color_expected_vocabulary) {
        GLINTFX_CHECK(!identifier.empty());
        bool is_snake_case = true;
        for (const char ch : identifier) {
            const bool is_lower = ch >= 'a' && ch <= 'z';
            const bool is_underscore = ch == '_';
            if (!is_lower && !is_underscore) {
                is_snake_case = false;
                break;
            }
        }
        GLINTFX_CHECK(is_snake_case);
        GLINTFX_CHECK(identifier.find(' ') == std::string_view::npos);
        ++swept;
    }
    // GODS_LAWS.md L-40: zero swept is a floor violation, never a pass.
    GLINTFX_CHECK(swept > 0);
    GLINTFX_CHECK_EQ(swept, k_color_expected_vocabulary_count);
    std::println("color_diagnostic_vocabulary_is_enumerated_closed_and_every_identifier_is_"
                 "produced: {} identifier(s) swept",
                 swept);

    // Every one of the 14 identifiers really is producible - each row
    // below matches ONE test above by name, so a reviewer can find the
    // directed-production case for any row in this list in one grep.
    check_color_failure(parse_color(""), glintfx::style::detail::k_color_expected_color_value);
    check_color_failure(parse_color("#ggg"), glintfx::style::detail::k_color_expected_hex_digit);
    check_color_failure(parse_color("#ab"),
                        glintfx::style::detail::k_color_expected_valid_hex_length);
    check_color_failure(parse_color("notacolor"),
                        glintfx::style::detail::k_color_expected_known_color_keyword);
    check_color_failure(parse_color("foo(1, 2, 3)"),
                        glintfx::style::detail::k_color_expected_known_color_function);
    check_color_failure(parse_color("oklch(0.6 0.15 30)"),
                        glintfx::style::detail::k_color_expected_shipped_color_notation);
    check_color_failure(parse_color("rgb(red, 0, 0)"),
                        glintfx::style::detail::k_color_expected_number_or_percentage);
    check_color_failure(parse_color("hsl(50%, 50%, 50%)"),
                        glintfx::style::detail::k_color_expected_number);
    check_color_failure(parse_color("hsl(0, 50, 50%)"),
                        glintfx::style::detail::k_color_expected_percentage);
    check_color_failure(parse_color("rgb(255, 50%, 0)"),
                        glintfx::style::detail::k_color_expected_uniform_component_types);
    check_color_failure(parse_color("rgb(255 0 0)"),
                        glintfx::style::detail::k_color_expected_comma);
    check_color_failure(parse_color("rgb(255, 0, 0, 0)"),
                        glintfx::style::detail::k_color_expected_argument_count);
    check_color_failure(parse_color("rgb(255, 0, 0]"),
                        glintfx::style::detail::k_color_expected_closing_parenthesis);
    check_color_failure(parse_color("red extra"),
                        glintfx::style::detail::k_color_expected_no_trailing_content);
}
