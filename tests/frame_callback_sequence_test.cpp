// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstdint>
#include <print>

#include <glintfx/platform/gl/context.hpp>

#include "platform/wayland/frame_callback_sequence.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// frame_callback_sequence_test.cpp - W-EGL (docs/plano-w6b-placa-e-
// laco.md fatia 3, D-W6b-6, GODS_LAWS.md L-20): the TDD red/green
// witness for glintfx::platform::frame_callback_sequence (src/
// platform/wayland/frame_callback_sequence.hpp) - the exact five cases
// the plan's own fatia 3 row names, driven "com proxy nulo": no
// wl_callback, no EGLDisplay, no socket anywhere in this file.
//
// RED, SEEN: before frame_callback_sequence.{hpp,cpp} existed, this
// file's own #include line failed to compile.

using glintfx::gltfx_present_outcome;
using glintfx::platform::frame_callback_sequence;
using glintfx::platform::frame_wait_plan;

GLINTFX_TEST(frame_callback_sequence_presents_immediately_with_nothing_pending) {
    const frame_callback_sequence sequence;
    GLINTFX_CHECK(!sequence.has_pending_callback());
    GLINTFX_CHECK(sequence.plan_before_wait(100) == frame_wait_plan::present_immediately);
}

GLINTFX_TEST(frame_callback_sequence_presents_and_rearms_when_the_callback_arrives) {
    frame_callback_sequence sequence;
    sequence.arm_pending(1'000);
    GLINTFX_CHECK(sequence.has_pending_callback());
    GLINTFX_CHECK(sequence.plan_before_wait(100) == frame_wait_plan::poll_then_decide);

    // Stands in for the adapter's own real poll()+dispatch_pending()
    // attempt actually delivering the wl_callback's `done` event.
    sequence.mark_frame_done();
    GLINTFX_CHECK(sequence.decide_after_wait() == gltfx_present_outcome::presented);

    // The adapter re-arms after every genuine present - the NEXT frame
    // starts pending again.
    sequence.arm_pending(2'000);
    GLINTFX_CHECK(sequence.has_pending_callback());
}

GLINTFX_TEST(frame_callback_sequence_degrades_to_skipped_hidden_when_the_budget_runs_out) {
    frame_callback_sequence sequence;
    sequence.arm_pending(1'000);
    GLINTFX_CHECK(sequence.plan_before_wait(100) == frame_wait_plan::poll_then_decide);

    // No mark_frame_done() call here: stands in for the adapter's own
    // real poll() timing out before the compositor ever acked.
    GLINTFX_CHECK(sequence.decide_after_wait() == gltfx_present_outcome::skipped_hidden);
    // A frame this library never presented never re-arms - the SAME
    // outstanding callback is still the one the next swap_buffers()
    // call will keep trying to drain.
    GLINTFX_CHECK(sequence.has_pending_callback());
}

GLINTFX_TEST(frame_callback_sequence_a_second_done_never_double_counts) {
    frame_callback_sequence sequence;
    sequence.arm_pending(1'000);
    sequence.mark_frame_done();
    GLINTFX_CHECK(!sequence.has_pending_callback());

    // A duplicate done() with nothing pending is a no-op - no
    // underflow, no "negative pending" state to observe.
    sequence.mark_frame_done();
    GLINTFX_CHECK(!sequence.has_pending_callback());
    GLINTFX_CHECK(sequence.plan_before_wait(100) == frame_wait_plan::present_immediately);
}

GLINTFX_TEST(frame_callback_sequence_reads_the_budget_instead_of_presuming_it) {
    frame_callback_sequence sequence;
    sequence.arm_pending(1'000);

    // The proof this fatia's own header comment names: a budget of 0
    // with a callback outstanding is decided WITHOUT ever attempting a
    // poll - give_up_without_polling, never poll_then_decide followed
    // by an immediate timeout. A mutant collapsing the two plans would
    // still produce skipped_hidden as the final OUTCOME, so this case
    // asserts the PLAN value itself.
    GLINTFX_CHECK(sequence.plan_before_wait(0) == frame_wait_plan::give_up_without_polling);
    GLINTFX_CHECK(sequence.has_pending_callback());
}

// The three cases D-W6b-58/fatia 7's own plan names (docs/plano-w6b-
// fatias-6-8.md sec. 8.2): pending_older_than() is the SECOND criterion
// present_would_skip() (gl_context_adapter_port.hpp) consults, and this
// is its own vermelho/verde witness, isolated from any real clock or
// socket - the caller (egl_context_adapter.cpp) is the one that ever
// reads a real std::chrono::steady_clock, this atom only compares two
// integers it was handed.
GLINTFX_TEST(frame_callback_sequence_armed_now_is_not_old) {
    frame_callback_sequence sequence;
    sequence.arm_pending(1'000'000'000); // 1s, arbitrary origin
    // now_ns == armed_at_ns: zero elapsed, never "older than" any
    // positive budget.
    GLINTFX_CHECK(!sequence.pending_older_than(1'000'000'000, 100));
}

GLINTFX_TEST(frame_callback_sequence_armed_150ms_ago_is_old_at_a_100ms_budget) {
    frame_callback_sequence sequence;
    constexpr std::int64_t k_armed_at_ns = 1'000'000'000;
    sequence.arm_pending(k_armed_at_ns);
    constexpr std::int64_t k_150ms_later_ns = k_armed_at_ns + 150'000'000;
    GLINTFX_CHECK(sequence.pending_older_than(k_150ms_later_ns, 100));
    // The mirror case, same pair, smaller budget: still not old at 40ms
    // elapsed against a 100ms budget - the boundary itself, not just
    // "eventually true", is what a mutant collapsing >= to > or the
    // multiplication by 1'000'000 to 1'000 would fail differently on.
    constexpr std::int64_t k_40ms_later_ns = k_armed_at_ns + 40'000'000;
    GLINTFX_CHECK(!sequence.pending_older_than(k_40ms_later_ns, 100));
}

GLINTFX_TEST(frame_callback_sequence_mark_frame_done_zeroes_the_age) {
    frame_callback_sequence sequence;
    sequence.arm_pending(1'000'000'000);
    sequence.mark_frame_done();
    // Nothing pending at all - pending_older_than() answers false
    // regardless of how far `now_ns` has drifted from the stale
    // armed-at instant this atom no longer considers meaningful.
    GLINTFX_CHECK(!sequence.pending_older_than(1'000'000'000 + 999'000'000'000, 100));

    std::println(
        "frame_callback_sequence_test: 8/8 cases of this file's own closed enumeration passed");
}
