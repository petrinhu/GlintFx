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
    GLINTFX_CHECK(!opened.has_error());

    // The window was created WITHOUT WS_VISIBLE (D-W5-8/D-W5-13,
    // window_adapter.hpp's own header comment) - mapped here only
    // because THIS case needs a real, visible top-level window to
    // minimize/restore, never because open() itself ever calls
    // ShowWindow.
    ::ShowWindow(window.native_handle(), SW_SHOWNOACTIVATE);

    const glintfx::gltfx_rslt<glintfx::gltfx_present_outcome> presented_before_minimize =
        context.swap_buffers();
    GLINTFX_CHECK(!presented_before_minimize.has_error());
    const bool presented_before =
        presented_before_minimize.value() == glintfx::gltfx_present_outcome::presented;
    std::println("MEASURED win32_iconic_present_test.presented_before_minimize={}",
                 presented_before);
    GLINTFX_CHECK(presented_before);
    const std::uint32_t swaps_before_minimize = context.swap_calls_issued();

    ::ShowWindow(window.native_handle(), SW_MINIMIZE);
    const BOOL is_iconic_now = ::IsIconic(window.native_handle());
    std::println("MEASURED win32_iconic_present_test.is_iconic_after_minimize={}",
                 is_iconic_now != 0);
    GLINTFX_CHECK(is_iconic_now != 0);

    const glintfx::gltfx_rslt<glintfx::gltfx_present_outcome> while_hidden = context.swap_buffers();
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
    const glintfx::gltfx_rslt<glintfx::gltfx_present_outcome> after_restore =
        context.swap_buffers();
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
