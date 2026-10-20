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

// projective_quad_test.cpp - R2D-TEXTURE, fatias T4b-A to T4b-A-fix6 (D-W8-13, D-W8-290,
// D-W8-455, D-W8-707): the weights q of a textured quadrilateral (draw2d/projective_quad.hpp), one
// subject, in the order of the cases below:
// - the closed values by hand (the square, a trapezoid whose diagonals cross at two thirds), and
//   the quadrilaterals with no mapping, refused;
// - the MAPPING, not the formula: for 3000 convex quadrilaterals, texture points (u, v) are carried
//   to the screen by the square-to-quadrilateral homography in the closed form of P. Heckbert
//   ("Fundamentals of Texture Mapping and Image Warping", 1989, section 2.2.3, written here from
//   the paper's equations), and the interpolation the program does (the barycentric weights of
//   the point in the triangle of the frozen diagonal that holds it, applied to (u q, v q, q), then
//   divided by q) must give (u, v) back; and 500 convex quadrilaterals keep their weights at nine
//   scales from 1e-300 to 1e300;
// - the exact decision: corners that are whole numbers, or whole multiples of the smallest double,
//   against the answer computed in integers (exact_weights);
// - corners of mixed magnitude in one quadrilateral, and the cases kept from the reviews (1 - s
//   near 1 among them);
// - the promise of 2^-50 on each weight, against the definition.
// No GL, no window.

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

// T4b-A-fix2 (D-W8-515): corners that are whole multiples of the smallest double are exact in
// double, so a quadrilateral of them has an EXACT answer in integers: accepted when the diagonals
// cross strictly inside both (0 < s < 1, 0 < t < 1 as fractions of integers), with
// q = 1 / (2 (1 - s)) and the rest computed from those fractions.
struct exact_answer {
    bool accepted = false;
    std::array<double, 4> weights{};
};

[[nodiscard]] std::int64_t cross_int(std::int64_t ax, std::int64_t ay, std::int64_t bx,
                                     std::int64_t by) {
    return (ax * by) - (ay * bx);
}

[[nodiscard]] exact_answer exact_weights(const std::array<std::array<std::int64_t, 2>, 4> &c) {
    const std::int64_t d1x = c[2][0] - c[0][0];
    const std::int64_t d1y = c[2][1] - c[0][1];
    const std::int64_t d2x = c[3][0] - c[1][0];
    const std::int64_t d2y = c[3][1] - c[1][1];
    std::int64_t den = cross_int(d1x, d1y, d2x, d2y);
    std::int64_t ns = cross_int(c[1][0] - c[0][0], c[1][1] - c[0][1], d2x, d2y);
    std::int64_t nt = cross_int(c[1][0] - c[0][0], c[1][1] - c[0][1], d1x, d1y);
    if (den < 0) {
        den = -den;
        ns = -ns;
        nt = -nt;
    }
    if (!(den > 0 && ns > 0 && ns < den && nt > 0 && nt < den)) {
        return exact_answer{};
    }
    const double s = static_cast<double>(ns) / static_cast<double>(den);
    const double t = static_cast<double>(nt) / static_cast<double>(den);
    return exact_answer{true, {0.5 / (1.0 - s), 0.5 / (1.0 - t), 0.5 / s, 0.5 / t}};
}

// T4b-A-fix3 (D-W8-542): the corners of quadrilateral number `code` of the 5^8 whose eight
// coordinates are the whole numbers 0 to 4, one base-5 digit each.
[[nodiscard]] std::array<std::array<std::int64_t, 2>, 4> corners_of_code(std::int64_t code) {
    std::array<std::array<std::int64_t, 2>, 4> c{};
    for (auto &corner : c) {
        for (std::int64_t &coordinate : corner) {
            coordinate = code % 5;
            code /= 5;
        }
    }
    return c;
}

// The quadrilateral of whole-number corners, each coordinate times `unit` (exact in double for
// the units of the tests: 1 and the smallest double).
[[nodiscard]] gltfx_quad_world quad_of_integers(const std::array<std::array<std::int64_t, 2>, 4> &c,
                                                double unit) {
    const auto at = [&](std::size_t i, std::size_t axis) {
        return static_cast<double>(c[i][axis]) * unit;
    };
    return gltfx_quad_world{
        {at(0, 0), at(0, 1)}, {at(1, 0), at(1, 1)}, {at(2, 0), at(2, 1)}, {at(3, 0), at(3, 1)}};
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

// The promise of projective_quad.hpp (D-W8-553): each weight within 2^-50 of the exact one. `want`
// is the exact weight rounded once (within 2^-53 of it), so the check leaves that rounding out:
// |got - want| <= (2^-50 - 2^-53) want = 7 x 2^-53 want (T4b-A-fix5, D-W8-599). A weight that
// leaves the promise by more than 2^-53 fails it.
[[nodiscard]] bool within_promise(const std::optional<std::array<double, 4>> &got,
                                  const std::array<double, 4> &want) {
    if (!got.has_value()) {
        return false;
    }
    bool within = true;
    for (std::size_t i = 0; i < 4U; ++i) {
        within = within && std::abs((*got)[i] - want[i]) <= 7.0 * 0x1p-53 * want[i];
    }
    return within;
}

// The quadrilaterals with no mapping: the shapes of projective_quad.hpp, a value that is not a
// number, and (T4b-A-fix5, D-W8-601) plus and minus infinity in each of the four corners of a
// square that has a mapping.
[[nodiscard]] std::array<gltfx_quad_world, 18> degenerate_quads() {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    return {{
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
         {2e-310, 1}},                       // corner 0 next to the crossing: 1/(2s) overflows
        {{inf, 0}, {1, 0}, {1, 1}, {0, 1}},  // plus infinity in corner 0
        {{0, 0}, {inf, 0}, {1, 1}, {0, 1}},  // ... in corner 1
        {{0, 0}, {1, 0}, {inf, 1}, {0, 1}},  // ... in corner 2
        {{0, 0}, {1, 0}, {1, 1}, {inf, 1}},  // ... in corner 3
        {{0, -inf}, {1, 0}, {1, 1}, {0, 1}}, // minus infinity in corner 0
        {{0, 0}, {1, -inf}, {1, 1}, {0, 1}}, // ... in corner 1
        {{0, 0}, {1, 0}, {1, -inf}, {0, 1}}, // ... in corner 2
        {{0, 0}, {1, 0}, {1, 1}, {0, -inf}}, // ... in corner 3
    }};
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
    std::size_t refused = 0;
    std::size_t counted = 0;
    for (const gltfx_quad_world &quad : degenerate_quads()) {
        ++counted;
        refused += projective_weights(quad).has_value() ? 0U : 1U;
    }
    std::println("projective_quad_test: degenerados casos={} recusados={}", counted, refused);
    GLINTFX_CHECK_EQ(counted, std::size_t{18});
    GLINTFX_CHECK_EQ(refused, std::size_t{18});
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
    // are still found, because every cross product is computed exactly, in whole numbers wide
    // enough for any double (exact_cross.hpp; T4b-A-fix4, D-W8-551).
    const gltfx_quad_world widest{
        {-1.5e308, -1.5e308}, {1.5e308, -1.5e308}, {1.5e308, 1.5e308}, {-1.5e308, 1.5e308}};
    const std::optional<std::array<double, 4>> ones{{1.0, 1.0, 1.0, 1.0}};
    GLINTFX_CHECK(same_weights(projective_weights(widest), ones));
}

// T4b-A-fix2 (D-W8-515): 20000 quadrilaterals whose corners are whole multiples, 0 to 64, of the
// smallest double (4.9e-324, subnormal) get the EXACT answer of exact_weights(): accepted or
// refused as the integers say, and the weights to twelve digits. Halving a subnormal coordinate
// rounded its last bit and changed the shape (T4b-A-fix1: 1696 of 200000 accepted wrongly, 2759
// refused wrongly, measured by the CTO).
GLINTFX_TEST(projective_quad_subnormal_corners_keep_their_shape) {
    constexpr double k_unit = std::numeric_limits<double>::denorm_min();
    quad_draw draw;
    std::size_t counted = 0;
    std::size_t wrong = 0;
    for (std::size_t i = 0; i < 20000U; ++i) {
        std::array<std::array<std::int64_t, 2>, 4> c{};
        for (auto &corner : c) {
            corner = {static_cast<std::int64_t>(65.0 * draw.next_unit()),
                      static_cast<std::int64_t>(65.0 * draw.next_unit())};
        }
        const gltfx_quad_world quad{
            {static_cast<double>(c[0][0]) * k_unit, static_cast<double>(c[0][1]) * k_unit},
            {static_cast<double>(c[1][0]) * k_unit, static_cast<double>(c[1][1]) * k_unit},
            {static_cast<double>(c[2][0]) * k_unit, static_cast<double>(c[2][1]) * k_unit},
            {static_cast<double>(c[3][0]) * k_unit, static_cast<double>(c[3][1]) * k_unit}};
        const exact_answer want = exact_weights(c);
        const std::optional<std::array<double, 4>> got = projective_weights(quad);
        ++counted;
        const bool right = want.accepted ? same_weights(got, want.weights) : !got.has_value();
        wrong += right ? 0U : 1U;
    }
    std::println("projective_quad_test: subnormais quadrilateros={} errados={}", counted, wrong);
    GLINTFX_CHECK_EQ(counted, std::size_t{20000});
    GLINTFX_CHECK_EQ(wrong, std::size_t{0});
}

// T4b-A-fix3 (D-W8-541, D-W8-542; IMPORTANTE-2 of the review of T4b-A-fix1): EVERY quadrilateral
// whose corners are whole numbers 0 to 4, in every orientation and with every vector pointing any
// way (5^8 = 390625 of them), gets the exact answer of exact_weights(), at unit 1 and at the
// smallest double. Without the absolute value in the largest component, the quadrilaterals whose
// six vector components are all zero or below were refused (1118 convex ones, measured).
GLINTFX_TEST(projective_quad_every_small_integer_quad_is_decided_exactly) {
    constexpr std::int64_t k_codes = 390625; // 5^8
    std::size_t counted = 0;
    std::size_t convex = 0;
    std::size_t wrong = 0;
    for (const double unit : {1.0, std::numeric_limits<double>::denorm_min()}) {
        for (std::int64_t code = 0; code < k_codes; ++code) {
            const std::array<std::array<std::int64_t, 2>, 4> c = corners_of_code(code);
            const exact_answer want = exact_weights(c);
            const std::optional<std::array<double, 4>> got =
                projective_weights(quad_of_integers(c, unit));
            ++counted;
            convex += want.accepted ? 1U : 0U;
            const bool right = want.accepted ? same_weights(got, want.weights) : !got.has_value();
            wrong += right ? 0U : 1U;
        }
    }
    std::println("projective_quad_test: enumeracao quadrilateros={} convexos={} errados={}",
                 counted, convex, wrong);
    GLINTFX_CHECK_EQ(counted, std::size_t{781250});
    GLINTFX_CHECK_EQ(convex, std::size_t{118368});
    GLINTFX_CHECK_EQ(wrong, std::size_t{0});
}

// T4b-A-fix4 (D-W8-554): corners of MIXED magnitude in one quadrilateral. A kite of half-width L
// whose two other corners are k1 and k2 multiples of the smallest double off the axis has, exactly,
// s = 1/2 and t = k1 / (k1 + k2), so q = 1, (k1 + k2) / (2 k2), 1, (k1 + k2) / (2 k1). With the
// one common scale of T4b-A-fix1 to fix3 the subnormal components were rounded away: of the 320
// kites of L 0.5 to 3, 84 refused and 186 with wrong weights (review of the fix2). L up to 2^1023
// (where the diagonal 2 L is not a double), each kite also turned a quarter (x and y swapped) and
// mirrored (y negated): the weights do not change.
namespace {

// The shape of a kite: half-width L, and the heights k1 and k2, in multiples of the smallest
// double, of its two other corners (T4b-A-fix5, D-W8-602: the review counted five parameters).
struct kite_shape {
    double half_width = 0.0;
    std::int64_t k1 = 0;
    std::int64_t k2 = 0;
};

// How the kite is laid: turned a quarter (x and y swapped) and mirrored (y negated).
struct kite_pose {
    bool turned = false;
    bool mirrored = false;
};

// How many kites were checked, and how many of them left the promised weights.
struct kite_tally {
    std::size_t counted = 0;
    std::size_t wrong = 0;

    void add(const kite_tally &other) {
        counted += other.counted;
        wrong += other.wrong;
    }
};

[[nodiscard]] gltfx_quad_world kite(const kite_shape &shape, kite_pose pose) {
    constexpr double k_unit = std::numeric_limits<double>::denorm_min();
    const double up = static_cast<double>(shape.k1) * k_unit;
    const double down = -static_cast<double>(shape.k2) * k_unit;
    const double sign = pose.mirrored ? -1.0 : 1.0;
    const auto at = [&](double x, double y) {
        return pose.turned ? glintfx::gltfx_vec2_world{sign * y, x}
                           : glintfx::gltfx_vec2_world{x, sign * y};
    };
    return gltfx_quad_world{at(-shape.half_width, 0.0), at(0.0, up), at(shape.half_width, 0.0),
                            at(0.0, down)};
}

// The four poses of `shape` against the promised weights `want`, each pose counted as it is
// checked (T4b-A-fix5, D-W8-603; T4b-A-fix6, D-W8-703: the count comes from the poses checked, not
// from a constant).
[[nodiscard]] kite_tally tally_of_poses(const kite_shape &shape,
                                        const std::array<double, 4> &want) {
    kite_tally tally;
    for (const int variant : {0, 1, 2, 3}) {
        const kite_pose pose{(variant & 1) != 0, (variant & 2) != 0};
        ++tally.counted;
        tally.wrong += within_promise(projective_weights(kite(shape, pose)), want) ? 0U : 1U;
    }
    return tally;
}

// The weights of the kite of heights k1 and k2: exactly s = 1/2 and t = k1 / (k1 + k2), so
// q = 1, (k1 + k2) / (2 k2), 1, (k1 + k2) / (2 k1), each one division of small whole numbers,
// rounded once.
[[nodiscard]] std::array<double, 4> kite_weights(std::int64_t k1, std::int64_t k2) {
    const double sum = static_cast<double>(k1 + k2);
    return {1.0, sum / (2.0 * static_cast<double>(k2)), 1.0, sum / (2.0 * static_cast<double>(k1))};
}

// The 64 kites of half-width `half_width`, heights k1 and k2 from 1 to 8, in their four poses
// (T4b-A-fix6, D-W8-702: the two loops of the heights were the second and third levels inside the
// test, and the rule of the house, clang-tidy with the body of the function as level 1, D-W8-561,
// counts that as level 4).
[[nodiscard]] kite_tally kites_of_width(double half_width) {
    kite_tally tally;
    for (std::int64_t k1 = 1; k1 <= 8; ++k1) {
        for (std::int64_t k2 = 1; k2 <= 8; ++k2) {
            tally.add(tally_of_poses(kite_shape{half_width, k1, k2}, kite_weights(k1, k2)));
        }
    }
    return tally;
}

} // namespace

GLINTFX_TEST(projective_quad_mixed_magnitudes_keep_their_weights) {
    constexpr std::array<double, 7> half_widths{0.5, 1.0, 1.5, 2.0, 3.0, 1e300, 0x1p1023};
    kite_tally total;
    for (const double half_width : half_widths) {
        total.add(kites_of_width(half_width));
    }
    std::println("projective_quad_test: magnitude mista quadrilateros={} errados={}", total.counted,
                 total.wrong);
    GLINTFX_CHECK_EQ(total.counted, std::size_t{1792});
    GLINTFX_CHECK_EQ(total.wrong, std::size_t{0});
}

// T4b-A-fix4 (D-W8-554, D-W8-556): the cases of the reviews where a mutant of the common scale
// went wrong and the code was right, kept so that no rewrite loses them: number 1403 of the fix2
// review (a coordinate near -1.9e308 next to -1 and 0: without the absolute value in the check of
// a quarter of the largest double, refused by overflow), number 3996 of the same family (refused
// when bottom_left.y was read as bottom_left.x) and number 3773 of the fix3 review (the
// components of y without their absolute value: refused). The weights are those of exact
// fractions, rounded once. And number 2947 of the fix2 family, which has NO mapping (a corner
// points inward by 1.6e-17 of the exact fraction): the same five cross products computed in
// double, with no bound on their rounding, gave it weights (measured by the CTO, T4b-A-fix4).
GLINTFX_TEST(projective_quad_review_cases_keep_their_weights) {
    const gltfx_quad_world case_1403{{-0x1.0000000000000p+0, -0x1.e42d130773b76p+1023},
                                     {-0x1.c7b1f3cac7433p+1019, 0x1.c7b1f3cac7433p+1019},
                                     {0x1.e848000000000p+19, 0x1.55c576d815726p+1021},
                                     {0x1.f543bf5f0e638p+1021, 0x0.0p+0}};
    const gltfx_quad_world case_3996{{0x1.c7b1f3cac7433p+1019, 0x1.0000000000000p+0},
                                     {0x1.c7b1f3cac7433p+1019, -0x1.c7b1f3cac7433p+1019},
                                     {-0x1.56e1fc2f8f359p-997, 0x1.56e1fc2f8f359p-996},
                                     {0x1.56e1fc2f8f359p-996, 0x1.e42d130773b76p+1023}};
    const gltfx_quad_world case_3773{{-0x1.b7cdfd9d7bdbbp-34, 0x1.e42d130773b76p+1023},
                                     {-0x0.0p+0, 0x1.e42d130773b76p+1023},
                                     {0x0.0p+0, -0x1.56e1fc2f8f359p-997},
                                     {-0x1.b7cdfd9d7bdbbp-34, 0x1.b7cdfd9d7bdbbp-34}};
    const gltfx_quad_world case_2947{{0x1.c7b1f3cac7433p+1019, 0x1.e848000000000p+19},
                                     {0x1.55c576d815726p+1021, 0x1.55c576d815726p+1023},
                                     {0x1.56e1fc2f8f359p-997, 0x1.55c576d815726p+1021},
                                     {-0x1.c7b1f3cac7433p+1019, 0x1.56e1fc2f8f359p-997}};
    const std::optional<std::array<double, 4>> want_1403{
        {0x1.24e1a08ad8f30p+2, 0x1.3a2e8ba2e8ba3p-1, 0x1.1f66b82651f67p-1, 0x1.599999999999ap+1}};
    const std::optional<std::array<double, 4>> want_3996{
        {0x1.0f0f0f0f0f0f1p-1, 0x1.0f0f0f0f0f0f1p-1, 0x1.2000000000000p+3, 0x1.2000000000000p+3}};
    const std::optional<std::array<double, 4>> ones{{1.0, 1.0, 1.0, 1.0}};
    GLINTFX_CHECK(same_weights(projective_weights(case_1403), want_1403));
    GLINTFX_CHECK(same_weights(projective_weights(case_3996), want_3996));
    GLINTFX_CHECK(same_weights(projective_weights(case_3773), ones));
    GLINTFX_CHECK(!projective_weights(case_2947).has_value());
}

// T4b-A-fix4 (D-W8-552): 1 - s keeps its digits near 1. The kite (-1, 0), (0, 1), (2^-60, 0),
// (0, -1) has s = 1 / (1 + 2^-60): computed as 1 - s from the rounded s it was 0, and the kite
// was refused (COSMETICO-2 of the fix2 review). Exactly, q0 = (1 + 2^-60) / 2^-59 = 2^59 + 1/2,
// which rounds to 2^59; q1 = q3 = 1 (t = 1/2) and q2 = (1 + 2^-60) / 2, which rounds to 1/2.
GLINTFX_TEST(projective_quad_one_minus_s_keeps_its_digits) {
    const gltfx_quad_world thin{{-1.0, 0.0}, {0.0, 1.0}, {0x1p-60, 0.0}, {0.0, -1.0}};
    const std::optional<std::array<double, 4>> want{{0x1p59, 1.0, 0.5, 1.0}};
    GLINTFX_CHECK(same_weights(projective_weights(thin), want));
}

// T4b-A-fix5 (D-W8-599): the promise of 2^-50 on the weights, held by a test (I-1 of the fix4
// review: nothing failed when the leading digits were cut to 50 bits). 1000 convex quadrilaterals
// of whole-number corners: a square of side 2^25 with each coordinate moved by 0 to 2^23 - 1.
// Every cross product of their corners is below 2^53, so it is exact in 64-bit integers and in
// double. The reference is the DEFINITION, not the path of the code: den = d1 x d2, s den = r x d2,
// t den = r x d1, 1 - s = (den - s den) / den, and each weight 0.5 den / x, one division of two
// exact doubles, rounded once. With the leading digits cut to 50 bits (mutant R1 of the review),
// 334 of the 1000 leave the promise (emulated by the CTO, T4b-A-fix5).
namespace {

[[nodiscard]] std::array<std::array<std::int64_t, 2>, 4> square_moved(quad_draw &draw) {
    constexpr std::int64_t k_side = std::int64_t{1} << 25U;
    std::array<std::array<std::int64_t, 2>, 4> c{
        {{0, 0}, {k_side, 0}, {k_side, k_side}, {0, k_side}}};
    for (auto &corner : c) {
        for (std::int64_t &coordinate : corner) {
            coordinate += static_cast<std::int64_t>(draw.next_unit() * 0x1p23);
        }
    }
    return c;
}

[[nodiscard]] std::array<double, 4>
defined_weights(const std::array<std::array<std::int64_t, 2>, 4> &c) {
    const std::int64_t d1x = c[2][0] - c[0][0];
    const std::int64_t d1y = c[2][1] - c[0][1];
    const std::int64_t d2x = c[3][0] - c[1][0];
    const std::int64_t d2y = c[3][1] - c[1][1];
    const std::int64_t den = cross_int(d1x, d1y, d2x, d2y);
    const std::int64_t ns = cross_int(c[1][0] - c[0][0], c[1][1] - c[0][1], d2x, d2y);
    const std::int64_t nt = cross_int(c[1][0] - c[0][0], c[1][1] - c[0][1], d1x, d1y);
    const auto weight = [den](std::int64_t x) {
        return 0.5 * static_cast<double>(den) / static_cast<double>(x);
    };
    return {weight(den - ns), weight(den - nt), weight(ns), weight(nt)};
}

} // namespace

GLINTFX_TEST(projective_quad_weights_keep_the_promised_digits) {
    quad_draw draw{20261020U};
    std::size_t counted = 0;
    std::size_t outside = 0;
    for (int i = 0; i < 1000; ++i) {
        const std::array<std::array<std::int64_t, 2>, 4> c = square_moved(draw);
        ++counted;
        const bool within =
            within_promise(projective_weights(quad_of_integers(c, 1.0)), defined_weights(c));
        outside += within ? 0U : 1U;
    }
    std::println("projective_quad_test: cota 2^-50 quadrilateros={} fora_da_cota={}", counted,
                 outside);
    GLINTFX_CHECK_EQ(counted, std::size_t{1000});
    GLINTFX_CHECK_EQ(outside, std::size_t{0});
}
