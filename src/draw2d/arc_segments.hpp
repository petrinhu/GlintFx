// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

#include "draw2d/quad_vertices.hpp"

// draw2d/arc_segments.hpp - R2D-SHAPES, fatia S3 (D-W8-269): how many straight segments stand in
// for an arc on the SCREEN, so the farthest point of the true curve is at most `max_error_pixels`
// away from them. Born for the rounded line cap (S3) and reused by the ellipse (S5). Pure: no GL,
// no window, allocates nothing.
//
// THE COUNT for a whole circle of radius r pixels and a largest error e pixels: n = ceil(pi /
// acos(1 - e / r)), the fewest n for which the sagitta r (1 - cos(pi / n)) is at most e, limited to
// [4, 512]. A radius not larger than the error (or one that is not a number) gives 4; a radius so
// large that n would pass 512 (or an infinite one) gives 512. The pair 4 and 512 is the one of Dear
// ImGui's automatic circle segments, learned, not copied (GODS_LAWS.md L-29).
//
// THE RADIUS ON THE SCREEN of a radius in the world is the world radius times the largest stretch
// of the linear part of the batch transform (its largest singular value): under a non-uniform scale
// the circle becomes an ellipse, and the count follows its longer axis. It is computed for every
// finite transform without an overflow or an underflow on the way, and without a subtraction that
// cancels (D-W8-434): the answer is the largest stretch within a few roundings, or infinite when
// that stretch itself passes the largest double; a NaN in the transform gives a NaN. It is never a
// finite number that is wrong.
namespace glintfx::draw2d {

inline constexpr std::uint32_t k_min_arc_segments = 4U;
inline constexpr std::uint32_t k_max_arc_segments = 512U;

// The segments of a whole circle.
[[nodiscard]] std::uint32_t arc_segment_count(double radius_pixels,
                                              double max_error_pixels) noexcept;

// The segments of a half circle (the rounded cap): half of the whole circle's count, rounded up, so
// the half keeps the whole one's largest error.
[[nodiscard]] std::uint32_t half_arc_segment_count(double radius_pixels,
                                                   double max_error_pixels) noexcept;

// `world_radius` as a radius on the screen, by the largest stretch of `affine`.
[[nodiscard]] double screen_radius(double world_radius, const pixel_affine &affine) noexcept;

} // namespace glintfx::draw2d
