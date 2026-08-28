// SPDX-License-Identifier: AGPL-3.0-or-later
#include "value_parse.hpp"

#include <array>
#include <charconv>
#include <limits>

#include <glintfx/gfss/tokenizer.hpp>

#include "diagnostic_vocabulary.hpp"
#include "lexical_rules.hpp"
#include "named_colors.hpp"
#include "numeric_lexeme.hpp"

// value_parse.cpp - GFSS-VALUE (TODO.md, GODS_LAWS.md L-17: each
// function below answers exactly one question of value_parse.hpp's own
// header comment scope - one token kind decoded into its own nature,
// never more than one per function).

namespace glintfx::style::detail {

namespace {

// --- shared plumbing -------------------------------------------------

gltfx_gfss_diagnostic make_diagnostic(const gltfx_gfss_token &token,
                                      std::string_view expected) noexcept {
    return gltfx_gfss_diagnostic{.line = token.line, .column = token.column, .expected = expected};
}

value_parse_result fail_at(const gltfx_gfss_token &token, std::string_view expected) noexcept {
    return value_parse_result{
        .ok = false, .value = {}, .diagnostic = make_diagnostic(token, expected)};
}

value_parse_result succeed(const gltfx_gfss_value &value) noexcept {
    return value_parse_result{.ok = true, .value = value, .diagnostic = {}};
}

// --- number/integer nature ---------------------------------------------

// CSS Syntax Module Level 3 4.3.12's own "type" flag: a <number-token>
// is "integer" unless its lexeme contains a decimal point or an
// exponent marker - value.hpp's own gltfx_gfss_value_kind::number vs
// ::integer split is a DIRECT reading of that flag (ESCOPO.md SS2
// decision 5: "<integer>, distinto de <number>"). Safe to scan the
// WHOLE lexeme: a <number-token>'s own lexeme (unlike a <dimension-
// token>'s) is EXACTLY the number grammar's own consumed span
// (lexical_rules.hpp's own consume_number()), nothing else appended.
bool number_lexeme_is_integer(std::string_view lexeme) noexcept {
    return lexeme.find_first_of(".eE") == std::string_view::npos;
}

// Parses a <number-token>'s OWN lexeme, already known to hold no '.'/
// exponent (number_lexeme_is_integer() above), into a signed integer
// magnitude - std::from_chars's INTEGER overload directly, never
// through numeric_lexeme.hpp's own double path (which would round-trip
// a large integer lossily through a 53-bit mantissa for no reason: a
// plain digit run has an EXACT integer reading std::from_chars can
// give directly). Saturates on overflow instead of failing - the SAME
// "the library never aborts the consumer's process on hostile external
// text" principle numeric_lexeme.cpp's own decode_number_lexeme()
// already applies, at the integer domain instead of the double one.
long long decode_integer_lexeme(std::string_view lexeme) noexcept {
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

value_parse_result number_token_value(const gltfx_gfss_token &token) noexcept {
    gltfx_gfss_value value{};
    if (number_lexeme_is_integer(token.lexeme)) {
        value.kind = gltfx_gfss_value_kind::integer;
        value.integer_value = decode_integer_lexeme(token.lexeme);
    } else {
        value.kind = gltfx_gfss_value_kind::number;
        value.number = decode_number_lexeme(token.lexeme);
    }
    return succeed(value);
}

// --- percentage nature ---------------------------------------------

value_parse_result percentage_token_value(const gltfx_gfss_token &token) noexcept {
    gltfx_gfss_value value{};
    value.kind = gltfx_gfss_value_kind::percentage;
    // RAW magnitude before the '%' sign - "50%" decodes to 50.0, never
    // 0.5 (value.hpp's own header comment on gltfx_gfss_value::
    // percentage; GFSS-RESOLVE's job to multiply by 0.01 later).
    value.percentage = decode_percentage_lexeme(token.lexeme);
    return succeed(value);
}

// --- keyword nature ---------------------------------------------

value_parse_result ident_token_value(const gltfx_gfss_token &token) noexcept {
    gltfx_gfss_value value{};
    value.kind = gltfx_gfss_value_kind::keyword;
    // Raw ident text, INCLUDING the three universal keywords (value.hpp
    // own header comment: their semantics live in GFSS-INHERIT, not
    // here).
    value.keyword_text = token.lexeme;
    return succeed(value);
}

// --- length nature ---------------------------------------------

struct length_unit_lookup_entry {
    std::string_view name;
    gltfx_gfss_length_unit unit = gltfx_gfss_length_unit::px;
};

// THE lookup table - 16 rows, one per value.hpp's own closed
// gltfx_gfss_length_unit enum (see that header's own comment on the
// "treze"/12/16 provenance - GFSS-VALUE-2, 28/08/2026, grew this from
// 12 to 16: ch, lh, vmin, vmax). static_assert below is this file's
// OWN half of the "add a unit without registering it, watch it fail to
// compile" proof - value_kind.cpp's own k_length_unit_table is the
// OTHER half (the name<->enum round trip); this table is the third and
// last place a 17th entry would need a matching row, and the size
// literal (16, not gltfx_gfss_length_unit_count itself) is what makes
// forgetting one fail to build rather than silently mismatch two out
// of three tables
// - the SAME technique color_parse.cpp's own try_match_color_function_
// kind() already established for its own 4-entry function-name lookup.
constexpr std::array<length_unit_lookup_entry, 16> k_length_unit_lookup{{
    {"px", gltfx_gfss_length_unit::px},
    {"dp", gltfx_gfss_length_unit::dp},
    {"em", gltfx_gfss_length_unit::em},
    {"rem", gltfx_gfss_length_unit::rem},
    {"ex", gltfx_gfss_length_unit::ex},
    {"ch", gltfx_gfss_length_unit::ch},
    {"lh", gltfx_gfss_length_unit::lh},
    {"vw", gltfx_gfss_length_unit::vw},
    {"vh", gltfx_gfss_length_unit::vh},
    {"vmin", gltfx_gfss_length_unit::vmin},
    {"vmax", gltfx_gfss_length_unit::vmax},
    {"in", gltfx_gfss_length_unit::in},
    {"cm", gltfx_gfss_length_unit::cm},
    {"mm", gltfx_gfss_length_unit::mm},
    {"pt", gltfx_gfss_length_unit::pt},
    {"pc", gltfx_gfss_length_unit::pc},
}};

static_assert(k_length_unit_lookup.size() == gltfx_gfss_length_unit_count,
              "GODS_LAWS.md L-40: k_length_unit_lookup's row count must track value.hpp's own "
              "gltfx_gfss_length_unit_count - a unit added to the enum without a lookup row here "
              "must not compile silently");

// --- angle nature (GFSS-VALUE-2, ESCOPO.md SS2 decision 2) -----------

struct angle_unit_lookup_entry {
    std::string_view name;
    gltfx_gfss_angle_unit unit = gltfx_gfss_angle_unit::deg;
};

// THE lookup table - 4 rows, one per value.hpp's own closed
// gltfx_gfss_angle_unit enum. Same "size literal, not the mechanical
// count itself" discipline as k_length_unit_lookup above.
constexpr std::array<angle_unit_lookup_entry, 4> k_angle_unit_lookup{{
    {"deg", gltfx_gfss_angle_unit::deg},
    {"rad", gltfx_gfss_angle_unit::rad},
    {"grad", gltfx_gfss_angle_unit::grad},
    {"turn", gltfx_gfss_angle_unit::turn},
}};

static_assert(k_angle_unit_lookup.size() == gltfx_gfss_angle_unit_count,
              "GODS_LAWS.md L-40: k_angle_unit_lookup's row count must track value.hpp's own "
              "gltfx_gfss_angle_unit_count - a unit added to the enum without a lookup row here "
              "must not compile silently");

bool try_match_angle_unit(std::string_view name, gltfx_gfss_angle_unit &out_unit) noexcept {
    for (const angle_unit_lookup_entry &entry : k_angle_unit_lookup) {
        if (ascii_case_insensitive_equal(entry.name, name)) {
            out_unit = entry.unit;
            return true;
        }
    }
    return false;
}

// --- time nature (GFSS-VALUE-2, ESCOPO.md SS2 decisions 1 and 3) -----

struct time_unit_lookup_entry {
    std::string_view name;
    gltfx_gfss_time_unit unit = gltfx_gfss_time_unit::ms;
};

// THE lookup table - 6 rows, one per value.hpp's own closed
// gltfx_gfss_time_unit enum (the five SI-symbol spellings plus this
// library's own `frames` - value.hpp's own header comment on that
// unit's non-obvious semantics).
constexpr std::array<time_unit_lookup_entry, 6> k_time_unit_lookup{{
    {"ms", gltfx_gfss_time_unit::ms},
    {"s", gltfx_gfss_time_unit::s},
    {"min", gltfx_gfss_time_unit::min},
    {"h", gltfx_gfss_time_unit::h},
    {"ns", gltfx_gfss_time_unit::ns},
    {"frames", gltfx_gfss_time_unit::frames},
}};

static_assert(k_time_unit_lookup.size() == gltfx_gfss_time_unit_count,
              "GODS_LAWS.md L-40: k_time_unit_lookup's row count must track value.hpp's own "
              "gltfx_gfss_time_unit_count - a unit added to the enum without a lookup row here "
              "must not compile silently");

bool try_match_time_unit(std::string_view name, gltfx_gfss_time_unit &out_unit) noexcept {
    for (const time_unit_lookup_entry &entry : k_time_unit_lookup) {
        if (ascii_case_insensitive_equal(entry.name, name)) {
            out_unit = entry.unit;
            return true;
        }
    }
    return false;
}

// ASCII case-insensitive match (CSS Syntax Module Level 3's own
// <ident-token> match rule - named_colors.hpp's own header comment
// already cites the spec section; reused here for a THIRD call site,
// well past CONTRACT.md SS6's "three occurrences" bar for a shared
// helper).
bool try_match_length_unit(std::string_view name, gltfx_gfss_length_unit &out_unit) noexcept {
    for (const length_unit_lookup_entry &entry : k_length_unit_lookup) {
        if (ascii_case_insensitive_equal(entry.name, name)) {
            out_unit = entry.unit;
            return true;
        }
    }
    return false;
}

struct dimension_split {
    std::string_view number_part;
    std::string_view unit_part;
};

// Re-derives the boundary consume_numeric_token() (tokenizer.cpp)
// already found once while PRODUCING this dimension token, but did not
// RECORD as two separate spans (token.hpp's own "lexeme is the raw
// source span" scope decision: a dimension's lexeme is the
// CONCATENATION of the number and the unit, never kept apart). Runs
// the SAME lexical_rules.hpp::consume_number() grammar a second time,
// over a throwaway cursor built on just THIS token's own lexeme - never
// the caller's whole source buffer - so the split can never disagree
// with how the tokenizer itself found the boundary the first time (a
// hand-rolled second implementation of "what does a number look like"
// here would risk exactly that drift, the same duplication risk
// numeric_lexeme.hpp's own header comment names for a different pair
// of functions).
dimension_split split_dimension_lexeme(std::string_view lexeme) noexcept {
    gltfx_gfss_cursor cursor{.source = lexeme};
    consume_number(cursor);
    return dimension_split{.number_part = lexeme.substr(0, cursor.byte_offset),
                           .unit_part = lexeme.substr(cursor.byte_offset)};
}

// Tries the THREE closed unit families in turn - length, then angle,
// then time - never a shared "generic dimension" table (value.hpp's
// own top comment, achado 6: the three enums stay separate types).
// None of the 16+4+6 = 26 lexemes collide (verified by inspection: no
// length unit spells "deg"/"rad"/"grad"/"turn", no angle unit spells
// any of the six time spellings, and "min" - the one name close enough
// to warrant a directed test, gfss_value_test.cpp's own parse_value_
// time_unit_min_does_not_collide_with_min_function - is a TIME unit,
// never a length or angle one), so trying them in a fixed order can
// never silently mask one family behind another; the order itself
// carries no meaning beyond "try the largest table first".
value_parse_result dimension_token_value(const gltfx_gfss_token &token) noexcept {
    const dimension_split split = split_dimension_lexeme(token.lexeme);

    gltfx_gfss_length_unit length_unit = gltfx_gfss_length_unit::px;
    if (try_match_length_unit(split.unit_part, length_unit)) {
        gltfx_gfss_value value{};
        value.kind = gltfx_gfss_value_kind::length;
        value.length = gltfx_gfss_length{.magnitude = decode_number_lexeme(split.number_part),
                                         .unit = length_unit};
        return succeed(value);
    }

    gltfx_gfss_angle_unit angle_unit = gltfx_gfss_angle_unit::deg;
    if (try_match_angle_unit(split.unit_part, angle_unit)) {
        gltfx_gfss_value value{};
        value.kind = gltfx_gfss_value_kind::angle;
        value.angle = gltfx_gfss_angle{.magnitude = decode_number_lexeme(split.number_part),
                                       .unit = angle_unit};
        return succeed(value);
    }

    gltfx_gfss_time_unit time_unit = gltfx_gfss_time_unit::ms;
    if (try_match_time_unit(split.unit_part, time_unit)) {
        gltfx_gfss_value value{};
        value.kind = gltfx_gfss_value_kind::time;
        // `frames` decodes through this SAME branch as every other
        // time unit, magnitude preserved EXACTLY as written - no 60 Hz
        // arithmetic here (value.hpp's own gltfx_gfss_value::duration
        // comment: that conversion is GFSS-RESOLVE's job).
        value.duration = gltfx_gfss_time{.magnitude = decode_number_lexeme(split.number_part),
                                         .unit = time_unit};
        return succeed(value);
    }

    // None of the three closed families recognized this dimension's
    // own unit text - a single GENERIC diagnostic, never one that
    // falsely narrows to "length" (this project's own GODS_LAWS.md
    // L-27: never claim to know more than was actually determined -
    // at this point the parser has ALREADY tried all three families
    // and none matched, so naming just one of them would mislead
    // whoever reads the diagnostic about which unit the author might
    // have meant).
    return fail_at(token, k_expected_known_dimension_unit);
}

} // namespace

value_parse_result parse_value(const gltfx_gfss_token &token) noexcept {
    if (token.kind == gltfx_gfss_token_kind::ident) {
        return ident_token_value(token);
    }
    if (token.kind == gltfx_gfss_token_kind::number) {
        return number_token_value(token);
    }
    if (token.kind == gltfx_gfss_token_kind::percentage) {
        return percentage_token_value(token);
    }
    if (token.kind == gltfx_gfss_token_kind::dimension) {
        return dimension_token_value(token);
    }
    // Every other token kind (string, hash, function, a punctuator...)
    // is not a gfss component value this fatia's own scope covers
    // (value_parse.hpp's own header comment) - a diagnosable defect,
    // never a crash on hostile external text (LEI ZERO's own "anti-DoS
    // de folha hostil").
    return fail_at(token, k_expected_component_value);
}

} // namespace glintfx::style::detail
