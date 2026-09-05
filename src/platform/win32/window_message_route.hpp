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

// platform/win32/window_message_route.hpp - X-2 (docs/plano-w6a-
// janela.md fatia 9, TODO.md WIN-WINDOW): the ONE declaration display_
// adapter.cpp's own shared, class-level window_proc (the SAME function
// every window of the class win32_display_adapter registers uses -
// display_adapter.hpp's own "F2/D-W6a-16" paragraph; that file's own
// window_proc comment: "Fatia 9's own window_adapter... is the first
// caller that reads GWLP_USERDATA back out with GetWindowLongPtrW and
// actually routes a message against it") needs of window_adapter.cpp,
// WITHOUT display_adapter.cpp ever including window_adapter.hpp itself
// (GODS_LAWS.md L-19: WIN-DISPLAY, the lower fatia, does not gain a
// compile-time dependency on WIN-WINDOW, the higher one - a type-
// erased `void *self` free function is the entire coupling surface).
//
// WHY THIS EXISTS AT ALL (sec. 5 risk 2 of the plan: "o primeiro
// WM_SIZE chega DURANTE CreateWindowExW"): DefWindowProcW synthesizes
// WM_SIZE/WM_MOVE while processing WM_WINDOWPOSCHANGED (learn.
// microsoft.com/windows/win32/winmsg/wm-size, Remarks: "The
// DefWindowProc function sends the WM_SIZE and WM_MOVE messages when
// it processes the WM_WINDOWPOSCHANGED message"), and CreateWindowExW
// itself drives that sequence before ever returning to the caller
// (learn.microsoft.com/windows/win32/winmsg/about-windows#window-
// creation). A window's GWLP_WNDPROC subclass (the mechanism win32_
// seat_adapter uses, seat_adapter.hpp's own "MECHANISM" paragraph) can
// only be installed AFTER CreateWindowExW returns - too late to see
// that first WM_SIZE. The class-level window_proc (display_adapter.cpp)
// is the ONLY code that runs early enough, so IT is what this fatia
// extends, through this one free function, rather than teaching win32_
// window_adapter to subclass its own instance the way the seat does.
//
// SAFE ONLY BECAUSE OF WHAT THE FOUR MESSAGE TYPES THEMSELVES MEAN
// (learn.microsoft.com/windows/win32/winmsg/window-features#window-
// types, "Message-Only Windows": "It is not visible, has no z-order,
// cannot be enumerated, and does not receive broadcast messages"):
// WM_SIZE, WM_CLOSE, WM_ACTIVATE and WM_DPICHANGED are never delivered
// to a message-only window (HWND_MESSAGE parent) - both win32_display_
// adapter's own window and win32_seat_adapter's own window
// (src/platform/win32/seat_adapter.hpp) are message-only. win32_
// window_adapter::open() (this fatia) is the ONLY caller anywhere in
// this project that ever creates a REAL (non-message-only) window
// under this shared class, so whatever GWLP_USERDATA holds for an hwnd
// that DOES receive one of these four messages is, by construction,
// the win32_window_adapter instance that created it - never win32_
// display_adapter's or win32_seat_adapter's own `this`. This is the
// entire safety argument for the two reinterpret_cast/static_cast
// pairs this coupling relies on (display_adapter.cpp's own call site,
// window_adapter.cpp's own definition of the function below) - it is
// NOT a general-purpose type-erased callback, and must never be routed
// for any other message type without re-checking this same argument.
namespace glintfx::platform {

// Defined in window_adapter.cpp. `self` is the raw GWLP_USERDATA value
// display_adapter.cpp's own window_proc already reads back for WM_SIZE/
// WM_CLOSE/WM_ACTIVATE/WM_DPICHANGED - always a win32_window_adapter*
// per this header's own safety argument above, cast back to that
// concrete type on the OTHER side of this call, where the full class
// definition is visible. Returns true and writes `out_result` when
// win32_window_adapter::handle_message() itself handled `msg` (window_
// proc then returns `out_result` WITHOUT ever calling DefWindowProcW
// for it); false means "let DefWindowProcW run", the same fall-through
// shape window_proc already uses for every message it does not
// recognize.
[[nodiscard]] bool win32_window_adapter_route_message(void *self, HWND hwnd, UINT msg,
                                                      WPARAM wparam, LPARAM lparam,
                                                      LRESULT &out_result) noexcept;

} // namespace glintfx::platform

#endif // defined(_WIN32)
