// SPDX-License-Identifier: AGPL-3.0-or-later
#if defined(_WIN32)

#include "platform/win32/first_present_map.hpp"

namespace glintfx::platform {

DWORD show_window_now(HWND window) noexcept {
    if (window == nullptr) {
        return ERROR_INVALID_WINDOW_HANDLE;
    }
    if (::IsWindowVisible(window) != 0) {
        return 0;
    }
    // Consumes the STARTUPINFO show command if the launcher gave one.
    ::ShowWindow(window, SW_SHOW);
    if (::IsWindowVisible(window) != 0) {
        return 0;
    }
    // STARTUPINFO swallowed the call above (SW_HIDE launch): show without nCmdShow.
    ::SetLastError(0);
    const BOOL positioned = ::SetWindowPos(window, nullptr, 0, 0, 0, 0,
                                           SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);
    const DWORD reported = positioned != 0 ? 0 : ::GetLastError();
    if (positioned != 0 && ::IsWindowVisible(window) != 0) {
        return 0;
    }
    return reported != 0 ? reported : ERROR_INVALID_STATE;
}

DWORD first_present_map::map_once(HWND window) noexcept {
    if (m_done) {
        return 0;
    }
    const DWORD error = m_show(window);
    if (error == 0) {
        m_done = true;
    }
    return error;
}

} // namespace glintfx::platform

#endif // defined(_WIN32)
