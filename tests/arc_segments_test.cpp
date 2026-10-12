// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <print>

#include <glintfx/core/transform.hpp>

#include "draw2d/arc_segments.hpp"
#include "draw2d/quad_vertices.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// arc_segments_test.cpp - R2D-SHAPES, fatia S3 (D-W8-269): the count of straight segments that
// stand in for an arc on the screen, as a CLOSED table, and the radius on the screen of a world
// radius. The counts are computed BEFORE the code, outside it (python, the formula of the header
// and the sagitta r (1 - cos(pi / n)) <= 0.25 < r (1 - cos(pi / (n - 1)))), never read from its
// output. No GL, no window.

using glintfx::gltfx_transform;
using glintfx::draw2d::arc_segment_count;
using glintfx::draw2d::half_arc_segment_count;
using glintfx::draw2d::pixel_affine;
using glintfx::draw2d::pixel_affine_from_transform;
using glintfx::draw2d::screen_radius;

namespace {

constexpr double k_error = 0.25;

struct count_row {
    double radius = 0.0;
    std::uint32_t whole = 0U;
    std::uint32_t half = 0U;
};

[[nodiscard]] bool near(double got, double want) {
    return std::abs(got - want) <= 1e-12 * std::max(1.0, std::abs(want));
}

} // namespace

GLINTFX_TEST(arc_segments_closed_table_of_the_count) {
    // The nine rows of D-W8-269, then the last radius before the count reaches 512 by itself
    // (13280: 511.99...), one past it where only the limit keeps it at 512 (13300: 512.38...), an
    // infinite radius, and the radius equal to the error.
    const std::array<count_row, 13> rows{{
        {0.25, 4U, 2U},
        {0.5, 4U, 2U},
        {1.0, 5U, 3U},
        {2.0, 7U, 4U},
        {10.0, 15U, 8U},
        {50.0, 32U, 16U},
        {100.0, 45U, 23U},
        {1000.0, 141U, 71U},
        {10000.0, 445U, 223U},
        {13280.0, 512U, 256U},
        {13300.0, 512U, 256U},
        {std::numeric_limits<double>::infinity(), 512U, 256U},
        {0.0, 4U, 2U},
    }};
    std::size_t counted = 0;
    std::size_t failed = 0;
    for (const count_row &row : rows) {
        ++counted;
        const std::uint32_t whole = arc_segment_count(row.radius, k_error);
        const std::uint32_t half = half_arc_segment_count(row.radius, k_error);
        if (whole != row.whole || half != row.half) {
            ++failed;
            std::println(
                "arc_segments_test: FALHOU r={} (inteiro {} esperado {}, meio {} esperado {})",
                row.radius, whole, row.whole, half, row.half);
        }
    }
    std::println("arc_segments_test: casos={} falhas={}", counted, failed);
    GLINTFX_CHECK_EQ(counted, std::size_t{13});
    GLINTFX_CHECK_EQ(failed, std::size_t{0});
}

// The contract says the caller never passes a NaN; the function still answers inside [4, 512]
// (never an undefined conversion of a NaN into an integer).
GLINTFX_TEST(arc_segments_a_radius_that_is_not_a_number_gives_the_minimum) {
    GLINTFX_CHECK_EQ(arc_segment_count(std::numeric_limits<double>::quiet_NaN(), k_error), 4U);
}

GLINTFX_TEST(arc_segments_the_screen_radius_follows_the_largest_stretch) {
    const pixel_affine identity{};
    GLINTFX_CHECK(near(screen_radius(3.0, identity), 3.0));
    const pixel_affine doubled = pixel_affine_from_transform(
        gltfx_transform{.translation = {7.0, -5.0}, .rotation = {0.0}, .scale = {2.0, 2.0}});
    GLINTFX_CHECK(near(screen_radius(3.0, doubled), 6.0));
    // A non-uniform scale, turned by 30 degrees: the longer axis, 3, whatever the turn.
    const pixel_affine stretched = pixel_affine_from_transform(gltfx_transform{
        .translation = {0.0, 0.0}, .rotation = {0.5235987755982988}, .scale = {3.0, 1.0}});
    GLINTFX_CHECK(near(screen_radius(3.0, stretched), 9.0));
    // A shear, columns (1, 0) and (1, 1): its largest singular value is the golden ratio.
    const pixel_affine sheared{1.0, 0.0, 1.0, 1.0, 0.0, 0.0};
    GLINTFX_CHECK(near(screen_radius(2.0, sheared), 2.0 * 1.6180339887498949));
    const pixel_affine collapsed{0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    GLINTFX_CHECK_EQ(screen_radius(3.0, collapsed), 0.0);
}
