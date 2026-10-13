// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/core/color.hpp>
#include <glintfx/core/rect.hpp>
#include <glintfx/draw2d/frame_2d_report.hpp>
#include <glintfx/draw2d/stroke.hpp>

// draw2d/stroke_rect_check.hpp - R2D-SHAPES, fatia S4: the PURE check of one call of stroke_rect().
// It knows nothing of GL, of the window, of the transform or of the report: whether the outline
// lands in the pixel range the vertex program can scale needs the transform, and
// stroke_rect_pixels.hpp asks it.
//
// ONE REASON PER CALL, in the order of the vocabulary (the same order as a rectangle's,
// piece_refusal.hpp, and as a line's, stroke_line_check.hpp):
//   1. not_a_number: a NaN in the corner, the size, the thickness or the colour;
//   2. infinite: an infinity in any of them;
//   3. negative_size: a width, a height or a thickness below zero (-0.0 is zero, not below it);
//   4. unknown_option: a thickness unit or an alignment outside its enumeration (stroke.hpp,
//      D-W8-285).
// A thickness of zero, or a rectangle whose width or height is zero, is NOT refused: it is a shape,
// drawn by the rule of stroke_rect_geometry.hpp (frame_2d_report.hpp, pieces_drawn).
namespace glintfx::draw2d {

[[nodiscard]] gltfx_draw_2d_refusal
refusal_of_stroke_rect(const glintfx::gltfx_rect_world &rect,
                       const glintfx::gltfx_rect_outline_style &style,
                       glintfx::gltfx_rgba color) noexcept;

} // namespace glintfx::draw2d
