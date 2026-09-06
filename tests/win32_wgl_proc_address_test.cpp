// SPDX-License-Identifier: AGPL-3.0-or-later
#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/win32/wgl_proc_address.hpp"

// win32_wgl_proc_address_test.cpp - X-WGL (docs/plano-w6b-placa-e-
// laco.md fatia 4, GODS_LAWS.md L-04/L-20): the test that bites the
// exact armadilha this fatia's own busca names (sec. 0, "WGL moderno")
// - a resolver that only ever calls wglGetProcAddress never resolves
// an OpenGL 1.1 core function like glGetString. This fixture never
// creates a window, a device context, or a rendering context: with NO
// current rendering context at all, wglGetProcAddress's own
// documentation (learn.microsoft.com/windows/win32/api/wingdi/nf-
// wingdi-wglgetprocaddress) makes plain the return is NULL for every
// name, including one opengl32.dll exports directly - and this
// project's own src/platform/win32/CMakeLists.txt links opengl32
// directly into glintfx_library (this fatia), so opengl32.dll is
// already loaded in this very process the moment main() starts: an
// ordinary GetProcAddress against that already-loaded module needs no
// GL context whatsoever, unlike wglGetProcAddress.
//
// This project has no Windows toolchain on the machine that wrote this
// file (GODS_LAWS.md L-27, the same declared limitation tests/win32_
// runner_probe_test.cpp's own header comment already carries) - the
// windows CI job is the first place any of this ever actually runs.

GLINTFX_TEST(wgl_naive_proc_address_lookup_fails_for_gl_1_1_function_without_a_context) {
    // THE BITE: exactly the naive resolver an earlier version of this
    // fatia would have shipped - only wglGetProcAddress, no fallback -
    // returns nullptr here, because there is no current rendering
    // context anywhere in this process yet.
    const PROC naive = ::wglGetProcAddress("glGetString");
    GLINTFX_CHECK(naive == nullptr);
}

GLINTFX_TEST(wgl_resolve_falls_back_to_opengl32_export_table_for_gl_1_1_function) {
    // resolve_wgl_proc_address() closes the bite above: glGetString is
    // an ordinary export of the already-loaded opengl32.dll, resolved
    // through GetProcAddress once wglGetProcAddress's own answer is
    // recognized as unusable (nullptr here; one of the four documented
    // sentinels on some drivers).
    void *resolved = glintfx::platform::resolve_wgl_proc_address("glGetString");
    GLINTFX_CHECK(resolved != nullptr);
}

GLINTFX_TEST(wgl_resolve_returns_null_for_a_name_neither_path_resolves) {
    // An ordinary lookup miss (this project's own docs/api-
    // conventions.md shape for proc_address()-style resolution, never
    // a gltfx_err) - a name that is neither a real opengl32.dll export
    // nor a real WGL extension anywhere.
    void *resolved =
        glintfx::platform::resolve_wgl_proc_address("GlintfxThisFunctionDoesNotExistAnywhere");
    GLINTFX_CHECK(resolved == nullptr);
}

#endif // defined(_WIN32)
