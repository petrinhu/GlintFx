// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/win32/window_adapter.hpp"

#if defined(_WIN32)

#include "platform/win32/window_message_route.hpp"

// window_message_route.cpp - split out of window_adapter.cpp (fix for
// the link defect the windows-latest runner found live, GODS_LAWS.md
// L-04): win32_display_connect_test, win32_display_refusal_test,
// two_displays_test, win32_seat_translation_test and seat_test
// (tests/CMakeLists.txt) each compile display_adapter.cpp (two of them
// also seat_adapter.cpp) on their OWN, without window_adapter.cpp,
// because they test win32_display_adapter/win32_seat_adapter in
// isolation and have nothing to do with win32_window_adapter's own
// open()/close()/set_title(). display_adapter.cpp's own shared
// window_proc has, since window_message_route.hpp's own fatia, a real
// call to win32_window_adapter_route_message() (declared there,
// defined here) - that reference has to resolve at LINK time even in
// a fixture that will never actually reach it at RUN time (none of
// those five ever create a real, non-message-only window - see that
// header's own "SAFE ONLY BECAUSE OF WHAT THE FOUR MESSAGE TYPES
// THEMSELVES MEAN" paragraph for why the branch that calls it can
// never fire against their own message-only windows).
//
// Pulling the WHOLE of window_adapter.cpp into those five targets
// would also have fixed the link, but drags open()/set_title()'s own
// dependencies (app_user_model_id.cpp - and its shell32/ole32/propsys
// libraries - plus window/window_desc_validation.cpp and window/
// utf8_validation.cpp) into five test binaries that never call either
// method and have no reason to care whether that unrelated code even
// compiles (GODS_LAWS.md L-32: a surgical fix does not drag in
// behavior nobody asked for). handle_message() and the trampoline that
// reaches it never call into any of that - the only EXTRA link
// dependency this file's own object needs is platform/window/
// window_state.cpp (window_state's member functions are out-of-line).
//
// This is also a genuine atom by GODS_LAWS.md L-17's own "a pergunta
// das leis" test, not just a workaround: window_adapter.cpp changes
// for window LIFECYCLE reasons (D-W5-13, D-W6a-21, D-W6a-23); this
// file changes for MESSAGE ROUTING reasons (sec. 5 risk 2/6 of docs/
// plano-w6a-janela.md) - two unrelated reasons to change were sharing
// one file before this split, the same "o .cpp de application_id e um
// atomo separado" precedent src/platform/win32/CMakeLists.txt already
// documents for app_user_model_id.cpp.
//
// handle_message()'s own DECLARATION stays in window_adapter.hpp (it
// is still a member of win32_window_adapter) - only the out-of-line
// DEFINITION moves here, next to the free function that was always the
// other half of the same coupling surface (window_message_route.hpp's
// own header comment carries the full safety argument for both).

namespace glintfx::platform {

// window_message_route.hpp's own free function - the type-erased
// coupling surface display_adapter.cpp's own shared window_proc calls
// through, without ever including this file's own header. See that
// header's own comment for the full safety argument (why `self` is
// always genuinely a win32_window_adapter* here).
bool win32_window_adapter_route_message(void *self, HWND hwnd, UINT msg, WPARAM wparam,
                                        LPARAM lparam, LRESULT &out_result) noexcept {
    auto *adapter = static_cast<win32_window_adapter *>(self);
    return adapter->handle_message(hwnd, msg, wparam, lparam, out_result);
}

bool win32_window_adapter::handle_message(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam,
                                          LRESULT &out_result) noexcept {
    switch (msg) {
    case WM_SIZE: {
        // WM_SIZE's own documented lParam (learn.microsoft.com/windows/
        // win32/winmsg/wm-size): the LOW/HIGH words already ARE the new
        // CLIENT AREA width/height in pixels - no GetClientRect() call
        // needed, and no read of desc.logical_width/height either (this
        // header's own "TWO SIZES" paragraph, sec. 5 risk 2 of the
        // plan): THIS is "the aviso de dimensao itself", converted back
        // to the 96-DPI logical baseline via m_dpi.
        const auto width_px = static_cast<std::uint32_t>(LOWORD(lparam));
        const auto height_px = static_cast<std::uint32_t>(HIWORD(lparam));
        const auto logical_width = static_cast<std::uint32_t>(
            ::MulDiv(static_cast<int>(width_px), 96, static_cast<int>(m_dpi)));
        const auto logical_height = static_cast<std::uint32_t>(
            ::MulDiv(static_cast<int>(height_px), 96, static_cast<int>(m_dpi)));
        m_state.apply_logical_size(logical_width, logical_height);
        // SIZE_MAXIMIZED/SIZE_RESTORED (WM_SIZE's own wParam values) -
        // SIZE_MINIMIZED is out of D-W5-3's "v1 congela so o minimo"
        // scope (window_state_bit has no minimized slot), silently
        // folded into "not maximized", same "unknown/newer state
        // ignored" shape window_configure_sequence.hpp's own header
        // comment documents for the Wayland side.
        m_state.set_state(window_state_bit::maximized, wparam == SIZE_MAXIMIZED);
        out_result = 0; // WM_SIZE's own documented return value.
        return true;
    }
    case WM_CLOSE:
        // Marks the one-way latch and returns 0 WITHOUT EVER reaching
        // DefWindowProcW (sec. 5 risk 6 of the plan): DefWindowProcW's
        // own default WM_CLOSE handling calls DestroyWindow, which
        // would leave m_window a dangling HWND while close_requested()
        // stayed true - close() (window_adapter.cpp) is the ONLY caller
        // that ever destroys this window.
        m_state.request_close();
        out_result = 0;
        return true;
    case WM_ACTIVATE: {
        // WM_ACTIVATE's own documented wParam low-order word: WA_ACTIVE
        // (1), WA_CLICKACTIVE (2), WA_INACTIVE (0) - "active" here means
        // anything other than WA_INACTIVE.
        const WORD activation = LOWORD(wparam);
        m_state.set_state(window_state_bit::active, activation != WA_INACTIVE);
        out_result = 0;
        return true;
    }
    case WM_DPICHANGED: {
        // WM_DPICHANGED's own documented wParam: LOWORD is the new
        // X-axis DPI, "identical" to the Y-axis value for this
        // project's own windows (learn.microsoft.com/windows/win32/
        // hidpi/wm-dpichanged) - lParam points to a RECT with the
        // suggested new window rect, screen coordinates, already
        // scaled for the new DPI.
        const auto new_dpi = static_cast<std::uint32_t>(LOWORD(wparam));
        m_dpi = new_dpi;
        m_state.apply_dpi(new_dpi);
        // Guarded on `hwnd` (never on `lparam` alone: a real message
        // always carries a real RECT pointer here) so win32_message_
        // translation_test can call this method directly with
        // hwnd == nullptr and exercise only the pure m_state update,
        // the same "mensagens sinteticas" shape wayland_window_
        // adapter's own null-proxy callback tests already use -
        // SetWindowPos against a null/invalid handle is a documented
        // failure (returns FALSE, GetLastError() ERROR_INVALID_WINDOW_
        // HANDLE), never called here on purpose.
        if (hwnd != nullptr) {
            // WM_DPICHANGED's own documented lParam IS a RECT* smuggled
            // through the integer-typed LPARAM (learn.microsoft.com/
            // windows/win32/hidpi/wm-dpichanged) - unavoidable at this
            // Win32 message boundary, the same idiom display_adapter.cpp's
            // own WM_NCCREATE/GWLP_USERDATA casts already carry the same
            // suppression for.
            // NOLINTNEXTLINE(performance-no-int-to-ptr) reason: see comment above
            const auto *suggested = reinterpret_cast<const RECT *>(lparam);
            ::SetWindowPos(hwnd, nullptr, suggested->left, suggested->top,
                           suggested->right - suggested->left, suggested->bottom - suggested->top,
                           SWP_NOZORDER | SWP_NOACTIVATE);
        }
        out_result = 0;
        return true;
    }
    default:
        return false;
    }
}

} // namespace glintfx::platform

#endif // defined(_WIN32)
