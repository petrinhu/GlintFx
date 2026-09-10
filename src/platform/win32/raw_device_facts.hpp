// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdint>

// platform/win32/raw_device_facts.hpp - SF-3 of WIN-SEAT's 09/09/2026
// reopening (/var/tmp/glintfx-plan/win-seat.md sec. 3, D-WS-5): the ONE
// fact win32_seat_adapter::translate() needs about a raw input device
// beyond its RIM_TYPE* class - how many keys a keyboard-class device
// reports, the number the Wayland-side parity rule (this header's own
// "WHY" paragraph below) is measured against.
//
// A plain data holder, not RAWINPUTDEVICELIST itself: translate() used
// to take a `const RAWINPUTDEVICELIST *` directly, which only carries
// dwType - the key-count rule needs a SECOND fact
// (GetRawInputDeviceInfoW's own RID_DEVICE_INFO::keyboard.
// dwNumberOfKeysTotal) that win32_seat_adapter::recompute_capabilities()
// (seat_adapter.cpp) fetches with one extra syscall PER keyboard-class
// entry - never per mouse entry, which this rule does not apply to
// (this header's own "WHY" paragraph, second half).
//
// WHY A KEY COUNT AT ALL (win-seat.md sec. 1.6, D-WS-5): systemd/udev's
// own ID_INPUT_KEYBOARD rule (src/udev/udev-builtin-input_id.c) only
// grants a device the "keyboard" class if the first 32 bits of its key
// bitmask cover ESC, the number row, and Q-through-D - roughly "has a
// full keyboard's worth of keys". Without an equivalent rule on
// Windows, a mouse/receiver that exposes a handful of media keys
// enumerates as RIM_TYPEKEYBOARD and reports keyboard=1 on Windows
// while the SAME hardware reports keyboard=0 on Linux - a real,
// measured behavioral divergence (GODS_LAWS.md L-04), not a difference
// of mechanism. `keyboard_key_count == 0` (the driver did not report,
// or the query failed) counts as PRESENT - "nao invente ausencia"
// (GODS_LAWS.md L-04's own rule, seat_adapter.hpp's own predecessor
// comment already cites it) - only a device that POSITIVELY reports
// fewer than the minimum is excluded.
namespace glintfx::platform {

struct win32_raw_device_facts {
    DWORD raw_type = 0;
    std::uint32_t keyboard_key_count = 0;
};

} // namespace glintfx::platform

#endif // defined(_WIN32)
