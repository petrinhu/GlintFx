// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <print>

#include <glintfx/core/vec2.hpp>

#include "draw2d/exact_cross.hpp"
#include "draw2d/filtered_cross.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// filtered_cross_test.cpp - R2D-TEXTURE, fatia T4b-A-FILTER (D-W8-637 to D-W8-646): the cross
// product in double-double with a certified bound (draw2d/filtered_cross.hpp). The reference is
// exact_cross(), the wide integers, an independent computation: when the filter answers, its sign
// must be the exact one and its value within 2^-51 of the exact value (the promise of the header
// is 1.1251 x 2^-53, and exact_cross() itself is up to 1.0002 x 2^-53 off). The cases that a
// filter with a loose bound gets wrong were found by the CTO with an emulator of this computation
// in Python floats against exact fractions (T4b-A-FILTER, Anexo P), and are drawn here from a
// fixed generator, so every system draws the same points.

using glintfx::gltfx_vec2_world;
using glintfx::draw2d::exact_cross;
using glintfx::draw2d::exact_cross_value;
using glintfx::draw2d::filtered_cross;

namespace {

struct cross_case {
    gltfx_vec2_world a;
    gltfx_vec2_world b;
    gltfx_vec2_world c;
    gltfx_vec2_world d;
};

[[nodiscard]] std::optional<exact_cross_value> filtered_of(const cross_case &k) {
    return filtered_cross(k.a, k.b, k.c, k.d);
}

[[nodiscard]] exact_cross_value exact_of(const cross_case &k) {
    return exact_cross(k.a, k.b, k.c, k.d);
}

// The answer of the filter is the exact one: the same sign, |m| in [2^62, 2^63), and the value
// m * 2^e within 2^-51 of the exact value (the two exponents at most one apart, so the difference
// of the two m is exact or nearly).
[[nodiscard]] bool same_answer(const exact_cross_value &got, const exact_cross_value &want) {
    if (got.sign != want.sign) {
        return false;
    }
    if (want.sign == 0) {
        return got.m == 0.0;
    }
    const bool normal = std::abs(got.m) >= 0x1p62 && std::abs(got.m) < 0x1p63;
    const int shift = got.e - want.e;
    if (!normal || shift < -1 || shift > 1) {
        return false;
    }
    return std::abs(std::ldexp(got.m, shift) - want.m) <= std::ldexp(std::abs(want.m), -51);
}

// SplitMix64 with a fixed seed: whole-number operations only, the same on every system.
struct point_draw {
    std::uint64_t state = 20261020U;
    [[nodiscard]] std::uint64_t next() {
        state += 0x9E3779B97F4A7C15ULL;
        std::uint64_t z = state;
        z = (z ^ (z >> 30U)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27U)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31U);
    }
    // A double in [1, 2) with all 53 bits drawn: a whole number below 2^53, exact in double.
    [[nodiscard]] double mantissa() {
        const auto whole = static_cast<std::int64_t>((next() >> 11U) | (std::uint64_t{1} << 52U));
        return std::ldexp(static_cast<double>(whole), -52);
    }
    [[nodiscard]] std::size_t below(std::size_t count) {
        return static_cast<std::size_t>(next() % count);
    }
};

// Four points at the scale 2^scale whose (b - a) x (d - c) cancels about `bits` bits: d - c is
// lambda (b - a) plus or minus 2^-bits times (b - a) turned a quarter (the sign drawn, so the cross
// product has either sign), each operation rounded, so the differences are not exact and the low
// parts of the double-doubles are not zero. One draw per statement, so the order of the draws is
// the same everywhere.
[[nodiscard]] cross_case near_line_case(point_draw &draw, int scale, int bits) {
    const double sign_x = draw.below(2) != 0 ? 1.0 : -1.0;
    cross_case k{};
    k.a.x = std::ldexp(draw.mantissa(), scale);
    k.a.y = std::ldexp(draw.mantissa(), scale);
    k.b.x = std::ldexp(sign_x * draw.mantissa(), scale - 1);
    k.b.y = std::ldexp(draw.mantissa(), scale + 1);
    k.c.x = std::ldexp(draw.mantissa(), scale);
    k.c.y = std::ldexp(-draw.mantissa(), scale);
    const double lambda = draw.mantissa();
    const double x1 = k.b.x - k.a.x;
    const double y1 = k.b.y - k.a.y;
    const double epsilon_size = std::ldexp(draw.mantissa(), -bits);
    const double epsilon = draw.below(2) != 0 ? -epsilon_size : epsilon_size;
    k.d.x = k.c.x + ((lambda * x1) - (epsilon * y1));
    k.d.y = k.c.y + ((lambda * y1) + (epsilon * x1));
    return k;
}

struct sweep_tally {
    std::size_t counted = 0;
    std::size_t certified = 0;
    std::size_t wrong = 0;
    std::size_t simple = 0;
    std::size_t simple_refused = 0;
};

// One case into the tally. `simple`: little cancellation, every difference inside the window, so
// the filter must answer.
void tally_case(sweep_tally &tally, const cross_case &k, bool simple) {
    ++tally.counted;
    tally.simple += simple ? 1U : 0U;
    const std::optional<exact_cross_value> got = filtered_of(k);
    if (!got.has_value()) {
        tally.simple_refused += simple ? 1U : 0U;
        return;
    }
    ++tally.certified;
    tally.wrong += same_answer(*got, exact_of(k)) ? 0U : 1U;
}

// K1 and K2 of the CTO's search: (b - a) x (d - c) with b - a = (1 + al, 1) and d - c =
// (1 + ga, 1 + al), al = 2^-53 - 2^-105, so every low part sits where double-double rounds.
// K1: ga = 0x1.07fffffffffffp-52; the exact value is about -6.94e-18, and the double-double value
// is a relative 1.7e-15 off it: a bound of 2^-114 A (16 times too small) or a threshold of 2^-40
// certifies it. K2: ga = 0x1.ffffffffffffep-53; the exact value is about +1.23e-32, and the
// double-double value is exactly zero: a bound of zero certifies the wrong sign.
constexpr double k_al = 0x1.ffffffffffffep-54;
constexpr cross_case k_loose_bound_case{
    {-k_al, 0.0}, {1.0, 1.0}, {-0x1.07fffffffffffp-52, -k_al}, {1.0, 1.0}};
constexpr cross_case k_zero_bound_case{
    {-k_al, 0.0}, {1.0, 1.0}, {-0x1.ffffffffffffep-53, -k_al}, {1.0, 1.0}};

// Six cases of the sweep below in which the filter answers with the very bits of exact_cross(),
// both signs: four of them change in the last bit if the low part of ph - qh (step 4 of the
// header) is dropped (cases 64, 155, 528 and 693 of the generator; 74 and 162 are two ordinary
// ones).
constexpr std::array<cross_case, 6> k_same_bits_cases{{
    {{0x1.caab51f9ba607p+460, 0x1.41b08707894e3p+460},
     {-0x1.4e8a6301f9671p+459, 0x1.93ff6af1f0807p+461},
     {0x1.c43094128de13p+460, -0x1.9dfbf904dc641p+460},
     {-0x1.9df3eed03f04ep+461, 0x1.d63665bda13c1p+460}},
    {{0x1.df2b87c2b62bap+0, 0x1.9638e8151f6e5p+0},
     {-0x1.407d06086fbefp-1, 0x1.a7cf583b740cep+1},
     {0x1.ba705b16b8b42p+0, -0x1.520b49f1211bbp+0},
     {-0x1.39f7f8a574845p+1, 0x1.9f4981f4d1ee3p+0}},
    {{0x1.049b058f1450fp-40, 0x1.42ec0f1c84753p-40},
     {-0x1.8b0ba7f06e5c5p-41, 0x1.ac6c43a092492p-39},
     {0x1.64df221133691p-40, -0x1.597b0fd9be0c3p-40},
     {-0x1.9303a0fc60164p-39, -0x1.2b589936b0a3ap-41}},
    {{0x1.41ec517752728p-40, 0x1.75d4faf0a8112p-40},
     {0x1.80c5496ef116dp-41, 0x1.601a75bf98a68p-39},
     {0x1.bf8e777e64915p-40, -0x1.87a01379c6b71p-40},
     {0x1.bdb93f1bf499bp-40, 0x1.10c4cb7dd802ap-41}},
    {{0x1.2cab270a6cc40p-300, 0x1.94e1a6b8eab9ap-300},
     {0x1.df58af7275a71p-301, 0x1.c13ca8b00adc4p-299},
     {0x1.a5a5412d1f0c2p-300, -0x1.cd758b921a86bp-300},
     {0x1.3ca29ffea20f8p-298, 0x1.35252fb737daep-301}},
    {{0x1.1309c768a5f61p+460, 0x1.f3db4be0b143dp+460},
     {0x1.3d53d0121cb49p+459, 0x1.9048b3f4889a6p+461},
     {0x1.49a8a13a1e306p+460, -0x1.3e15122c682adp+460},
     {0x1.4d40f7e9d6c62p+458, -0x1.95745b00216a0p+457}},
}};

} // namespace

// Whole numbers: (3, 1) x (1, 5) = 14 and the turned product -14, answered and exact, with |m| in
// [2^62, 2^63); two equal points make both products exactly zero (A = 0, step 6 of the header):
// answered, zero.
GLINTFX_TEST(filtered_cross_vouches_for_an_ordinary_product) {
    const gltfx_vec2_world origin{0.0, 0.0};
    const auto forward = filtered_cross(origin, gltfx_vec2_world{3.0, 1.0},
                                        gltfx_vec2_world{1.0, 0.0}, gltfx_vec2_world{2.0, 5.0});
    const auto turned = filtered_cross(gltfx_vec2_world{1.0, 0.0}, gltfx_vec2_world{2.0, 5.0},
                                       origin, gltfx_vec2_world{3.0, 1.0});
    const auto zero = filtered_cross(gltfx_vec2_world{2.0, 3.0}, gltfx_vec2_world{2.0, 3.0}, origin,
                                     gltfx_vec2_world{1.0, 1.0});
    GLINTFX_CHECK(forward.has_value() && turned.has_value() && zero.has_value());
    if (forward.has_value() && turned.has_value() && zero.has_value()) {
        GLINTFX_CHECK(forward->sign == 1 && std::ldexp(forward->m, forward->e) == 14.0);
        GLINTFX_CHECK(forward->m >= 0x1p62 && forward->m < 0x1p63);
        GLINTFX_CHECK(turned->sign == -1 && std::ldexp(turned->m, turned->e) == -14.0);
        GLINTFX_CHECK(zero->sign == 0 && zero->m == 0.0);
    }
}

// What the bound cannot vouch for has no answer: K1 and K2 (whose exact signs are -1 and +1,
// which exact_cross() gives); a difference above the window (2^500) or below it (three times the
// smallest double); and a coordinate that is not finite.
GLINTFX_TEST(filtered_cross_refuses_what_its_bound_cannot_vouch_for) {
    const gltfx_vec2_world origin{0.0, 0.0};
    const gltfx_vec2_world up{0.0, 1.0};
    constexpr double k_tiny = 3.0 * std::numeric_limits<double>::denorm_min();
    const double infinite = std::numeric_limits<double>::infinity();
    GLINTFX_CHECK(!filtered_of(k_loose_bound_case).has_value());
    GLINTFX_CHECK_EQ(exact_of(k_loose_bound_case).sign, -1);
    GLINTFX_CHECK(!filtered_of(k_zero_bound_case).has_value());
    GLINTFX_CHECK_EQ(exact_of(k_zero_bound_case).sign, 1);
    GLINTFX_CHECK(!filtered_cross(origin, gltfx_vec2_world{0x1p500, 0.0}, origin, up).has_value());
    GLINTFX_CHECK(!filtered_cross(origin, gltfx_vec2_world{k_tiny, 0.0}, origin, up).has_value());
    GLINTFX_CHECK(!filtered_cross(origin, gltfx_vec2_world{infinite, 0.0}, origin, up).has_value());
}

// 20000 cases near a line, at scales from 2^-540 to 2^500, cancellations of 0 to 70 bits and both
// signs: every answer of the filter is the exact one, and every case with at most 30 bits of
// cancellation and its differences inside the window (scales 2^-470 to 2^460) is answered.
GLINTFX_TEST(filtered_cross_keeps_the_promise_at_every_scale) {
    constexpr std::array<int, 13> k_scales{0,   1,    -3,   20,   -40,  200, -300,
                                           460, -470, -500, -520, -540, 500};
    point_draw draw;
    sweep_tally tally;
    for (std::size_t i = 0; i < 20000U; ++i) {
        const int scale = k_scales[draw.below(k_scales.size())];
        const auto bits = static_cast<int>(draw.below(71U));
        const cross_case k = near_line_case(draw, scale, bits);
        tally_case(tally, k, bits <= 30 && scale >= -470 && scale <= 460);
    }
    std::println("filtered_cross_test: varredura casos={} certificados={} errados={} simples={} "
                 "simples_recusados={}",
                 tally.counted, tally.certified, tally.wrong, tally.simple, tally.simple_refused);
    GLINTFX_CHECK_EQ(tally.counted, std::size_t{20000});
    GLINTFX_CHECK_EQ(tally.wrong, std::size_t{0});
    GLINTFX_CHECK(tally.simple > 0U);
    GLINTFX_CHECK_EQ(tally.simple_refused, std::size_t{0});
}

// The six cases above: the filter answers with the bits of exact_cross(), the same on every
// system, since exact_cross() works in whole numbers.
GLINTFX_TEST(filtered_cross_agrees_bit_for_bit_with_the_exact_path) {
    std::size_t same = 0;
    for (const cross_case &k : k_same_bits_cases) {
        const std::optional<exact_cross_value> got = filtered_of(k);
        const exact_cross_value want = exact_of(k);
        const bool equal =
            got.has_value() && got->sign == want.sign && got->m == want.m && got->e == want.e;
        same += equal ? 1U : 0U;
    }
    std::println("filtered_cross_test: mesmos bits casos={} iguais={}", k_same_bits_cases.size(),
                 same);
    GLINTFX_CHECK_EQ(same, k_same_bits_cases.size());
}
