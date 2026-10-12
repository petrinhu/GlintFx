// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/piece_refusal.hpp"

namespace glintfx::draw2d {

gltfx_draw_2d_refusal refusal_of_quad(const quad_corners_world &corners,
                                      glintfx::gltfx_rgba color) noexcept {
    return refusal_of_values(corners[0].x, corners[0].y, corners[1].x, corners[1].y, corners[2].x,
                             corners[2].y, corners[3].x, corners[3].y, color.red, color.green,
                             color.blue, color.alpha);
}

gltfx_draw_2d_refusal refusal_of_rect(const glintfx::gltfx_rect_world &rect,
                                      glintfx::gltfx_rgba color) noexcept {
    const gltfx_draw_2d_refusal by_value =
        refusal_of_values(rect.corner.x, rect.corner.y, rect.size.x, rect.size.y, color.red,
                          color.green, color.blue, color.alpha);
    if (by_value != gltfx_draw_2d_refusal::none) {
        return by_value;
    }
    return (rect.size.x < 0.0 || rect.size.y < 0.0) ? gltfx_draw_2d_refusal::negative_size
                                                    : gltfx_draw_2d_refusal::none;
}

} // namespace glintfx::draw2d
