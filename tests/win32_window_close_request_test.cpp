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
#include "platform/win32/display_adapter.hpp"
#include "platform/win32/window_adapter.hpp"

// win32_window_close_request_test.cpp - X-2 (docs/plano-w6a-janela.md
// fatia 9, sec. 5 risk 6): the REAL half of "close pegajoso" -
// win32_message_translation_test.cpp already proves handle_message()
// marks the one-way latch and reports the message as HANDLED against a
// synthetic call with no window at all; THIS fixture opens a real
// window on the real windows-latest runner (the same interactive
// window station tests/win32_runner_probe_test.cpp already measured
// live there) and sends a real WM_CLOSE to prove the OTHER half of the
// risk: that a WM_CLOSE forwarded to DefWindowProcW would have called
// DestroyWindow already, leaving this adapter's own HWND dangling
// while close_requested() stayed true - IsWindow() still answering
// TRUE afterward is what proves that never happened.
//
// win32_display_adapter/win32_window_adapter are compiled a SECOND
// time directly into this executable's own object set (tests/
// CMakeLists.txt), the same technique win32_display_connect_test.cpp
// and win32_seat_translation_test.cpp already use for their own
// adapters: neither has GLINTFX_API (GODS_LAWS.md L-19, "nada e
// exportado"), so a separate executable linking only against
// glintfx::glintfx could never resolve these methods across that
// hidden-visibility boundary.

GLINTFX_TEST(sending_a_real_wm_close_sets_the_latch_and_leaves_the_window_alive) {
    glintfx::platform::win32_display_adapter display;
    GLINTFX_CHECK(display.open().has_value());

    glintfx::platform::win32_window_adapter window;
    glintfx::platform::win32_window_desc desc{};
    desc.logical_width = 800;
    desc.logical_height = 600;
    desc.title = "glintfx win32_window_close_request_test";

    GLINTFX_CHECK(window.open(display, desc).has_value());
    GLINTFX_CHECK(window.is_open());
    GLINTFX_CHECK(!window.state().close_requested());

    // SendMessageW dispatches SYNCHRONOUSLY, on the calling thread,
    // straight into the window's own wndproc (display_adapter.cpp's
    // own shared, class-level window_proc, routed to win32_window_
    // adapter::handle_message() via window_message_route.hpp) - the
    // same "synthetic message, own thread, never a real user click"
    // technique tests/CMakeLists.txt's own seat_test block already
    // documents for its WM_INPUT_DEVICE_CHANGE case.
    const HWND handle = window.native_handle();
    ::SendMessageW(handle, WM_CLOSE, 0, 0);

    GLINTFX_CHECK(window.state().close_requested());
    // THE assertion this fixture exists for: if handle_message() had
    // ever fallen through to DefWindowProcW for WM_CLOSE, the window
    // would already be destroyed here.
    GLINTFX_CHECK(::IsWindow(handle) != 0);

    // Reverse order of creation (same convention every close()/
    // destructor pair in this project already documents): the window
    // is torn down before the class-owning display, because
    // UnregisterClassW (inside display.close()) refuses while any
    // window of that class is still alive.
    window.close();
    GLINTFX_CHECK(!window.is_open());
    display.close();
    GLINTFX_CHECK(!display.is_open());
}

#endif // defined(_WIN32)
