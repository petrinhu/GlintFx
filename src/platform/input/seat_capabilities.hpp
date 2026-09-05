// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

// platform/input/seat_capabilities.hpp - S-A' (docs/plano-w6a-
// janela.md fatia 4): the one set of input-device-class facts both
// seat mechanisms feed and every caller reads - the same "one shared
// value type, no #if" shape window_state.hpp uses for the window's own
// state (GODS_LAWS.md L-04).
//
// Wayland's wl_seat.capabilities event and Win32's
// GetRawInputDeviceList + SM_DIGITIZER answer the SAME three
// questions - does this seat have a pointer, a keyboard, a
// touch/digitizer surface - through two completely different
// mechanisms (a bitmask event vs. an enumerated device list); this
// type is where that difference stops mattering to any caller. S-B
// (Wayland, fatia 12) and Y-1 (Win32, fatia 13) each translate their
// own mechanism into calls to set_capability() below; neither is
// written by this fatia.

namespace glintfx::platform {

enum class seat_capability : std::uint8_t {
    pointer,
    keyboard,
    touch,
};

class seat_capabilities {
  public:
    seat_capabilities() noexcept = default;

    void set_capability(seat_capability capability, bool present) noexcept;

    [[nodiscard]] bool has_capability(seat_capability capability) const noexcept;

  private:
    bool m_pointer = false;
    bool m_keyboard = false;
    bool m_touch = false;
};

} // namespace glintfx::platform
