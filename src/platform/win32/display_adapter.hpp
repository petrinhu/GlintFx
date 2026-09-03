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

#include <glintfx/core/err.hpp>

// display_adapter.hpp - WIN-DISPLAY fatia 1 (TODO.md, GODS_LAWS.md
// L-04 reabertura de ARCH-PORTS/WL-PROTO por paridade, see
// fechar-w1.md sec. 2 for the full order): the Windows counterpart of
// src/platform/wayland/display_adapter.hpp, satisfying the EXACT SAME
// platform::display_connection_port concept
// (src/platform/port/display_connection_port.hpp) with the same three
// members and nothing else - the port is DELIBERATELY NARROW (that
// header's own comment) precisely so a Win32 adapter, which has no
// file descriptor to wait on, can satisfy it without any change to
// the port itself.
//
// SCOPE, matching the Wayland side's own fatia A (ARCH-PORTS): only
// open/close/is_open. No registry, no roundtrip, no event pump - those
// have no par requested yet (fechar-w1.md sec. 2.3), and no VISIBLE
// window either (WIN-WINDOW, still blocked on ARCH-PORTS/WL-WINDOW,
// TODO.md).
//
// WHY windows.h HERE, unlike wl_display/wl_registry being FORWARD-
// DECLARED in the Wayland header: HWND and ATOM are both typedefs
// (`typedef struct HWND__ *HWND;`, `typedef WORD ATOM;`), not tag
// types - there is no standard way to forward-declare a typedef
// without re-declaring it exactly, which would be duplicating a
// system header's own contract rather than avoiding it. This entire
// file is already wrapped in `#if defined(_WIN32)`, so nothing that
// is not building for Windows ever sees this include - the same
// reasoning tests/win32_runner_probe_test.cpp already documents for
// its own identical guard block.
//
// MECHANISM: this project has no usable Windows toolchain on this
// machine (clang-cl exists here without a standard library or the
// Windows SDK, measured before writing this file) - so nothing below
// has been compiled or run locally. The mechanism reuses, verbatim,
// what tests/win32_runner_probe_test.cpp already proved live on the
// windows-latest GitHub Actions runner (run 33643748402): RegisterClassExW
// of a stably-named window class, then CreateWindowExW with the parent
// set to HWND_MESSAGE - the documented Win32 pattern for "a valid HWND
// with no UI, ever" (a message-only window never receives WM_PAINT,
// is never shown, and never appears in Alt+Tab or the taskbar). This
// is not a new mechanism invented for this file; it is the exact same
// two calls that probe already exercises, aimed at a message-only
// parent instead of the probe's own visible WS_OVERLAPPEDWINDOW.
namespace glintfx::platform {

class win32_display_adapter {
  public:
    // Starts CLOSED (m_window null) - default_initializable<A>, the
    // same contract wayland_display_adapter's own default constructor
    // satisfies (display_adapter.hpp, Wayland side).
    win32_display_adapter() noexcept = default;

    // Move-only (display_connection_port requires std::movable<A>, not
    // copyable): copying a live HWND/ATOM pair would hand two owners
    // the same window and class registration, and destroying/
    // unregistering twice is a double-free of OS-owned resources -
    // same reasoning as wayland_display_adapter's own deleted copy
    // members.
    win32_display_adapter(const win32_display_adapter &) = delete;
    win32_display_adapter &operator=(const win32_display_adapter &) = delete;

    win32_display_adapter(win32_display_adapter &&other) noexcept;
    win32_display_adapter &operator=(win32_display_adapter &&other) noexcept;

    // Closes whatever this adapter still owns - safe to run on a
    // moved-from or never-opened instance, because close() itself only
    // acts when m_window is non-null (same idempotent shape as
    // wayland_display_adapter::close()).
    ~win32_display_adapter();

    // RegisterClassExW's own class, then CreateWindowExW(..., parent =
    // HWND_MESSAGE, ...) - see this header's own "MECHANISM" paragraph
    // above. GODS_LAWS.md L-22: any refusal along this path (the class
    // fails to register, or CreateWindowExW returns NULL) comes back
    // as gltfx_err_code::platform_failure through the ordinary
    // gltfx_rslt<void> channel, carrying GetLastError()'s own value via
    // with_os_error_code() - no exception, no abort. A failed open()
    // tears down whatever it had already created (the class, if it
    // registered but the window then failed) before returning -
    // is_open() reads false either way.
    [[nodiscard]] gltfx_rslt<void> open() noexcept;

    // Idempotent-safe: does nothing when already closed (is_open() is
    // false), so both the destructor above and display_connection<A>'s
    // own close_if_open() can call this unconditionally without
    // checking first themselves. Tears down in the REVERSE order
    // open() built things in: DestroyWindow before UnregisterClassW,
    // same "reverse order of creation" shape wayland_display_adapter::
    // close() documents.
    void close() noexcept;

    [[nodiscard]] bool is_open() const noexcept { return m_window != nullptr; }

  private:
    HWND m_window = nullptr;
    ATOM m_class_atom = 0;
};

} // namespace glintfx::platform

#endif // defined(_WIN32)
