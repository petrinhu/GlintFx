// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/gl/gfx_preset_table.hpp"

namespace glintfx::platform {

std::span<const gfx_preset_row> gfx_preset_rows(std::int64_t preset) noexcept {
    const std::span<const gfx_preset_row> all(k_gfx_preset_table);
    std::size_t first = 0;
    while (first < all.size() && all[first].preset != preset) {
        ++first;
    }
    std::size_t last = first;
    while (last < all.size() && all[last].preset == preset) {
        ++last;
    }
    return all.subspan(first, last - first);
}

std::size_t gfx_preset_row_count(std::int64_t preset) noexcept {
    return gfx_preset_rows(preset).size();
}

std::optional<gltfx_gfx_option_entry> gfx_preset_row_at(std::int64_t preset,
                                                        std::size_t row_index) noexcept {
    const std::span<const gfx_preset_row> rows = gfx_preset_rows(preset);
    if (row_index >= rows.size()) {
        return std::nullopt;
    }
    return gltfx_gfx_option_entry{rows[row_index].id, rows[row_index].value};
}

} // namespace glintfx::platform
