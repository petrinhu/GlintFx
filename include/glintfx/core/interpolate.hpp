// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/export.hpp>

// core/interpolate.hpp - CORE-MATH2D (TODO.md W4, GODS_LAWS.md
// L-19/L-20/L-26): the core layer's linear interpolation between two
// numbers.
//
// ============================================================
// THE TRAP THIS FILE EXISTS TO AVOID, MEASURED AND NOT ASSUMED
// ============================================================
//
// The obvious way to write interpolation is `a + t * (b - a)`. It is
// NOT exact at the far end: with t exactly 1 it can return something
// that is not b - and not merely off by a rounding step.
//
// MEASURED LIVE in a container on 05/09/2026 (GODS_LAWS.md L-09),
// double precision, this exact expression:
//
//     a = 1e17, b = 1.0, t = 1.0
//       naive `a + t * (b - a)`  ->  0            WRONG
//       the form used here       ->  1            exact
//
//     a = -1e17, b = 1.0, t = 1.0
//       naive                    ->  0            WRONG
//       the form used here       ->  1            exact
//
//     a = 1e8, b = 1.0, t = 1.0   -> both give 1  (the trap does NOT
//                                    show up at every magnitude, which
//                                    is exactly why a hand-picked
//                                    happy case proves nothing here)
//
// Zero is not "close to 1" by any tolerance a test would write: the
// subtraction `b - a` rounds to exactly `-a`, and `a + (-a)` is zero.
// An animation that ends at t = 1 would land on the wrong value with
// no complaint from anything - the silent-wrong-answer shape this
// project refuses.
//
// THE FORM USED HERE and its source, checked and not remembered
// (GODS_LAWS.md L-43): std::lerp from <cmath>, standardized by
// P0811 "Well-behaved interpolation for numbers and pointers"
// (https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2019/p0811r3.html),
// which guarantees exactness at both ends - lerp(a, b, 0) == a and
// lerp(a, b, 1) == b - and monotonicity near the far end, precisely
// the two properties the naive expression loses. Reference summary:
// https://en.cppreference.com/cpp/numeric/lerp
//
// It is standard library, so GODS_LAWS.md L-07 (zero dependency beyond
// the C++23 standard library and the operating system's own API) is
// satisfied by construction - nothing is vendored and nothing is
// pulled in.
//
// WHY THIS IS WRAPPED AT ALL INSTEAD OF TELLING CONSUMERS TO CALL
// std::lerp THEMSELVES (implementer inference, GODS_LAWS.md L-27):
// TODO.md's CORE-MATH2D line names interpolation as one of the six
// pieces this slice freezes, so it belongs on the public surface; and
// the wrapper is where the measurement above is written down, which is
// the part a consumer will not rediscover on their own.
//
// EXTRAPOLATION IS ALLOWED, not clamped: t below 0 or above 1 keeps
// going past the endpoints, which std::lerp also defines. Clamping
// would silently discard what an easing curve that overshoots (a
// "ultrapassa-e-volta" curve, see TODO.md's ANIM-EASING) deliberately
// asks for. Exercised at t = -0.5 and t = 1.5 by
// `interpolation_extrapolates_past_both_ends_instead_of_clamping` in
// tests/math2d_test.cpp.
//
// WHAT IS NOT HERE, AND WHY: there is no interpolation of a vector,
// of a rectangle, of an angle or of a transform. Only the number.
// TODO.md names "interpolacao" once, and every one of those would be
// public surface frozen forever (GODS_LAWS.md L-26) for something a
// consumer composes from two or three calls to the function below.
// Interpolating an ANGLE in particular is NOT a plain component-wise
// job - the right answer for a rotation usually takes the short way
// around the circle, which is a decision about behavior, not a helper,
// and it belongs to the slice that needs it with its own review.
//
// PRECISION: double, the world side (core/vec2.hpp). A consumer
// interpolating on the screen side crosses with
// gltfx_vec2_screen_to_world()/gltfx_vec2_world_to_screen() the same
// way every other crossing in this library is written - explicitly.

namespace glintfx {

// Linear interpolation from `a` to `b` by `t`.
//
// EXACT AT BOTH ENDS: `gltfx_lerp(a, b, 0)` is exactly `a` and
// `gltfx_lerp(a, b, 1)` is exactly `b`, for every finite pair -
// including the pair where the naive expression returns zero (see this
// file's own measurement above, reproduced by
// `interpolation_returns_the_exact_endpoint_at_one_where_the_naive_form_does_not`
// in tests/math2d_test.cpp).
//
// TOTAL, per the pure-math rule this project already carries
// (core/time.hpp's own header comment, canonical record in
// DECISOES_AUTONOMAS.md entry D8): deterministic, never undefined
// behavior, never fallible, no error channel.
//
// NON-FINITE INPUT IS THIS LIBRARY'S OWN DECISION, NOT THE STANDARD
// LIBRARY'S - and that is a correction made DURING this slice, after
// measuring, not a preference:
//
//   IF ANY OF `a`, `b` OR `t` IS NOT FINITE, THE RESULT IS
//   NOT-A-NUMBER. One rule, one outcome, the same on all five
//   platforms.
//
// WHAT WAS MEASURED, live in a container on 05/09/2026, GCC 16.2.1
// with libstdc++ - the first version of this function called
// std::lerp unguarded, and these are the answers it gave:
//
//     std::lerp(not-a-number, 1.0, 0.5)  ->  1        FABRICATED
//     std::lerp(+infinity,    1.0, 0.5)  ->  1        FABRICATED
//     std::lerp(2.0, 3.0, not-a-number)  ->  3        FABRICATED
//     std::lerp(0.0, 1.0, +infinity)     ->  not-a-number
//     std::lerp(+infinity, -infinity, 0.5) -> not-a-number
//
// The first three are exactly the defect shape DECISOES_AUTONOMAS.md
// entry D8 was written about: a clean, plausible, finite answer
// returned for an input that carried no information at all, with
// nothing to distinguish it from a real success. A consumer feeding a
// position that has already gone bad gets a valid-looking coordinate
// back and never learns.
//
// AND THE EDGE IS CONTESTED UPSTREAM, which is the second reason not
// to delegate it: llvm/llvm-project issue 166628 argues that
// std::lerp(0, 1, +infinity) SHOULD return infinity rather than the
// not-a-number both implementations return today
// (https://github.com/llvm/llvm-project/issues/166628). No divergence
// between two standard libraries was measured HERE - stated plainly
// rather than implied - but a behavior that is under active argument
// upstream is a behavior that can change under this library on any
// platform, in a compiler update, silently. GODS_LAWS.md L-04 requires
// this library to behave IDENTICALLY on all five targets; the way to
// guarantee that is to decide it here, not to inherit it.
//
// WHY NOT-A-NUMBER RATHER THAN D8'S "GOES TO ZERO" CLAUSE (implementer
// inference, GODS_LAWS.md L-27 - flagged to the orchestrator): D8's
// clause exists because a system call was returning the SAME sentinel
// for every invalid input, indistinguishable from success. Zero here
// would be that same defect wearing a different number - a perfectly
// plausible coordinate. A not-a-number is the opposite: it is visible
// in the result and poisons everything downstream until somebody
// looks. D8's other clause, "saturate toward the same side", is not
// applicable either: `+infinity` interpolated toward `-infinity` has
// no side at all, and an infinite `t` between two EQUAL endpoints has
// no direction to move along - answering those per combination would
// be a table of special cases, each needing its own defense, instead
// of one rule anybody can state from memory.
//
// STILL TOTAL: deterministic, never undefined behavior, never
// fallible, no error channel - the guard runs BEFORE any call into the
// standard library, never after, which is the same order-of-operations
// fix core/time.hpp already documents for its own std::llround call.
// Every FINITE input keeps std::lerp's exactness and monotonicity
// untouched. Exercised by
// `interpolation_never_fabricates_a_finite_answer_from_a_non_finite_input`.
[[nodiscard]] GLINTFX_API double gltfx_lerp(double a, double b, double t) noexcept;

} // namespace glintfx
