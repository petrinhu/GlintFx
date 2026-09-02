// SPDX-License-Identifier: AGPL-3.0-or-later
#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <print>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// win32_runner_probe_test.cpp - X-0 (TODO.md, GODS_LAWS.md L-09/L-40):
// a DIAGNOSTIC probe, not a witness for a backend that does not exist
// yet. src/platform/CMakeLists.txt's own WIN32 branch says so in
// plain terms ("no display backend on this platform yet, WIN-WINDOW
// pending") - nobody in this project has measured whether the
// windows-latest GitHub Actions runner even hands out an interactive
// window station where CreateWindowExW and the WM_SIZE/WM_ACTIVATE
// messages behave the way every Win32 tutorial assumes. Writing
// WIN-WINDOW's real backend on top of that assumption would be a bet,
// not an engineering decision (GODS_LAWS.md L-44: do not declare a
// mechanism's behavior without testing it first).
//
// WHAT THIS FILE ACTUALLY PROVES: only that a window CAN be created
// (GLINTFX_CHECK below - the one true assertion this probe makes).
// Everything else - GetLastError() after each Win32 call, how many
// messages PeekMessageW drained in a bounded budget, and whether
// WM_SIZE/WM_ACTIVATE arrived - is PRINTED, never asserted. A runner
// that creates a window but never delivers WM_SIZE is a fact to
// design the real backend around, not a test failure to chase.
//
// CORRECTED IN THIS REVISION: the first version of this probe counted
// WM_SIZE/WM_ACTIVATE only inside the PeekMessageW loop below, and a
// real CI run (windows-latest, GitHub Actions, run 33643748402) came
// back with both counters at zero even though RegisterClassExW,
// CreateWindowExW and ShowWindow all reported ok=true GetLastError=0.
// That zero did not prove the runner withholds these messages - per
// Microsoft's own documentation, WM_SIZE and WM_ACTIVATE are
// NONQUEUED messages, sent directly to the window procedure rather
// than posted to the thread's message queue:
//   - "About Messages and Message Queues" lists queued messages as
//     mouse/keyboard input plus WM_TIMER/WM_PAINT/WM_QUIT, and says
//     "most other messages, which are sent directly to a window
//     procedure, are called nonqueued messages"
//     (https://learn.microsoft.com/windows/win32/winmsg/about-messages-and-message-queues).
//   - The WM_SIZE reference page opens with "Sent to a window after
//     its size has changed" and says DefWindowProc sends WM_SIZE (and
//     WM_MOVE) while handling WM_WINDOWPOSCHANGED
//     (https://learn.microsoft.com/windows/win32/winmsg/wm-size).
//   - The WM_ACTIVATE reference page opens with "Sent to both the
//     window being activated and the window being deactivated... the
//     message is sent synchronously"
//     (https://learn.microsoft.com/windows/win32/inputdev/wm-activate).
//   - PeekMessageW's own Remarks section confirms the mechanism that
//     makes the old counter blind to this traffic: "the system
//     dispatches (DispatchMessage) pending, nonqueued messages, that
//     is, messages sent to windows owned by the calling thread using
//     the SendMessage... function", BEFORE it retrieves the first
//     queued message
//     (https://learn.microsoft.com/windows/win32/api/winuser/nf-winuser-peekmessagew).
// A nonqueued message is delivered straight to probe_window_proc and
// never becomes a MSG the PeekMessageW loop can pull out - so a
// PeekMessageW-only counter reading zero was always the EXPECTED
// outcome on a working runner, not evidence of one that withholds the
// messages. This revision counts in both places (window-procedure
// delivery and the queue loop) and prints all four numbers under
// distinct names, so the two hypotheses - "runner behaves like every
// Win32 tutorial assumes" vs. "runner is actually silent" - produce
// different, distinguishable output instead of the same ambiguous
// zero.
//
// DECLARED SCOPE, per GODS_LAWS.md L-27 (fact vs inference) and the
// order that produced this file: this project has no Windows
// toolchain on this machine (clang-cl exists here but without a
// standard library or the Windows SDK, measured before writing a line
// of this file) - so nothing in this translation unit has been
// compiled or run locally. Every Win32 call below is written from
// Microsoft's own current documentation, not from having watched it
// run. The GitHub Actions windows-latest job is the FIRST place this
// file ever executes; whatever it prints there is the fact this probe
// exists to produce.
//
// RAII SHAPE copied from tests/asset_load_test.cpp's own
// exclusive_handle_guard (same file family already documents copying
// that pattern from tests/display_connect_failure_test.cpp): this
// harness is CASE-FATAL (harness/check.hpp) - a failing GLINTFX_CHECK
// unwinds the current case via an exception, so any Win32 handle this
// probe opens must be released by a destructor, never by a line of
// code that a thrown exception can skip over.

namespace {

// A name specific to this probe: two runs of this test on the same
// machine (or two Windows jobs in the same CI matrix) must never
// collide on a class name still registered by a previous, unrelated
// process.
constexpr wchar_t k_window_class_name[] = L"GlintfxWin32RunnerProbeWindowClass";

// Upper bound on how many queued messages one case body drains from
// PeekMessageW. This is a BUDGET, not an expectation: a runner that
// queues zero messages is a measured fact (see messages_pumped in the
// printed line below), not a starved loop - PM_REMOVE without
// PM_NOYIELD does not block, so an empty queue simply ends the loop on
// its own the first time PeekMessageW returns FALSE.
constexpr UINT k_pump_budget = 64;

// Counters incremented from inside probe_window_proc itself, not from
// the PeekMessageW loop below - this is the half of the measurement
// that catches WM_SIZE/WM_ACTIVATE when the system delivers them as
// NONQUEUED messages (see this file's header comment and the
// Microsoft Learn pages it cites). probe_window_proc is a callback
// the system invokes directly, so file-scope storage is the only way
// the case body can read what happened inside it; plain unsigned
// (no atomic, no lock) is safe here only because this whole probe -
// window creation, message pump, and the callback the system invokes
// during both - runs on a single thread. If this file ever grows a
// second thread, these must become atomics or gain a lock first.
unsigned g_wndproc_wm_size_count = 0;
unsigned g_wndproc_wm_activate_count = 0;

LRESULT CALLBACK probe_window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_SIZE) {
        ++g_wndproc_wm_size_count;
    } else if (msg == WM_ACTIVATE) {
        ++g_wndproc_wm_activate_count;
    }
    return ::DefWindowProcW(hwnd, msg, wparam, lparam);
}

// window_class_guard - registers k_window_class_name in the
// constructor, unregisters it in the destructor. is_valid()/
// last_error() report what RegisterClassExW actually did instead of
// the case body guessing from a bool alone.
class window_class_guard {
  public:
    window_class_guard() {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = 0;
        wc.lpfnWndProc = &probe_window_proc;
        wc.hInstance = ::GetModuleHandleW(nullptr);
        wc.lpszClassName = k_window_class_name;

        ::SetLastError(0);
        m_atom = ::RegisterClassExW(&wc);
        m_last_error = ::GetLastError();
    }

    window_class_guard(const window_class_guard &) = delete;
    window_class_guard &operator=(const window_class_guard &) = delete;

    ~window_class_guard() {
        if (m_atom != 0) {
            ::UnregisterClassW(k_window_class_name, ::GetModuleHandleW(nullptr));
        }
    }

    [[nodiscard]] bool is_valid() const noexcept { return m_atom != 0; }
    [[nodiscard]] DWORD last_error() const noexcept { return m_last_error; }

  private:
    ATOM m_atom = 0;
    DWORD m_last_error = 0;
};

// window_guard - same non-copyable, destructor-releases shape as
// tests/asset_load_test.cpp's exclusive_handle_guard, adapted to HWND/
// DestroyWindow instead of HANDLE/CloseHandle.
class window_guard {
  public:
    explicit window_guard(HWND hwnd) : m_hwnd(hwnd) {}

    window_guard(const window_guard &) = delete;
    window_guard &operator=(const window_guard &) = delete;

    ~window_guard() {
        if (m_hwnd != nullptr) {
            ::DestroyWindow(m_hwnd);
        }
    }

    [[nodiscard]] HWND get() const noexcept { return m_hwnd; }
    [[nodiscard]] bool is_valid() const noexcept { return m_hwnd != nullptr; }

  private:
    HWND m_hwnd = nullptr;
};

} // namespace

GLINTFX_TEST(windows_runner_reports_window_creation_and_message_pump_state) {
    const window_class_guard class_guard;
    std::println("win32_runner_probe: RegisterClassExW ok={} GetLastError={}",
                 class_guard.is_valid(), class_guard.last_error());

    ::SetLastError(0);
    HWND hwnd = ::CreateWindowExW(0, k_window_class_name, L"glintfx win32 runner probe",
                                  WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 320, 240,
                                  nullptr, nullptr, ::GetModuleHandleW(nullptr), nullptr);
    const DWORD create_last_error = ::GetLastError();
    const window_guard win_guard(hwnd);
    std::println("win32_runner_probe: CreateWindowExW ok={} GetLastError={}", win_guard.is_valid(),
                 create_last_error);

    // The ONE assertion this probe makes (this file's own header
    // comment): everything from here on is measurement, printed
    // regardless of outcome, never a pass/fail condition.
    GLINTFX_CHECK(win_guard.is_valid());

    ::SetLastError(0);
    const BOOL show_previously_visible = ::ShowWindow(win_guard.get(), SW_SHOWNOACTIVATE);
    const DWORD show_last_error = ::GetLastError();
    std::println("win32_runner_probe: ShowWindow(SW_SHOWNOACTIVATE) previously_visible={} "
                 "GetLastError={}",
                 show_previously_visible != 0, show_last_error);

    unsigned messages_pumped = 0;
    unsigned queue_wm_size_count = 0;
    unsigned queue_wm_activate_count = 0;
    MSG msg{};
    while (messages_pumped < k_pump_budget &&
           ::PeekMessageW(&msg, win_guard.get(), 0, 0, PM_REMOVE) != 0) {
        ++messages_pumped;
        if (msg.message == WM_SIZE) {
            ++queue_wm_size_count;
        } else if (msg.message == WM_ACTIVATE) {
            ++queue_wm_activate_count;
        }
        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
    }

    // GODS_LAWS.md L-40 (piso de varredura nao-vazia): every count is
    // printed even at zero - a silent zero would be indistinguishable
    // from "this loop never ran at all". Four names, two sources
    // (this file's header comment explains why they can legitimately
    // disagree): wndproc_* comes from probe_window_proc catching
    // nonqueued WM_SIZE/WM_ACTIVATE as the system delivers them
    // in-line during CreateWindowExW/ShowWindow; queue_* comes from
    // this PeekMessageW loop, which only ever sees queued messages.
    std::println("win32_runner_probe: messages_pumped={} (budget={}) "
                 "wndproc_wm_size={} wndproc_wm_activate={} "
                 "queue_wm_size={} queue_wm_activate={}",
                 messages_pumped, k_pump_budget, g_wndproc_wm_size_count,
                 g_wndproc_wm_activate_count, queue_wm_size_count, queue_wm_activate_count);
}

#endif // defined(_WIN32)
