// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/core/vec2.hpp>

// draw2d/exact_cross.hpp - R2D-TEXTURE, fatia T4b-A-fix4 (D-W8-551, D-W8-552): the cross product of
// two differences of points, (b - a) x (d - c), EXACTLY, for any finite coordinates: its sign with
// no rounding at all, and its leading digits to within 2^-52 of the exact value. Pure: no GL, no
// window, allocates nothing.
//
// WHY EXACT (the review of T4b-A-fix2 and fix3): in double, a product of two coordinate differences
// rounds, overflows near the largest double and vanishes among subnormals, and one common scale
// for the whole quadrilateral rounded the small components away when the corners mixed magnitudes
// (a sliver a few multiples of the smallest double tall next to a corner at 1, or at 1e308). The
// sign of a cross product decides whether a quadrilateral has a projective mapping, so it must not
// depend on any of that.
//
// THE TECHNIQUE (the integer fallback of exact geometric predicates: J. R. Shewchuk, "Adaptive
// Precision Floating-Point Arithmetic and Fast Robust Geometric Predicates", 1997, assumes no
// overflow nor underflow; past that range the predicate libraries fall back to integers): every
// finite double is a whole number times a power of two, so the eight coordinates are written as
// whole numbers over the smallest of their units, and the differences, the two products and their
// difference are computed in integers wide enough for any of them (4480 bits; a product of two
// differences of doubles stays below 2^4200). The width used is only the spread of the
// coordinates' magnitudes: a few 32-bit limbs for an ordinary quadrilateral. The same integer
// operations on every system (GODS_LAWS.md L-04): no long double, no fused multiply-add.
namespace glintfx::draw2d {

// The value m * 2^e of a cross product: sign zero when it is exactly zero (then m is 0); m carries
// the sign and the leading digits (|m| in [2^62, 2^63) or 0).
struct exact_cross_value {
    int sign = 0;
    double m = 0.0;
    int e = 0;
};

// (b - a) x (d - c) = (b.x - a.x) (d.y - c.y) - (b.y - a.y) (d.x - c.x), for finite coordinates
// (a coordinate that is not finite is the caller's to refuse before).
[[nodiscard]] exact_cross_value exact_cross(glintfx::gltfx_vec2_world a,
                                            glintfx::gltfx_vec2_world b,
                                            glintfx::gltfx_vec2_world c,
                                            glintfx::gltfx_vec2_world d) noexcept;

} // namespace glintfx::draw2d
