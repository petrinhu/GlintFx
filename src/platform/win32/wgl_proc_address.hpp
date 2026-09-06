// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#if defined(_WIN32)

#include <string_view>

// platform/win32/wgl_proc_address.hpp - X-WGL (docs/plano-w6b-placa-e-
// laco.md fatia 4, GODS_LAWS.md L-04/L-17/L-19): the Win32 mirror of
// what eglGetProcAddress already does for wayland_egl_context_adapter
// (egl_context_adapter.cpp, fatia 3) - the ONE atom win32_gl_context_
// adapter::proc_address() (context.hpp's own public contract) calls to
// resolve ANY GL/WGL function by name, and the atom win32_gl_context_
// adapter's own internal steps (resolving wglGetExtensionsStringARB to
// detect WGL_EXT_swap_control_tear, D-W6b-18) reuse instead of a second,
// hand-rolled wglGetProcAddress call.
//
// THE ARMADILHA THIS ATOM EXISTS TO CLOSE (this fatia's own busca,
// docs/plano-w6b-placa-e-laco.md sec. 0, "WGL moderno"): wglGetProc
// Address does NOT resolve OpenGL 1.1 core functions (glGetString,
// glClear, glViewport, and around forty more) - Microsoft's own
// documentation for the function says plainly it "retrieves the
// address of an OpenGL extension function", and every real-world WGL
// loader (the gist/guide this fatia's own busca cites) resolves those
// ~40 names through GetProcAddress(GetModuleHandleW(L"opengl32"))
// instead. A caller that only ever tries wglGetProcAddress gets nullptr
// for exactly this class of name - win32_wgl_proc_address_test.cpp is
// the test that bites on that exact mistake.
//
// THE SECOND HALF OF THE ARMADILHA, ALSO CLOSED HERE: wglGetProcAddress
// can return a NON-NULL SENTINEL instead of nullptr for a name it does
// not resolve - Microsoft's own reference page for the function lists
// the documented sentinel values (1, 2, 3, or -1, cast to the return
// type) as "not appropriate error codes" a caller still has to treat as
// failure. A resolver that only checks `!= nullptr` would hand a
// caller one of these four fake addresses instead of falling back to
// opengl32's own export table - resolve_wgl_proc_address() below checks
// all five failure shapes (nullptr plus the four sentinels) before
// ever accepting wglGetProcAddress's own answer.
//
// REQUIRES A CURRENT RENDERING CONTEXT (the same precondition
// wglGetProcAddress's own documentation states): this atom never
// creates or makes one current itself - win32_gl_context_adapter (this
// fatia) only ever calls it after its own wglMakeCurrent() succeeded,
// the same discipline egl_context_adapter.cpp's own proc_address()
// gives eglGetProcAddress one file over.

namespace glintfx::platform {

// Resolves `name` against the CURRENTLY CURRENT WGL rendering context,
// falling back to opengl32.dll's own static export table (this
// header's own "THE ARMADILHA" paragraph) when wglGetProcAddress
// itself returns nullptr or one of its four documented sentinel
// values. Returns nullptr when NEITHER path resolves `name` - an
// ordinary lookup miss, the same shape gltfx_gl_context::proc_address()
// (context.hpp) already promises a consumer, never a gltfx_err.
[[nodiscard]] void *resolve_wgl_proc_address(std::string_view name) noexcept;

} // namespace glintfx::platform

#endif // defined(_WIN32)
