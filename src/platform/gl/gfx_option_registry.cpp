// SPDX-License-Identifier: AGPL-3.0-or-later
#include <glintfx/platform/gl/gfx_option.hpp>

#include <glintfx/core/err_code.hpp>

#include "platform/gl/gfx_option_registry.hpp"
#include "platform/gl/gfx_preset_table.hpp"

// platform/gl/gfx_option_registry.cpp - defines the GLINTFX_API discovery
// functions gfx_option.hpp declares, over gfx_option_
// registry.hpp's own k_gfx_option_table - the same split property.cpp/
// property_table.hpp already uses for this project's other append-only
// registry (see that pair's own header comments for the shared
// reasoning, not repeated here).

namespace glintfx {

std::size_t gltfx_gfx_option_count() noexcept { return platform::k_gfx_option_table.size(); }

gltfx_gfx_option_info gltfx_gfx_option_at(std::size_t index_in_table) noexcept {
    // docs/api-conventions.md R4: an out-of-range index degrades to a
    // default-constructed info, never undefined behavior.
    if (index_in_table >= platform::k_gfx_option_table.size()) {
        return gltfx_gfx_option_info{};
    }
    const platform::gfx_option_row &row = platform::k_gfx_option_table[index_in_table];
    return gltfx_gfx_option_info{
        .id = row.id,
        .name = row.name,
        .kind = row.kind,
        .when = row.when,
        .min_value = row.min_value,
        .max_value = row.max_value,
        .default_value = row.default_value,
    };
}

gltfx_gfx_option_info gltfx_gfx_option_describe(gltfx_gfx_option id) noexcept {
    const platform::gfx_option_row *row = platform::find_gfx_option_row(id);
    if (row == nullptr) {
        // Same R4 degrade as gltfx_gfx_option_at() above - an `id` this
        // build's table has never heard of is not a crash.
        return gltfx_gfx_option_info{};
    }
    return gltfx_gfx_option_info{
        .id = row->id,
        .name = row->name,
        .kind = row->kind,
        .when = row->when,
        .min_value = row->min_value,
        .max_value = row->max_value,
        .default_value = row->default_value,
    };
}

gltfx_rslt<gltfx_gfx_option> gltfx_gfx_option_by_name(std::string_view name) noexcept {
    for (const platform::gfx_option_row &row : platform::k_gfx_option_table) {
        if (row.name == name) {
            return gltfx_rslt<gltfx_gfx_option>::ok(row.id);
        }
    }
    return gltfx_rslt<gltfx_gfx_option>::err(
        gltfx_err(gltfx_err_code::not_found).with_rejected_value(name));
}

std::size_t gltfx_gfx_preset_row_count(std::int64_t preset) noexcept {
    return platform::gfx_preset_row_count(preset);
}

gltfx_gfx_option_entry gltfx_gfx_preset_row_at(std::int64_t preset,
                                               std::size_t row_index) noexcept {
    // docs/api-conventions.md R4 and PLANO-errata.md E1: out of range degrades to the ONE entry
    // that hands nothing over by mistake - an id that is read_only, so applying it is refused with
    // invalid_argument and changes no row and no label. The table atom answers the ABSENCE; the
    // policy of what the consumer gets instead lives here, where the public names exist.
    const std::optional<gltfx_gfx_option_entry> row =
        platform::gfx_preset_row_at(preset, row_index);
    if (!row.has_value()) {
        return gltfx_gfx_option_entry{.id = gltfx_gfx_option::suggested_preset,
                                      .value = k_gltfx_preset_manual};
    }
    return *row;
}

} // namespace glintfx
