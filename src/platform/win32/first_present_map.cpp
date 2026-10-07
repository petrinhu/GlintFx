// SPDX-License-Identifier: AGPL-3.0-or-later
#if defined(_WIN32)

#include "platform/win32/first_present_map.hpp"

namespace glintfx::platform {

bool first_present_map::map_once(HWND window) noexcept {
    if (m_done) {
        return true;
    }
    if (window == nullptr) {
        return false;
    }
    if (::IsWindowVisible(window) == 0) {
        // Consumes the STARTUPINFO show command if the launcher gave one.
        ::ShowWindow(window, SW_SHOW);
        if (::IsWindowVisible(window) == 0) {
            // STARTUPINFO swallowed the call above (SW_HIDE launch): show without nCmdShow.
            ::SetWindowPos(window, nullptr, 0, 0, 0, 0,
                           SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);
        }
        if (::IsWindowVisible(window) == 0) {
            return false;
        }
    }
    m_done = true;
    return true;
}

} // namespace glintfx::platform

#endif // defined(_WIN32)
