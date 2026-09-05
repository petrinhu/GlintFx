// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/win32/display_adapter.hpp"

#if defined(_WIN32)

#include <cstddef>
#include <cstdint>
#include <cwchar>
#include <utility>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

// display_adapter.cpp - see display_adapter.hpp's own header comment
// for scope, the F2 defect this fatia (X-1', docs/plano-w6a-janela.md
// fatia 7) fixes, and the mechanism sources. Every Win32 call below is
// written from Microsoft's own current documentation, reusing the
// exact pattern tests/win32_runner_probe_test.cpp already proved live
// on the windows-latest GitHub Actions runner where this fatia's
// additions are new (the GWLP_USERDATA routing, the PeekMessageW
// pump) - not from having watched THIS file run, since this project
// has no usable Windows toolchain on the machine that wrote it
// (declared, GODS_LAWS.md L-27/L-44, same as that probe file's own
// header comment).

namespace glintfx::platform {

namespace {

// class_name_for()'s own arithmetic, checked once here instead of
// trusted by inspection (GODS_LAWS.md L-17): "GlintfxWnd" is 10
// characters, a zero-padded 16-hex-digit std::uintptr_t (enough for
// every address on a 64-bit build, %016zx never widens past that) is
// 16 more, plus the terminating NUL is 27 - safely under
// win32_display_adapter::k_class_name_chars (32, display_adapter.hpp).
static_assert(win32_display_adapter::k_class_name_chars >= 10 + 16 + 1,
              "win32_display_adapter::class_name_for's buffer must fit \"GlintfxWnd\" plus "
              "16 hex digits plus the terminating NUL");

// window_proc: WIN-WINDOW fatia 7's own routing mechanism (D-W6a-16,
// display_adapter.hpp's header comment) - installs GWLP_USERDATA
// during WM_NCCREATE, the FIRST message any window receives
// (learn.microsoft.com/windows/win32/learnwin32/managing-application-state-:
// "When you call CreateWindowEx, pass a pointer to this structure in
// the final void* parameter. When you receive the WM_NCCREATE and
// WM_CREATE messages, the lParam parameter of each message is a
// pointer to a CREATESTRUCT structure... The lpCreateParams member...
// is the original void pointer that you specified in CreateWindowEx"),
// so no LATER message - WM_SIZE among them, plano-w6a-janela.md sec.
// 5 item 2's own named landmine - can ever be routed against a null
// or stale pointer for a window created through this mechanism.
//
// This display adapter's own window is message-only (HWND_MESSAGE,
// see open() below) and has no per-instance behavior of its own to
// dispatch into past that point - it falls through to
// DefWindowProcW exactly like the ORIGINAL version of this function
// did for every message. Fatia 9's own window_adapter (X-2, not yet
// implemented), sharing this SAME class via window_class_name()
// (display_adapter.hpp), is the first caller that reads GWLP_USERDATA
// back out with GetWindowLongPtrW and actually routes a message
// against it - this function's job here is only to prove, for
// whatever test reads native_handle() back (display_adapter.hpp), that
// the installation itself happens at the right time and points at the
// right instance.
LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) noexcept {
    if (msg == WM_NCCREATE) {
        const auto *create = reinterpret_cast<const CREATESTRUCTW *>(lparam);
        ::SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                            reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    }
    return ::DefWindowProcW(hwnd, msg, wparam, lparam);
}

} // namespace

std::array<wchar_t, win32_display_adapter::k_class_name_chars>
win32_display_adapter::class_name_for(const void *instance) noexcept {
    std::array<wchar_t, k_class_name_chars> buffer{};
    // D-W6a-16, option (b): distinct per ADDRESS, never a meaningful
    // decode of it by anyone reading the name later - the only
    // requirement is that two different instances produce two
    // different names (this fatia's own "two_displays_test" proves
    // exactly that). swprintf (<cwchar>, standard C++, not the
    // MSVC-only _snwprintf_s) with a bounded count is enough for that;
    // <format> would pull std::string machinery this internal seam
    // has no need for.
    const auto address = reinterpret_cast<std::uintptr_t>(instance);
    ::swprintf(buffer.data(), buffer.size(), L"GlintfxWnd%016zx",
               static_cast<std::size_t>(address));
    return buffer;
}

win32_display_adapter::win32_display_adapter(win32_display_adapter &&other) noexcept
    : m_window(other.m_window), m_class_atom(other.m_class_atom), m_class_name(other.m_class_name) {
    other.m_window = nullptr;
    other.m_class_atom = 0;
}

win32_display_adapter &win32_display_adapter::operator=(win32_display_adapter &&other) noexcept {
    if (this != &other) {
        close();
        m_window = other.m_window;
        m_class_atom = other.m_class_atom;
        m_class_name = other.m_class_name;
        other.m_window = nullptr;
        other.m_class_atom = 0;
    }
    return *this;
}

win32_display_adapter::~win32_display_adapter() { close(); }

gltfx_rslt<void> win32_display_adapter::open() noexcept {
    const std::array<wchar_t, k_class_name_chars> class_name = class_name_for(this);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    // CS_OWNDC (docs/plano-w6a-janela.md fatia 7, D-W6a-16's own row):
    // gives every window created under this class its own PERSISTENT
    // device context for the life of the window - the documented
    // Win32 way W6b's own WGL context needs (learn.microsoft.com/
    // windows/win32/winmsg/window-class-styles). Harmless for THIS
    // adapter's own message-only window, which never calls GetDC at
    // all; set now so fatia 9's window_adapter (X-2), reusing this
    // SAME registered class through window_class_name()
    // (display_adapter.hpp), never has to re-register a second class
    // just to add this bit later.
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = &window_proc;
    wc.hInstance = ::GetModuleHandleW(nullptr);
    wc.lpszClassName = class_name.data();

    ::SetLastError(0);
    const ATOM atom = ::RegisterClassExW(&wc);
    if (atom == 0) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::platform_failure).with_os_error_code(::GetLastError()));
    }

    // HWND_MESSAGE: the documented Win32 pattern for a window that
    // needs a valid HWND but is never painted, shown, or reachable via
    // Alt+Tab or the taskbar - see this adapter's own header comment
    // ("MECHANISM"). WS_OVERLAPPEDWINDOW's usual position/size
    // arguments are meaningless for a message-only window; CW_USEDEFAULT
    // and a nominal size are passed only because CreateWindowExW
    // requires SOME values there.
    //
    // lpParam = this (was nullptr before this fatia): WM_NCCREATE's
    // own CREATESTRUCTW::lpCreateParams carries it straight to
    // window_proc above, BEFORE CreateWindowExW itself returns - the
    // "Managing Application State" sequence this file's window_proc
    // comment cites. This window has nothing of its own to route into
    // yet, but wiring the pointer through here now means fatia 9's
    // window_adapter, sharing this same class/wndproc, inherits a
    // routing mechanism already proven correct instead of writing a
    // second one from scratch.
    ::SetLastError(0);
    HWND window = ::CreateWindowExW(0, class_name.data(), L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                                    ::GetModuleHandleW(nullptr), this);
    if (window == nullptr) {
        const DWORD create_error = ::GetLastError();
        ::UnregisterClassW(class_name.data(), ::GetModuleHandleW(nullptr));
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::platform_failure).with_os_error_code(create_error));
    }

    m_window = window;
    m_class_atom = atom;
    m_class_name = class_name;
    return gltfx_rslt<void>::ok();
}

void win32_display_adapter::close() noexcept {
    if (m_window != nullptr) {
        // Reverse order of creation (same "teardown in reverse" shape
        // wayland_display_adapter::close() documents): the window is
        // destroyed before the class it was created from is
        // unregistered - UnregisterClassW refuses (ERROR_CLASS_HAS_WINDOWS)
        // while any window of that class is still alive. m_class_name
        // (not a fixed constant anymore, since F2's fix) is still
        // valid here: it is only ever overwritten by a successful
        // open(), never cleared early.
        ::DestroyWindow(m_window);
        m_window = nullptr;
        if (m_class_atom != 0) {
            ::UnregisterClassW(m_class_name.data(), ::GetModuleHandleW(nullptr));
            m_class_atom = 0;
        }
    }
}

gltfx_rslt<void> win32_display_adapter::pump_events() noexcept {
    // hWnd = nullptr: pumps EVERY message queued for the CALLING
    // THREAD, not only this adapter's own window - PeekMessageW's own
    // documented Remarks ("If hWnd is NULL, PeekMessage retrieves
    // messages for any window that belongs to the calling thread, and
    // any messages on the calling thread's message queue whose hwnd
    // value is NULL"). See this method's own header comment
    // (display_adapter.hpp) for why this is the right scope for a
    // display CONNECTION's pump, not a paridade gap against the
    // Wayland side's own per-fd pump_events().
    //
    // PM_REMOVE, looped until PeekMessageW returns 0: drains the
    // queue completely every call, the same "never blocks, never
    // leaves anything behind for the next frame" contract
    // tests/win32_runner_probe_test.cpp's own PeekMessageW loop already
    // documents and this project's TESTES.md expects of a non-
    // blocking pump.
    MSG msg{};
    while (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE) != 0) {
        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
    }
    return gltfx_rslt<void>::ok();
}

} // namespace glintfx::platform

#endif // defined(_WIN32)
