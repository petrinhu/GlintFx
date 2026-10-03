// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/win32/wgl_pixel_format_cascade.hpp"

#if defined(_WIN32)

#include <array>
#include <cstddef>

#include <glintfx/core/err_code.hpp>

#include "platform/win32/wgl_srgb_pixel_format.hpp"

// wgl_pixel_format_cascade.cpp - D-SRGB-2 (D-SRGB2-2/3, S5): the pixel format cascade of the WGL
// adapter (see wgl_pixel_format_cascade.hpp). Moved out of wgl_context_adapter.cpp without any
// change of behavior; the token values are copied from the published Khronos registry tables
// (GODS_LAWS.md L-29), the SAME ones that file cites:
//   https://registry.khronos.org/OpenGL/extensions/ARB/WGL_ARB_pixel_format.txt
//   https://registry.khronos.org/OpenGL/extensions/ARB/WGL_ARB_multisample.txt
//   https://registry.khronos.org/OpenGL/extensions/ARB/WGL_ARB_framebuffer_sRGB.txt

namespace glintfx::platform {

namespace {

// WGL_ARB_pixel_format.
constexpr int k_wgl_draw_to_window_arb = 0x2001;
constexpr int k_wgl_support_opengl_arb = 0x2010;
constexpr int k_wgl_double_buffer_arb = 0x2011;
constexpr int k_wgl_pixel_type_arb = 0x2013;
constexpr int k_wgl_color_bits_arb = 0x2014;
constexpr int k_wgl_depth_bits_arb = 0x2022;
constexpr int k_wgl_stencil_bits_arb = 0x2023;
constexpr int k_wgl_type_rgba_arb = 0x202B;
// WGL_ARB_multisample.
constexpr int k_wgl_sample_buffers_arb = 0x2041;
constexpr int k_wgl_samples_arb = 0x2042;
// WGL_ARB_framebuffer_sRGB: WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB lives in wgl_srgb_pixel_format.hpp.

using wgl_get_pixel_format_attribiv_arb_fn = BOOL(WINAPI *)(HDC, int, int, UINT, const int *,
                                                            int *);

// RGBA8+stencil8, no depth (D-W6b-4), same shape egl_context_adapter.cpp's own choose_config()
// builds one directory over: a fixed-size array, never a heap allocation, appended to only when
// msaa/srgb were actually requested.
[[nodiscard]] std::array<int, 24> build_choose_attribs(const wgl_format_request &request) noexcept {
    std::array<int, 24> attribs = {
        k_wgl_draw_to_window_arb, TRUE, k_wgl_support_opengl_arb, TRUE,
        k_wgl_double_buffer_arb,  TRUE, k_wgl_pixel_type_arb,     k_wgl_type_rgba_arb,
        k_wgl_color_bits_arb,     32,   k_wgl_stencil_bits_arb,   8,
        k_wgl_depth_bits_arb,     0,
    };
    std::size_t next = 14;
    if (request.msaa_samples > 0) {
        attribs[next++] = k_wgl_sample_buffers_arb;
        attribs[next++] = 1;
        attribs[next++] = k_wgl_samples_arb;
        attribs[next++] = static_cast<int>(request.msaa_samples);
    }
    if (request.srgb) {
        attribs[next++] = k_wgl_framebuffer_srgb_capable_arb;
        attribs[next++] = TRUE;
    }
    attribs[next] = 0;
    return attribs;
}

// One wglChoosePixelFormatARB call for `request`.
[[nodiscard]] bool try_choose_format(HDC dc, wgl_choose_pixel_format_arb_fn fn,
                                     const wgl_format_request &request, int &format_out) noexcept {
    const std::array<int, 24> attribs = build_choose_attribs(request);
    int format = 0;
    UINT num_formats = 0;
    // GEMEO (GODS_LAWS.md L-17/L-22, achado no integrador run 34171429194 - ver o header comment
    // de swap_buffers() para o incidente completo): ::SetLastError(0) IMEDIATAMENTE antes de toda
    // chamada Win32/WGL cujo fracasso este arquivo atribui via ::GetLastError(); sem isso, uma
    // chamada que falha SEM chamar SetLastError() devolve o valor RESIDUAL de uma chamada anterior
    // (learn.microsoft.com/windows/win32/api/errhandlingapi/nf-errhandlingapi-getlasterror#remarks).
    // tests/tools/check_win32_last_error_cleared.py vigia este sitio.
    ::SetLastError(0);
    const bool ok =
        fn(dc, attribs.data(), nullptr, 1, &format, &num_formats) != 0 && num_formats > 0;
    if (ok) {
        format_out = format;
    }
    return ok;
}

} // namespace

[[nodiscard]] wgl_format_request
read_wgl_format_request(std::span<const gltfx_gfx_option_entry> options) noexcept {
    wgl_format_request request;
    for (const gltfx_gfx_option_entry &entry : options) {
        if (entry.id == gltfx_gfx_option::msaa_samples) {
            request.msaa_samples = entry.value;
        } else if (entry.id == gltfx_gfx_option::srgb_framebuffer) {
            request.srgb = entry.value != 0;
        }
    }
    return request;
}

// D-SRGB2-2 steps 1 and 2: with the sRGB asked and NOT announced there is no choose at all (the
// decision refuses by the announcement, D-A62); otherwise the choose with everything asked, and,
// only if it failed with the sRGB asked, a second one without the sRGB just to name the culprit.
[[nodiscard]] wgl_format_choice choose_wgl_formats(HDC dc, wgl_choose_pixel_format_arb_fn fn,
                                                   const wgl_format_request &request,
                                                   bool srgb_advertised) noexcept {
    wgl_format_choice choice;
    if (request.srgb && !srgb_advertised) {
        return choice;
    }
    choice.found = try_choose_format(dc, fn, request, choice.format);
    choice.found_without_srgb = choice.found;
    if (choice.found) {
        return choice;
    }
    choice.os_error = ::GetLastError();
    if (request.srgb) {
        wgl_format_request without_srgb = request;
        without_srgb.srgb = false;
        int unused_format = 0;
        choice.found_without_srgb = try_choose_format(dc, fn, without_srgb, unused_format);
    }
    return choice;
}

// D-SRGB2-2 step 3 (D-A62): asks the driver whether `format` is sRGB capable. Only a call that
// succeeded and answered TRUE confirms; a missing query, a failed call or any other answer is "not
// confirmed". The attribute is read on `format` AS A PIXEL FORMAT INDEX of `dc`, so it can be asked
// before the one SetPixelFormat the window will ever get.
[[nodiscard]] bool confirm_wgl_srgb_capable(HDC dc, void *get_pixel_format_attribiv_arb,
                                            int format) noexcept {
    if (get_pixel_format_attribiv_arb == nullptr || format <= 0) {
        return false;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) reason: the dlsym-style cast of
    // an extension entry point, as everywhere in this file.
    const auto query =
        reinterpret_cast<wgl_get_pixel_format_attribiv_arb_fn>(get_pixel_format_attribiv_arb);
    const int attribute = k_wgl_framebuffer_srgb_capable_arb;
    int value = 0;
    const BOOL query_ok = query(dc, format, /*iLayerPlane=*/0, 1, &attribute, &value);
    return wgl_srgb_confirmed(query_ok != 0, value);
}

// The refusal the neutral decision (gfx_format_decision.hpp) chose, as the error open() returns,
// the mirror of egl_context_adapter.cpp's own wgl_refusal_result(): `no_format` is a platform
// failure.
[[nodiscard]] gltfx_rslt<void> wgl_refusal_result(gfx_format_refusal refusal,
                                                  DWORD os_error) noexcept {
    switch (refusal) {
    case gfx_format_refusal::none:
        return gltfx_rslt<void>::ok();
    case gfx_format_refusal::srgb_framebuffer:
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::unsupported).with_rejected_value("srgb_framebuffer"));
    case gfx_format_refusal::msaa_samples:
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::unsupported).with_rejected_value("msaa_samples"));
    case gfx_format_refusal::no_format:
        break;
    }
    return gltfx_rslt<void>::err(gltfx_err(gltfx_err_code::platform_failure)
                                     .with_rejected_value("wgl_pixel_format")
                                     .with_os_error_code(os_error));
}

// The facts the neutral decision reads (gfx_format_decision.hpp), from what the options asked, what
// the choose answered, what the driver announces and what the query confirmed.
[[nodiscard]] gfx_format_facts make_wgl_format_facts(const wgl_format_request &request,
                                                     const wgl_format_choice &choice,
                                                     bool srgb_advertised,
                                                     bool srgb_confirmed) noexcept {
    return gfx_format_facts{
        .msaa_requested = request.msaa_samples > 0,
        .srgb_requested = request.srgb,
        .srgb_advertised = srgb_advertised,
        .format_found = choice.found,
        .format_found_without_srgb = choice.found_without_srgb,
        .srgb_confirmed = srgb_confirmed,
    };
}

// D-SRGB2-2 step 4: SetPixelFormat may be called only ONCE per window (D-W6b-4,
// learn.microsoft.com/windows/win32/api/wingdi/nf-wingdi-setpixelformat), which is why it comes
// LAST: nothing that could still refuse the format runs after it. DescribePixelFormat is the
// standard way to obtain a valid PIXELFORMATDESCRIPTOR for an ARB-chosen format.
[[nodiscard]] gltfx_rslt<void> bind_wgl_pixel_format(HDC dc, int format) noexcept {
    PIXELFORMATDESCRIPTOR pfd{};
    // GEMEO (GODS_LAWS.md L-17/L-22) - ver o comentario de try_choose_format() acima.
    ::SetLastError(0);
    if (::DescribePixelFormat(dc, format, sizeof(PIXELFORMATDESCRIPTOR), &pfd) == 0) {
        return gltfx_rslt<void>::err(gltfx_err(gltfx_err_code::platform_failure)
                                         .with_rejected_value("wgl_pixel_format")
                                         .with_os_error_code(::GetLastError()));
    }
    // GEMEO (GODS_LAWS.md L-17/L-22) - mesmo motivo do comentario acima.
    ::SetLastError(0);
    if (::SetPixelFormat(dc, format, &pfd) == 0) {
        return gltfx_rslt<void>::err(gltfx_err(gltfx_err_code::platform_failure)
                                         .with_rejected_value("wgl_pixel_format")
                                         .with_os_error_code(::GetLastError()));
    }
    return gltfx_rslt<void>::ok();
}

} // namespace glintfx::platform

#endif // defined(_WIN32)
