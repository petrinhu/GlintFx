// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/projective_quad.hpp"

#include <array>
#include <cmath>
#include <limits>
#include <optional>

// draw2d/projective_quad.cpp - R2D-TEXTURE, fatias T4b-A, T4b-A-fix1 and T4b-A-fix2: the weights of
// projective_quad.hpp.
namespace glintfx::draw2d {

namespace {

// The cross product of (ax, ay) and (bx, by).
[[nodiscard]] double cross(double ax, double ay, double bx, double by) noexcept {
    return (ax * by) - (ay * bx);
}

// The three vectors the solution needs: the diagonal 0 -> 2, the diagonal 1 -> 3 and the edge
// 0 -> 1.
struct quad_vectors {
    double d1x = 0.0;
    double d1y = 0.0;
    double d2x = 0.0;
    double d2y = 0.0;
    double rx = 0.0;
    double ry = 0.0;
};

// Whether a difference of two of the coordinates could overflow: some coordinate is above a
// quarter of the largest double (T4b-A-fix2, D-W8-514).
[[nodiscard]] bool near_the_largest_double(const glintfx::gltfx_quad_world &corners) noexcept {
    constexpr double k_quarter_of_max = std::numeric_limits<double>::max() / 4.0;
    const std::array<double, 8> values{corners.top_left.x,     corners.top_left.y,
                                       corners.top_right.x,    corners.top_right.y,
                                       corners.bottom_right.x, corners.bottom_right.y,
                                       corners.bottom_left.x,  corners.bottom_left.y};
    for (const double value : values) {
        if (std::fabs(value) > k_quarter_of_max) {
            return true;
        }
    }
    return false;
}

// The vectors, scaled together so the largest of their six components lies in [1, 2) (T4b-A-fix1,
// D-W8-493). s and t are RATIOS of the products below, so a common factor changes none of their
// digits, and the products neither overflow nor vanish whatever the size of the quadrilateral.
// Near the largest double each coordinate is halved BEFORE the subtraction, so the difference of
// two finite values stays finite; there halving is exact. Anywhere else nothing is halved
// (T4b-A-fix2, D-W8-514): halving a SUBNORMAL coordinate rounds away its last bit, and a
// quadrilateral of a few multiples of the smallest double changed shape (measured: 1696 of 200000
// accepted wrongly, 2759 refused wrongly). The subtraction of two subnormals is exact, and so is
// the power of two. No value when a component is not finite or all of them are zero (the four
// corners are one point).
[[nodiscard]] std::optional<quad_vectors>
scaled_vectors(const glintfx::gltfx_quad_world &corners) noexcept {
    const int halving = near_the_largest_double(corners) ? -1 : 0;
    const auto half = [halving](double value) noexcept { return std::ldexp(value, halving); };
    std::array<double, 6> v{half(corners.bottom_right.x) - half(corners.top_left.x),
                            half(corners.bottom_right.y) - half(corners.top_left.y),
                            half(corners.bottom_left.x) - half(corners.top_right.x),
                            half(corners.bottom_left.y) - half(corners.top_right.y),
                            half(corners.top_right.x) - half(corners.top_left.x),
                            half(corners.top_right.y) - half(corners.top_left.y)};
    double largest = 0.0;
    for (const double component : v) {
        if (!std::isfinite(component)) {
            return std::nullopt;
        }
        largest = std::fmax(largest, std::fabs(component));
    }
    if (largest == 0.0) {
        return std::nullopt;
    }
    const int exponent = std::ilogb(largest);
    for (double &component : v) {
        component = std::ldexp(component, -exponent);
    }
    return quad_vectors{v[0], v[1], v[2], v[3], v[4], v[5]};
}

} // namespace

std::optional<std::array<double, 4>>
projective_weights(const glintfx::gltfx_quad_world &corners) noexcept {
    const std::optional<quad_vectors> vectors = scaled_vectors(corners);
    if (!vectors.has_value()) {
        return std::nullopt;
    }
    const quad_vectors &q = *vectors;
    const double denominator = cross(q.d1x, q.d1y, q.d2x, q.d2y);
    // corner0 + s d1 = corner1 + t d2, solved by Cramer's rule.
    const double s = cross(q.rx, q.ry, q.d2x, q.d2y) / denominator;
    const double t = cross(q.rx, q.ry, q.d1x, q.d1y) / denominator;
    // Strictly inside both diagonals; a NaN (parallel diagonals) fails every comparison.
    if (!(s > 0.0 && s < 1.0 && t > 0.0 && t < 1.0)) {
        return std::nullopt;
    }
    const std::array<double, 4> weights{0.5 / (1.0 - s), 0.5 / (1.0 - t), 0.5 / s, 0.5 / t};
    for (const double weight : weights) {
        if (!std::isfinite(weight)) {
            return std::nullopt;
        }
    }
    return weights;
}

} // namespace glintfx::draw2d
