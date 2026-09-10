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

// platform/win32/device_change_message.hpp - SF-1 of WIN-SEAT's
// 09/09/2026 reopening (/var/tmp/glintfx-plan/win-seat.md sec. 3,
// D-090918 in DECISOES_AUTONOMAS.md): the PURE half of classifying a
// WM_DEVICECHANGE message win32_seat_adapter's own window procedure
// (seat_adapter.cpp) receives after RegisterDeviceNotificationW - split
// out as its own atom (GODS_LAWS.md L-17) so a test can feed a
// SYNTHETIC (WPARAM, DEV_BROADCAST_HDR*) pair, no window, no live
// registration, the same "prove the pure half on its own" role win32_
// seat_adapter::translate() already plays for the device-list-to-
// capabilities half of this same adapter.
//
// THE TWO GUARDS THIS FUNCTION APPLIES, both load-bearing
// (win-seat.md's own sec. 1.3): (1) `header` may be nullptr - the
// system's own BROADCAST notifications (e.g. DBT_DEVNODES_CHANGED)
// carry no DEV_BROADCAST_HDR block at all, only the REGISTERED
// notification this adapter asked for via RegisterDeviceNotificationW
// does; (2) even a non-null header may name a DIFFERENT broadcast type
// (DBT_DEVTYP_VOLUME, DBT_DEVTYP_PORT, ...) than the interface-class
// notification this adapter registered for - only
// DBT_DEVTYP_DEVICEINTERFACE is this adapter's own concern.
namespace glintfx::platform {

enum class device_change_kind : std::uint8_t {
    none,
    arrival,
    removal,
};

// `header` is read ONLY as a DEV_BROADCAST_HDR (never as the larger
// DEV_BROADCAST_DEVICEINTERFACE_W it actually points to in production -
// this function never reads past dbch_devicetype, the same "read only
// the common header before trusting a more specific one" discipline
// learn.microsoft.com/windows/win32/devio/registering-for-device-
// notification's own example applies).
[[nodiscard]] device_change_kind classify_device_change(WPARAM wparam, const void *header) noexcept;

} // namespace glintfx::platform

#endif // defined(_WIN32)
