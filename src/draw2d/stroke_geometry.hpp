// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include <glintfx/core/segment.hpp>
#include <glintfx/draw2d/stroke.hpp>

#include "draw2d/quad_vertices.hpp"

// draw2d/stroke_geometry.hpp - R2D-SHAPES, fatia S3 (D-W8-269, D-W8-270, D-W8-273): the outline of
// ONE stroked straight line on the SCREEN, in pixels, in double precision, and the triangles that
// fill it. Pure: no GL, no window, no report, allocates nothing; the caller gives the storage.
//
// THE FRAME of a line is its two ends on the screen and two vectors of length half the thickness:
// `along` points from the start to the end, `across` is the direction turned a quarter of a turn
// (from +x towards +y, the normal (-dy, dx) of the plan). The thickness unit decides where they are
// measured (D-W8-273):
//   - world: the direction and the half thickness are taken in the WORLD and then carried by the
//     linear part of the batch transform, so the outline is the world outline transformed (a
//     non-uniform scale makes the line thicker on one axis than on the other, as in SVG and Cairo);
//   - pixel: the two ends are transformed first and the direction and the half thickness are taken
//     on the SCREEN, so the thickness stays the same at every zoom (as in Dear ImGui).
// A line of length zero has no direction of its own; it takes +x of the space of its unit (SVG 2,
// paths, 'Path directionality': the positive x-axis), so its square cap is a square aligned with
// the axes of that space (D-W8-270). The direction is computed from halves of the coordinates and
// scaled by its larger coordinate, so it neither overflows nor vanishes for any finite input.
//
// THE OUTLINE is one CONVEX polygon, written once around it:
//   start + across, end + across, [the half disc around the end], end - across, start - across,
//   [the half disc around the start],
// where `flat` ends at the end points (`butt` in SVG and the canvas), `square` moves both ends out
// by `along` (half the thickness, as SVG, the canvas, Cairo, Skia and NanoVG do), and `rounded`
// adds a half disc of radius half the thickness, of `cap_segments` straight segments, around each
// end. A line of zero thickness, or of zero length with `flat`, has an outline of zero area: it is
// drawn and covers no pixel (frame_2d_report.hpp, the same rule as a quad of zero area).
//
// THE TRIANGLES are a fan from the first point, (0, k, k + 1): the polygon is convex, so they do
// not overlap, and a half-transparent line has no darker seam.
namespace glintfx::draw2d {

// A displacement on the screen, in pixels.
struct pixel_offset {
    double x = 0.0;
    double y = 0.0;
};

struct stroke_frame {
    pixel_position_wide start{};
    pixel_position_wide end{};
    pixel_offset along{};
    pixel_offset across{};
};

// The frame of `segment` drawn with `stroke`, by the batch transform `affine`.
[[nodiscard]] stroke_frame stroke_frame_of(const glintfx::gltfx_segment_world &segment,
                                           const glintfx::gltfx_stroke &stroke,
                                           const pixel_affine &affine) noexcept;

// Half the thickness as a radius on the screen, in pixels: itself in pixel unit; in world unit,
// by the largest stretch of the transform (arc_segments.hpp, screen_radius()).
[[nodiscard]] double stroke_cap_radius_pixels(const glintfx::gltfx_stroke &stroke,
                                              const pixel_affine &affine) noexcept;

// How many points the outline has: 4 for `flat` and `square`; 4 + 2 (cap_segments - 1) for
// `rounded` (a cap of one segment, or of none, adds no point).
[[nodiscard]] std::size_t stroke_outline_point_count(glintfx::gltfx_line_cap cap,
                                                     std::uint32_t cap_segments) noexcept;

// Writes the outline of `frame`, in the order above. Writes NOTHING when `points` does not have
// exactly stroke_outline_point_count(cap, cap_segments) points.
void write_stroke_outline(const stroke_frame &frame, glintfx::gltfx_line_cap cap,
                          std::uint32_t cap_segments,
                          std::span<pixel_position_wide> points) noexcept;

// The indices of the fan of a convex polygon of `point_count` points: 3 (point_count - 2), or none
// below three points.
[[nodiscard]] std::size_t fan_index_count(std::size_t point_count) noexcept;

// Writes the fan (0, 1, 2), (0, 2, 3), ... into all of `indices`, whose size is a
// fan_index_count().
void write_fan_indices(std::span<std::uint32_t> indices) noexcept;

} // namespace glintfx::draw2d
