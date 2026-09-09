// SPDX-License-Identifier: AGPL-3.0-or-later
#include "property_name_lookup.hpp"

#include "ascii_case.hpp"
#include "diagnostic_vocabulary.hpp"
#include "property_table.hpp"
#include "refused_property_names.hpp"
#include "shorthand_name_vocabulary.hpp"

// property_name_lookup.cpp - GFSS-DECL-PARSE, DP-4 (GODS_LAWS.md L-17:
// each function below answers exactly one question of property_name_
// lookup.hpp's own header comment scope - one lookup table tried in
// turn, never more than one per function).

namespace glintfx::style::detail {

namespace {

[[nodiscard]] bool try_known_property(std::string_view name, gltfx_gfss_property &out) noexcept {
    for (const property_entry &entry : k_property_table) {
        if (ascii_case_insensitive_equal(entry.sheet_name, name)) {
            out = entry.id;
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool try_shorthand(std::string_view name, std::string_view &out) noexcept {
    for (std::string_view candidate : k_shorthand_names) {
        if (ascii_case_insensitive_equal(candidate, name)) {
            out = candidate;
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool try_refused(std::string_view name, std::string_view &out_detail) noexcept {
    for (const refused_property_name_entry &entry : k_refused_property_names) {
        if (ascii_case_insensitive_equal(entry.name, name)) {
            out_detail = entry.detail;
            return true;
        }
    }
    return false;
}

} // namespace

property_name_lookup_result lookup_property_name(std::string_view name, std::uint32_t line,
                                                 std::uint32_t column) noexcept {
    gltfx_gfss_property property = gltfx_gfss_property::display;
    if (try_known_property(name, property)) {
        return property_name_lookup_result{.kind = property_name_lookup_kind::known_property,
                                           .property = property};
    }

    std::string_view shorthand_name;
    if (try_shorthand(name, shorthand_name)) {
        return property_name_lookup_result{.kind = property_name_lookup_kind::shorthand,
                                           .shorthand_name = shorthand_name};
    }

    std::string_view refused_detail;
    if (try_refused(name, refused_detail)) {
        const std::string_view expected = (ascii_case_insensitive_equal(name, "white-space"))
                                              ? k_expected_renamed_property_name
                                              : k_expected_longhand_property_names;
        return property_name_lookup_result{
            .kind = property_name_lookup_kind::refused,
            .diagnostic = gltfx_gfss_diagnostic{
                .line = line, .column = column, .expected = expected, .detail = refused_detail}};
    }

    return property_name_lookup_result{
        .kind = property_name_lookup_kind::unknown,
        .diagnostic = gltfx_gfss_diagnostic{.line = line,
                                            .column = column,
                                            .expected = k_expected_known_property_name,
                                            .detail = {}}};
}

} // namespace glintfx::style::detail
