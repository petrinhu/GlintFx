// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <print>
#include <span>
#include <vector>

#include "draw2d/ellipse_triangles.hpp"
#include "draw2d/quad_vertices.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// ellipse_triangles_test.cpp - R2D-SHAPES, fatia S5-B (D-W8-413): the indices of the fan and of the
// ring (draw2d/ellipse_triangles.hpp). The indices of the closed cases are written by hand from the
// rule of the header; the sweep checks the two properties that make a ring cover each pixel once:
// every triangle turns the way the outer polygon turns (or has no area), and the triangles add up
// to the outer polygon less the inner one. No GL, no window.

using glintfx::draw2d::pixel_position_wide;
using glintfx::draw2d::write_fan_indices;
using glintfx::draw2d::write_ring_indices;

namespace {

[[nodiscard]] double turn_of(const pixel_position_wide &a, const pixel_position_wide &b,
                             const pixel_position_wide &c) {
    return ((b.x - a.x) * (c.y - a.y)) - ((b.y - a.y) * (c.x - a.x));
}

[[nodiscard]] double turn_of_polygon(std::span<const pixel_position_wide> polygon) {
    double sum = 0.0;
    for (std::size_t k = 1U; k + 1U < polygon.size(); ++k) {
        sum += turn_of(polygon[0], polygon[k], polygon[k + 1U]);
    }
    return sum;
}

// The ring of the closed cases: the square 0..10 outside; inside, a quadrilateral whose first
// side folds (the corner at inner 0 is reflex in the quadrilateral of k = 0).
[[nodiscard]] std::array<pixel_position_wide, 8> folded_ring(double y_sign) {
    return std::array<pixel_position_wide, 8>{{
        {0.0, 0.0 * y_sign},
        {10.0, 0.0 * y_sign},
        {10.0, 10.0 * y_sign},
        {0.0, 10.0 * y_sign},
        {4.0, 1.0 * y_sign},
        {8.0, 5.0 * y_sign},
        {7.0, 7.0 * y_sign},
        {3.0, 7.0 * y_sign},
    }};
}

} // namespace

GLINTFX_TEST(ellipse_triangles_the_fan_of_four_rim_points) {
    std::array<std::uint32_t, 12> out{};
    GLINTFX_CHECK_EQ(write_fan_indices(4U, out), std::size_t{12});
    const std::array<std::uint32_t, 12> want{0U, 1U, 2U, 0U, 2U, 3U, 0U, 3U, 4U, 0U, 4U, 1U};
    GLINTFX_CHECK(out == want);
    std::array<std::uint32_t, 11> short_out{};
    GLINTFX_CHECK_EQ(write_fan_indices(4U, short_out), std::size_t{0});
    const std::array<std::uint32_t, 11> untouched_11{};
    GLINTFX_CHECK(short_out == untouched_11);
    GLINTFX_CHECK_EQ(write_fan_indices(0U, out), std::size_t{0});
}

GLINTFX_TEST(ellipse_triangles_the_ring_takes_the_diagonal_that_keeps_the_turn) {
    const std::array<pixel_position_wide, 8> ring = folded_ring(1.0);
    std::array<std::uint32_t, 24> out{};
    GLINTFX_CHECK_EQ(write_ring_indices(ring, out), std::size_t{24});
    // k = 0: (outer 0, inner 1) would turn (0,0) (8,5) (4,1) clockwise, so the other diagonal:
    // (0, 1, 4) and (1, 5, 4). k = 1: (1, 2, 6) turns counter-clockwise but (1, 6, 5) does not
    // (cross of (-3, 7) and (-2, 5) is -1), so (1, 2, 5) and (2, 6, 5).
    GLINTFX_CHECK(out[0] == 0U && out[1] == 1U && out[2] == 4U);
    GLINTFX_CHECK(out[3] == 1U && out[4] == 5U && out[5] == 4U);
    GLINTFX_CHECK(out[6] == 1U && out[7] == 2U && out[8] == 5U);
    GLINTFX_CHECK(out[9] == 2U && out[10] == 6U && out[11] == 5U);
    // A mirror turns every triangle and the outer polygon alike: the same cut.
    std::array<std::uint32_t, 24> mirrored_out{};
    GLINTFX_CHECK_EQ(write_ring_indices(folded_ring(-1.0), mirrored_out), std::size_t{24});
    GLINTFX_CHECK(mirrored_out == out);
    // Every triangle of this ring turns with the outer square.
    std::size_t against = 0;
    for (std::size_t t = 0; t < 24U; t += 3U) {
        against += turn_of(ring[out[t]], ring[out[t + 1U]], ring[out[t + 2U]]) < 0.0 ? 1U : 0U;
    }
    GLINTFX_CHECK_EQ(against, std::size_t{0});
}

GLINTFX_TEST(ellipse_triangles_the_ring_refuses_what_it_cannot_hold) {
    const std::array<pixel_position_wide, 8> ring = folded_ring(1.0);
    std::array<std::uint32_t, 23> short_out{};
    GLINTFX_CHECK_EQ(write_ring_indices(ring, short_out), std::size_t{0});
    const std::array<std::uint32_t, 23> untouched_23{};
    GLINTFX_CHECK(short_out == untouched_23);
    std::array<std::uint32_t, 24> out{};
    GLINTFX_CHECK_EQ(write_ring_indices(std::span<const pixel_position_wide>(ring).first(7), out),
                     std::size_t{0});
    GLINTFX_CHECK_EQ(write_ring_indices(std::span<const pixel_position_wide>(ring).first(4), out),
                     std::size_t{0});
}

// 300 rings of regular polygons, n from 3 to 302, the inner one scaled by a factor from 0 to 1 and
// turned by up to half a step (so its corners do not face the outer ones), some mirrored: every
// triangle turns with the outer polygon (or has no area), and the turns add up to the outer
// polygon less the inner one.
GLINTFX_TEST(ellipse_triangles_ring_sweep_covers_once) {
    std::size_t counted = 0;
    std::size_t wrong = 0;
    for (std::size_t i = 0; i < 300U; ++i) {
        const std::size_t n = 3U + i;
        const double factor = static_cast<double>(i % 10U) / 10.0;
        const double offset = 0.5 * static_cast<double>(i % 7U) / 7.0;
        const double y_sign = i % 2U == 0U ? 1.0 : -1.0;
        std::vector<pixel_position_wide> ring(2U * n);
        for (std::size_t k = 0; k < n; ++k) {
            const double angle =
                2.0 * std::numbers::pi * static_cast<double>(k) / static_cast<double>(n);
            const double inner_angle =
                angle + (offset * 2.0 * std::numbers::pi / static_cast<double>(n));
            ring[k] = {1000.0 + (50.0 * std::cos(angle)),
                       700.0 + (50.0 * y_sign * std::sin(angle))};
            ring[n + k] = {1000.0 + (50.0 * factor * std::cos(inner_angle)),
                           700.0 + (50.0 * factor * y_sign * std::sin(inner_angle))};
        }
        std::vector<std::uint32_t> out(6U * n);
        const std::size_t written = write_ring_indices(ring, out);
        const double sense = turn_of_polygon(std::span<const pixel_position_wide>(ring).first(n));
        double sum = 0.0;
        bool good = written == 6U * n;
        for (std::size_t t = 0; good && t < written; t += 3U) {
            const double turn = turn_of(ring[out[t]], ring[out[t + 1U]], ring[out[t + 2U]]);
            good = sense > 0.0 ? turn >= -1e-9 : turn <= 1e-9;
            sum += turn;
        }
        const double want =
            sense - turn_of_polygon(std::span<const pixel_position_wide>(ring).subspan(n));
        good = good && std::abs(sum - want) <= 1e-7 * std::abs(sense);
        ++counted;
        if (!good) {
            ++wrong;
            if (wrong <= 4U) {
                std::println("ellipse_triangles_test: FALHOU anel n={} fator={}", n, factor);
            }
        }
    }
    std::println("ellipse_triangles_test: varredura aneis={} errados={}", counted, wrong);
    GLINTFX_CHECK_EQ(counted, std::size_t{300});
    GLINTFX_CHECK_EQ(wrong, std::size_t{0});
}
