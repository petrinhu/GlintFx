// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/ellipse_check.hpp"

#include "draw2d/piece_refusal.hpp"
#include "draw2d/stroke_unit.hpp"

// draw2d/ellipse_check.cpp - R2D-SHAPES, fatia S5-B: the checks of ellipse_check.hpp.
namespace glintfx::draw2d {

gltfx_draw_2d_refusal refusal_of_fill_ellipse(const glintfx::gltfx_ellipse_world &ellipse,
                                              glintfx::gltfx_rgba color) noexcept {
    const gltfx_draw_2d_refusal by_value = refusal_of_values(
        ellipse.center.x, ellipse.center.y, ellipse.radius_x, ellipse.radius_y,
        ellipse.rotation.radians, color.red, color.green, color.blue, color.alpha);
    if (by_value != gltfx_draw_2d_refusal::none) {
        return by_value;
    }
    if (ellipse.radius_x < 0.0 || ellipse.radius_y < 0.0) {
        return gltfx_draw_2d_refusal::negative_size;
    }
    return gltfx_draw_2d_refusal::none;
}

gltfx_draw_2d_refusal refusal_of_stroke_ellipse(const glintfx::gltfx_ellipse_world &ellipse,
                                                const glintfx::gltfx_stroke &stroke,
                                                glintfx::gltfx_rgba color) noexcept {
    const gltfx_draw_2d_refusal by_value =
        refusal_of_values(ellipse.center.x, ellipse.center.y, ellipse.radius_x, ellipse.radius_y,
                          ellipse.rotation.radians, stroke.thickness, color.red, color.green,
                          color.blue, color.alpha);
    if (by_value != gltfx_draw_2d_refusal::none) {
        return by_value;
    }
    if (ellipse.radius_x < 0.0 || ellipse.radius_y < 0.0 || stroke.thickness < 0.0) {
        return gltfx_draw_2d_refusal::negative_size;
    }
    if (!is_known_stroke_unit(stroke.unit)) {
        return gltfx_draw_2d_refusal::unknown_option;
    }
    return gltfx_draw_2d_refusal::none;
}

} // namespace glintfx::draw2d
