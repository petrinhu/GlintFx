// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>
#include <optional>

#include <glintfx/core/quad.hpp>

// draw2d/projective_quad.hpp - R2D-TEXTURE, fatia T4b-A (D-W8-13, D-W8-290, D-W8-455): the weights
// q that make a texture drawn into a free quadrilateral PROJECTIVE, so a straight line of the
// texture stays a straight line on the screen even when the quadrilateral is a trapezoid. Pure: no
// GL, no window, allocates nothing.
//
// THE TECHNIQUE (D-W8-455; learned from N. Reed, "Quadrilateral Interpolation, Part 1", and
// rewritten here, GODS_LAWS.md L-29): the two triangles of the quadrilateral carry, at each corner,
// the texture coordinate times q and q itself (the vertex of triangle_batch.hpp, T4a); the program
// interpolates the three linearly and divides by the third, which is the projective interpolation.
// With the diagonals from corner 0 to corner 2 and from corner 1 to corner 3 crossing at the point
// that lies the fraction s along the first and t along the second, the weights are
//   q0 = 1 / (2 (1 - s)),  q1 = 1 / (2 (1 - t)),  q2 = 1 / (2 s),  q3 = 1 / (2 t),
// halved so a parallelogram (s = t = 1/2) gives q = 1 at every corner, the weight every other
// piece already carries. An affine transform keeps s and t, so the weights of the world corners
// are those of the screen corners. One set per quadrilateral, so textured quadrilaterals batch like
// every other piece.
//
// THE CORNERS are the roles of core/quad.hpp, walked once around: top_left (0), top_right (1),
// bottom_right (2), bottom_left (3).
//
// NO WEIGHTS (std::nullopt) when the corners define no projective mapping (`degenerate_quad`): the
// diagonals do not cross strictly inside both of them. That is three corners on one line (a
// quadrilateral of zero area is one of these), two edges that cross each other, or a corner that
// points inward (a concave quadrilateral, whose diagonals cross outside one of them). Also when a
// value is not a finite number, or a weight would not be one (a corner so close to the crossing
// that 1 / (2 s) overflows).
namespace glintfx::draw2d {

[[nodiscard]] std::optional<std::array<double, 4>>
projective_weights(const glintfx::gltfx_quad_world &corners) noexcept;

} // namespace glintfx::draw2d
