// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "draw2d/arc_parameters.hpp"

// harness/curve_oracle.hpp - fatia CURVE-FLOAT-NARROW-fix2 (GODS_LAWS.md L-33 global, the rule of
// three; the review of CURVE-FLOAT-NARROW, IMPORTANT-6): the oracle of the edge of a curve on the
// screen, read by the sweeps of arc_parameters_test, stroke_line_pixels_test and
// ellipse_pixels_test, which carried three copies of it that had begun to differ. Every bound here
// is the public text of fill_ellipse() in renderer_2d.hpp, written by hand, never read from the
// library's code; the curve comes from its definition, centre + u cos t + v sin t.
namespace glintfx::test_support {

using glintfx::draw2d::pixel_box;
using glintfx::draw2d::pixel_ellipse;
using glintfx::draw2d::pixel_position_wide;

// The unit roundoff of single precision (IEEE 754, round to nearest): a double rounded to the
// nearest float moves by at most this fraction of its magnitude, and by all of it at a tie.
inline constexpr double k_single_rounding = 0x1.0p-24;

// How far the ends of a segment on the surface may lie from the BAND, the surface grown by one
// pixel and the reach: 1024 pixels, or 652 times the distance when that is more.
inline double near_bound(double error) { return std::max(1024.0, 652.0 * error); }

// `box` grown by `grow` pixels on every side.
inline pixel_box grown_by(const pixel_box &box, double grow) {
    return pixel_box{box.min_x - grow, box.min_y - grow, box.max_x + grow, box.max_y + grow};
}

// How far `p` lies outside `box`, on the farther axis; 0 inside.
inline double outside_of(const pixel_box &box, const pixel_position_wide &p) {
    const double dx = std::max({box.min_x - p.x, p.x - box.max_x, 0.0});
    const double dy = std::max({box.min_y - p.y, p.y - box.max_y, 0.0});
    return std::max(dx, dy);
}

// The rounding the public text allows on `surface` for a distance `error` and a reach: 2^-24 of the
// distance from the origin of the farthest point the ends may reach, the band grown again by
// near_bound(error).
inline double allowed_rounding(const pixel_box &surface, double reach, double error) {
    const pixel_box far = grown_by(surface, 1.0 + reach + near_bound(error));
    const double x = std::max(std::fabs(far.min_x), std::fabs(far.max_x));
    const double y = std::max(std::fabs(far.min_y), std::fabs(far.max_y));
    return k_single_rounding * std::hypot(x, y);
}

// The point of the curve at t, in double: centre + u cos t + v sin t.
inline pixel_position_wide point_on(const pixel_ellipse &e, double t) {
    return pixel_position_wide{e.center.x + (e.axis_u.x * std::cos(t)) + (e.axis_v.x * std::sin(t)),
                               e.center.y + (e.axis_u.y * std::cos(t)) +
                                   (e.axis_v.y * std::sin(t))};
}

// True when the box of the hull of the piece from `from` to `to` (its two ends and the apex where
// their tangents meet) touches `box`. A sweep passes the band the library keeps (one pixel and the
// reach) when it checks the parameters, which are exact, and half a pixel when it checks float
// vertices, so that the oracle's own rounding never counts a piece the library drew coarse.
inline bool hull_touches(const pixel_ellipse &e, double from, double to, const pixel_box &box) {
    const double stretch = 1.0 / std::cos(0.5 * (to - from));
    const pixel_position_wide a = point_on(e, from);
    const pixel_position_wide b = point_on(e, to);
    const pixel_position_wide m = point_on(e, 0.5 * (from + to));
    const pixel_position_wide apex{e.center.x + ((m.x - e.center.x) * stretch),
                                   e.center.y + ((m.y - e.center.y) * stretch)};
    const auto [low_x, high_x] = std::minmax({a.x, b.x, apex.x});
    const auto [low_y, high_y] = std::minmax({a.y, b.y, apex.y});
    return !(high_x < box.min_x || low_x > box.max_x || high_y < box.min_y || low_y > box.max_y);
}

using offset_wide = std::array<long double, 2>;

// The offset of the curve from its centre at t, in long double: u cos t + v sin t.
inline offset_wide offset_on(const pixel_ellipse &e, long double t) {
    return offset_wide{(static_cast<long double>(e.axis_u.x) * std::cos(t)) +
                           (static_cast<long double>(e.axis_v.x) * std::sin(t)),
                       (static_cast<long double>(e.axis_u.y) * std::cos(t)) +
                           (static_cast<long double>(e.axis_v.y) * std::sin(t))};
}

// A chord against the curve: the parameters of its two ends, and the two ends as offsets from the
// centre (the curve's own points for a cut, the float vertices for a drawing).
struct chord_on_curve {
    long double from = 0.0L;
    long double to = 0.0L;
    offset_wide a{};
    offset_wide b{};
};

// The largest distance, in long double, from 15 points of the curve between the two parameters
// (the middle among them) to the line through the two ends: the geometric error of the chord,
// never the sagitta formula the library uses to choose a level.
inline double largest_distance_to_chord(const pixel_ellipse &e, const chord_on_curve &chord) {
    const long double cx = chord.b[0] - chord.a[0];
    const long double cy = chord.b[1] - chord.a[1];
    const long double length = std::hypot(cx, cy);
    long double largest = 0.0L;
    for (int i = 1; i < 16; ++i) {
        const long double t =
            chord.from + ((chord.to - chord.from) * static_cast<long double>(i) / 16.0L);
        const offset_wide p = offset_on(e, t);
        const long double px = p[0] - chord.a[0];
        const long double py = p[1] - chord.a[1];
        const long double d =
            length > 0.0L ? std::fabs((cx * py) - (cy * px)) / length : std::hypot(px, py);
        largest = std::max(largest, d);
    }
    return static_cast<double>(largest);
}

// The larger radius of the curve: the larger semi-axis of the ellipse u and v span (they need not
// be at right angles), by the formula of the conjugate diameters (Apollonius), in long double.
inline double larger_radius(const pixel_ellipse &e) {
    const long double ux = e.axis_u.x;
    const long double uy = e.axis_u.y;
    const long double vx = e.axis_v.x;
    const long double vy = e.axis_v.y;
    const long double half_sum = 0.5L * ((ux * ux) + (uy * uy) + (vx * vx) + (vy * vy));
    const long double half_difference = 0.5L * ((ux * ux) + (uy * uy) - (vx * vx) - (vy * vy));
    const long double dot = (ux * vx) + (uy * vy);
    return static_cast<double>(
        std::sqrt(half_sum + std::sqrt((half_difference * half_difference) + (dot * dot))));
}

} // namespace glintfx::test_support
