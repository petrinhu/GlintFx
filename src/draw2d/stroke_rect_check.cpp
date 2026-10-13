// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/stroke_rect_check.hpp"

#include "draw2d/piece_refusal.hpp"

// draw2d/stroke_rect_check.cpp - R2D-SHAPES, fatia S4: the check of stroke_rect_check.hpp.
namespace glintfx::draw2d {

namespace {

// Each enumeration is a switch over every value, with `return false;` after it. A value a newer
// glintfx adds is then refused, and never taken for a default (docs/api-conventions.md R4).
[[nodiscard]] bool is_known_unit(glintfx::gltfx_stroke_unit unit) noexcept {
    switch (unit) {
    case glintfx::gltfx_stroke_unit::world:
    case glintfx::gltfx_stroke_unit::pixel:
        return true;
    }
    return false;
}

[[nodiscard]] bool is_known_align(glintfx::gltfx_stroke_align align) noexcept {
    switch (align) {
    case glintfx::gltfx_stroke_align::centered:
    case glintfx::gltfx_stroke_align::inside:
    case glintfx::gltfx_stroke_align::outside:
        return true;
    }
    return false;
}

} // namespace

gltfx_draw_2d_refusal refusal_of_stroke_rect(const glintfx::gltfx_rect_world &rect,
                                             const glintfx::gltfx_rect_outline_style &style,
                                             glintfx::gltfx_rgba color) noexcept {
    const gltfx_draw_2d_refusal by_value =
        refusal_of_values(rect.corner.x, rect.corner.y, rect.size.x, rect.size.y,
                          style.stroke.thickness, color.red, color.green, color.blue, color.alpha);
    if (by_value != gltfx_draw_2d_refusal::none) {
        return by_value;
    }
    if (rect.size.x < 0.0 || rect.size.y < 0.0 || style.stroke.thickness < 0.0) {
        return gltfx_draw_2d_refusal::negative_size;
    }
    if (!is_known_unit(style.stroke.unit) || !is_known_align(style.align)) {
        return gltfx_draw_2d_refusal::unknown_option;
    }
    return gltfx_draw_2d_refusal::none;
}

} // namespace glintfx::draw2d
