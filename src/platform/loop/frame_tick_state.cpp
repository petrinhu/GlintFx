// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/loop/frame_tick_state.hpp"

namespace glintfx::platform {

gltfx_frame_tick compute_frame_tick(gltfx_time_point previous_now, gltfx_time_point now,
                                    std::uint64_t previous_frame_index,
                                    gltfx_present_outcome last_present,
                                    bool probe_says_visible) noexcept {
    // gltfx_duration_between() (core/time.hpp) already answers "never
    // negative" for a clock stepping backward - this atom only adds
    // the OTHER clamp that function has no opinion about (a genuine
    // stall, or a resume-from-suspend, larger than
    // k_gltfx_max_frame_elapsed - platform/loop/loop.hpp's own comment
    // on that constant).
    const gltfx_duration raw_elapsed = gltfx_duration_between(previous_now, now);
    std::int64_t elapsed_ns = raw_elapsed.nanoseconds;
    if (elapsed_ns < 0) {
        elapsed_ns = 0;
    }
    if (elapsed_ns > k_gltfx_max_frame_elapsed.nanoseconds) {
        elapsed_ns = k_gltfx_max_frame_elapsed.nanoseconds;
    }

    // P5's third rule (LOOP-FIRST-TICK-ELAPSED, 13/09/2026, D-091306):
    // zero on the first tick, BY INDEX, never by clock. When
    // previous_frame_index == 0 there is no previous tick to measure
    // from, so elapsed is zero WHATEVER previous_now holds - this
    // wins over both clamps above. The wall time between
    // gltfx_loop::open() and the first step() is the consumer's own
    // loading time (image, sound, map) and must never reach its first
    // physics integration as one large step - measured live
    // 13/09/2026 by tests/parity/loop_parity_test.cpp's own
    // elapsed_zero_no_primeiro_tique check, after the facade had been
    // stamping that instant inside open() (src/platform/loop/
    // loop_facade.cpp, removed in this same commit).
    if (previous_frame_index == 0) {
        elapsed_ns = 0;
    }

    // P4, exactly (this header's own top comment carries the formula
    // and the two implications it proves): `last_present` is copied
    // through untouched, on the SAME line, so a reviewer can see
    // neither field infers the other.
    const bool should_render =
        (last_present != gltfx_present_outcome::skipped_hidden) || probe_says_visible;

    return gltfx_frame_tick{
        .elapsed = gltfx_duration{.nanoseconds = elapsed_ns},
        .now = now,
        .frame_index = previous_frame_index + 1,
        .should_render = should_render,
        .last_present = last_present,
    };
}

} // namespace glintfx::platform
