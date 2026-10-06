// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/win32/wgl_extension_loader.hpp"

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <cwchar>

#include <glintfx/core/err_code.hpp>

#include "platform/extension_token.hpp"
#include "platform/win32/wgl_srgb_pixel_format.hpp"

// wgl_extension_loader.cpp - see this file's own header comment for
// the full "why a disposable window" reasoning. RAII is deliberately
// NOT used here (unlike tests/win32_runner_probe_test.cpp's own guard
// classes): every step below is a plain, sequential Win32 call with an
// explicit, ordered teardown on every exit path - the same "no
// exception ever crosses this boundary" discipline this project's own
// gltfx_rslt<T> convention already assumes (docs/api-conventions.md
// R3), so there is no throwing constructor/destructor pair for RAII to
// protect here in the first place.
//
// wglCreateContext/wglMakeCurrent/wglDeleteContext/wglGetProcAddress/
// ChoosePixelFormat/SetPixelFormat are declared in <wingdi.h>, already
// pulled in by <windows.h> - confirmed against Microsoft Learn by
// tests/win32_runner_probe_test.cpp's own header comment, the same
// sequence of calls this file reuses for the throwaway legacy context.

namespace glintfx::platform {

namespace {

std::atomic<std::uint64_t> g_loader_sequence{0};

// Wide enough for "GlintfxWglExt" (13 chars) plus a zero-padded
// 16-hex-digit sequence number plus the terminating NUL (30) - the
// same "checked once here instead of trusted by inspection"
// (GODS_LAWS.md L-17) discipline win32_display_adapter.cpp's own
// static_assert next to class_name_for() already applies to the
// identical kind of arithmetic, one directory over.
constexpr std::size_t k_class_name_chars = 40;

LRESULT CALLBACK loader_window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) noexcept {
    return ::DefWindowProcW(hwnd, msg, wparam, lparam);
}

// A name unique to THIS call (this file's own header comment, "A
// UNIQUE CLASS NAME PER CALL") - swprintf (<cwchar>, standard C++, the
// same choice win32_display_adapter::class_name_for() already
// documents making over the MSVC-only _snwprintf_s) formatting an
// atomically-incremented sequence number, never a meaningful decode of
// it by anyone reading the name later.
[[nodiscard]] std::array<wchar_t, k_class_name_chars> loader_class_name() noexcept {
    std::array<wchar_t, k_class_name_chars> buffer{};
    const std::uint64_t sequence = g_loader_sequence.fetch_add(1, std::memory_order_relaxed);
    // Return value checked (cert-err33-c) though unreachable in
    // practice: k_class_name_chars fits the fixed prefix plus 16 hex
    // digits plus the NUL with room to spare, so this exact format
    // string can never overrun the buffer - same reasoning class_name_
    // for()'s own comment gives for the identical shape.
    const int written = ::swprintf(buffer.data(), buffer.size(), L"GlintfxWglExt%016llx",
                                   static_cast<unsigned long long>(sequence));
    (void)written;
    return buffer;
}

using wgl_get_extensions_string_arb_fn = const char *(WINAPI *)(HDC);
using wgl_get_extensions_string_ext_fn = const char *(WINAPI *)();

// The WGL extension list of the CURRENT context (D-SRGB-2, D-SRGB2-5):
// wglGetExtensionsStringARB(dc), and when the driver does not resolve it, the older
// wglGetExtensionsStringEXT(). nullptr when it has neither. The string is the driver's: it is valid
// only while the context is current, so the caller reads FACTS off it and never keeps the pointer.
[[nodiscard]] const char *read_wgl_extension_list(HDC dc) noexcept {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) reason: the dlsym-style cast of
    // an extension entry point, as everywhere in this file.
    const auto get_arb = reinterpret_cast<wgl_get_extensions_string_arb_fn>(
        ::wglGetProcAddress("wglGetExtensionsStringARB"));
    if (get_arb != nullptr) {
        return get_arb(dc);
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) reason: same as above.
    const auto get_ext = reinterpret_cast<wgl_get_extensions_string_ext_fn>(
        ::wglGetProcAddress("wglGetExtensionsStringEXT"));
    return get_ext != nullptr ? get_ext() : nullptr;
}

// What the driver announces, as facts, read while the disposable context is current.
[[nodiscard]] wgl_advertised read_wgl_advertised(HDC dc) noexcept {
    const char *list = read_wgl_extension_list(dc);
    wgl_advertised advertised;
    advertised.framebuffer_srgb = wgl_framebuffer_srgb_advertised(list);
    advertised.swap_control_tear = extension_token_listed(list, "WGL_EXT_swap_control_tear");
    return advertised;
}

// Resolves the four entry points and reads the announced facts. Called ONLY while the disposable
// legacy context is current: wglGetProcAddress and the extension list both need it.
[[nodiscard]] gltfx_rslt<wgl_extension_pointers> resolve_current_context_pointers(HDC dc) noexcept {
    wgl_extension_pointers pointers{};
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) reason: the universal
    // dlsym-style function-to-object-pointer cast every GL loader already relies on.
    pointers.choose_pixel_format_arb =
        reinterpret_cast<void *>(::wglGetProcAddress("wglChoosePixelFormatARB"));
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) reason: same as above.
    pointers.create_context_attribs_arb =
        reinterpret_cast<void *>(::wglGetProcAddress("wglCreateContextAttribsARB"));
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) reason: same as above.
    pointers.swap_interval_ext =
        reinterpret_cast<void *>(::wglGetProcAddress("wglSwapIntervalEXT"));
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) reason: same as above.
    pointers.get_pixel_format_attribiv_arb =
        reinterpret_cast<void *>(::wglGetProcAddress("wglGetPixelFormatAttribivARB"));
    pointers.advertised = read_wgl_advertised(dc);

    if (pointers.choose_pixel_format_arb == nullptr ||
        pointers.create_context_attribs_arb == nullptr) {
        // These two are NOT optional (wgl_extension_loader.hpp's own struct comment): without
        // them there is no way to open the real window's own modern, ARB-chosen 3.3 core context
        // at all.
        return gltfx_rslt<wgl_extension_pointers>::err(
            gltfx_err(gltfx_err_code::platform_failure)
                .with_rejected_value("wgl_loader_extensions"));
    }
    return gltfx_rslt<wgl_extension_pointers>::ok(pointers);
}

} // namespace

gltfx_rslt<wgl_extension_pointers>
load_wgl_extension_pointers(HWND *out_discarded_window) noexcept {
    const std::array<wchar_t, k_class_name_chars> class_name = loader_class_name();
    // Not `const HINSTANCE` (clang-tidy misc-misplaced-const, caught on
    // the real Windows CI runner): HINSTANCE is itself a pointer
    // typedef (`struct HINSTANCE__ *`), so `const HINSTANCE` applies
    // the qualifier to the POINTER, not the pointee
    // (`HINSTANCE__ *const`, never `const HINSTANCE__ *`) - the same
    // reasoning every other HWND/HDC/HGLRC local in this file already
    // follows by simply never writing `const` in front of them.
    HINSTANCE module = ::GetModuleHandleW(nullptr);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    // CS_OWNDC - same reasoning win32_display_adapter.cpp's own
    // registered class already documents: a private, persistent DC
    // this throwaway window can GetDC() once and hold for its own,
    // short lifetime.
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = &loader_window_proc;
    wc.hInstance = module;
    wc.lpszClassName = class_name.data();

    const ATOM class_atom = ::RegisterClassExW(&wc);
    if (class_atom == 0) {
        return gltfx_rslt<wgl_extension_pointers>::err(
            gltfx_err(gltfx_err_code::platform_failure).with_rejected_value("wgl_loader_class"));
    }

    HWND hwnd = ::CreateWindowExW(0, class_name.data(), L"", WS_OVERLAPPEDWINDOW, 0, 0, 1, 1,
                                  nullptr, nullptr, module, nullptr);
    if (hwnd == nullptr) {
        ::UnregisterClassW(class_name.data(), module);
        return gltfx_rslt<wgl_extension_pointers>::err(
            gltfx_err(gltfx_err_code::platform_failure).with_rejected_value("wgl_loader_window"));
    }
    // Test seam (this header's own comment on out_discarded_window):
    // recorded the moment the handle exists, valid for every teardown
    // path below - the value itself never changes before DestroyWindow
    // actually runs, only the CALLER's snapshot of it does.
    if (out_discarded_window != nullptr) {
        *out_discarded_window = hwnd;
    }

    HDC dc = ::GetDC(hwnd);
    if (dc == nullptr) {
        ::DestroyWindow(hwnd);
        ::UnregisterClassW(class_name.data(), module);
        return gltfx_rslt<wgl_extension_pointers>::err(
            gltfx_err(gltfx_err_code::platform_failure).with_rejected_value("wgl_loader_dc"));
    }

    // A plain, legacy pixel format - never the ARB-chosen one (that is
    // what THIS function exists to make possible on the REAL window,
    // this file's own header comment): RGBA, double-buffered, enough
    // to create SOME legacy context and make it current, nothing more.
    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    const int pixel_format = ::ChoosePixelFormat(dc, &pfd);
    if (pixel_format == 0 || ::SetPixelFormat(dc, pixel_format, &pfd) == 0) {
        ::ReleaseDC(hwnd, dc);
        ::DestroyWindow(hwnd);
        ::UnregisterClassW(class_name.data(), module);
        return gltfx_rslt<wgl_extension_pointers>::err(
            gltfx_err(gltfx_err_code::platform_failure)
                .with_rejected_value("wgl_loader_pixel_format"));
    }

    HGLRC legacy_context = ::wglCreateContext(dc);
    if (legacy_context == nullptr) {
        ::ReleaseDC(hwnd, dc);
        ::DestroyWindow(hwnd);
        ::UnregisterClassW(class_name.data(), module);
        return gltfx_rslt<wgl_extension_pointers>::err(
            gltfx_err(gltfx_err_code::platform_failure)
                .with_rejected_value("wgl_loader_legacy_context"));
    }

    gltfx_rslt<wgl_extension_pointers> result = gltfx_rslt<wgl_extension_pointers>::err(
        gltfx_err(gltfx_err_code::platform_failure).with_rejected_value("wgl_loader_make_current"));

    if (::wglMakeCurrent(dc, legacy_context) != 0) {
        // Everything that needs the legacy context is read HERE, while it is current (D-SRGB2-5).
        result = resolve_current_context_pointers(dc);
        ::wglMakeCurrent(nullptr, nullptr);
    }

    ::wglDeleteContext(legacy_context);
    ::ReleaseDC(hwnd, dc);
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(class_name.data(), module);
    return result;
}

} // namespace glintfx::platform

#endif // defined(_WIN32)
