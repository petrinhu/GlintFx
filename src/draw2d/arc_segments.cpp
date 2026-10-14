// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/arc_segments.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

// draw2d/arc_segments.cpp - R2D-SHAPES, fatia S3: the count of arc_segments.hpp.
namespace glintfx::draw2d {

// The two tests on `exact` come BEFORE the conversion: a NaN or an infinity never reaches a cast
// to an integer (undefined behaviour), and a count past 512 is the limit, not an overflow.
std::uint32_t arc_segment_count(double radius_pixels, double max_error_pixels) noexcept {
    if (!(radius_pixels > max_error_pixels)) {
        return k_min_arc_segments;
    }
    const double exact = std::numbers::pi / std::acos(1.0 - max_error_pixels / radius_pixels);
    if (!(exact < static_cast<double>(k_max_arc_segments))) {
        return k_max_arc_segments;
    }
    return std::max(k_min_arc_segments, static_cast<std::uint32_t>(std::ceil(exact)));
}

std::uint32_t half_arc_segment_count(double radius_pixels, double max_error_pixels) noexcept {
    return (arc_segment_count(radius_pixels, max_error_pixels) + 1U) / 2U;
}

namespace {

// The largest singular value of the 2x2 linear part, the largest stretch (S5-A-fix2, D-W8-434).
// The four entries are first divided by the power of two 2^e just above the largest of their sizes
// (only the exponent changes; an entry rounds only if it falls below the normal range on the way,
// and then it is under 2^-1021 of the largest and cannot change the answer), so the largest is in
// [0.5, 1) and no square below can overflow, nor lose the answer to underflow. Then the closed form
// of the 2x2 singular values: with the matrix [[a, b], [c, d]] (a = column0_x, b = column1_x, c =
// column0_y, d = column1_y), e = (a + d) / 2, f = (a - d) / 2, g = (c + b) / 2 and h = (c - b) / 2,
// the largest is sqrt(e^2 + h^2) + sqrt(f^2 + g^2): a sum of two sizes, with no subtraction that
// could cancel, multiplied back by 2^e. Only IEEE operations that round once (the square root
// included), and std::hypot is not one of them (its rounding is the library's own). The same bits
// on every system also need the compiler to keep each operation separate: a fused multiply-add
// rounds once where the code rounds twice. Every glintfx target is built with -ffp-contract=off
// under GCC and Clang, and MSVC's /fp:precise does not contract (cmake/GlintfxCompileOptions.cmake,
// S5-A-fix3, D-W8-441); fp_contract_test fails if a build fuses. One of e, f, g, h is at least
// 0.25, so a square that underflows below changes nothing. An entry that is not finite gives the
// sum of the sizes: infinite, or NaN when one of them is a NaN.
double largest_stretch(const pixel_affine &affine) noexcept {
    const double a = affine.column0_x;
    const double b = affine.column1_x;
    const double c = affine.column0_y;
    const double d = affine.column1_y;
    if (!std::isfinite(a) || !std::isfinite(b) || !std::isfinite(c) || !std::isfinite(d)) {
        return std::fabs(a) + std::fabs(b) + std::fabs(c) + std::fabs(d);
    }
    const double largest = std::max({std::fabs(a), std::fabs(b), std::fabs(c), std::fabs(d)});
    if (largest == 0.0) {
        return 0.0;
    }
    int exponent = 0;
    static_cast<void>(std::frexp(largest, &exponent));
    const double sa = std::ldexp(a, -exponent);
    const double sb = std::ldexp(b, -exponent);
    const double sc = std::ldexp(c, -exponent);
    const double sd = std::ldexp(d, -exponent);
    const double e = 0.5 * (sa + sd);
    const double f = 0.5 * (sa - sd);
    const double g = 0.5 * (sc + sb);
    const double h = 0.5 * (sc - sb);
    const double stretch = std::sqrt((e * e) + (h * h)) + std::sqrt((f * f) + (g * g));
    return std::ldexp(stretch, exponent);
}

} // namespace

// One path for every affine (D-W8-434): the stretch is right, or not finite, and never finite and
// wrong.
double screen_radius(double world_radius, const pixel_affine &affine) noexcept {
    return world_radius * largest_stretch(affine);
}

} // namespace glintfx::draw2d
