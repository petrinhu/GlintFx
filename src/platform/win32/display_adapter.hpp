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

#include <array>
#include <cstddef>

#include <glintfx/core/err.hpp>

// display_adapter.hpp - WIN-DISPLAY fatia 1 (TODO.md, GODS_LAWS.md
// L-04 reabertura de ARCH-PORTS/WL-PROTO por paridade), EXTENDED by
// X-1' (docs/plano-w6a-janela.md fatia 7, D-W6a-16, 05/09/2026): the
// Windows counterpart of src/platform/wayland/display_adapter.hpp,
// satisfying the EXACT SAME platform::display_connection_port concept
// (src/platform/port/display_connection_port.hpp) with the same three
// members plus pump_events() - the port is DELIBERATELY NARROW (that
// header's own comment) precisely so a Win32 adapter, which has no
// file descriptor to wait on, can satisfy it without any change to
// the port itself.
//
// SCOPE OF THIS FATIA (7, "X-1'"): still no VISIBLE window (WIN-
// WINDOW's own window_adapter is fatia 9, X-2, still to be written) -
// but the class this adapter registers, and the routing mechanism its
// window procedure now installs, are exactly the ones fatia 9 will
// reuse without a second registration or a second wndproc.
//
// F2/D-W6a-16 - THIS FATIA'S OWN DEFECT AND FIX (plano fatia 7 "onde
// esta o perigo" item 8, "dois displays no mesmo processo"): the
// window class open() registers used to be a SINGLE FIXED NAME shared
// by every instance in the process (k_window_class_name, this file's
// own history before this fatia). RegisterClassExW's registration is
// per-PROCESS (learn.microsoft.com/windows/win32/winmsg/about-window-
// classes): a second win32_display_adapter in the same process asking
// to register the SAME name failed with ERROR_CLASS_ALREADY_EXISTS
// (1410) on its own open() - a defect invisible until something
// actually opened two, because nothing did (measured against `main`
// at commit 270bef3 by the CTO planning this onda, not by a test
// here - fechar/plano-w6a-janela.md sec. 0, fact F2). The Wayland side
// has no such limit (a second wl_display_connect() just works), so
// this was a paridade defect by construction (GODS_LAWS.md L-04),
// latent rather than caught, exactly the shape that law exists to
// close.
//
// THE FIX (D-W6a-16, option (b) of three considered): class_name_for()
// below derives a name from the ADAPTER'S OWN ADDRESS, so two
// instances register two DIFFERENT classes and neither refuses the
// other. Option (a) (keep the fixed name) is the defect itself.
// Option (c) (register once per process, refcounted) was rejected
// because it needs process-wide mutable state living OUTSIDE any one
// adapter instance - exactly what tests/tools/check_port_privacy.sh's
// closed list of adapter classes (KNOWN_ADAPTER_CLASSES) and this
// project's "no hidden global" shape (GODS_LAWS.md L-19) exist to
// keep out of this layer; (b) costs one swprintf and nothing else.
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
// has been compiled or run locally (GODS_LAWS.md L-27, fact vs
// inference, declared here exactly as tests/win32_runner_probe_test.cpp's
// own header comment already declares for itself). The original
// open()/close()/is_open() mechanism reused, verbatim, what
// tests/win32_runner_probe_test.cpp already proved live on the
// windows-latest GitHub Actions runner: RegisterClassExW of a
// stably-named window class, then CreateWindowExW with the parent set
// to HWND_MESSAGE - the documented Win32 pattern for "a valid HWND
// with no UI, ever". This fatia's own additions - the per-instance
// class name, the GWLP_USERDATA routing in the window procedure, and
// the PeekMessageW pump - are written from Microsoft's own current
// documentation as well: "Managing Application State"
// (learn.microsoft.com/windows/win32/learnwin32/managing-application-state-,
// the WM_NCCREATE/CREATESTRUCT/SetWindowLongPtr(GWLP_USERDATA)
// sequence display_adapter.cpp's own window_proc follows exactly) and
// PeekMessageW's own reference page (the hWnd=NULL "pumps every
// message for the calling thread" behavior pump_events() relies on).
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

    // RegisterClassExW's own class (now under a name unique to THIS
    // instance - see this header's own "F2/D-W6a-16" paragraph above),
    // then CreateWindowExW(..., parent = HWND_MESSAGE, ...) - see this
    // header's own "MECHANISM" paragraph. GODS_LAWS.md L-22: any
    // refusal along this path (the class fails to register, or
    // CreateWindowExW returns NULL) comes back as
    // gltfx_err_code::platform_failure through the ordinary
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

    // WIN-WINDOW fatia 7 (X-1', docs/plano-w6a-janela.md), the Win32
    // sibling of wayland_display_adapter::pump_events()
    // (src/platform/wayland/display_adapter.hpp) - the non-blocking
    // event pump ARCH-PORTS's own consumer loop calls once per frame.
    //
    // hWnd = nullptr (display_adapter.cpp's own body): pumps EVERY
    // message queued for the CALLING THREAD, not only this adapter's
    // own message-only window - PeekMessageW's own documented
    // behavior for a null window handle. This is a DECLARED mechanism
    // difference from the Wayland side, not a paridade gap
    // (GODS_LAWS.md L-04: "mecanismo pode diferir; comportamento
    // observavel e cobertura, nao"): Win32's message queue is per-
    // THREAD, wl_display's event queue is per-CONNECTION, so "pump
    // everything this display connection owns in one call" - the
    // observable contract both sides promise - means "the whole
    // thread queue" on Win32 the moment a second window (fatia 9's
    // window_adapter, sharing this SAME class) exists on it, and
    // "this one wl_display" on Wayland. Never fails by any mechanism
    // this adapter has found documented for a valid message loop, so
    // it always returns ok() - the return type stays gltfx_rslt<void>
    // to match the port's own shape and leave room for a future
    // failure mode without an API break.
    [[nodiscard]] gltfx_rslt<void> pump_events() noexcept;

    // Test seam only (tests/win32_two_displays_test.cpp via
    // tests/two_displays_test.cpp, tests/win32_display_refusal_test.cpp):
    // the raw window handle this adapter owns, so a test can read back
    // GWLP_USERDATA with GetWindowLongPtrW(native_handle(), ...) and
    // confirm window_proc installed it correctly (plano-w6a-janela.md
    // sec. 5 item 2, the landmine this mechanism exists to close) -
    // never used by open()/close()/pump_events() themselves, which
    // already keep m_window as their own private RAII state.
    [[nodiscard]] HWND native_handle() const noexcept { return m_window; }

    // Fatia 9's own seam (X-2, window_adapter, not yet implemented):
    // the class name THIS adapter already registered, so a future
    // window it opens under the same display connection reuses this
    // SAME class (and therefore this same window_proc/GWLP_USERDATA
    // routing, and the CS_OWNDC style open()'s own WNDCLASSEXW already
    // sets - see display_adapter.cpp) instead of a second
    // RegisterClassExW under a second name. Empty/undefined before
    // open() succeeds - is_open() is the caller's own guard for that,
    // same as every other accessor on a possibly-unopened adapter in
    // this project.
    [[nodiscard]] const wchar_t *window_class_name() const noexcept { return m_class_name.data(); }

    // Wide enough for the "GlintfxWnd" prefix plus a zero-padded
    // 16-hex-digit std::uintptr_t (covers a 64-bit address) plus the
    // terminating NUL - display_adapter.cpp's own static_assert next
    // to class_name_for()'s definition checks this arithmetic once,
    // at compile time, rather than trusting it by inspection
    // (GODS_LAWS.md L-17).
    static constexpr std::size_t k_class_name_chars = 32;

    // D-W6a-16 (this header's own "F2" paragraph above): the class
    // name a call to open() FROM `instance` is about to ask
    // RegisterClassExW for - distinct per instance address, never the
    // single fixed name this fatia's own defect used. PUBLIC ONLY as a
    // seam: tests/win32_display_refusal_test.cpp pre-registers exactly
    // this name (computed from an adapter it has not opened yet) to
    // force ERROR_CLASS_ALREADY_EXISTS deterministically, instead of
    // guessing a name open() might use. Never called by anything
    // outside a test and open() itself.
    [[nodiscard]] static std::array<wchar_t, k_class_name_chars>
    class_name_for(const void *instance) noexcept;

  private:
    HWND m_window = nullptr;
    ATOM m_class_atom = 0;
    std::array<wchar_t, k_class_name_chars> m_class_name{};
};

} // namespace glintfx::platform

#endif // defined(_WIN32)
