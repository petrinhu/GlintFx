// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/input/seat_capabilities.hpp"

namespace glintfx::platform {

void seat_capabilities::set_capability(seat_capability capability, bool present) noexcept {
    switch (capability) {
    case seat_capability::pointer:
        m_pointer = present;
        return;
    case seat_capability::keyboard:
        m_keyboard = present;
        return;
    case seat_capability::touch:
        m_touch = present;
        return;
    }
}

bool seat_capabilities::has_capability(seat_capability capability) const noexcept {
    switch (capability) {
    case seat_capability::pointer:
        return m_pointer;
    case seat_capability::keyboard:
        return m_keyboard;
    case seat_capability::touch:
        return m_touch;
    }
    return false;
}

} // namespace glintfx::platform
