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

#include <array>
#include <cstdint>

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

// wglGetProcAddress needs a NUL-terminated name, but `name` arrives as
// a std::string_view with no such guarantee - the same reasoning
// egl_context_adapter.cpp's own proc_address() already documents for
// eglGetProcAddress one file over. Every real GL/WGL function name is
// short and well-known (the longest published ARB/EXT names stay well
// under 100 characters), so a fixed, bounded stack buffer copies the
// whole name WITHOUT ever allocating - clang-tidy's bugprone-
// exception-escape caught, on the real Windows CI runner, that this
// file's earlier revision (`const std::string owned(name);`) put an
// allocating, throw-capable constructor inside a function that
// promises `noexcept` (this project's own "no exception ever crosses
// this boundary" discipline, docs/api-conventions.md R3, applied here
// even though this atom sits below the public API surface). A `name`
// too long to fit is treated as an ordinary lookup miss (the same
// shape any other unresolvable name already gets), never truncated.
constexpr std::size_t k_max_name_chars = 256;

[[nodiscard]] bool copy_name_into(std::array<char, k_max_name_chars> &buffer,
                                  std::string_view name) noexcept {
    if (name.size() >= buffer.size()) {
        return false;
    }
    // string_view::copy(dest, count, pos) with the literal pos=0 used
    // here can never hit its own out_of_range precondition (pos=0 is
    // always <= size(), empty or not) - the one call left in this
    // function that the standard leaves free to declare throwing.
    name.copy(buffer.data(), name.size());
    buffer[name.size()] = '\0';
    return true;
}

} // namespace

void *resolve_wgl_proc_address(std::string_view name) noexcept {
    std::array<char, k_max_name_chars> name_buffer{};
    if (!copy_name_into(name_buffer, name)) {
        // Longer than any real WGL/GL function name - neither path
        // below could ever resolve it anyway.
        return nullptr;
    }

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) reason: the universal
    // dlsym-style function-to-object-pointer cast every GL loader already relies on (the
    // SAME technique egl_context_adapter.cpp's own proc_address() uses one file over).
    void *extension_address = reinterpret_cast<void *>(::wglGetProcAddress(name_buffer.data()));
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
    return reinterpret_cast<void *>(::GetProcAddress(opengl32, name_buffer.data()));
}

} // namespace glintfx::platform

#endif // defined(_WIN32)
