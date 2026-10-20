// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "draw2d/arc_parameters.hpp"
#include "draw2d/ellipse_frame.hpp"

// draw2d/ellipse_outline.hpp - R2D-SHAPES, fatia S5-B (D-W8-412 and D-W8-416): the outline of a
// THICK ellipse, the band of every point at most h (half the thickness) from the curve, in the
// plane of ellipse_frame.hpp. Pure: no GL, no window, allocates nothing, refuses nothing.
//
// THE POINTS are named by the angle phi of the outward normal (ellipse_frame.hpp). Each phi gives
// an OUTER point, the curve's point pushed out by h along its normal, which lies on the outer edge
// of the band for every h; and, when the band has a hole, an INNER point. The angles are the UNION
// of two sets (D-W8-416): the arc's own parameters (arc_parameters.hpp, which keep the curve itself
// within its error and cut what the surface does not show), turned into normal angles; and phi =
// 2 pi j / m, m from uniform_normal_angle_count(), which keep within the error what the thickness
// adds, h (1 - cos(delta phi)). m is a multiple of 4, so the four ends of the axes are always
// points.
//
// THE HOLE is the erosion of the ellipse by h, exactly, in the principal axes (major a, minor b,
// D-W8-412):
//   - h >= b: no hole. The band is the whole outer polygon, filled as a fan from the centre.
//   - h <= b^2 / a (the smallest radius of curvature): the inner point is the curve's point pushed
//     IN by h along its normal.
//   - b^2 / a < h < b: pushed in by h, the inner curve folds over itself near the two ends of the
//     major axis. The edge of the hole there is the point X = (+-x*, 0) where the two branches
//     cross, x* = a cos t* (1 - b^2/a^2) with sin^2 t* = (h^2 a^2 / b^2 - b^2) / (a^2 - b^2). An
//     inner point that has crossed the major axis (its y no longer on the side of its normal) is X,
//     on the side its normal faces.
// A radius of zero is not special (D-W8-414): with both radii zero there is no hole and the outer
// points draw the disc of radius h; with one zero, the bar with rounded ends.
namespace glintfx::draw2d {

inline constexpr std::size_t k_max_uniform_normal_angles = 1024U;
inline constexpr std::size_t k_max_outline_angles =
    (k_max_arc_parameters - 1U) + k_max_uniform_normal_angles;

// m, the uniform normal angles of an outline whose half thickness is `reach_pixels` on the screen,
// for the largest error `max_error_pixels` of the whole outline: the fewest that keep
// reach (1 - cos(2 pi / m)) within half of it, a multiple of 4, from 8 to 1024.
[[nodiscard]] std::size_t uniform_normal_angle_count(double reach_pixels,
                                                     double max_error_pixels) noexcept;

// Where the angles come from: the arc's parameters (t_0 to t_(k-1), the closing one left out) and
// the count of uniform angles.
struct outline_angle_sources {
    std::span<const double> parameters{};
    std::size_t uniform_count = 0;
};

// MERGES into `out` the normal angles of both `sources` - the arc's (one per parameter) and the
// uniform ones - each in [0, 2 pi), in increasing order, every value once. The result is the count
// of angles written; 0, writing nothing, when `out` holds fewer than the parameters and the
// uniform angles together.
// `plane` and `principal` are the same ellipse. Three atoms do the work (S5-B-fix2, D-W8-535): the
// arc's angles, the uniform angles, and the sort that drops repeated values.
[[nodiscard]] std::size_t merge_outline_angles(const conjugate_ellipse &plane,
                                               const principal_ellipse &principal,
                                               const outline_angle_sources &sources,
                                               std::span<double> out) noexcept;

enum class outline_hole : std::uint8_t {
    none,  // h >= b: a fan
    plain, // the inner points are the curve pushed in
    cut,   // the inner points that crossed the major axis are X
};

struct outline_shape {
    outline_hole hole = outline_hole::none;
    double cut_x = 0.0; // x* of X, in the principal axes; 0 unless `cut`
};

// The hole of the band of half thickness `half_thickness` (zero or more) around `principal`.
[[nodiscard]] outline_shape outline_shape_of(const principal_ellipse &principal,
                                             double half_thickness) noexcept;

// The outer and the inner point of normal angle phi, in the plane. The inner one is meaningful
// only when the shape has a hole.
struct outline_pair {
    plane_point outer{};
    plane_point inner{};
};

// The two points of `phi` for the band of `half_thickness` (with `shape` its outline_shape_of()).
struct outline_band {
    double half_thickness = 0.0;
    outline_shape shape{};
};

[[nodiscard]] outline_pair outline_pair_at(const principal_ellipse &principal,
                                           const outline_band &band, double phi) noexcept;

} // namespace glintfx::draw2d
