// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/win32/window_adapter.hpp"

#if defined(_WIN32)

#include <string>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

#include "platform/win32/app_user_model_id.hpp"
#include "platform/win32/window_message_route.hpp"
#include "platform/window/window_desc_validation.hpp"

// window_adapter.cpp - see window_adapter.hpp's own header comment for
// scope, and window_message_route.hpp's own for why display_adapter.cpp
// (not this file) routes WM_SIZE/WM_CLOSE/WM_ACTIVATE/WM_DPICHANGED to
// handle_message() below. Written from Microsoft's own current
// documentation (each mechanism cited at its own call site) - this
// project has no usable Windows toolchain on the machine that wrote it
// (GODS_LAWS.md L-27, same declared limitation display_adapter.cpp's
// own header comment already uses for the fatia this one builds on).

namespace glintfx::platform {

namespace {

// Same "refuse rather than silently substitute" widening app_user_
// model_id.cpp's own widen_utf8() already uses - WET on purpose (two
// call sites total in this file, GODS_LAWS.md L-17 "regra de 3": a
// third occurrence anywhere in src/platform/win32/ is what would move
// this into a shared atom, not two).
[[nodiscard]] bool widen_utf8(std::string_view value, std::wstring &out) noexcept {
    if (value.empty()) {
        out.clear();
        return true;
    }
    const int needed = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                             static_cast<int>(value.size()), nullptr, 0);
    if (needed <= 0) {
        return false;
    }
    out.resize(static_cast<std::size_t>(needed));
    const int written = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                              static_cast<int>(value.size()), out.data(), needed);
    return written == needed;
}

} // namespace

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

win32_window_adapter::win32_window_adapter(win32_window_adapter &&other) noexcept
    : m_window(other.m_window), m_dpi(other.m_dpi), m_state(other.m_state) {
    other.m_window = nullptr;
    if (m_window != nullptr) {
        // Re-home GWLP_USERDATA at the new address BEFORE `other`'s
        // destructor can run - same reasoning win32_seat_adapter's own
        // move members already document for the identical hazard.
        ::SetWindowLongPtrW(m_window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    }
}

win32_window_adapter &win32_window_adapter::operator=(win32_window_adapter &&other) noexcept {
    if (this != &other) {
        close();
        m_window = other.m_window;
        m_dpi = other.m_dpi;
        m_state = other.m_state;
        other.m_window = nullptr;
        if (m_window != nullptr) {
            ::SetWindowLongPtrW(m_window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        }
    }
    return *this;
}

win32_window_adapter::~win32_window_adapter() { close(); }

gltfx_rslt<void> win32_window_adapter::open(const win32_display_adapter &display,
                                            const win32_window_desc &desc) noexcept {
    // window_class_name() is "empty/undefined before open() succeeds"
    // (display_adapter.hpp's own accessor comment) - same precondition
    // shape win32_seat_adapter::open() already uses for the identical
    // dependency on `display`.
    if (!display.is_open()) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value("display"));
    }

    // D-W6a-23, validated BEFORE either backend ever sees the bytes -
    // same order wayland_window_adapter::apply_desc() already uses.
    if (gltfx_rslt<void> title_ok = validate_window_text_field("title", desc.title);
        title_ok.has_error()) {
        return title_ok;
    }
    if (gltfx_rslt<void> app_id_ok =
            validate_window_text_field("application_id", desc.application_id);
        app_id_ok.has_error()) {
        return app_id_ok;
    }

    std::wstring wide_title;
    if (!widen_utf8(desc.title, wide_title)) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value("title"));
    }

    // No WS_VISIBLE (D-W5-13/this header's own top comment): the window
    // exists, and can be found by native_handle()/IsWindow(), but shows
    // nothing until whoever paints the first frame maps it - never this
    // adapter's own job.
    constexpr DWORD style = WS_OVERLAPPEDWINDOW;
    constexpr DWORD ex_style = 0;

    // Seeded here, tracked in m_dpi from now on (window_adapter.hpp's
    // own "m_dpi" field comment) - the exact DPI AdjustWindowRectExForDpi
    // below sizes the window FOR, so the first WM_SIZE this window ever
    // receives converts against the same number it was created with.
    const UINT dpi = ::GetDpiForSystem();

    // D-W5-13's own sibling rule: 0x0 requested means CW_USEDEFAULT
    // ("let the system decide"), never a literal zero-sized window -
    // this header's own "TWO SIZES" paragraph explains why the ACTUAL
    // size still only ever reaches window_state through a real WM_SIZE,
    // never from this computation.
    int create_width = CW_USEDEFAULT;
    int create_height = CW_USEDEFAULT;
    if (desc.logical_width != 0 && desc.logical_height != 0) {
        RECT rect{0, 0, ::MulDiv(static_cast<int>(desc.logical_width), static_cast<int>(dpi), 96),
                  ::MulDiv(static_cast<int>(desc.logical_height), static_cast<int>(dpi), 96)};
        // AdjustWindowRectExForDpi (learn.microsoft.com/windows/win32/
        // api/winuser/nf-winuser-adjustwindowrectexfordpi): expands a
        // desired CLIENT rect into the OUTER window rect CreateWindowExW
        // itself expects. A failure here is treated the same as a 0x0
        // request (CW_USEDEFAULT) - GODS_LAWS.md L-22, no exception, no
        // abort over a sizing hint the system remains free to override
        // regardless (CreateWindowExW's own screen-bounds clipping,
        // WM_GETMINMAXINFO).
        if (::AdjustWindowRectExForDpi(&rect, style, FALSE, ex_style, dpi) != 0) {
            create_width = rect.right - rect.left;
            create_height = rect.bottom - rect.top;
        }
    }

    // lpParam = this: display_adapter.cpp's own shared window_proc
    // installs it into GWLP_USERDATA during WM_NCCREATE - the FIRST
    // message this window ever receives, strictly before the WM_SIZE
    // window_message_route.hpp's own header comment explains arrives
    // next, during this SAME CreateWindowExW call.
    ::SetLastError(0);
    HWND window =
        ::CreateWindowExW(ex_style, display.window_class_name(), wide_title.c_str(), style,
                          CW_USEDEFAULT, CW_USEDEFAULT, create_width, create_height, nullptr,
                          nullptr, ::GetModuleHandleW(nullptr), this);
    if (window == nullptr) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::platform_failure).with_os_error_code(::GetLastError()));
    }

    m_window = window;
    m_dpi = dpi;

    // D-W6a-21, best-effort (window_adapter.hpp's own open() comment):
    // a failure here does not fail open() itself - the window this
    // function's whole contract promises already exists and is valid.
    // (void): gltfx_rslt<void> is [[nodiscard]] (core/err.hpp); an
    // explicit cast to void is the standard-recognized way to discard
    // it on purpose, at the call site, rather than silencing the
    // warning globally.
    if (!desc.application_id.empty()) {
        (void)set_window_application_id(m_window, desc.application_id);
    }

    return gltfx_rslt<void>::ok();
}

void win32_window_adapter::close() noexcept {
    if (m_window != nullptr) {
        // The window's own class stays registered - owned by the
        // win32_display_adapter that registered it (display_adapter.
        // hpp's own F2/D-W6a-16 paragraph), torn down by THAT adapter's
        // own close(), never by this one.
        ::DestroyWindow(m_window);
        m_window = nullptr;
    }
}

gltfx_rslt<void> win32_window_adapter::set_title(std::string_view title) noexcept {
    if (!is_open()) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value("title"));
    }
    if (gltfx_rslt<void> title_ok = validate_window_text_field("title", title);
        title_ok.has_error()) {
        return title_ok;
    }

    std::wstring wide_title;
    if (!widen_utf8(title, wide_title)) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value("title"));
    }

    ::SetLastError(0);
    if (::SetWindowTextW(m_window, wide_title.c_str()) == 0) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::platform_failure).with_os_error_code(::GetLastError()));
    }
    return gltfx_rslt<void>::ok();
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
        // stayed true - close() (above) is the ONLY caller that ever
        // destroys this window.
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
