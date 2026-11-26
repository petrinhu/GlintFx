// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/core/ellipse.hpp>

#include "draw2d/quad_vertices.hpp"

// draw2d/ellipse_frame.hpp - R2D-SHAPES, fatia S5-B (D-W8-411): an ellipse in a PLANE, given two
// ways, and the points of its curve found by the direction of their outward normal. Pure: no GL, no
// window, allocates nothing, refuses nothing (the call was checked before, ellipse_check.hpp).
//
// THE PLANE is the one the outline is built in: the world, for an outline whose thickness is in
// world unit (it is built there and carried to the screen by the batch transform, D-W8-273); the
// screen, in pixels, for an outline in pixel unit and for every fill. Its points are two doubles,
// never narrowed.
//
// TWO FORMS OF ONE ELLIPSE:
//   - CONJUGATE: the centre and two half-axes u and v, the images of the two radii: the curve is
//     center + u cos t + v sin t. A transform of the house carries a conjugate ellipse to a
//     conjugate ellipse, so this is the form the transform and arc_parameters.hpp speak.
//   - PRINCIPAL: the centre, the LONGER half-axis `major` along the unit direction
//     `major_direction`, and the shorter `minor` along that direction turned a quarter
//     counter-clockwise. The outline of a thick stroke is built in this form (D-W8-412).
// principal_of() goes from the first to the second by the closed form of the 2x2 singular values,
// on half-axes divided first by a power of two (only the exponent changes), so no square overflows
// or underflows, and the shorter half-axis comes from the determinant (computed with one rounding
// by the fused multiply-add correction), never from a difference that cancels: a long thin ellipse
// keeps its thickness. The two half-axes use only operations that round once (with the build's
// -ffp-contract=off, cmake/GlintfxCompileOptions.cmake), so every system gives them the same bits
// (GODS_LAWS.md L-04); the direction comes from atan2, cos and sin of the system's library, whose
// last bit may differ between systems.
//
// THE NORMAL ANGLE phi of a point of the curve is the angle of its OUTWARD normal, measured from
// major_direction towards the minor axis. Every phi names one point, also on a curve with no
// thickness: with both half-axes zero the point is the centre; with the shorter one zero (a
// segment), it is the end of the segment the normal leans to (the sign of cos phi), so the outline
// of a thick segment is its two rounded ends joined by straight sides.
namespace glintfx::draw2d {

struct plane_point {
    double x = 0.0;
    double y = 0.0;
};

struct conjugate_ellipse {
    plane_point center{};
    plane_point axis_u{};
    plane_point axis_v{};
};

struct principal_ellipse {
    plane_point center{};
    plane_point major_direction{1.0, 0.0};
    double major = 0.0;
    double minor = 0.0;
};

// The ellipse in the world: u = (rx cos r, rx sin r), v = (-ry sin r, ry cos r), r the rotation.
[[nodiscard]] conjugate_ellipse
world_conjugate_ellipse(const glintfx::gltfx_ellipse_world &ellipse) noexcept;

// The same ellipse on the screen: the centre by `affine`, the two half-axes by its linear part.
[[nodiscard]] conjugate_ellipse
screen_conjugate_ellipse(const glintfx::gltfx_ellipse_world &ellipse,
                         const pixel_affine &affine) noexcept;

// The principal form of `ellipse`. Both half-axes zero give major = minor = 0 and the direction
// (1, 0).
[[nodiscard]] principal_ellipse principal_of(const conjugate_ellipse &ellipse) noexcept;

// The OFFSET from the centre of the point center + u cos t + v sin t.
[[nodiscard]] plane_point offset_at_parameter(const conjugate_ellipse &ellipse, double t) noexcept;

// The normal angle, in [0, 2 pi), of the point of `principal` at `offset` from its centre (an
// offset of offset_at_parameter() of the same ellipse). 0 when both half-axes are zero.
[[nodiscard]] double normal_angle_of_offset(const principal_ellipse &principal,
                                            const plane_point &offset) noexcept;

// The point of the curve whose outward normal has the angle `phi`, in the PRINCIPAL axes: x along
// major_direction, y along the minor axis, both from the centre. Its outward unit normal there is
// (cos phi, sin phi).
[[nodiscard]] plane_point local_point_at_normal_angle(const principal_ellipse &principal,
                                                      double phi) noexcept;

// A point given in the principal axes (as local_point_at_normal_angle() gives it), back in the
// plane.
[[nodiscard]] plane_point plane_point_of_local(const principal_ellipse &principal,
                                               const plane_point &local) noexcept;

} // namespace glintfx::draw2d
