// SPDX-License-Identifier: AGPL-3.0-or-later
#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/win32/window_adapter.hpp"
#include "platform/window/window_state.hpp"

// win32_message_translation_test.cpp - X-2 (docs/plano-w6a-janela.md
// fatia 9): the pure-translation half of win32_window_adapter,
// exercised by calling handle_message() DIRECTLY with SYNTHETIC
// wParam/lParam - no window, no CreateWindowExW, no live message
// queue - the exact "mensagens sinteticas" role this fatia's own plan
// names, and the Win32 counterpart of window_adapter_listener_test.cpp
// ("callbacks com proxy nulo") one directory over on the Wayland side.
//
// hwnd == nullptr throughout (handle_message()'s own header comment
// in window_adapter.hpp: guarded so this file never needs a real
// window) - every case below proves the STATE UPDATE, never an OS side
// effect (SetWindowPos on WM_DPICHANGED is skipped entirely when hwnd
// is null, by construction, not by luck).
//
// Sec. 5 risk 2 of the plan ("passa verde e esta quebrado" if a test
// only checks 'nao zero'): every WM_SIZE case here asserts the EXACT
// number handle_message() derives from lParam - a mutation that copied
// a hardcoded/requested size instead of reading lParam would fail the
// distinct 640x480/800x600/1024x768 values below, not just "non-zero".

using glintfx::platform::win32_window_adapter;
using glintfx::platform::window_state_bit;

GLINTFX_TEST(freshly_constructed_adapter_reports_closed) {
    win32_window_adapter adapter;
    GLINTFX_CHECK(!adapter.is_open());
    GLINTFX_CHECK(adapter.native_handle() == nullptr);
}

GLINTFX_TEST(wm_size_restored_updates_logical_size_from_the_lparam_not_a_copied_request) {
    win32_window_adapter adapter;
    LRESULT result = 1; // poisoned, so a no-op handler cannot masquerade as "handled, 0".

    const bool handled =
        adapter.handle_message(nullptr, WM_SIZE, SIZE_RESTORED, MAKELPARAM(640, 480), result);

    GLINTFX_CHECK(handled);
    GLINTFX_CHECK(result == 0);
    GLINTFX_CHECK(adapter.state().logical_size().width == 640);
    GLINTFX_CHECK(adapter.state().logical_size().height == 480);
    GLINTFX_CHECK(!adapter.state().state(window_state_bit::maximized));
}

GLINTFX_TEST(wm_size_maximized_sets_the_maximized_bit) {
    win32_window_adapter adapter;
    LRESULT result = 1; // poisoned, so a no-op handler cannot masquerade as "handled, 0".

    const bool handled =
        adapter.handle_message(nullptr, WM_SIZE, SIZE_MAXIMIZED, MAKELPARAM(1024, 768), result);

    GLINTFX_CHECK(handled);
    GLINTFX_CHECK(result == 0);
    GLINTFX_CHECK(adapter.state().state(window_state_bit::maximized));
    GLINTFX_CHECK(adapter.state().logical_size().width == 1024);
    GLINTFX_CHECK(adapter.state().logical_size().height == 768);
}

GLINTFX_TEST(wm_size_restored_after_maximized_clears_the_maximized_bit) {
    win32_window_adapter adapter;
    LRESULT result = 0;
    const bool handled_while_maximizing =
        adapter.handle_message(nullptr, WM_SIZE, SIZE_MAXIMIZED, MAKELPARAM(1024, 768), result);
    GLINTFX_CHECK(handled_while_maximizing);

    const bool handled_while_restoring =
        adapter.handle_message(nullptr, WM_SIZE, SIZE_RESTORED, MAKELPARAM(800, 600), result);

    GLINTFX_CHECK(handled_while_restoring);
    GLINTFX_CHECK(!adapter.state().state(window_state_bit::maximized));
    GLINTFX_CHECK(adapter.state().logical_size().width == 800);
}

// "close pegajoso" (docs/plano-w6a-janela.md fatia 9's own row), the
// PURE half: the one-way latch fires and handle_message() reports the
// message as HANDLED (true) - window_message_route.hpp's own contract
// is that a `true` return means window_proc returns immediately
// WITHOUT ever reaching DefWindowProcW, which is precisely what keeps
// DefWindowProcW's own default WM_CLOSE handling (DestroyWindow) from
// ever running. The REAL half - a live window still answering
// IsWindow() true after this same message arrives for real - is
// win32_window_close_request_test.cpp's own job (sec. 5 risk 6).
GLINTFX_TEST(wm_close_sets_the_one_way_latch_and_reports_handled) {
    win32_window_adapter adapter;
    GLINTFX_CHECK(!adapter.state().close_requested());
    LRESULT result = 1;

    const bool handled = adapter.handle_message(nullptr, WM_CLOSE, 0, 0, result);

    GLINTFX_CHECK(handled);
    GLINTFX_CHECK(result == 0);
    GLINTFX_CHECK(adapter.state().close_requested());
}

GLINTFX_TEST(wm_activate_wa_inactive_clears_the_active_bit) {
    win32_window_adapter adapter;
    LRESULT result = 0;
    const bool handled_while_activating =
        adapter.handle_message(nullptr, WM_ACTIVATE, MAKEWPARAM(WA_ACTIVE, 0), 0, result);
    GLINTFX_CHECK(handled_while_activating);
    GLINTFX_CHECK(adapter.state().state(window_state_bit::active));

    const bool handled_while_deactivating =
        adapter.handle_message(nullptr, WM_ACTIVATE, MAKEWPARAM(WA_INACTIVE, 0), 0, result);

    GLINTFX_CHECK(handled_while_deactivating);
    GLINTFX_CHECK(!adapter.state().state(window_state_bit::active));
}

GLINTFX_TEST(wm_activate_wa_clickactive_sets_the_active_bit) {
    win32_window_adapter adapter;
    LRESULT result = 0;

    const bool handled =
        adapter.handle_message(nullptr, WM_ACTIVATE, MAKEWPARAM(WA_CLICKACTIVE, 0), 0, result);

    GLINTFX_CHECK(handled);
    GLINTFX_CHECK(adapter.state().state(window_state_bit::active));
}

// DPI 144 (docs/plano-w6a-janela.md fatia 9's own row): hwnd == nullptr
// proves the SetWindowPos side effect is skipped (window_adapter.cpp's
// own WM_DPICHANGED case, guarded on `hwnd`) while the pure m_state/
// m_dpi update still happens - a subsequent WM_SIZE, still synthetic,
// converts against the NEW dpi, proving handle_message() feeds the
// updated factor forward rather than caching the DPI it was
// constructed with (window_state.hpp's own "ONE FORMULA, TWO FACTORS"
// paragraph: pixel_size() == logical_size() * dpi / 96).
GLINTFX_TEST(wm_dpichanged_144_is_safe_with_a_null_hwnd_and_feeds_later_wm_size) {
    win32_window_adapter adapter;
    LRESULT result = 1;
    RECT suggested{0, 0, 900, 675};

    const bool handled = adapter.handle_message(nullptr, WM_DPICHANGED, MAKEWPARAM(144, 144),
                                                reinterpret_cast<LPARAM>(&suggested), result);
    GLINTFX_CHECK(handled);
    GLINTFX_CHECK(result == 0);

    // A pixel client rect of 900x675 at 144 DPI is 600x450 at the
    // 96-DPI logical baseline (900*96/144 = 600, 675*96/144 = 450) -
    // the exact conversion window_adapter.cpp's own WM_SIZE case runs
    // against whatever m_dpi WM_DPICHANGED most recently set.
    const bool handled_wm_size_after_dpi_change =
        adapter.handle_message(nullptr, WM_SIZE, SIZE_RESTORED, MAKELPARAM(900, 675), result);

    GLINTFX_CHECK(handled_wm_size_after_dpi_change);
    GLINTFX_CHECK(adapter.state().logical_size().width == 600);
    GLINTFX_CHECK(adapter.state().logical_size().height == 450);
    // pixel_size() re-derives from logical_size * dpi / 96 (window_
    // state.hpp's own formula, already proven with synthetic values by
    // window_state_test.cpp) - checked here only to prove THIS adapter
    // actually called apply_dpi(144), not merely tracked m_dpi
    // privately without ever telling window_state.
    GLINTFX_CHECK(adapter.state().pixel_size().width == 900);
    GLINTFX_CHECK(adapter.state().pixel_size().height == 675);
}

GLINTFX_TEST(an_unrecognized_message_is_reported_unhandled) {
    win32_window_adapter adapter;
    LRESULT result = 0;

    const bool handled = adapter.handle_message(nullptr, WM_PAINT, 0, 0, result);

    GLINTFX_CHECK(!handled);
}

#endif // defined(_WIN32)
