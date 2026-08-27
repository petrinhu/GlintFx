// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstddef>
#include <cstdint>
#include <print>
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
