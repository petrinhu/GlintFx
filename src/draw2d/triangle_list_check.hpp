// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include <glintfx/draw2d/frame_2d_report.hpp>
#include <glintfx/draw2d/texture_region.hpp>
#include <glintfx/draw2d/vertex_2d.hpp>

#include "draw2d/texture_extent.hpp"

// draw2d/triangle_list_check.hpp - R2D-SHAPES, fatia S2b (docs/plano-fechamento-w8-r19.md, §3 S2b;
// the closed enumeration of twelve cells in r2 §3): the PURE check of one call of fill_triangles()
// or fill_indexed_triangles(). It knows nothing of GL, of the window or of the report; it takes the
// counts apart from the data, so the case of 2^32 vertices is tested without allocating.
//
// THE RULES, in the order they are applied (D-W8-278, D-W8-287):
//   1. an empty call (no vertices, or, indexed, no indices) is NOTHING: it is not a piece, and it
//      is not counted anywhere;
//   2. the counts: more than 2^32 vertices, which a 32-bit index cannot address, is
//      too_many_vertices (2^32 itself is accepted), then a vertex count (or an index count,
//      indexed) that is not a multiple of three is count_not_multiple_of_three;
//   3. the index range (indexed only): an index that is not a vertex of the span is
//      index_out_of_range;
//   4. the values, in the order of the vocabulary (the same as a quad's, piece_refusal.hpp): a NaN
//      in any position or colour of the call is not_a_number; then an infinity in any of them is
//      infinite (D-W8-287 (c); S2b-fix1).
// Whether a position is in the pixel range once the batch transform turns it is not
// asked here (it needs the transform): triangle_pixel_vertices.hpp asks it.
// A refusal refuses the WHOLE call, once (D-W8-287).
//
// THE TEXTURED CALL (R2D-TEXTURE T5, D-W8-672 and D-W8-673): fill_textured_triangles() and
// fill_indexed_textured_triangles() keep rules 1 to 3 as they are, and rule 4 reads the texel
// coordinate too: a NaN in any position, tint or texel coordinate of the call is not_a_number, then
// an infinity in any of them is infinite. After the drawing call has asked about the texture
// (texture_not_open, then texture_other_renderer: only the renderer knows), rule 5 is
// region_out_of_range: a texel coordinate outside 0..width by 0..height of the texture (its edges
// included), the reason a source region outside the texture has in draw_texture().
namespace glintfx::draw2d {

// The counts of one call, apart from its data. 64-bit, so the 2^32 case exists on every system.
struct triangle_call_counts {
    std::uint64_t vertex_count = 0;
    std::uint64_t index_count = 0;
    bool indexed = false;
};

// True when the call is empty: nothing to draw, nothing to count (rule 1).
[[nodiscard]] bool is_empty_triangle_call(const triangle_call_counts &counts) noexcept;

// Rule 2, by the counts alone.
[[nodiscard]] gltfx_draw_2d_refusal
refusal_of_triangle_counts(const triangle_call_counts &counts) noexcept;

// Rule 3: every index must be a vertex of a span of `vertex_count` vertices.
[[nodiscard]] gltfx_draw_2d_refusal
refusal_of_triangle_indices(std::span<const std::uint32_t> indices,
                            std::uint64_t vertex_count) noexcept;

// Rule 4: the positions first, then the colours.
[[nodiscard]] gltfx_draw_2d_refusal
refusal_of_triangle_values(std::span<const gltfx_vertex_2d_world> vertices) noexcept;

// The four rules, in order, for one whole call. `indices` is read only when `indexed` is true.
[[nodiscard]] gltfx_draw_2d_refusal
refusal_of_triangle_call(std::span<const gltfx_vertex_2d_world> vertices,
                         std::span<const std::uint32_t> indices, bool indexed) noexcept;

// Rule 4 of a textured call (T5): the positions, the tints and the texel coordinates.
[[nodiscard]] gltfx_draw_2d_refusal
refusal_of_textured_triangle_values(std::span<const gltfx_textured_vertex_2d> vertices) noexcept;

// Rules 2 to 4, in order, for one whole textured call (rule 1 is asked by the caller first).
[[nodiscard]] gltfx_draw_2d_refusal
refusal_of_textured_triangle_call(std::span<const gltfx_textured_vertex_2d> vertices,
                                  std::span<const std::uint32_t> indices, bool indexed) noexcept;

// Rule 5 of a textured call (T5): `region_out_of_range` when a texel coordinate lies outside
// 0..width by 0..height (the edges are inside); `none` otherwise. The values were already checked
// (finite).
[[nodiscard]] gltfx_draw_2d_refusal
refusal_of_texel_range(std::span<const gltfx_textured_vertex_2d> vertices,
                       texture_extent extent) noexcept;

} // namespace glintfx::draw2d
