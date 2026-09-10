// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/win32/seat_adapter.hpp"

#if defined(_WIN32)

#include <dbt.h>

#include <iterator>
#include <new>
#include <vector>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

// seat_adapter.cpp - see seat_adapter.hpp's own header comment for
// scope, the mechanism sources, and the 09/09/2026 redesign (D-090918)
// this file carries out. Written from Microsoft's own current
// documentation (that header's own comment cites the exact pages), not
// from having watched this file run on a Windows machine (GODS_LAWS.md
// L-27, same declared limitation as display_adapter.cpp's own header
// comment for the fatia this one extends).

namespace glintfx::platform {

namespace {

// SM_DIGITIZER's own NID_* bitmask (seat_adapter.hpp's own "MECHANISM"
// paragraph, learn.microsoft.com/windows/win32/api/winuser/
// nf-winuser-getsystemmetrics Remarks section) - unchanged by this
// fatia's redesign.
constexpr int k_nid_integrated_touch = 0x01;
constexpr int k_nid_external_touch = 0x02;

// D-WS-5 (SF-3, win-seat.md sec. 1.6/3): the minimum key count a
// RIM_TYPEKEYBOARD device must positively report to count as a keyboard
// (raw_device_facts.hpp's own header comment explains the udev/systemd
// parity rule this derives from) - a device that reports FEWER keys is
// excluded; a device that reports 0 (query failed, or the driver never
// answers) is NOT ("nao invente ausencia").
constexpr std::uint32_t k_minimum_keyboard_key_count = 32;

// GUID_DEVINTERFACE_KEYBOARD / GUID_DEVINTERFACE_MOUSE, written by hand
// from Microsoft's own documented values (D-WS-2, seat_adapter.hpp's own
// header comment: <ntddkbd.h>/<ntddmou.h> + <initguid.h> is the
// PKEY_AppUserModel_ID trap this project already paid for once - a
// symbol whose DEFINITION only exists in the translation unit that
// includes <initguid.h> BEFORE the driver header). Values quoted
// verbatim from learn.microsoft.com/windows-hardware/drivers/install/
// guid-devinterface-keyboard and .../guid-devinterface-mouse.
constexpr GUID k_guid_devinterface_keyboard = {
    0x884b96c3, 0x56ef, 0x11d1, {0xbc, 0x8c, 0x00, 0xa0, 0xc9, 0x14, 0x05, 0xdd}};
constexpr GUID k_guid_devinterface_mouse = {
    0x378de44c, 0x56ef, 0x11d1, {0xbc, 0x8c, 0x00, 0xa0, 0xc9, 0x14, 0x05, 0xdd}};

// Builds the DEV_BROADCAST_DEVICEINTERFACE_W filter RegisterDevice
// NotificationW expects for one device interface class GUID -
// dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE is what classify_device_
// change() (device_change_message.cpp) later checks for on the way
// back in.
DEV_BROADCAST_DEVICEINTERFACE_W make_device_interface_filter(const GUID &class_guid) noexcept {
    DEV_BROADCAST_DEVICEINTERFACE_W filter{};
    filter.dbcc_size = sizeof(filter);
    filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    filter.dbcc_classguid = class_guid;
    return filter;
}

// D-WS-5 (SF-3): one GetRawInputDeviceInfoW(RIDI_DEVICEINFO) call per
// RIM_TYPEKEYBOARD entry - never per RIM_TYPEMOUSE (the key-count rule
// only applies to the keyboard class, seat_adapter.hpp's own translate()
// comment). A failed query, or a type mismatch (the device changed
// class between GetRawInputDeviceList() and this call - the same race
// recompute_capabilities() already treats as legitimate elsewhere in
// this file), degrades to 0 ("unknown"), which translate() treats as
// PRESENT, never as a reason to drop the device.
std::uint32_t query_keyboard_key_count(HANDLE device) noexcept {
    RID_DEVICE_INFO info{};
    info.cbSize = sizeof(info);
    UINT size = sizeof(info);
    const UINT result = ::GetRawInputDeviceInfoW(device, RIDI_DEVICEINFO, &info, &size);
    if (result == static_cast<UINT>(-1) || info.dwType != RIM_TYPEKEYBOARD) {
        return 0;
    }
    return info.keyboard.dwNumberOfKeysTotal;
}

// seat_window_proc - the instance-level subclass open() installs via
// GWLP_WNDPROC (SetWindowLongPtrW) AFTER CreateWindowExW returns, on
// top of the display's own class-level window_proc that already
// installed GWLP_USERDATA during WM_NCCREATE (display_adapter.cpp,
// anonymous namespace) - see seat_adapter.hpp's own "WHY A SECOND
// HWND" paragraph for why this ordering means GWLP_USERDATA is always
// already valid by the time THIS function ever runs.
LRESULT CALLBACK seat_window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) noexcept {
    // GetWindowLongPtrW returns the GWLP_USERDATA slot as a LONG_PTR by
    // Win32's own design - it holds a genuine win32_seat_adapter*
    // (open() below is the only writer, lpParam = this).
    auto *adapter =
        // NOLINTNEXTLINE(performance-no-int-to-ptr) reason: see comment above
        reinterpret_cast<win32_seat_adapter *>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    // DIAGNOSTIC (seat_adapter.hpp's own record_raw_message() comment,
    // 10/09/2026): unconditional, before any WM_DEVICECHANGE-specific
    // guard - answers whether a message even reached this window
    // procedure at all, independent of what happens to it next.
    if (adapter != nullptr) {
        adapter->record_raw_message(msg, wparam);
    }

    if (msg == WM_DEVICECHANGE && adapter != nullptr) {
        // WM_DEVICECHANGE's lParam carries a pointer to a DEV_BROADCAST_
        // HDR-shaped block (or is unused/zero for some wParam codes) -
        // classify_device_change() (device_change_message.cpp) applies
        // both guards win-seat.md sec. 1.3 requires before this adapter
        // ever treats it as an arrival or removal.
        // NOLINTNEXTLINE(performance-no-int-to-ptr) reason: same idiom as GWLP_USERDATA above
        const auto kind = classify_device_change(wparam, reinterpret_cast<const void *>(lparam));
        if (kind != device_change_kind::none) {
            adapter->handle_device_change(kind);
        }
        // learn.microsoft.com/windows/win32/devio/registering-for-
        // device-notification's own WindowProc example returns TRUE
        // from its WM_DEVICECHANGE case - unlike WM_INPUT_DEVICE_CHANGE
        // (this file's predecessor, which returned zero), this message
        // is not itself part of the raw input family this fatia's
        // whole redesign moved away from.
        return TRUE;
    }

    if (adapter != nullptr && adapter->previous_wndproc() != nullptr) {
        return ::CallWindowProcW(adapter->previous_wndproc(), hwnd, msg, wparam, lparam);
    }
    return ::DefWindowProcW(hwnd, msg, wparam, lparam);
}

} // namespace

void win32_seat_adapter::translate(std::span<const win32_raw_device_facts> devices,
                                   int digitizer_bitmask, seat_capabilities &out) noexcept {
    bool pointer_present = false;
    bool keyboard_present = false;

    for (const win32_raw_device_facts &device : devices) {
        switch (device.raw_type) {
        case RIM_TYPEMOUSE:
            pointer_present = true;
            break;
        case RIM_TYPEKEYBOARD:
            // D-WS-5: keyboard_key_count is IGNORED for every other
            // raw_type - the rule only excludes a keyboard-class device
            // that positively reports too few keys.
            keyboard_present |= (device.keyboard_key_count == 0 ||
                                 device.keyboard_key_count >= k_minimum_keyboard_key_count);
            break;
        default:
            // RIM_TYPEHID and any future dwType this project's three-
            // value seat_capability enum has no slot for - counted by
            // nothing here, same declared scope as tests/win32_runner_
            // probe_test.cpp's own "unknown_count" bucket.
            break;
        }
    }

    const bool touch_present =
        (digitizer_bitmask & (k_nid_integrated_touch | k_nid_external_touch)) != 0;

    out.set_capability(seat_capability::pointer, pointer_present);
    out.set_capability(seat_capability::keyboard, keyboard_present);
    out.set_capability(seat_capability::touch, touch_present);
}

void win32_seat_adapter::recompute_capabilities() noexcept {
    // Two-call idiom straight from GetRawInputDeviceList's own
    // documentation, unchanged by this fatia's redesign (this
    // function's own reason to run again - a routed WM_DEVICECHANGE -
    // changed; what it does once it runs did not).
    UINT device_count = 0;
    const UINT count_query_result =
        ::GetRawInputDeviceList(nullptr, &device_count, sizeof(RAWINPUTDEVICELIST));

    std::vector<RAWINPUTDEVICELIST> devices;
    if (count_query_result != static_cast<UINT>(-1) && device_count > 0) {
        // resize() growing from empty CAN throw std::bad_alloc despite
        // this function's own noexcept - degrading to "no devices this
        // round" on failure, same reasoning this project's other
        // resize()-guarding call sites already document (GODS_LAWS.md
        // L-22: no exception crosses the public boundary).
        bool resized = true;
        try {
            devices.resize(device_count);
        } catch (const std::bad_alloc &) {
            resized = false;
        }
        if (resized) {
            const UINT filled =
                ::GetRawInputDeviceList(devices.data(), &device_count, sizeof(RAWINPUTDEVICELIST));
            if (filled == static_cast<UINT>(-1)) {
                // A device was unplugged between the two calls - this
                // adapter reacts to its own WM_DEVICECHANGE handler
                // running again shortly after, so treating the race
                // itself as "no devices this round" keeps this
                // function's own contract simple.
                devices.clear();
            } else {
                try {
                    devices.resize(filled);
                } catch (const std::bad_alloc &) {
                    devices.clear();
                }
            }
        }
    }

    // D-WS-5 (SF-3): one win32_raw_device_facts per raw device, with
    // keyboard_key_count filled in ONLY for RIM_TYPEKEYBOARD entries
    // (query_keyboard_key_count() above, an extra syscall this project
    // never pays for a mouse entry). Guarded the same way as the
    // resize() calls above - an allocation failure here degrades to
    // "no devices this round", not a crash (GODS_LAWS.md L-22).
    std::vector<win32_raw_device_facts> facts;
    try {
        facts.reserve(devices.size());
        for (const RAWINPUTDEVICELIST &raw_device : devices) {
            win32_raw_device_facts device_facts;
            device_facts.raw_type = raw_device.dwType;
            if (raw_device.dwType == RIM_TYPEKEYBOARD) {
                device_facts.keyboard_key_count = query_keyboard_key_count(raw_device.hDevice);
            }
            facts.push_back(device_facts);
        }
    } catch (const std::bad_alloc &) {
        facts.clear();
    }

    const int digitizer_bitmask = ::GetSystemMetrics(SM_DIGITIZER);
    translate(facts, digitizer_bitmask, m_capabilities);

    // D-WS-4 (SF-2): this function runs exactly once per "system
    // announcement" this adapter reacts to - open()'s own first call
    // (the synchronous initial read) and once more per routed
    // WM_DEVICECHANGE (handle_device_change() below) - so incrementing
    // HERE, once, covers both without separate bookkeeping at each call
    // site (seat_adapter.hpp's own last_change() comment).
    ++m_last_change;
}

void win32_seat_adapter::handle_device_change(device_change_kind kind) noexcept {
    m_last_device_change = kind;
    recompute_capabilities();
}

WNDPROC win32_seat_adapter::previous_wndproc() const noexcept { return m_previous_wndproc; }

void win32_seat_adapter::record_raw_message(UINT msg, WPARAM wparam) noexcept {
    m_last_raw_message = msg;
    m_last_raw_wparam = wparam;
}

int win32_seat_adapter::device_notification_count() const noexcept {
    int count = 0;
    if (m_keyboard_notification != nullptr) {
        ++count;
    }
    if (m_mouse_notification != nullptr) {
        ++count;
    }
    return count;
}

win32_seat_adapter::~win32_seat_adapter() { close(); }

gltfx_rslt<void> win32_seat_adapter::open(const win32_display_adapter &display) noexcept {
    if (!display.is_open()) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value("display"));
    }

    ::SetLastError(0);
    HWND window = ::CreateWindowExW(0, display.window_class_name(), L"", 0, 0, 0, 0, 0,
                                    HWND_MESSAGE, nullptr, ::GetModuleHandleW(nullptr), this);
    if (window == nullptr) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::platform_failure).with_os_error_code(::GetLastError()));
    }

    // Instance subclass (seat_adapter.hpp's own "MECHANISM" paragraph):
    // WM_NCCREATE already ran, through the class's OWN window_proc,
    // during the CreateWindowExW call above - GWLP_USERDATA is already
    // `this`. From here on, THIS window's messages go through seat_
    // window_proc instead.
    ::SetLastError(0);
    // NOLINTNEXTLINE(performance-no-int-to-ptr) reason: see seat_window_proc's own comment
    const auto previous = reinterpret_cast<WNDPROC>(
        ::SetWindowLongPtrW(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&seat_window_proc)));
    if (previous == nullptr) {
        const DWORD subclass_error = ::GetLastError();
        if (subclass_error != 0) {
            ::DestroyWindow(window);
            return gltfx_rslt<void>::err(
                gltfx_err(gltfx_err_code::platform_failure).with_os_error_code(subclass_error));
        }
        // SetWindowLongPtr's own documented failure signal is a ZERO
        // return with GetLastError() ALSO zero being the (rare)
        // legitimate case "the previous value genuinely was zero" - a
        // window procedure address is never legitimately null on a
        // window that has already been created, so a null return with
        // no error set is treated the same as a real value here rather
        // than invented special-cased trust.
    }

    // D-WS-7: two INDEPENDENT registrations, held per-instance
    // (m_keyboard_notification/m_mouse_notification below) - never a
    // shared or process-wide handle, exactly the property RIDEV_REMOVE
    // (this file's predecessor) did not have.
    DEV_BROADCAST_DEVICEINTERFACE_W keyboard_filter =
        make_device_interface_filter(k_guid_devinterface_keyboard);
    ::SetLastError(0);
    HDEVNOTIFY keyboard_notification =
        ::RegisterDeviceNotificationW(window, &keyboard_filter, DEVICE_NOTIFY_WINDOW_HANDLE);
    if (keyboard_notification == nullptr) {
        const DWORD register_error = ::GetLastError();
        ::DestroyWindow(window);
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::platform_failure).with_os_error_code(register_error));
    }

    DEV_BROADCAST_DEVICEINTERFACE_W mouse_filter =
        make_device_interface_filter(k_guid_devinterface_mouse);
    ::SetLastError(0);
    HDEVNOTIFY mouse_notification =
        ::RegisterDeviceNotificationW(window, &mouse_filter, DEVICE_NOTIFY_WINDOW_HANDLE);
    if (mouse_notification == nullptr) {
        const DWORD register_error = ::GetLastError();
        // Undo the keyboard registration this same open() already won,
        // before tearing down the window - the same "close() to undo a
        // partial open()" shape this project's other adapters already
        // document.
        ::UnregisterDeviceNotification(keyboard_notification);
        ::DestroyWindow(window);
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::platform_failure).with_os_error_code(register_error));
    }

    m_window = window;
    m_previous_wndproc = previous;
    m_keyboard_notification = keyboard_notification;
    m_mouse_notification = mouse_notification;
    recompute_capabilities();
    return gltfx_rslt<void>::ok();
}

void win32_seat_adapter::close() noexcept {
    if (m_window != nullptr) {
        // Unregister ONLY this instance's own two handles (D-WS-7) -
        // never a process-wide flag like RIDEV_REMOVE (this file's
        // predecessor), so a SIBLING win32_seat_adapter in the same
        // process is untouched by this call (two_seats_test proves
        // exactly this).
        if (m_keyboard_notification != nullptr) {
            ::UnregisterDeviceNotification(m_keyboard_notification);
            m_keyboard_notification = nullptr;
        }
        if (m_mouse_notification != nullptr) {
            ::UnregisterDeviceNotification(m_mouse_notification);
            m_mouse_notification = nullptr;
        }

        ::DestroyWindow(m_window);
        m_window = nullptr;
        m_previous_wndproc = nullptr;
    }
}

} // namespace glintfx::platform

#endif // defined(_WIN32)
