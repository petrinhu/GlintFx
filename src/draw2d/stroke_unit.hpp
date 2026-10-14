// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/draw2d/stroke.hpp>

// draw2d/stroke_unit.hpp - R2D-SHAPES, fatia S5-B (D-W8-410): whether a thickness unit is one of
// the values of gltfx_stroke_unit. Asked by the checks of the line, of the rectangle outline and of
// the ellipse outline; extracted at its third use (CONTRACT.md section 6, D-S4-10). Pure: no GL, no
// window, allocates nothing.
namespace glintfx::draw2d {

// A switch over every value, with `return false;` after it: a value a newer glintfx adds is then
// refused, and never taken for a default (docs/api-conventions.md R4).
[[nodiscard]] inline bool is_known_stroke_unit(glintfx::gltfx_stroke_unit unit) noexcept {
    switch (unit) {
    case glintfx::gltfx_stroke_unit::world:
    case glintfx::gltfx_stroke_unit::pixel:
        return true;
    }
    return false;
}

} // namespace glintfx::draw2d
