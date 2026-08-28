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

// The elapsed span from `earlier` to `later`. GUARANTEE (proved by
// time_point_difference_is_never_negative_for_monotonic_readings in
// time_test.cpp): when both readings come from the SAME monotonic
// clock and `later` was read no earlier than `earlier` - the only
// order a monotonic clock's own contract allows - the result is never
// negative. Calling this with the two arguments swapped is a caller
// error this function does not guard against: it is ordinary integer
// subtraction, nothing more, the same honesty gltfx_rgba_to_srgb8's
// own header comment already applies to an out-of-range input.
[[nodiscard]] GLINTFX_API gltfx_duration gltfx_duration_between(gltfx_time_point earlier,
                                                                gltfx_time_point later) noexcept;

// The convenience conversion decision 3's "atalho" names. Reads the
// canonical count fresh from `d.nanoseconds` and divides by one
// billion - nothing here is cached or accumulated, so repeated calls
// on the SAME gltfx_duration never compound rounding error onto each
// other; each call is an independent, exact function of its argument.
[[nodiscard]] GLINTFX_API double gltfx_duration_to_seconds(gltfx_duration d) noexcept;

// The inverse convenience conversion, for a consumer that prefers to
// author a threshold or configuration value in seconds and hand it
// back to the canonical nanosecond form. Rounds to the nearest
// nanosecond (proved round-trip-exact for representative spans up to
// one day by duration_seconds_round_trip_does_not_accumulate_error in
// time_test.cpp - a double's ~15-17 significant decimal digits comfortably
// covers a day's worth of nanoseconds, 14 digits).
[[nodiscard]] GLINTFX_API gltfx_duration gltfx_duration_from_seconds(double seconds) noexcept;

} // namespace glintfx
