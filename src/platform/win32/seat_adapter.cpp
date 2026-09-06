// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/win32/seat_adapter.hpp"

#if defined(_WIN32)

#include <iterator>
#include <new>
#include <vector>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

// seat_adapter.cpp - see seat_adapter.hpp's own header comment for
// scope, the mechanism sources, and why this fatia (Y-1, docs/plano-
// w6a-janela.md fatia 13) reuses win32_display_adapter's own window
// class instead of registering a second one. Written from Microsoft's
// own current documentation (that header's own "MECHANISM" paragraph
// cites the exact pages), not from having watched this file run on a
// Windows machine (GODS_LAWS.md L-27, same declared limitation as
// display_adapter.cpp's own header comment for the fatia this one
// extends).

namespace glintfx::platform {

namespace {

// Generic Desktop Controls usage page and the two usage IDs this
// adapter cares about (seat_adapter.hpp's own "MECHANISM" paragraph:
// learn.microsoft.com/windows-hardware/drivers/hid/hid-usages). Named
// constants rather than bare 0x01/0x02/0x06 at each call site, the
// same "spell out where a number comes from" reasoning win32_runner_
// probe_test.cpp already applies to its own k_gl_vendor/k_gl_renderer/
// k_gl_version.
constexpr USHORT k_usage_page_generic_desktop = 0x01;
constexpr USHORT k_usage_mouse = 0x02;
constexpr USHORT k_usage_keyboard = 0x06;

// SM_DIGITIZER's own NID_* bitmask (seat_adapter.hpp's own "MECHANISM"
// paragraph, learn.microsoft.com/windows/win32/api/winuser/
// nf-winuser-getsystemmetrics Remarks section) - NID_INTEGRATED_TOUCH
// and NID_EXTERNAL_TOUCH are the two bits this adapter reads for
// seat_capability::touch ("touch/digitizer surface", seat_
// capabilities.hpp's own header comment); NID_INTEGRATED_PEN/
// NID_EXTERNAL_PEN answer a stylus question this project's three-value
// enum has no slot for, and NID_MULTI_INPUT/NID_READY qualify an
// existing digitizer rather than proving one exists - none of those
// four are read here.
constexpr int k_nid_integrated_touch = 0x01;
constexpr int k_nid_external_touch = 0x02;

// seat_window_proc - the instance-level subclass open() installs via
// GWLP_WNDPROC (SetWindowLongPtrW) AFTER CreateWindowExW returns, on
// top of the display's own class-level window_proc that already
// installed GWLP_USERDATA during WM_NCCREATE (display_adapter.cpp,
// anonymous namespace) - see seat_adapter.hpp's own "WHY A SECOND
// HWND" paragraph for why this ordering means GWLP_USERDATA is always
// already valid by the time THIS function ever runs.
//
// Reads `this` back out of GWLP_USERDATA (never null for a window this
// adapter created with lpParam = the adapter itself), and only acts on
// WM_INPUT_DEVICE_CHANGE - every other message is chained to the
// PREVIOUS window procedure via CallWindowProcW, exactly as SetWindow
// LongPtr's own documentation requires for GWLP_WNDPROC subclassing
// (seat_adapter.hpp's own "MECHANISM" paragraph).
LRESULT CALLBACK seat_window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) noexcept {
    // GetWindowLongPtrW returns the GWLP_USERDATA slot as a LONG_PTR by
    // Win32's own design (learn.microsoft.com/windows/win32/api/winuser/
    // nf-winuser-getwindowlongptrw) - it holds a genuine win32_seat_
    // adapter* (open() below is the only writer, lpParam = this), the
    // integer-typed return is just how the Win32 API carries it,
    // unavoidable at this boundary - same idiom display_adapter.cpp's
    // own GWLP_USERDATA read already carries the same suppression for.
    auto *adapter =
        // NOLINTNEXTLINE(performance-no-int-to-ptr) reason: see comment above
        reinterpret_cast<win32_seat_adapter *>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (msg == WM_INPUT_DEVICE_CHANGE && adapter != nullptr) {
        // WM_INPUT_DEVICE_CHANGE's own documentation (this file's own
        // "MECHANISM" paragraph, seat_adapter.hpp): lParam carries "a
        // handle to the device that generated the change" - the same
        // "the Win32 API hands back a pointer through an integer-typed
        // slot" idiom the GWLP_USERDATA read above already carries the
        // identical suppression for, just LPARAM instead of the return
        // of GetWindowLongPtrW.
        // NOLINTNEXTLINE(performance-no-int-to-ptr) reason: see comment above
        adapter->handle_input_device_change(wparam, reinterpret_cast<HANDLE>(lparam));
        // WM_INPUT_DEVICE_CHANGE's own documentation (seat_adapter.hpp's
        // "MECHANISM" paragraph): "If an application processes this
        // message, it should return zero."
        return 0;
    }

    if (adapter != nullptr && adapter->previous_wndproc() != nullptr) {
        return ::CallWindowProcW(adapter->previous_wndproc(), hwnd, msg, wparam, lparam);
    }
    return ::DefWindowProcW(hwnd, msg, wparam, lparam);
}

} // namespace

void win32_seat_adapter::translate(const RAWINPUTDEVICELIST *devices, UINT device_count,
                                   int digitizer_bitmask, seat_capabilities &out) noexcept {
    // Real defect found by clang-tidy's clang-analyzer-core.
    // NullDereference (lint-enum run, 06/09/2026): `devices` may be
    // nullptr when `device_count` is 0 (this function's own header
    // comment, seat_adapter.hpp's "Pure translation seam" paragraph),
    // but nothing here ever enforced that a CALLER honors the other
    // half of that pairing - translate() is a public static seam any
    // test can call directly with an arbitrary pair, and a caller
    // passing a null pointer alongside a non-zero count would walk the
    // loop below off the end of a null buffer. Clamped here, once, so
    // the loop's own precondition ("devices has at least device_count
    // entries whenever it runs at all") is actually enforced rather
    // than merely documented.
    if (devices == nullptr) {
        device_count = 0;
    }

    bool pointer_present = false;
    bool keyboard_present = false;

    // devices may be nullptr when device_count is 0 (seat_adapter.hpp's
    // own header comment on this function) - the loop below simply
    // never runs in that case, the same "empty is a legitimate answer"
    // shape GetRawInputDeviceList's own two-call idiom already produces
    // when the system truly has zero devices attached.
    for (UINT i = 0; i < device_count; ++i) {
        switch (devices[i].dwType) {
        case RIM_TYPEMOUSE:
            pointer_present = true;
            break;
        case RIM_TYPEKEYBOARD:
            keyboard_present = true;
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
    // documentation - the SAME idiom tests/win32_runner_probe_test.cpp
    // already proved live on the windows-latest runner (05/09/2026):
    // first call with a null buffer to learn the count via the out-
    // parameter, second call to fill a buffer sized for that count.
    UINT device_count = 0;
    const UINT count_query_result =
        ::GetRawInputDeviceList(nullptr, &device_count, sizeof(RAWINPUTDEVICELIST));

    std::vector<RAWINPUTDEVICELIST> devices;
    if (count_query_result != static_cast<UINT>(-1) && device_count > 0) {
        // resize() growing from empty CAN throw std::bad_alloc despite
        // this function's own noexcept - `device_count` comes from the
        // OS (GetRawInputDeviceList), not bounded by anything this
        // adapter controls, the same "not realistically engineered-
        // around, but must not reach the caller as a crash" shape
        // widen_utf8()'s own resize() (app_user_model_id.cpp, window_
        // adapter.cpp) already handles for a different allocation
        // (GODS_LAWS.md L-22: no exception crosses the public boundary,
        // and letting this one escape a noexcept function would call
        // std::terminate() instead). An allocation failure here
        // degrades to the SAME outcome the "device unplugged mid-race"
        // branch below already treats as legitimate: this function's
        // own contract ("never fails, only recomputes from whatever it
        // could read") already accepts an empty read.
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
                // A device was unplugged between the two calls (the same
                // race GetRawInputDeviceList's own documented retry-loop
                // example guards against) - this adapter reacts to its own
                // WM_INPUT_DEVICE_CHANGE handler running again shortly
                // after, so treating the RACE itself as "no devices this
                // round" (rather than looping to retry inline) keeps this
                // function's own contract simple: it never fails, only
                // recomputes from whatever it could read.
                devices.clear();
            } else {
                // This second resize() SHRINKS in practice (`filled` is
                // never greater than the `device_count` just allocated
                // for, per the two-call idiom's own contract above) - a
                // shrinking resize() never reallocates, so this call can
                // never actually throw. clang-tidy's bugprone-exception-
                // escape does not make that grow-vs-shrink distinction
                // for std::vector::resize() though: it flagged this
                // exact line as a possible-throw call site on the real
                // server (MSVC STL, CI run 34033291326, 06/09/2026) even
                // after the first resize() above was already guarded -
                // this project's local mingw/libstdc++ toolchain did not
                // catch it either time, the same declared limitation the
                // first resize()'s own comment names. Guarded here for
                // the SAME reason, not because this path is believed to
                // fail in practice: degrading to "no devices this round"
                // matches the race branch immediately above.
                try {
                    devices.resize(filled);
                } catch (const std::bad_alloc &) {
                    devices.clear();
                }
            }
        }
    }

    const int digitizer_bitmask = ::GetSystemMetrics(SM_DIGITIZER);
    translate(devices.empty() ? nullptr : devices.data(), static_cast<UINT>(devices.size()),
              digitizer_bitmask, m_capabilities);
}

void win32_seat_adapter::handle_input_device_change(WPARAM kind, HANDLE device) noexcept {
    m_last_change_kind = kind;
    m_last_change_device = device;
    recompute_capabilities();
}

WNDPROC win32_seat_adapter::previous_wndproc() const noexcept { return m_previous_wndproc; }

win32_seat_adapter::win32_seat_adapter(win32_seat_adapter &&other) noexcept
    : m_window(other.m_window), m_previous_wndproc(other.m_previous_wndproc),
      m_capabilities(other.m_capabilities), m_last_change_kind(other.m_last_change_kind),
      m_last_change_device(other.m_last_change_device) {
    other.m_window = nullptr;
    other.m_previous_wndproc = nullptr;
    if (m_window != nullptr) {
        // Same re-homing display_adapter.hpp's own move members would
        // need if it ever kept a `this`-derived pointer in GWLP_USERDATA
        // across a move (it does not, today - see that header's own
        // move comment); this adapter DOES, so the moved-to instance
        // must repoint GWLP_USERDATA at ITSELF before `other`'s
        // destructor can run, or a message arriving in the window
        // between the move and the destructor would read a dangling
        // adapter pointer back out.
        ::SetWindowLongPtrW(m_window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    }
}

win32_seat_adapter &win32_seat_adapter::operator=(win32_seat_adapter &&other) noexcept {
    if (this != &other) {
        close();
        m_window = other.m_window;
        m_previous_wndproc = other.m_previous_wndproc;
        m_capabilities = other.m_capabilities;
        m_last_change_kind = other.m_last_change_kind;
        m_last_change_device = other.m_last_change_device;
        other.m_window = nullptr;
        other.m_previous_wndproc = nullptr;
        if (m_window != nullptr) {
            ::SetWindowLongPtrW(m_window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        }
    }
    return *this;
}

win32_seat_adapter::~win32_seat_adapter() { close(); }

gltfx_rslt<void> win32_seat_adapter::open(const win32_display_adapter &display) noexcept {
    if (!display.is_open()) {
        // seat_adapter.hpp's own open() comment: window_class_name() is
        // "empty/undefined before open() succeeds" by display_adapter.
        // hpp's own accessor comment - refusing here, by value, is more
        // specific than letting CreateWindowExW fail against an empty
        // class name below (GODS_LAWS.md L-22: no exception, no abort,
        // a caller-supplied argument is invalid_argument, not
        // platform_failure).
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
    // SetWindowLongPtr's own documentation (seat_adapter.hpp's own
    // "MECHANISM" paragraph, GWLP_WNDPROC instance subclassing):
    // "SetWindowLongPtr returns the address of the window's original
    // window procedure" through the same LONG_PTR-typed return
    // GWLP_USERDATA already carries a pointer through above (seat_
    // window_proc, anonymous namespace) - same idiom, same suppression.
    // NOLINTNEXTLINE(performance-no-int-to-ptr) reason: see comment above
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
        // window that has already been created (every WNDCLASSEXW this
        // project registers sets a non-null lpfnWndProc, display_
        // adapter.cpp's own open()), so a null return with no error set
        // is treated the same as a real value here rather than invented
        // special-cased trust.
    }

    RAWINPUTDEVICE devices[2]{};
    devices[0].usUsagePage = k_usage_page_generic_desktop;
    devices[0].usUsage = k_usage_mouse;
    devices[0].dwFlags = RIDEV_DEVNOTIFY;
    devices[0].hwndTarget = window;
    devices[1].usUsagePage = k_usage_page_generic_desktop;
    devices[1].usUsage = k_usage_keyboard;
    devices[1].dwFlags = RIDEV_DEVNOTIFY;
    devices[1].hwndTarget = window;

    ::SetLastError(0);
    const BOOL registered = ::RegisterRawInputDevices(
        devices, static_cast<UINT>(std::size(devices)), sizeof(RAWINPUTDEVICE));
    if (registered == FALSE) {
        const DWORD register_error = ::GetLastError();
        ::DestroyWindow(window);
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::platform_failure).with_os_error_code(register_error));
    }

    m_window = window;
    m_previous_wndproc = previous;
    recompute_capabilities();
    return gltfx_rslt<void>::ok();
}

void win32_seat_adapter::close() noexcept {
    if (m_window != nullptr) {
        // RIDEV_REMOVE first, hwndTarget = NULL (RAWINPUTDEVICE's own
        // documented precondition for that flag, seat_adapter.hpp's own
        // "MECHANISM" paragraph: "If RIDEV_REMOVE is set and the
        // hwndTarget member is not set to NULL, then
        // RegisterRawInputDevices function will fail") - done BEFORE
        // DestroyWindow, while m_window is still a valid handle to name
        // as the target being un-registered from.
        RAWINPUTDEVICE devices[2]{};
        devices[0].usUsagePage = k_usage_page_generic_desktop;
        devices[0].usUsage = k_usage_mouse;
        devices[0].dwFlags = RIDEV_REMOVE;
        devices[0].hwndTarget = nullptr;
        devices[1].usUsagePage = k_usage_page_generic_desktop;
        devices[1].usUsage = k_usage_keyboard;
        devices[1].dwFlags = RIDEV_REMOVE;
        devices[1].hwndTarget = nullptr;
        ::RegisterRawInputDevices(devices, static_cast<UINT>(std::size(devices)),
                                  sizeof(RAWINPUTDEVICE));

        ::DestroyWindow(m_window);
        m_window = nullptr;
        m_previous_wndproc = nullptr;
    }
}

} // namespace glintfx::platform

#endif // defined(_WIN32)
