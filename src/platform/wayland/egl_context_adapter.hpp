// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include <glintfx/core/err.hpp>
#include <glintfx/platform/gl/context.hpp>
#include <glintfx/platform/gl/gfx_option.hpp>
#include <glintfx/platform/gl/gpu.hpp>

#include "platform/gl/gpu_kind_state.hpp"
#include "platform/wayland/frame_callback_sequence.hpp"

// platform/wayland/egl_context_adapter.hpp - W-EGL (docs/plano-w6b-
// placa-e-laco.md fatia 3, D-W6b-1/4/5/6/7/17/18, GODS_LAWS.md L-04/
// L-07/L-17/L-19/L-31): the ONE type that turns an already-open
// wayland_window_adapter's own wl_surface (window_adapter.hpp, WL-
// WINDOW fatia W-E) into a real, current-able OpenGL 3.3 core context -
// wayland_egl_context_adapter, checked against platform::gl_context_
// adapter_port by selected_gl_context_adapter_check.cpp, this SAME
// fatia. gl_context_facade.cpp (fatia 2b, already frozen) is the ONLY
// caller: it resolves the FULL options table BEFORE this adapter's own
// open() is ever called (D-W6b-25's own fixation-before-support
// ordering), so every gltfx_gfx_option_entry this class ever sees is
// already shape-checked.
//
// wl_surface/wl_egl_window/wl_callback FORWARD-DECLARED, <wayland-
// client.h>/<wayland-egl.h> NOT included here (the SAME reasoning
// window_adapter.hpp/display_adapter.hpp already document one file
// over): a pointer is all this header needs. EGLDisplay/EGLContext/
// EGLSurface are spelled `void *` rather than pulling in <EGL/egl.h> -
// Khronos's own <EGL/eglplatform.h> types all three as a bare `void *`
// typedef, so this is not a lossy simplification, only a header this
// internal type never needs to include (the real EGL calls live
// entirely in egl_context_adapter.cpp, where the real headers are).
struct wl_surface;
struct wl_egl_window;
struct wl_callback;

namespace glintfx::platform {

class wayland_window_adapter;

// The three DEDICATED atoms this adapter's own open() runs, in order
// (this file's own header comment names each step's failure mode -
// egl_context_adapter.cpp is where every one of these actually calls
// into EGL/libwayland-egl):
//
//   1. create_egl_display() - eglGetPlatformDisplay(EGL_PLATFORM_
//      WAYLAND_KHR, ...) over the wl_display this window's own
//      wl_surface already belongs to (wl_proxy_get_display() - this
//      class never needs a separate wayland_display_adapter reference,
//      the SAME reason gl_context_facade.cpp's own open() call only
//      ever hands this adapter the window, docs/plano-w6b-placa-e-
//      laco.md fatia 3's own F2 fact), then eglInitialize()/
//      eglBindAPI(EGL_OPENGL_API).
//   2. choose_config() - reads `msaa_samples`/`srgb_framebuffer`
//      (open_only, gfx_option.hpp) from the already-resolved options
//      span open() receives; RGBA8+stencil8 always (D-W6b-4). A config
//      this driver cannot produce REFUSES naming the option that could
//      not be honored (gltfx_err_code::unsupported, D-W6b-16's "nunca
//      degrada em silencio") - never silently drops the request.
//   3. create_context() - EGL_CONTEXT_MAJOR/MINOR_VERSION 3/3,
//      EGL_CONTEXT_OPENGL_PROFILE_MASK core-only (D-W6b-4: "sem
//      forward-compatible"), then validate_gl_context_version() (gl_
//      version_policy.hpp, fatia 2b) against what the driver actually
//      granted - never trusting the REQUEST as proof of the RESULT.
//
// create_egl_window() is its OWN step, not folded into create_
// context(): wl_egl_window_create() needs the window's own pixel_
// size() (never logical_size() - the same D-W6a-17 distinction this
// header's own top comment already cites), which is unrelated to
// negotiating a GL version.
//
// attach_frame_listener() arms the FIRST wl_surface.frame callback
// (frame_callback_sequence.hpp, this fatia) - swap_buffers() below is
// what re-arms it after every subsequent presented frame.
//
// close() TEARS DOWN IN REVERSE ORDER (this fatia's own read of tests/
// container/egl_probe_smoke.cpp's own header comment, the fatia-1
// sonda this fatia's own briefing named as "o mapa do caminho que
// funciona, medido"): unbind+destroy the EGL context, destroy the EGL
// surface, eglTerminate() the EGL display, THEN wl_egl_window_destroy()
// - egl_probe_smoke.cpp's own header comment documents the real SIGSEGV
// a reviewer found from getting this order wrong (eglTerminate() after
// the underlying wl_display connection was already gone). This class
// never owns that connection itself (window MUST outlive the context,
// context.hpp's own precondition) - the risk that comment describes
// does not apply HERE, but the teardown ORDER it establishes as EGL's
// own documented contract still does, and this class follows it.
class wayland_egl_context_adapter {
  public:
    wayland_egl_context_adapter() noexcept = default;

    // PINNED, NEVER MOVABLE (FACADE-PIN, docs/plano-conserto-fachadas-
    // uaf.md sec. 3/6/7.2, varredura #7) - a live adapter owns a real
    // EGL display/context/surface and a wl_egl_window, same reasoning
    // as before. Moving used to be allowed via std::exchange() on every
    // pointer, INCLUDING m_pending_frame_callback - safe ONLY because
    // attach_frame_listener() (registers `this` with wl_callback_add_
    // listener) is called exclusively from swap_buffers(), always AFTER
    // gl_context_facade.cpp's own move into the heap, never from
    // open(). That safety was an accident of call order, not of
    // design (this plan's own D1: "a opcao B mantem exatamente essa
    // dependencia e a esconde melhor") - deleting the move closes the
    // class of defect by construction, uniform with every other adapter
    // in this fatia, so a future refactor that arms the frame callback
    // from open() cannot silently reintroduce the crash.
    wayland_egl_context_adapter(const wayland_egl_context_adapter &) = delete;
    wayland_egl_context_adapter &operator=(const wayland_egl_context_adapter &) = delete;
    wayland_egl_context_adapter(wayland_egl_context_adapter &&) = delete;
    wayland_egl_context_adapter &operator=(wayland_egl_context_adapter &&) = delete;

    ~wayland_egl_context_adapter();

    // DELIBERATELY NOT PART OF gl_context_adapter_port (src/platform/
    // port/gl_context_adapter_port.hpp's own header comment, "porta
    // gorda"/"open() exclu­ido"): the concrete parameter type is
    // Wayland-specific. `window` MUST already be open() (gl_context_
    // facade.cpp's own precondition, checked one layer up); `options`
    // is the FULLY RESOLVED table gl_context_facade.cpp's own
    // resolve_full_option_table() already produced - every row of
    // gfx_option_registry.hpp's own table, in id order, one entry
    // each, never a partial list this adapter has to fill defaults
    // into itself.
    [[nodiscard]] gltfx_rslt<void> open(wayland_window_adapter &window,
                                        std::span<const gltfx_gfx_option_entry> options) noexcept;

    // Idempotent-safe: destroys whatever this adapter still owns, in
    // the reverse order this file's own top comment names. Safe to
    // call on a moved-from or never-opened instance.
    void close() noexcept;

    [[nodiscard]] bool is_open() const noexcept { return m_egl_context != nullptr; }

    [[nodiscard]] gltfx_rslt<void> make_current() noexcept;

    // D-W6b-6's own contract (context.hpp's own header comment, item
    // 2): NEVER blocks indefinitely. Drives frame_callback_sequence.hpp
    // (this fatia) for the budgeted wait, and gl_surface_size_policy.hpp
    // (fatia 2b) to decide whether wl_egl_window_resize() is due before
    // this frame ever reaches eglSwapBuffers().
    [[nodiscard]] gltfx_rslt<gltfx_present_outcome> swap_buffers() noexcept;

    [[nodiscard]] void *proc_address(std::string_view name) const noexcept;

    // Only ever called with a `live` entry (gl_context_facade.cpp's
    // own set_option() already refused an open_only/read_only entry
    // before reaching here, gfx_option_validation.hpp) - `vsync` is
    // the one id this adapter acts on directly (D-W6b-18): `adaptive`
    // has no Wayland/EGL equivalent (this fatia's own busca, docs/
    // plano-w6b-placa-e-laco.md sec. 0) and is refused BY NAME, never
    // silently downgraded to `on`. Every other live id (frame_rate_cap,
    // preset, and so on) is accepted here with no adapter-side effect
    // yet - gl_context_facade.cpp's own current_values already stores
    // whatever value a consumer set, generically, for option() to read
    // back; the fatia that gives an id real behavior (LOOP-RUN for
    // frame_rate_cap, G-PRESET for preset) teaches ITS OWN layer to act
    // on it, never retrofits this adapter out of turn.
    [[nodiscard]] gltfx_rslt<void> apply_option(gltfx_gfx_option_entry entry) noexcept;

    [[nodiscard]] gltfx_gfx_option_support option_support(gltfx_gfx_option id) const noexcept;

    // sec. 10.3's own "minimo honesto" (D-W6b-13): `name` is always
    // GL_RENDERER once a context is current; `kind` stays `unknown`
    // until fatia 5b's own drm_gpu_kind.hpp teaches this adapter how to
    // classify a REAL driver answer - this fatia never guesses.
    [[nodiscard]] gltfx_gpu_info gpu() const noexcept { return m_gpu.read(); }

    // FACADE-PIN (docs/plano-conserto-fachadas-uaf.md, T0): the fifth
    // pair tests/container/facade_pin_smoke.cpp's own T0 compares
    // against wl_proxy_get_user_data() - same "internal, never
    // installed" visibility every other adapter accessor in this
    // project already has, added ONLY so that fixture can read the
    // pending frame callback back after swap_buffers(); nothing inside
    // this class needs it exposed for its own sake. Null before the
    // first successful swap_buffers() (attach_frame_listener() is the
    // only writer).
    [[nodiscard]] wl_callback *pending_frame_callback() const noexcept {
        return m_pending_frame_callback;
    }

    // The wl_callback listener's own `done` callback (wayland-client's
    // C ABI - PUBLIC only so egl_context_adapter.cpp's own anonymous-
    // namespace listener constant can take its address from outside
    // this class, the SAME shape wayland_window_adapter's own listener
    // callbacks already document one file over).
    static void frame_callback_done(void *data, wl_callback *callback,
                                    std::uint32_t callback_data) noexcept;

  private:
    [[nodiscard]] gltfx_rslt<void> create_egl_display(wl_surface &surface) noexcept;
    [[nodiscard]] gltfx_rslt<void> choose_config(std::span<const gltfx_gfx_option_entry> options,
                                                 void *&out_config) noexcept;
    [[nodiscard]] gltfx_rslt<void> create_egl_window(wl_surface &surface, void *config,
                                                     std::uint32_t pixel_width,
                                                     std::uint32_t pixel_height) noexcept;
    // Runs AFTER create_egl_window() above has already produced
    // m_egl_surface: eglCreateContext(3.3 core) over `config`, make it
    // current, then validate_gl_context_version() (gl_version_policy.
    // hpp, fatia 2b) against what the driver actually granted - never
    // trusting the REQUEST as proof of the RESULT. Also resolves
    // GL_RENDERER for gpu_kind_state's own "minimo honesto" (sec.
    // 10.3, D-W6b-13).
    [[nodiscard]] gltfx_rslt<void> create_context(void *config) noexcept;
    void attach_frame_listener() noexcept;
    void resize_surface_if_due() noexcept;

    void *m_egl_display = nullptr; // EGLDisplay
    void *m_egl_context = nullptr; // EGLContext
    void *m_egl_surface = nullptr; // EGLSurface
    wl_egl_window *m_egl_window = nullptr;
    wl_surface *m_surface = nullptr; // borrowed from the window - never owned
    // Borrowed too, never owned (context.hpp's own public precondition
    // already guarantees `window` outlives this context - the same
    // reasoning that makes egl_probe_smoke.cpp's own teardown-order
    // SIGSEGV, this file's own header comment, not a risk here):
    // swap_buffers() below takes NO parameters (gl_context_adapter_
    // port.hpp's own concept), so resize_surface_if_due() needs
    // somewhere to read the window's own CURRENT pixel_size() from
    // every frame - this pointer is that somewhere.
    wayland_window_adapter *m_window = nullptr;
    wl_callback *m_pending_frame_callback = nullptr;

    frame_callback_sequence m_frame_sequence;
    gpu_kind_state m_gpu;

    std::uint32_t m_buffer_width = 0;
    std::uint32_t m_buffer_height = 0;

    bool m_vsync_on = true; // D-W6b-7's own default
    bool m_msaa_supported = false;
    bool m_srgb_supported = false;
};

} // namespace glintfx::platform
