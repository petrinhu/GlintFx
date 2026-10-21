// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cmath>

// draw2d/renderer_2d_desc_check.hpp - R2D-SHAPES, fatia S5-C1 (D-W8-566): the PURE check of the one
// field of gltfx_renderer_2d_desc that open() can refuse, curve_max_error_pixels. Header only, so
// no test target gains a source file (the precedent of stroke_unit.hpp, D-W8-410).
//
// THE RULE (D-W8-284): a finite number greater than zero; no other interval. NaN, both infinities,
// zero, -0.0 and every negative value are refused; the smallest positive subnormal and the largest
// double are accepted (the cost of a tiny value has the ceiling of arc_parameters.hpp, not a
// floor).
namespace glintfx::draw2d {

[[nodiscard]] inline bool is_valid_curve_max_error(double pixels) noexcept {
    return std::isfinite(pixels) && pixels > 0.0;
}

} // namespace glintfx::draw2d
