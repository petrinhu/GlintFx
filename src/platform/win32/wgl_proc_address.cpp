// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/win32/wgl_proc_address.hpp"

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdint>
#include <string>

// wgl_proc_address.cpp - see this file's own header comment for the
// two-part armadilha this resolver closes. wglGetProcAddress/
// GetProcAddress/GetModuleHandleW are declared in <windows.h> already
// (winbase.h/wingdi.h, both pulled in transitively) - the same
// confirmation tests/win32_runner_probe_test.cpp's own header comment
// already gives for the identical set of calls.

namespace glintfx::platform {

namespace {

// The four documented sentinel values wglGetProcAddress may return
// instead of nullptr for a name it does not resolve (learn.microsoft.
// com/windows/win32/api/wingdi/nf-wingdi-wglgetprocaddress, "Remarks":
// "some implementations will return 1, 2, 3, or -1... these values...
// are not appropriate error codes"), plus nullptr itself - this file's
// own header comment, "THE SECOND HALF OF THE ARMADILHA".
[[nodiscard]] bool is_unresolved_sentinel(void *address) noexcept {
    const auto value = reinterpret_cast<std::intptr_t>(address);
    return value == 0 || value == 1 || value == 2 || value == 3 || value == -1;
}

} // namespace

void *resolve_wgl_proc_address(std::string_view name) noexcept {
    // wglGetProcAddress needs a NUL-terminated name, same reasoning
    // egl_context_adapter.cpp's own proc_address() already documents
    // for eglGetProcAddress one file over.
    const std::string owned(name);

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) reason: the universal
    // dlsym-style function-to-object-pointer cast every GL loader already relies on (the
    // SAME technique egl_context_adapter.cpp's own proc_address() uses one file over).
    void *extension_address = reinterpret_cast<void *>(::wglGetProcAddress(owned.c_str()));
    if (!is_unresolved_sentinel(extension_address)) {
        return extension_address;
    }

    // GL 1.1 core functions (glGetString, glClear, glViewport, and
    // around forty more, this file's own header comment) are NOT
    // resolved by wglGetProcAddress at all - they are ordinary exports
    // of opengl32.dll, resolved the same way any other DLL export is.
    HMODULE opengl32 = ::GetModuleHandleW(L"opengl32.dll");
    if (opengl32 == nullptr) {
        return nullptr;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) reason: same universal
    // dlsym-style cast as above, for GetProcAddress's own FARPROC return type.
    return reinterpret_cast<void *>(::GetProcAddress(opengl32, owned.c_str()));
}

} // namespace glintfx::platform

#endif // defined(_WIN32)
