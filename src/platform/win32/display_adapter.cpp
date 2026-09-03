// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/win32/display_adapter.hpp"

#if defined(_WIN32)

#include <utility>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

// display_adapter.cpp - see display_adapter.hpp's own header comment
// for scope and mechanism. Every Win32 call below is written from
// Microsoft's own current documentation (RegisterClassExW,
// CreateWindowExW, DestroyWindow, UnregisterClassW), reusing the exact
// pattern tests/win32_runner_probe_test.cpp already proved live on the
// windows-latest GitHub Actions runner - not from having watched THIS
// file run, since this project has no usable Windows toolchain on the
// machine that wrote it (declared, GODS_LAWS.md L-27/L-44, same as
// that probe file's own header comment).

namespace glintfx::platform {

namespace {

// A name specific to this adapter, distinct from
// tests/win32_runner_probe_test.cpp's own k_window_class_name - the
// two never link into the same executable (this is production code
// under glintfx_library, the probe is its own separate test binary),
// but a distinct name keeps that true even if that ever changes, and
// documents at a glance which class a given HWND belongs to.
constexpr wchar_t k_window_class_name[] = L"GlintfxDisplayAdapterWindowClass";

// window_proc: this adapter never shows the window it creates (parent
// = HWND_MESSAGE, see open() below), so there is nothing for this
// callback to do except hand every message back to the default
// handler - same shape tests/win32_runner_probe_test.cpp's own
// probe_window_proc uses when it is not counting a specific message.
LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    return ::DefWindowProcW(hwnd, msg, wparam, lparam);
}

} // namespace

win32_display_adapter::win32_display_adapter(win32_display_adapter &&other) noexcept
    : m_window(other.m_window), m_class_atom(other.m_class_atom) {
    other.m_window = nullptr;
    other.m_class_atom = 0;
}

win32_display_adapter &win32_display_adapter::operator=(win32_display_adapter &&other) noexcept {
    if (this != &other) {
        close();
        m_window = other.m_window;
        m_class_atom = other.m_class_atom;
        other.m_window = nullptr;
        other.m_class_atom = 0;
    }
    return *this;
}

win32_display_adapter::~win32_display_adapter() { close(); }

gltfx_rslt<void> win32_display_adapter::open() noexcept {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = 0;
    wc.lpfnWndProc = &window_proc;
    wc.hInstance = ::GetModuleHandleW(nullptr);
    wc.lpszClassName = k_window_class_name;

    ::SetLastError(0);
    const ATOM atom = ::RegisterClassExW(&wc);
    if (atom == 0) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::platform_failure).with_os_error_code(::GetLastError()));
    }

    // HWND_MESSAGE: the documented Win32 pattern for a window that
    // needs a valid HWND but is never painted, shown, or reachable via
    // Alt+Tab or the taskbar - see this adapter's own header comment
    // ("MECHANISM"). WS_OVERLAPPEDWINDOW's usual position/size
    // arguments are meaningless for a message-only window; CW_USEDEFAULT
    // and a nominal size are passed only because CreateWindowExW
    // requires SOME values there.
    ::SetLastError(0);
    HWND window = ::CreateWindowExW(0, k_window_class_name, L"", 0, 0, 0, 0, 0, HWND_MESSAGE,
                                    nullptr, ::GetModuleHandleW(nullptr), nullptr);
    if (window == nullptr) {
        const DWORD create_error = ::GetLastError();
        ::UnregisterClassW(k_window_class_name, ::GetModuleHandleW(nullptr));
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::platform_failure).with_os_error_code(create_error));
    }

    m_window = window;
    m_class_atom = atom;
    return gltfx_rslt<void>::ok();
}

void win32_display_adapter::close() noexcept {
    if (m_window != nullptr) {
        // Reverse order of creation (same "teardown in reverse" shape
        // wayland_display_adapter::close() documents): the window is
        // destroyed before the class it was created from is
        // unregistered - UnregisterClassW refuses (ERROR_CLASS_HAS_WINDOWS)
        // while any window of that class is still alive.
        ::DestroyWindow(m_window);
        m_window = nullptr;
        if (m_class_atom != 0) {
            ::UnregisterClassW(k_window_class_name, ::GetModuleHandleW(nullptr));
            m_class_atom = 0;
        }
    }
}

} // namespace glintfx::platform

#endif // defined(_WIN32)
