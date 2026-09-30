// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>

#include <glintfx/core/transform.hpp>
#include <glintfx/core/vec2.hpp>

// draw2d/quad_vertices.hpp - R2D-BATCH, fatia B2a (docs/plano-w7d.md sec. 4.3, D-W7D-09;
// docs/auditoria-api-draw2d.md B0-C1): the PURE atom that turns the four corners of one piece
// from WORLD position (double) into PIXEL position (float) through ONE transform. It knows
// nothing of GL, of the operating system or of any batch: it takes values and returns values, so
// it compiles and runs on all five systems, and includes nothing but core/ (GODS_LAWS.md L-19:
// src/draw2d/ never reaches src/platform/).
//
// THE DECISION THIS ATOM CARRIES (the leader's precision decision, "mundo double, tela float"):
// the transform is applied to world positions that were NEVER narrowed, in DOUBLE, and only the
// finished pixel positions become single precision. Two ways to break that, both refused here:
//   - narrowing a corner before transforming it (a world x of 16 777 217 cannot be held by a
//     float, so the piece would land on the wrong pixel);
//   - narrowing the transform before using it (B0-C1: the camera at x = 16 777 217 cannot be held
//     by a float either, and the matrix that reaches the graphics card IS single precision).
// That is why the transform reaches this atom as a pixel_affine of DOUBLES, prepared once per
// batch from the gltfx_transform, and never as the float gltfx_mat3 of the graphics card. The
// float matrix that does go to the card is built elsewhere, once per frame.
//
// THE ORDER of the three parts is the one of core/transform.hpp: SCALE first, then ROTATION, then
// TRANSLATION; the same convention as gltfx_mat3_from_transform (x' = cos*sx*x - sin*sy*y + tx,
// y' = sin*sx*x + cos*sy*y + ty).
//
// NOTHING IS VALIDATED HERE: a corner that is not a finite number comes out as not finite, and a
// quad with zero area comes out as four (equal) vertices. Refusing a piece by value, and counting
// it, belongs to the frame report (a later unit); this atom never hides the value it was given.
namespace glintfx::draw2d {

// The four corners of a piece, in WORLD position, by ROLE: [0] top_left, [1] top_right,
// [2] bottom_right, [3] bottom_left (the order of core/quad.hpp; walking 0 -> 1 -> 2 -> 3 goes once
// around the quad).
using quad_corners_world = std::array<gltfx_vec2_world, 4>;

// The same four corners, in PIXEL position, in the same order.
using quad_corners_pixel = std::array<gltfx_vec2_screen, 4>;

// The world -> pixel map of one batch, in DOUBLE: two columns and a translation. Prepared once
// (the two trigonometric calls), used for every piece of the batch.
struct pixel_affine {
    double column0_x = 1.0;
    double column0_y = 0.0;
    double column1_x = 0.0;
    double column1_y = 1.0;
    double translation_x = 0.0;
    double translation_y = 0.0;
};

[[nodiscard]] pixel_affine pixel_affine_from_transform(gltfx_transform transform) noexcept;

// The four corners of a piece in pixel position. World in, double arithmetic, float out.
[[nodiscard]] quad_corners_pixel quad_vertices(const quad_corners_world &corners,
                                               const pixel_affine &affine) noexcept;

} // namespace glintfx::draw2d
