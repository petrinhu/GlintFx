// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/win32/display_adapter.hpp"

#if defined(_WIN32)

#include <cstddef>
#include <cstdint>
#include <cwchar>
#include <utility>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

#include "platform/win32/window_message_route.hpp"

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
// dispatch into past that point - for THIS window, every message still
// falls through to DefWindowProcW exactly like the ORIGINAL version of
// this function did for every message. win32_window_adapter (X-2,
// src/platform/win32/window_adapter.hpp, fatia 9) is the caller that
// reads GWLP_USERDATA back out and actually routes a message against
// it - see the WM_SIZE/WM_CLOSE/WM_ACTIVATE/WM_DPICHANGED branch below
// for that mechanism.
LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) noexcept {
    if (msg == WM_NCCREATE) {
        // WM_NCCREATE's own documented lParam IS a CREATESTRUCTW*
        // smuggled through the integer-typed LPARAM (learn.microsoft.com/
        // windows/win32/learnwin32/managing-application-state) -
        // unavoidable at this Win32 message boundary, same idiom as the
        // GWLP_USERDATA cast a few lines below.
        // NOLINTNEXTLINE(performance-no-int-to-ptr) reason: see comment above
        const auto *create = reinterpret_cast<const CREATESTRUCTW *>(lparam);
        ::SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                            reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        return ::DefWindowProcW(hwnd, msg, wparam, lparam);
    }

    // X-2 (docs/plano-w6a-janela.md fatia 9, TODO.md WIN-WINDOW,
    // window_message_route.hpp's own header comment - read that
    // header's comment IN FULL before touching this branch): WM_SIZE,
    // WM_CLOSE, WM_ACTIVATE and WM_DPICHANGED are never delivered to a
    // message-only window (HWND_MESSAGE parent - this adapter's own
    // window above, and win32_seat_adapter's own, both are). win32_
    // window_adapter::open() (fatia 9) is the ONLY caller anywhere in
    // this project that ever creates a REAL (non-message-only) window
    // under this shared class, so whatever GWLP_USERDATA holds for an
    // hwnd that reaches one of these four message types is, by
    // construction, that adapter's own `this` - routed through a
    // type-erased free function so THIS file never gains a compile-time
    // dependency on window_adapter.hpp (GODS_LAWS.md L-19: WIN-DISPLAY,
    // the lower fatia, does not depend on WIN-WINDOW, the higher one).
    // This is also WHY the first WM_SIZE a window_adapter's own window
    // ever receives is never missed (sec. 5 risk 2 of the plan):
    // DefWindowProcW synthesizes it while processing
    // WM_WINDOWPOSCHANGED, which CreateWindowExW itself drives BEFORE
    // returning to the caller - long before a GWLP_WNDPROC instance
    // subclass (win32_seat_adapter's own mechanism) could ever be
    // installed. This class-level function is the only code that runs
    // early enough to route that message at all.
    if (msg == WM_SIZE || msg == WM_CLOSE || msg == WM_ACTIVATE || msg == WM_DPICHANGED) {
        // GetWindowLongPtrW returns the GWLP_USERDATA slot as a LONG_PTR
        // by Win32's own design (learn.microsoft.com/windows/win32/api/
        // winuser/nf-winuser-getwindowlongptrw) - it holds a genuine
        // pointer (window_proc's own WM_NCCREATE branch above is the
        // only writer, storing `this`/lpCreateParams), the integer-typed
        // return is just how the Win32 API carries it, unavoidable at
        // this boundary.
        // NOLINTNEXTLINE(performance-no-int-to-ptr) reason: see comment above
        void *user_data = reinterpret_cast<void *>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (user_data != nullptr) {
            LRESULT routed_result = 0;
            if (win32_window_adapter_route_message(user_data, hwnd, msg, wparam, lparam,
                                                   routed_result)) {
                return routed_result;
            }
        }
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
    // Return value checked (cert-err33-c), though unreachable in
    // practice by construction: the static_assert above already proves
    // k_class_name_chars fits "GlintfxWnd" plus 16 hex digits plus the
    // terminating NUL with 5 characters to spare, so this exact format
    // string can never overrun the buffer and swprintf can only return
    // negative on an encoding failure - never truncation. On that
    // unreachable path `buffer` is left at its all-zero std::array
    // initializer above (never garbage/UB), so open() still gets a
    // well-formed, merely-empty class name, and RegisterClassExW's
    // ordinary refusal of it carries through the normal gltfx_rslt
    // error path (GODS_LAWS.md L-22) - no crash, no special case needed.
    const int written = ::swprintf(buffer.data(), buffer.size(), L"GlintfxWnd%016zx",
                                   static_cast<std::size_t>(address));
    (void)written;
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
