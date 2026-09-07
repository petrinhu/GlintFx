// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/loop/frame_cap_schedule.hpp"

#include <cmath>

namespace glintfx::platform {

std::uint32_t frame_cap_schedule::plan(gltfx_time_point now, std::uint32_t cap_hz) noexcept {
    if (cap_hz == 0) {
        // No cap: nothing to wait for, and nothing to remember - a cap
        // requested later starts fresh (this class's own header
        // comment: "a cap re-enabled later starts a FRESH schedule").
        m_has_deadline = false;
        return 0;
    }

    // cap_hz is an integer Hz - 1e9 / cap_hz is not always a whole
    // number of nanoseconds (60 Hz -> 16 666 666.67 ns). Rounded to the
    // nearest nanosecond, freshly every call, never cached across a
    // live cap change - this IS "mudar o teto ao vivo recalcula": the
    // very next call already uses the new period against the SAME
    // m_next_deadline this schedule already held.
    //
    // std::llround(), never a hand-rolled "+ 0.5 then truncate" (the
    // same idiom core/time.cpp's own gltfx_duration_from_seconds()
    // already avoids, for the identical reason: it rounds the WRONG
    // way for a negative input, and clang-tidy's own bugprone-
    // incorrect-roundings check reproved this exact line live, before
    // this fix - cap_hz is unsigned here so the negative case cannot
    // occur today, but the idiom itself is banned project-wide, not
    // only where it would currently misbehave).
    const auto period_ns =
        static_cast<std::int64_t>(std::llround(1'000'000'000.0 / static_cast<double>(cap_hz)));

    if (!m_has_deadline) {
        // Nothing armed yet (either this is the very first call ever,
        // or the cap was off and just turned back on) - present this
        // frame immediately, and arm the deadline for the NEXT one
        // exactly one period from here.
        m_next_deadline = gltfx_time_point{.ticks = now.ticks + period_ns};
        m_has_deadline = true;
        return 0;
    }

    // `now` and `m_next_deadline` are both real gltfx_now() readings
    // from the SAME caller, close together in time - gltfx_duration_
    // between()'s own "well-defined but not meaningful outside a
    // realistic same-clock pair" caveat (core/time.hpp) never applies
    // here, so this signed difference behaves exactly like plain
    // subtraction: negative means the deadline has not arrived yet.
    const std::int64_t since_deadline_ns = gltfx_duration_between(m_next_deadline, now).nanoseconds;

    if (since_deadline_ns < 0) {
        // Deadline not reached yet. Clamped to at most one period even
        // if `now` is somehow far short of it (a clock that stepped
        // backward) - this header's own "clock reading that steps
        // backward" safety valve.
        const std::int64_t remaining_ns =
            (-since_deadline_ns < period_ns) ? -since_deadline_ns : period_ns;
        return static_cast<std::uint32_t>((remaining_ns + 500'000) / 1'000'000);
    }

    if (since_deadline_ns >= period_ns) {
        // Late by at least one WHOLE period beyond the deadline just
        // missed - this header's own "more than one period late"
        // safety valve: reanchor on `now` instead of advancing one
        // period at a time.
        m_next_deadline = gltfx_time_point{.ticks = now.ticks + period_ns};
        return 0;
    }

    // Deadline reached, late by less than one whole period: advance
    // from the PREVIOUS DEADLINE, never from `now` - the anti-drift
    // property this header's own top comment names.
    m_next_deadline = gltfx_time_point{.ticks = m_next_deadline.ticks + period_ns};
    return 0;
}

} // namespace glintfx::platform
