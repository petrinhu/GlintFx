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
#include "platform/win32/wgl_extension_loader.hpp"

// win32_wgl_extension_loader_test.cpp - X-WGL (docs/plano-w6b-placa-e-
// laco.md fatia 4, GODS_LAWS.md L-04/L-09/L-40): proves, against the
// REAL windows-latest GitHub Actions runner and the Mesa/D3D12 aparato
// tools/ci/install-mesa-opengl32.ps1 already installs there (F3, docs/
// plano-w6b-placa-e-laco.md sec. 0), that load_wgl_extension_pointers()
// (this fatia) resolves the two ARB functions win32_gl_context_adapter
// cannot open a modern context without, AND that the disposable window
// it creates to do so is genuinely gone by the time it returns - never
// leaked, never left registered for a caller to trip over later.
//
// This project has no Windows toolchain on the machine that wrote this
// file (GODS_LAWS.md L-27, the same declared limitation tests/win32_
// runner_probe_test.cpp's own header comment already carries) - the
// windows CI job is the first place any of this ever actually runs.

GLINTFX_TEST(wgl_extension_loader_resolves_the_two_required_pointers) {
    HWND discarded_window = nullptr;
    const glintfx::gltfx_rslt<glintfx::platform::wgl_extension_pointers> loaded =
        glintfx::platform::load_wgl_extension_pointers(&discarded_window);

    std::println("win32_wgl_extension_loader_test: load ok={}", !loaded.has_error());
    std::println("MEASURED win32_wgl_extension_loader_test.load_ok={}", !loaded.has_error());

    // The ONE assertion this case makes about the pointers themselves
    // (this file's own header comment): choose_pixel_format_arb and
    // create_context_attribs_arb are NOT optional (wgl_extension_
    // loader.hpp's own struct comment) - without either one, win32_gl_
    // context_adapter has no way to open the real window's own modern,
    // ARB-chosen 3.3 core context at all, on ANY driver this project
    // targets.
    GLINTFX_CHECK(!loaded.has_error());
    GLINTFX_CHECK(loaded.value().choose_pixel_format_arb != nullptr);
    GLINTFX_CHECK(loaded.value().create_context_attribs_arb != nullptr);

    // swap_interval_ext MAY legitimately be nullptr on a driver with no
    // WGL_EXT_swap_control at all (this header's own struct comment) -
    // printed, not asserted, on the Mesa/D3D12 aparato this project's
    // own CI installs; win32_gl_context_adapter's own option_support
    // (vsync) is what degrades honestly if this is ever null there.
    std::println("win32_wgl_extension_loader_test: swap_interval_ext resolved={}",
                 loaded.value().swap_interval_ext != nullptr);
    std::println("MEASURED win32_wgl_extension_loader_test.swap_interval_ext_resolved={}",
                 loaded.value().swap_interval_ext != nullptr);

    // The disposable window itself: gone, by construction, before
    // load_wgl_extension_pointers() ever returns (every exit path in
    // that function calls DestroyWindow before returning) - this is
    // the ONE fact no return value alone could prove, which is why the
    // test seam exists (wgl_extension_loader.hpp's own comment on
    // out_discarded_window).
    GLINTFX_CHECK(discarded_window != nullptr);
    const BOOL still_a_window = ::IsWindow(discarded_window);
    std::println("win32_wgl_extension_loader_test: IsWindow(discarded_window)={}",
                 still_a_window != 0);
    std::println("MEASURED win32_wgl_extension_loader_test.discarded_window_is_window={}",
                 still_a_window != 0);
    GLINTFX_CHECK(still_a_window == 0);
}

GLINTFX_TEST(wgl_extension_loader_can_run_more_than_once_in_the_same_process) {
    // A second, independent call - never reusing the first call's own
    // class name (this fatia's own per-call sequence number,
    // wgl_extension_loader.cpp) - proves the FIRST call's own teardown
    // did not leave anything behind that would make a SECOND real
    // context genuinely un-openable later (win32_gl_context_adapter's
    // own v1 opens and closes a context per gltfx_gl_context::open()
    // call, never assuming this atom only ever runs once per process).
    const glintfx::gltfx_rslt<glintfx::platform::wgl_extension_pointers> first =
        glintfx::platform::load_wgl_extension_pointers();
    const glintfx::gltfx_rslt<glintfx::platform::wgl_extension_pointers> second =
        glintfx::platform::load_wgl_extension_pointers();

    std::println("win32_wgl_extension_loader_test: first_ok={} second_ok={}", !first.has_error(),
                 !second.has_error());
    std::println("MEASURED win32_wgl_extension_loader_test.repeat_first_ok={}", !first.has_error());
    std::println("MEASURED win32_wgl_extension_loader_test.repeat_second_ok={}",
                 !second.has_error());
    GLINTFX_CHECK(!first.has_error());
    GLINTFX_CHECK(!second.has_error());
}

#endif // defined(_WIN32)
