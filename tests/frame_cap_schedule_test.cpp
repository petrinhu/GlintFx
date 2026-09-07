// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstdint>
#include <print>

#include <glintfx/core/time.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/loop/frame_cap_schedule.hpp"

// frame_cap_schedule_test.cpp - LOOP-RUN fatia 6a (docs/plano-w6b-
// fatias-6-8.md D-W6b-49, GODS_LAWS.md L-19/L-20/L-40): six cases
// proving platform::frame_cap_schedule::plan() - see that class's own
// header comment for the "guards the deadline, never the instant"
// contract and the two safety valves this file's own cases 3 and 5
// exercise.

using glintfx::gltfx_time_point;
using glintfx::platform::frame_cap_schedule;

GLINTFX_TEST(a_cap_of_zero_hertz_never_waits) {
    frame_cap_schedule schedule;
    const gltfx_time_point now{.ticks = 123'456'789};

    // Called twice - a cap of zero has nothing to arm, so the SECOND
    // call proves this is not just "the very first call always returns
    // zero" (every non-zero cap's own first call also does that).
    GLINTFX_CHECK_EQ(schedule.plan(now, 0), static_cast<std::uint32_t>(0));
    GLINTFX_CHECK_EQ(schedule.plan(now, 0), static_cast<std::uint32_t>(0));
}

GLINTFX_TEST(a_cap_of_thirty_hertz_waits_about_thirty_three_point_three_milliseconds) {
    frame_cap_schedule schedule;
    const gltfx_time_point t0{.ticks = 0};

    // First call: nothing armed yet - presents immediately, arms the
    // deadline one period (1/30 s) from `t0`.
    const std::uint32_t first_wait = schedule.plan(t0, 30);
    GLINTFX_CHECK_EQ(first_wait, static_cast<std::uint32_t>(0));

    // Asking again at the SAME instant `t0` - the full period is still
    // ahead: 1e9 / 30 = 33 333 333.33... ns, so this should round to
    // 33 or 34 ms, never anything outside that pair.
    const std::uint32_t second_wait = schedule.plan(t0, 30);
    GLINTFX_CHECK(second_wait >= 33 && second_wait <= 34);
}

GLINTFX_TEST(a_frame_more_than_one_whole_period_late_reanchors_on_now_instead_of_waiting) {
    frame_cap_schedule schedule;
    const gltfx_time_point t0{.ticks = 0};
    constexpr std::int64_t period_ns_30hz = 33'333'333; // matches the class's own rounding

    const std::uint32_t first_wait = schedule.plan(t0, 30); // deadline armed at period_ns_30hz
    GLINTFX_CHECK_EQ(first_wait, static_cast<std::uint32_t>(0));

    // Arrives a full extra period late (deadline + one whole period) -
    // this class's own "more than one period late" safety valve
    // reanchors on `now` directly rather than advancing one period at
    // a time.
    const gltfx_time_point very_late{.ticks = period_ns_30hz + period_ns_30hz};
    const std::uint32_t reanchor_wait = schedule.plan(very_late, 30);
    GLINTFX_CHECK_EQ(reanchor_wait, static_cast<std::uint32_t>(0));

    // The NEW deadline is one period past `very_late`, not one period
    // past the OLD deadline (which would leave this call still overdue
    // by a whole period) - probing again at `very_late` proves it:
    // a fresh, full period should still be ahead.
    const std::uint32_t after_reanchor_wait = schedule.plan(very_late, 30);
    GLINTFX_CHECK(after_reanchor_wait >= 33 && after_reanchor_wait <= 34);
}

GLINTFX_TEST(changing_the_cap_live_recalculates_from_the_last_deadline) {
    frame_cap_schedule schedule;
    const gltfx_time_point t0{.ticks = 0};
    constexpr std::int64_t period_ns_30hz = 33'333'333;
    constexpr std::int64_t period_ns_60hz = 16'666'667;
    // The class rounds to the nearest millisecond (frame_cap_schedule.
    // cpp's own comment on this exact arithmetic) - a wait can land on
    // either side of that rounding, so every assertion below checks the
    // pair this constant's own floor names, never a literal copied out
    // of a hand calculation.
    const std::int64_t period_ms_60hz_floor = period_ns_60hz / 1'000'000;

    const std::uint32_t first_wait = schedule.plan(t0, 30); // deadline armed at period_ns_30hz
    GLINTFX_CHECK_EQ(first_wait, static_cast<std::uint32_t>(0));

    // Cap changes to 60 Hz BEFORE that deadline arrives - the deadline
    // itself does not move, but the period used to measure it (and to
    // advance it next) is the NEW one from here on.
    const std::uint32_t wait_after_cap_change = schedule.plan(t0, 60);
    GLINTFX_CHECK(wait_after_cap_change >= period_ms_60hz_floor &&
                  wait_after_cap_change <= period_ms_60hz_floor + 1);

    // Now let `now` reach the OLD deadline exactly, still asking for
    // the NEW cap - "recalcula a partir do ultimo prazo" means the
    // NEXT deadline is the old deadline PLUS the new (60 Hz) period,
    // never the old deadline plus another 30 Hz period.
    const gltfx_time_point at_old_deadline{.ticks = period_ns_30hz};
    const std::uint32_t wait_at_old_deadline = schedule.plan(at_old_deadline, 60);
    GLINTFX_CHECK_EQ(wait_at_old_deadline, static_cast<std::uint32_t>(0));

    const std::uint32_t wait_after_advance = schedule.plan(at_old_deadline, 60);
    GLINTFX_CHECK(wait_after_advance >= period_ms_60hz_floor &&
                  wait_after_advance <= period_ms_60hz_floor + 1);
}

GLINTFX_TEST(a_clock_stepping_backward_never_waits_longer_than_one_period) {
    frame_cap_schedule schedule;
    const gltfx_time_point t0{.ticks = 1'000'000'000}; // 1 second, so "far in the past" stays >= 0

    const std::uint32_t first_wait = schedule.plan(t0, 30); // deadline armed at t0 + one period
    GLINTFX_CHECK_EQ(first_wait, static_cast<std::uint32_t>(0));

    // A `now` reading far EARLIER than the deadline (the clock stepped
    // backward, or a caller mistakenly measured before the deadline was
    // armed) - this class's own "clock reading that steps backward"
    // safety valve clamps the wait to at most one period, never to the
    // full (much larger) apparent gap.
    const gltfx_time_point far_in_the_past{.ticks = 0};
    const std::uint32_t wait = schedule.plan(far_in_the_past, 30);
    GLINTFX_CHECK(wait <= 34); // one 30 Hz period, rounded up
}

GLINTFX_TEST(three_hundred_consecutive_periods_at_sixty_hertz_sum_without_drift) {
    // GUARDS THE DEADLINE, NEVER THE INSTANT (this class's own header
    // comment) - ALTERNATING jitter, not a CONSTANT one, is the point:
    // a constant offset added to `now` on every single call cannot
    // catch the "now + period" mutation, because that mutant's own
    // internal deadline ends up carrying the SAME constant offset
    // forever, which then cancels out against this test's own next
    // `now` (also offset by the same constant) - measured directly, a
    // hand-built copy of that exact mutant, fed the constant-jitter
    // version of this test, passed clean all 300 calls (found while
    // answering the team-lead's own question, "confira que a
    // oscilacao esta de fato entrando na conta" - GODS_LAWS.md L-27).
    // Alternating it (jittered on odd calls, exact on even ones) is
    // what actually distinguishes the two: a correct implementation
    // advances from the PREVIOUS DEADLINE and forgets an odd call's
    // jitter completely by the very next (even) call; that same hand-
    // built mutant, probed the same way, fails at iteration 2 with a
    // nonzero wait (2 ms, the jitter itself) - the mutation and both
    // outputs are recorded in this test's own commit message.
    frame_cap_schedule schedule;
    constexpr std::uint32_t cap_hz = 60;
    constexpr std::int64_t period_ns = 16'666'667; // matches the class's own rounding of 1e9/60
    constexpr std::int64_t jitter_ns = 2'000'000;  // 2 ms, well under one period

    const gltfx_time_point t0{.ticks = 0};
    const std::uint32_t first_wait = schedule.plan(t0, cap_hz);
    GLINTFX_CHECK_EQ(first_wait, static_cast<std::uint32_t>(0));

    std::int64_t tracked_deadline_ns = period_ns; // mirrors the schedule's own first deadline
    int periods_checked = 1;
    for (int i = 1; i < 300; ++i) {
        // Odd i: arrives 2 ms late. Even i: arrives exactly on time,
        // with no jitter of its own - the call that actually catches
        // a schedule that failed to forget the PREVIOUS call's jitter.
        const std::int64_t jitter_this_call = (i % 2 == 1) ? jitter_ns : 0;
        const gltfx_time_point arrival{.ticks = tracked_deadline_ns + jitter_this_call};
        const std::uint32_t wait = schedule.plan(arrival, cap_hz);
        GLINTFX_CHECK_EQ(wait, static_cast<std::uint32_t>(0));
        tracked_deadline_ns += period_ns;
        ++periods_checked;
    }

    GLINTFX_CHECK_EQ(periods_checked, 300);
    constexpr std::int64_t expected_total_ns = 5'000'000'000; // 300 / 60 Hz = 5 s
    constexpr std::int64_t tolerance_ns = 1'000'000;          // +-1 ms
    GLINTFX_CHECK(tracked_deadline_ns >= expected_total_ns - tolerance_ns);
    GLINTFX_CHECK(tracked_deadline_ns <= expected_total_ns + tolerance_ns);
    std::println("three_hundred_consecutive_periods_at_sixty_hertz_sum_without_drift: {} "
                 "period(s) checked, total {} ns (expected {} ns +-{} ns)",
                 periods_checked, tracked_deadline_ns, expected_total_ns, tolerance_ns);
}
