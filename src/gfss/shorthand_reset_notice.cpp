// SPDX-License-Identifier: AGPL-3.0-or-later
#include "shorthand_reset_notice.hpp"

#include "diagnostic_vocabulary.hpp"
#include "property_table.hpp"

// shorthand_reset_notice.cpp - GFSS-SHORTHAND, S-3b (GODS_LAWS.md L-17:
// this file answers exactly one question, shorthand_reset_notice.hpp's
// own header comment scope - never HOW the expansion itself happened,
// shorthand_expand.cpp's own job, nor WHERE the result lands in the
// block, declaration_list_parse.cpp's own fold_into_result()).

namespace glintfx::style::detail {

std::vector<gltfx_gfss_diagnostic>
shorthand_reset_notices(const std::vector<gfss_declaration> &declarations,
                        std::size_t expansion_begin, std::size_t longhand_count,
                        std::uint32_t shorthand_line, std::uint32_t shorthand_column) {
    std::vector<gltfx_gfss_diagnostic> notices;
    for (std::size_t offset = 0; offset < longhand_count; ++offset) {
        const gfss_declaration &produced = declarations[expansion_begin + offset];

        bool already_written = false;
        for (std::size_t earlier = 0; earlier < expansion_begin && !already_written; ++earlier) {
            if (!declarations[earlier].is_shorthand &&
                declarations[earlier].property == produced.property) {
                already_written = true;
            }
        }
        if (!already_written) {
            continue;
        }

        const property_entry *entry = find_property_entry(produced.property);
        // Never nullptr: `produced.property` only ever came from
        // k_shorthand_longhand_table (shorthand_longhand_table.hpp),
        // itself only ever populated with property.hpp ids that
        // property_table.hpp's own static_assert already proves has
        // one row per id.
        notices.push_back(gltfx_gfss_diagnostic{
            .line = shorthand_line,
            .column = shorthand_column,
            .expected = k_expected_longhand_not_overridden_by_later_shorthand,
            .detail = entry->sheet_name,
        });
    }
    return notices;
}

} // namespace glintfx::style::detail
