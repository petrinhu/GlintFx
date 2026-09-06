// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/win32/wgl_context_adapter.hpp"

#if defined(_WIN32)

#include <string>

#include <glintfx/core/err_code.hpp>

#include "platform/gl/gl_version_policy.hpp"
#include "platform/win32/wgl_extension_loader.hpp"
#include "platform/win32/wgl_proc_address.hpp"
#include "platform/win32/window_adapter.hpp"

// wgl_context_adapter.cpp - X-WGL (docs/plano-w6b-placa-e-laco.md
// fatia 4, D-W6b-1/4/6/7/17/18, GODS_LAWS.md L-04/L-07/L-17/L-19/L-31):
// the real WGL calls wgl_context_adapter.hpp's own header comment
// names, one atom per named step, correspondence by correspondence
// with egl_context_adapter.cpp one directory over (this fatia's own
// team-lead briefing, 06/09/2026).
//
// GL FUNCTIONS DECLARED BY HAND, LINKED DIRECTLY AGAINST opengl32.lib
// (GODS_LAWS.md L-07, D-W6b's own "aparato Mesa do servidor"): unlike
// the Wayland side (which resolves glGetString/glGetIntegerv through
// eglGetProcAddress because this project never links -lGL at all),
// glGetString and glGetIntegerv are GL 1.0/1.1 core functions Windows'
// own opengl32.dll exports DIRECTLY - the same fixed-address category
// tests/win32_runner_probe_test.cpp's own header comment already
// documents for glGetString, and src/platform/win32/CMakeLists.txt's
// own sibling comment for user32 already establishes as the house
// convention for a Win32 system library ("Win32 counts as system API
// for the dependency-zero rule, GODS_LAWS.md L-07"). Declared here by
// hand rather than through a vendored <GL/gl.h> (this project
// deliberately never includes one, src/render/gl_abi.hpp's own header
// comment), sourced against the SAME vendored registry gl_abi.hpp is
// itself measured against (third_party/khronos/gl.xml: GL_RENDERER
// 0x1F01, GL_MAJOR_VERSION 0x821B, GL_MINOR_VERSION 0x821C, GL_CONTEXT_
// PROFILE_MASK 0x9126 - the SAME four constants egl_context_adapter.cpp
// already declares this same way one directory over).
//
// wglChoosePixelFormatARB/wglCreateContextAttribsARB/wglSwapIntervalEXT
// have NO fixed address (extensions, resolved only through wgl_
// extension_loader.hpp's own disposable-window dance, this fatia) -
// this is why D-W6a-18's own two-tier split (fixed vs. extension)
// matters: glGetString/glGetIntegerv below need no loader at all.
//
// WGL_ARB_pixel_format / WGL_ARB_multisample / WGL_ARB_framebuffer_sRGB
// / WGL_ARB_create_context(_profile) token values below are copied from
// their own published Khronos registry token tables (GODS_LAWS.md
// L-29: a specification's PUBLIC token values are learned, not
// "plagiarized" - no implementation code is copied, only integer
// constants a program needs to speak the extension), the SAME
// technique tests/win32_runner_probe_test.cpp's own header comment
// already documents and cites for WGL_CONTEXT_MAJOR_VERSION_ARB and
// its siblings:
//   https://registry.khronos.org/OpenGL/extensions/ARB/WGL_ARB_pixel_format.txt
//   https://registry.khronos.org/OpenGL/extensions/ARB/WGL_ARB_multisample.txt
//   https://registry.khronos.org/OpenGL/extensions/ARB/WGL_ARB_framebuffer_sRGB.txt
//   https://registry.khronos.org/OpenGL/extensions/ARB/WGL_ARB_create_context.txt
//   https://registry.khronos.org/OpenGL/extensions/ARB/WGL_ARB_create_context_profile.txt
//   https://registry.khronos.org/OpenGL/extensions/EXT/WGL_EXT_swap_control_tear.txt

namespace glintfx::platform {

namespace {

using gl_enum = unsigned int;
using gl_ubyte = unsigned char;
using gl_int = int;

constexpr gl_enum k_gl_renderer = 0x1F01;
constexpr gl_enum k_gl_major_version = 0x821B;
constexpr gl_enum k_gl_minor_version = 0x821C;
constexpr gl_enum k_gl_context_profile_mask = 0x9126;

using gl_get_string_fn = const gl_ubyte *(WINAPI *)(gl_enum);
using gl_get_integerv_fn = void(WINAPI *)(gl_enum, gl_int *);

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
// WGL_ARB_framebuffer_sRGB.
constexpr int k_wgl_framebuffer_srgb_capable_arb = 0x20A9;
// WGL_ARB_create_context / WGL_ARB_create_context_profile - same
// values tests/win32_runner_probe_test.cpp already cites.
constexpr int k_wgl_context_major_version_arb = 0x2091;
constexpr int k_wgl_context_minor_version_arb = 0x2092;
constexpr int k_wgl_context_profile_mask_arb = 0x9126;

using wgl_choose_pixel_format_arb_fn = BOOL(WINAPI *)(HDC, const int *, const FLOAT *, UINT, int *,
                                                      UINT *);
using wgl_create_context_attribs_arb_fn = HGLRC(WINAPI *)(HDC, HGLRC, const int *);
using wgl_swap_interval_ext_fn = BOOL(WINAPI *)(int);
using wgl_get_extensions_string_arb_fn = const char *(WINAPI *)(HDC);

} // namespace

// glGetString/glGetIntegerv - GL 1.0/1.1 core, exported DIRECTLY by
// opengl32.dll (this file's own header comment) - declared at
// namespace scope, outside the anonymous namespace above, the same
// "extern "C" language linkage belongs at ordinary namespace scope"
// reasoning tests/win32_runner_probe_test.cpp's own header comment
// already gives for its own identical declaration of glGetString.
extern "C" const gl_ubyte *WINAPI glGetString(gl_enum name);
extern "C" void WINAPI glGetIntegerv(gl_enum pname, gl_int *params);

win32_gl_context_adapter::~win32_gl_context_adapter() { close(); }

gltfx_rslt<void> win32_gl_context_adapter::set_pixel_format_once(
    void *choose_pixel_format_arb, std::span<const gltfx_gfx_option_entry> options) noexcept {
    const auto choose = reinterpret_cast<wgl_choose_pixel_format_arb_fn>(choose_pixel_format_arb);

    std::int64_t msaa_samples = 0;
    std::int64_t srgb_framebuffer = 0;
    for (const gltfx_gfx_option_entry &entry : options) {
        if (entry.id == gltfx_gfx_option::msaa_samples) {
            msaa_samples = entry.value;
        } else if (entry.id == gltfx_gfx_option::srgb_framebuffer) {
            srgb_framebuffer = entry.value;
        }
    }

    // RGBA8+stencil8, no depth (D-W6b-4), same shape egl_context_
    // adapter.cpp's own choose_config() already builds one directory
    // over: a fixed-size array, never a heap allocation, appended to
    // only when msaa/srgb were actually requested.
    auto try_choose = [this](std::int64_t samples, bool srgb, wgl_choose_pixel_format_arb_fn fn,
                             int &format_out) noexcept -> bool {
        int attribs[24] = {
            k_wgl_draw_to_window_arb, TRUE, k_wgl_support_opengl_arb, TRUE,
            k_wgl_double_buffer_arb,  TRUE, k_wgl_pixel_type_arb,     k_wgl_type_rgba_arb,
            k_wgl_color_bits_arb,     32,   k_wgl_stencil_bits_arb,   8,
            k_wgl_depth_bits_arb,     0,
        };
        std::size_t next = 14;
        if (samples > 0) {
            attribs[next++] = k_wgl_sample_buffers_arb;
            attribs[next++] = 1;
            attribs[next++] = k_wgl_samples_arb;
            attribs[next++] = static_cast<int>(samples);
        }
        if (srgb) {
            attribs[next++] = k_wgl_framebuffer_srgb_capable_arb;
            attribs[next++] = TRUE;
        }
        attribs[next] = 0;

        int format = 0;
        UINT num_formats = 0;
        const bool ok =
            fn(m_dc, attribs, nullptr, 1, &format, &num_formats) != 0 && num_formats > 0;
        if (ok) {
            format_out = format;
        }
        return ok;
    };

    int chosen_format = 0;
    bool msaa_ok = false;
    bool srgb_ok = false;

    if (try_choose(msaa_samples, srgb_framebuffer != 0, choose, chosen_format)) {
        msaa_ok = true;
        srgb_ok = true;
    } else if (msaa_samples > 0 && try_choose(0, srgb_framebuffer != 0, choose, chosen_format)) {
        // D-W6b-17: never degrade in silence - isolate WHICH option
        // this driver could not honor before refusing, one attempt at
        // a time, same discipline egl_context_adapter.cpp's own
        // choose_config() already applies.
        msaa_ok = false;
        srgb_ok = true;
    } else if (srgb_framebuffer != 0 && try_choose(msaa_samples, false, choose, chosen_format)) {
        msaa_ok = true;
        srgb_ok = false;
    } else if (msaa_samples > 0 || srgb_framebuffer != 0) {
        m_msaa_supported = false;
        m_srgb_supported = false;
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::unsupported)
                .with_rejected_value(msaa_samples > 0 ? "msaa_samples" : "srgb_framebuffer"));
    } else if (!try_choose(0, false, choose, chosen_format)) {
        return gltfx_rslt<void>::err(gltfx_err(gltfx_err_code::platform_failure)
                                         .with_rejected_value("wgl_pixel_format")
                                         .with_os_error_code(::GetLastError()));
    }

    m_msaa_supported = msaa_ok;
    m_srgb_supported = srgb_ok;

    // SetPixelFormat may be called only ONCE per window (D-W6b-4,
    // learn.microsoft.com/windows/win32/api/wingdi/nf-wingdi-
    // setpixelformat) - DescribePixelFormat's own documentation is the
    // standard way to obtain a valid PIXELFORMATDESCRIPTOR for an
    // ARB-chosen format, rather than fabricating one by hand.
    PIXELFORMATDESCRIPTOR pfd{};
    if (::DescribePixelFormat(m_dc, chosen_format, sizeof(PIXELFORMATDESCRIPTOR), &pfd) == 0) {
        return gltfx_rslt<void>::err(gltfx_err(gltfx_err_code::platform_failure)
                                         .with_rejected_value("wgl_pixel_format")
                                         .with_os_error_code(::GetLastError()));
    }
    if (::SetPixelFormat(m_dc, chosen_format, &pfd) == 0) {
        return gltfx_rslt<void>::err(gltfx_err(gltfx_err_code::platform_failure)
                                         .with_rejected_value("wgl_pixel_format")
                                         .with_os_error_code(::GetLastError()));
    }
    return gltfx_rslt<void>::ok();
}

gltfx_rslt<void>
win32_gl_context_adapter::create_context(void *create_context_attribs_arb) noexcept {
    const auto create =
        reinterpret_cast<wgl_create_context_attribs_arb_fn>(create_context_attribs_arb);

    const int context_attribs[] = {
        k_wgl_context_major_version_arb,
        3,
        k_wgl_context_minor_version_arb,
        3,
        k_wgl_context_profile_mask_arb,
        static_cast<int>(k_gl_context_core_profile_bit),
        0,
    };
    HGLRC context = create(m_dc, nullptr, context_attribs);
    if (context == nullptr) {
        return gltfx_rslt<void>::err(gltfx_err(gltfx_err_code::platform_failure)
                                         .with_rejected_value("wgl_context")
                                         .with_os_error_code(::GetLastError()));
    }
    m_context = context;

    if (::wglMakeCurrent(m_dc, m_context) == 0) {
        return gltfx_rslt<void>::err(gltfx_err(gltfx_err_code::platform_failure)
                                         .with_rejected_value("wgl_make_current")
                                         .with_os_error_code(::GetLastError()));
    }

    gl_int major = 0;
    gl_int minor = 0;
    gl_int profile_mask = 0;
    glGetIntegerv(k_gl_major_version, &major);
    glGetIntegerv(k_gl_minor_version, &minor);
    glGetIntegerv(k_gl_context_profile_mask, &profile_mask);

    if (const gltfx_rslt<void> version_ok = validate_gl_context_version(
            static_cast<std::uint32_t>(major), static_cast<std::uint32_t>(minor),
            static_cast<std::uint32_t>(profile_mask));
        version_ok.has_error()) {
        return version_ok;
    }

    const gl_ubyte *renderer = glGetString(k_gl_renderer);
    m_gpu.learn(gltfx_gpu_kind::unknown, renderer != nullptr
                                             ? reinterpret_cast<const char *>(renderer)
                                             : std::string_view{});
    return gltfx_rslt<void>::ok();
}

void win32_gl_context_adapter::detect_adaptive_vsync_support() noexcept {
    // D-W6b-18: resolved through the SAME resolve_wgl_proc_address()
    // atom proc_address() below hands a consumer - not a fourth pointer
    // added to wgl_extension_loader.hpp's own three (that atom's own
    // header comment: only the two functions the REAL window's pixel
    // format/context creation genuinely needs, plus swap-interval).
    const auto get_extensions = reinterpret_cast<wgl_get_extensions_string_arb_fn>(
        resolve_wgl_proc_address("wglGetExtensionsStringARB"));
    if (get_extensions == nullptr) {
        return;
    }
    const char *extensions = get_extensions(m_dc);
    if (extensions == nullptr) {
        return;
    }
    m_adaptive_supported =
        std::string_view(extensions).find("WGL_EXT_swap_control_tear") != std::string_view::npos;
}

gltfx_rslt<void>
win32_gl_context_adapter::open(win32_window_adapter &window,
                               std::span<const gltfx_gfx_option_entry> options) noexcept {
    if (!window.is_open()) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value("window"));
    }
    HWND hwnd = window.native_handle();
    if (hwnd == nullptr) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value("window"));
    }

    const gltfx_rslt<wgl_extension_pointers> loaded = load_wgl_extension_pointers();
    if (loaded.has_error()) {
        return gltfx_rslt<void>::err(loaded.error());
    }
    m_swap_interval_ext = loaded.value().swap_interval_ext;

    m_dc = ::GetDC(hwnd);
    if (m_dc == nullptr) {
        close();
        return gltfx_rslt<void>::err(gltfx_err(gltfx_err_code::platform_failure)
                                         .with_rejected_value("device_context")
                                         .with_os_error_code(::GetLastError()));
    }
    m_window = hwnd;

    if (const gltfx_rslt<void> pixel_format_ok =
            set_pixel_format_once(loaded.value().choose_pixel_format_arb, options);
        pixel_format_ok.has_error()) {
        close();
        return pixel_format_ok;
    }

    if (const gltfx_rslt<void> context_ok =
            create_context(loaded.value().create_context_attribs_arb);
        context_ok.has_error()) {
        close();
        return context_ok;
    }

    detect_adaptive_vsync_support();

    // vsync is `live` (gfx_option.hpp), but the opening list may still
    // carry it (D-W6b-18's own default, resolved by gl_context_facade.
    // cpp's own resolve_full_option_table() into EVERY call's `options`
    // span, requested-or-default) - applying it here is what makes
    // wglSwapIntervalEXT actually get called at least once per open(),
    // the SAME "mechanism differs, effect equal" contract D-W6b-6/18
    // promise a consumer.
    for (const gltfx_gfx_option_entry &entry : options) {
        if (entry.id == gltfx_gfx_option::vsync) {
            if (const gltfx_rslt<void> vsync_ok = apply_option(entry); vsync_ok.has_error()) {
                close();
                return vsync_ok;
            }
            break;
        }
    }

    return gltfx_rslt<void>::ok();
}

void win32_gl_context_adapter::close() noexcept {
    if (m_context != nullptr) {
        ::wglMakeCurrent(nullptr, nullptr);
        ::wglDeleteContext(m_context);
        m_context = nullptr;
    }
    if (m_dc != nullptr && m_window != nullptr) {
        ::ReleaseDC(m_window, m_dc);
    }
    m_dc = nullptr;
    m_window = nullptr;
    m_swap_interval_ext = nullptr;
    m_gpu = gpu_kind_state{};
    m_swap_calls_issued = 0;
    m_msaa_supported = false;
    m_srgb_supported = false;
    m_adaptive_supported = false;
}

gltfx_rslt<void> win32_gl_context_adapter::make_current() noexcept {
    if (::wglMakeCurrent(m_dc, m_context) == 0) {
        return gltfx_rslt<void>::err(gltfx_err(gltfx_err_code::platform_failure)
                                         .with_rejected_value("wgl_make_current")
                                         .with_os_error_code(::GetLastError()));
    }
    return gltfx_rslt<void>::ok();
}

gltfx_rslt<gltfx_present_outcome> win32_gl_context_adapter::swap_buffers() noexcept {
    // D-W6b-6's own Windows mechanism: IsIconic() in place of the
    // Wayland side's own frame-callback budget - checked UNCONDITIONALLY
    // (never gated on the current vsync value, unlike Wayland where
    // vsync=off skips the wait entirely): a minimized top-level window
    // costs CPU to paint into for no observer on this platform
    // regardless of v-sync, the busca's own finding (sec. 0, "Janela
    // minimizada no Windows: SwapBuffers nao bloqueia... mas desenhar
    // para janela minimizada e CPU jogado fora").
    if (::IsIconic(m_window) != 0) {
        return gltfx_rslt<gltfx_present_outcome>::ok(gltfx_present_outcome::skipped_hidden);
    }
    if (::SwapBuffers(m_dc) == 0) {
        // with_os_error_code(::GetLastError()) - THE GEMEO this file
        // itself was missing (GODS_LAWS.md L-17/CLAUDE.md "ao corrigir,
        // procurar o gemeo"): every OTHER win32/ adapter that returns
        // platform_failure from a failed Win32 call already attaches
        // ::GetLastError() this same way (display_adapter.cpp,
        // window_adapter.cpp, seat_adapter.cpp, app_user_model_id.cpp)
        // - this file's nine platform_failure sites, swap_buffers()
        // included, were the one place in src/platform/win32/ that did
        // not, leaving a caller (and this file's own iconic_present
        // test) with no way to see WHY a real SwapBuffers() call
        // failed on the real Windows runner, only THAT it did.
        return gltfx_rslt<gltfx_present_outcome>::err(gltfx_err(gltfx_err_code::platform_failure)
                                                          .with_rejected_value("swap_buffers")
                                                          .with_os_error_code(::GetLastError()));
    }
    ++m_swap_calls_issued;
    return gltfx_rslt<gltfx_present_outcome>::ok(gltfx_present_outcome::presented);
}

void *win32_gl_context_adapter::proc_address(std::string_view name) const noexcept {
    return resolve_wgl_proc_address(name);
}

gltfx_rslt<void> win32_gl_context_adapter::call_swap_interval(int interval) noexcept {
    if (m_swap_interval_ext == nullptr) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::unsupported).with_rejected_value("vsync"));
    }
    const auto swap_interval = reinterpret_cast<wgl_swap_interval_ext_fn>(m_swap_interval_ext);
    if (swap_interval(interval) == 0) {
        return gltfx_rslt<void>::err(gltfx_err(gltfx_err_code::platform_failure)
                                         .with_rejected_value("wgl_swap_interval")
                                         .with_os_error_code(::GetLastError()));
    }
    return gltfx_rslt<void>::ok();
}

gltfx_rslt<void> win32_gl_context_adapter::apply_option(gltfx_gfx_option_entry entry) noexcept {
    if (entry.id == gltfx_gfx_option::vsync) {
        if (entry.value == 2) { // adaptive - D-W6b-18
            if (!m_adaptive_supported) {
                return gltfx_rslt<void>::err(
                    gltfx_err(gltfx_err_code::unsupported).with_rejected_value("vsync"));
            }
            return call_swap_interval(-1);
        }
        return call_swap_interval(entry.value != 0 ? 1 : 0);
    }

    // Every other `live` id (frame_rate_cap, preset) is accepted here
    // with no adapter-side effect yet - see this class's own header
    // comment on apply_option() for why that is honest, not a gap.
    return gltfx_rslt<void>::ok();
}

gltfx_gfx_option_support
win32_gl_context_adapter::option_support(gltfx_gfx_option id) const noexcept {
    switch (id) {
    case gltfx_gfx_option::vsync:
        // D-W6b-16's own "nunca degrada em silencio", applied at the
        // whole-option level: a driver with no WGL_EXT_swap_control at
        // all (wgl_extension_loader.hpp's own struct comment) reports
        // unsupported_here rather than silently accepting a request it
        // can never honor.
        return m_swap_interval_ext != nullptr ? gltfx_gfx_option_support::supported
                                              : gltfx_gfx_option_support::unsupported_here;
    case gltfx_gfx_option::frame_rate_cap:
    case gltfx_gfx_option::gpu_preference:
    case gltfx_gfx_option::preset:
        return gltfx_gfx_option_support::supported;
    case gltfx_gfx_option::msaa_samples:
        return m_msaa_supported ? gltfx_gfx_option_support::supported
                                : gltfx_gfx_option_support::unsupported_here;
    case gltfx_gfx_option::srgb_framebuffer:
        return m_srgb_supported ? gltfx_gfx_option_support::supported
                                : gltfx_gfx_option_support::unsupported_here;
    case gltfx_gfx_option::auto_choice_reason:
    case gltfx_gfx_option::power_source:
        return gltfx_gfx_option_support::read_only_here;
    }
    return gltfx_gfx_option_support::unsupported_here;
}

} // namespace glintfx::platform

#endif // defined(_WIN32)
