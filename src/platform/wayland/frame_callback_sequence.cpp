// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/wayland/frame_callback_sequence.hpp"

namespace glintfx::platform {

void frame_callback_sequence::mark_frame_done() noexcept { m_pending = false; }

void frame_callback_sequence::arm_pending(std::int64_t now_ns) noexcept {
    m_pending = true;
    m_armed_at_ns = now_ns;
}

bool frame_callback_sequence::has_pending_callback() const noexcept { return m_pending; }

frame_wait_plan frame_callback_sequence::plan_before_wait(std::uint32_t budget_ms) const noexcept {
    if (!m_pending) {
        return frame_wait_plan::present_immediately;
    }
    if (budget_ms == 0) {
        return frame_wait_plan::give_up_without_polling;
    }
    return frame_wait_plan::poll_then_decide;
}

gltfx_present_outcome frame_callback_sequence::decide_after_wait() const noexcept {
    return m_pending ? gltfx_present_outcome::skipped_hidden : gltfx_present_outcome::presented;
}

bool frame_callback_sequence::pending_older_than(std::int64_t now_ns,
                                                 std::uint32_t budget_ms) const noexcept {
    if (!m_pending) {
        return false;
    }
    // budget_ms fits comfortably in std::int64_t nanoseconds up to
    // roughly 292 years (std::uint32_t's own full range times 1e6) -
    // never a realistic overflow risk for a budget this project ever
    // hands here (the largest today, D-W6b-44's own hidden-wait
    // budget, is three orders of magnitude smaller).
    const std::int64_t budget_ns = static_cast<std::int64_t>(budget_ms) * 1'000'000;
    return (now_ns - m_armed_at_ns) >= budget_ns;
}

} // namespace glintfx::platform
