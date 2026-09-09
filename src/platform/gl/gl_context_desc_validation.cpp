// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/gl/gl_context_desc_validation.hpp"

#include <cstddef>
#include <span>

#include <glintfx/core/err_code.hpp>

#include "platform/gl/gfx_option_registry.hpp"
#include "platform/gl/gfx_option_validation.hpp"

namespace glintfx::platform {

namespace {

gltfx_rslt<void> refuse(std::string_view rejected_value) noexcept {
    return gltfx_rslt<void>::err(
        gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value(rejected_value));
}

} // namespace

gltfx_rslt<void> validate_gl_context_desc(const gltfx_gl_context_desc &desc) noexcept {
    // Reason 1: a non-null count with a null pointer is refused before
    // ever being dereferenced.
    if (desc.options == nullptr && desc.option_count > 0) {
        return refuse("options");
    }

    const std::span<const gltfx_gfx_option_entry> entries(desc.options, desc.option_count);

    for (std::size_t i = 0; i < entries.size(); ++i) {
        const gltfx_gfx_option_entry &entry = entries[i];

        // Reason 2: the entry's own shape (unknown id, out-of-range
        // value, read_only) - `already_open` is always false here,
        // this atom only ever runs at open() time.
        if (const gltfx_rslt<void> shape = validate_gfx_option_entry(entry, /*already_open=*/false);
            shape.has_error()) {
            return gltfx_rslt<void>::err(shape.err());
        }

        // Reason 3: the SAME id already appeared earlier in this same
        // list - the first duplicate found is what is named.
        for (std::size_t j = 0; j < i; ++j) {
            if (entries[j].id == entry.id) {
                const gfx_option_row *row = find_gfx_option_row(entry.id);
                // Unreachable in practice: reason 2 above already
                // proved `entry.id` is in the table, for BOTH this
                // entry and entries[j] (same id) - the null check is
                // this atom's own defensive floor, never trusted to be
                // hit (docs/api-conventions.md R4's own "never
                // undefined behavior" spirit, applied to an internal
                // atom instead of a public accessor).
                return refuse(row != nullptr ? row->name : std::string_view("gfx_option"));
            }
        }

        // Reason 4 (D-W6b-14): gpu_preference reserved but unhonored
        // until GL-GPU-PREFERENCE exists - only `no_preference` (0) is
        // accepted.
        if (entry.id == gltfx_gfx_option::gpu_preference && entry.value != 0) {
            return refuse("gpu_preference");
        }
    }

    return gltfx_rslt<void>::ok();
}

} // namespace glintfx::platform
