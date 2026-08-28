// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

#include <glintfx/export.hpp>

// core/time.hpp - CORE-TIME (TODO.md W3, GODS_LAWS.md L-19/L-20/L-26):
// the core layer's public representation of elapsed time and duration.
// Value types of the core layer (GODS_LAWS.md L-19's opacity clause is
// scoped to "handle e subsistema com estado" - these are neither; the
// layout stable IS the contract, the same exception glintfx::version
// and glintfx::gltfx_rgba already use).
//
// THE ONE DECISION OF THE PROJECT LEADER THIS FREEZES, 27/08/2026, via
// AskUserQuestion (ESCOPO.md SS2, decision 3 - verbatim, choosing
// between the two options he was shown): "Exato, com atalho".
//
//   The library reports elapsed time as an EXACT INTEGER COUNT of a
//   tiny unit, which NEVER ACCUMULATES ERROR, plus a ready-made
//   conversion to seconds ALONGSIDE it. The alternative - seconds with
//   decimal places as the primary form - was REFUSED: it is easier to
//   use and DRIFTS over hours of runtime, exactly the regime a game
//   runs in.
//
//   The product reasoning, his own words: precision for whoever needs
//   it, convenience for whoever does not, without forcing anyone to
//   choose between the two.
//
// THE UNIT is nanoseconds - the same "unidade minuscula" the prior-art
// research this slice's own service order already cites settled on
// (SDL3's SDL_GetTicksNS(), a 64-bit nanosecond count; GODS_LAWS.md
// L-29: the TECHNIQUE was read to learn from, nothing here is copied -
// no shared name, no shared signature, no shared code). A signed
// 64-bit nanosecond count covers roughly 292 years before it
// overflows, far past any plausible single process runtime.
//
// WHAT IS OUT OF SCOPE HERE, ON PURPOSE: reading the ACTUAL monotonic
// clock from the operating system. GODS_LAWS.md L-19 lists "tempo"
// among the core layer's pure subjects, explicitly alongside "nao
// conhece o sistema operacional" - this header only carries the
// REPRESENTATION (the value types and the arithmetic on them). The
// platform adapter that produces a REAL gltfx_time_point by calling
// into the operating system's own clock belongs to the platform layer,
// and lands in a later slice (LOOP-RUN, TODO.md) that CONSUMES this
// one, never the other way around.
//
// NAMES AND SHAPE BELOW - implementer inference, NOT a leader decision
// (GODS_LAWS.md L-27, the same discipline color.hpp's own header
// comment already applies to gltfx_rgba8 and to its field names):
//
//   - `gltfx_duration`/`gltfx_time_point`, not the unprefixed
//     `duration`/`time_point`: those two unprefixed spellings are
//     EXACTLY std::chrono::duration and std::chrono::time_point,
//     docs/api-conventions.md R6's own collision class (CE-1's
//     error_code -> gltfx_err_code finding is the precedent this
//     mirrors) - and a stronger case than that finding, because the
//     spelling match here is total, not partial. A 2D engine's own
//     consumer routinely writes `using namespace std::chrono;` for
//     their own timing code, which is exactly the shape that turns an
//     unqualified name into a silent wrong pick.
//   - `gltfx_duration_between(earlier, later)` as a NAMED free
//     function, not `operator-`: every other public function in this
//     project today is named, not an operator overload: the
//     collision-check gate's own awk scanner
//     (tests/tools/check_public_name_collision.sh) has never been
//     exercised against an operator declaration either. Staying inside
//     the pattern already proven correct is the smaller, better
//     understood surface - GODS_LAWS.md L-17's "monolito nasce por
//     conveniencia local" cuts the other way just as well against
//     "just this once, an operator is nicer to read".
//   - field names `nanoseconds` (gltfx_duration) and `ticks`
//     (gltfx_time_point), deliberately DIFFERENT words for the same
//     underlying integer scale: `gltfx_duration::nanoseconds` is a
//     real, meaningful span (SECONDS x 1e9, safe to read on its own).
//     `gltfx_time_point::ticks` is a raw reading whose only well-
//     defined operation is being compared to ANOTHER reading from the
//     SAME clock (see the struct's own comment below) - naming it
//     "nanoseconds" too would invite reading it as "nanoseconds since
//     some epoch", which no monotonic clock on any of this project's
//     five target platforms promises.
//
// EXACTLY THREE public free functions ship in this header - the
// closed set time_test.cpp's own enumeration test
// (every_public_time_operation_is_exercised) checks its printed count
// against (GODS_LAWS.md L-40): gltfx_duration_between(),
// gltfx_duration_to_seconds(), gltfx_duration_from_seconds(). A fourth
// added later means updating BOTH this sentence and that test's
// k_all_operations array, in lockstep - the same contract
// err_code_test.cpp's own header comment already documents for
// k_all_codes.
//
// THE RULE THAT STAYS, bigger than this slice (CTO decision in autonomous
// mode, adversarial review, 28/08/2026 - canonical record: DECISOES_AUTONOMAS.md,
// entry D8 - this paragraph is the precedent for every FUTURE pure
// math/conversion function this project writes, not just the two below):
//
//   A pure math or conversion function is TOTAL: deterministic,
//   saturating (the result keeps the input's DIRECTION - out-of-range
//   or +-infinity moves toward the nearest representable value on the
//   SAME side; a non-number, which has no direction, goes to zero),
//   NEVER undefined behavior, NEVER fallible. Diagnosing invalid input
//   is the INGESTION BOUNDARY's job (a file reader, a loader) - that is
//   the layer that returns through the error channel (GODS_LAWS.md
//   L-22), never a pure conversion sitting behind it.
//
// WHY THIS PROJECT CANNOT LEAN ON THE SANITIZER HERE, stated once so
// every future conversion inherits the lesson instead of relearning it:
// part of what this rule guards against happens INSIDE the system's own
// math library (std::llround, called from gltfx_duration_from_seconds()
// below) - GODS_LAWS.md L-23's ASan/UBSan gate does not instrument
// glibc's own compiled code, and (separately, an already-open finding)
// this project's sanitizer CI job does not currently fail the build
// even when it DOES trap. There is no net under either of these two
// functions: correctness has to come from the CODE, checked BEFORE any
// call into an uninstrumented library, never from a portal that might
// catch the mistake later.
//
// CORRECTION to a claim this header used to make, found and fixed the
// same day the rule above was written: an earlier revision described
// gltfx_rgba_to_srgb8() (core/color.hpp) as merely "honest" about an
// out-of-range input, as if it just let a bad value through undocumented.
// That was BACKWARDS - re-read color.hpp's own comment: that function
// CLAMPS an out-of-range linear value before encoding it, a defined,
// tested result, not silence. The saturating rule above is that same
// precedent, generalized on purpose, not a new idea introduced here.

namespace glintfx {

// An exact elapsed span of time, as an integer count of nanoseconds -
// the CANONICAL form decision 3 above names. Trivial aggregate, same
// shape as glintfx::version/glintfx::gltfx_rgba - no user-declared
// special member to default (Rule of Zero, CONTRACT.md SS2.2).
struct gltfx_duration {
    std::int64_t nanoseconds;
};

// A single reading from a monotonic clock, carried as a raw
// nanosecond tick count. Meaningful ONLY relative to another
// gltfx_time_point produced by the SAME clock: a monotonic clock has
// no defined relationship to wall-clock time, to a different process,
// or (on some platforms) to a reading taken before a machine
// suspend/resume. This pure core type intentionally does not say WHICH
// clock, or how to read one right now - see this file's own "what is
// out of scope" paragraph above.
struct gltfx_time_point {
    std::int64_t ticks;
};

// The elapsed span from `earlier` to `later`.
//
// TOTAL, per the rule above: computed in std::uint64_t - well-defined
// BY THE STANDARD for every pair of std::int64_t inputs, including the
// two swapped - then converted back to std::int64_t, also well-defined
// since C++20 mandates two's-complement (an exact bit-pattern
// reinterpretation, not implementation-defined guesswork). CORRECTED,
// adversarial review 28/08/2026: the FIRST version of this function
// computed `later.ticks - earlier.ticks` as plain SIGNED subtraction -
// reproduced live, under UBSan, as a genuine trap ("signed integer
// overflow... cannot be represented") for a CORRECTLY ORDERED pair
// whose true span exceeds what int64_t holds - see
// duration_between_extremes_of_type_are_well_defined_in_both_orders in
// time_test.cpp for the exact pair and the exact message.
//
// GUARANTEE, scoped honestly, not widened by the fix above (proved by
// time_point_difference_is_never_negative_for_monotonic_readings in
// time_test.cpp): when both readings come from the SAME monotonic
// clock, within ONE process's realistic runtime - a precondition no
// real clock can violate, since two readings 2^63 nanoseconds
// (roughly 292 years) apart are not physically obtainable from a
// running process - and `later` was read no earlier than `earlier`,
// the result is never negative. Outside that precondition (an
// adversarial pair spanning the type's full domain, or the two
// arguments swapped), the function still never crashes and always
// returns a deterministic std::int64_t, but that value is a modular
// wraparound, not a meaningful elapsed span - the same class of
// "defined but not meaningful" result unsigned integer arithmetic
// always carries outside its intended range, not a new promise this
// function invents.
[[nodiscard]] GLINTFX_API gltfx_duration gltfx_duration_between(gltfx_time_point earlier,
                                                                gltfx_time_point later) noexcept;

// The convenience conversion decision 3's "atalho" names. Reads the
// canonical count fresh from `d.nanoseconds` and divides by one
// billion - nothing here is cached or accumulated, so repeated calls
// on the SAME gltfx_duration never compound rounding error onto each
// other; each call is an independent, exact function of its argument.
// Total trivially: every std::int64_t converts to a finite double, and
// division by the nonzero constant one billion never produces NaN or
// infinity from a finite input.
[[nodiscard]] GLINTFX_API double gltfx_duration_to_seconds(gltfx_duration d) noexcept;

// The inverse convenience conversion, for a consumer that prefers to
// author a threshold or configuration value in seconds and hand it
// back to the canonical nanosecond form.
//
// TOTAL SATURATING CONVERSION, per the rule above - the signature is
// UNCHANGED (still returns gltfx_duration by value, no error channel):
//   - a value IN RANGE rounds to the nearest nanosecond (proved
//     round-trip-exact for representative spans up to one day by
//     duration_seconds_round_trip_does_not_accumulate_error in
//     time_test.cpp - a double's ~15-17 significant decimal digits
//     comfortably covers a day's worth of nanoseconds, 14 digits);
//   - a value OUT OF RANGE, or +-infinity - both HAVE A DIRECTION -
//     saturates to the closest representable gltfx_duration on that
//     SAME side (std::numeric_limits<std::int64_t>::max()/min());
//   - NaN - the one double value with NO direction, every comparison
//     against it is false by IEEE 754 - returns a zero gltfx_duration.
//
// CORRECTED, adversarial review 28/08/2026: the FIRST version of this
// function called std::llround(seconds * 1e9) unconditionally. For
// NaN, +-infinity, or a magnitude the scaled product cannot hold,
// glibc's std::llround - an UNINSTRUMENTED system math-library call,
// see the rule above - returned the SAME sentinel bit pattern
// (std::numeric_limits<std::int64_t>::min()) for every one of those
// cases: silent, direction-blind, apparent success. Reproduced live
// before the fix - see
// duration_from_seconds_is_a_total_saturating_conversion in
// time_test.cpp for the exact inputs and this exact number.
//
// THE ORDER OF OPERATIONS IS THE FIX, not an implementation detail:
// the range check compares the scaled value against the two range
// bounds BY COMPARISON, strictly BEFORE any call to std::llround(). A
// NaN input fails every one of those comparisons (IEEE 754: no
// comparison against NaN is ever true) and falls straight to the zero
// branch - std::llround() is therefore NEVER called with an invalid
// argument, for any input this function can receive. Reordering these
// checks after the rounding call reopens the exact defect this
// paragraph describes.
[[nodiscard]] GLINTFX_API gltfx_duration gltfx_duration_from_seconds(double seconds) noexcept;

} // namespace glintfx
