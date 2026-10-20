// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <print>

#include <glintfx/core/vec2.hpp>

#include "draw2d/exact_cross.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// exact_cross_test.cpp - R2D-TEXTURE, fatia T4b-A-fix4 (D-W8-551, D-W8-552): the exact cross
// product of two differences of points (draw2d/exact_cross.hpp). The expected values are whole
// numbers or powers of two, so every one is exact in double and the test needs no wide integers
// of its own.

using glintfx::gltfx_vec2_world;
using glintfx::draw2d::exact_cross;
using glintfx::draw2d::exact_cross_value;

namespace {

// The value m * 2^e of a result, as a double (exact for the values of this test).
[[nodiscard]] double value_of(const exact_cross_value &v) { return std::ldexp(v.m, v.e); }

// A linear congruential generator with a fixed seed, so every system draws the same points.
struct point_draw {
    std::uint64_t state = 20261020U;
    [[nodiscard]] std::int64_t next_small() {
        state = (state * 6364136223846793005ULL) + 1442695040888963407ULL;
        return static_cast<std::int64_t>(state >> 54U) - 512; // -512 to 511
    }
};

} // namespace

// Whole-number points from -512 to 511, scaled by a power of two from the smallest double to
// 2^960: the product of the differences is a whole number below 2^22, so its exact value and sign
// are known in 64-bit integers, and the result must be that value times the square of the scale.
GLINTFX_TEST(exact_cross_whole_numbers_at_every_scale) {
    constexpr std::array<int, 5> scales{-1074, -500, 0, 500, 960};
    point_draw draw;
    std::size_t counted = 0;
    std::size_t wrong = 0;
    for (std::size_t i = 0; i < 4000U; ++i) {
        std::array<std::int64_t, 8> c{};
        for (std::int64_t &coordinate : c) {
            coordinate = draw.next_small();
        }
        const std::int64_t want = ((c[2] - c[0]) * (c[7] - c[5])) - ((c[3] - c[1]) * (c[6] - c[4]));
        for (const int scale : scales) {
            const auto at = [&](std::size_t k) {
                return std::ldexp(static_cast<double>(c[k]), scale);
            };
            const exact_cross_value got =
                exact_cross(gltfx_vec2_world{at(0), at(1)}, gltfx_vec2_world{at(2), at(3)},
                            gltfx_vec2_world{at(4), at(5)}, gltfx_vec2_world{at(6), at(7)});
            const int want_sign = want > 0 ? 1 : (want < 0 ? -1 : 0);
            const bool right =
                got.sign == want_sign &&
                (want == 0 ? got.m == 0.0
                           : std::ldexp(got.m, got.e - (2 * scale)) == static_cast<double>(want));
            ++counted;
            wrong += right ? 0U : 1U;
        }
    }
    std::println("exact_cross_test: inteiros quadrilateros={} errados={}", counted, wrong);
    GLINTFX_CHECK_EQ(counted, std::size_t{20000});
    GLINTFX_CHECK_EQ(wrong, std::size_t{0});
}

// Magnitudes mixed in one product: a difference that is not a double, L - (-L) = 2 L = 2^1024 with
// L = 2^1023 (it overflows in double), times -3 m with m the smallest double: exactly -3 * 2^-50.
GLINTFX_TEST(exact_cross_mixed_magnitudes) {
    constexpr double k_small = std::numeric_limits<double>::denorm_min();
    constexpr double k_large = 0x1p1023;
    const exact_cross_value v =
        exact_cross(gltfx_vec2_world{-k_large, 0.0}, gltfx_vec2_world{k_large, 0.0},
                    gltfx_vec2_world{0.0, k_small}, gltfx_vec2_world{0.0, -2.0 * k_small});
    // (2^1024) (-3 * 2^-1074) = -3 * 2^-50.
    GLINTFX_CHECK_EQ(v.sign, -1);
    GLINTFX_CHECK(value_of(v) == -3.0 * 0x1p-50);
    // A sliver: (1, k_small) x (1, k_small) is exactly zero, and (1, k_small) x (1, 2 k_small) is
    // exactly k_small.
    const gltfx_vec2_world origin{0.0, 0.0};
    const exact_cross_value zero =
        exact_cross(origin, gltfx_vec2_world{1.0, k_small}, origin, gltfx_vec2_world{1.0, k_small});
    GLINTFX_CHECK_EQ(zero.sign, 0);
    const exact_cross_value tiny = exact_cross(origin, gltfx_vec2_world{1.0, k_small}, origin,
                                               gltfx_vec2_world{1.0, 2.0 * k_small});
    GLINTFX_CHECK_EQ(tiny.sign, 1);
    GLINTFX_CHECK(value_of(tiny) == k_small);
    // The leading digits keep more than 32 bits: (2^40 + 1, 0) x (0, 1) is 2^40 + 1, exact.
    const exact_cross_value wide_value = exact_cross(origin, gltfx_vec2_world{0x1p40 + 1.0, 0.0},
                                                     origin, gltfx_vec2_world{0.0, 1.0});
    GLINTFX_CHECK(value_of(wide_value) == 0x1p40 + 1.0);
}

// Cancellation: a = (0.5, 0.6), b = (12.5, 12.6) and d = (24.5, 24.6), each coordinate the double
// nearest to the decimal, are NOT on one line as doubles (0.6, 12.6 and 24.6 are not exact). The
// exact cross product (b - a) x (d - a) of the doubles as they are is 57 * 2^-51 (computed in
// fractions by the CTO, T4b-A-fix4); the double computation of the same formula gives 0.
GLINTFX_TEST(exact_cross_keeps_the_sign_through_cancellation) {
    const gltfx_vec2_world a{0.5, 0.6};
    const gltfx_vec2_world b{12.5, 12.6};
    const gltfx_vec2_world d{24.5, 24.6};
    const exact_cross_value v = exact_cross(a, b, a, d);
    const double naive = ((b.x - a.x) * (d.y - a.y)) - ((b.y - a.y) * (d.x - a.x));
    std::println("exact_cross_test: cancelamento exato m={} e={} ingenuo={}", v.m, v.e, naive);
    GLINTFX_CHECK_EQ(v.sign, 1);
    GLINTFX_CHECK(value_of(v) == 57.0 * 0x1p-51);
    GLINTFX_CHECK(naive == 0.0);
}

// T4b-A-fix5 (D-W8-598): the leading digits keep 62 bits, which the promise of the header (2^-52)
// and that of the weights (2^-50) need. (2147483647, 0) x (0, 1515870811) is the odd whole number
// 3255307777587127717, 62 bits long; the double nearest to it is 0x1.696969692d2d3p+61 (computed
// by the CTO in integers). Leading digits cut to 50 bits (mutant R1 of the fix4 review) give
// 0x1.696969692d2d0p+61, and nothing in the suite failed (I-1 of that review).
GLINTFX_TEST(exact_cross_keeps_sixty_two_bits) {
    const gltfx_vec2_world origin{0.0, 0.0};
    const gltfx_vec2_world across{2147483647.0, 0.0};
    const gltfx_vec2_world up{0.0, 1515870811.0};
    const exact_cross_value v = exact_cross(origin, across, origin, up);
    const exact_cross_value turned = exact_cross(origin, up, origin, across);
    std::println("exact_cross_test: sessenta e dois bits m={} e={}", v.m, v.e);
    GLINTFX_CHECK_EQ(v.sign, 1);
    GLINTFX_CHECK(value_of(v) == 0x1.696969692d2d3p+61);
    GLINTFX_CHECK_EQ(turned.sign, -1);
    GLINTFX_CHECK(value_of(turned) == -0x1.696969692d2d3p+61);
}
