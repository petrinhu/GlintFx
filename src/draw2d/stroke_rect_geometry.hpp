// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include <glintfx/core/rect.hpp>
#include <glintfx/draw2d/stroke.hpp>

#include "draw2d/quad_vertices.hpp"

// draw2d/stroke_rect_geometry.hpp - R2D-SHAPES, fatia S4 (D-W8-06, D-W8-271, D-W8-273, D-W8-281):
// the outline of ONE rectangle on the SCREEN, in pixels, in double precision, and the triangles
// that fill it. Pure: no GL, no window, no report, allocates nothing; the caller gives the storage.
//
// THE TWO POLYGONS. The outline lies between an OUTER and an INNER polygon of four corners each, in
// the order of core/quad.hpp: top-left, top-right, bottom-right, bottom-left of the rectangle as
// the consumer wrote it, before any transform. The alignment decides how far each lies from the
// edge, in the unit of the thickness:
//   - centered: the outer half the thickness out, the inner half the thickness in;
//   - inside: the outer IS the rectangle, the inner the whole thickness in;
//   - outside: the outer the whole thickness out, the inner IS the rectangle.
// The corners are square: a miter join at a right angle, the default join of the browser canvas,
// Cairo, Skia and NanoVG (D-S4-1).
//
// THE UNIT decides where the distances are measured (D-W8-273):
//   - world: the two polygons are the rectangle grown and shrunk in the WORLD, then carried by the
//     batch transform, so the sides scale with it;
//   - pixel: the four corners are carried first, and the polygons are grown and shrunk on the
//     SCREEN, along the directions the transform gives to the two sides of the rectangle, so the
//     thickness is the same at every zoom. A side the transform flattens (a scale of zero) has no
//     direction of its own and takes the other side's turned a quarter of a turn; with both sides
//     flattened, +x and +y of the screen (D-S4-4). The directions are computed from coordinates
//     scaled by their larger one, so they neither overflow nor vanish for any finite transform.
//
// THE HOLE. When the inner polygon has no area (its width or its height, measured in the space of
// the unit, is zero or less: the thickness reaches the middle of the rectangle, or the rectangle
// has no width or no height), there is no hole and the outline is the outer polygon alone, filled;
// as Skia's strokeRect draws it, and as the union a path stroke paints in the canvas and in Cairo
// (D-W8-271, D-S4-2, D-S4-3).
//
// THE TRIANGLES. With a hole: four trapezoids, side k from outer k and outer k + 1 to inner k + 1
// and inner k (k + 1 taken around), each split by its diagonal from outer k: (k, k + 1, 4 + k + 1)
// and (k, 4 + k + 1, 4 + k), indices into the outer four then the inner four. The trapezoids only
// share edges, so no pixel is covered twice and a half-transparent outline has no darker corner.
// Without a hole: the outer quad, (0, 1, 2) and (0, 2, 3).
namespace glintfx::draw2d {

struct rect_outline {
    std::array<pixel_position_wide, 4> outer{};
    std::array<pixel_position_wide, 4> inner{};
    bool has_hole = false;
};

inline constexpr std::size_t k_rect_outline_max_points = 8U;
inline constexpr std::size_t k_rect_outline_max_indices = 24U;

// The outline of `rect` drawn with `style`, by the batch transform `affine`.
[[nodiscard]] rect_outline rect_outline_of(const glintfx::gltfx_rect_world &rect,
                                           const glintfx::gltfx_rect_outline_style &style,
                                           const pixel_affine &affine) noexcept;

// Writes the points of `outline` in the order the indices read them: the outer four, then, with a
// hole, the inner four. Returns how many it wrote: 8, 4, or 0 (nothing written) when `points`
// holds fewer than k_rect_outline_max_points.
[[nodiscard]] std::size_t write_rect_outline_points(const rect_outline &outline,
                                                    std::span<pixel_position_wide> points) noexcept;

// Writes the indices of the triangles in the order above. Returns how many it wrote: 24 with a
// hole, 6 without, or 0 (nothing written) when `indices` holds fewer than
// k_rect_outline_max_indices.
[[nodiscard]] std::size_t write_rect_outline_indices(bool has_hole,
                                                     std::span<std::uint32_t> indices) noexcept;

} // namespace glintfx::draw2d
