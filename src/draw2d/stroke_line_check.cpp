// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/stroke_line_check.hpp"

#include "draw2d/piece_refusal.hpp"
#include "draw2d/stroke_unit.hpp"

// draw2d/stroke_line_check.cpp - R2D-SHAPES, fatia S3: the check of stroke_line_check.hpp.
namespace glintfx::draw2d {

namespace {

// Each enumeration is a switch over every value, with `return false;` after it. A value a newer
// glintfx adds is then refused, and never taken for a default (docs/api-conventions.md R4).
[[nodiscard]] bool is_known_cap(glintfx::gltfx_line_cap cap) noexcept {
    switch (cap) {
    case glintfx::gltfx_line_cap::flat:
    case glintfx::gltfx_line_cap::square:
    case glintfx::gltfx_line_cap::rounded:
        return true;
    }
    return false;
}

} // namespace

gltfx_draw_2d_refusal refusal_of_stroke_line(const glintfx::gltfx_segment_world &segment,
                                             const glintfx::gltfx_line_style &style,
                                             glintfx::gltfx_rgba color) noexcept {
    const gltfx_draw_2d_refusal by_value =
        refusal_of_values(segment.start.x, segment.start.y, segment.end.x, segment.end.y,
                          style.stroke.thickness, color.red, color.green, color.blue, color.alpha);
    if (by_value != gltfx_draw_2d_refusal::none) {
        return by_value;
    }
    if (style.stroke.thickness < 0.0) {
        return gltfx_draw_2d_refusal::negative_size;
    }
    if (!is_known_stroke_unit(style.stroke.unit) || !is_known_cap(style.cap)) {
        return gltfx_draw_2d_refusal::unknown_option;
    }
    return gltfx_draw_2d_refusal::none;
}

} // namespace glintfx::draw2d
