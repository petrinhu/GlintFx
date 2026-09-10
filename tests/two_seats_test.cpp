// SPDX-License-Identifier: AGPL-3.0-or-later
#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <dbt.h>

#include <glintfx/core/err.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/win32/device_change_message.hpp"
#include "platform/win32/display_adapter.hpp"
#include "platform/win32/seat_adapter.hpp"

// two_seats_test.cpp - SF-1 of WIN-SEAT's 09/09/2026 reopening (win-
// seat.md sec. 1.1 defect 3, sec. 3 D-WS-7, D-090918 in DECISOES_
// AUTONOMAS.md): the case §1.1 of that plan named as never having
// existed for this adapter - "dois assentos no mesmo processo roubam o
// registro um do outro" was true of the predecessor mechanism
// (RegisterRawInputDevices, process-wide per device CLASS) and
// untestable with a single seat. This file opens TWO win32_seat_adapter
// instances in the SAME process and proves each holds its OWN pair of
// RegisterDeviceNotificationW handles, and that closing ONE never
// disturbs the OTHER - the direct behavioral proof of D-WS-7.
//
// NAMED "two_seats_test", not "win32_two_seats_test" - the same "no
// platform prefix" requirement tests/two_displays_test.cpp's own header
// comment documents for its own name, so tests/tools/check_test_
// parity.py's own P-0 inventory lines this ctest up against the Wayland
// side's own container fixture of the identical name (tests/container/
// two_seats_test.cpp), the same shape two_displays_test/seat_test
// already use for their own pairs.
//
// win32_seat_adapter/win32_display_adapter are compiled a SECOND time
// directly into this executable's own object set (tests/CMakeLists.
// txt), the same technique win32_display_connect_test.cpp already
// documents.

using glintfx::platform::device_change_kind;
using glintfx::platform::win32_display_adapter;
using glintfx::platform::win32_seat_adapter;

GLINTFX_TEST(two_seats_in_one_process_have_independent_registrations) {
    win32_display_adapter first_display;
    win32_display_adapter second_display;
    GLINTFX_CHECK(first_display.open().has_value());
    GLINTFX_CHECK(second_display.open().has_value());

    win32_seat_adapter first_seat;
    win32_seat_adapter second_seat;
    GLINTFX_CHECK(first_seat.open(first_display).has_value());
    GLINTFX_CHECK(second_seat.open(second_display).has_value());

    GLINTFX_CHECK(first_seat.device_notification_count() == 2);
    GLINTFX_CHECK(second_seat.device_notification_count() == 2);

    // D-WS-7's own guarantee, exercised for real: closing the FIRST
    // seat must never touch the SECOND seat's own, independent
    // registration - the exact defect a process-wide RIDEV_REMOVE
    // (this file's predecessor mechanism) would have had (win-seat.md
    // sec. 1.1, defect 3).
    first_seat.close();
    GLINTFX_CHECK(!first_seat.is_open());
    GLINTFX_CHECK(second_seat.is_open());
    GLINTFX_CHECK(second_seat.device_notification_count() == 2);

    // The SURVIVING seat still routes a synthetic WM_DEVICECHANGE after
    // its sibling closed - proves the routing itself (GWLP_USERDATA/
    // GWLP_WNDPROC, per-window) never depended on the sibling being
    // alive. Real DEV_BROADCAST_DEVICEINTERFACE_W shape, dbcc_size set
    // - see tests/seat_test.cpp's own win32_seat_adapter_routes_a_
    // synthetic_wm_devicechange_to_itself comment for why: a bare
    // DEV_BROADCAST_HDR is silently discarded by SendMessage's own
    // system-message marshalling before reaching a window procedure.
    DEV_BROADCAST_DEVICEINTERFACE_W arrival_block{};
    arrival_block.dbcc_size = sizeof(arrival_block);
    arrival_block.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    ::SendMessageW(second_seat.native_handle(), WM_DEVICECHANGE, DBT_DEVICEARRIVAL,
                   reinterpret_cast<LPARAM>(&arrival_block));
    GLINTFX_CHECK(second_seat.last_device_change() == device_change_kind::arrival);

    second_seat.close();
    GLINTFX_CHECK(!second_seat.is_open());
    first_display.close();
    second_display.close();

    // Reopen in the INVERSE order - proves the independence D-WS-7
    // claims is not an artifact of which seat happened to open() first.
    GLINTFX_CHECK(second_display.open().has_value());
    GLINTFX_CHECK(first_display.open().has_value());
    GLINTFX_CHECK(second_seat.open(second_display).has_value());
    GLINTFX_CHECK(first_seat.open(first_display).has_value());

    GLINTFX_CHECK(second_seat.device_notification_count() == 2);
    GLINTFX_CHECK(first_seat.device_notification_count() == 2);

    second_seat.close();
    GLINTFX_CHECK(first_seat.is_open());
    GLINTFX_CHECK(first_seat.device_notification_count() == 2);

    first_seat.close();
    first_display.close();
    second_display.close();
}

#endif // defined(_WIN32)
