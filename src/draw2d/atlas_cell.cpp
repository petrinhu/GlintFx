// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/atlas_cell.hpp"

#include <cstdint>
#include <string_view>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

#include "draw2d/texture_request_check.hpp"

// draw2d/atlas_cell.cpp - R2D-TEXTURE, fatia T6-B1; draw2d/atlas_cell.hpp says what each function
// does.
namespace glintfx::draw2d {

namespace {

using glintfx::gltfx_err;
using glintfx::gltfx_err_code;
using glintfx::gltfx_rslt;
using glintfx::gltfx_texture_atlas_desc;

[[nodiscard]] gltfx_rslt<atlas_layout> refused(std::string_view field) noexcept {
    return gltfx_rslt<atlas_layout>::err(
        gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value(field));
}

// "margin_pixels": below 2^k, or so large that not even a one-texel image fits in a side.
[[nodiscard]] bool margin_fits(const gltfx_texture_atlas_desc &desc,
                               std::uint32_t alignment) noexcept {
    if (desc.margin_pixels < alignment) {
        return false;
    }
    const std::uint64_t smallest_cell = 2ULL * desc.margin_pixels + 1ULL;
    return smallest_cell <= desc.width && smallest_cell <= desc.height;
}

} // namespace

gltfx_rslt<atlas_layout> check_atlas_desc(const gltfx_texture_atlas_desc &desc) noexcept {
    if (desc.width == 0U) {
        return refused("width");
    }
    if (desc.height == 0U) {
        return refused("height");
    }
    if (!is_known_texture_filter(desc.filter)) {
        return refused("filter");
    }
    if (desc.mipmap_levels > reduced_level_limit(desc.width, desc.height)) {
        return refused("mipmap_levels");
    }
    // reduced_level_limit() is at most 31, so the shift stays inside 32 bits.
    const std::uint32_t alignment = 1U << desc.mipmap_levels;
    if (!margin_fits(desc, alignment)) {
        return refused("margin_pixels");
    }
    if (desc.width % alignment != 0U) {
        return refused("width");
    }
    if (desc.height % alignment != 0U) {
        return refused("height");
    }
    return gltfx_rslt<atlas_layout>::ok(
        atlas_layout{desc.margin_pixels, desc.mipmap_levels, alignment});
}

std::uint64_t atlas_cell_side(std::uint32_t image_side, const atlas_layout &layout) noexcept {
    const std::uint64_t with_margin = std::uint64_t{image_side} + 2ULL * layout.margin;
    const std::uint64_t alignment = layout.alignment;
    return (with_margin + alignment - 1ULL) / alignment * alignment;
}

gltfx_rect_texel atlas_image_region(const gltfx_atlas_rect &cell, atlas_image_size image,
                                    const atlas_layout &layout) noexcept {
    return gltfx_rect_texel{
        gltfx_vec2_texel{static_cast<double>(cell.x) + layout.margin,
                         static_cast<double>(cell.y) + layout.margin},
        gltfx_vec2_texel{static_cast<double>(image.width), static_cast<double>(image.height)}};
}

} // namespace glintfx::draw2d
