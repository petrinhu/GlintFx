// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <concepts>
#include <cstdint>

#include <glintfx/platform/gl/context.hpp>

#include "platform/wayland/frame_callback_sequence.hpp"

// platform/wayland/frame_callback_wait.hpp - WL-FRAME-ORDER-ARM-TEST (D-W8-227): the bounded wait
// for THIS surface's frame callback, kept apart from the socket and the clock so a test can show
// the wait ends when the budget does. The adapter (egl_context_adapter.cpp, swap_buffers()) binds
// the two ports to steady_clock and poll_and_dispatch_with_budget();
// tests/frame_callback_wait_test.cpp binds them to a script.
//
// No EGL and no Wayland type is reachable from this header, the same rule as
// frame_callback_sequence.hpp: it registers unguarded and runs on every system.

namespace glintfx::platform {

template <class Ports>
concept frame_wait_ports = requires(Ports &ports, std::uint32_t budget_ms) {
    // A monotonic clock, in nanoseconds.
    { ports.now_ns() } noexcept -> std::same_as<std::int64_t>;
    // One poll plus dispatch of the default queue, for at most budget_ms; false only when the
    // connection is now unusable.
    { ports.poll_for(budget_ms) } noexcept -> std::same_as<bool>;
};

// WL-FRAME-WAIT (D-W8-213): waits for THIS surface's frame callback, never for "any event". One
// poll returns after the first batch of events read from the socket, and that batch is often not
// ours: Mesa's wl_buffer.release lands on Mesa's private queue but wakes the same fd, and the
// callback was still pending - every other frame became skipped_hidden (loop_parity_test
// measured rendered_frames=15 of 30). So it polls again, with what is left of the ONE budget,
// until `sequence` says the callback arrived or the budget ends, and then it returns: the budget
// is the ceiling of this wait (D-W8-213). Returns false only when a poll found the connection
// unusable.
template <frame_wait_ports Ports>
[[nodiscard]] bool wait_for_frame_callback(Ports &ports, const frame_callback_sequence &sequence,
                                           std::uint32_t budget_ms) noexcept {
    const std::int64_t deadline_ns =
        ports.now_ns() + static_cast<std::int64_t>(budget_ms) * 1'000'000;
    while (sequence.decide_after_wait() == gltfx_present_outcome::skipped_hidden) {
        const std::int64_t remaining_ms = (deadline_ns - ports.now_ns()) / 1'000'000;
        if (remaining_ms <= 0) {
            return true;
        }
        if (!ports.poll_for(static_cast<std::uint32_t>(remaining_ms))) {
            return false;
        }
    }
    return true;
}

} // namespace glintfx::platform
