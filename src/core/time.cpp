// SPDX-License-Identifier: AGPL-3.0-or-later
#include <glintfx/core/time.hpp>

#include <cmath>

// core/time.cpp - implementation of CORE-TIME (see time.hpp's own
// header comment for the frozen decision and the naming rationale).
// Pure arithmetic on the two value types above; no OS call anywhere in
// this file (GODS_LAWS.md L-19).

namespace glintfx {

namespace {

// One billion: the nanosecond<->second scale factor, named once so
// the two conversion functions below cannot drift apart by using two
// different literals for what is the same constant (GODS_LAWS.md
// L-17: a magic number repeated in two places is the same class of
// duplication a repeated function body would be).
constexpr double k_nanoseconds_per_second = 1'000'000'000.0;

} // namespace

gltfx_duration gltfx_duration_between(gltfx_time_point earlier, gltfx_time_point later) noexcept {
    return gltfx_duration{.nanoseconds = later.ticks - earlier.ticks};
}

double gltfx_duration_to_seconds(gltfx_duration d) noexcept {
    return static_cast<double>(d.nanoseconds) / k_nanoseconds_per_second;
}

gltfx_duration gltfx_duration_from_seconds(double seconds) noexcept {
    // std::llround (round-half-away-from-zero), not a truncating cast:
    // the round trip this function's own header comment promises
    // depends on rounding to the NEAREST nanosecond, not discarding
    // the fractional one - see time_test.cpp's own round-trip case for
    // the samples this was verified against, independently, in Python,
    // before this line was written.
    return gltfx_duration{
        .nanoseconds = static_cast<std::int64_t>(std::llround(seconds * k_nanoseconds_per_second))};
}

} // namespace glintfx
