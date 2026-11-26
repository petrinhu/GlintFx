// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/core/err.hpp>
#include <glintfx/core/light_2d.hpp>
#include <glintfx/core/quad.hpp>
#include <glintfx/core/vec2.hpp>
#include <glintfx/export.hpp>

// core/projected_shadow.hpp - R2D-SPRITE-SHADE (docs/auditoria-api-sprite-
// shade.md, H0; decisions D-W8-16 and D-W8-27). The shadow of an upright
// piece on the ground plane, as a quadrilateral. Pure geometry, like
// core/quad.hpp: no drawing, no GL, no operating system.
//
// DETERMINISM, as promised: the same inputs give the same bits on the same
// binary. Between systems each component agrees within 4 ULP of the scale of
// the account: at most 4 * ulp(M) from the expected value, M being the largest
// absolute value among the inputs and the outputs of that projection
// (coordinates, heights and result). The same rule covers a component that
// must be zero; no absolute floor is used.
// Proved by: projected_shadow_test.
//
// THE PIECE. A standing piece has its base segment on the ground and stands
// `height` above it. Each base point is a ground point; its top is the same
// point lifted by `height`. The shadow of a point at height h is where the
// ray from the light, through that point, meets the ground.
//
// THE RESULT, and the corner roles it uses. The quadrilateral takes the
// roles of gltfx_quad_world (top_left, top_right, bottom_right, bottom_left),
// chosen so that a texture drawn on it reads correctly:
//   top_left     = the shadow of the top of base_start (the far end of the shadow)
//   top_right    = the shadow of the top of base_end
//   bottom_right = base_end   (on the ground: the piece's own base point)
//   bottom_left  = base_start
// A sprite's texture, whose top-left texel is at the top of the sprite and
// whose bottom-left texel is at base_start, lands on its own shadow with no
// swap of corners.
//
// Two examples the tests check, corners in the order above:
//   directional, azimuth 0 degrees, elevation 45, base (0,0)-(0,10), height
//     20: the shadow is (-20,0), (-20,10), (0,10), (0,0).
//   point at (10,-30), height 40, base (5,0)-(15,0), height 20: the shadow is
//     (0,30), (20,30), (15,0), (5,0).
//
// Proved by: projected_shadow_test.

namespace glintfx {

struct gltfx_standing_piece_2d {
    // The two ends of the base on the ground, in the order left, right, as
    // the camera sees the piece.
    gltfx_vec2_world base_start{};
    gltfx_vec2_world base_end{};
    // The height above the ground of the piece's top. Finite, and zero or
    // greater. A negative or not finite height is refused: invalid_argument,
    // rejected_value() "piece_height". Zero is not refused: the shadow is
    // degenerate and covers no pixel.
    double height = 0.0;
};

// REFUSALS, all gltfx_err with code invalid_argument and the field named in
// rejected_value() (docs/api-conventions.md R1, R4):
//   "elevation"    : the elevation is not in (0, 90] degrees;
//   "light_height" : the point light's height is not finite, or not greater
//                    than piece.height;
//   "piece_height" : the piece's height is not finite or is negative;
//   "position"     : a base coordinate or a light position is not finite;
//   "azimuth"      : the azimuth of a directional light is not finite;
//   "shadow_extent": every input above is accepted, but a corner of the
//                    shadow would not be a finite number: an elevation so
//                    close to 0 that the shadow is longer than a double can
//                    hold (even for a piece of height zero), or coordinates
//                    and heights near the largest double. An accepted result
//                    never holds an infinite or NaN corner.
// A zero-length base is NOT refused: the quadrilateral degenerates to a
// segment and covers no pixel.
//
// An elevation of exactly 90 degrees gives a shadow of exactly zero length.
// The code does not rely on the cosine of 90 degrees, which is not zero in
// double precision.
// Proved by: projected_shadow_test.
[[nodiscard]] GLINTFX_API gltfx_rslt<gltfx_quad_world>
gltfx_project_directional_shadow(gltfx_directional_light_2d light,
                                 gltfx_standing_piece_2d piece) noexcept;

[[nodiscard]] GLINTFX_API gltfx_rslt<gltfx_quad_world>
gltfx_project_point_shadow(gltfx_point_light_2d light, gltfx_standing_piece_2d piece) noexcept;

} // namespace glintfx
