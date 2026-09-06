// SPDX-License-Identifier: AGPL-3.0-or-later
#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdint>
#include <print>

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

GLINTFX_TEST(wgl_naive_proc_address_lookup_needs_a_fallback_exactly_when_unresolved) {
    // THE BITE: exactly the naive resolver an earlier version of this
    // fatia would have shipped - only wglGetProcAddress, no fallback.
    //
    // THIRD REVISION OF THIS CASE, and the second correction is the
    // lesson to keep (GODS_LAWS.md L-44/L-42): the FIRST revision
    // asserted strict nullptr for the naive lookup. The SECOND
    // (Microsoft's own reference page, learn.microsoft.com/windows/
    // win32/api/wingdi/nf-wingdi-wglgetprocaddress, "Return value":
    // "When no current rendering context exists or the function
    // fails, the return value is NULL" - the four extra sentinels
    // 1/2/3/-1 this project's own busca had found are a WIDELY
    // OBSERVED, driver-specific quirk, not Microsoft's own documented
    // contract for the no-context case) widened the assertion to any
    // of five shapes. The REAL windows-latest runner measured a THIRD
    // thing neither revision allowed for: a genuinely RESOLVED address
    // for glGetString, from the naive call alone, with no current
    // rendering context anywhere in this process. Two busca rounds
    // both confirmed the same thing from the other side (Khronos
    // Forums, "Examples of cases... wglGetProcAddress() returns
    // non-NULL on error", and the OpenGL wiki's Load_OpenGL_Functions
    // page): which of the documented-or-not shapes a given driver
    // actually returns is a FACT OF THAT DRIVER, never a portable
    // assumption - this project's own test cannot keep asserting a
    // shape and calling it "the" behavior.
    //
    // What THIS case asserts instead is what glintfx's OWN CODE
    // guarantees no matter which shape the driver underneath happens
    // to produce: resolve_wgl_proc_address() never hands a caller one
    // of the five documented "unresolved" shapes, and it only replaces
    // the naive answer with something else when the naive answer WAS
    // one of those five - never overriding a driver that already
    // resolved the name on its own (this atom's own header comment,
    // "falling back... only when wglGetProcAddress itself returns
    // nullptr or one of its four documented sentinel values").
    //
    // Not `const PROC` (clang-tidy misc-misplaced-const - the same
    // gemeo class fixed in src/platform/win32/wgl_extension_loader.cpp
    // and tests/win32_window_close_request_test.cpp, L-17's own "varra
    // a superficie inteira atras do gemeo": PROC is ALSO a pointer
    // typedef, `INT_PTR (WINAPI *PROC)()`, so `const PROC` qualifies
    // the pointer, not the pointee).
    PROC naive = ::wglGetProcAddress("glGetString");
    const auto naive_value = reinterpret_cast<std::intptr_t>(naive);
    std::println("MEASURED win32_wgl_proc_address_test.naive_lookup_value={}",
                 static_cast<long long>(naive_value));
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) reason: same universal
    // dlsym-style cast wgl_proc_address.cpp's own resolve_wgl_proc_address() already uses.
    const bool naive_is_unresolved =
        glintfx::platform::is_unresolved_sentinel(reinterpret_cast<void *>(naive_value));
    std::println("MEASURED win32_wgl_proc_address_test.naive_is_unresolved={}",
                 naive_is_unresolved);

    void *resolved = glintfx::platform::resolve_wgl_proc_address("glGetString");
    // THE GUARANTEE, true regardless of which of the two MEASURED
    // shapes above this particular driver produced: never a sentinel
    // coming back out.
    GLINTFX_CHECK(!glintfx::platform::is_unresolved_sentinel(resolved));
    if (naive_is_unresolved) {
        // The naive path was unusable - the fallback MUST have kicked
        // in and produced something other than the sentinel itself.
        GLINTFX_CHECK(resolved != reinterpret_cast<void *>(naive_value));
    } else {
        // The naive path already resolved on its own (this project's
        // own measured case on the real runner, see above) - the
        // resolver must accept that answer unchanged, never second-
        // guess a driver that already did the right thing.
        GLINTFX_CHECK(resolved == reinterpret_cast<void *>(naive_value));
    }
}

GLINTFX_TEST(wgl_is_unresolved_sentinel_recognizes_all_five_documented_shapes) {
    // What the case above deliberately stopped asserting (which shape
    // a given driver produces) moves HERE, where it belongs: a direct,
    // driver-INDEPENDENT proof that glintfx's own classification of
    // the five documented "unresolved" shapes (wgl_proc_address.hpp's
    // own header comment, "THE SECOND HALF OF THE ARMADILHA" -
    // nullptr, or the sentinels 1, 2, 3, -1) is complete, using
    // synthetic values instead of whatever one driver happens to hand
    // back.
    for (const std::intptr_t sentinel : {std::intptr_t{0}, std::intptr_t{1}, std::intptr_t{2},
                                         std::intptr_t{3}, std::intptr_t{-1}}) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) reason: synthetic
        // sentinel values under test, never dereferenced - same dlsym-style cast category
        // wgl_proc_address.cpp's own resolve_wgl_proc_address() already uses.
        GLINTFX_CHECK(
            glintfx::platform::is_unresolved_sentinel(reinterpret_cast<void *>(sentinel)));
    }
    // An ordinary, genuinely resolved address (this very function's
    // own entry point - never called through this pointer, only its
    // bit pattern compared) must NOT be misidentified as one of the
    // five unresolved shapes.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) reason: same universal
    // dlsym-style function-to-object-pointer cast as above.
    void *ordinary_address = reinterpret_cast<void *>(&glintfx::platform::resolve_wgl_proc_address);
    GLINTFX_CHECK(!glintfx::platform::is_unresolved_sentinel(ordinary_address));
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
