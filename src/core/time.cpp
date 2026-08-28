// SPDX-License-Identifier: AGPL-3.0-or-later
#include <glintfx/core/time.hpp>

#include <cmath>
#include <limits>

// core/time.cpp - implementation of CORE-TIME (see time.hpp's own
// header comment for the frozen decision, the naming rationale, and
// the TOTAL/saturating rule both functions below implement). Pure
// arithmetic on the two value types above; no OS call anywhere in this
// file (GODS_LAWS.md L-19).

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
    // Unsigned intermediate (time.hpp's own comment on this function
    // explains WHY: plain signed subtraction is undefined behavior for
    // a pair whose true span exceeds int64_t, reproduced live under
    // UBSan before this fix). Converting a std::int64_t to
    // std::uint64_t, subtracting, and converting the std::uint64_t
    // result back to std::int64_t are BOTH well-defined by the C++20
    // standard (two's-complement is mandated), for every possible pair
    // of inputs - there is no precondition this expression itself
    // needs to hold.
    const std::uint64_t earlier_unsigned = static_cast<std::uint64_t>(earlier.ticks);
    const std::uint64_t later_unsigned = static_cast<std::uint64_t>(later.ticks);
    const std::uint64_t elapsed_unsigned = later_unsigned - earlier_unsigned;
    return gltfx_duration{.nanoseconds = static_cast<std::int64_t>(elapsed_unsigned)};
}

double gltfx_duration_to_seconds(gltfx_duration d) noexcept {
    return static_cast<double>(d.nanoseconds) / k_nanoseconds_per_second;
}

gltfx_duration gltfx_duration_from_seconds(double seconds) noexcept {
    // Scale FIRST - this is plain IEEE 754 multiplication, a hardware
    // operation, not a call into the uninstrumented system math
    // library time.hpp's own comment warns about. It is well-defined
    // for EVERY double, including NaN (NaN * anything is NaN) and
    // infinity (finite * infinity is +-infinity): nothing below this
    // line needs `seconds` itself to have been valid.
    const double scaled = seconds * k_nanoseconds_per_second;

    // The two range bounds, as the closest double to each end of
    // std::int64_t's range. static_cast<double>() of an integer is
    // always well-defined (it may round, never overflows, never traps).
    const double max_nanoseconds_as_double =
        static_cast<double>(std::numeric_limits<std::int64_t>::max());
    const double min_nanoseconds_as_double =
        static_cast<double>(std::numeric_limits<std::int64_t>::min());

    // THE ORDER IS THE FIX (time.hpp's own comment on this function
    // spells out why): three COMPARISONS, in this exact sequence,
    // strictly BEFORE the only call into <cmath> below. IEEE 754 makes
    // every comparison against NaN false, so a NaN `scaled` fails all
    // three and reaches the final branch - std::llround() is never
    // reached with an invalid argument, for any `seconds` this
    // function can receive.
    if (scaled >= max_nanoseconds_as_double) {
        // Out of range OR +infinity: both have a direction: saturate
        // toward the type's own maximum.
        return gltfx_duration{.nanoseconds = std::numeric_limits<std::int64_t>::max()};
    }
    if (scaled <= min_nanoseconds_as_double) {
        // Symmetric case: out of range OR -infinity, saturate toward
        // the type's own minimum.
        return gltfx_duration{.nanoseconds = std::numeric_limits<std::int64_t>::min()};
    }
    if (!(scaled > min_nanoseconds_as_double && scaled < max_nanoseconds_as_double)) {
        // The only value able to reach this line: NaN. Both
        // comparisons above were false for it (no direction to
        // saturate toward), and so is this one - the negation is what
        // makes NaN the value that returns true HERE, an explicit
        // "checked, and could not place you in a direction" branch,
        // not a default reached by omission.
        return gltfx_duration{.nanoseconds = 0};
    }

    // Reached only when `scaled` is proven finite and strictly inside
    // std::int64_t's range: std::llround() (round-half-away-from-zero,
    // not a truncating cast - the round trip time.hpp's own header
    // comment promises depends on rounding to the NEAREST nanosecond)
    // cannot overflow here, and its argument was never NaN or
    // infinite.
    return gltfx_duration{.nanoseconds = static_cast<std::int64_t>(std::llround(scaled))};
}

} // namespace glintfx
