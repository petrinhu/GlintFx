// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/projective_quad.hpp"

#include <array>
#include <cmath>
#include <optional>

// draw2d/projective_quad.cpp - R2D-TEXTURE, fatia T4b-A: the weights of projective_quad.hpp.
namespace glintfx::draw2d {

namespace {

// The cross product of (ax, ay) and (bx, by).
[[nodiscard]] double cross(double ax, double ay, double bx, double by) noexcept {
    return (ax * by) - (ay * bx);
}

} // namespace

std::optional<std::array<double, 4>>
projective_weights(const glintfx::gltfx_quad_world &corners) noexcept {
    // Everything measured from corner 0, so a quadrilateral far from the origin keeps its digits.
    const double d1x = corners.bottom_right.x - corners.top_left.x; // the diagonal 0 -> 2
    const double d1y = corners.bottom_right.y - corners.top_left.y;
    const double d2x = corners.bottom_left.x - corners.top_right.x; // the diagonal 1 -> 3
    const double d2y = corners.bottom_left.y - corners.top_right.y;
    const double rx = corners.top_right.x - corners.top_left.x; // from corner 0 to corner 1
    const double ry = corners.top_right.y - corners.top_left.y;
    const double denominator = cross(d1x, d1y, d2x, d2y);
    // corner0 + s d1 = corner1 + t d2, solved by Cramer's rule.
    const double s = cross(rx, ry, d2x, d2y) / denominator;
    const double t = cross(rx, ry, d1x, d1y) / denominator;
    // Strictly inside both diagonals; a NaN (parallel diagonals, a value that is not finite) fails
    // every comparison.
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
