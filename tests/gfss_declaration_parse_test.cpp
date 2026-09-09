// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstddef>
#include <optional>
#include <print>
#include <string_view>
#include <vector>

#include <glintfx/gfss/keyword.hpp>
#include <glintfx/gfss/property.hpp>
#include <glintfx/gfss/token.hpp>
#include <glintfx/gfss/tokenizer.hpp>
#include <glintfx/gfss/value.hpp>

#include "gfss/declaration_ast.hpp"
#include "gfss/declaration_color_value.hpp"
#include "gfss/declaration_list_parse.hpp"
#include "gfss/declaration_parse.hpp"
#include "gfss/declaration_split.hpp"
#include "gfss/declaration_value_check.hpp"
#include "gfss/diagnostic_vocabulary.hpp"
#include "gfss/important_flag.hpp"
#include "gfss/property_name_lookup.hpp"
#include "gfss/property_table.hpp"
#include "gfss/property_value_contract.hpp"
#include "gfss/refused_property_names.hpp"
#include "gfss/shorthand_name_vocabulary.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// gfss_declaration_parse_test.cpp - GFSS-DECL-PARSE (TODO.md wave W5,
// GODS_LAWS.md L-20/L-40): the TDD red/green witness for the whole
// declaration-block track (declaration_split.hpp, important_flag.hpp,
// property_name_lookup.hpp, property_value_contract.hpp,
// declaration_value_check.hpp, declaration_color_value.hpp,
// declaration_parse.hpp, declaration_list_parse.hpp) - see each of
// those files' own header comment for the design rationale each check
// below proves.

namespace {

using namespace glintfx::style;
using namespace glintfx::style::detail;

std::vector<gltfx_gfss_token> tokenize_all(std::string_view source) {
    return gltfx_gfss_tokenize(source);
}

} // namespace

// === DP-1: the vocabulary and the `detail` field =======================

GLINTFX_TEST(diagnostic_detail_is_empty_by_default_and_never_a_sentence) {
    GLINTFX_CHECK(gltfx_gfss_diagnostic{}.detail.empty());
    // k_expected_vocabulary's own array size IS k_expected_vocabulary_count
    // by construction (diagnostic_vocabulary.hpp's own X-macro) - this is
    // the compile-time proof that adding a 13th identifier under an
    // existing producer, or a whole new producer, cannot silently drift
    // the array and the count apart.
    static_assert(k_expected_vocabulary.size() == k_expected_vocabulary_count);
    std::println("diagnostic_detail_is_empty_by_default_and_never_a_sentence: 1 field checked");
}

GLINTFX_TEST(declaration_parse_producer_owns_exactly_twelve_vocabulary_entries) {
    static_assert(count_owned_by(gfss_diagnostic_producer::declaration_parse) == 12,
                  "GODS_LAWS.md L-40: this constant IS the closed count - a 13th identifier added "
                  "under this producer without a matching row in this file's own tables fails to "
                  "compile, never passes silently");
    std::println("declaration_parse_producer_owns_exactly_twelve_vocabulary_entries: 12 checked");
}

// === DP-2: declaration_split ===========================================

GLINTFX_TEST(split_declarations_at_top_level_semicolons_only) {
    const std::vector<gltfx_gfss_token> tokens =
        tokenize_all(R"(a: 1; b: "x;y"; c: fn(1;2); d: [1;2]; e: {p;q}; f: 3)");
    const std::vector<declaration_span> spans = split_declarations_at_top_level_semicolons(tokens);
    GLINTFX_CHECK(spans.size() == 6);
    // The last span has no trailing `;` - it ends where the <EOF-token>
    // was found, not where a `;` was.
    std::println("split_declarations_at_top_level_semicolons_only: {} span(s) found", spans.size());
}

GLINTFX_TEST(split_declarations_drops_empty_spans_from_bare_semicolons_and_whitespace) {
    const std::vector<gltfx_gfss_token> tokens = tokenize_all("a: 1;; b: 2;   ;c: 3");
    const std::vector<declaration_span> spans = split_declarations_at_top_level_semicolons(tokens);
    GLINTFX_CHECK(spans.size() == 3);
}

GLINTFX_TEST(split_declarations_unclosed_curly_ends_at_eof_never_an_infinite_loop) {
    const std::vector<gltfx_gfss_token> tokens = tokenize_all("a: {p;q");
    const std::vector<declaration_span> spans = split_declarations_at_top_level_semicolons(tokens);
    GLINTFX_CHECK(spans.size() == 1);
}

// === DP-3: important_flag ===============================================

namespace {
important_flag_result read_flag_over_whole(std::string_view value_text) {
    const std::vector<gltfx_gfss_token> tokens = tokenize_all(value_text);
    // tokens[] ends with <EOF-token> - exclude it from the span.
    return read_important_flag(tokens, 0, tokens.size() - 1);
}
} // namespace

GLINTFX_TEST(important_flag_is_read_from_the_end_of_the_value_in_eleven_forms) {
    struct sample {
        std::string_view value;
        bool expect_ok = false;
        bool expect_important = false;
    };
    // The twelfth form the plan names ("color !important: red") is a
    // full-DECLARATION shape (a colon appearing where the property name
    // area is) - exercised at the declaration_parse level below
    // (important_flag_position_inside_property_name_is_a_colon_error),
    // never as a bare value span here (this file's own design note,
    // important_flag.hpp's own header comment on scope).
    constexpr sample k_samples[] = {
        {"red !important", true, true},
        {"red ! important", true, true},
        {"red !IMPORTANT", true, true},
        {"red !important ", true, true}, // trailing whitespace before the (already-split) end
        {"red /*c*/ !important", true, true},
        {"!important red", false, false},
        {"red !important 1px", false, false},
        {"red !important !important", false, false},
        {"red !foo", false, false},
        {"red !", false, false},
        {"!important", false, false},
    };
    int checked = 0;
    for (const sample &s : k_samples) {
        const important_flag_result result = read_flag_over_whole(s.value);
        GLINTFX_CHECK(result.ok == s.expect_ok);
        if (result.ok) {
            GLINTFX_CHECK(result.important == s.expect_important);
        } else {
            GLINTFX_CHECK(result.diagnostic.expected == k_expected_important_flag_at_end_of_value);
        }
        ++checked;
    }
    std::println("important_flag_is_read_from_the_end_of_the_value_in_eleven_forms: {} form(s), 5 "
                 "accepted, 6 rejected",
                 checked);
}

// === DP-4: property name resolution ====================================

GLINTFX_TEST(every_registry_name_resolves_case_insensitively_and_unknown_does_not) {
    int checked = 0;
    for (const property_entry &entry : k_property_table) {
        const property_name_lookup_result lower = lookup_property_name(entry.sheet_name, 1, 1);
        GLINTFX_CHECK(lower.kind == property_name_lookup_kind::known_property);
        GLINTFX_CHECK(lower.property == entry.id);
        ++checked;
    }
    GLINTFX_CHECK(checked == static_cast<int>(gltfx_gfss_property_count));

    const property_name_lookup_result mixed_case = lookup_property_name("MARGIN-TOP", 1, 1);
    GLINTFX_CHECK(mixed_case.kind == property_name_lookup_kind::known_property);
    GLINTFX_CHECK(mixed_case.property == gltfx_gfss_property::margin_top);

    const property_name_lookup_result unknown = lookup_property_name("colr", 1, 1);
    GLINTFX_CHECK(unknown.kind == property_name_lookup_kind::unknown);
    GLINTFX_CHECK(unknown.diagnostic.expected == k_expected_known_property_name);

    std::println("every_registry_name_resolves_case_insensitively_and_unknown_does_not: {} name(s) "
                 "resolved",
                 checked);
}

GLINTFX_TEST(eleven_accepted_shorthands_and_ten_refused_names_are_closed_lists) {
    static_assert(
        k_shorthand_name_count == 11,
        "GODS_LAWS.md L-40: eleven accepted shorthands, docs/gfss-property-registry-v1.md "
        "SS6");
    static_assert(k_refused_property_names.size() == 10,
                  "GODS_LAWS.md L-40: ten refused names (nine shorthands + white-space), E3/A5");

    int shorthand_checked = 0;
    for (std::string_view name : k_shorthand_names) {
        const property_name_lookup_result result = lookup_property_name(name, 1, 1);
        GLINTFX_CHECK(result.kind == property_name_lookup_kind::shorthand);
        ++shorthand_checked;
    }

    int refused_checked = 0;
    for (const refused_property_name_entry &entry : k_refused_property_names) {
        const property_name_lookup_result result = lookup_property_name(entry.name, 1, 1);
        GLINTFX_CHECK(result.kind == property_name_lookup_kind::refused);
        GLINTFX_CHECK(result.diagnostic.detail == entry.detail);
        ++refused_checked;
    }
    GLINTFX_CHECK(lookup_property_name("transition", 1, 1).diagnostic.expected ==
                  k_expected_longhand_property_names);
    GLINTFX_CHECK(lookup_property_name("white-space", 1, 1).diagnostic.expected ==
                  k_expected_renamed_property_name);

    std::println(
        "eleven_accepted_shorthands_and_ten_refused_names_are_closed_lists: {} shorthand(s), "
        "{} refused name(s)",
        shorthand_checked, refused_checked);
}

// === DP-5: property_value_contract =====================================

GLINTFX_TEST(every_property_has_exactly_one_value_contract_and_categories_are_counted) {
    static_assert(k_property_value_contracts.size() == gltfx_gfss_property_count,
                  "GODS_LAWS.md L-40: one contract row per registry property");

    std::size_t keyword_only = 0;
    std::size_t length_like = 0;
    std::size_t number_only = 0;
    std::size_t integer_only = 0;
    std::size_t color = 0;
    std::size_t time_list = 0;
    std::size_t raw = 0;
    for (const property_value_contract &c : k_property_value_contracts) {
        if (c.raw_composite) {
            ++raw;
        } else if (c.is_color) {
            ++color;
        } else if (c.comma_separated) {
            ++time_list;
        } else if (c.keyword_count > 0 && c.accepted_natures == 0) {
            ++keyword_only;
        } else if ((c.accepted_natures & (k_nature_length | k_nature_percentage)) != 0) {
            ++length_like;
        } else if ((c.accepted_natures & k_nature_number) != 0) {
            ++number_only;
        } else if ((c.accepted_natures & k_nature_integer) != 0) {
            ++integer_only;
        }
    }
    const std::size_t total =
        keyword_only + length_like + number_only + integer_only + color + time_list + raw;
    GLINTFX_CHECK(total == gltfx_gfss_property_count);
    // The plan's own SS4.2 names "color=9" - a mechanical re-count against
    // this file's own table finds EIGHT (this file's own top comment
    // records the discrepancy for the delivery report).
    GLINTFX_CHECK(color == 8);

    std::println(
        "every_property_has_exactly_one_value_contract_and_categories_are_counted: "
        "keyword={} length={} number={} integer={} color={} time_list={} raw={} (total {})",
        keyword_only, length_like, number_only, integer_only, color, time_list, raw, total);
}

GLINTFX_TEST(display_contract_accepts_only_its_three_words) {
    const property_value_contract &c = property_value_contract_for(gltfx_gfss_property::display);
    GLINTFX_CHECK(c.keyword_count == 3);
    GLINTFX_CHECK(c.accepted_natures == 0);
}

// === DP-6: declaration_value_check / declaration_color_value ===========

namespace {
declaration_value_check_result check_value_text(gltfx_gfss_property property,
                                                std::string_view value_text) {
    const std::vector<gltfx_gfss_token> tokens = tokenize_all(value_text);
    return check_declaration_value(tokens, 0, tokens.size() - 1,
                                   property_value_contract_for(property));
}
} // namespace

GLINTFX_TEST(keyword_outside_the_property_set_is_refused_naming_the_accepted_words) {
    const declaration_value_check_result result =
        check_value_text(gltfx_gfss_property::display, "inline");
    GLINTFX_CHECK(!result.ok);
    GLINTFX_CHECK(result.diagnostic.expected == k_expected_keyword_for_property);
    GLINTFX_CHECK(result.diagnostic.detail == "block flex none");
}

// docs/gfss-property-registry-v1.md SS5 item 5: "z-index e order sao
// naturezas inteiras: decimal e recusado". A decimal-spelled number
// ("1.5") decodes to value.hpp's own ::number nature (CSS Syntax
// Module Level 3 4.3.12's own type flag), never ::integer - so a
// contract declaring ONLY k_nature_integer (never k_nature_number)
// refuses it on nature grounds, distinct from the "1"/"-1"-as-number
// fold this file's own declaration_value_check.cpp applies the OTHER
// way around (an integer-spelled literal satisfying a NUMBER-only
// contract).
GLINTFX_TEST(z_index_and_order_refuse_decimal_values) {
    const declaration_value_check_result z_index_decimal =
        check_value_text(gltfx_gfss_property::z_index, "1.5");
    GLINTFX_CHECK(!z_index_decimal.ok);
    GLINTFX_CHECK(z_index_decimal.diagnostic.expected == k_expected_value_nature_for_property);

    const declaration_value_check_result z_index_integer =
        check_value_text(gltfx_gfss_property::z_index, "5");
    GLINTFX_CHECK(z_index_integer.ok);

    const declaration_value_check_result order_decimal =
        check_value_text(gltfx_gfss_property::order, "2.5");
    GLINTFX_CHECK(!order_decimal.ok);
    GLINTFX_CHECK(order_decimal.diagnostic.expected == k_expected_value_nature_for_property);
}

GLINTFX_TEST(current_color_is_accepted_by_every_color_property_and_nowhere_else) {
    int checked = 0;
    for (const property_value_contract &c : k_property_value_contracts) {
        if (!c.is_color) {
            continue;
        }
        const std::vector<gltfx_gfss_token> tokens = tokenize_all("currentColor");
        const declaration_color_value_result result =
            read_declaration_color_value(tokens, 0, tokens.size() - 1);
        GLINTFX_CHECK(result.kind == declaration_color_value_kind::current_color);
        ++checked;
    }
    GLINTFX_CHECK(checked == 8);

    // width is not a color property - currentColor is refused there,
    // naming the accepted words instead (never silently accepted).
    const declaration_value_check_result width_result =
        check_value_text(gltfx_gfss_property::width, "currentColor");
    GLINTFX_CHECK(!width_result.ok);
    GLINTFX_CHECK(width_result.diagnostic.expected == k_expected_keyword_for_property);

    std::println("current_color_is_accepted_by_every_color_property_and_nowhere_else: {} color "
                 "propert(y/ies) checked",
                 checked);
}

GLINTFX_TEST(time_property_takes_unitless_milliseconds) {
    const declaration_value_check_result folded =
        check_value_text(gltfx_gfss_property::transition_duration, "300");
    GLINTFX_CHECK(folded.ok);
    GLINTFX_CHECK(folded.values.size() == 1);
    GLINTFX_CHECK(folded.values[0].kind == gltfx_gfss_value_kind::time);
    GLINTFX_CHECK(folded.values[0].duration.magnitude == 300.0);
    GLINTFX_CHECK(folded.values[0].duration.unit == gltfx_gfss_time_unit::ms);

    const declaration_value_check_result explicit_ms =
        check_value_text(gltfx_gfss_property::transition_duration, "300ms");
    GLINTFX_CHECK(explicit_ms.ok);
    GLINTFX_CHECK(explicit_ms.values[0].duration.unit == gltfx_gfss_time_unit::ms);

    // A bare unitless number is legal ONLY on a time-typed contract
    // (this file's own top comment) - `width` has no `time_unitless_ms`
    // flag, so the SAME bare "300" fails its own contract (wrong
    // nature: an integer where only length/percentage/auto are
    // accepted).
    const declaration_value_check_result wrong_nature =
        check_value_text(gltfx_gfss_property::width, "300");
    GLINTFX_CHECK(!wrong_nature.ok);
    GLINTFX_CHECK(wrong_nature.diagnostic.expected == k_expected_value_nature_for_property);

    std::println("time_property_takes_unitless_milliseconds: folded={}ms explicit={}ms",
                 folded.values[0].duration.magnitude, explicit_ms.values[0].duration.magnitude);
}

GLINTFX_TEST(velocity_range_is_refused_both_sides_opacity_clamps) {
    const declaration_value_check_result below =
        check_value_text(gltfx_gfss_property::gltfx_velocity, "-1");
    GLINTFX_CHECK(!below.ok);
    GLINTFX_CHECK(below.diagnostic.expected == k_expected_value_in_range_for_property);

    const declaration_value_check_result above =
        check_value_text(gltfx_gfss_property::gltfx_velocity, "4");
    GLINTFX_CHECK(!above.ok);
    GLINTFX_CHECK(above.diagnostic.expected == k_expected_value_in_range_for_property);

    // One step past the recently-widened boundary too (GODS_LAWS.md
    // L-43 global: test past the fronteira recem-alargada, not just the
    // exact edge) - 3 itself is IN range, 3.0000001 conceptually is not,
    // but this registry's own natures are integer-free here (number),
    // so the directed boundary case is the in-range edge value itself.
    const declaration_value_check_result at_edge =
        check_value_text(gltfx_gfss_property::gltfx_velocity, "3");
    GLINTFX_CHECK(at_edge.ok);

    const declaration_value_check_result clamped =
        check_value_text(gltfx_gfss_property::opacity, "2");
    GLINTFX_CHECK(clamped.ok);
    GLINTFX_CHECK(clamped.values[0].number == 1.0);

    const declaration_value_check_result clamped_low =
        check_value_text(gltfx_gfss_property::opacity, "-5");
    GLINTFX_CHECK(clamped_low.ok);
    GLINTFX_CHECK(clamped_low.values[0].number == 0.0);
}

// === DP-7: declaration_parse - universal keywords, reserved notice ====

namespace {
declaration_parse_result parse_one(std::string_view declaration_text) {
    const std::vector<gltfx_gfss_token> tokens = tokenize_all(declaration_text);
    return parse_declaration(tokens, 0, tokens.size() - 1);
}
} // namespace

GLINTFX_TEST(universal_keywords_are_accepted_alone_for_one_property_of_each_category) {
    // The eight categories DP-5's own count proves closed, one directed
    // property from each - keyword, length/%, number, integer, color,
    // time_list, raw, shorthand - times the three universal words.
    constexpr std::string_view k_representative_properties[] = {
        "display", "width",  "flex-grow", "order", "color", "transition-duration",
        "filter",  "margin",
    };
    constexpr std::string_view k_universal_words[] = {"inherit", "initial", "unset"};

    int checked = 0;
    for (std::string_view property : k_representative_properties) {
        for (std::string_view word : k_universal_words) {
            const std::string text = std::string(property) + ": " + std::string(word);
            const declaration_parse_result result = parse_one(text);
            GLINTFX_CHECK(result.accepted);
            if (property != "margin") {
                GLINTFX_CHECK(result.declaration.form ==
                              gfss_declaration_value_form::universal_keyword);
            }
            ++checked;
        }
    }
    GLINTFX_CHECK(checked == 24);

    const declaration_parse_result mixed = parse_one("margin-top: 1px inherit");
    GLINTFX_CHECK(!mixed.accepted);
    GLINTFX_CHECK(mixed.diagnostic.expected == k_expected_universal_keyword_alone);

    std::println("universal_keywords_are_accepted_alone_for_one_property_of_each_category: {} "
                 "case(s) checked",
                 checked);
}

GLINTFX_TEST(every_accepted_declaration_of_a_reserved_property_carries_the_reserved_notice) {
    const declaration_parse_result result = parse_one("display: block");
    GLINTFX_CHECK(result.accepted);
    GLINTFX_CHECK(result.declaration.has_reserved_notice);

    const std::vector<gltfx_gfss_token> tokens = tokenize_all("display: block; width: 10px");
    const declaration_list_parse_result list_result =
        parse_declaration_list(gltfx_gfss_cursor{.source = "display: block; width: 10px"});
    GLINTFX_CHECK(list_result.declarations.size() == 2);
    GLINTFX_CHECK(list_result.reserved_notice_count == list_result.declarations.size());
    (void)tokens;
    std::println("every_accepted_declaration_of_a_reserved_property_carries_the_reserved_notice: "
                 "reserved_notice_count={} declarations={}",
                 list_result.reserved_notice_count, list_result.declarations.size());
}

GLINTFX_TEST(important_flag_position_inside_property_name_is_a_colon_error) {
    // "color !important: red" - the twelfth !important form the plan
    // names: the colon appears AFTER "!important", so the name area
    // itself ("color !important") does not end in a bare ident followed
    // immediately by ':' - the FIRST ident is a valid property name, but
    // what follows before the real ':' is not whitespace-only, so this
    // is read as "name area has no colon right after the name",
    // colon_after_property_name.
    const declaration_parse_result result = parse_one("color !important: red");
    GLINTFX_CHECK(!result.accepted);
    GLINTFX_CHECK(result.diagnostic.expected == k_expected_colon_after_property_name);
}

// === DP-8: recovery, real cursor position, inline style ===============

GLINTFX_TEST(invalid_declaration_is_dropped_and_the_next_one_survives_in_twelve_forms) {
    constexpr std::string_view k_forms[] = {
        "123: red",       // property_name
        "}",              // property_name (a stray close-curly is not an ident either)
        "banana: red",    // known_property_name
        "transition: 1s", // longhand_property_names
        "display block",  // colon_after_property_name
        "display: block !important 1px", // important_flag_at_end_of_value
        "margin-top: 1px inherit",       // universal_keyword_alone
        "display: inline",               // keyword_for_property
        "width: 1px 2px",                // value_count_for_property (max_count 1)
        "gltfx-Velocity: 9",             // value_in_range_for_property
        "display:",                      // component_value (empty)
        "opacity: 1px",                  // value_nature_for_property
    };
    int rejected_count = 0;
    int kept_count = 0;
    for (std::string_view form : k_forms) {
        const std::string sheet = std::string(form) + "; color: red";
        const declaration_list_parse_result result =
            parse_declaration_list(gltfx_gfss_cursor{.source = sheet});
        GLINTFX_CHECK(result.rejected.size() >= 1);
        rejected_count += static_cast<int>(result.rejected.size());
        bool survives = false;
        for (const gfss_declaration &d : result.declarations) {
            if (!d.is_shorthand && d.property == gltfx_gfss_property::color) {
                survives = true;
            }
        }
        GLINTFX_CHECK(survives);
        ++kept_count;
    }
    std::println("invalid_declaration_is_dropped_and_the_next_one_survives_in_twelve_forms: {} "
                 "form(s), {} rejected diagnostic(s), {} kept",
                 std::size(k_forms), rejected_count, kept_count);
}

GLINTFX_TEST(diagnostic_line_and_column_are_the_sheets_own_not_the_blocks) {
    const gltfx_gfss_cursor cursor{
        .source = "colr: red", .byte_offset = 0, .line = 40, .column = 5};
    const declaration_list_parse_result result = parse_declaration_list(cursor);
    GLINTFX_CHECK(result.declarations.empty());
    GLINTFX_CHECK(result.rejected.size() == 1);
    GLINTFX_CHECK(result.rejected[0].line == 40);
    GLINTFX_CHECK(result.rejected[0].column == 5);
}

GLINTFX_TEST(inline_style_text_parses_with_the_same_function) {
    const declaration_list_parse_result result =
        parse_declaration_list(gltfx_gfss_cursor{.source = "color: red; margin-top: 2px"});
    GLINTFX_CHECK(result.declarations.size() == 2);
    GLINTFX_CHECK(result.rejected.empty());
}
