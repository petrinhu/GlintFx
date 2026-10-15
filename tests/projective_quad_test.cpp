// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <print>

#include <glintfx/core/quad.hpp>

#include "draw2d/projective_quad.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// projective_quad_test.cpp - R2D-TEXTURE, fatia T4b-A (D-W8-13, D-W8-290, D-W8-455): the weights q
// of a textured quadrilateral (draw2d/projective_quad.hpp). The closed values are by hand (the
// square, a trapezoid whose diagonals cross at two thirds). The sweep checks the MAPPING, not the
// formula: for 3000 convex quadrilaterals, texture points (u, v) are carried to the screen by the
// square-to-quadrilateral homography in the closed form of P. Heckbert ("Fundamentals of Texture
// Mapping and Image Warping", 1989, section 2.2.3, written here from the paper's equations), and
// the interpolation the program does (the barycentric weights of the point in the triangle of the
// frozen diagonal that holds it, applied to (u q, v q, q), then divided by q) must give (u, v)
// back. No GL, no window.

using glintfx::gltfx_quad_world;
using glintfx::draw2d::projective_weights;

namespace {

[[nodiscard]] bool near(double got, double want, double tolerance) {
    return std::abs(got - want) <= tolerance;
}

// A linear congruential generator with a fixed seed, so every system draws the same quadrilaterals.
struct quad_draw {
    std::uint64_t state = 20261014U;
    [[nodiscard]] double next_unit() {
        state = (state * 6364136223846793005ULL) + 1442695040888963407ULL;
        return static_cast<double>(state >> 11U) * 0x1.0p-53;
    }
};

struct point {
    double x = 0.0;
    double y = 0.0;
};

// Heckbert's square-to-quadrilateral mapping: (0, 0), (1, 0), (1, 1), (0, 1) to the four corners.
struct homography {
    double a = 0.0;
    double b = 0.0;
    double c = 0.0;
    double d = 0.0;
    double e = 0.0;
    double f = 0.0;
    double g = 0.0;
    double h = 0.0;
};

[[nodiscard]] homography square_to(const gltfx_quad_world &q) {
    const double x0 = q.top_left.x;
    const double y0 = q.top_left.y;
    const double x1 = q.top_right.x;
    const double y1 = q.top_right.y;
    const double x2 = q.bottom_right.x;
    const double y2 = q.bottom_right.y;
    const double x3 = q.bottom_left.x;
    const double y3 = q.bottom_left.y;
    const double sx = x0 - x1 + x2 - x3;
    const double sy = y0 - y1 + y2 - y3;
    const double dx1 = x1 - x2;
    const double dx2 = x3 - x2;
    const double dy1 = y1 - y2;
    const double dy2 = y3 - y2;
    const double den = (dx1 * dy2) - (dx2 * dy1);
    const double g = ((sx * dy2) - (dx2 * sy)) / den;
    const double h = ((dx1 * sy) - (sx * dy1)) / den;
    return homography{x1 - x0 + (g * x1),
                      x3 - x0 + (h * x3),
                      x0,
                      y1 - y0 + (g * y1),
                      y3 - y0 + (h * y3),
                      y0,
                      g,
                      h};
}

[[nodiscard]] point map(const homography &m, double u, double v) {
    const double w = (m.g * u) + (m.h * v) + 1.0;
    return point{((m.a * u) + (m.b * v) + m.c) / w, ((m.d * u) + (m.e * v) + m.f) / w};
}

// The barycentric weights of p in the triangle (a, b, c).
[[nodiscard]] std::array<double, 3> barycentric(point p, point a, point b, point c) {
    const double den = ((b.y - c.y) * (a.x - c.x)) + ((c.x - b.x) * (a.y - c.y));
    const double wa = (((b.y - c.y) * (p.x - c.x)) + ((c.x - b.x) * (p.y - c.y))) / den;
    const double wb = (((c.y - a.y) * (p.x - c.x)) + ((a.x - c.x) * (p.y - c.y))) / den;
    return {wa, wb, 1.0 - wa - wb};
}

// What the program gives at p: the triangle of the frozen diagonal (0, 1, 2) or (0, 2, 3) holding
// p, the corners' (u q, v q, q) interpolated linearly, then divided by q.
[[nodiscard]] point interpolated(const gltfx_quad_world &q, const std::array<double, 4> &weight,
                                 point p) {
    const std::array<point, 4> corner{
        point{q.top_left.x, q.top_left.y}, point{q.top_right.x, q.top_right.y},
        point{q.bottom_right.x, q.bottom_right.y}, point{q.bottom_left.x, q.bottom_left.y}};
    const std::array<point, 4> uv{point{0.0, 0.0}, point{1.0, 0.0}, point{1.0, 1.0},
                                  point{0.0, 1.0}};
    std::array<double, 3> w = barycentric(p, corner[0], corner[1], corner[2]);
    std::array<std::size_t, 3> index{0U, 1U, 2U};
    if (w[1] < 0.0) {
        w = barycentric(p, corner[0], corner[2], corner[3]);
        index = {0U, 2U, 3U};
    }
    double uq = 0.0;
    double vq = 0.0;
    double sum_q = 0.0;
    for (std::size_t k = 0; k < 3U; ++k) {
        uq += w[k] * uv[index[k]].x * weight[index[k]];
        vq += w[k] * uv[index[k]].y * weight[index[k]];
        sum_q += w[k] * weight[index[k]];
    }
    return point{uq / sum_q, vq / sum_q};
}

// A convex quadrilateral: each corner jittered up to 30 inside its own corner of a square of side
// 100, which keeps it convex: along the segment between the two neighbours of a corner, x + y (or
// its mirror) is at least 100 - 30, while at the corner it is at most 2 x 30 = 60 < 70, so no
// corner reaches the line of its neighbours.
[[nodiscard]] gltfx_quad_world jittered_quad(quad_draw &draw) {
    const auto jitter = [&draw]() { return 30.0 * draw.next_unit(); };
    return gltfx_quad_world{{jitter(), jitter()},
                            {100.0 - jitter(), jitter()},
                            {100.0 - jitter(), 100.0 - jitter()},
                            {jitter(), 100.0 - jitter()}};
}

// Ten texture points carried to the screen by the homography and brought back by the
// interpolation; how many come back wrong.
[[nodiscard]] std::size_t wrong_points(const gltfx_quad_world &quad,
                                       const std::array<double, 4> &weight, quad_draw &draw) {
    const homography m = square_to(quad);
    std::size_t wrong = 0;
    for (std::size_t k = 0; k < 10U; ++k) {
        const double u = 0.02 + (0.96 * draw.next_unit());
        const double v = 0.02 + (0.96 * draw.next_unit());
        const point back = interpolated(quad, weight, map(m, u, v));
        wrong += (near(back.x, u, 1e-9) && near(back.y, v, 1e-9)) ? 0U : 1U;
    }
    return wrong;
}

// The quadrilateral with every coordinate multiplied by `scale`.
[[nodiscard]] gltfx_quad_world scaled(const gltfx_quad_world &q, double scale) {
    return gltfx_quad_world{{q.top_left.x * scale, q.top_left.y * scale},
                            {q.top_right.x * scale, q.top_right.y * scale},
                            {q.bottom_right.x * scale, q.bottom_right.y * scale},
                            {q.bottom_left.x * scale, q.bottom_left.y * scale}};
}

// Both have weights, and they agree to twelve digits.
[[nodiscard]] bool same_weights(const std::optional<std::array<double, 4>> &got,
                                const std::optional<std::array<double, 4>> &want) {
    if (!got.has_value() || !want.has_value()) {
        return false;
    }
    bool same = true;
    for (std::size_t i = 0; i < 4U; ++i) {
        same = same && near((*got)[i], (*want)[i], 1e-12 * (*want)[i]);
    }
    return same;
}

} // namespace

GLINTFX_TEST(projective_quad_closed_cases) {
    // A square (and any parallelogram): the diagonals cross at their middles, q = 1 everywhere.
    const auto square = projective_weights(gltfx_quad_world{{0, 0}, {10, 0}, {10, 10}, {0, 10}});
    GLINTFX_CHECK(square.has_value());
    if (square.has_value()) {
        for (const double q : *square) {
            GLINTFX_CHECK(near(q, 1.0, 1e-15));
        }
    }
    const auto sheared = projective_weights(gltfx_quad_world{{0, 0}, {10, 0}, {14, 5}, {4, 5}});
    GLINTFX_CHECK(sheared.has_value());
    if (sheared.has_value()) {
        for (const double q : *sheared) {
            GLINTFX_CHECK(near(q, 1.0, 1e-15));
        }
    }
    // The trapezoid (0, 0), (4, 0), (3, 2), (1, 2): the diagonals cross at two thirds of each, so
    // q = 1/(2/3), 1/(2/3), 1/(4/3), 1/(4/3) = 1.5, 1.5, 0.75, 0.75.
    const auto trapezoid = projective_weights(gltfx_quad_world{{0, 0}, {4, 0}, {3, 2}, {1, 2}});
    GLINTFX_CHECK(trapezoid.has_value());
    if (trapezoid.has_value()) {
        GLINTFX_CHECK(near((*trapezoid)[0], 1.5, 1e-15) && near((*trapezoid)[1], 1.5, 1e-15));
        GLINTFX_CHECK(near((*trapezoid)[2], 0.75, 1e-15) && near((*trapezoid)[3], 0.75, 1e-15));
    }
    // Far from the origin, the same trapezoid gives the same weights.
    const auto far = projective_weights(
        gltfx_quad_world{{1e9, 1e9}, {1e9 + 4, 1e9}, {1e9 + 3, 1e9 + 2}, {1e9 + 1, 1e9 + 2}});
    GLINTFX_CHECK(far.has_value() && near((*far)[0], 1.5, 1e-6) && near((*far)[2], 0.75, 1e-6));
}

GLINTFX_TEST(projective_quad_no_mapping_is_degenerate) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    std::size_t refused = 0;
    std::size_t counted = 0;
    const std::array<gltfx_quad_world, 10> degenerate{{
        {{0, 0}, {1, 0}, {2, 0}, {0, 1}},   // three corners on one line
        {{0, 0}, {1, 0}, {0, 1}, {1, 1}},   // two edges cross (a bow tie)
        {{0, 0}, {4, 0}, {1, 1}, {0, 4}},   // a corner pointing inward
        {{0, 0}, {4, 0}, {4, 4}, {3, 1}},   // corner 3 pointing inward: t = 2, s = 1/2
        {{3, 3}, {4, 0}, {4, 4}, {0, 4}},   // corner 0 pointing inward: s = -1 (T4b-A-fix1)
        {{0, 0}, {1, 3}, {4, 4}, {0, 4}},   // corner 1 pointing inward: t = -1 (T4b-A-fix1)
        {{3, 3}, {3, 3}, {3, 3}, {3, 3}},   // no area at all
        {{0, 0}, {2, 0}, {4, 0}, {6, 0}},   // all four on one line
        {{0, 0}, {nan, 0}, {1, 1}, {0, 1}}, // a value that is not a number
        {{0, 0},
         {2e-310, -1},
         {2, 0},
         {2e-310, 1}}, // corner 0 next to the crossing: 1/(2s) overflows
    }};
    for (const gltfx_quad_world &quad : degenerate) {
        ++counted;
        refused += projective_weights(quad).has_value() ? 0U : 1U;
    }
    std::println("projective_quad_test: degenerados casos={} recusados={}", counted, refused);
    GLINTFX_CHECK_EQ(counted, std::size_t{10});
    GLINTFX_CHECK_EQ(refused, std::size_t{10});
}

// 3000 convex quadrilaterals (jittered_quad), 10 texture points each: the interpolation gives the
// texture point back.
GLINTFX_TEST(projective_quad_sweep_maps_texture_points_back) {
    quad_draw draw;
    std::size_t counted = 0;
    std::size_t wrong = 0;
    for (std::size_t i = 0; i < 3000U; ++i) {
        const gltfx_quad_world quad = jittered_quad(draw);
        const std::optional<std::array<double, 4>> weight = projective_weights(quad);
        const std::size_t bad = weight.has_value() ? wrong_points(quad, *weight, draw) : 10U;
        counted += 10U;
        wrong += bad;
        if (bad != 0U && wrong <= 40U) {
            std::println("projective_quad_test: FALHOU o quadrilatero i={} ({} de 10)", i, bad);
        }
    }
    std::println("projective_quad_test: varredura pontos={} errados={}", counted, wrong);
    GLINTFX_CHECK_EQ(counted, std::size_t{30000});
    GLINTFX_CHECK_EQ(wrong, std::size_t{0});
}

// T4b-A-fix1 (D-W8-493, I-1 of the review): the weights do not depend on the size of the
// quadrilateral. The corners come in world units, and a camera zoom can bring a quadrilateral of
// 1e-170 or 1e160 to ordinary pixels; there the products of the coordinates would underflow to
// zero or overflow to infinity, and a convex quadrilateral would be refused. 500 convex
// quadrilaterals at nine scales from 1e-300 to 1e300 get the weights of the same quadrilateral at
// scale 1, to twelve digits.
GLINTFX_TEST(projective_quad_any_size_gets_the_same_weights) {
    constexpr std::array<double, 9> scales{1e-300, 1e-170, 1e-100, 1e-3, 1.0,
                                           1e6,    1e150,  1e160,  1e300};
    quad_draw draw;
    std::size_t counted = 0;
    std::size_t wrong = 0;
    for (std::size_t i = 0; i < 500U; ++i) {
        const gltfx_quad_world unit = jittered_quad(draw);
        const std::optional<std::array<double, 4>> reference = projective_weights(unit);
        for (const double scale : scales) {
            ++counted;
            wrong += same_weights(projective_weights(scaled(unit, scale)), reference) ? 0U : 1U;
        }
    }
    std::println("projective_quad_test: escalas quadrilateros={} errados={}", counted, wrong);
    GLINTFX_CHECK_EQ(counted, std::size_t{4500});
    GLINTFX_CHECK_EQ(wrong, std::size_t{0});
    // The closed trapezoid at the two scales of the review: 1.5, 1.5, 0.75, 0.75.
    const gltfx_quad_world trapezoid{{0, 0}, {4, 0}, {3, 2}, {1, 2}};
    const std::optional<std::array<double, 4>> want{{1.5, 1.5, 0.75, 0.75}};
    GLINTFX_CHECK(same_weights(projective_weights(scaled(trapezoid, 1e-170)), want));
    GLINTFX_CHECK(same_weights(projective_weights(scaled(trapezoid, 1e160)), want));
    // Corners near the largest double: their difference (3e308) is not a double, and the vectors
    // are still found, because each coordinate is halved before the subtraction.
    const gltfx_quad_world widest{
        {-1.5e308, -1.5e308}, {1.5e308, -1.5e308}, {1.5e308, 1.5e308}, {-1.5e308, 1.5e308}};
    const std::optional<std::array<double, 4>> ones{{1.0, 1.0, 1.0, 1.0}};
    GLINTFX_CHECK(same_weights(projective_weights(widest), ones));
}
