// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/texture_region_check.hpp"

#include "draw2d/piece_refusal.hpp"

// draw2d/texture_region_check.cpp - R2D-TEXTURE, fatia T4b-A: the checks of
// texture_region_check.hpp.
namespace glintfx::draw2d {

namespace {

// A switch over every value, with `return false;` after it: a value a newer glintfx adds is
// refused, and never taken for a default (docs/api-conventions.md R4).
[[nodiscard]] bool is_known_mirror(glintfx::gltfx_texture_mirror mirror) noexcept {
    switch (mirror) {
    case glintfx::gltfx_texture_mirror::none:
    case glintfx::gltfx_texture_mirror::flip_left_right:
    case glintfx::gltfx_texture_mirror::flip_top_bottom:
    case glintfx::gltfx_texture_mirror::flip_left_right_and_top_bottom:
        return true;
    }
    return false;
}

// Reasons 3 and 4, common to both calls, after the values are known to be finite.
[[nodiscard]] gltfx_draw_2d_refusal
refusal_of_region_shape(const glintfx::gltfx_texture_paint &paint, bool negative_target) noexcept {
    const glintfx::gltfx_rect_texel &source = paint.region.source;
    if (negative_target || source.size.x < 0.0 || source.size.y < 0.0) {
        return gltfx_draw_2d_refusal::negative_size;
    }
    if (!is_known_mirror(paint.region.mirror)) {
        return gltfx_draw_2d_refusal::unknown_option;
    }
    return gltfx_draw_2d_refusal::none;
}

} // namespace

gltfx_draw_2d_refusal refusal_of_texture_rect(const glintfx::gltfx_texture_paint &paint,
                                              const glintfx::gltfx_rect_world &rect) noexcept {
    const glintfx::gltfx_rect_texel &source = paint.region.source;
    const gltfx_draw_2d_refusal by_value =
        refusal_of_values(rect.corner.x, rect.corner.y, rect.size.x, rect.size.y, source.corner.x,
                          source.corner.y, source.size.x, source.size.y, paint.tint.red,
                          paint.tint.green, paint.tint.blue, paint.tint.alpha);
    if (by_value != gltfx_draw_2d_refusal::none) {
        return by_value;
    }
    return refusal_of_region_shape(paint, rect.size.x < 0.0 || rect.size.y < 0.0);
}

gltfx_draw_2d_refusal refusal_of_texture_quad(const glintfx::gltfx_texture_paint &paint,
                                              const glintfx::gltfx_quad_world &corners) noexcept {
    const glintfx::gltfx_rect_texel &source = paint.region.source;
    const gltfx_draw_2d_refusal by_value = refusal_of_values(
        corners.top_left.x, corners.top_left.y, corners.top_right.x, corners.top_right.y,
        corners.bottom_right.x, corners.bottom_right.y, corners.bottom_left.x,
        corners.bottom_left.y, source.corner.x, source.corner.y, source.size.x, source.size.y,
        paint.tint.red, paint.tint.green, paint.tint.blue, paint.tint.alpha);
    if (by_value != gltfx_draw_2d_refusal::none) {
        return by_value;
    }
    return refusal_of_region_shape(paint, false);
}

gltfx_draw_2d_refusal refusal_of_region_range(const glintfx::gltfx_rect_texel &source,
                                              texture_extent extent) noexcept {
    const double width = static_cast<double>(extent.width);
    const double height = static_cast<double>(extent.height);
    const bool inside = source.corner.x >= 0.0 && source.corner.y >= 0.0 &&
                        source.corner.x + source.size.x <= width &&
                        source.corner.y + source.size.y <= height;
    return inside ? gltfx_draw_2d_refusal::none : gltfx_draw_2d_refusal::region_out_of_range;
}

} // namespace glintfx::draw2d
