// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/filtered_cross.hpp"

#include <cmath>
#include <optional>

// draw2d/filtered_cross.cpp - R2D-TEXTURE, fatia T4b-A-FILTER: filtered_cross() of
// filtered_cross.hpp, in double-double arithmetic.
//
// THE BUDGET of step 5 of the header, u = 2^-53, A = |ph| + |qh|, alpha = |x1.hi y2.hi|,
// beta = |y1.hi x2.hi| (so alpha + beta <= (1 + u) A). A rounding to nearest of a sum is at most
// u of the result off, with no underflow error (a sum of doubles that underflows is exact); of a
// product, u of the result plus 2^-1075.
//   - P - (ph + pl + pt): the dropped x1.lo y2.lo, at most u^2 alpha; the two products of pt, each
//     u of itself plus 2^-1075, and their sum, u of it: (2 u + u^2) 2 u alpha plus 2^-1074 (1 + u).
//     A product of a hi by a lo is not zero only when that hi and the other hi are not zero, so
//     inside the window alpha >= 2^-960 and 2^-1074 (1 + u) <= 2^-113 alpha. With Q: 5.01 u^2 A.
//   - The tail: pl - ql is at most u A, rounded: u^2 A. pt - qt is at most 2.011 u A: 2.011 u^2 A.
//     Their sum, at most 3.012 u A: 3.012 u^2 A. sl, at most u (1 + u) A, plus that sum:
//     4.02 u^2 A. Together 10.05 u^2 A.
//   - |S - C| <= 15.06 u^2 A < 16 u^2 A = 2^-102 A. The bound 2^-98 A is computed with one
//     rounding of |ph| + |qh| (u) and a product by a power of two (exact, or among subnormals
//     2^-16 of itself at worst, since 2^-98 A >= 2^-1058): far inside the factor 16.
// THE PROMISE of step 6: certified means 2^-98 A <= 2^-56 |v| (the right side rounded at most to
// the next double, a relative 2^-16 at worst among subnormals), so |S - C| <= 2^-56 (1 + u)^2 |v|:
// the sign of S, hence of v, is the sign of C, and |v - C| <= (u + 2^-56 (1 + u)^2) |S| <=
// 1.1251 u |C|. A weight of projective_quad.hpp is 0.5 D / X rounded once: (1 + 1.1251 u)
// (1 + u) / (1 - 1.1251 u) - 1 <= 3.2503 u when both are certified; less when one comes from
// exact_cross() (1.0002 u). The fix5 tests of projective_quad_test hold every weight to 7 u of a
// reference that is itself u off.
namespace glintfx::draw2d {

namespace {

constexpr double k_splitter = 134217729.0;  // 2^27 + 1: Veltkamp's split of 53 bits into 26 and 27
constexpr double k_window_low = 0x1p-480;   // step 2 of the header
constexpr double k_window_high = 0x1p480;   // step 2 of the header
constexpr double k_bound_scale = 0x1p-98;   // step 5: 16 times the proved 2^-102
constexpr double k_certify_scale = 0x1p-56; // step 6: the bound at most 2^-56 of the value
constexpr int k_leading_bits = 63;          // |m| in [2^62, 2^63), as exact_cross_value says
constexpr double k_leading_scale = 0x1p63;  // 2^k_leading_bits, exact; cheaper than std::ldexp

// hi + lo, exactly the value it stands for.
struct double_double {
    double hi = 0.0;
    double lo = 0.0;
};

// a - b as hi + lo, exactly (Knuth's TwoSum of a and -b).
[[nodiscard]] double_double difference_of(double a, double b) noexcept {
    const double hi = a - b;
    const double minus_b_virtual = hi - a;
    const double lo = (a - (hi - minus_b_virtual)) - (b + minus_b_virtual);
    return double_double{hi, lo};
}

// a + b as hi + lo, exactly (Knuth's TwoSum).
[[nodiscard]] double_double sum_of(double a, double b) noexcept {
    const double hi = a + b;
    const double b_virtual = hi - a;
    const double lo = (a - (hi - b_virtual)) + (b - b_virtual);
    return double_double{hi, lo};
}

// a as hi + lo, hi with 26 significant bits and lo with 27 (Veltkamp).
[[nodiscard]] double_double halves_of(double a) noexcept {
    const double scaled = k_splitter * a;
    const double hi = scaled - (scaled - a);
    return double_double{hi, a - hi};
}

// a b as hi + lo, exactly inside the window (Dekker; no fused multiply-add).
[[nodiscard]] double_double product_of(double a, double b) noexcept {
    const double hi = a * b;
    const double_double ah = halves_of(a);
    const double_double bh = halves_of(b);
    const double lo =
        (((ah.hi * bh.hi) - hi) + (ah.hi * bh.lo) + (ah.lo * bh.hi)) + (ah.lo * bh.lo);
    return double_double{hi, lo};
}

// The four differences of (b - a) x (d - c): x1 = b.x - a.x, y2 = d.y - c.y, y1 = b.y - a.y and
// x2 = d.x - c.x.
struct four_differences {
    double_double x1;
    double_double y2;
    double_double y1;
    double_double x2;
};

[[nodiscard]] bool inside_window(double value) noexcept {
    const double magnitude = std::fabs(value);
    return magnitude == 0.0 || (magnitude >= k_window_low && magnitude <= k_window_high);
}

[[nodiscard]] bool all_inside_window(const four_differences &f) noexcept {
    return inside_window(f.x1.hi) && inside_window(f.y2.hi) && inside_window(f.y1.hi) &&
           inside_window(f.x2.hi);
}

// x y for two double-doubles: hi x hi exactly as head, the cross terms rounded as cross; lo x lo
// left out (step 3 of the header).
struct expanded_product {
    double_double head;
    double cross = 0.0;
};

[[nodiscard]] expanded_product expanded_product_of(const double_double &x,
                                                   const double_double &y) noexcept {
    return expanded_product{product_of(x.hi, y.hi), (x.hi * y.lo) + (x.lo * y.hi)};
}

// The value v of step 4 and the bound 2^-98 A of step 5, for P - Q.
struct value_and_bound {
    double value = 0.0;
    double bound = 0.0;
};

[[nodiscard]] value_and_bound difference_of_products(const expanded_product &p,
                                                     const expanded_product &q) noexcept {
    const double_double leading = sum_of(p.head.hi, -q.head.hi);
    const double tail = leading.lo + ((p.head.lo - q.head.lo) + (p.cross - q.cross));
    return value_and_bound{leading.hi + tail,
                           k_bound_scale * (std::fabs(p.head.hi) + std::fabs(q.head.hi))};
}

// v as m * 2^e with |m| in [2^62, 2^63), the form of exact_cross_value.
[[nodiscard]] exact_cross_value leading_digits_of(double value) noexcept {
    if (value == 0.0) {
        return exact_cross_value{};
    }
    int exponent = 0;
    const double fraction = std::frexp(value, &exponent); // |fraction| in [1/2, 1)
    const int sign = value > 0.0 ? 1 : -1;
    return exact_cross_value{sign, fraction * k_leading_scale, exponent - k_leading_bits};
}

} // namespace

std::optional<exact_cross_value> filtered_cross(glintfx::gltfx_vec2_world a,
                                                glintfx::gltfx_vec2_world b,
                                                glintfx::gltfx_vec2_world c,
                                                glintfx::gltfx_vec2_world d) noexcept {
    const four_differences f{difference_of(b.x, a.x), difference_of(d.y, c.y),
                             difference_of(b.y, a.y), difference_of(d.x, c.x)};
    if (!all_inside_window(f)) {
        return std::nullopt;
    }
    const value_and_bound v =
        difference_of_products(expanded_product_of(f.x1, f.y2), expanded_product_of(f.y1, f.x2));
    if (!(v.bound <= k_certify_scale * std::fabs(v.value))) {
        return std::nullopt;
    }
    return leading_digits_of(v.value);
}

} // namespace glintfx::draw2d
