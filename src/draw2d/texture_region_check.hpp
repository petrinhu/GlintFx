// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/core/quad.hpp>
#include <glintfx/core/rect.hpp>
#include <glintfx/draw2d/frame_2d_report.hpp>
#include <glintfx/draw2d/texture_region.hpp>

#include "draw2d/texture_region_uv.hpp"

// draw2d/texture_region_check.hpp - R2D-TEXTURE, fatia T4b-A (D-W8-290, D-W8-457): the PURE checks
// of one draw_texture() or draw_texture_quad() call. They know nothing of GL, of the renderer, of
// the transform or of the report.
//
// ONE REASON PER CALL, in the order of the vocabulary (frame_2d_report.hpp), which the drawing call
// keeps by asking in this order:
//   1. not_a_number: a NaN in the rectangle (or a corner), the source region or the tint;
//   2. infinite: an infinity in any of them;
//   3. negative_size: a rectangle or a source region with a negative width or height (-0.0 is
//      zero, not below it);
//   4. unknown_option: a mirroring outside its enumeration;
//   then the drawing call itself asks about the texture: texture_not_open, then
//   texture_other_renderer (the renderer knows; these checks cannot);
//   5. region_out_of_range: the source region reaches outside the texture
//      (refusal_of_region_range(), which needs the texture's size, so it comes after the texture
//      is known to be open);
//   6. degenerate_quad: the corners of draw_texture_quad() define no projective mapping
//      (projective_quad.hpp), last, because it is a property of the shape, not of a value.
// A rectangle or a region of zero width or height is NOT refused: it is a piece of zero area,
// drawn, covering no pixel (frame_2d_report.hpp, pieces_drawn).
namespace glintfx::draw2d {

// Reasons 1 to 4 for draw_texture().
[[nodiscard]] gltfx_draw_2d_refusal
refusal_of_texture_rect(const glintfx::gltfx_texture_paint &paint,
                        const glintfx::gltfx_rect_world &rect) noexcept;

// Reasons 1 to 4 for draw_texture_quad() (a quadrilateral has no size, so no negative_size of its
// own; the source region still has one).
[[nodiscard]] gltfx_draw_2d_refusal
refusal_of_texture_quad(const glintfx::gltfx_texture_paint &paint,
                        const glintfx::gltfx_quad_world &corners) noexcept;

// Reason 5: `region_out_of_range` when the source rectangle does not lie inside 0..width by
// 0..height (its edges may touch the texture's edges); `none` otherwise. The region's values were
// already checked (finite, sizes zero or more).
[[nodiscard]] gltfx_draw_2d_refusal refusal_of_region_range(const glintfx::gltfx_rect_texel &source,
                                                            texture_extent extent) noexcept;

} // namespace glintfx::draw2d
