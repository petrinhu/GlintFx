// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include <glintfx/draw2d/frame_2d_report.hpp>
#include <glintfx/draw2d/vertex_2d.hpp>

// draw2d/triangle_list_check.hpp - R2D-SHAPES, fatia S2b (docs/plano-fechamento-w8-r19.md, §3 S2b;
// the closed enumeration of twelve cells in r2 §3): the PURE check of one call of fill_triangles()
// or fill_indexed_triangles(). It knows nothing of GL, of the window or of the report; it takes the
// counts apart from the data, so the case of 2^32 vertices is tested without allocating.
//
// THE RULES, in the order they are applied (D-W8-278, D-W8-287):
//   1. an empty call (no vertices, or, indexed, no indices) is NOTHING: it is not a piece, and it
//      is not counted anywhere;
//   2. the counts: more vertices than a 32-bit index can address is too_many_vertices, then a
//      vertex count (or an index count, indexed) that is not a multiple of three is
//      count_not_multiple_of_three;
//   3. the index range (indexed only): an index that is not a vertex of the span is
//      index_out_of_range;
//   4. the values: a vertex whose position is not a finite number is not_a_number; then a colour
//      component that is NaN is not_a_number, and one that is infinite is infinite.
// A refusal refuses the WHOLE call, once (D-W8-287).
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

} // namespace glintfx::draw2d
