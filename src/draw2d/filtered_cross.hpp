// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <optional>

#include <glintfx/core/vec2.hpp>

#include "draw2d/exact_cross.hpp"

// draw2d/filtered_cross.hpp - R2D-TEXTURE, fatia T4b-A-FILTER (D-W8-637 to D-W8-640): the cross
// product of two differences of points, (b - a) x (d - c), in double-double arithmetic with a
// certified error bound. When the bound vouches for the answer, it is the answer of
// exact_cross.hpp in kind: the sign with no doubt at all, and the leading digits within 2^-52 of
// the exact value, |m| in [2^62, 2^63). When it does not, there is no answer, and the caller asks
// exact_cross(). Pure: no GL, no window, allocates nothing.
//
// WHY (the review of T4b-A-fix4 measured it): the wide integers of exact_cross() cost about 17
// times the double arithmetic they replaced, for EVERY quadrilateral. A filtered predicate answers
// the ordinary case in floating point and leaves the exact computation to the cases the bound
// cannot vouch for (J. R. Shewchuk, "Adaptive Precision Floating-Point Arithmetic and Fast Robust
// Geometric Predicates", 1997; the Filtered_predicate of CGAL). Plain double is not enough here:
// the weights of projective_quad.hpp are a ratio of two cross products and promise 2^-50, and a
// cross product in plain double is about 4 x 2^-53 off even with no cancellation (the weight, about
// 9 x 2^-53). Double-double brings each certified cross product to about 1.13 x 2^-53.
//
// THE COMPUTATION, u = 2^-53, every step one operation rounded to nearest, NO contraction into a
// fused multiply-add and no long double (GODS_LAWS.md L-04; -ffp-contract=off, D-W8-441):
//   1. The four differences b.x - a.x, d.y - c.y, b.y - a.y, d.x - c.x, each as hi + lo EXACTLY
//      (Knuth's TwoSum: exact with subnormals, and no step overflows when the first does not:
//      S. Boldo, S. Graillat and J.-M. Muller, "On the robustness of the 2Sum and Fast2Sum
//      algorithms", ACM TOMS, 2017). |lo| <= u |hi|.
//   2. THE WINDOW: every hi is zero or within [2^-480, 2^480], or there is no answer. Above, a
//      product could overflow; below, the error of a product might not be a double (Dekker's
//      product is exact when the exponents of the factors add up to at least -970: e_min + p - 1
//      for doubles), and the bound below could vanish among subnormals, where the floating-point
//      filter of Shewchuk certifies wrong signs (Bartels, Fisikopoulos and Weiser, "Fast
//      Floating-Point Filters for Robust Predicates", 2023). Inside, the exponents add up to at
//      least -960, and every partial product of Dekker's algorithm is a whole multiple of 2^-1064.
//   3. P = x1 y2 and Q = y1 x2: hi x hi exactly as ph + pl (Veltkamp's split by 2^27 + 1 and
//      Dekker's product), the cross terms x1.hi y2.lo + x1.lo y2.hi rounded (pt), and x1.lo y2.lo
//      dropped (at most u^2 |x1.hi y2.hi|). The same for Q.
//   4. ph - qh = sh + sl exactly (TwoSum); tail = sl + ((pl - ql) + (pt - qt)); v = sh + tail.
//   5. THE BOUND: with A = |ph| + |qh|, the exact cross product C and S = sh + tail (before
//      the last rounding) satisfy |S - C| <= 15.06 u^2 A < 2^-102 A (THE BUDGET in
//      filtered_cross.cpp adds the terms one by one). The bound used is 2^-98 A, 16 times that.
//   6. CERTIFIED when 2^-98 A <= 2^-56 |v|. Then the sign of v is the sign of C, and |v - C| <=
//      1.1251 u |C| (u of the last rounding, 2^-56 of the bound). A = 0 only when both products
//      are exactly zero: v is then zero, certified, and C is zero.
// A ratio of two certified values, or of one certified and one exact (exact_cross: 2^-62 cut plus
// one rounding), rounded once, is within 3.26 x 2^-53 of the exact ratio: inside the 2^-50 that
// projective_quad.hpp promises for its weights.
namespace glintfx::draw2d {

// (b - a) x (d - c), as exact_cross() gives it, when the bound vouches for it; nothing otherwise
// (then exact_cross() answers). For finite coordinates; a value that is not finite gives nothing.
[[nodiscard]] std::optional<exact_cross_value> filtered_cross(glintfx::gltfx_vec2_world a,
                                                              glintfx::gltfx_vec2_world b,
                                                              glintfx::gltfx_vec2_world c,
                                                              glintfx::gltfx_vec2_world d) noexcept;

} // namespace glintfx::draw2d
