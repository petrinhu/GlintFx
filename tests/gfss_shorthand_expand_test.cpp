// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <cstdint>
#include <print>
#include <string>
#include <string_view>
#include <vector>

#include <glintfx/core/color.hpp>
#include <glintfx/gfss/property.hpp>
#include <glintfx/gfss/token.hpp>
#include <glintfx/gfss/tokenizer.hpp>
#include <glintfx/gfss/value.hpp>

#include "gfss/declaration_ast.hpp"
#include "gfss/declaration_list_parse.hpp"
#include "gfss/diagnostic_vocabulary.hpp"
#include "gfss/property_table.hpp"
#include "gfss/shorthand_longhand_table.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// gfss_shorthand_expand_test.cpp - GFSS-SHORTHAND (TODO.md wave W6,
// GODS_LAWS.md L-20/L-40, docs/plano-w6-folha-de-estilo.md S-3): the
// TDD red/green witness for the eleven accepted shorthands' own
// expansion (shorthand_longhand_table.hpp, shorthand_component_split.
// hpp, shorthand_four_sides.hpp and its four siblings, shorthand_expand.
// hpp) - built on top of GFSS-DECL-PARSE's own declaration pipeline
// (declaration_list_parse.hpp). tests/gfss_declaration_parse_test.cpp's
// own no_is_shorthand_declaration_survives_parse_declaration_list is the
// sibling block-level invariant this file does not repeat.

namespace {

using namespace glintfx::style;
using namespace glintfx::style::detail;

[[nodiscard]] declaration_list_parse_result parse(std::string_view source) {
    return parse_declaration_list(gltfx_gfss_cursor{.source = source});
}

} // namespace

// === structural: the table itself =======================================

GLINTFX_TEST(eleven_shorthands_map_to_a_family_and_a_closed_longhand_list) {
    static_assert(k_shorthand_longhand_table.size() == 11,
                  "GODS_LAWS.md L-40: eleven shorthand rows, shorthand_name_vocabulary.hpp");

    struct expectation {
        std::string_view name;
        shorthand_family family = shorthand_family::four_sides;
        std::uint8_t longhand_count = 0;
    };
    static constexpr std::array<expectation, 11> k_expected{{
        {"margin", shorthand_family::four_sides, 4},
        {"padding", shorthand_family::four_sides, 4},
        {"border-width", shorthand_family::four_sides, 4},
        {"border-style", shorthand_family::four_sides, 4},
        {"border-color", shorthand_family::four_sides, 4},
        {"border-radius", shorthand_family::four_corners, 4},
        {"gap", shorthand_family::axis_pair, 2},
        {"overflow", shorthand_family::axis_pair, 2},
        {"border", shorthand_family::border_parts, 12},
        {"outline", shorthand_family::border_parts, 3},
        {"flex-flow", shorthand_family::flex_flow, 2},
    }};

    int checked = 0;
    for (const expectation &e : k_expected) {
        const shorthand_longhand_entry *entry = find_shorthand_longhand_entry(e.name);
        GLINTFX_CHECK(entry != nullptr);
        GLINTFX_CHECK(entry->family == e.family);
        GLINTFX_CHECK(entry->longhand_count == e.longhand_count);
        ++checked;
    }
    std::println("eleven_shorthands_map_to_a_family_and_a_closed_longhand_list: {} row(s) checked",
                 checked);
    GLINTFX_CHECK(checked == 11);
}

// === S-3's own closed matrix: 11 shorthands x {no !important, with} x
// {alone, preceded by an explicit longhand, followed by one} = 66 =======

GLINTFX_TEST(shorthand_expansion_matrix_eleven_by_important_by_position) {
    struct row {
        std::string_view name;
        std::string_view value_text;   // valid for every longhand of the family, 1-value form
        std::string_view marker_value; // valid for entry.longhands[0] alone, written directly
    };
    static constexpr std::array<row, 11> k_rows{{
        {"margin", "1px", "9px"},
        {"padding", "1px", "9px"},
        {"border-width", "1px", "9px"},
        {"border-style", "solid", "none"},
        {"border-color", "red", "blue"},
        {"border-radius", "1px", "9px"},
        {"gap", "1px", "9px"},
        {"overflow", "hidden", "visible"},
        {"border", "solid", "9px"},
        {"outline", "solid", "9px"},
        {"flex-flow", "row", "column"},
    }};

    int cells_checked = 0;
    for (const row &r : k_rows) {
        const shorthand_longhand_entry *entry = find_shorthand_longhand_entry(r.name);
        GLINTFX_CHECK(entry != nullptr);
        const property_entry *marker_property = find_property_entry(entry->longhands[0]);
        GLINTFX_CHECK(marker_property != nullptr);

        for (const bool important : {false, true}) {
            const std::string_view suffix = important ? " !important" : "";

            // sozinho
            {
                const std::string source = std::string(r.name) + ": " + std::string(r.value_text) +
                                           std::string(suffix) + ";";
                const declaration_list_parse_result result = parse(source);
                GLINTFX_CHECK(result.declarations.size() == entry->longhand_count);
                for (std::uint8_t i = 0; i < entry->longhand_count; ++i) {
                    GLINTFX_CHECK(result.declarations[i].property == entry->longhands[i]);
                    GLINTFX_CHECK(result.declarations[i].is_shorthand == false);
                    GLINTFX_CHECK(result.declarations[i].important == important);
                }
                ++cells_checked;
            }

            // precedido pelo longhand explicito - a expansao entra DEPOIS
            // do marcador, no indice do atalho (D-W6-3)
            {
                const std::string source = std::string(marker_property->sheet_name) + ": " +
                                           std::string(r.marker_value) + "; " +
                                           std::string(r.name) + ": " + std::string(r.value_text) +
                                           std::string(suffix) + ";";
                const declaration_list_parse_result result = parse(source);
                GLINTFX_CHECK(result.declarations.size() ==
                              static_cast<std::size_t>(entry->longhand_count) + 1);
                GLINTFX_CHECK(result.declarations[0].property == entry->longhands[0]);
                GLINTFX_CHECK(result.declarations[0].important == false);
                for (std::uint8_t i = 0; i < entry->longhand_count; ++i) {
                    GLINTFX_CHECK(result.declarations[i + 1].property == entry->longhands[i]);
                    GLINTFX_CHECK(result.declarations[i + 1].important == important);
                }
                ++cells_checked;
            }

            // seguido pelo longhand explicito - a expansao entra ANTES do
            // marcador, no indice do atalho (D-W6-3)
            {
                const std::string source = std::string(r.name) + ": " + std::string(r.value_text) +
                                           std::string(suffix) + "; " +
                                           std::string(marker_property->sheet_name) + ": " +
                                           std::string(r.marker_value) + ";";
                const declaration_list_parse_result result = parse(source);
                GLINTFX_CHECK(result.declarations.size() ==
                              static_cast<std::size_t>(entry->longhand_count) + 1);
                for (std::uint8_t i = 0; i < entry->longhand_count; ++i) {
                    GLINTFX_CHECK(result.declarations[i].property == entry->longhands[i]);
                    GLINTFX_CHECK(result.declarations[i].important == important);
                }
                GLINTFX_CHECK(result.declarations[entry->longhand_count].property ==
                              entry->longhands[0]);
                GLINTFX_CHECK(result.declarations[entry->longhand_count].important == false);
                ++cells_checked;
            }
        }
    }

    std::println("shorthand_expansion_matrix_eleven_by_important_by_position: {} cell(s) checked "
                 "(expected 66)",
                 cells_checked);
    GLINTFX_CHECK(cells_checked == 66);
}

// === shorthand_four_sides: 1/2/3/4-value distribution ===================

GLINTFX_TEST(four_sides_distribution_from_one_to_four_values) {
    struct case_row {
        std::string_view value_text;
        std::array<double, 4> expected_trbl{}; // top, right, bottom, left
    };
    static constexpr std::array<case_row, 4> k_cases{{
        {"1px", {1.0, 1.0, 1.0, 1.0}},
        {"1px 2px", {1.0, 2.0, 1.0, 2.0}},
        {"1px 2px 3px", {1.0, 2.0, 3.0, 2.0}},
        {"1px 2px 3px 4px", {1.0, 2.0, 3.0, 4.0}},
    }};

    int checked = 0;
    for (const case_row &c : k_cases) {
        const declaration_list_parse_result result =
            parse(std::string("margin: ") + std::string(c.value_text) + ";");
        GLINTFX_CHECK(result.declarations.size() == 4);
        for (std::size_t i = 0; i < 4; ++i) {
            GLINTFX_CHECK(result.declarations[i].form == gfss_declaration_value_form::values);
            GLINTFX_CHECK(result.declarations[i].values.size() == 1);
            GLINTFX_CHECK(result.declarations[i].values[0].kind == gltfx_gfss_value_kind::length);
            GLINTFX_CHECK(result.declarations[i].values[0].length.magnitude == c.expected_trbl[i]);
        }
        ++checked;
    }
    std::println("four_sides_distribution_from_one_to_four_values: {} case(s) checked", checked);
    GLINTFX_CHECK(checked == 4);
}

GLINTFX_TEST(four_sides_five_values_is_refused) {
    const declaration_list_parse_result result = parse("margin: 1px 2px 3px 4px 5px;");
    GLINTFX_CHECK(result.declarations.empty());
    GLINTFX_CHECK(result.rejected.size() == 1);
    GLINTFX_CHECK(result.rejected[0].expected == k_expected_one_to_four_side_values);
    GLINTFX_CHECK(!result.rejected[0].detail.empty());
}

// === shorthand_four_corners: TL/TR/BR/BL distribution, and D-W6-9's own
// slash refusal, in both written forms =====================================

GLINTFX_TEST(border_radius_corner_distribution_from_one_to_four_values) {
    struct case_row {
        std::string_view value_text;
        std::array<double, 4> expected_tl_tr_br_bl{};
    };
    static constexpr std::array<case_row, 4> k_cases{{
        {"1px", {1.0, 1.0, 1.0, 1.0}},
        {"1px 2px", {1.0, 2.0, 1.0, 2.0}},     // (TL+BR)=1px, (TR+BL)=2px
        {"1px 2px 3px", {1.0, 2.0, 3.0, 2.0}}, // TL=1px, (TR+BL)=2px, BR=3px
        {"1px 2px 3px 4px", {1.0, 2.0, 3.0, 4.0}},
    }};

    int checked = 0;
    for (const case_row &c : k_cases) {
        const declaration_list_parse_result result =
            parse(std::string("border-radius: ") + std::string(c.value_text) + ";");
        GLINTFX_CHECK(result.declarations.size() == 4);
        for (std::size_t i = 0; i < 4; ++i) {
            GLINTFX_CHECK(result.declarations[i].values[0].length.magnitude ==
                          c.expected_tl_tr_br_bl[i]);
        }
        ++checked;
    }
    std::println("border_radius_corner_distribution_from_one_to_four_values: {} case(s) checked",
                 checked);
    GLINTFX_CHECK(checked == 4);
}

GLINTFX_TEST(border_radius_slash_is_refused_in_both_written_forms) {
    const declaration_list_parse_result spaced = parse("border-radius: 10px / 5px;");
    GLINTFX_CHECK(spaced.declarations.empty());
    GLINTFX_CHECK(spaced.rejected.size() == 1);
    GLINTFX_CHECK(spaced.rejected[0].expected == k_expected_corner_radius_without_slash);

    const declaration_list_parse_result tight = parse("border-radius: 10px/5px;");
    GLINTFX_CHECK(tight.declarations.empty());
    GLINTFX_CHECK(tight.rejected.size() == 1);
    GLINTFX_CHECK(tight.rejected[0].expected == k_expected_corner_radius_without_slash);
}

// === shorthand_axis_pair: gap and overflow ==============================

GLINTFX_TEST(axis_pair_distribution_from_one_to_two_values) {
    const declaration_list_parse_result one = parse("gap: 1px;");
    GLINTFX_CHECK(one.declarations.size() == 2);
    GLINTFX_CHECK(one.declarations[0].values[0].length.magnitude == 1.0);
    GLINTFX_CHECK(one.declarations[1].values[0].length.magnitude == 1.0);

    const declaration_list_parse_result two = parse("gap: 1px 2px;");
    GLINTFX_CHECK(two.declarations.size() == 2);
    GLINTFX_CHECK(two.declarations[0].values[0].length.magnitude == 1.0);
    GLINTFX_CHECK(two.declarations[1].values[0].length.magnitude == 2.0);

    const declaration_list_parse_result overflow_two = parse("overflow: hidden visible;");
    GLINTFX_CHECK(overflow_two.declarations.size() == 2);
    GLINTFX_CHECK(overflow_two.declarations[0].property == gltfx_gfss_property::overflow_x);
    GLINTFX_CHECK(overflow_two.declarations[1].property == gltfx_gfss_property::overflow_y);
}

GLINTFX_TEST(axis_pair_three_values_is_refused) {
    const declaration_list_parse_result result = parse("gap: 1px 2px 3px;");
    GLINTFX_CHECK(result.declarations.empty());
    GLINTFX_CHECK(result.rejected.size() == 1);
    GLINTFX_CHECK(result.rejected[0].expected == k_expected_one_or_two_axis_values);
}

// === shorthand_border_parts: free order, each type at most once, and
// the omitted-part-resets-to-registry-initial invariant (D-W6-4) ========

GLINTFX_TEST(border_and_outline_classify_components_in_any_order) {
    const declaration_list_parse_result border = parse("border: red solid 2px;");
    GLINTFX_CHECK(border.declarations.size() == 12);
    for (std::size_t i = 0; i < 4; ++i) {
        GLINTFX_CHECK(border.declarations[i].values[0].length.magnitude == 2.0); // width
    }
    for (std::size_t i = 4; i < 8; ++i) {
        GLINTFX_CHECK(border.declarations[i].values[0].keyword_text == "solid"); // style
    }
    for (std::size_t i = 8; i < 12; ++i) {
        GLINTFX_CHECK(border.declarations[i].form == gfss_declaration_value_form::color);
        GLINTFX_CHECK(border.declarations[i].color.red == 1.0F); // red
        GLINTFX_CHECK(border.declarations[i].color.green == 0.0F);
        GLINTFX_CHECK(border.declarations[i].color.blue == 0.0F);
    }

    const declaration_list_parse_result outline = parse("outline: solid 2px red;");
    GLINTFX_CHECK(outline.declarations.size() == 3);
    GLINTFX_CHECK(outline.declarations[0].values[0].length.magnitude == 2.0);
    GLINTFX_CHECK(outline.declarations[1].values[0].keyword_text == "solid");
    GLINTFX_CHECK(outline.declarations[2].color.red == 1.0F);
}

GLINTFX_TEST(border_part_omitted_resets_to_the_registry_initial) {
    // Only style given - width/color fall back to the registry's own
    // initial (D-W6-4): "medium" (width), resolved black (color).
    const declaration_list_parse_result result = parse("border: solid;");
    GLINTFX_CHECK(result.declarations.size() == 12);
    for (std::size_t i = 0; i < 4; ++i) {
        GLINTFX_CHECK(result.declarations[i].values[0].kind == gltfx_gfss_value_kind::keyword);
        GLINTFX_CHECK(result.declarations[i].values[0].keyword_text == "medium");
    }
    for (std::size_t i = 8; i < 12; ++i) {
        GLINTFX_CHECK(result.declarations[i].form == gfss_declaration_value_form::color);
        GLINTFX_CHECK(result.declarations[i].color.red == 0.0F);
        GLINTFX_CHECK(result.declarations[i].color.green == 0.0F);
        GLINTFX_CHECK(result.declarations[i].color.blue == 0.0F);
        GLINTFX_CHECK(result.declarations[i].color.alpha == 1.0F);
    }
}

GLINTFX_TEST(border_duplicate_part_type_is_refused) {
    const declaration_list_parse_result duplicate_style = parse("outline: solid none;");
    GLINTFX_CHECK(duplicate_style.declarations.empty());
    GLINTFX_CHECK(duplicate_style.rejected.size() == 1);
    GLINTFX_CHECK(duplicate_style.rejected[0].expected == k_expected_each_border_part_at_most_once);

    const declaration_list_parse_result duplicate_color = parse("outline: red blue;");
    GLINTFX_CHECK(duplicate_color.declarations.empty());
    GLINTFX_CHECK(duplicate_color.rejected.size() == 1);
    GLINTFX_CHECK(duplicate_color.rejected[0].expected == k_expected_each_border_part_at_most_once);
}

GLINTFX_TEST(border_part_unrecognized_word_is_refused) {
    const declaration_list_parse_result result = parse("outline: banana;");
    GLINTFX_CHECK(result.declarations.empty());
    GLINTFX_CHECK(result.rejected.size() == 1);
    GLINTFX_CHECK(result.rejected[0].expected == k_expected_border_part_width_style_or_color);
}

GLINTFX_TEST(border_part_four_components_is_refused) {
    const declaration_list_parse_result result = parse("outline: solid 2px red 3px;");
    GLINTFX_CHECK(result.declarations.empty());
    GLINTFX_CHECK(result.rejected.size() == 1);
    GLINTFX_CHECK(result.rejected[0].expected == k_expected_border_part_width_style_or_color);
}

// === shorthand_flex_flow: free order, one-word defaults the other =======

GLINTFX_TEST(flex_flow_accepts_either_order_and_defaults_the_missing_word) {
    const declaration_list_parse_result direction_first = parse("flex-flow: column wrap;");
    GLINTFX_CHECK(direction_first.declarations.size() == 2);
    GLINTFX_CHECK(direction_first.declarations[0].values[0].keyword_text == "column");
    GLINTFX_CHECK(direction_first.declarations[1].values[0].keyword_text == "wrap");

    const declaration_list_parse_result wrap_first = parse("flex-flow: wrap column;");
    GLINTFX_CHECK(wrap_first.declarations.size() == 2);
    GLINTFX_CHECK(wrap_first.declarations[0].values[0].keyword_text == "column");
    GLINTFX_CHECK(wrap_first.declarations[1].values[0].keyword_text == "wrap");

    // direction alone - wrap resets to the registry initial ("nowrap")
    const declaration_list_parse_result alone = parse("flex-flow: row;");
    GLINTFX_CHECK(alone.declarations.size() == 2);
    GLINTFX_CHECK(alone.declarations[0].values[0].keyword_text == "row");
    GLINTFX_CHECK(alone.declarations[1].values[0].keyword_text == "nowrap");
}

GLINTFX_TEST(flex_flow_unrecognized_word_is_refused) {
    const declaration_list_parse_result result = parse("flex-flow: banana;");
    GLINTFX_CHECK(result.declarations.empty());
    GLINTFX_CHECK(result.rejected.size() == 1);
    GLINTFX_CHECK(result.rejected[0].expected == k_expected_flex_direction_or_flex_wrap_word);
}

// === the three universal keywords, alone or mixed (D-DP-7's own
// precedent, applied here to the shorthand's own value) =================

GLINTFX_TEST(shorthand_universal_keyword_alone_expands_to_every_longhand) {
    const declaration_list_parse_result result = parse("margin: inherit;");
    GLINTFX_CHECK(result.declarations.size() == 4);
    for (const gfss_declaration &declaration : result.declarations) {
        GLINTFX_CHECK(declaration.form == gfss_declaration_value_form::universal_keyword);
        GLINTFX_CHECK(declaration.universal == gfss_universal_keyword::inherit_keyword);
    }
}

GLINTFX_TEST(shorthand_universal_keyword_mixed_with_other_tokens_is_refused) {
    const declaration_list_parse_result result = parse("margin: inherit 1px;");
    GLINTFX_CHECK(result.declarations.empty());
    GLINTFX_CHECK(result.rejected.size() == 1);
    GLINTFX_CHECK(result.rejected[0].expected == k_expected_universal_keyword_alone);
}

// === a component that hides whitespace of its own (a function call)
// stays ONE component, never split by the comma/space inside it =========

GLINTFX_TEST(a_function_call_component_is_never_split_by_its_own_internal_whitespace) {
    const declaration_list_parse_result result = parse("border-color: rgb(255, 0, 0);");
    GLINTFX_CHECK(result.declarations.size() == 4);
    for (const gfss_declaration &declaration : result.declarations) {
        GLINTFX_CHECK(declaration.form == gfss_declaration_value_form::color);
        GLINTFX_CHECK(declaration.color.red == 1.0F);
        GLINTFX_CHECK(declaration.color.green == 0.0F);
        GLINTFX_CHECK(declaration.color.blue == 0.0F);
    }
}
