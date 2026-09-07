// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

// platform/gl/gl_memory_facts.hpp - GL-GPU-KIND, "via 2" reader (docs/
// plano-w6b-fatias-5b-revisao.md sec. 1.5, D-W6b-38, the lider's own
// second question: "Se tiver apenas nvidia, outra maneira e checar se
// ram e vram sao separadas ou compartilhadas"): PLAIN DATA read off
// the CURRENT GL context by GL_NVX_gpu_memory_info (glGetIntegerv,
// tokens cited by number in gl_memory_facts.cpp per third_party/
// khronos/gl.xml, GODS_LAWS.md L-07: no vendored header included).
//
// `nvx_present=false` means the extension is not in this context's own
// implementation - `dedicated_kb`/`total_available_kb` are then
// meaningless and memory_separation_kind.hpp's own classifier answers
// "no opinion" (std::nullopt), never a guess.
struct gl_memory_facts {
    bool nvx_present = false;
    std::int64_t dedicated_kb = -1;
    std::int64_t total_available_kb = -1;
};

// read_gl_memory_facts() - PLATFORM-AGNOSTIC (D-W6b-38's own "os tres
// ficam em platform/gl/ porque valem nos dois sistemas"), but GL
// function pointers themselves are resolved DIFFERENTLY per platform
// (eglGetProcAddress on Linux, statically-linked opengl32.lib exports
// on Windows - the SAME split egl_context_adapter.cpp/wgl_context_
// adapter.cpp already have for glGetString/glGetIntegerv) - so the
// CALLING adapter, which already resolved these two GL 1.1 entry
// points for its own version check, passes them in rather than this
// shared atom re-resolving them a second, platform-specific way.
//
// PRESENCE IS DECIDED BY glGetError(), NEVER BY SCANNING GL_EXTENSIONS
// (docs/api-conventions.md's own "never a fragile text scan when a
// numeric answer exists"): glGetIntegerv() on an enum this driver does
// not implement leaves the output untouched and raises GL_INVALID_
// ENUM - reading that error IMMEDIATELY after the call (never batching
// two glGetIntegerv() calls before checking) is what tells `nvx_
// present` apart from a driver that happens to write zero.
//
// GLINTFX_GL_STDCALL: every GL entry point on Windows is __stdcall
// (wgl_context_adapter.cpp's own extern "C" ... WINAPI declarations,
// one directory over) - a raw function-pointer typedef with NO calling
// convention defaults to __cdecl there, which would corrupt the stack
// on every call through it. `__stdcall` is a bare COMPILER KEYWORD
// (MSVC, MinGW-GCC, clang-cl all recognize it with no header at all,
// same as `__cdecl`) - this file stays dependency-zero (no <windows.h>)
// while still getting the convention right on the one platform where
// it matters; on every other platform GLINTFX_GL_STDCALL expands to
// nothing, exactly what egl_context_adapter.cpp's own plain `void
// (*)(gl_enum, gl_int *)` already assumes.
#if defined(_WIN32)
#define GLINTFX_GL_STDCALL __stdcall
#else
#define GLINTFX_GL_STDCALL
#endif

using gl_get_integerv_fn = void(GLINTFX_GL_STDCALL *)(unsigned int pname, int *params);
using gl_get_error_fn = unsigned int(GLINTFX_GL_STDCALL *)();

#undef GLINTFX_GL_STDCALL

[[nodiscard]] gl_memory_facts read_gl_memory_facts(gl_get_integerv_fn get_integerv,
                                                   gl_get_error_fn get_error) noexcept;
