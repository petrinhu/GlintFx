// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/gl/preset_expansion.hpp"

namespace glintfx::platform {

namespace {
[[nodiscard]] bool names_option(std::span<const gltfx_gfx_option_entry> entries,
                                gltfx_gfx_option id) noexcept {
    for (const gltfx_gfx_option_entry &entry : entries) {
        if (entry.id == id) {
            return true;
        }
    }
    return false;
}

// One entry per option, the FIRST occurrence wins (the same rule as
// gl_context_desc_validation.cpp). A result never holds more entries than there
// are options in the registry, so the fixed capacity cannot overflow for any
// option the registry knows; the bounds check below is the last line of defense
// for an id the registry does not know (validation refuses those before, P3),
// never a path the correct inputs reach.
void append_if_new(preset_expansion &result, const gltfx_gfx_option_entry &entry) noexcept {
    for (std::size_t i = 0; i < result.count; ++i) {
        if (result.entries[i].id == entry.id) {
            return;
        }
    }
    // The full-capacity drop below is reached ONLY by an id outside the registry: the capacity is
    // one slot per registry option and an id already in the result returned above, so no known
    // id can overflow it, and validate_gl_context_desc() refuses an unknown id before any
    // expansion. Dropping it silently is therefore not a way to lose a valid entry.
    if (result.count < result.entries.size()) {
        result.entries[result.count] = entry;
        ++result.count;
    }
}
} // namespace

preset_expansion expand_preset(std::int64_t preset,
                               std::span<const gltfx_gfx_option_entry> explicit_entries) noexcept {
    preset_expansion result;
    for (const gfx_preset_row &row : gfx_preset_rows(preset)) {
        if (!names_option(explicit_entries, row.id)) {
            append_if_new(result, gltfx_gfx_option_entry{row.id, row.value});
        }
    }
    for (const gltfx_gfx_option_entry &entry : explicit_entries) {
        append_if_new(result, entry);
    }
    return result;
}

} // namespace glintfx::platform
