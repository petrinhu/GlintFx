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

#include <cstdint>
#include <span>
#include <string_view>

#include <glintfx/core/err.hpp>
#include <glintfx/platform/gl/context.hpp>
#include <glintfx/platform/gl/gfx_option.hpp>
#include <glintfx/platform/gl/gpu.hpp>

#include "platform/gl/gpu_kind_state.hpp"

// platform/win32/wgl_context_adapter.hpp - X-WGL (docs/plano-w6b-
// placa-e-laco.md fatia 4, D-W6b-1/4/6/7/17/18, GODS_LAWS.md L-04/L-07/
// L-17/L-19/L-31): the Windows mirror of W-EGL's own wayland_egl_
// context_adapter (src/platform/wayland/egl_context_adapter.hpp,
// fatia 3), correspondence by correspondence (team-lead briefing,
// 06/09/2026): create_egl_display <-> a legacy context plus the three
// ARB/EXT extension pointers (wgl_extension_loader.hpp, this fatia);
// choose_config <-> set_pixel_format_once() below (wglChoosePixel
// FormatARB); create_context <-> create_context() below (wglCreate
// ContextAttribsARB, 3.3 core, validated the SAME way by gl_version_
// policy.hpp, fatia 2b); swap_buffers <-> IsIconic() in place of the
// Wayland side's own frame-callback budget (D-W6b-6: "mecanismo
// diferente, efeito igual" - no compositor to negotiate a frame
// callback with on this platform, so a minimized top-level window is
// the ONE signal this system gives for "not currently being
// repainted").
//
// win32_gl_context_adapter, checked against platform::gl_context_
// adapter_port by selected_gl_context_adapter_check.cpp (this SAME
// fatia). gl_context_facade.cpp (fatia 2b, already frozen) is the ONLY
// caller: it resolves the FULL options table BEFORE this adapter's own
// open() is ever called (D-W6b-25's own fixation-before-support
// ordering), so every gltfx_gfx_option_entry this class ever sees is
// already shape-checked - the SAME precondition egl_context_adapter.
// hpp's own class comment already documents one directory over.
//
// win32_window_adapter FORWARD-DECLARED, its own header NOT included
// here (the same "a pointer/reference is all this header needs"
// reasoning egl_context_adapter.hpp already gives for wayland_window_
// adapter): open() below takes a `win32_window_adapter &` and only
// ever calls its own already-public is_open()/native_handle(), never
// anything requiring the complete type in THIS header.
//
// HDC/HGLRC ARE REAL WINDOWS TYPES HERE, NOT VOID* (unlike EGLDisplay/
// EGLContext/EGLSurface in egl_context_adapter.hpp): both are already
// ordinary typedefs windows.h itself declares (`typedef HANDLE__ *
// HDC;`, `typedef HGLRC__ *HGLRC;`) - the SAME "typedef, not a tag
// type, so forward-declaring it would mean re-declaring it exactly"
// reasoning win32/display_adapter.hpp's own header comment already
// gives for HWND/ATOM one directory over, and this file already
// includes <windows.h> for HWND itself (open()'s own parameter reaches
// it through win32_window_adapter::native_handle()).
namespace glintfx::platform {

class win32_window_adapter;

// D-W6b-18's own Windows-side mechanism: `adaptive` v-sync is honored
// through wglSwapIntervalEXT(-1) ONLY when this system's own WGL_EXT_
// swap_control_tear is present (detect_adaptive_vsync_support() below,
// checked once at open() time via wglGetExtensionsStringARB) - refused
// BY NAME (gltfx_err_code::unsupported, rejected_value() == "vsync")
// otherwise, never silently downgraded to plain `on` (the exact
// mistake this project's own D-W6b-18 comment names, and the mirror of
// the Wayland side's own unconditional refusal - Wayland/EGL has no
// equivalent extension at all, this system MIGHT).
class win32_gl_context_adapter {
  public:
    win32_gl_context_adapter() noexcept = default;

    // PINNED, NEVER MOVABLE (FACADE-PIN, docs/plano-conserto-fachadas-
    // uaf.md sec. 3/6/7.2, varredura #wgl) - a live adapter owns a real
    // device context and rendering context, same reasoning as before.
    // This class registers no `this`-derived pointer with the OS at all
    // (grep for lpParam/CreateWindowExW/GWLP_USERDATA against this
    // file's own .cpp comes back empty, this plan's own sec. 3 #wgl) -
    // moving it was never unsafe. It is pinned anyway, by the SAME
    // uniform rule every other port-selected adapter in this fatia
    // follows (D-UAF-2: "regra uniforme, sem julgamento caso a caso -
    // julgamento e onde se erra", the reasoning the leader gave for
    // L-04 on 02/09/2026): gl_context_adapter_port now requires
    // pinned_adapter<A> for every adapter it selects, not only the ones
    // measured unsafe today.
    win32_gl_context_adapter(const win32_gl_context_adapter &) = delete;
    win32_gl_context_adapter &operator=(const win32_gl_context_adapter &) = delete;
    win32_gl_context_adapter(win32_gl_context_adapter &&) = delete;
    win32_gl_context_adapter &operator=(win32_gl_context_adapter &&) = delete;

    ~win32_gl_context_adapter();

    // DELIBERATELY NOT PART OF gl_context_adapter_port (src/platform/
    // port/gl_context_adapter_port.hpp's own header comment, "porta
    // gorda"/"open() excluido"): the concrete parameter type is Win32-
    // specific. `window` MUST already be open() (checked one layer up
    // by gl_context_facade.cpp, the same precondition egl_context_
    // adapter.hpp's own open() documents for the Wayland side); `options`
    // is the FULLY RESOLVED table gl_context_facade.cpp's own resolve_
    // full_option_table() already produced.
    [[nodiscard]] gltfx_rslt<void> open(win32_window_adapter &window,
                                        std::span<const gltfx_gfx_option_entry> options) noexcept;

    // Idempotent-safe: destroys whatever this adapter still owns, in
    // reverse order of creation. Safe to call on a moved-from or
    // never-opened instance.
    void close() noexcept;

    [[nodiscard]] bool is_open() const noexcept { return m_context != nullptr; }

    [[nodiscard]] gltfx_rslt<void> make_current() noexcept;

    // D-W6b-6's own contract (context.hpp's own header comment, item
    // 2): NEVER blocks indefinitely. IsIconic() is the ONE signal this
    // platform gives for "not currently being repainted" - no frame
    // callback to negotiate, no budget to wait out, unlike the Wayland
    // side one directory over.
    [[nodiscard]] gltfx_rslt<gltfx_present_outcome> swap_buffers() noexcept;

    // present_would_skip() - LOOP-RUN fatia 7 (docs/plano-w6b-fatias-
    // 6-8.md, D-W6b-46, gl_context_adapter_port.hpp's own header
    // comment has the full contract): IsIconic(m_window) != 0 - the
    // SAME signal swap_buffers() above already checks, exposed as its
    // own atom (one call, two uses: gltfx_loop's own oculto-wait sonda,
    // AND swap_buffers() itself below) rather than two adapters
    // answering "would this frame be skipped" through two different
    // mechanisms.
    [[nodiscard]] bool present_would_skip() const noexcept { return ::IsIconic(m_window) != 0; }

    [[nodiscard]] void *proc_address(std::string_view name) const noexcept;

    // Only ever called with a `live` entry (gl_context_facade.cpp's
    // own set_option() already refused an open_only/read_only entry
    // before reaching here) - `vsync` is the one id this adapter acts
    // on directly (D-W6b-18); every other live id (frame_rate_cap,
    // preset) is accepted here with no adapter-side effect yet, the
    // SAME "the fatia that gives an id real behavior teaches ITS OWN
    // layer to act on it" reasoning egl_context_adapter.hpp's own class
    // comment already documents.
    [[nodiscard]] gltfx_rslt<void> apply_option(gltfx_gfx_option_entry entry) noexcept;

    [[nodiscard]] gltfx_gfx_option_support option_support(gltfx_gfx_option id) const noexcept;

    // sec. 10.3's own "minimo honesto" (D-W6b-13): `name` is always
    // GL_RENDERER once a context is current; `kind` stays `unknown`
    // until fatia 5b's own dxgi_gpu_kind.hpp teaches this adapter how
    // to classify a REAL driver answer - this fatia never guesses.
    [[nodiscard]] gltfx_gpu_info gpu() const noexcept { return m_gpu.read(); }

    // Test seam (win32_iconic_present_test, this fatia): how many times
    // this adapter's own swap_buffers() actually called ::SwapBuffers()
    // - never read by production code. A minimized window's own
    // swap_buffers() call must leave this counter UNCHANGED (D-W6b-6:
    // `skipped_hidden` never touches the real GDI call), the one fact
    // no return-value assertion alone could distinguish from "SwapBuffers
    // was called and coincidentally reported success".
    [[nodiscard]] std::uint32_t swap_calls_issued() const noexcept { return m_swap_calls_issued; }

  private:
    [[nodiscard]] gltfx_rslt<void>
    set_pixel_format_once(void *choose_pixel_format_arb,
                          std::span<const gltfx_gfx_option_entry> options) noexcept;
    [[nodiscard]] gltfx_rslt<void> create_context(void *create_context_attribs_arb) noexcept;
    void detect_adaptive_vsync_support() noexcept;
    [[nodiscard]] gltfx_rslt<void> call_swap_interval(int interval) noexcept;

    HDC m_dc = nullptr;
    HGLRC m_context = nullptr;
    HWND m_window = nullptr; // borrowed from the window - never owned

    // BOOL(WINAPI*)(int) - resolved once by wgl_extension_loader.hpp at
    // open() time; may legitimately stay nullptr (that header's own
    // struct comment) on a driver with no WGL_EXT_swap_control at all.
    void *m_swap_interval_ext = nullptr;

    gpu_kind_state m_gpu;

    std::uint32_t m_swap_calls_issued = 0;

    bool m_msaa_supported = false;
    bool m_srgb_supported = false;
    // D-W6b-18: whether THIS system's own WGL_EXT_swap_control_tear is
    // present - detected once, at open() time, never guessed.
    bool m_adaptive_supported = false;
};

} // namespace glintfx::platform

#endif // defined(_WIN32)
