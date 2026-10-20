// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/projective_quad.hpp"

#include <array>
#include <cmath>
#include <optional>

#include "draw2d/exact_cross.hpp"

// draw2d/projective_quad.cpp - R2D-TEXTURE, fatias T4b-A to T4b-A-fix4: the weights of
// projective_quad.hpp.
namespace glintfx::draw2d {

namespace {

// The five cross products of the solution, each straight from the corners and exact
// (exact_cross.hpp; T4b-A-fix4, D-W8-551, D-W8-552): the diagonals' D = d1 x d2, and s, 1 - s, t
// and 1 - t times D. 1 - s and 1 - t are cross products of their own, never a subtraction from 1,
// so they keep their digits near 1. With corner0 + s d1 = corner1 + t d2 and r = corner1 - corner0:
// s D = r x d2, (1 - s) D = (corner2 - corner1) x d2, t D = r x d1, (1 - t) D = d1 x (corner3 -
// corner0).
struct quad_crosses {
    exact_cross_value diagonals;
    exact_cross_value s;
    exact_cross_value one_minus_s;
    exact_cross_value t;
    exact_cross_value one_minus_t;
};

[[nodiscard]] quad_crosses crosses_of(const glintfx::gltfx_quad_world &c) noexcept {
    return quad_crosses{
        exact_cross(c.top_left, c.bottom_right, c.top_right, c.bottom_left),
        exact_cross(c.top_left, c.top_right, c.top_right, c.bottom_left),
        exact_cross(c.top_right, c.bottom_right, c.top_right, c.bottom_left),
        exact_cross(c.top_left, c.top_right, c.top_left, c.bottom_right),
        exact_cross(c.top_left, c.bottom_right, c.top_left, c.bottom_left),
    };
}

[[nodiscard]] bool all_finite(const glintfx::gltfx_quad_world &c) noexcept {
    const std::array<double, 8> values{c.top_left.x,    c.top_left.y,     c.top_right.x,
                                       c.top_right.y,   c.bottom_right.x, c.bottom_right.y,
                                       c.bottom_left.x, c.bottom_left.y};
    for (const double value : values) {
        if (!std::isfinite(value)) {
            return false;
        }
    }
    return true;
}

// Strictly inside both diagonals: D is not zero, and s, 1 - s, t and 1 - t have its sign. The
// signs are exact, so this is decided exactly.
[[nodiscard]] bool crossing_inside(const quad_crosses &q) noexcept {
    const int sign = q.diagonals.sign;
    return sign != 0 && q.s.sign == sign && q.one_minus_s.sign == sign && q.t.sign == sign &&
           q.one_minus_t.sign == sign;
}

// 1 / (2 x) = D / (2 x D) as a double; infinite when it is not one.
[[nodiscard]] double weight_of(const exact_cross_value &diagonals,
                               const exact_cross_value &x_times_d) noexcept {
    return std::ldexp(0.5 * diagonals.m / x_times_d.m, diagonals.e - x_times_d.e);
}

} // namespace

std::optional<std::array<double, 4>>
projective_weights(const glintfx::gltfx_quad_world &corners) noexcept {
    if (!all_finite(corners)) {
        return std::nullopt;
    }
    const quad_crosses q = crosses_of(corners);
    if (!crossing_inside(q)) {
        return std::nullopt;
    }
    // q0 = 1 / (2 (1 - s)), q1 = 1 / (2 (1 - t)), q2 = 1 / (2 s), q3 = 1 / (2 t).
    const std::array<double, 4> weights{weight_of(q.diagonals, q.one_minus_s),
                                        weight_of(q.diagonals, q.one_minus_t),
                                        weight_of(q.diagonals, q.s), weight_of(q.diagonals, q.t)};
    for (const double weight : weights) {
        if (!std::isfinite(weight)) {
            return std::nullopt;
        }
    }
    return weights;
}

} // namespace glintfx::draw2d
