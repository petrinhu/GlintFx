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

#include <cstdint>
#include <print>

#include <glintfx/core/err.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/win32/device_change_message.hpp"
#include "platform/win32/display_adapter.hpp"
#include "platform/win32/seat_adapter.hpp"

// seat_test.cpp - Y-1 (docs/plano-w6a-janela.md fatia 13, "bind real" -
// the plan's own name for THIS file): win32_seat_adapter opened for
// REAL against the windows-latest runner - a real message-only window
// under a real win32_display_adapter's own class, TWO real
// RegisterDeviceNotificationW registrations (09/09/2026 redesign,
// D-090918, DECISOES_AUTONOMAS.md - see seat_adapter.hpp's own header
// comment for the full "why"), and the FIRST capabilities() reading
// coming from the runner's own GetRawInputDeviceList()/
// GetSystemMetrics(SM_DIGITIZER), not a fixture. NAMED "seat_test", NOT
// "win32_seat_test" - tests/container/seat_test.cpp's own header
// comment already names this requirement.
//
// THE THREE CASES THIS FILE ADDS FOR THE REDESIGN (win-seat.md sec. 3,
// SF-1): (1) the seat takes NO slot in the process-wide raw input
// registry - the real fact that PROVES the defect win-seat.md sec. 1.1
// found is gone, not just that a different API got called; (2) the
// system actually accepted BOTH device-interface registrations this
// adapter asks for; (3) a synthetic WM_DEVICECHANGE still routes
// through the GWLP_WNDPROC subclass to this instance, the direct
// replacement for the predecessor's WM_INPUT_DEVICE_CHANGE case.
//
// win32_seat_adapter/win32_display_adapter are compiled a SECOND time
// directly into this executable's own object set (tests/CMakeLists.
// txt), the same technique win32_display_connect_test.cpp already
// documents: glintfx's own shared build hides everything without
// GLINTFX_API, so a separate executable linking only against
// glintfx::glintfx could never resolve these methods across that
// boundary.

using glintfx::platform::device_change_kind;
using glintfx::platform::seat_capability;
using glintfx::platform::win32_display_adapter;
using glintfx::platform::win32_seat_adapter;

GLINTFX_TEST(win32_seat_adapter_opens_against_a_real_display_and_reads_capabilities) {
    win32_display_adapter display;
    const glintfx::gltfx_rslt<void> display_opened = display.open();
    GLINTFX_CHECK(display_opened.has_value());
    GLINTFX_CHECK(display.is_open());

    win32_seat_adapter seat;
    GLINTFX_CHECK(!seat.is_open());

    const glintfx::gltfx_rslt<void> seat_opened = seat.open(display);

    // The ONE assertion this case makes (this file's own header
    // comment): the runner's own device mix, printed below, is a
    // measurement, not a pass/fail condition.
    GLINTFX_CHECK(seat_opened.has_value());
    GLINTFX_CHECK(seat.is_open());

    const bool has_pointer = seat.capabilities().has_capability(seat_capability::pointer);
    const bool has_keyboard = seat.capabilities().has_capability(seat_capability::keyboard);
    const bool has_touch = seat.capabilities().has_capability(seat_capability::touch);
    std::println("seat_test: pointer={} keyboard={} touch={}", has_pointer, has_keyboard,
                 has_touch);
    // MEASURED-COLLECTOR: tests/container/seat_test.cpp's own Wayland
    // twin already prints these same three keys.
    std::println("MEASURED seat_test.pointer={}", has_pointer ? 1 : 0);
    std::println("MEASURED seat_test.keyboard={}", has_keyboard ? 1 : 0);
    std::println("MEASURED seat_test.touch={}", has_touch ? 1 : 0);
    // capability_events, NOT last_change_kind (SF-2, D-WS-4, win-
    // seat.md sec. 3): tests/container/seat_test.cpp's own Wayland side
    // already prints this same key name - the two are NOT comparable by
    // equality (seat_adapter.hpp's own last_change() comment), only the
    // name and "at least one announcement" shape are shared.
    // seat_test.last_change_kind (a single WM_INPUT_DEVICE_CHANGE
    // message's own wParam code) is DELIBERATELY gone - the mechanism
    // it measured (RIDEV_DEVNOTIFY) no longer exists (win-seat.md sec.
    // 1.1, D-090918).
    std::println("MEASURED seat_test.capability_events={}",
                 static_cast<unsigned long long>(seat.last_change()));

    seat.close();
    GLINTFX_CHECK(!seat.is_open());

    // Idempotent, same shape win32_display_adapter::close() already
    // documents for itself.
    seat.close();
    GLINTFX_CHECK(!seat.is_open());

    display.close();
    GLINTFX_CHECK(!display.is_open());
}

// The real fact that PROVES win-seat.md sec. 1.1's defect is gone: a
// consumer that later registers its own RAW INPUT (RegisterRawInput
// Devices, common for FPS-style mouse look) must never find this
// adapter already occupying the process-wide slot for the mouse/
// keyboard device class. GetRegisteredRawInputDevices(nullptr, ...)
// with a null buffer returns the COUNT of currently registered raw
// input devices for the whole process (learn.microsoft.com/windows/
// win32/api/winuser/nf-winuser-getregisteredrawinputdevices) - zero is
// the only value a library that never calls RegisterRawInputDevices at
// all can honestly produce. The mutation this case exists to catch:
// reintroducing RegisterRawInputDevices/RIDEV_DEVNOTIFY into open().
GLINTFX_TEST(win32_seat_adapter_takes_no_process_wide_raw_input_slot) {
    win32_display_adapter display;
    GLINTFX_CHECK(display.open().has_value());

    win32_seat_adapter seat;
    GLINTFX_CHECK(seat.open(display).has_value());

    UINT device_count = 0;
    const UINT probe_result =
        ::GetRegisteredRawInputDevices(nullptr, &device_count, sizeof(RAWINPUTDEVICE));
    std::println("MEASURED seat_test.registered_raw_input_devices={}",
                 static_cast<unsigned long long>(device_count));
    GLINTFX_CHECK(probe_result == 0);
    GLINTFX_CHECK(device_count == 0);

    seat.close();
    display.close();
}

// The system actually accepted BOTH device-interface registrations
// open() asks for (keyboard, mouse - seat_adapter.hpp's own "D-WS-3"
// paragraph explains why touch is not a third). The mutation this case
// exists to catch: silently dropping the mouse registration (or the
// keyboard one) while still reporting open() as successful.
GLINTFX_TEST(win32_seat_adapter_holds_two_device_interface_registrations_the_system_accepted) {
    win32_display_adapter display;
    GLINTFX_CHECK(display.open().has_value());

    win32_seat_adapter seat;
    GLINTFX_CHECK(seat.open(display).has_value());

    GLINTFX_CHECK(seat.device_notification_count() == 2);

    seat.close();
    display.close();
}

// The direct replacement for the predecessor's WM_INPUT_DEVICE_CHANGE
// case: a SYNTHETIC WM_DEVICECHANGE (SendMessageW, never a real
// unplug/replug the runner cannot be made to do) still routes through
// the real GWLP_WNDPROC subclass this fatia installs to THIS adapter
// instance and no other. The mutation this case exists to catch:
// deleting the WM_DEVICECHANGE branch in seat_window_proc (seat_
// adapter.cpp) would leave last_device_change() stuck at
// device_change_kind::none forever.
GLINTFX_TEST(win32_seat_adapter_routes_a_synthetic_wm_devicechange_to_itself) {
    win32_display_adapter display;
    GLINTFX_CHECK(display.open().has_value());

    win32_seat_adapter seat;
    GLINTFX_CHECK(seat.open(display).has_value());

    // No real WM_DEVICECHANGE has arrived yet - constructed default
    // (seat_adapter.hpp's own last_device_change() comment).
    GLINTFX_CHECK(seat.last_device_change() == device_change_kind::none);

    // classify_device_change() (device_change_message.hpp) only ever
    // reads dbch_devicetype off this block - the class GUID is
    // irrelevant to routing, only to WHICH registration the real system
    // would have fired this notification from.
    DEV_BROADCAST_HDR arrival_block{};
    arrival_block.dbch_devicetype = DBT_DEVTYP_DEVICEINTERFACE;

    // DIAGNOSTIC (server run 34432463346 found this exact check failing
    // - never seen live before, and neither code review nor Microsoft's
    // own documentation for WM_DEVICECHANGE/DEV_BROADCAST_HDR/
    // RegisterDeviceNotificationW/message-only windows explains why):
    // proves classify_device_change() itself, called DIRECTLY on the
    // exact same block, independent of any window/message delivery at
    // all - isolates "the pure function is wrong" from "the message
    // never reached the window procedure".
    GLINTFX_CHECK(glintfx::platform::classify_device_change(DBT_DEVICEARRIVAL, &arrival_block) ==
                  device_change_kind::arrival);

    // DIAGNOSTIC, second round (server run 34434559496 found record_
    // raw_message() staying at its constructed default - the message
    // never reached seat_window_proc with a non-null adapter at all,
    // even though it is recorded UNCONDITIONALLY before any WM_
    // DEVICECHANGE-specific guard): the direct handle this call is
    // about to target, and whether the ACTIVE window procedure (read
    // LIVE, never cached) is really seat_window_proc - team-lead's own
    // hypotheses 1 and 2 (wrong handle; wrong/overwritten procedure).
    std::println("MEASURED seat_test.diag_native_handle={}",
                 reinterpret_cast<std::uintptr_t>(seat.native_handle()));
    std::println("MEASURED seat_test.diag_wndproc_installed={}",
                 seat.wndproc_is_installed() ? 1 : 0);
    GLINTFX_CHECK(seat.wndproc_is_installed());

    // Sent with the CALLING thread's own SendMessageW - the message-
    // only window this adapter owns belongs to this same thread, so
    // this is delivered synchronously, straight into seat_window_proc
    // (seat_adapter.cpp), before SendMessageW returns.
    ::SetLastError(0);
    const LRESULT send_result =
        ::SendMessageW(seat.native_handle(), WM_DEVICECHANGE, DBT_DEVICEARRIVAL,
                       reinterpret_cast<LPARAM>(&arrival_block));
    std::println("MEASURED seat_test.diag_send_result={}", static_cast<long long>(send_result));
    std::println("MEASURED seat_test.diag_send_last_error={}",
                 static_cast<unsigned long long>(::GetLastError()));

    // DIAGNOSTIC: what seat_window_proc actually saw, unconditionally
    // recorded before any WM_DEVICECHANGE-specific guard runs (seat_
    // adapter.hpp's own record_raw_message() comment) - printed always,
    // asserted only on the message id, since a mismatch there means the
    // message never arrived at all (the more useful of the two facts to
    // know before touching any production line).
    std::println("MEASURED seat_test.diag_last_raw_message={}",
                 static_cast<unsigned long long>(seat.last_raw_message()));
    std::println("MEASURED seat_test.diag_last_raw_wparam={}",
                 static_cast<unsigned long long>(seat.last_raw_wparam()));
    // DIAGNOSTIC addendum (team-lead's own follow-up, 10/09/2026): the
    // OTHER guard classify_device_change() applies - did the block
    // arrive at all, and with which dbch_devicetype. A message that
    // arrived with the right wParam but a block naming some OTHER
    // devicetype (or no block at all) would look identical to "never
    // arrived" from last_device_change() alone.
    std::println("MEASURED seat_test.diag_block_present={}",
                 seat.last_device_change_block_present() ? 1 : 0);
    std::println("MEASURED seat_test.diag_block_devicetype={}",
                 static_cast<unsigned long long>(seat.last_device_change_block_devicetype()));
    GLINTFX_CHECK(seat.last_raw_message() == WM_DEVICECHANGE);
    GLINTFX_CHECK(seat.last_raw_wparam() == static_cast<WPARAM>(DBT_DEVICEARRIVAL));

    GLINTFX_CHECK(seat.last_device_change() == device_change_kind::arrival);

    DEV_BROADCAST_HDR removal_block{};
    removal_block.dbch_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    ::SendMessageW(seat.native_handle(), WM_DEVICECHANGE, DBT_DEVICEREMOVECOMPLETE,
                   reinterpret_cast<LPARAM>(&removal_block));

    GLINTFX_CHECK(seat.last_device_change() == device_change_kind::removal);

    seat.close();
    display.close();
}

// D-WS-4 (SF-2, win-seat.md sec. 3): last_change() counts open()'s own
// initial read as 1, then +1 per routed WM_DEVICECHANGE - the mutation
// this case exists to catch: incrementing only in handle_device_change()
// (which would leave the count at 0 right after open()) or not
// incrementing inside recompute_capabilities() at all.
GLINTFX_TEST(capability_events_count_the_initial_read_and_each_device_change) {
    win32_display_adapter display;
    GLINTFX_CHECK(display.open().has_value());

    win32_seat_adapter seat;
    GLINTFX_CHECK(seat.open(display).has_value());
    GLINTFX_CHECK(seat.last_change() == 1);

    DEV_BROADCAST_HDR arrival_block{};
    arrival_block.dbch_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    ::SendMessageW(seat.native_handle(), WM_DEVICECHANGE, DBT_DEVICEARRIVAL,
                   reinterpret_cast<LPARAM>(&arrival_block));
    GLINTFX_CHECK(seat.last_change() == 2);

    DEV_BROADCAST_HDR removal_block{};
    removal_block.dbch_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    ::SendMessageW(seat.native_handle(), WM_DEVICECHANGE, DBT_DEVICEREMOVECOMPLETE,
                   reinterpret_cast<LPARAM>(&removal_block));
    GLINTFX_CHECK(seat.last_change() == 3);

    seat.close();
    display.close();
}

#endif // defined(_WIN32)
