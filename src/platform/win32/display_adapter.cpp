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
    // WHY THIS ROUTING PATH EXISTS AT ALL, MEASURED, NOT ASSUMED
    // (corrected 06/09/2026, achado do time-lead na fatia de
    // fechamento - the earlier version of this comment asserted, citing
    // Microsoft's own WM_SIZE/WM_WINDOWPOSCHANGED documentation, that
    // "the first WM_SIZE a window_adapter's own window ever receives is
    // never missed" because DefWindowProcW always synthesizes it while
    // processing WM_WINDOWPOSCHANGED, itself always driven by
    // CreateWindowExW BEFORE returning to the caller): that claim was
    // never actually measured against a real Windows runner, and once
    // measured, it did not hold for THIS project's own window.
    // win32_window_close_request_test's own size_messages_before_open_
    // returns() counter (window_adapter.hpp) - incremented from inside
    // THIS class-level window_proc, the one piece of code early enough
    // to see a message that arrives before win32_window_adapter::open()
    // ever stores `this` into m_window - reported `MEASURED win32_
    // window_close_request_test.wm_size_during_create=0` on the real
    // windows-latest runner (run 34020376320, commit 093a22e): ZERO
    // WM_SIZE messages arrived during CreateWindowExW for a window
    // created WS_OVERLAPPEDWINDOW without WS_VISIBLE (D-W5-13, this
    // project's own "born invisible" rule) - the documented mechanism
    // is real, but this project's own window apparently never triggers
    // it before returning. This routing path stays exactly as it is:
    // it costs nothing when no early message arrives, and remains the
    // ONLY code early enough to catch one on a future window shape that
    // does trigger it (WS_VISIBLE, a different style, a different
    // Windows version) - removing it on the strength of this one
    // measurement would trade a proven-cheap safety net for an
    // unmeasured bet the other way. `wm_size_during_create` stays as a
    // permanent sentinel in the measured-parity table (tests/parity_
    // exceptions.txt), reproving itself the moment a future run ever
    // reports a nonzero value here.
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

    // LOOP-RUN fatia 7 (D-W6b-50): a high-resolution waitable timer,
    // created ONCE here and armed once per wait_events() call below -
    // never recreated per call, the same "open() sets up, the per-
    // frame method only arms/waits" shape this project already uses
    // for every other per-frame resource. NOT fatal on failure
    // (display_adapter.hpp's own header comment on high_resolution_
    // wait()): a build on Windows before 1803, or one where this flag
    // is otherwise refused, still opens successfully and still pumps -
    // wait_events() falls back to the plain dwMilliseconds argument
    // with no timer object at all, a DECLARED, MEASURED degradation
    // (tests/win32_wait_events_test.cpp's own high_resolution_wait()
    // read), never a reason to fail open() itself over a capability
    // this adapter can do without.
    ::SetLastError(0);
    HANDLE wait_timer = ::CreateWaitableTimerExW(
        nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    m_wait_timer = wait_timer; // nullptr on refusal - see comment above
    m_high_resolution_wait = wait_timer != nullptr;

    return gltfx_rslt<void>::ok();
}

void win32_display_adapter::close() noexcept {
    if (m_window != nullptr) {
        // LOOP-RUN fatia 7: the timer is closed BEFORE the window -
        // display_adapter.hpp's own header comment on wait_events()
        // states this ordering explicitly. CloseHandle() on a timer
        // that is not currently armed (SetWaitableTimer never called,
        // or the last wait already consumed its one-shot signal) is
        // always safe - there is no "cancel first" step this class
        // needs before releasing the handle.
        if (m_wait_timer != nullptr) {
            ::CloseHandle(m_wait_timer);
            m_wait_timer = nullptr;
            m_high_resolution_wait = false;
        }
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

// drain_queued_messages() - LOOP-RUN fatia 7: the SAME PeekMessageW
// loop pump_events() below always ran, factored out so wait_events()
// (this same fatia) can run it too, after its own wait - never a
// second, separately-maintained copy of "drain everything queued".
// Returns whether at least one message was actually dispatched -
// wait_events() below folds this into its own "did anything arrive"
// answer alongside what the wait itself observed.
namespace {

bool drain_queued_messages() noexcept {
    // hWnd = nullptr: pumps EVERY message queued for the CALLING
    // THREAD, not only this adapter's own window - PeekMessageW's own
    // documented Remarks ("If hWnd is NULL, PeekMessage retrieves
    // messages for any window that belongs to the calling thread, and
    // any messages on the calling thread's message queue whose hwnd
    // value is NULL"). See win32_display_adapter::pump_events()'s own
    // header comment (display_adapter.hpp) for why this is the right
    // scope for a display CONNECTION's pump, not a paridade gap
    // against the Wayland side's own per-fd pump_events().
    //
    // PM_REMOVE, looped until PeekMessageW returns 0: drains the
    // queue completely every call, the same "never blocks, never
    // leaves anything behind for the next frame" contract
    // tests/win32_runner_probe_test.cpp's own PeekMessageW loop already
    // documents and this project's TESTES.md expects of a non-
    // blocking pump.
    MSG msg{};
    bool dispatched_any = false;
    while (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE) != 0) {
        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
        dispatched_any = true;
    }
    return dispatched_any;
}

} // namespace

gltfx_rslt<void> win32_display_adapter::pump_events() noexcept {
    // "is there anything to read RIGHT NOW" - wait_events()'s own
    // budget_ms 0 (this method's own header comment, display_
    // adapter.hpp: "pump_events() above IS this call with the bool
    // discarded").
    if (const gltfx_rslt<bool> waited = wait_events(0); waited.has_error()) {
        return gltfx_rslt<void>::err(waited.error());
    }
    return gltfx_rslt<void>::ok();
}

gltfx_rslt<bool> win32_display_adapter::wait_events(std::uint32_t budget_ms) noexcept {
    bool woke_on_message = false;

    if (budget_ms > 0) {
        if (m_wait_timer != nullptr) {
            // Negative == relative time, in 100-nanosecond intervals
            // (SetWaitableTimer's own documented lpDueTime contract) -
            // budget_ms milliseconds converted up front, never re-read
            // mid-wait.
            LARGE_INTEGER due_time{};
            due_time.QuadPart = -(static_cast<LONGLONG>(budget_ms) * 10'000);
            ::SetWaitableTimer(m_wait_timer, &due_time, 0, nullptr, nullptr, FALSE);

            // MsgWaitForMultipleObjectsEx, not a plain WaitForSingle
            // Object on the timer alone: a thread that owns windows
            // MUST use one of the Msg* wait functions instead of the
            // plain WaitForMultipleObjects family (that function's own
            // documented Remarks, "Use caution when calling the wait
            // functions and code that... creates windows... a thread
            // that uses a wait function with no time-out interval may
            // cause the system to become deadlocked") - MWMO_
            // INPUTAVAILABLE (this method's own header comment)
            // guarantees existing, already-seen input still wakes this
            // call, never only brand-new input.
            const DWORD wait_result = ::MsgWaitForMultipleObjectsEx(
                1, &m_wait_timer, budget_ms, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
            // WAIT_OBJECT_0 + nCount (here, +1) is documented as "new
            // input... available" - the timer signaling alone
            // (WAIT_OBJECT_0) or the dwMilliseconds fallback expiring
            // (WAIT_TIMEOUT) both mean "woke up with nothing new".
            woke_on_message = (wait_result == WAIT_OBJECT_0 + 1);
        } else {
            // No high-resolution timer (open()'s own refusal path,
            // pre-1803 or otherwise) - nCount 0 waits on input ALONE,
            // for exactly dwMilliseconds (MsgWaitForMultipleObjectsEx's
            // own documented "If this parameter has the value zero,
            // then the function waits only for an input event"); the
            // coarser default scheduler grain this leaves is the
            // declared degradation high_resolution_wait() reports.
            const DWORD wait_result = ::MsgWaitForMultipleObjectsEx(
                0, nullptr, budget_ms, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
            woke_on_message = (wait_result == WAIT_OBJECT_0);
        }
    }

    const bool dispatched_any = drain_queued_messages();
    return gltfx_rslt<bool>::ok(woke_on_message || dispatched_any);
}

} // namespace glintfx::platform

#endif // defined(_WIN32)
