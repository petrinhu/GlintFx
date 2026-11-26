// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/core/color.hpp>
#include <glintfx/core/ellipse.hpp>
#include <glintfx/draw2d/frame_2d_report.hpp>
#include <glintfx/draw2d/stroke.hpp>

// draw2d/ellipse_check.hpp - R2D-SHAPES, fatia S5-B (D-W8-410): the PURE check of one call of
// fill_ellipse() or stroke_ellipse(). It knows nothing of GL, of the window, of the transform or of
// the report: whether the outline lands in the pixel range the vertex program can scale needs the
// transform, and the pixels of the ellipse ask it.
//
// ONE REASON PER CALL, in the order of the vocabulary (the same order as a rectangle's,
// piece_refusal.hpp, a line's, stroke_line_check.hpp, and a rectangle outline's):
//   1. not_a_number: a NaN in the centre, a radius, the rotation, the thickness (stroke) or the
//      colour;
//   2. infinite: an infinity in any of them;
//   3. negative_size: a radius or a thickness below zero (-0.0 is zero, not below it);
//   4. unknown_option: a thickness unit outside its enumeration (stroke.hpp, D-W8-285).
// A radius of zero is NOT refused (D-W8-414): the ellipse is a shape. Filled, it is a piece of zero
// area, drawn and counted. Outlined, it is the outline the same thickness gives every other
// ellipse, continued: a disc when both radii are zero, a bar with rounded ends when one is.
namespace glintfx::draw2d {

[[nodiscard]] gltfx_draw_2d_refusal
refusal_of_fill_ellipse(const glintfx::gltfx_ellipse_world &ellipse,
                        glintfx::gltfx_rgba color) noexcept;

[[nodiscard]] gltfx_draw_2d_refusal
refusal_of_stroke_ellipse(const glintfx::gltfx_ellipse_world &ellipse,
                          const glintfx::gltfx_stroke &stroke, glintfx::gltfx_rgba color) noexcept;

} // namespace glintfx::draw2d
