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

#include <glintfx/core/err_code.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/win32/display_adapter.hpp"
#include "platform/win32/wgl_context_adapter.hpp"
#include "platform/win32/window_adapter.hpp"

// win32_iconic_present_test.cpp - X-WGL (docs/plano-w6b-placa-e-laco.
// md fatia 4, D-W6b-6/18, GODS_LAWS.md L-04/L-09/L-40): proves, against
// the REAL windows-latest GitHub Actions runner and the Mesa/D3D12
// aparato this project's own CI installs (F3, docs/plano-w6b-placa-e-
// laco.md sec. 0), the ONE mechanism D-W6b-6 promises on this platform
// - IsIconic() before SwapBuffers(), the Win32 mirror of the Wayland
// side's own frame-callback budget (egl_context_adapter.cpp, fatia 3,
// "mecanismo diferente, efeito igual").
//
// win32_gl_context_adapter/win32_window_adapter/win32_display_adapter
// have no GLINTFX_API (deliberately - GODS_LAWS.md L-19, "nada e
// exportado"), so this fixture compiles all three a SECOND time,
// directly into its own object set - the SAME technique every other
// win32/*_test.cpp fixture in this suite already uses (tests/
// CMakeLists.txt's own win32_display_connect_test comment).
//
// This project has no Windows toolchain on the machine that wrote this
// file (GODS_LAWS.md L-27, the same declared limitation every other
// win32/ test in this suite already carries) - the windows CI job is
// the first place any of this ever actually runs.

namespace {

// THE INSTRUMENT THIS FILE'S EARLIER TWO REVISIONS WERE MISSING (team-
// lead briefing, 06/09/2026, on the second CI reproval by the same
// name): a case that measures six booleans and reprova (comment
// header above) yet never once printed WHY the failing gltfx_rslt held
// an error, leaving the next reader to guess. Prints all three
// diagnostic fields a gltfx_err can ever carry for a Win32
// platform_failure (core/err.hpp's own "DIAGNOSTIC CONTEXT" paragraph)
// - the error's own gltfx_err_code by NAME (never the bare integer),
// rejected_value() (WHICH Win32 call this adapter attributes the
// failure to), and os_error_code() (the raw ::GetLastError() that call
// reported - only just wired up, this fatia's own wgl_context_
// adapter.cpp, for every platform_failure this file's adapter can
// produce, the one gemeo every OTHER win32/ adapter already had,
// GODS_LAWS.md L-17). A no-op when `result` holds a value: called
// unconditionally, right before every has_error() check below, so a
// future failure - whichever check trips it - always prints first.
template <typename T>
void print_error_detail_if_failed(std::string_view label,
                                  const glintfx::gltfx_rslt<T> &result) noexcept {
    if (!result.has_error()) {
        return;
    }
    const glintfx::gltfx_err &err = result.error();
    std::println("MEASURED {}.error_code={}", label, glintfx::gltfx_err_code_name(err.code()));
    std::println("MEASURED {}.rejected_value={}", label, err.rejected_value());
    std::println("MEASURED {}.os_error_code={}", label, err.os_error_code());
}

[[nodiscard]] bool
open_display_and_window(glintfx::platform::win32_display_adapter &display,
                        glintfx::platform::win32_window_adapter &window) noexcept {
    if (display.open().has_error()) {
        return false;
    }
    glintfx::platform::win32_window_desc desc{};
    desc.logical_width = 320;
    desc.logical_height = 240;
    desc.title = "glintfx win32 iconic present test";
    return !window.open(display, desc).has_error();
}

} // namespace

GLINTFX_TEST(win32_gl_context_swap_buffers_skips_iconic_window) {
    glintfx::platform::win32_display_adapter display;
    glintfx::platform::win32_window_adapter window;
    GLINTFX_CHECK(open_display_and_window(display, window));

    glintfx::platform::win32_gl_context_adapter context;
    const std::vector<glintfx::gltfx_gfx_option_entry> options;
    const glintfx::gltfx_rslt<void> opened = context.open(window, options);
    std::println("MEASURED win32_iconic_present_test.context_open_ok={}", !opened.has_error());
    print_error_detail_if_failed("win32_iconic_present_test.context_open", opened);
    GLINTFX_CHECK(!opened.has_error());

    // The window was created WITHOUT WS_VISIBLE (D-W5-8/D-W5-13,
    // window_adapter.hpp's own header comment) - mapped here only
    // because THIS case needs a real, visible top-level window to
    // minimize/restore, never because open() itself ever calls
    // ShowWindow.
    ::ShowWindow(window.native_handle(), SW_SHOWNOACTIVATE);
    // pump_events() after EVERY ShowWindow() (display_adapter.hpp's
    // own header comment on pump_events(): "the non-blocking event
    // pump ARCH-PORTS's own consumer loop calls once per frame" - a
    // REAL consumer never goes from a window-state change straight to
    // GL work without pumping in between). ShowWindow()'s own
    // WM_WINDOWPOSCHANGED -> WM_SIZE/WM_MOVE chain is SENT (delivered
    // synchronously, window_message_route.hpp's own header comment),
    // but Windows also POSTS messages around a visibility/iconic
    // transition (WM_PAINT, WM_NCACTIVATE among them) that only reach
    // this window's own queue through a pump - this fixture is the
    // FIRST win32 test in this suite to call ShowWindow() at all, and
    // was the first to skip the pump every other consumer of this
    // adapter is documented to perform.
    GLINTFX_CHECK(!display.pump_events().has_error());

    const glintfx::gltfx_rslt<glintfx::gltfx_present_outcome> presented_before_minimize =
        context.swap_buffers();
    print_error_detail_if_failed("win32_iconic_present_test.presented_before_minimize",
                                 presented_before_minimize);
    GLINTFX_CHECK(!presented_before_minimize.has_error());
    const bool presented_before =
        presented_before_minimize.value() == glintfx::gltfx_present_outcome::presented;
    std::println("MEASURED win32_iconic_present_test.presented_before_minimize={}",
                 presented_before);
    GLINTFX_CHECK(presented_before);
    const std::uint32_t swaps_before_minimize = context.swap_calls_issued();

    ::ShowWindow(window.native_handle(), SW_MINIMIZE);
    GLINTFX_CHECK(!display.pump_events().has_error()); // same reasoning as the pump above
    const BOOL is_iconic_now = ::IsIconic(window.native_handle());
    std::println("MEASURED win32_iconic_present_test.is_iconic_after_minimize={}",
                 is_iconic_now != 0);
    GLINTFX_CHECK(is_iconic_now != 0);

    const glintfx::gltfx_rslt<glintfx::gltfx_present_outcome> while_hidden = context.swap_buffers();
    print_error_detail_if_failed("win32_iconic_present_test.while_hidden", while_hidden);
    GLINTFX_CHECK(!while_hidden.has_error());
    const bool skipped_hidden =
        while_hidden.value() == glintfx::gltfx_present_outcome::skipped_hidden;
    std::println("MEASURED win32_iconic_present_test.skipped_while_iconic={}", skipped_hidden);
    GLINTFX_CHECK(skipped_hidden);
    // THE FACT NO RETURN VALUE ALONE COULD PROVE (this file's own
    // header comment): swap_buffers() must NEVER have actually called
    // ::SwapBuffers() while iconic - a mutant that returns
    // skipped_hidden but swaps anyway would still pass the assertion
    // above.
    GLINTFX_CHECK(context.swap_calls_issued() == swaps_before_minimize);

    ::ShowWindow(window.native_handle(), SW_RESTORE);
    // SECOND CI REPROVAL BY THIS SAME NAME (team-lead briefing,
    // 06/09/2026, GODS_LAWS.md L-42's own "segunda reprovacao obriga
    // busca antes da terceira"): the pump added below (the fix the
    // FIRST reproval's own comment, preserved above unedited, argued
    // for) did NOT close the gap - the real windows-latest runner
    // still measured `!after_restore.has_error()` FALSE with the pump
    // in place. Two hypotheses remained on the table, restated from
    // the first reproval: a real WGL adapter defect on restore (fix
    // the product), or the headless runner genuinely needing more than
    // one pump cycle to finish delivering the SW_RESTORE transition
    // before a present can succeed (Chromium's own message_pump_win.cc
    // loops PeekMessage more than once per iteration for exactly this
    // class of "one pump is not always enough" reasoning - the same
    // busca this fatia's own team-lead ordered before this attempt).
    // NEITHER hypothesis is asserted here as the answer: the code
    // path in win32_gl_context_adapter (src/platform/win32/wgl_
    // context_adapter.cpp) was re-read end to end for this reproval
    // and still shows no WM_SIZE/WM_ACTIVATE handling and no path that
    // could leave a stale DC/context across a minimize cycle (m_dc is
    // CS_OWNDC, display_adapter.cpp's own class registration - not the
    // shared/common DC Windows can silently invalidate), which argues
    // AGAINST a product defect without proving the environment either.
    // print_error_detail_if_failed() below is what turns "it failed
    // again" into an actual verdict on the NEXT run: gltfx_err_code,
    // WHICH Win32 call this adapter blames (rejected_value()), and the
    // raw ::GetLastError() that call reported (os_error_code(), only
    // just wired up by this same reproval - wgl_context_adapter.cpp's
    // swap_buffers() was the one platform_failure site in this file
    // that never attached it, GODS_LAWS.md L-17's own gemeo). This
    // project has no Windows toolchain to reproduce either hypothesis
    // locally (GODS_LAWS.md L-27) - the windows-latest job is still the
    // only place that can settle which one this is, but it will now do
    // so with a code and a GetLastError() printed, not a bare
    // pass/fail.
    GLINTFX_CHECK(!display.pump_events().has_error());
    const glintfx::gltfx_rslt<glintfx::gltfx_present_outcome> after_restore =
        context.swap_buffers();
    print_error_detail_if_failed("win32_iconic_present_test.after_restore", after_restore);
    GLINTFX_CHECK(!after_restore.has_error());
    const bool presented_after_restore =
        after_restore.value() == glintfx::gltfx_present_outcome::presented;
    std::println("MEASURED win32_iconic_present_test.presented_after_restore={}",
                 presented_after_restore);
    GLINTFX_CHECK(presented_after_restore);
    GLINTFX_CHECK(context.swap_calls_issued() == swaps_before_minimize + 1);
}

// D-W6b-18: v-sync `adaptive` on Windows is honored (wglSwapIntervalEXT
// (-1)) ONLY when this system's own WGL_EXT_swap_control_tear is
// present, and refused BY NAME otherwise - never silently downgraded
// to plain `on`, and the mirror of the Wayland side's own UNCONDITIONAL
// refusal (Wayland/EGL has no equivalent extension at all, ever).
// Whether the Mesa/D3D12 aparato this project's own CI installs
// actually advertises the extension is a MEASURED, driver-dependent
// fact (GODS_LAWS.md L-44) - printed BEFORE it decides which branch
// this case asserts, never assumed either way.
GLINTFX_TEST(win32_gl_context_adaptive_vsync_matches_driver_capability) {
    glintfx::platform::win32_display_adapter display;
    glintfx::platform::win32_window_adapter window;
    GLINTFX_CHECK(open_display_and_window(display, window));

    glintfx::platform::win32_gl_context_adapter context;
    const std::vector<glintfx::gltfx_gfx_option_entry> options;
    GLINTFX_CHECK(!context.open(window, options).has_error());

    const glintfx::gltfx_gfx_option_support vsync_support =
        context.option_support(glintfx::gltfx_gfx_option::vsync);
    const bool vsync_id_supported = vsync_support == glintfx::gltfx_gfx_option_support::supported;
    std::println("MEASURED win32_iconic_present_test.vsync_option_support_supported={}",
                 vsync_id_supported);
    // vsync itself (on/off) is supported on every driver this project
    // targets in practice - only `adaptive`, a specific VALUE, may be
    // refused (D-W6b-16's own id-vs-value distinction, egl_context_
    // adapter.cpp's own option_support() documents the identical
    // shape one directory over).
    GLINTFX_CHECK(vsync_id_supported);

    const glintfx::gltfx_rslt<void> adaptive =
        context.apply_option({.id = glintfx::gltfx_gfx_option::vsync, .value = 2});
    const bool adaptive_accepted = !adaptive.has_error();
    std::println("MEASURED win32_iconic_present_test.vsync_adaptive_accepted={}",
                 adaptive_accepted);

    if (!adaptive_accepted) {
        // D-W6b-18: refused BY NAME, never silently downgraded to `on`.
        GLINTFX_CHECK(adaptive.error().code() == glintfx::gltfx_err_code::unsupported);
        GLINTFX_CHECK(adaptive.error().rejected_value() == "vsync");
    }
    // adaptive_accepted == true needs no further assertion: D-W6b-18's own success case IS
    // "apply_option() returned without error" - the same fact already MEASURED and printed above,
    // and re-asserting !adaptive.has_error() inside `if (adaptive_accepted)` would be dead code by
    // construction (cppcheck's own oppositeInnerCondition, caught live in this fatia's own preci.sh
    // run: the branch condition already guarantees it).
}

#endif // defined(_WIN32)
