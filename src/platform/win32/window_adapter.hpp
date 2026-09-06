// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdint>
#include <string_view>

#include <glintfx/core/err.hpp>

#include "platform/win32/display_adapter.hpp"
#include "platform/window/window_state.hpp"

// platform/win32/window_adapter.hpp - X-2 (docs/plano-w6a-janela.md
// fatia 9, TODO.md WIN-WINDOW): the Windows counterpart of WL-WINDOW's
// own wayland_window_adapter (src/platform/wayland/window_adapter.hpp,
// fatia 8, W-E) - the one caller that turns an already-open win32_
// display_adapter into a real, top-level Win32 window, sharing that
// SAME registered window class (win32_display_adapter::window_class_
// name(), display_adapter.hpp's own "F2/D-W6a-16" paragraph) instead
// of a second RegisterClassExW under a second name - exactly the reuse
// that header's own comment, and display_adapter.cpp's own window_proc
// comment, already promise this fatia.
//
// D-W5-8/D-W5-13 (confirmed for this fatia too): the window is created
// WITHOUT WS_VISIBLE and open() never calls ShowWindow - a window
// "nasce invisivel ate o primeiro desenho" on both platforms; mapping/
// showing is a later fatia's job (whoever paints the first frame),
// never this adapter's.
//
// WHY THE FIRST WM_SIZE NEEDS A SEPARATE ROUTING PATH (sec. 5 risk 2 of
// the plan, "o primeiro WM_SIZE chega DURANTE CreateWindowExW"):
// win32_seat_adapter (seat_adapter.hpp, already delivered) installs its
// own instance behavior by subclassing GWLP_WNDPROC AFTER CreateWindowExW
// returns - safe for seat because WM_INPUT_DEVICE_CHANGE is an
// asynchronous hotplug notification that can never fire during
// creation. WM_SIZE is different: DefWindowProcW synthesizes it while
// processing WM_WINDOWPOSCHANGED (learn.microsoft.com/windows/win32/
// winmsg/wm-size, Remarks), and CreateWindowExW drives that whole
// sequence BEFORE returning to open() - by the time this adapter could
// subclass its own instance, the very first WM_SIZE has already come
// and gone. window_message_route.hpp is how this fatia reaches back
// into display_adapter.cpp's own shared, class-level window_proc (the
// ONE piece of code that runs early enough) without display_adapter.cpp
// ever depending on this header - see that file's own comment for the
// full safety argument.
//
// TWO SIZES, THE FORMULA LIVES IN window_state (decision 15,
// window_state.hpp's own "ONE FORMULA, TWO FACTORS" paragraph): this
// adapter never derives pixel_size() itself. handle_message()'s own
// WM_SIZE case converts the pixel client-area size WM_SIZE's own lParam
// already carries (learn.microsoft.com/windows/win32/winmsg/wm-size,
// "lParam... the new width/height of the client area") back to the
// 96-DPI logical baseline using m_dpi (this adapter's own tracked copy,
// fed by WM_DPICHANGED and seeded from GetDpiForSystem() in open()) and
// calls window_state::apply_logical_size() with THAT - never with
// desc.logical_width/height copied verbatim (the exact "passa verde e
// esta quebrado" shape sec. 5 risk 2 names: a test that only checks
// "not zero" cannot tell a real WM_SIZE reading from a blind copy of
// the request).
namespace glintfx::platform {

// The INTERNAL descriptor this fatia's own open() takes - NOT the
// public gltfx_window_desc (not built yet: include/glintfx/platform/
// window/window.hpp's own "WHAT THIS HEADER DELIBERATELY DOES NOT
// FREEZE YET" paragraph). Same field names/units as wayland_window_desc
// (src/platform/wayland/window_adapter.hpp) and as decision 15's own
// public struct, so a future translation layer copies field for field,
// never redesigns.
struct win32_window_desc {
    std::uint32_t logical_width = 0;
    std::uint32_t logical_height = 0;
    std::string_view title;
    std::string_view application_id;
};

class win32_window_adapter {
  public:
    // Starts CLOSED (m_window null) - same default_initializable<A>
    // shape every other adapter in this project has.
    win32_window_adapter() noexcept = default;

    // PINNED, NEVER MOVABLE (FACADE-PIN, docs/plano-conserto-fachadas-
    // uaf.md sec. 6/7.2, varredura #9): GWLP_USERDATA on the live HWND
    // holds THIS instance's own address (installed by display_adapter.
    // cpp's own window_proc during WM_NCCREATE, lpParam = this, open()
    // below) - copying would hand two owners the same window. This
    // class used to re-point GWLP_USERDATA at the new address on every
    // move (the ONE adapter in this project that remembered to, per
    // this plan's own sec. 3/5) - that re-homing code is DELETED here,
    // not kept as a second safety net alongside the pin: window_
    // adapter_port now requires pinned_adapter<A>, and a second,
    // hand-written mechanism doing the same job the concept already
    // guarantees is exactly the class of defect this plan exists to
    // stop relying on (D-UAF-1 - "a opcao B... mantem exatamente essa
    // dependencia e a esconde melhor").
    win32_window_adapter(const win32_window_adapter &) = delete;
    win32_window_adapter &operator=(const win32_window_adapter &) = delete;
    win32_window_adapter(win32_window_adapter &&) = delete;
    win32_window_adapter &operator=(win32_window_adapter &&) = delete;

    // Idempotent close() in the destructor, same shape every other
    // adapter in this project has.
    ~win32_window_adapter();

    // `display` must already be open() (window_class_name() is "empty/
    // undefined before open() succeeds" by that header's own accessor
    // comment) - refused as invalid_argument, same precondition shape
    // win32_seat_adapter::open() already uses for the identical
    // dependency. Validates title/application_id (D-W6a-23, window_
    // desc_validation.hpp) BEFORE either ever reaches the OS, same
    // "validated once, common to both backends" discipline wayland_
    // window_adapter::apply_desc() already documents. Converts
    // desc.logical_width/height (96-DPI baseline) to a pixel window
    // rect via AdjustWindowRectExForDpi()+GetDpiForSystem() (0x0 in
    // either field means CW_USEDEFAULT, same "let the system decide"
    // shape as D-W5-13's own sibling rule), creates the window WITHOUT
    // WS_VISIBLE (D-W5-13, no ShowWindow call anywhere in this file),
    // and best-effort applies application_id (D-W6a-21, app_user_model_
    // id.hpp) when non-empty - a failure there does not fail open()
    // itself (conservative choice, declared: this fatia's own plan
    // leaves how that failure propagates to a future revision of the
    // public API, not this internal adapter). Tears down whatever this
    // call had already created before returning on any OTHER failure -
    // is_open() reads false either way.
    [[nodiscard]] gltfx_rslt<void> open(const win32_display_adapter &display,
                                        const win32_window_desc &desc) noexcept;

    // Idempotent-safe: does nothing when already closed. DestroyWindow
    // only - the window's own class stays registered (owned by the
    // win32_display_adapter that registered it, torn down by ITS OWN
    // close(), never by this one).
    void close() noexcept;

    [[nodiscard]] bool is_open() const noexcept { return m_window != nullptr; }

    // D-W5-2 (confirmed): consultable state, never an event queue - see
    // window_state.hpp's own header comment for the full reasoning.
    [[nodiscard]] const window_state &state() const noexcept { return m_state; }

    // MultiByteToWideChar + SetWindowTextW (docs/plano-w6a-janela.md
    // fatia 9's own row) - validated the same way open() validates
    // `desc.title`. Refused as invalid_argument when !is_open().
    [[nodiscard]] gltfx_rslt<void> set_title(std::string_view title) noexcept;

    // Test seam (win32_window_close_request_test, win32_message_
    // translation_test): the raw handle, so a test can SendMessageW()
    // a synthetic WM_CLOSE directly and then confirm IsWindow() still
    // true afterward (sec. 5 risk 6 of the plan) - never used by
    // open()/close()/handle_message() themselves.
    [[nodiscard]] HWND native_handle() const noexcept { return m_window; }

    // INTERNAL SEAM, public only because window_message_route.hpp's
    // own win32_window_adapter_route_message() (window_adapter.cpp,
    // called from display_adapter.cpp's own shared window_proc) needs
    // to call it from outside this class - see that header's own
    // comment for why this coupling is safe without either adapter
    // knowing the other's full type. Translates WM_SIZE/WM_CLOSE/
    // WM_ACTIVATE/WM_DPICHANGED into window_state (and, for WM_
    // DPICHANGED, repositions the real window via SetWindowPos when
    // `hwnd` is a live handle - guarded so win32_message_translation_
    // test can call this directly with hwnd == nullptr and exercise
    // only the pure state update, the same "mensagens sinteticas"
    // technique wayland_window_adapter's own static callbacks use with
    // a null proxy). Returns true (with `out_result`) when this method
    // itself handled `msg` - window_proc then returns `out_result`
    // WITHOUT ever calling DefWindowProcW for it (sec. 5 risk 6: this
    // is what keeps WM_CLOSE from ever reaching DefWindowProcW's own
    // DestroyWindow); false means "let DefWindowProcW run".
    [[nodiscard]] bool handle_message(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam,
                                      LRESULT &out_result) noexcept;

    // WIN-SIZE-AT-OPEN measurement seam (achado do time-lead, fatia de
    // 06/09/2026): this file's own "WHY THE FIRST WM_SIZE NEEDS A
    // SEPARATE ROUTING PATH" paragraph above claims the first WM_SIZE
    // arrives DURING CreateWindowExW, citing Microsoft's own docs for
    // WM_WINDOWPOSCHANGED - but the SAME docs only promise WM_GETMIN
    // MAXINFO and WM_NCCREATE/WM_NCCALCSIZE before CreateWindowExW
    // returns, and say the rest depend on the window's own class/style.
    // That claim was never actually measured against a real Windows
    // runner. handle_message()'s own WM_SIZE case (window_message_
    // route.cpp) increments this counter whenever it fires with
    // `m_window` still null - the exact window this open() has not yet
    // finished creating - so it counts ONLY messages that genuinely
    // arrived before CreateWindowExW returned, never one that arrived
    // afterward. win32_window_close_request_test.cpp prints this value,
    // measured, never asserted (this project's own "measured, not
    // asserted" convention, seat_test.cpp's own header comment) - once
    // a real run reports it, the claim above becomes a measured fact
    // (kept as written) or a wrong one (removed), never left as an
    // unverified assumption either way.
    [[nodiscard]] std::uint32_t size_messages_before_open_returns() const noexcept {
        return m_size_messages_before_open_returns;
    }

  private:
    HWND m_window = nullptr;

    // This adapter's OWN tracked copy of the window's current DPI -
    // window_state.hpp's own apply_dpi() setter also receives every
    // update (so pixel_size()'s derived formula stays correct), but
    // the WM_SIZE handler needs the RAW value back to convert a pixel
    // client rect to the 96-DPI logical baseline (window_state.hpp
    // exposes no such raw getter, deliberately - see this header's own
    // "TWO SIZES" paragraph). Seeded from GetDpiForSystem() in open(),
    // the same call this adapter's own AdjustWindowRectExForDpi() sizing
    // math already uses, so the FIRST WM_SIZE converts against the
    // exact DPI the window was actually created for.
    std::uint32_t m_dpi = 96;

    // size_messages_before_open_returns()'s own backing field - see
    // that accessor's comment above for what it counts and why.
    std::uint32_t m_size_messages_before_open_returns = 0;

    window_state m_state;
};

} // namespace glintfx::platform

#endif // defined(_WIN32)
