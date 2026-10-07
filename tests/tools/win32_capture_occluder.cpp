// SPDX-License-Identifier: AGPL-3.0-or-later
#include "win32_capture_occluder.hpp"

#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "win32_capture_text.hpp"

namespace glintfx::capture_tool {

namespace {

// Magenta: not one of the scene's colors (red, blue, white and half pieces over transparent black).
constexpr COLORREF k_cover_color = RGB(255, 0, 255);
constexpr int k_paint_pump_ms = 150;
constexpr int k_pump_slice_ms = 10;

HWND as_window(void *opaque) { return static_cast<HWND>(opaque); }

std::wstring class_name_wide() { return utf8_to_wide(std::string(k_occluder_class)); }

// Dispatches every message queued for this thread's windows, for about `milliseconds`.
void pump_for(int milliseconds) {
    const ULONGLONG deadline = ::GetTickCount64() + static_cast<ULONGLONG>(milliseconds);
    while (::GetTickCount64() < deadline) {
        MSG message{};
        while (::PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != 0) {
            ::TranslateMessage(&message);
            ::DispatchMessageW(&message);
        }
        ::Sleep(k_pump_slice_ms);
    }
}

bool register_cover_class(const std::wstring &name) {
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = ::DefWindowProcW;
    window_class.hInstance = ::GetModuleHandleW(nullptr);
    window_class.hbrBackground = ::CreateSolidBrush(k_cover_color);
    window_class.lpszClassName = name.c_str();
    return window_class.hbrBackground != nullptr && ::RegisterClassExW(&window_class) != 0;
}

} // namespace

bool window_occluder::cover(const pixel_rect &area) {
    const std::wstring name = class_name_wide();
    if (!register_cover_class(name)) {
        return false;
    }
    m_class_registered = true;
    const int width = area.right - area.left;
    const int height = area.bottom - area.top;
    // WS_POPUP, TOPMOST and NOACTIVATE; enabled (no WS_DISABLED) and not WS_EX_TRANSPARENT.
    const HWND window = ::CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, name.c_str(), L"", WS_POPUP, area.left,
        area.top, width, height, nullptr, nullptr, ::GetModuleHandleW(nullptr), nullptr);
    if (window == nullptr) {
        return false;
    }
    m_window = window;
    ::SetWindowPos(window, HWND_TOPMOST, area.left, area.top, width, height,
                   SWP_NOACTIVATE | SWP_SHOWWINDOW);
    ::ShowWindow(window, SW_SHOWNOACTIVATE);
    ::UpdateWindow(window);
    pump_for(k_paint_pump_ms);
    return ::IsWindowVisible(window) != 0;
}

window_occluder::~window_occluder() {
    if (m_window != nullptr) {
        ::DestroyWindow(as_window(m_window));
    }
    if (m_class_registered) {
        ::UnregisterClassW(class_name_wide().c_str(), ::GetModuleHandleW(nullptr));
    }
}

} // namespace glintfx::capture_tool
