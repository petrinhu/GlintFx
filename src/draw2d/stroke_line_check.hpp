// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/core/color.hpp>
#include <glintfx/core/segment.hpp>
#include <glintfx/draw2d/frame_2d_report.hpp>
#include <glintfx/draw2d/stroke.hpp>

// draw2d/stroke_line_check.hpp - R2D-SHAPES, fatia S3: the PURE check of one call of stroke_line().
// It knows nothing of GL, of the window, of the transform or of the report: whether the line lands
// in the pixel range the vertex program can scale needs the transform, and stroke_line_pixels.hpp
// asks it.
//
// ONE REASON PER CALL, in the order of the vocabulary (the same order as a quad's,
// piece_refusal.hpp, and as a triangle call's, triangle_list_check.hpp):
//   1. not_a_number: a NaN in the start, the end, the thickness or the colour;
//   2. infinite: an infinity in any of them;
//   3. negative_size: a thickness below zero (-0.0 is zero, not below it);
//   4. unknown_option: a thickness unit or a line cap outside its enumeration (stroke.hpp,
//   D-W8-285).
// A thickness of zero, or a line of length zero with the `flat` cap, is NOT refused: it is a shape
// whose area is zero, drawn and covering no pixel (frame_2d_report.hpp, pieces_drawn; the same rule
// as a quad of zero area).
namespace glintfx::draw2d {

[[nodiscard]] gltfx_draw_2d_refusal
refusal_of_stroke_line(const glintfx::gltfx_segment_world &segment,
                       const glintfx::gltfx_line_style &style, glintfx::gltfx_rgba color) noexcept;

} // namespace glintfx::draw2d
