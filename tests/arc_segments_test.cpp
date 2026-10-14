// SPDX-License-Identifier: AGPL-3.0-or-later
#include <algorithm>
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

// Relative to the answer at EVERY size (S5-A-fix2): near() above is absolute below 1, and would
// take 0 for 1e-100.
[[nodiscard]] bool relatively_near(double got, double want, double tolerance) {
    return std::abs(got - want) <= tolerance * std::abs(want);
}

// A linear congruential generator with a fixed seed, so every system draws the same affines.
struct affine_draw {
    std::uint64_t state = 20261014U;
    [[nodiscard]] double next_unit() {
        state = (state * 6364136223846793005ULL) + 1442695040888963407ULL;
        return static_cast<double>(state >> 11U) * 0x1.0p-53;
    }
};

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

// A stretch whose squares overflow is still the largest stretch (S5-A-fix1, D-W8-399): the entries
// are scaled first, and the common case above keeps its bits. 1e200 on the diagonal, or only in
// the last entry, is a stretch of 1e200; the shear of 1e200 is the golden ratio times 1e200; one
// column of (1.7e308, 1.7e308) is a stretch past the largest double.
GLINTFX_TEST(arc_segments_the_screen_radius_does_not_overflow_on_the_way) {
    const pixel_affine diagonal{1.0e200, 0.0, 0.0, 1.0e200, 0.0, 0.0};
    GLINTFX_CHECK(near(screen_radius(1.0, diagonal), 1.0e200));
    const pixel_affine last_entry{1.0, 0.0, 0.0, 1.0e200, 0.0, 0.0};
    GLINTFX_CHECK(near(screen_radius(1.0, last_entry), 1.0e200));
    const pixel_affine sheared{1.0e200, 0.0, 1.0e200, 1.0e200, 0.0, 0.0};
    GLINTFX_CHECK(near(screen_radius(1.0, sheared), 1.6180339887498949e200));
    const pixel_affine past{1.7e308, 1.7e308, 0.0, 0.0, 0.0, 0.0};
    GLINTFX_CHECK(!std::isfinite(screen_radius(1.0, past)));
}

// The radius on the screen is right at every size, or not finite (S5-A-fix2, D-W8-434 and
// D-W8-436): never a finite number that is wrong. The two answers the review measured wrong
// (1e78 by 1e76 gave 7.07e77, 1e-100 by 1e-120 gave 7.07e-101), squares that underflow, entries
// with a sign (a turn of 180 degrees, and the largest entry alone and negative), a world radius
// other than 1 at both ends, and a stretch almost the same along both axes, where a subtraction of
// the squares would cancel (the formula before this fix answered 1 + 7.45e-9 for 1 - 1e-15 by 1,
// turned by 30 degrees).
GLINTFX_TEST(arc_segments_the_screen_radius_is_right_or_not_finite_at_every_size) {
    const pixel_affine thin_huge{1.0e78, 0.0, 0.0, 1.0e76, 0.0, 0.0};
    GLINTFX_CHECK(relatively_near(screen_radius(1.0, thin_huge), 1.0e78, 1e-15));
    const pixel_affine thin_tiny{1.0e-100, 0.0, 0.0, 1.0e-120, 0.0, 0.0};
    GLINTFX_CHECK(relatively_near(screen_radius(1.0, thin_tiny), 1.0e-100, 1e-15));
    const pixel_affine both_tiny{1.0e-200, 0.0, 0.0, 1.0e-200, 0.0, 0.0};
    GLINTFX_CHECK(relatively_near(screen_radius(1.0, both_tiny), 1.0e-200, 1e-15));
    const pixel_affine half_turn{-1.0e200, 0.0, 0.0, -1.0e200, 0.0, 0.0};
    GLINTFX_CHECK(relatively_near(screen_radius(1.0, half_turn), 1.0e200, 1e-15));
    const pixel_affine negative_shear{-1.0e200, 0.0, 1.0e200, -1.0e200, 0.0, 0.0};
    GLINTFX_CHECK(
        relatively_near(screen_radius(1.0, negative_shear), 1.6180339887498949e200, 1e-15));
    // The largest entry alone and negative, in each of the four places: the scale must come from
    // its size, never from its signed value.
    const std::array<pixel_affine, 4> lone_negative{{
        {-1.0e200, 0.0, 0.0, 1.0, 0.0, 0.0},
        {1.0, -1.0e200, 0.0, 0.0, 0.0, 0.0},
        {0.0, 0.0, -1.0e200, 1.0, 0.0, 0.0},
        {1.0, 0.0, 0.0, -1.0e200, 0.0, 0.0},
    }};
    for (const pixel_affine &affine : lone_negative) {
        GLINTFX_CHECK(relatively_near(screen_radius(1.0, affine), 1.0e200, 1e-15));
    }
    const pixel_affine huge{1.0e200, 0.0, 0.0, 1.0e200, 0.0, 0.0};
    GLINTFX_CHECK(relatively_near(screen_radius(3.0, huge), 3.0e200, 1e-15));
    const pixel_affine tiny{1.0e-300, 0.0, 0.0, 1.0e-300, 0.0, 0.0};
    GLINTFX_CHECK(relatively_near(screen_radius(0.5, tiny), 5.0e-301, 1e-15));
    const pixel_affine almost_round =
        pixel_affine_from_transform(gltfx_transform{.translation = {0.0, 0.0},
                                                    .rotation = {0.5235987755982988},
                                                    .scale = {1.0, 1.0 - 1.0e-15}});
    GLINTFX_CHECK(relatively_near(screen_radius(1.0, almost_round), 1.0, 1e-15));
    const double smallest = std::numeric_limits<double>::denorm_min();
    const pixel_affine subnormal{smallest, 0.0, 0.0, 0.0, 0.0, 0.0};
    GLINTFX_CHECK_EQ(screen_radius(1.0, subnormal), smallest);
    const double infinity = std::numeric_limits<double>::infinity();
    const pixel_affine infinite_entry{1.0, 0.0, 0.0, -infinity, 0.0, 0.0};
    GLINTFX_CHECK(std::isinf(screen_radius(1.0, infinite_entry)));
    const pixel_affine nan_entry{1.0, std::numeric_limits<double>::quiet_NaN(), 0.0, 1.0, 0.0, 0.0};
    GLINTFX_CHECK(std::isnan(screen_radius(1.0, nan_entry)));
}

// A sweep (D-W8-436): 4000 turned non-uniform scales R(theta) diag(sx, sy), sx and sy drawn from
// 1e-300 to 1e300 by their exponent, so the two axes differ by up to 600 orders of size. The
// largest stretch is max(sx, sy) by construction, known without the code; every answer is within
// 1e-13 of it (the entries themselves are rounded products of a sine or a cosine). The formula
// before this fix was finite and wrong in about one sweep case in six (the review's long double
// reference: 34043 of 200000).
GLINTFX_TEST(arc_segments_the_screen_radius_sweep_of_sizes_and_turns) {
    affine_draw draw;
    std::size_t counted = 0;
    std::size_t wrong = 0;
    for (std::size_t i = 0; i < 4000U; ++i) {
        const double exponent_x = -300.0 + (600.0 * draw.next_unit());
        const double exponent_y = -300.0 + (600.0 * draw.next_unit());
        const double turn = 6.283185307179586 * draw.next_unit();
        const double sx = std::pow(10.0, exponent_x);
        const double sy = std::pow(10.0, exponent_y);
        const pixel_affine affine{sx * std::cos(turn),
                                  sx * std::sin(turn),
                                  -sy * std::sin(turn),
                                  sy * std::cos(turn),
                                  0.0,
                                  0.0};
        const double want = std::max(sx, sy);
        const double got = screen_radius(1.0, affine);
        ++counted;
        if (!relatively_near(got, want, 1e-13)) {
            ++wrong;
            if (wrong <= 4U) {
                std::println("arc_segments_test: FALHOU varredura i={} sx={} sy={} giro={} "
                             "resposta={} esperado={}",
                             i, sx, sy, turn, got, want);
            }
        }
    }
    std::println("arc_segments_test: varredura casos={} erradas={}", counted, wrong);
    GLINTFX_CHECK_EQ(counted, std::size_t{4000});
    GLINTFX_CHECK_EQ(wrong, std::size_t{0});
}
