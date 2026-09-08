// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/gl/gfx_option_validation.hpp"

#include <cstdint>
#include <new>
#include <string>

#include <glintfx/core/err_code.hpp>

#include "platform/gl/gfx_option_registry.hpp"

namespace glintfx::platform {

gltfx_rslt<void> validate_gfx_option_entry(const gltfx_gfx_option_entry &entry,
                                           bool already_open) noexcept {
    const gfx_option_row *row = find_gfx_option_row(entry.id);
    if (row == nullptr) {
        // No name to report for an id nobody registered - the decimal
        // numeric value is the only honest token available (docs/
        // api-conventions.md R7: a token, never a sentence).
        //
        // GODS_LAWS.md L-22: std::to_string() can throw std::bad_alloc
        // despite this function's own noexcept - the same "no
        // exception crosses a noexcept boundary" guard gfx_open_only_
        // fixation.cpp's own resolve_gfx_open_only_fixation() already
        // applies one file over. This atom has no out_of_memory shape
        // to hand back through gltfx_rslt<void> for an id that is
        // ALSO invalid, so it degrades to a fixed placeholder token
        // (never a guess at the real numeric id) - the same class of
        // fallback option_name_or_placeholder() (gl_context_facade.cpp)
        // already uses when a row cannot be found at all.
        std::string unknown_id;
        try {
            unknown_id = std::to_string(static_cast<std::uint16_t>(entry.id));
        } catch (const std::bad_alloc &) {
            unknown_id = "gfx_option";
        }
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value(unknown_id));
    }

    if (row->when == gltfx_gfx_option_when::read_only) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value(row->name));
    }

    if (row->when == gltfx_gfx_option_when::open_only && already_open) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value(row->name));
    }

    if (entry.value < row->min_value || entry.value > row->max_value) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value(row->name));
    }

    return gltfx_rslt<void>::ok();
}

} // namespace glintfx::platform
