// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "draw2d/quad_vertices.hpp"

// draw2d/arc_parameters.hpp - R2D-SHAPES, fatia S5-A (TODO.md ARC-VIEWPORT-TESSELLATION; decisions
// D-W8-269, D-W8-377 and D-W8-378): WHERE the straight segments that stand in for an arc on the
// SCREEN begin and end, as values of the arc's parameter t. Pure: no GL, no window, allocates
// nothing. Born for the ellipse of fatia S5; the rounded line cap moved onto it in fatia S5-C2
// (stroke_line_pixels.hpp).
//
// THE ARC is the screen ellipse p(t) = center + axis_u cos t + axis_v sin t, in pixels, for t from
// 0 to 2 pi (the whole curve) or from 0 to pi (half of it). axis_u and axis_v are OFFSETS from the
// center: the images on the screen of the two radii, so a world ellipse under any transform of the
// house is one of these. r is the largest radius on the screen: the largest singular value of the
// two axes, screen_radius() of arc_segments.hpp applied to them.
//
// TWO REGIMES, chosen by r and the largest error e:
//   - UNIFORM, when pi / acos(1 - e / r) <= 512, or r <= e: the n equal steps of before this fatia,
//     n = arc_segment_count(r, e) for the whole curve and half_arc_segment_count(r, e) for the
//     half, t_k = pi * (h * k) / n with h = 2 for the whole curve and 1 for the half. Nothing is
//     culled.
//   - CUT, above that (D-W8-377; the technique of Skia's PreChopPathCurves, learned and not copied,
//     GODS_LAWS.md L-29): the arc is cut in quarters, t = 0, pi/2, pi, ..., and each piece in two
//     halves again, depth first and in the order of t. A piece stops being cut when its HULL misses
//     the surface, or when it is as fine as e needs. The hull of the piece from ta to tb is the
//     triangle of p(ta), p(tb) and the apex where their tangents meet,
//     center + (axis_u cos tm + axis_v sin tm) / cos(w), tm its middle and w half its angle: the
//     arc lies inside it, so a piece whose hull misses the surface is drawn as its chord and no
//     pixel on the surface changes. "Misses" is: the box of those three points does not touch the
//     surface grown on every side by k_arc_surface_margin_pixels plus the request's reach_pixels (a
//     stroke's half width, so the band around the chord stays off the surface too). "As fine as e
//     needs" is level j, the first from 8 up to k_max_arc_cut_level whose pieces, (pi/2) / 2^j, are
//     no wider than 2 acos(1 - e / r), the widest angle whose chord stays within e of a circle of
//     radius r. Every parameter of this regime is t = (pi/2) * (N / 2^level), N an integer: the
//     division is exact, and there is one rounding.
//
// THE GUARANTEE HAS TWO CONDITIONS, both declared (D-W8-398): where the curve is on the surface, it
// is at most e from its segments (1) while the level e asks for fits the room, and (2) while r is
// at most k_largest_exact_arc_radius_pixels (2^40, D-W8-378).
//   (1) The room is at most min(out.size(), k_max_arc_parameters) parameters (1024 segments) per
//       call. When the level e asks for would give more, the call takes the deepest level, down to
//       7, that fits, and the error on the surface then passes e: with e = 1e-9 and r = 1e6 it is
//       about 1100 times e. A tolerance far below what a screen needs, or a surface much larger
//       than 16384 pixels on a side, comes to that; the two sweeps of arc_parameters_test, up to
//       that size, never do: the whole curve at the default 0.25 pixel, and the half curve of a
//       rounded cap at errors from 1/16 to 8 pixels (fatia S5-C2-fix1).
//   (2) Above 2^40, the rounding of the double arithmetic, and not the count, sets the error.
namespace glintfx::draw2d {

inline constexpr std::size_t k_max_arc_parameters = 1025U;
inline constexpr std::size_t k_fewest_whole_arc_parameters = 513U;
inline constexpr std::size_t k_fewest_half_arc_parameters = 257U;
inline constexpr std::uint32_t k_max_arc_cut_level = 40U;
inline constexpr double k_arc_surface_margin_pixels = 1.0;
inline constexpr double k_largest_exact_arc_radius_pixels = 1099511627776.0; // 2^40

enum class arc_extent : std::uint8_t {
    whole, // t from 0 to 2 pi
    half,  // t from 0 to pi
};

// The ellipse on the screen, in pixels. axis_u and axis_v are OFFSETS from center, held in the
// same two-double form as a position.
struct pixel_ellipse {
    pixel_position_wide center{};
    pixel_position_wide axis_u{};
    pixel_position_wide axis_v{};
};

// What to cut. max_error_pixels: the largest distance between the true curve and its segments,
// finite and greater than zero. reach_pixels: how far the drawing goes beyond the curve on the
// screen, 0 for a fill and half the width for a stroke, finite and zero or more.
struct arc_request {
    pixel_ellipse ellipse{};
    arc_extent extent = arc_extent::whole;
    double max_error_pixels = 0.25;
    double reach_pixels = 0.0;
};

// The surface, in pixels: x from min_x to max_x, y from min_y to max_y.
struct pixel_box {
    double min_x = 0.0;
    double min_y = 0.0;
    double max_x = 0.0;
    double max_y = 0.0;
};

// The screen of one call that draws a curve: the batch transform, the surface in pixels, and the
// largest error (gltfx_renderer_2d_desc::curve_max_error_pixels; open() refuses it unless it is
// finite and above zero). Read by the ellipse (ellipse_pixels.hpp, since fatia S5-C1) and by the
// rounded cap of a line, a half ellipse on the screen (stroke_line_pixels.hpp, since fatia S5-C2).
struct ellipse_screen {
    pixel_affine affine{};
    pixel_box surface{};
    double max_error_pixels = 0.25;
};

// The fewest parameters `out` must hold for `extent`: one more than the most segments of the
// uniform regime, k_fewest_whole_arc_parameters or k_fewest_half_arc_parameters; 0 for a value that
// is not one of the two.
[[nodiscard]] std::size_t fewest_arc_parameters(arc_extent extent) noexcept;

// Writes t_0 = 0 < t_1 < ... < t_m into out, both ends included (t_m is 2 pi or pi), and answers
// m + 1. Answers 0, writing nothing, when out holds fewer than fewest_arc_parameters(extent), when
// the extent is not one of the two, when a value of the request or of the surface is not finite,
// when max_error_pixels is not greater than zero, when reach_pixels is negative, or when r is not
// finite. r is computed without overflow on the way (D-W8-399): every finite radius up to the
// largest double is cut, and only a stretch past the largest double answers 0.
[[nodiscard]] std::size_t arc_parameters(const arc_request &request, const pixel_box &surface,
                                         std::span<double> out) noexcept;

} // namespace glintfx::draw2d
