// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/wayland/flush_retry_policy.hpp"

namespace glintfx::platform {

bool flush_write_wait_is_fatal(std::uint32_t budget_ms, bounded_wait_outcome outcome) noexcept {
    if (outcome == bounded_wait_outcome::poll_failed) {
        return true;
    }
    return budget_ms != 0;
}

} // namespace glintfx::platform
