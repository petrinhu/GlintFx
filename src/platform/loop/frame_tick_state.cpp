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
