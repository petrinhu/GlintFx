// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <cstdint>
#include <print>

#include <glintfx/platform/gl/context.hpp>
#include <glintfx/platform/loop/loop.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/loop/frame_tick_state.hpp"

// frame_tick_state_test.cpp - LOOP-RUN fatia 6a (docs/plano-w6b-
// fatias-6-8.md D-W6b-42/44/45, GODS_LAWS.md L-19/L-20/L-40), plus
// LOOP-FIRST-TICK-ELAPSED (13/09/2026): twelve cases proving
// platform::compute_frame_tick() - see that atom's own header comment
// for the P4 formula this file exercises directly, and docs/plano-
// w6b-fatias-6-8.md finding F4 for why the plain equality an earlier
// draft asserted (should_render == (last_present != skipped_hidden))
// is wrong: it never lets should_render recover once a window has
// been hidden.
//
// What this file proves about the FIRST tick is the atom's rule only
// (previous_frame_index == 0 => elapsed == 0, regardless of
// previous_now); that the facade actually FEEDS the atom an index of
// 0 on gltfx_loop's own first step() is proven live by
// tests/parity/loop_parity_test.cpp's own elapsed_zero_no_primeiro_
// tique check, never here.

using glintfx::gltfx_present_outcome;
using glintfx::gltfx_time_point;
using glintfx::k_gltfx_max_frame_elapsed;
using glintfx::platform::compute_frame_tick;

GLINTFX_TEST(first_tick_reports_zero_elapsed_and_frame_index_one) {
    // LOOP-FIRST-TICK-ELAPSED (13/09/2026, D-091306): until this
    // change, this case fed compute_frame_tick() two EQUAL instants
    // and asserted elapsed == 0 - that only proves "equal instants
    // give zero", never "the first tick gives zero" (P5, platform/
    // loop/loop.hpp:92,332-334). The facade stamps previous_now inside
    // gltfx_loop::open() (src/platform/loop/loop_facade.cpp), so the
    // real first loop_step() measures open()-to-step(), which can be
    // seconds - and no case here could see that, because none fed a
    // previous_now genuinely far from now. This case now does: three
    // seconds apart, same shape the live facade produces.
    const gltfx_time_point previous{.ticks = 1'000};
    const gltfx_time_point now{.ticks = 3'000'001'000}; // 3 s later

    const auto tick = compute_frame_tick(previous, now, 0, gltfx_present_outcome::presented, true);

    GLINTFX_CHECK_EQ(tick.elapsed.nanoseconds, static_cast<std::int64_t>(0));
    GLINTFX_CHECK_EQ(tick.frame_index, static_cast<std::uint64_t>(1));
}

GLINTFX_TEST(sixteen_milliseconds_of_real_elapsed_time_are_reported_exactly) {
    // index 1: a SUBSEQUENT tick - index 0 is the first tick and
    // reports zero unconditionally by P5 (see the case above), so a
    // case proving the real-clock math needs a previous tick to
    // measure from.
    const gltfx_time_point previous{.ticks = 0};
    const gltfx_time_point now{.ticks = 16'000'000}; // 16 ms in nanoseconds

    const auto tick = compute_frame_tick(previous, now, 1, gltfx_present_outcome::presented, true);

    GLINTFX_CHECK_EQ(tick.elapsed.nanoseconds, static_cast<std::int64_t>(16'000'000));
}

GLINTFX_TEST(a_clock_stepping_backward_reports_zero_elapsed_never_negative) {
    // index 1: with index 0 the zero would come from the first-tick
    // rule, not from the clamp this case exists to prove - it would
    // pass for the wrong reason (verde por acidente).
    const gltfx_time_point previous{.ticks = 1'000};
    const gltfx_time_point now{.ticks = 500}; // earlier than `previous`

    const auto tick = compute_frame_tick(previous, now, 1, gltfx_present_outcome::presented, true);

    GLINTFX_CHECK_EQ(tick.elapsed.nanoseconds, static_cast<std::int64_t>(0));
}

GLINTFX_TEST(three_seconds_elapsed_clamps_to_the_public_max_frame_elapsed_constant) {
    // Read straight from the public constant (platform/loop/loop.hpp),
    // never a literal 250'000'000 copied into this file - the same
    // "the value the header names, not a copy of it" discipline
    // window_state_test.cpp's own dpi_144 case already applies to a
    // different constant.
    //
    // index 1: a subsequent tick - with index 0 this would assert the
    // first-tick rule, not the ceiling clamp this case names.
    const gltfx_time_point previous{.ticks = 0};
    const gltfx_time_point now{.ticks = 3'000'000'000}; // 3 seconds

    const auto tick = compute_frame_tick(previous, now, 1, gltfx_present_outcome::presented, true);

    GLINTFX_CHECK_EQ(tick.elapsed.nanoseconds, k_gltfx_max_frame_elapsed.nanoseconds);
}

GLINTFX_TEST(first_tick_ignores_previous_now_entirely) {
    // P5's third rule is "zero on the first tick, BY INDEX, never by
    // clock" (frame_tick_state.hpp's own header, LOOP-FIRST-TICK-
    // ELAPSED): previous_now must not matter AT ALL when previous_
    // frame_index == 0. The small, closed space (L-17 global) is
    // enumerated whole, never sampled: past, just-before-now, and
    // future previous_now values, all against the SAME now.
    const gltfx_time_point now{.ticks = 5'000'000'000};
    const std::array<gltfx_time_point, 3> previous_values = {
        gltfx_time_point{.ticks = 0},                         // long past
        gltfx_time_point{.ticks = now.ticks - 1'000'000},     // 1 ms before now
        gltfx_time_point{.ticks = now.ticks + 1'000'000'000}, // 1 s in the future
    };

    std::size_t verified = 0;
    for (const gltfx_time_point &previous : previous_values) {
        const auto tick =
            compute_frame_tick(previous, now, 0, gltfx_present_outcome::presented, true);
        GLINTFX_CHECK_EQ(tick.elapsed.nanoseconds, static_cast<std::int64_t>(0));
        ++verified;
    }
    std::println("first_tick_ignores_previous_now_entirely: {} de {} verificados", verified,
                 previous_values.size());
}

GLINTFX_TEST(frame_index_always_advances_by_exactly_one_from_the_previous_tick) {
    const gltfx_time_point previous{.ticks = 0};
    const gltfx_time_point now{.ticks = 1'000};

    const auto tick = compute_frame_tick(previous, now, 41, gltfx_present_outcome::presented, true);

    GLINTFX_CHECK_EQ(tick.frame_index, static_cast<std::uint64_t>(42));
}

GLINTFX_TEST(last_present_presented_always_forces_should_render_true) {
    // P-a (docs/plano-w6b-fatias-6-8.md sec. 3.2): `presented` alone
    // decides it - the probe's own answer never matters when the last
    // present() genuinely presented.
    const gltfx_time_point previous{.ticks = 0};
    const gltfx_time_point now{.ticks = 1'000};

    const auto with_probe_visible =
        compute_frame_tick(previous, now, 0, gltfx_present_outcome::presented, true);
    const auto with_probe_hidden =
        compute_frame_tick(previous, now, 0, gltfx_present_outcome::presented, false);

    GLINTFX_CHECK(with_probe_visible.should_render);
    GLINTFX_CHECK(with_probe_hidden.should_render);
}

GLINTFX_TEST(skipped_hidden_with_the_probe_still_saying_hidden_gives_should_render_false) {
    const gltfx_time_point previous{.ticks = 0};
    const gltfx_time_point now{.ticks = 1'000};

    const auto tick = compute_frame_tick(previous, now, 0, gltfx_present_outcome::skipped_hidden,
                                         /*probe_says_visible=*/false);

    GLINTFX_CHECK(!tick.should_render);
    GLINTFX_CHECK(tick.last_present == gltfx_present_outcome::skipped_hidden);
}

GLINTFX_TEST(skipped_hidden_with_the_probe_now_saying_visible_recovers_should_render) {
    // THE RECOVERY (docs/plano-w6b-fatias-6-8.md finding F4/D-W6b-45):
    // the exact case the OLD plain-equality formula could never
    // produce - last_present is still skipped_hidden (nothing has
    // presented yet this tick), but the probe now says the window is
    // visible again, and should_render must already be true so the
    // consumer draws THIS tick.
    const gltfx_time_point previous{.ticks = 0};
    const gltfx_time_point now{.ticks = 1'000};

    const auto tick = compute_frame_tick(previous, now, 0, gltfx_present_outcome::skipped_hidden,
                                         /*probe_says_visible=*/true);

    GLINTFX_CHECK(tick.should_render);
}

namespace {

// The closed 2x2 matrix P4 defines: {presented, skipped_hidden} x
// {probe visible, probe hidden}. GODS_LAWS.md L-40: the count is
// printed even when every cell passes.
struct cell_t {
    gltfx_present_outcome last_present;
    bool probe_says_visible;
    bool expected_should_render;
};

constexpr std::array<cell_t, 4> k_cells{{
    {gltfx_present_outcome::presented, true, true},
    {gltfx_present_outcome::presented, false, true},
    {gltfx_present_outcome::skipped_hidden, false, false},
    {gltfx_present_outcome::skipped_hidden, true, true}, // the recovery cell
}};

} // namespace

GLINTFX_TEST(last_present_is_copied_through_independent_of_should_render) {
    // The recovery cell above is the one that PROVES independence:
    // should_render == true there, and last_present == skipped_hidden
    // in the SAME tick - if this atom ever re-derived last_present
    // FROM should_render (or vice versa) instead of copying the
    // argument straight through, this is the cell that would catch it.
    std::size_t cells_checked = 0;

    for (const cell_t &cell : k_cells) {
        const gltfx_time_point previous{.ticks = 0};
        const gltfx_time_point now{.ticks = 1'000};
        const auto tick =
            compute_frame_tick(previous, now, 0, cell.last_present, cell.probe_says_visible);

        GLINTFX_CHECK(tick.should_render == cell.expected_should_render);
        GLINTFX_CHECK(tick.last_present == cell.last_present);
        ++cells_checked;
    }

    GLINTFX_CHECK_EQ(cells_checked, k_cells.size());
    std::println(
        "last_present_is_copied_through_independent_of_should_render: {} of {} cell(s) checked",
        cells_checked, k_cells.size());
}

// Two more, both requested by the team-lead's own review: "vale um
// caso extra com a ordem invertida" - the recovery cell above is
// always the LAST one the enumeration test reaches, and its two
// fields are always checked should_render-then-last_present. Neither
// order matters to a PURE function with no state carried between
// calls (compute_frame_tick reads only its own five arguments, every
// single call, and this atom carries no member state at all to have
// an "order" in the first place) - but a future edit that gave this
// atom state, or that reordered these two independent statements
// inside it, is exactly the kind of change these two cases exist to
// catch, cheaply, forever.

GLINTFX_TEST(the_should_render_matrix_still_holds_when_walked_in_reverse) {
    // Same four cells as last_present_is_copied_through_independent_
    // of_should_render above, walked BACKWARD - the recovery cell is
    // now the FIRST one this loop reaches, not the last.
    std::size_t cells_checked = 0;

    for (std::size_t i = k_cells.size(); i > 0; --i) {
        const cell_t &cell = k_cells[i - 1];
        const gltfx_time_point previous{.ticks = 0};
        const gltfx_time_point now{.ticks = 1'000};
        const auto tick =
            compute_frame_tick(previous, now, 0, cell.last_present, cell.probe_says_visible);

        // Checked in the OPPOSITE field order from the forward walk
        // above (last_present first, should_render second).
        GLINTFX_CHECK(tick.last_present == cell.last_present);
        GLINTFX_CHECK(tick.should_render == cell.expected_should_render);
        ++cells_checked;
    }

    GLINTFX_CHECK_EQ(cells_checked, k_cells.size());
    std::println("the_should_render_matrix_still_holds_when_walked_in_reverse: {} of {} cell(s) "
                 "checked",
                 cells_checked, k_cells.size());
}

GLINTFX_TEST(the_recovery_case_holds_as_a_cold_start_with_no_prior_calls) {
    // The two enumeration tests above only ever reach the recovery
    // combination (skipped_hidden + probe visible) after at least one
    // OTHER cell has already run through this same atom first. This
    // case is the recovery combination as the ONLY call this test
    // makes - previous_frame_index == 0, exactly as if this were the
    // very first tick this loop ever produced - proving the
    // independence is a property of the FORMULA itself, not something
    // that only holds once "warmed up" by prior calls.
    const gltfx_time_point previous{.ticks = 0};
    const gltfx_time_point now{.ticks = 1'000};

    const auto tick = compute_frame_tick(previous, now, 0, gltfx_present_outcome::skipped_hidden,
                                         /*probe_says_visible=*/true);

    GLINTFX_CHECK(tick.last_present == gltfx_present_outcome::skipped_hidden);
    GLINTFX_CHECK(tick.should_render);
    GLINTFX_CHECK_EQ(tick.frame_index, static_cast<std::uint64_t>(1));
}
