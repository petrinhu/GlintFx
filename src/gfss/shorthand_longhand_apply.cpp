// SPDX-License-Identifier: AGPL-3.0-or-later
#include "shorthand_longhand_apply.hpp"

#include <array>
#include <string>

#include "declaration_color_value.hpp"
#include "declaration_value_check.hpp"
#include "property_status.hpp"
#include "property_table.hpp"
#include "property_value_contract.hpp"

// shorthand_longhand_apply.cpp - GFSS-SHORTHAND (GODS_LAWS.md L-17: each
// function below answers exactly one question of shorthand_longhand_
// apply.hpp's own header comment scope).
//
// NEITHER OF THE ELEVEN SHORTHANDS' OWN LONGHANDS IS EVER `raw_
// composite` (measured against property_value_contract.hpp's own table,
// docs/plano-w6-folha-de-estilo.md F8): margin/padding/border-*/gap/
// overflow/outline/flex-flow's longhands are every one either `is_color`
// or a natures/keywords contract - apply_longhand_component() below
// therefore only ever needs the SAME two branches declaration_parse.cpp's
// own apply_contract_value() carries for those two shapes, never its
// third (`raw_composite`) branch. This is a real, closed property of the
// eleven-name table (shorthand_name_vocabulary.hpp) this fatia's own
// test proves by construction (every longhand this table names is
// checked against its own contract, and `raw_composite` never appears).

namespace glintfx::style::detail {

longhand_component_outcome apply_longhand_component(const std::vector<gltfx_gfss_token> &tokens,
                                                    std::size_t begin, std::size_t end,
                                                    gltfx_gfss_property property, bool important) {
    const property_value_contract &contract = property_value_contract_for(property);

    gfss_declaration declaration;
    declaration.is_shorthand = false;
    declaration.property = property;
    declaration.important = important;

    if (contract.is_color) {
        const declaration_color_value_result color =
            read_declaration_color_value(tokens, begin, end);
        if (color.kind == declaration_color_value_kind::failed) {
            return longhand_component_outcome{.ok = false, .diagnostic = color.diagnostic};
        }
        if (color.kind == declaration_color_value_kind::current_color) {
            declaration.form = gfss_declaration_value_form::current_color;
        } else {
            declaration.form = gfss_declaration_value_form::color;
            declaration.color = color.value;
        }
    } else {
        const declaration_value_check_result checked =
            check_declaration_value(tokens, begin, end, contract);
        if (!checked.ok) {
            return longhand_component_outcome{.ok = false, .diagnostic = checked.diagnostic};
        }
        declaration.form = gfss_declaration_value_form::values;
        declaration.values = checked.values;
    }

    declaration.has_reserved_notice = gfss_property_status(property) == property_status::reserved;
    return longhand_component_outcome{.ok = true, .declaration = std::move(declaration)};
}

gfss_declaration longhand_declaration_with_registry_initial(gltfx_gfss_property property,
                                                            bool important) {
    const property_entry *entry = find_property_entry(property);
    const property_value_contract &contract = property_value_contract_for(property);

    gfss_declaration declaration;
    declaration.is_shorthand = false;
    declaration.property = property;
    declaration.important = important;

    if (contract.is_color) {
        // Every is_color row in property_table.hpp's own 104 rows stores
        // its initial as a KEYWORD (property_keyword_initial(), e.g.
        // "black"/"transparent") - resolved here through the REAL color
        // grammar via a synthetic single-ident token, so this never
        // re-implements named_colors.cpp's own lookup table by hand.
        const gltfx_gfss_token synthetic{.kind = gltfx_gfss_token_kind::ident,
                                         .lexeme = entry->initial.keyword_text,
                                         .line = 0,
                                         .column = 0,
                                         .diagnostic = {}};
        const std::vector<gltfx_gfss_token> one{synthetic};
        const declaration_color_value_result color = read_declaration_color_value(one, 0, 1);
        if (color.kind == declaration_color_value_kind::current_color) {
            declaration.form = gfss_declaration_value_form::current_color;
        } else {
            declaration.form = gfss_declaration_value_form::color;
            declaration.color = color.value;
        }
    } else {
        declaration.form = gfss_declaration_value_form::values;
        declaration.values.push_back(entry->initial);
    }

    declaration.has_reserved_notice = gfss_property_status(property) == property_status::reserved;
    return declaration;
}

std::string_view longhand_names_detail(const shorthand_longhand_entry &entry) {
    // Built ONCE, in program-lifetime storage - the SAME cppcheck-caught
    // dangling-view lesson declaration_value_check.cpp's own accepted_
    // keyword_detail_table() already documents (that file's own header
    // comment tells the full story): a std::string returned by value
    // from a per-call helper would bind `detail` to a temporary that
    // dies at the end of the calling expression.
    static const std::array<std::string, k_shorthand_name_count> table = [] {
        std::array<std::string, k_shorthand_name_count> built{};
        for (std::size_t i = 0; i < k_shorthand_longhand_table.size(); ++i) {
            const shorthand_longhand_entry &row = k_shorthand_longhand_table[i];
            std::string detail;
            for (std::uint8_t k = 0; k < row.longhand_count; ++k) {
                if (k > 0) {
                    detail += ' ';
                }
                const property_entry *property_row = find_property_entry(row.longhands[k]);
                detail += property_row->sheet_name;
            }
            built[i] = std::move(detail);
        }
        return built;
    }();

    const std::size_t index = static_cast<std::size_t>(&entry - k_shorthand_longhand_table.data());
    return table[index];
}

} // namespace glintfx::style::detail
