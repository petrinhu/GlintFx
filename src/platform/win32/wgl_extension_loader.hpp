// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <glintfx/core/err.hpp>

// platform/win32/wgl_extension_loader.hpp - X-WGL (docs/plano-w6b-
// placa-e-laco.md fatia 4, GODS_LAWS.md L-04/L-17/L-19/L-29): the ONE
// atom that resolves the three WGL extension functions win32_gl_
// context_adapter::open() (this fatia) needs BEFORE it can touch the
// real window's own pixel format - wglChoosePixelFormatARB,
// wglCreateContextAttribsARB, wglSwapIntervalEXT.
//
// WHY A DISPOSABLE WINDOW (this fatia's own busca, docs/plano-w6b-
// placa-e-laco.md sec. 0, "WGL moderno"): wglGetProcAddress only ever
// resolves an extension function against a CURRENTLY CURRENT rendering
// context (its own documentation, quoted in tests/win32_runner_probe_
// test.cpp's own header comment) - so a LEGACY context has to exist
// and be current before either ARB function can be resolved AT ALL.
// SetPixelFormat may be called only ONCE per window (learn.microsoft.
// com/windows/win32/api/wingdi/nf-wingdi-setpixelformat, and D-W6b-4's
// own citation of that same rule) - the REAL window's own pixel format
// has to be the ARB-chosen one (msaa/srgb honored, D-W6b-4), never a
// throwaway legacy one set first and then somehow replaced. The only
// way to resolve the ARB functions without spending the real window's
// one SetPixelFormat call on a format nobody wants is a SEPARATE,
// disposable window - created, used, and torn down entirely inside
// this one function call, never seen by anything else in this project.
//
// A UNIQUE CLASS NAME PER CALL (this file's own .cpp, an atomic
// sequence number formatted the same `swprintf` way win32_display_
// adapter::class_name_for() already does one directory over, D-W6a-16):
// this loader may run more than once in the same process (once per
// gltfx_gl_context::open() call, GL-CONTEXT's own v1 is single-context-
// at-a-time but nothing stops a consumer opening, closing, and
// reopening a context over the same window) - a fixed class name would
// risk ERROR_CLASS_ALREADY_EXISTS if a previous call's own
// UnregisterClassW ever raced a fast reopen on another thread; a
// per-call name removes the question entirely, the same reasoning
// D-W6a-16 already gives for win32_display_adapter's own per-instance
// class name.
//
// TEARS DOWN EVERYTHING BEFORE RETURNING, SUCCESS OR FAILURE: the
// legacy context, the disposable window, and its registered class are
// ALL gone by the time this function returns - the three resolved
// function pointers are the only thing that survives, exactly what
// win32_gl_context_adapter::open() needs to then touch the REAL
// window's own device context for the first and only time.

namespace glintfx::platform {

// The three pointers this atom resolves, untyped (void*) the same way
// egl_context_adapter.hpp keeps EGLDisplay/EGLContext/EGLSurface as
// void* rather than pulling <EGL/egl.h> into a header nothing else in
// this file needs - win32_gl_context_adapter.cpp (this fatia) is the
// ONE translation unit that casts each one to its real WGL function-
// pointer type, immediately before calling it.
struct wgl_extension_pointers {
    void *choose_pixel_format_arb =
        nullptr; // BOOL(WINAPI*)(HDC, const int*, const FLOAT*, UINT, int*, UINT*)
    void *create_context_attribs_arb = nullptr; // HGLRC(WINAPI*)(HDC, HGLRC, const int*)
    // May legitimately stay nullptr - a driver with no WGL_EXT_swap_
    // control at all still hands out a valid 3.3 core context (this
    // header's own "TEARS DOWN" paragraph); win32_gl_context_adapter's
    // own option_support(vsync) degrades to unsupported_here instead
    // of this atom failing the whole load over a missing v-sync knob
    // (D-W6b-16's own "nunca degrada em silencio", applied one layer
    // up, not here).
    void *swap_interval_ext = nullptr; // BOOL(WINAPI*)(int)
};

// `out_discarded_window`, when non-null, receives the disposable
// window's own HWND right before this function tears it down (never
// after - the pointer is a snapshot of a value that is about to become
// invalid) - a TEST SEAM ONLY (win32_wgl_extension_loader_test.cpp,
// this fatia), letting a test confirm ::IsWindow() answers false for
// that exact handle once this call returns, the one fact this atom's
// own reversed teardown promises but that no return value alone can
// prove. Production code (win32_gl_context_adapter::open(), this
// fatia) never passes anything but the default nullptr - the same
// "test-only accessor, never used by open()/close() themselves" shape
// win32_window_adapter::size_messages_before_open_returns() already
// documents one directory over.
[[nodiscard]] gltfx_rslt<wgl_extension_pointers>
load_wgl_extension_pointers(HWND *out_discarded_window = nullptr) noexcept;

} // namespace glintfx::platform

#endif // defined(_WIN32)
