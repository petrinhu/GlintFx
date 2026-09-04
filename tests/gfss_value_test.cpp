// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstddef>
#include <limits>
#include <print>
#include <string_view>

#include <glintfx/gfss/token.hpp>
#include <glintfx/gfss/tokenizer.hpp>
#include <glintfx/gfss/value.hpp>

#include "gfss/diagnostic_vocabulary.hpp"
#include "gfss/value_parse.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// gfss_value_test.cpp - GFSS-VALUE (TODO.md, GODS_LAWS.md L-20/L-40):
// the TDD red/green witness for glintfx::style::gltfx_gfss_value/
// gltfx_gfss_value_kind/gltfx_gfss_length/gltfx_gfss_length_unit
// (value.hpp) and glintfx::style::detail::parse_value() (value_parse.
// hpp) - see each of those two files' own header comment for the
// design rationale each check below proves.
//
// THE ONE PAIR THE ORDER OF SERVICE NAMES BY NAME ("caso dirigido dos
// dois lados prova que numero nao colapsa em comprimento e que
// porcentagem nao vira valor absoluto"): parse_value_bare_number_
// never_collapses_into_length below and parse_value_percentage_
// preserves_percentage_never_becomes_length_or_ratio below - read
// those two first if you are checking this fatia's own proof, not the
// enumeration tests.

namespace {

using glintfx::style::gltfx_gfss_angle_unit;
using glintfx::style::gltfx_gfss_length_unit;
using glintfx::style::gltfx_gfss_time_unit;
using glintfx::style::gltfx_gfss_value_kind;
using glintfx::style::detail::count_owned_by;
using glintfx::style::detail::gfss_diagnostic_producer;
using glintfx::style::detail::parse_value;
using glintfx::style::detail::value_parse_result;

// Tokenizes `source` with GFSS-TOKEN's own real scanner
// (gltfx_gfss_tokenize(), tokenizer.hpp) and returns the FIRST
// non-whitespace, non-EOF token - every test below feeds parse_value()
// a token this project's OWN tokenizer actually produced, never a
// hand-fabricated gltfx_gfss_token, so a diagnostic's line/column is
// exercised through the real path, the SAME discipline gfss_color_
// parse_test.cpp's own helpers already apply one layer up.
glintfx::style::gltfx_gfss_token first_value_token(std::string_view source) {
    for (const glintfx::style::gltfx_gfss_token &token :
         glintfx::style::gltfx_gfss_tokenize(source)) {
        if (token.kind != glintfx::style::gltfx_gfss_token_kind::whitespace &&
            token.kind != glintfx::style::gltfx_gfss_token_kind::eof) {
            return token;
        }
    }
    return glintfx::style::gltfx_gfss_token{};
}

value_parse_result parse_first_value(std::string_view source) {
    return parse_value(first_value_token(source));
}

void check_value_failure(const value_parse_result &result, std::string_view expected_identifier) {
    GLINTFX_CHECK(!result.ok);
    GLINTFX_CHECK(result.diagnostic.expected == expected_identifier);
}

} // namespace

// --- gltfx_gfss_value_kind_name(): closed enumeration of the 5 natures

namespace {
struct value_kind_name_sample {
    gltfx_gfss_value_kind kind = gltfx_gfss_value_kind::keyword;
    std::string_view expected_name;
};
} // namespace

GLINTFX_TEST(gltfx_gfss_value_kind_name_covers_every_nature_with_its_exact_identifier) {
    using glintfx::style::gltfx_gfss_value_kind_name;
    constexpr value_kind_name_sample k_samples[] = {
        {gltfx_gfss_value_kind::keyword, "keyword"},
        {gltfx_gfss_value_kind::number, "number"},
        {gltfx_gfss_value_kind::integer, "integer"},
        {gltfx_gfss_value_kind::length, "length"},
        {gltfx_gfss_value_kind::percentage, "percentage"},
        // GFSS-VALUE-2 (ESCOPO.md SS2, 28/08/2026 decision, "entram
        // agora"): angle and time are TWO MORE natures, seven total -
        // the leader's own six-decision commit (400df8f).
        {gltfx_gfss_value_kind::angle, "angle"},
        {gltfx_gfss_value_kind::time, "time"},
    };
    // GODS_LAWS.md L-40: this table IS the closed enumeration - a 6th
    // nature added to value.hpp's own X-macro list without a matching
    // row here fails to compile, not just this suite's own verdict.
    static_assert(sizeof(k_samples) / sizeof(k_samples[0]) ==
                      glintfx::style::gltfx_gfss_value_kind_count,
                  "GODS_LAWS.md L-40: keep this table in sync with value.hpp's own enum, never "
                  "sample a subset");

    int checked = 0;
    for (const value_kind_name_sample &sample : k_samples) {
        const std::string_view name = gltfx_gfss_value_kind_name(sample.kind);
        GLINTFX_CHECK(name == sample.expected_name);
        ++checked;
    }
    std::println(
        "gltfx_gfss_value_kind_name_covers_every_nature_with_its_exact_identifier: {} nature(s) "
        "checked",
        checked);
}

// --- gltfx_gfss_length_unit_name(): closed enumeration of the 16 units
// (ESCOPO.md SS2, GFSS-VALUE-2 impact notes SS2.3: +ch/+lh/+vmin/+vmax)

namespace {
struct length_unit_name_sample {
    gltfx_gfss_length_unit unit = gltfx_gfss_length_unit::px;
    std::string_view expected_name;
};
} // namespace

GLINTFX_TEST(gltfx_gfss_length_unit_name_covers_every_unit_with_its_exact_identifier) {
    using glintfx::style::gltfx_gfss_length_unit_name;
    constexpr length_unit_name_sample k_samples[] = {
        {gltfx_gfss_length_unit::px, "px"},     {gltfx_gfss_length_unit::dp, "dp"},
        {gltfx_gfss_length_unit::em, "em"},     {gltfx_gfss_length_unit::rem, "rem"},
        {gltfx_gfss_length_unit::ex, "ex"},     {gltfx_gfss_length_unit::ch, "ch"},
        {gltfx_gfss_length_unit::lh, "lh"},     {gltfx_gfss_length_unit::vw, "vw"},
        {gltfx_gfss_length_unit::vh, "vh"},     {gltfx_gfss_length_unit::vmin, "vmin"},
        {gltfx_gfss_length_unit::vmax, "vmax"}, {gltfx_gfss_length_unit::in, "in"},
        {gltfx_gfss_length_unit::cm, "cm"},     {gltfx_gfss_length_unit::mm, "mm"},
        {gltfx_gfss_length_unit::pt, "pt"},     {gltfx_gfss_length_unit::pc, "pc"},
    };
    static_assert(sizeof(k_samples) / sizeof(k_samples[0]) ==
                      glintfx::style::gltfx_gfss_length_unit_count,
                  "GODS_LAWS.md L-40: keep this table in sync with value.hpp's own enum, never "
                  "sample a subset");

    int checked = 0;
    for (const length_unit_name_sample &sample : k_samples) {
        const std::string_view name = gltfx_gfss_length_unit_name(sample.unit);
        GLINTFX_CHECK(name == sample.expected_name);
        ++checked;
    }
    std::println(
        "gltfx_gfss_length_unit_name_covers_every_unit_with_its_exact_identifier: {} unit(s) "
        "checked",
        checked);
}

// --- gltfx_gfss_angle_unit_name(): closed enumeration of the 4 units
// (ESCOPO.md SS2, GFSS-VALUE-2 decision 2: "as quatro do padrao")

namespace {
struct angle_unit_name_sample {
    gltfx_gfss_angle_unit unit = gltfx_gfss_angle_unit::deg;
    std::string_view expected_name;
};
} // namespace

GLINTFX_TEST(gltfx_gfss_angle_unit_name_covers_every_unit_with_its_exact_identifier) {
    using glintfx::style::gltfx_gfss_angle_unit_name;
    constexpr angle_unit_name_sample k_samples[] = {
        {gltfx_gfss_angle_unit::deg, "deg"},
        {gltfx_gfss_angle_unit::rad, "rad"},
        {gltfx_gfss_angle_unit::grad, "grad"},
        {gltfx_gfss_angle_unit::turn, "turn"},
    };
    static_assert(sizeof(k_samples) / sizeof(k_samples[0]) ==
                      glintfx::style::gltfx_gfss_angle_unit_count,
                  "GODS_LAWS.md L-40: keep this table in sync with value.hpp's own enum, never "
                  "sample a subset");

    int checked = 0;
    for (const angle_unit_name_sample &sample : k_samples) {
        const std::string_view name = gltfx_gfss_angle_unit_name(sample.unit);
        GLINTFX_CHECK(name == sample.expected_name);
        ++checked;
    }
    std::println(
        "gltfx_gfss_angle_unit_name_covers_every_unit_with_its_exact_identifier: {} unit(s) "
        "checked",
        checked);
}

// --- gltfx_gfss_time_unit_name(): closed enumeration of the 6 units
// (ESCOPO.md SS2, GFSS-VALUE-2 decisions 1 and 3: the five SI-symbol
// spellings plus this library's own `frames`)

namespace {
struct time_unit_name_sample {
    gltfx_gfss_time_unit unit = gltfx_gfss_time_unit::ms;
    std::string_view expected_name;
};
} // namespace

GLINTFX_TEST(gltfx_gfss_time_unit_name_covers_every_unit_with_its_exact_identifier) {
    using glintfx::style::gltfx_gfss_time_unit_name;
    constexpr time_unit_name_sample k_samples[] = {
        {gltfx_gfss_time_unit::ms, "ms"},   {gltfx_gfss_time_unit::s, "s"},
        {gltfx_gfss_time_unit::min, "min"}, {gltfx_gfss_time_unit::h, "h"},
        {gltfx_gfss_time_unit::ns, "ns"},   {gltfx_gfss_time_unit::frames, "frames"},
    };
    static_assert(sizeof(k_samples) / sizeof(k_samples[0]) ==
                      glintfx::style::gltfx_gfss_time_unit_count,
                  "GODS_LAWS.md L-40: keep this table in sync with value.hpp's own enum, never "
                  "sample a subset");

    int checked = 0;
    for (const time_unit_name_sample &sample : k_samples) {
        const std::string_view name = gltfx_gfss_time_unit_name(sample.unit);
        GLINTFX_CHECK(name == sample.expected_name);
        ++checked;
    }
    std::println(
        "gltfx_gfss_time_unit_name_covers_every_unit_with_its_exact_identifier: {} unit(s) "
        "checked",
        checked);
}

// --- keyword nature ---------------------------------------------

GLINTFX_TEST(parse_value_ident_token_decodes_to_keyword_with_raw_lexeme) {
    const value_parse_result result = parse_first_value("border-box");
    GLINTFX_CHECK(result.ok);
    GLINTFX_CHECK(result.value.kind == gltfx_gfss_value_kind::keyword);
    GLINTFX_CHECK(result.value.keyword_text == std::string_view{"border-box"});
}

// ESCOPO.md SS2 decision 5: the three universal keywords decode
// through the SAME plain keyword branch - value.hpp's own header
// comment: their semantics are GFSS-INHERIT's job, not this fatia's.
// Enumerated CLOSED (all three, GODS_LAWS.md L-40), not one sample.
GLINTFX_TEST(parse_value_the_three_universal_keywords_decode_as_plain_keywords) {
    constexpr std::string_view k_universal_keywords[] = {"inherit", "initial", "unset"};
    int checked = 0;
    for (const std::string_view keyword : k_universal_keywords) {
        const value_parse_result result = parse_first_value(keyword);
        GLINTFX_CHECK(result.ok);
        GLINTFX_CHECK(result.value.kind == gltfx_gfss_value_kind::keyword);
        GLINTFX_CHECK(result.value.keyword_text == keyword);
        ++checked;
    }
    std::println("parse_value_the_three_universal_keywords_decode_as_plain_keywords: {} "
                 "keyword(s) checked",
                 checked);
}

// --- number/integer nature ---------------------------------------------

GLINTFX_TEST(parse_value_number_token_without_decimal_or_exponent_decodes_to_integer) {
    const value_parse_result result = parse_first_value("42");
    GLINTFX_CHECK(result.ok);
    GLINTFX_CHECK(result.value.kind == gltfx_gfss_value_kind::integer);
    GLINTFX_CHECK_EQ(result.value.integer_value, 42LL);
}

GLINTFX_TEST(parse_value_negative_integer_token_decodes_to_integer_with_correct_sign) {
    const value_parse_result result = parse_first_value("-7");
    GLINTFX_CHECK(result.ok);
    GLINTFX_CHECK(result.value.kind == gltfx_gfss_value_kind::integer);
    GLINTFX_CHECK_EQ(result.value.integer_value, -7LL);
}

GLINTFX_TEST(parse_value_number_token_with_decimal_point_decodes_to_number) {
    const value_parse_result result = parse_first_value("3.5");
    GLINTFX_CHECK(result.ok);
    GLINTFX_CHECK(result.value.kind == gltfx_gfss_value_kind::number);
    GLINTFX_CHECK_EQ(result.value.number, 3.5);
}

GLINTFX_TEST(parse_value_number_token_with_exponent_decodes_to_number) {
    // CSS Syntax Module Level 3 4.3.12's own type flag: an exponent
    // marker alone (no '.') still makes the token "number", never
    // "integer" - "1e2" is 100.0, decoded through the number branch,
    // not through decode_integer_lexeme().
    const value_parse_result result = parse_first_value("1e2");
    GLINTFX_CHECK(result.ok);
    GLINTFX_CHECK(result.value.kind == gltfx_gfss_value_kind::number);
    GLINTFX_CHECK_EQ(result.value.number, 100.0);
}

GLINTFX_TEST(parse_value_integer_lexeme_overflow_saturates_instead_of_crashing) {
    // LEI ZERO's own "anti-DoS de folha hostil": a digit run this
    // project's own tokenizer happily produces (the grammar has no
    // length cap) but that overflows long long must saturate, never
    // read undefined std::from_chars output nor abort the consumer's
    // process (ESCOPO.md SS2, CORE-ERROR decision 1).
    const value_parse_result positive = parse_first_value("99999999999999999999999999999999");
    GLINTFX_CHECK(positive.ok);
    GLINTFX_CHECK(positive.value.kind == gltfx_gfss_value_kind::integer);
    GLINTFX_CHECK_EQ(positive.value.integer_value, std::numeric_limits<long long>::max());

    const value_parse_result negative = parse_first_value("-99999999999999999999999999999999");
    GLINTFX_CHECK(negative.ok);
    GLINTFX_CHECK(negative.value.kind == gltfx_gfss_value_kind::integer);
    GLINTFX_CHECK_EQ(negative.value.integer_value, std::numeric_limits<long long>::lowest());
}

// --- length nature: closed enumeration of all 12 units ---------------

namespace {
struct length_unit_dimension_sample {
    std::string_view lexeme;
    gltfx_gfss_length_unit expected_unit = gltfx_gfss_length_unit::px;
    double expected_magnitude = 0.0;
};
} // namespace

GLINTFX_TEST(parse_value_dimension_token_decodes_every_one_of_the_length_units) {
    constexpr length_unit_dimension_sample k_samples[] = {
        {"5px", gltfx_gfss_length_unit::px, 5.0},     {"5dp", gltfx_gfss_length_unit::dp, 5.0},
        {"5em", gltfx_gfss_length_unit::em, 5.0},     {"5rem", gltfx_gfss_length_unit::rem, 5.0},
        {"5ex", gltfx_gfss_length_unit::ex, 5.0},     {"5ch", gltfx_gfss_length_unit::ch, 5.0},
        {"5lh", gltfx_gfss_length_unit::lh, 5.0},     {"5vw", gltfx_gfss_length_unit::vw, 5.0},
        {"5vh", gltfx_gfss_length_unit::vh, 5.0},     {"5vmin", gltfx_gfss_length_unit::vmin, 5.0},
        {"5vmax", gltfx_gfss_length_unit::vmax, 5.0}, {"5in", gltfx_gfss_length_unit::in, 5.0},
        {"5cm", gltfx_gfss_length_unit::cm, 5.0},     {"5mm", gltfx_gfss_length_unit::mm, 5.0},
        {"5pt", gltfx_gfss_length_unit::pt, 5.0},     {"5pc", gltfx_gfss_length_unit::pc, 5.0},
    };
    // GODS_LAWS.md L-40: closed against value.hpp's own mechanically
    // counted enum, not a hand-copied literal - the SAME discipline
    // the two name-covering tests above already apply.
    static_assert(sizeof(k_samples) / sizeof(k_samples[0]) ==
                      glintfx::style::gltfx_gfss_length_unit_count,
                  "GODS_LAWS.md L-40: keep this table in sync with value.hpp's own enum, never "
                  "sample a subset");

    int checked = 0;
    for (const length_unit_dimension_sample &sample : k_samples) {
        const value_parse_result result = parse_first_value(sample.lexeme);
        GLINTFX_CHECK(result.ok);
        GLINTFX_CHECK(result.value.kind == gltfx_gfss_value_kind::length);
        GLINTFX_CHECK(result.value.length.unit == sample.expected_unit);
        GLINTFX_CHECK_EQ(result.value.length.magnitude, sample.expected_magnitude);
        ++checked;
    }
    std::println(
        "parse_value_dimension_token_decodes_every_one_of_the_length_units: {} unit(s) checked",
        checked);
}

// --- angle nature: closed enumeration of all 4 units -----------------

namespace {
struct angle_unit_dimension_sample {
    std::string_view lexeme;
    gltfx_gfss_angle_unit expected_unit = gltfx_gfss_angle_unit::deg;
    double expected_magnitude = 0.0;
};
} // namespace

GLINTFX_TEST(parse_value_dimension_token_decodes_every_one_of_the_angle_units) {
    constexpr angle_unit_dimension_sample k_samples[] = {
        {"90deg", gltfx_gfss_angle_unit::deg, 90.0},
        {"1.5rad", gltfx_gfss_angle_unit::rad, 1.5},
        {"200grad", gltfx_gfss_angle_unit::grad, 200.0},
        {"0.5turn", gltfx_gfss_angle_unit::turn, 0.5},
    };
    static_assert(sizeof(k_samples) / sizeof(k_samples[0]) ==
                      glintfx::style::gltfx_gfss_angle_unit_count,
                  "GODS_LAWS.md L-40: keep this table in sync with value.hpp's own enum, never "
                  "sample a subset");

    int checked = 0;
    for (const angle_unit_dimension_sample &sample : k_samples) {
        const value_parse_result result = parse_first_value(sample.lexeme);
        GLINTFX_CHECK(result.ok);
        GLINTFX_CHECK(result.value.kind == gltfx_gfss_value_kind::angle);
        GLINTFX_CHECK(result.value.angle.unit == sample.expected_unit);
        GLINTFX_CHECK_EQ(result.value.angle.magnitude, sample.expected_magnitude);
        ++checked;
    }
    std::println(
        "parse_value_dimension_token_decodes_every_one_of_the_angle_units: {} unit(s) checked",
        checked);
}

// --- time nature: closed enumeration of all 6 units -------------------

namespace {
struct time_unit_dimension_sample {
    std::string_view lexeme;
    gltfx_gfss_time_unit expected_unit = gltfx_gfss_time_unit::ms;
    double expected_magnitude = 0.0;
};
} // namespace

GLINTFX_TEST(parse_value_dimension_token_decodes_every_one_of_the_time_units) {
    constexpr time_unit_dimension_sample k_samples[] = {
        {"150ms", gltfx_gfss_time_unit::ms, 150.0},
        {"1.5s", gltfx_gfss_time_unit::s, 1.5},
        {"5min", gltfx_gfss_time_unit::min, 5.0},
        {"2h", gltfx_gfss_time_unit::h, 2.0},
        {"400ns", gltfx_gfss_time_unit::ns, 400.0},
        // ESCOPO.md SS2, GFSS-VALUE-2 decision 3: `frames` is a
        // duration alias resolved elsewhere (GFSS-RESOLVE) - THIS
        // layer preserves the magnitude EXACTLY as written (3.0, never
        // the 50.0 ms a 60 Hz conversion would produce). See
        // parse_value_frames_unit_preserves_written_magnitude_without_
        // 60hz_conversion below for the directed proof of that "no
        // conversion here" boundary.
        {"3frames", gltfx_gfss_time_unit::frames, 3.0},
    };
    static_assert(sizeof(k_samples) / sizeof(k_samples[0]) ==
                      glintfx::style::gltfx_gfss_time_unit_count,
                  "GODS_LAWS.md L-40: keep this table in sync with value.hpp's own enum, never "
                  "sample a subset");

    int checked = 0;
    for (const time_unit_dimension_sample &sample : k_samples) {
        const value_parse_result result = parse_first_value(sample.lexeme);
        GLINTFX_CHECK(result.ok);
        GLINTFX_CHECK(result.value.kind == gltfx_gfss_value_kind::time);
        GLINTFX_CHECK(result.value.duration.unit == sample.expected_unit);
        GLINTFX_CHECK_EQ(result.value.duration.magnitude, sample.expected_magnitude);
        ++checked;
    }
    std::println(
        "parse_value_dimension_token_decodes_every_one_of_the_time_units: {} unit(s) checked",
        checked);
}

// ESCOPO.md SS2, GFSS-VALUE-2 decision 3, the leader's own verbatim
// consequence: `frames` is an APELIDO DE DURACAO, never "one real
// frame of this monitor" - this fatia does NOT do the 60 Hz
// arithmetic. "3frames" must read back magnitude 3.0, unit frames -
// never 50.0 (the converted millisecond value a resolved reading would
// carry).
GLINTFX_TEST(parse_value_frames_unit_preserves_written_magnitude_without_60hz_conversion) {
    const value_parse_result result = parse_first_value("3frames");
    GLINTFX_CHECK(result.ok);
    GLINTFX_CHECK(result.value.kind == gltfx_gfss_value_kind::time);
    GLINTFX_CHECK(result.value.duration.unit == gltfx_gfss_time_unit::frames);
    GLINTFX_CHECK_EQ(result.value.duration.magnitude, 3.0);
    GLINTFX_CHECK(result.value.duration.magnitude != 50.0);
}

// CTO design note (/var/tmp/glintfx-plan/valor-angulo-tempo.md SS2.2):
// "5min" IS a value component (a <dimension-token>, unit "min") -
// "min(" is a DIFFERENT token kind (a <function-token>) by the
// tokenizer's own grammar, decided by whether '(' immediately follows
// the ident with no digit run before it. The pair the CTO's own note
// names by name: the two must never collide.
GLINTFX_TEST(parse_value_time_unit_min_does_not_collide_with_min_function) {
    const value_parse_result dimension = parse_first_value("5min");
    GLINTFX_CHECK(dimension.ok);
    GLINTFX_CHECK(dimension.value.kind == gltfx_gfss_value_kind::time);
    GLINTFX_CHECK(dimension.value.duration.unit == gltfx_gfss_time_unit::min);
    GLINTFX_CHECK_EQ(dimension.value.duration.magnitude, 5.0);

    // "min(" tokenizes as a <function-token>, not a component value
    // this fatia's own scope covers (value_parse.hpp's own header
    // comment) - a diagnosable defect, the SAME path every other
    // unsupported token kind already takes.
    const value_parse_result function_call = parse_first_value("min(50%, 300px)");
    check_value_failure(function_call, glintfx::style::detail::k_expected_component_value);
}

// CSS units are ASCII case-insensitive (named_colors.hpp's own cited
// spec rule, reused here for unit matching too).
GLINTFX_TEST(parse_value_dimension_token_unit_match_is_ascii_case_insensitive) {
    const value_parse_result result = parse_first_value("12PX");
    GLINTFX_CHECK(result.ok);
    GLINTFX_CHECK(result.value.kind == gltfx_gfss_value_kind::length);
    GLINTFX_CHECK(result.value.length.unit == gltfx_gfss_length_unit::px);
    GLINTFX_CHECK_EQ(result.value.length.magnitude, 12.0);
}

// GFSS-VALUE-2: renamed from k_expected_known_length_unit
// (diagnostic_vocabulary.hpp's own header comment on why) - "5foo" is
// tried against all THREE closed families (length, angle, time) and
// matches none, so the diagnostic no longer claims to know the author
// meant "length" specifically.
GLINTFX_TEST(parse_value_dimension_token_with_unknown_unit_fails_with_diagnostic) {
    const value_parse_result result = parse_first_value("5foo");
    check_value_failure(result, glintfx::style::detail::k_expected_known_dimension_unit);
    GLINTFX_CHECK_EQ(result.diagnostic.line, static_cast<std::uint32_t>(1));
    GLINTFX_CHECK_EQ(result.diagnostic.column, static_cast<std::uint32_t>(1));
}

// --- magnitude saturation across the five non-integer natures --------
// (GFSS-VALUE regression: an adversarial mutation review found this
// suite protected ONLY the integer overflow path
// (parse_value_integer_lexeme_overflow_saturates_instead_of_crashing
// above) - decode_number_lexeme()'s own saturate_out_of_range_number()
// (numeric_lexeme.cpp) feeds number/length/percentage/angle/duration
// too, and none of those five had a directed test. The gap was real:
// neutralizing saturate_out_of_range_number()'s own out-of-range branch
// left this suite 100% green while a live probe showed "1e400px"
// decoding to magnitude 0.0 instead of the type's max -
// gfss_color_parse_test.cpp (an UNRELATED fatia) was the only test in
// the whole repository that happened to exercise "1e400" at all. Each
// case below checks the SATURATED VALUE itself, not just "did not
// crash" - a mutated saturate_out_of_range_number() that always returns
// 0.0 must fail the overflow rows (0.0 != max()/lowest()), even though
// it would still pass the underflow row by coincidence.

GLINTFX_TEST(parse_value_number_magnitude_out_of_range_saturates_instead_of_crashing) {
    const value_parse_result positive = parse_first_value("1e400");
    GLINTFX_CHECK(positive.ok);
    GLINTFX_CHECK(positive.value.kind == gltfx_gfss_value_kind::number);
    GLINTFX_CHECK_EQ(positive.value.number, std::numeric_limits<double>::max());

    const value_parse_result negative = parse_first_value("-1e400");
    GLINTFX_CHECK(negative.ok);
    GLINTFX_CHECK(negative.value.kind == gltfx_gfss_value_kind::number);
    GLINTFX_CHECK_EQ(negative.value.number, std::numeric_limits<double>::lowest());

    // Magnitude too SMALL to represent (underflow) rounds toward zero,
    // never toward the extreme - numeric_lexeme.cpp's own
    // saturate_out_of_range_number() comment.
    const value_parse_result underflow = parse_first_value("1e-400");
    GLINTFX_CHECK(underflow.ok);
    GLINTFX_CHECK(underflow.value.kind == gltfx_gfss_value_kind::number);
    GLINTFX_CHECK_EQ(underflow.value.number, 0.0);
}

GLINTFX_TEST(parse_value_length_magnitude_out_of_range_saturates_instead_of_crashing) {
    const value_parse_result positive = parse_first_value("1e400px");
    GLINTFX_CHECK(positive.ok);
    GLINTFX_CHECK(positive.value.kind == gltfx_gfss_value_kind::length);
    GLINTFX_CHECK_EQ(positive.value.length.magnitude, std::numeric_limits<double>::max());

    const value_parse_result negative = parse_first_value("-1e400px");
    GLINTFX_CHECK(negative.ok);
    GLINTFX_CHECK(negative.value.kind == gltfx_gfss_value_kind::length);
    GLINTFX_CHECK_EQ(negative.value.length.magnitude, std::numeric_limits<double>::lowest());

    const value_parse_result underflow = parse_first_value("1e-400px");
    GLINTFX_CHECK(underflow.ok);
    GLINTFX_CHECK(underflow.value.kind == gltfx_gfss_value_kind::length);
    GLINTFX_CHECK_EQ(underflow.value.length.magnitude, 0.0);
}

GLINTFX_TEST(parse_value_percentage_magnitude_out_of_range_saturates_instead_of_crashing) {
    const value_parse_result positive = parse_first_value("1e400%");
    GLINTFX_CHECK(positive.ok);
    GLINTFX_CHECK(positive.value.kind == gltfx_gfss_value_kind::percentage);
    GLINTFX_CHECK_EQ(positive.value.percentage, std::numeric_limits<double>::max());

    const value_parse_result negative = parse_first_value("-1e400%");
    GLINTFX_CHECK(negative.ok);
    GLINTFX_CHECK(negative.value.kind == gltfx_gfss_value_kind::percentage);
    GLINTFX_CHECK_EQ(negative.value.percentage, std::numeric_limits<double>::lowest());

    const value_parse_result underflow = parse_first_value("1e-400%");
    GLINTFX_CHECK(underflow.ok);
    GLINTFX_CHECK(underflow.value.kind == gltfx_gfss_value_kind::percentage);
    GLINTFX_CHECK_EQ(underflow.value.percentage, 0.0);
}

GLINTFX_TEST(parse_value_angle_magnitude_out_of_range_saturates_instead_of_crashing) {
    const value_parse_result positive = parse_first_value("1e400deg");
    GLINTFX_CHECK(positive.ok);
    GLINTFX_CHECK(positive.value.kind == gltfx_gfss_value_kind::angle);
    GLINTFX_CHECK_EQ(positive.value.angle.magnitude, std::numeric_limits<double>::max());

    const value_parse_result negative = parse_first_value("-1e400deg");
    GLINTFX_CHECK(negative.ok);
    GLINTFX_CHECK(negative.value.kind == gltfx_gfss_value_kind::angle);
    GLINTFX_CHECK_EQ(negative.value.angle.magnitude, std::numeric_limits<double>::lowest());

    const value_parse_result underflow = parse_first_value("1e-400deg");
    GLINTFX_CHECK(underflow.ok);
    GLINTFX_CHECK(underflow.value.kind == gltfx_gfss_value_kind::angle);
    GLINTFX_CHECK_EQ(underflow.value.angle.magnitude, 0.0);
}

GLINTFX_TEST(parse_value_duration_magnitude_out_of_range_saturates_instead_of_crashing) {
    const value_parse_result positive = parse_first_value("1e400ms");
    GLINTFX_CHECK(positive.ok);
    GLINTFX_CHECK(positive.value.kind == gltfx_gfss_value_kind::time);
    GLINTFX_CHECK_EQ(positive.value.duration.magnitude, std::numeric_limits<double>::max());

    const value_parse_result negative = parse_first_value("-1e400ms");
    GLINTFX_CHECK(negative.ok);
    GLINTFX_CHECK(negative.value.kind == gltfx_gfss_value_kind::time);
    GLINTFX_CHECK_EQ(negative.value.duration.magnitude, std::numeric_limits<double>::lowest());

    const value_parse_result underflow = parse_first_value("1e-400ms");
    GLINTFX_CHECK(underflow.ok);
    GLINTFX_CHECK(underflow.value.kind == gltfx_gfss_value_kind::time);
    GLINTFX_CHECK_EQ(underflow.value.duration.magnitude, 0.0);
}

// --- unsupported token kind ---------------------------------------------

GLINTFX_TEST(parse_value_unsupported_token_kind_fails_with_component_value_diagnostic) {
    // A <string-token> is a real gfss token (GFSS-TOKEN's own closed
    // vocabulary), just not one of the four this fatia's own scope
    // covers (value_parse.hpp's own header comment).
    const value_parse_result result = parse_first_value("\"not a value component\"");
    check_value_failure(result, glintfx::style::detail::k_expected_component_value);
}

// --- THE PAIR THE ORDER OF SERVICE NAMES: neither collapse happens --

// "numero nao colapsa em comprimento": a bare <number>/<integer> and a
// <length> with an EXPLICIT unit are DIFFERENT natures with DIFFERENT
// fields populated - a bare number never silently becomes "px" (or any
// other implied unit), and its own gltfx_gfss_value::length field stays
// at its DEFAULT (0.0, unit::px), never fabricated from the number
// field.
GLINTFX_TEST(parse_value_bare_number_never_collapses_into_length) {
    const value_parse_result bare_number = parse_first_value("42");
    GLINTFX_CHECK(bare_number.ok);
    GLINTFX_CHECK(bare_number.value.kind == gltfx_gfss_value_kind::integer);
    GLINTFX_CHECK(bare_number.value.kind != gltfx_gfss_value_kind::length);
    // The length field was NEVER TOUCHED for a bare number - it reads
    // back as the type's own default, not as "42px" fabricated from
    // integer_value.
    GLINTFX_CHECK_EQ(bare_number.value.length.magnitude, 0.0);
    GLINTFX_CHECK(bare_number.value.length.unit == gltfx_gfss_length_unit::px);

    const value_parse_result explicit_length = parse_first_value("42px");
    GLINTFX_CHECK(explicit_length.ok);
    GLINTFX_CHECK(explicit_length.value.kind == gltfx_gfss_value_kind::length);
    GLINTFX_CHECK(explicit_length.value.kind != gltfx_gfss_value_kind::integer);
    GLINTFX_CHECK_EQ(explicit_length.value.length.magnitude, 42.0);
    // The bare-number reading's OWN integer_value is untouched here too
    // - the two natures never leak into each other's field.
    GLINTFX_CHECK_EQ(explicit_length.value.integer_value, 0LL);
}

// "porcentagem nao vira valor absoluto": a <percentage> is its own
// nature, never re-typed as ::length (no implied unit) and never
// stored as the [0,1] ratio a RESOLVED absolute value would use - the
// RAW magnitude before the '%' sign is what this decode step keeps.
GLINTFX_TEST(parse_value_percentage_preserves_percentage_never_becomes_length_or_ratio) {
    const value_parse_result result = parse_first_value("50%");
    GLINTFX_CHECK(result.ok);
    GLINTFX_CHECK(result.value.kind == gltfx_gfss_value_kind::percentage);
    GLINTFX_CHECK(result.value.kind != gltfx_gfss_value_kind::length);
    // The RAW magnitude, not the [0,1] ratio a resolved absolute value
    // would use (GFSS-RESOLVE's own later job) - "50%" reads back 50.0.
    GLINTFX_CHECK_EQ(result.value.percentage, 50.0);
    // Never silently multiplied down to a ratio.
    GLINTFX_CHECK(result.value.percentage != 0.5);
    // The length field was never populated with an implied unit.
    GLINTFX_CHECK_EQ(result.value.length.magnitude, 0.0);
}

// GFSS-VALUE-2's own THIRD pair, AMPLIANDO the two above (order of
// service, achado 6 of the review that sent this fatia back): "angulo,
// tempo e comprimento nao se confundem entre si" - length, angle and
// time are THREE separate fields (value.hpp's own top comment on why
// they are not one shared enum/struct), and decoding one nature must
// never populate either of the other two's own field.
GLINTFX_TEST(parse_value_length_angle_and_time_never_populate_each_others_field) {
    const value_parse_result length = parse_first_value("42px");
    GLINTFX_CHECK(length.ok);
    GLINTFX_CHECK(length.value.kind == gltfx_gfss_value_kind::length);
    GLINTFX_CHECK_EQ(length.value.length.magnitude, 42.0);
    // Angle/time fields stay at THEIR OWN defaults - never fabricated
    // from the length that was actually decoded.
    GLINTFX_CHECK_EQ(length.value.angle.magnitude, 0.0);
    GLINTFX_CHECK(length.value.angle.unit == gltfx_gfss_angle_unit::deg);
    GLINTFX_CHECK_EQ(length.value.duration.magnitude, 0.0);
    GLINTFX_CHECK(length.value.duration.unit == gltfx_gfss_time_unit::ms);

    const value_parse_result angle = parse_first_value("0.5turn");
    GLINTFX_CHECK(angle.ok);
    GLINTFX_CHECK(angle.value.kind == gltfx_gfss_value_kind::angle);
    // "0.5turn" is NOT normalized to 180.0 deg here (GFSS-RESOLVE's own
    // future job) - the WRITTEN unit and magnitude are preserved.
    GLINTFX_CHECK_EQ(angle.value.angle.magnitude, 0.5);
    GLINTFX_CHECK(angle.value.angle.unit == gltfx_gfss_angle_unit::turn);
    // Length/time fields stay at their own defaults.
    GLINTFX_CHECK_EQ(angle.value.length.magnitude, 0.0);
    GLINTFX_CHECK_EQ(angle.value.duration.magnitude, 0.0);
    GLINTFX_CHECK(angle.value.duration.unit == gltfx_gfss_time_unit::ms);

    const value_parse_result time = parse_first_value("40s");
    GLINTFX_CHECK(time.ok);
    GLINTFX_CHECK(time.value.kind == gltfx_gfss_value_kind::time);
    GLINTFX_CHECK_EQ(time.value.duration.magnitude, 40.0);
    GLINTFX_CHECK(time.value.duration.unit == gltfx_gfss_time_unit::s);
    // Length/angle fields stay at their own defaults.
    GLINTFX_CHECK_EQ(time.value.length.magnitude, 0.0);
    GLINTFX_CHECK_EQ(time.value.angle.magnitude, 0.0);
    GLINTFX_CHECK(time.value.angle.unit == gltfx_gfss_angle_unit::deg);
}

// --- the two value_parse-owned diagnostics, bound to the shared list -

// GFSS-VOCAB-BIND (TODO.md, GODS_LAWS.md L-40): parse_value_unsupported_
// token_kind_fails_with_component_value_diagnostic and parse_value_
// dimension_token_with_unknown_unit_fails_with_diagnostic above already
// prove component_value/known_dimension_unit for real - what was
// missing was a COMPILE-TIME floor tying "how many identifiers this
// layer owns" to those directed checks, the same floor gfss_tokenizer_
// test.cpp's own k_tokenizer_diagnostic_samples and gfss_selector_
// parse_test.cpp's own k_selector_diagnostic_samples now have.
// count_owned_by(value_parse) is counted MECHANICALLY from diagnostic_
// vocabulary.hpp's own shared list - an identifier added under this
// producer with no matching row in k_value_diagnostic_samples below now
// FAILS TO COMPILE.
GLINTFX_TEST(parse_value_diagnostics_are_produced_from_the_shared_vocabulary) {
    struct diagnostic_sample {
        std::string_view source;
        std::string_view expected_identifier;
    };
    static constexpr diagnostic_sample k_value_diagnostic_samples[] = {
        {"\"not a value component\"", glintfx::style::detail::k_expected_component_value},
        {"5foo", glintfx::style::detail::k_expected_known_dimension_unit},
    };
    static_assert(sizeof(k_value_diagnostic_samples) / sizeof(k_value_diagnostic_samples[0]) ==
                      count_owned_by(gfss_diagnostic_producer::value_parse),
                  "GODS_LAWS.md L-40 (GFSS-VOCAB-BIND): diagnostic_vocabulary.hpp's "
                  "value_parse-owned identifiers changed - add a directed production row to "
                  "k_value_diagnostic_samples above, this does not compile otherwise");

    std::size_t swept = 0;
    for (const diagnostic_sample &sample : k_value_diagnostic_samples) {
        check_value_failure(parse_first_value(sample.source), sample.expected_identifier);
        ++swept;
    }
    GLINTFX_CHECK_EQ(swept, count_owned_by(gfss_diagnostic_producer::value_parse));
}
