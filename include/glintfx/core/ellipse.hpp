// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/core/angle.hpp>
#include <glintfx/core/vec2.hpp>

// core/ellipse.hpp - R2D-SHAPES (docs/auditoria-api-draw2d.md, section 9;
// decision D-W8-07 of the W8 plan). An ellipse in WORLD space: centre,
// two radii, and a rotation about the centre. A circle is an ellipse whose
// two radii are equal; there is no second type for it (GODS_LAWS.md L-17).
//
// The radii are NAMED (radius_x, radius_y), not a pair: the reader sees which
// one is horizontal before the rotation is applied. The rotation is
// gltfx_angle, never a bare double, so degrees cannot pass for radians
// (GODS_LAWS.md L-19).
//
// Pure geometry, like core/quad.hpp: no drawing, no GL, no operating system.

namespace glintfx {

struct gltfx_ellipse_world {
    gltfx_vec2_world center{};
    double radius_x = 0.0;
    double radius_y = 0.0;
    gltfx_angle rotation{};
};

} // namespace glintfx
