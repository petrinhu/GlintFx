// SPDX-License-Identifier: AGPL-3.0-or-later
#include "core/atlas_request_check.hpp"

#include <cstdint>
#include <string_view>

#include <glintfx/core/err_code.hpp>

// core/atlas_request_check.cpp - R2D-TEXTURE. The refusals by name that need no store;
// core/atlas_request_check.hpp says the order.
namespace glintfx {

namespace {

gltfx_rslt<void> refused(std::string_view name) noexcept {
    return gltfx_rslt<void>::err(
        gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value(name));
}

// A width wider, or a height taller, than the atlas: no release can ever make it fit.
gltfx_rslt<void> check_fits_the_atlas(const atlas_identity &atlas, std::uint32_t width,
                                      std::uint32_t height) noexcept {
    if (width > atlas.width) {
        return refused("width");
    }
    if (height > atlas.height) {
        return refused("height");
    }
    return gltfx_rslt<void>::ok();
}

} // namespace

gltfx_rslt<void> check_atlas_size(std::uint32_t width, std::uint32_t height) noexcept {
    if (width == 0) {
        return refused("width");
    }
    if (height == 0) {
        return refused("height");
    }
    return gltfx_rslt<void>::ok();
}

gltfx_rslt<void> check_atlas_request(const atlas_identity &atlas, std::uint32_t width,
                                     std::uint32_t height) noexcept {
    if (!atlas.open) {
        return refused("allocator");
    }
    gltfx_rslt<void> size = check_atlas_size(width, height);
    if (size.has_error()) {
        return size;
    }
    return check_fits_the_atlas(atlas, width, height);
}

gltfx_rslt<void> check_atlas_release(const atlas_identity &atlas,
                                     const gltfx_atlas_slot &slot) noexcept {
    if (!atlas.open) {
        return refused("allocator");
    }
    if (slot.owner == 0) {
        return refused("stale_slot");
    }
    if (slot.owner != atlas.owner) {
        return refused("slot_other_allocator");
    }
    return gltfx_rslt<void>::ok();
}

} // namespace glintfx
