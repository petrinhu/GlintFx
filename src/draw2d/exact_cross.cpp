// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/exact_cross.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdint>

// draw2d/exact_cross.cpp - R2D-TEXTURE, fatia T4b-A-fix4: exact_cross() of exact_cross.hpp, in
// signed whole numbers of 32-bit limbs.
namespace glintfx::draw2d {

namespace {

constexpr std::size_t k_limbs = 140; // 4480 bits: a product of two differences of doubles fits
constexpr int k_limb_bits = 32;
constexpr int k_mantissa_bits = 53;
constexpr int k_leading_bits = 63; // kept below 2^63, so the conversion is from a signed integer

// A whole number: magnitude in limbs, least significant first, `size` of them in use (the top one
// not zero), and its sign. Zero is size 0, never negative.
struct whole {
    std::array<std::uint32_t, k_limbs> limb{};
    std::size_t size = 0;
    bool negative = false;
};

void trim(whole &w) noexcept {
    while (w.size > 0 && w.limb[w.size - 1] == 0U) {
        --w.size;
    }
    if (w.size == 0) {
        w.negative = false;
    }
}

// The exponent of the unit of `value`: value = M * 2^unit with M a whole number below 2^53.
[[nodiscard]] int unit_exponent(double value) noexcept {
    int exponent = 0;
    static_cast<void>(std::frexp(value, &exponent));
    return exponent - k_mantissa_bits;
}

// `value` (finite, its unit at or above 2^base) as the whole number value / 2^base.
[[nodiscard]] whole whole_of(double value, int base) noexcept {
    whole w;
    if (value == 0.0) {
        return w;
    }
    int exponent = 0;
    const double mantissa = std::frexp(std::fabs(value), &exponent);
    auto digits = static_cast<std::uint64_t>(std::ldexp(mantissa, k_mantissa_bits));
    const int shift = exponent - k_mantissa_bits - base;
    std::size_t at = static_cast<std::size_t>(shift / k_limb_bits);
    const int bits = shift % k_limb_bits;
    const std::uint64_t low = digits << bits;
    const std::uint64_t high = bits == 0 ? 0U : digits >> (64 - bits);
    for (const std::uint64_t part : {low, low >> 32U, high}) {
        w.limb[at] = static_cast<std::uint32_t>(part & 0xFFFFFFFFU);
        ++at;
    }
    w.size = at;
    w.negative = value < 0.0;
    trim(w);
    return w;
}

[[nodiscard]] int compare_magnitude(const whole &a, const whole &b) noexcept {
    if (a.size != b.size) {
        return a.size < b.size ? -1 : 1;
    }
    for (std::size_t i = a.size; i > 0; --i) {
        if (a.limb[i - 1] != b.limb[i - 1]) {
            return a.limb[i - 1] < b.limb[i - 1] ? -1 : 1;
        }
    }
    return 0;
}

// |a| + |b|, with the sign of a.
[[nodiscard]] whole add_magnitudes(const whole &a, const whole &b) noexcept {
    whole sum;
    std::uint64_t carry = 0;
    const std::size_t size = std::max(a.size, b.size);
    for (std::size_t i = 0; i < size; ++i) {
        const std::uint64_t total = std::uint64_t{a.limb[i]} + b.limb[i] + carry;
        sum.limb[i] = static_cast<std::uint32_t>(total & 0xFFFFFFFFU);
        carry = total >> 32U;
    }
    sum.limb[size] = static_cast<std::uint32_t>(carry);
    sum.size = size + 1;
    sum.negative = a.negative;
    trim(sum);
    return sum;
}

// |a| - |b| for |a| >= |b|, with the sign of a.
[[nodiscard]] whole subtract_magnitudes(const whole &a, const whole &b) noexcept {
    whole difference;
    std::uint64_t borrow = 0;
    for (std::size_t i = 0; i < a.size; ++i) {
        const std::uint64_t taken = std::uint64_t{b.limb[i]} + borrow;
        const std::uint64_t own = a.limb[i];
        difference.limb[i] = static_cast<std::uint32_t>((own - taken) & 0xFFFFFFFFU);
        borrow = own < taken ? 1U : 0U;
    }
    difference.size = a.size;
    difference.negative = a.negative;
    trim(difference);
    return difference;
}

// a + b, signed.
[[nodiscard]] whole add(const whole &a, const whole &b) noexcept {
    if (a.negative == b.negative) {
        return add_magnitudes(a, b);
    }
    return compare_magnitude(a, b) >= 0 ? subtract_magnitudes(a, b) : subtract_magnitudes(b, a);
}

[[nodiscard]] whole negated(whole w) noexcept {
    w.negative = w.size != 0 && !w.negative;
    return w;
}

// a * b, signed. Each limb product plus two limbs fits in 64 bits.
[[nodiscard]] whole multiply(const whole &a, const whole &b) noexcept {
    whole product;
    if (a.size == 0 || b.size == 0) {
        return product;
    }
    for (std::size_t i = 0; i < a.size; ++i) {
        std::uint64_t carry = 0;
        for (std::size_t j = 0; j < b.size; ++j) {
            const std::uint64_t total =
                (std::uint64_t{a.limb[i]} * b.limb[j]) + product.limb[i + j] + carry;
            product.limb[i + j] = static_cast<std::uint32_t>(total & 0xFFFFFFFFU);
            carry = total >> 32U;
        }
        product.limb[i + b.size] = static_cast<std::uint32_t>(carry);
    }
    product.size = a.size + b.size;
    product.negative = a.negative != b.negative;
    trim(product);
    return product;
}

// The bit at position `bit` of |w| (0 past either end).
[[nodiscard]] std::uint64_t bit_of(const whole &w, int bit) noexcept {
    if (bit < 0 || static_cast<std::size_t>(bit / k_limb_bits) >= w.size) {
        return 0U;
    }
    return (w.limb[static_cast<std::size_t>(bit / k_limb_bits)] >> (bit % k_limb_bits)) & 1U;
}

// The leading 63 bits of w as m * 2^e, m in [2^62, 2^63); the bits below are dropped (an error
// below 2^-62 of the value), and the conversion of m to double rounds once (2^-53).
[[nodiscard]] exact_cross_value leading_of(const whole &w, int base) noexcept {
    if (w.size == 0) {
        return exact_cross_value{};
    }
    const std::uint32_t top = w.limb[w.size - 1];
    const int top_bits = k_limb_bits - std::countl_zero(top);
    const int length = (static_cast<int>(w.size) - 1) * k_limb_bits + top_bits;
    const int from = length - k_leading_bits;
    std::int64_t digits = 0;
    for (int bit = length - 1; bit >= from; --bit) {
        digits = (digits << 1) | static_cast<std::int64_t>(bit_of(w, bit));
    }
    const double m = static_cast<double>(digits);
    return exact_cross_value{w.negative ? -1 : 1, w.negative ? -m : m, from + base};
}

} // namespace

exact_cross_value exact_cross(glintfx::gltfx_vec2_world a, glintfx::gltfx_vec2_world b,
                              glintfx::gltfx_vec2_world c, glintfx::gltfx_vec2_world d) noexcept {
    const std::array<double, 8> values{a.x, a.y, b.x, b.y, c.x, c.y, d.x, d.y};
    int base = INT_MAX;
    for (const double value : values) {
        base = value == 0.0 ? base : std::min(base, unit_exponent(value));
    }
    base = base == INT_MAX ? 0 : base;
    const whole bx_ax = add(whole_of(b.x, base), negated(whole_of(a.x, base)));
    const whole by_ay = add(whole_of(b.y, base), negated(whole_of(a.y, base)));
    const whole dx_cx = add(whole_of(d.x, base), negated(whole_of(c.x, base)));
    const whole dy_cy = add(whole_of(d.y, base), negated(whole_of(c.y, base)));
    const whole value = add(multiply(bx_ax, dy_cy), negated(multiply(by_ay, dx_cx)));
    return leading_of(value, 2 * base);
}

} // namespace glintfx::draw2d
