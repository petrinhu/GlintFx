// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/wayland/window_configure_sequence.hpp"

namespace glintfx::platform {

window_configure_result
window_configure_sequence::apply_configure(std::uint32_t width, std::uint32_t height,
                                           std::span<const std::int32_t> states,
                                           std::uint32_t serial) noexcept {
    window_configure_result result;
    result.has_size = (width != 0 && height != 0);
    if (result.has_size) {
        result.width = width;
        result.height = height;
    }

    for (const std::int32_t state : states) {
        if (state == k_xdg_toplevel_state_maximized) {
            result.maximized = true;
        } else if (state == k_xdg_toplevel_state_fullscreen) {
            result.fullscreen = true;
        } else if (state == k_xdg_toplevel_state_activated) {
            result.activated = true;
        } else if (state == k_xdg_toplevel_state_suspended) {
            result.suspended = true;
        }
        // Any other code (resizing, tiled_*, constrained_*, or a code a
        // future protocol version defines) is deliberately ignored -
        // see this file's own header comment.
    }

    m_pending_serial = serial;
    return result;
}

std::optional<std::uint32_t> window_configure_sequence::take_serial_to_ack() noexcept {
    const std::optional<std::uint32_t> serial = m_pending_serial;
    m_pending_serial.reset();
    return serial;
}

} // namespace glintfx::platform
