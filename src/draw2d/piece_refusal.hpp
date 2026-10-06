// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/core/color.hpp>
#include <glintfx/core/rect.hpp>
#include <glintfx/draw2d/frame_2d_report.hpp>

#include "draw2d/quad_vertices.hpp"

// draw2d/piece_refusal.hpp - R2D-BATCH (D-B7-1, I4): which piece is refused, and why. One subject,
// taken out of frame_report_tally.hpp, which only counts what happened to the frame (GODS_LAWS.md
// L-17: a sentence with no "and"). Pure: no GL, no operating system, allocates nothing, throws
// nothing.
//
// REFUSAL BY VALUE: a piece is refused when a value in it is not a finite number, or a rectangle
// has a negative width or height. ONE reason per piece, in the order of the vocabulary:
// not_a_number, then infinite, then negative_size. A quad with zero area is DRAWN (it covers no
// pixel) - a shape, not a bad value. The report keeps the reason of the FIRST refused piece only.
namespace glintfx::draw2d {

// Why a quad in world position, with this color, is refused (`none` when it is not).
[[nodiscard]] gltfx_draw_2d_refusal refusal_of_quad(const quad_corners_world &corners,
                                                    glintfx::gltfx_rgba color) noexcept;

// The same for a rectangle in world position, which can also have a negative size.
[[nodiscard]] gltfx_draw_2d_refusal refusal_of_rect(const glintfx::gltfx_rect_world &rect,
                                                    glintfx::gltfx_rgba color) noexcept;

} // namespace glintfx::draw2d
