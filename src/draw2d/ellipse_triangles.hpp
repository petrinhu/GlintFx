// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "draw2d/quad_vertices.hpp"

// draw2d/ellipse_triangles.hpp - R2D-SHAPES, fatia S5-B (D-W8-413): the triangles that fill a
// filled ellipse and a thick outline, as indices into its points. Pure: no GL, no window,
// allocates nothing.
//
// THE FAN (D-W8-272): vertex 0 is the centre and vertices 1 to n go once around the rim; triangle k
// is (0, k, k + 1), the last one closing on vertex 1. n triangles, 3 n indices. A filled ellipse,
// and an outline whose hole is empty, are fans.
//
// THE RING: points 0 to n - 1 go once around the outer polygon, points n to 2 n - 1 once around the
// inner one, the same way, point n + k facing point k. Each pair of neighbours k, k + 1 (k + 1
// taken round to 0) closes a quadrilateral (outer k, outer k + 1, inner k + 1, inner k), cut in two
// triangles; 2 n triangles, 6 n indices. The cut follows the diagonal (outer k, inner k + 1) when
// both of its triangles turn the way the outer polygon turns (or have no area), and the other
// diagonal (outer k + 1, inner k) otherwise: a quadrilateral whose inner side folds (where the hole
// of a thick outline has a corner, ellipse_outline.hpp) is then still covered once, never twice. No
// triangle is left out, so the count of indices is fixed by n.
namespace glintfx::draw2d {

// Writes the fan of a rim of `rim_count` points into `out` and answers the count written, 3
// rim_count; 0, writing nothing, when rim_count is 0, when the vertices would not fit a 32-bit
// index, or when `out` holds fewer.
[[nodiscard]] std::size_t write_fan_indices(std::size_t rim_count,
                                            std::span<std::uint32_t> out) noexcept;

// Writes the ring of `points` (2 n of them, n at least 3, positions on the screen) into `out` and
// answers the count written, 6 n; 0, writing nothing, when the count of points is odd or below 6,
// when they would not fit a 32-bit index, or when `out` holds fewer.
[[nodiscard]] std::size_t write_ring_indices(std::span<const pixel_position_wide> points,
                                             std::span<std::uint32_t> out) noexcept;

} // namespace glintfx::draw2d
