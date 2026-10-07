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

// platform/win32/first_present_map.hpp - WIN-MAP-FIRST-PRESENT (D-W8-38,
// D-W5-8, GODS_LAWS.md L-04/L-17/L-19): the one atom that makes the FIRST
// presented frame show the window on Windows, as the first committed
// buffer maps the surface on Wayland ("mapear e de quem anexa buffer").
// open() never shows the window (D-W5-13); the context adapter owns one
// of these and asks it, once, right before the first SwapBuffers.
//
// "Never shown yet" is not "hidden": the caller must NOT answer
// skipped_hidden for a window that was never shown, or the loop would
// never draw the frame that maps it. Only IsIconic() means hidden.
//
// Parity with Wayland, where mapping is unconditional: the window
// appears even when the launcher passed SW_HIDE in STARTUPINFO (the pain
// that made libuv split HIDE_GUI). The first ShowWindow() call of a
// process takes its nCmdShow from STARTUPINFO instead of the argument
// (learn.microsoft.com/windows/win32/api/winuser/nf-winuser-showwindow,
// Remarks), so a hidden-launch can swallow it; SetWindowPos() with
// SWP_SHOWWINDOW has no such rule and is the documented second step.
// Activation is requested (SW_SHOW, no SWP_NOACTIVATE) and arbitrated by
// the system, never promised or measured.
//
// Pinning to the HWND is the caller's: this class owns nothing, only the
// "already done" fact.
namespace glintfx::platform {

class first_present_map {
  public:
    // True when the window is visible afterwards (already mapped, or
    // mapped by this call); false when it still is not, in which case
    // the next call tries again. NEVER shows a window a second time once
    // it succeeded: a window hidden later by someone else stays hidden.
    [[nodiscard]] bool map_once(HWND window) noexcept;

    [[nodiscard]] bool done() const noexcept { return m_done; }

  private:
    bool m_done = false;
};

} // namespace glintfx::platform

#endif // defined(_WIN32)
