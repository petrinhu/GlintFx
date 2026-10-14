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

// The largest singular value of the 2x2 linear part. With `squares` the sum of the four squared
// entries and `determinant` the determinant, s1^2 = (squares + spread) / 2 where spread is
// sqrt(squares^2 - 4 determinant^2); a negative under the root (rounding) is clamped to zero.
// Entries past about 1e77 overflow the squares, and then the answer is not finite.
double largest_stretch(const pixel_affine &affine) noexcept {
    const double squares =
        (affine.column0_x * affine.column0_x) + (affine.column0_y * affine.column0_y) +
        (affine.column1_x * affine.column1_x) + (affine.column1_y * affine.column1_y);
    const double determinant =
        (affine.column0_x * affine.column1_y) - (affine.column1_x * affine.column0_y);
    const double spread =
        std::sqrt(std::max(0.0, (squares * squares) - (4.0 * determinant * determinant)));
    return std::sqrt((squares + spread) / 2.0);
}

// The same stretch with the four entries divided by the largest of their sizes first, and the
// answer multiplied back (D-W8-399): no square overflows, so every stretch up to the largest double
// comes out finite. Only for when the direct one overflows, so the common case keeps its bits.
double largest_stretch_scaled(const pixel_affine &affine) noexcept {
    const double largest = std::max({std::fabs(affine.column0_x), std::fabs(affine.column0_y),
                                     std::fabs(affine.column1_x), std::fabs(affine.column1_y)});
    if (!(largest > 0.0)) {
        return largest_stretch(affine);
    }
    const pixel_affine scaled{affine.column0_x / largest,
                              affine.column0_y / largest,
                              affine.column1_x / largest,
                              affine.column1_y / largest,
                              0.0,
                              0.0};
    return largest * largest_stretch(scaled);
}

} // namespace

double screen_radius(double world_radius, const pixel_affine &affine) noexcept {
    const double direct = largest_stretch(affine);
    if (std::isfinite(direct)) {
        return world_radius * direct;
    }
    return world_radius * largest_stretch_scaled(affine);
}

} // namespace glintfx::draw2d
