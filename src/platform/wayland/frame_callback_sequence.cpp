// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/wayland/frame_callback_sequence.hpp"

namespace glintfx::platform {

void frame_callback_sequence::mark_frame_done() noexcept { m_pending = false; }

void frame_callback_sequence::arm_pending() noexcept { m_pending = true; }

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

} // namespace glintfx::platform
