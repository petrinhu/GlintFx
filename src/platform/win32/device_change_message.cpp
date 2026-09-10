// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/win32/device_change_message.hpp"

#if defined(_WIN32)

#include <dbt.h>

// device_change_message.cpp - see device_change_message.hpp's own header
// comment for scope. classify_device_change() never reads past
// DEV_BROADCAST_HDR::dbch_devicetype - it does not need the rest of
// DEV_BROADCAST_DEVICEINTERFACE_W (the class GUID and the device path),
// only whether the block names an interface-class change at all; the
// GUID itself is checked by RegisterDeviceNotificationW's own filter at
// registration time (seat_adapter.cpp), not re-checked here.

namespace glintfx::platform {

device_change_kind classify_device_change(WPARAM wparam, const void *header) noexcept {
    // DBT_DEVNODES_CHANGED and other unregistered broadcasts carry no
    // block at all (device_change_message.hpp's own header comment,
    // guard 1) - win-seat.md sec. 1.1's own defect list is exactly what
    // this adapter stopped depending on (RIDEV_DEVNOTIFY's WM_INPUT_
    // DEVICE_CHANGE never carried this ambiguity, but it carried a
    // worse one - see seat_adapter.hpp's own top comment).
    if (header == nullptr) {
        return device_change_kind::none;
    }

    // Read only the common header (guard 2) - a DEV_BROADCAST_VOLUME or
    // DEV_BROADCAST_PORT block is a real, differently-shaped struct this
    // adapter never registered interest in and must not misread as a
    // device-interface block just because it arrived on the same
    // WM_DEVICECHANGE message.
    const auto *broadcast_header = static_cast<const DEV_BROADCAST_HDR *>(header);
    if (broadcast_header->dbch_devicetype != DBT_DEVTYP_DEVICEINTERFACE) {
        return device_change_kind::none;
    }

    switch (wparam) {
    case DBT_DEVICEARRIVAL:
        return device_change_kind::arrival;
    case DBT_DEVICEREMOVECOMPLETE:
        return device_change_kind::removal;
    default:
        // Other DBT_* codes (DBT_DEVICEQUERYREMOVE, DBT_DEVICEQUERYREMOVEFAILED,
        // ...) this adapter never subscribed to - counted by nothing here,
        // same declared scope as win32_seat_adapter::translate()'s own
        // "default: counted by nothing here" for RIM_TYPEHID.
        return device_change_kind::none;
    }
}

} // namespace glintfx::platform

#endif // defined(_WIN32)
