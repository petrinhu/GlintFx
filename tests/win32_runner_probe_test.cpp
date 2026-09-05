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
#include <vector>

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
//
// ADDED IN THIS REVISION (X-GL-0, docs/plano-w6a-janela.md sec. 3): two
// more cases, answering the question that blocks the whole W6b wave -
// does windows-latest hand out a real, modern (3.3 core) OpenGL
// context, or only the software "GDI Generic" 1.1 renderer every
// GPU-less VM is expected to fall back to (that expectation itself is
// declared, not measured - see this file's own header comment style
// and GODS_LAWS.md L-27/L-44: a hypothesis is not a fact until this
// probe's own printed line confirms it)? Same two rules as the
// original case: ONE assertion each (that the window/device context
// this case needs to even ask the question came into existence -
// GetDC/ChoosePixelFormat/SetPixelFormat are baseline OS services that
// have nothing to do with the GPU question being asked, unlike
// wglCreateContextAttribsARB's presence, which IS the question), and
// everything past that line is PRINTED, never asserted - the whole
// point of D-W6a-18's decision tree (docs/plano-w6a-janela.md sec. 3)
// is that the CTO reads the printed line and decides, this file does
// not decide for them by turning "GDI Generic" into a test failure.
//
// wglCreateContext/wglMakeCurrent/wglDeleteContext/ChoosePixelFormat/
// SetPixelFormat/wglGetProcAddress are declared in <wingdi.h>, already
// pulled in by <windows.h> above - confirmed against Microsoft Learn
// (learn.microsoft.com/windows/win32/api/wingdi/nf-wingdi-wglcreatecontext
// and neighboring wingdi.h pages, fetched while writing this revision),
// so no extra header is needed for those. glGetString and the three
// GL_VENDOR/GL_RENDERER/GL_VERSION token values below are NOT in
// wingdi.h - they are core OpenGL, declared by <GL/gl.h>, a header
// this project deliberately never includes even for the SHIPPED
// library (see src/render/gl_abi.hpp's own header comment: GODS_LAWS.md
// L-07 forbids depending on a system GL header, because a consumer
// machine may have the GL RUNTIME but not the GL development headers).
// This test file stays just as self-contained as gl_abi.hpp - rather
// than pull that production header in through a new CMake include
// directory this revision cannot locally verify, it re-declares the
// same three constants gl_abi.hpp's own convention would use, sourced
// from the SAME vendored registry gl_abi.hpp is itself measured
// against (third_party/khronos/gl.xml lines 1072-1074: GL_VENDOR
// 0x1F00, GL_RENDERER 0x1F01, GL_VERSION 0x1F02; line 15364's own
// <command> block gives glGetString's exact C signature, "const
// GLubyte *glGetString(GLenum name)") - a fact with a path, not a
// number remembered from having read a GL header before.
//
// wglCreateContextAttribsARB and its WGL_CONTEXT_*_ARB token values
// belong to a DIFFERENT registry (Khronos's WGL extension specs, not
// gl.xml - this project vendors gl.xml because GL-LOADER's scope is
// the cross-platform GL 3.3 core function set, never a single
// platform's context-creation extension, so nothing here was in scope
// to vendor). Values below are copied from the extensions' own
// published token tables (GODS_LAWS.md L-29: a specification's PUBLIC
// token values are learned, not "plagiarized" - no implementation code
// is copied, only integer constants a program needs to speak the
// extension), fetched live while writing this revision:
//   https://registry.khronos.org/OpenGL/extensions/ARB/WGL_ARB_create_context.txt
//   https://registry.khronos.org/OpenGL/extensions/ARB/WGL_ARB_create_context_profile.txt

// gl_enum/gl_ubyte match the real GL ABI's own <ptype> widths (32-bit
// unsigned int / 8-bit unsigned char - the same widths
// src/render/gl_abi.hpp documents measuring off glcorearb.h and
// gl.xml's <types> section), declared fresh here rather than reusing
// glintfx::render's aliases so this probe stays a single, self-
// contained translation unit (this file already keeps its own
// counters and RAII types instead of reaching into production code).
using gl_enum = unsigned int;
using gl_ubyte = unsigned char;

constexpr gl_enum k_gl_vendor = 0x1F00;
constexpr gl_enum k_gl_renderer = 0x1F01;
constexpr gl_enum k_gl_version = 0x1F02;

// glGetString is exported directly by opengl32.dll (GL 1.0, present in
// every Windows OpenGL implementation including "GDI Generic" - it is
// how a caller finds out WHICH implementation it got, so it cannot
// itself require the ARB extension this probe is trying to detect).
// Declared at file scope, outside the anonymous namespace below: a
// language-linkage ("C") declaration belongs at namespace scope one
// can reason about as ordinary external linkage, not nested inside an
// unnamed namespace where linkage rules for extern "C" get needlessly
// subtle - the exact same reasoning already keeps k_wgl_context_*_arb
// and wgl_create_context_attribs_arb_fn below at this same scope.
extern "C" const gl_ubyte *WINAPI glGetString(gl_enum name);

constexpr int k_wgl_context_major_version_arb = 0x2091;
constexpr int k_wgl_context_minor_version_arb = 0x2092;
constexpr int k_wgl_context_profile_mask_arb = 0x9126;
constexpr int k_wgl_context_core_profile_bit_arb = 0x00000001;

// wglCreateContextAttribsARB has no fixed address to link against -
// unlike glGetString, it is an EXTENSION, resolved at runtime only
// through wglGetProcAddress (this is why D-W6a-18 calls it out by name
// as the one fact this whole probe exists to measure). Matches this
// project's own gl_proc_address.hpp convention for the analogous
// resolve-by-name step: get_proc_address returns a raw pointer, the
// caller reinterpret_casts it to the concrete function type it asked
// for by name.
using wgl_create_context_attribs_arb_fn = HGLRC(WINAPI *)(HDC, HGLRC, const int *);

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

// device_context_guard - GetDC(hwnd) in the constructor, ReleaseDC in
// the destructor. Same non-copyable, destructor-releases shape as
// window_class_guard/window_guard above, for the same reason (this
// harness is case-fatal - see this file's own header comment).
// ReleaseDC's own documentation (learn.microsoft.com/windows/win32/
// api/winuser/nf-winuser-releasedc) requires the SAME hwnd that was
// passed to GetDC, so it is kept alongside the HDC rather than
// re-derived.
class device_context_guard {
  public:
    explicit device_context_guard(HWND hwnd) : m_hwnd(hwnd), m_hdc(::GetDC(hwnd)) {}

    device_context_guard(const device_context_guard &) = delete;
    device_context_guard &operator=(const device_context_guard &) = delete;

    ~device_context_guard() {
        if (m_hdc != nullptr) {
            ::ReleaseDC(m_hwnd, m_hdc);
        }
    }

    [[nodiscard]] HDC get() const noexcept { return m_hdc; }
    [[nodiscard]] bool is_valid() const noexcept { return m_hdc != nullptr; }

  private:
    HWND m_hwnd = nullptr;
    HDC m_hdc = nullptr;
};

// gl_context_guard - wglCreateContext/wglCreateContextAttribsARB
// already produced the HGLRC by the time this guard takes it; the
// destructor follows Microsoft's own documented teardown order
// (learn.microsoft.com/windows/win32/opengl/rendering-context-functions:
// "Before calling wglDeleteContext, make the rendering context not
// current by calling wglMakeCurrent" - wglMakeCurrent(nullptr, nullptr)
// is the documented way to do that without needing to know which
// device context this rendering context is currently paired with).
class gl_context_guard {
  public:
    explicit gl_context_guard(HGLRC context) : m_context(context) {}

    gl_context_guard(const gl_context_guard &) = delete;
    gl_context_guard &operator=(const gl_context_guard &) = delete;

    ~gl_context_guard() {
        if (m_context != nullptr) {
            ::wglMakeCurrent(nullptr, nullptr);
            ::wglDeleteContext(m_context);
        }
    }

    [[nodiscard]] HGLRC get() const noexcept { return m_context; }
    [[nodiscard]] bool is_valid() const noexcept { return m_context != nullptr; }

  private:
    HGLRC m_context = nullptr;
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

// X-GL-0 (docs/plano-w6a-janela.md sec. 2.2 row 2, sec. 3): does this
// runner hand out a modern (3.3 core) OpenGL context, or only the
// software "GDI Generic" 1.1 fallback? This file's own header comment
// (ADDED IN THIS REVISION paragraph) explains why only the window/
// device-context setup is asserted and everything about the GL
// capability itself is printed - D-W6a-18's decision tree is what
// turns these printed lines into a decision, not this test.
GLINTFX_TEST(windows_runner_reports_gl_context_creation_capability) {
    const window_class_guard class_guard;
    GLINTFX_CHECK(class_guard.is_valid());

    ::SetLastError(0);
    HWND hwnd = ::CreateWindowExW(0, k_window_class_name, L"glintfx win32 runner gl probe",
                                  WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 320, 240,
                                  nullptr, nullptr, ::GetModuleHandleW(nullptr), nullptr);
    const window_guard win_guard(hwnd);

    // The ONE assertion this case makes (this file's own header
    // comment): a window is a baseline OS resource this case's own
    // preceding case already proves the runner hands out, so failing
    // here would mean the runner regressed between cases, not that the
    // GL question below has an interesting answer.
    GLINTFX_CHECK(win_guard.is_valid());

    const device_context_guard dc_guard(win_guard.get());
    std::println("win32_runner_probe: GetDC ok={}", dc_guard.is_valid());
    if (!dc_guard.is_valid()) {
        return;
    }

    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    const int pixel_format = ::ChoosePixelFormat(dc_guard.get(), &pfd);
    std::println("win32_runner_probe: ChoosePixelFormat pixel_format={} GetLastError={}",
                 pixel_format, ::GetLastError());
    if (pixel_format == 0) {
        return;
    }

    ::SetLastError(0);
    const BOOL pixel_format_set = ::SetPixelFormat(dc_guard.get(), pixel_format, &pfd);
    std::println("win32_runner_probe: SetPixelFormat ok={} GetLastError={}", pixel_format_set != 0,
                 ::GetLastError());
    if (pixel_format_set == 0) {
        return;
    }

    const gl_context_guard legacy_context(::wglCreateContext(dc_guard.get()));
    std::println("win32_runner_probe: wglCreateContext ok={}", legacy_context.is_valid());
    if (!legacy_context.is_valid()) {
        return;
    }

    const BOOL made_current = ::wglMakeCurrent(dc_guard.get(), legacy_context.get());
    std::println("win32_runner_probe: wglMakeCurrent ok={}", made_current != 0);
    if (made_current == 0) {
        return;
    }

    // Printed, not asserted (this file's own header comment): which
    // GL implementation this runner has IS the fact this probe exists
    // to produce, and "(null)" is itself a legitimate, printworthy
    // answer rather than something to guard against with a fallback
    // that would hide it.
    const gl_ubyte *vendor = ::glGetString(k_gl_vendor);
    const gl_ubyte *renderer = ::glGetString(k_gl_renderer);
    const gl_ubyte *version = ::glGetString(k_gl_version);
    std::println("win32_runner_probe: GL_VENDOR={} GL_RENDERER={} GL_VERSION={}",
                 vendor != nullptr ? reinterpret_cast<const char *>(vendor) : "(null)",
                 renderer != nullptr ? reinterpret_cast<const char *>(renderer) : "(null)",
                 version != nullptr ? reinterpret_cast<const char *>(version) : "(null)");

    // wglGetProcAddress only ever resolves extension pointers for the
    // CURRENT rendering context (its own documentation, quoted in this
    // file's header comment) - legacy_context is current from the
    // wglMakeCurrent call above, which is why that call happens before
    // this one and not after.
    void *const create_context_attribs_arb_raw =
        reinterpret_cast<void *>(::wglGetProcAddress("wglCreateContextAttribsARB"));
    const auto create_context_attribs_arb =
        reinterpret_cast<wgl_create_context_attribs_arb_fn>(create_context_attribs_arb_raw);
    std::println("win32_runner_probe: wglCreateContextAttribsARB available={}",
                 create_context_attribs_arb != nullptr);

    if (create_context_attribs_arb != nullptr) {
        const int attribs[] = {
            k_wgl_context_major_version_arb,
            3,
            k_wgl_context_minor_version_arb,
            3,
            k_wgl_context_profile_mask_arb,
            k_wgl_context_core_profile_bit_arb,
            0,
        };
        const gl_context_guard core_context(
            create_context_attribs_arb(dc_guard.get(), nullptr, attribs));
        std::println("win32_runner_probe: wglCreateContextAttribsARB(3.3 core) ok={}",
                     core_context.is_valid());
    }
}

// X-GL-0 (docs/plano-w6a-janela.md sec. 2.2 row 2): what does this
// runner report for raw input devices (mouse/keyboard/HID counts) and
// the digitizer bitmask, and what DPI does a freshly created window
// carry? All four are printed, never asserted, for the same reason as
// this file's GL case: a headless CI runner reporting zero of
// everything is a measured fact this probe exists to produce, not a
// failure to chase (Y-1, docs/plano-w6a-janela.md fatia 13, is the
// fatia that will act on whatever this prints).
GLINTFX_TEST(windows_runner_reports_raw_input_devices_and_digitizer) {
    const window_class_guard class_guard;
    GLINTFX_CHECK(class_guard.is_valid());

    ::SetLastError(0);
    HWND hwnd = ::CreateWindowExW(0, k_window_class_name, L"glintfx win32 runner device probe",
                                  WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 320, 240,
                                  nullptr, nullptr, ::GetModuleHandleW(nullptr), nullptr);
    const window_guard win_guard(hwnd);

    // The ONE assertion this case makes, for the same reason as the GL
    // case above: window creation is the same baseline fact the first
    // case in this file already proves, unrelated to the device/DPI
    // question this case exists to answer.
    GLINTFX_CHECK(win_guard.is_valid());

    // Two-call idiom straight from GetRawInputDeviceList's own
    // documentation (learn.microsoft.com/windows/win32/api/winuser/
    // nf-winuser-getrawinputdevicelist): first call with a null buffer
    // to learn the count via the out-parameter, second call to fill a
    // buffer sized for that count. The FIRST call's return value is
    // NOT the device count (it is the number of entries written into
    // a null buffer, which is always 0 on success) - device_count is
    // read from the out-parameter, never from that return value.
    UINT device_count = 0;
    const UINT count_query_result =
        ::GetRawInputDeviceList(nullptr, &device_count, sizeof(RAWINPUTDEVICELIST));
    std::println("win32_runner_probe: GetRawInputDeviceList(count query) result={} "
                 "device_count={}",
                 count_query_result, device_count);

    unsigned mouse_count = 0;
    unsigned keyboard_count = 0;
    unsigned hid_count = 0;
    unsigned unknown_count = 0;

    if (count_query_result != static_cast<UINT>(-1) && device_count > 0) {
        std::vector<RAWINPUTDEVICELIST> devices(device_count);
        const UINT filled =
            ::GetRawInputDeviceList(devices.data(), &device_count, sizeof(RAWINPUTDEVICELIST));
        std::println("win32_runner_probe: GetRawInputDeviceList(fill) filled={}", filled);

        if (filled != static_cast<UINT>(-1)) {
            for (UINT i = 0; i < filled; ++i) {
                switch (devices[i].dwType) {
                case RIM_TYPEMOUSE:
                    ++mouse_count;
                    break;
                case RIM_TYPEKEYBOARD:
                    ++keyboard_count;
                    break;
                case RIM_TYPEHID:
                    ++hid_count;
                    break;
                default:
                    ++unknown_count;
                    break;
                }
            }
        }
    }
    std::println("win32_runner_probe: raw_input_devices mouse={} keyboard={} hid={} unknown={}",
                 mouse_count, keyboard_count, hid_count, unknown_count);

    // SM_DIGITIZER (GetSystemMetrics' own documentation, quoted in
    // this file's header comment) is a bitmask of NID_* flags this
    // file does not decode further - Y-1 is the fatia that will care
    // about the individual bits; this probe's job is to print the raw
    // value so that decision has a real number to start from.
    const int digitizer_bitmask = ::GetSystemMetrics(SM_DIGITIZER);
    std::println("win32_runner_probe: GetSystemMetrics(SM_DIGITIZER)={:#04x}", digitizer_bitmask);

    const UINT dpi = ::GetDpiForWindow(win_guard.get());
    std::println("win32_runner_probe: GetDpiForWindow={}", dpi);
}

#endif // defined(_WIN32)
