// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/win32/window_adapter.hpp"

#if defined(_WIN32)

#include <new>
#include <string>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

#include "platform/win32/app_user_model_id.hpp"
#include "platform/window/window_desc_validation.hpp"

// window_adapter.cpp - see window_adapter.hpp's own header comment for
// scope. handle_message() (and the window_message_route.hpp free
// function that reaches it from display_adapter.cpp's own shared
// window_proc) live in window_message_route.cpp, NOT here - split out
// on purpose so five test fixtures that recompile display_adapter.cpp
// in isolation (win32_display_connect_test and its four siblings,
// tests/CMakeLists.txt) can satisfy that reference without also
// linking this file's own open()/set_title() dependencies (app_user_
// model_id.cpp, window/window_desc_validation.cpp) - see window_
// message_route.cpp's own header comment for the full reasoning.
// Written from Microsoft's own current documentation (each mechanism
// cited at its own call site) - this project has no usable Windows
// toolchain on the machine that wrote it (GODS_LAWS.md L-27, same
// declared limitation display_adapter.cpp's own header comment already
// uses for the fatia this one builds on).

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
    // resize() CAN throw std::bad_alloc despite this function's own
    // noexcept - `out` is caller-supplied and unbounded by anything
    // this function controls, the same "not realistically
    // engineered-around, but must not reach the caller as a crash"
    // shape global_catalog.cpp's own insert() already handles for an
    // unrelated std::vector allocation, and the same reasoning core/
    // err.cpp's own with_path()/with_rejected_value() already document:
    // letting this escape a noexcept function calls std::terminate(),
    // exactly the process-abort GODS_LAWS.md L-22 and this project's
    // "a lib NUNCA aborta o processo do consumidor" rule forbid.
    // Caught here and reported through the existing bool contract -
    // the caller already treats `false` as "could not widen", the
    // identical outcome a real encoding failure two lines below
    // produces.
    try {
        out.resize(static_cast<std::size_t>(needed));
    } catch (const std::bad_alloc &) {
        return false;
    }
    const int written = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                              static_cast<int>(value.size()), out.data(), needed);
    return written == needed;
}

} // namespace

win32_window_adapter::win32_window_adapter(win32_window_adapter &&other) noexcept
    : m_window(other.m_window), m_dpi(other.m_dpi),
      m_size_messages_before_open_returns(other.m_size_messages_before_open_returns),
      m_state(other.m_state) {
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
        m_size_messages_before_open_returns = other.m_size_messages_before_open_returns;
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
    //
    // ASSIGNED TO m_dpi/m_state HERE, BEFORE CreateWindowExW (achado do
    // time-lead, fatia de 06/09/2026, defeito real): an earlier version
    // of this function assigned m_dpi only AFTER CreateWindowExW
    // returned, alongside m_window - but window_message_route.hpp's own
    // header comment (and this header's own "WHY THE FIRST WM_SIZE
    // NEEDS A SEPARATE ROUTING PATH" paragraph) both document that a
    // WM_SIZE can arrive DURING that very call, routed through GWLP_
    // USERDATA (already `this` since WM_NCCREATE) straight into handle_
    // message() - which reads m_dpi to convert that message's pixel
    // rect back to the 96-DPI logical baseline. Assigning m_dpi after
    // CreateWindowExW returns left exactly that window converting
    // against the DEFAULT 96 instead of the real DPI whenever a WM_SIZE
    // genuinely arrived mid-creation - GODS_LAWS.md L-27's own "afirmar
    // valor sem ter lido" defect family, just one member behind
    // schedule instead of never read at all. m_state.apply_dpi() moves
    // here too for the same reason WIN-SIZE-AT-OPEN needs it seeded
    // (below): pixel_size()'s own derived formula (window_state.hpp's
    // "ONE FORMULA, TWO FACTORS") is wrong the instant open() returns
    // if m_state still carries its default-constructed 96 while the
    // window's REAL dpi is something else.
    const UINT dpi = ::GetDpiForSystem();
    m_dpi = dpi;
    m_state.apply_dpi(dpi);

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

    // WIN-SIZE-AT-OPEN (docs/plano-w6a-janela.md, achado do time-lead
    // 06/09/2026, PARITY-JOB-NOT-CANCELLED's own sibling finding): the
    // contract is "the instant open() returns successfully, logical_
    // size() reports the client size the window ACTUALLY HAS" - never a
    // copy of the request (window_parity_test.cpp measured exactly that
    // divergence live: logical_size() came back 0x0 on Windows right
    // after a successful open()). Read synchronously, right here,
    // before this function returns: GetClientRect() on the just-created
    // window reflects whatever WM_SIZE messages already ran during
    // CreateWindowExW (this header's own "WHY THE FIRST WM_SIZE NEEDS A
    // SEPARATE ROUTING PATH" paragraph) AND the window's true starting
    // size either way, so this line is correct regardless of whether
    // that routing fired zero or several times before this point -
    // never seeded from desc.logical_width/height (that would be
    // asserting a value never actually read, the same defect class the
    // m_dpi ordering fix above closes one member over). A failure here
    // is treated the same conservative way AdjustWindowRectExForDpi's
    // own failure above is (GODS_LAWS.md L-22: no exception, no abort) -
    // the window this function's whole contract promises already
    // exists and is valid regardless.
    RECT client_rect{};
    if (::GetClientRect(m_window, &client_rect) != 0) {
        const auto logical_width = static_cast<std::uint32_t>(
            ::MulDiv(client_rect.right - client_rect.left, 96, static_cast<int>(m_dpi)));
        const auto logical_height = static_cast<std::uint32_t>(
            ::MulDiv(client_rect.bottom - client_rect.top, 96, static_cast<int>(m_dpi)));
        m_state.apply_logical_size(logical_width, logical_height);
    }

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

} // namespace glintfx::platform

#endif // defined(_WIN32)
