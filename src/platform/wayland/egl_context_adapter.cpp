// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/wayland/egl_context_adapter.hpp"

#include <cerrno>
#include <string>
#include <utility>

// WL_EGL_PLATFORM before <EGL/egl.h> (tests/container/egl_probe_
// smoke.cpp's own header comment, this fatia's own briefing "leia-a
// inteira"): without it, <EGL/eglplatform.h>'s own #elif chain types
// EGLNativeWindowType as a bare integer instead of `struct wl_egl_
// window *`, forcing an integer-cast this file never needs.
#define WL_EGL_PLATFORM
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <poll.h>

#include <wayland-client.h>
#include <wayland-egl.h>

#include <glintfx/core/err_code.hpp>

#include "platform/gl/gl_surface_size_policy.hpp"
#include "platform/gl/gl_version_policy.hpp"
#include "platform/wayland/connection_failure.hpp"
#include "platform/wayland/window_adapter.hpp"
#include "platform/window/window_state.hpp"

// egl_context_adapter.cpp - W-EGL (docs/plano-w6b-placa-e-laco.md
// fatia 3, D-W6b-1/4/5/6/7/17/18, GODS_LAWS.md L-04/L-07/L-17/L-19/
// L-31): the real EGL/libwayland-egl calls egl_context_adapter.hpp's
// own header comment names, one atom per named step - the mechanics
// this file exercises were MEASURED, not guessed: G-1's own sonda
// (tests/container/egl_probe_smoke.cpp, fatia 1) already proved this
// exact sequence (eglGetPlatformDisplay -> eglInitialize -> eglBindAPI
// -> eglChooseConfig(RGBA8+stencil8) -> wl_egl_window_create -> egl
// CreateWindowSurface -> eglCreateContext(3.3 core)) runs to completion
// against the SAME `kwin_wayland --virtual` compositor this project's
// CI uses (run 34028525955's own container job).
//
// GL FUNCTIONS DECLARED BY HAND, <GL/gl.h> NOT LINKED (GODS_LAWS.md
// L-07, the SAME technique egl_probe_smoke.cpp/tests/win32_runner_
// probe_test.cpp already use, sourced against the SAME vendored
// registry src/render/gl_abi.hpp is measured against, third_party/
// khronos/gl.xml lines 176/1072-1074/1980-1981/6124): this adapter
// only ever needs glGetString/glGetIntegerv, resolved through
// eglGetProcAddress() the same way the public proc_address() below
// resolves anything else - never a second, hidden linkage path against
// -lGL.
namespace glintfx::platform {

namespace {

using gl_enum = unsigned int;
using gl_ubyte = unsigned char;
using gl_int = int;

constexpr gl_enum k_gl_renderer = 0x1F01;
constexpr gl_enum k_gl_major_version = 0x821B;
constexpr gl_enum k_gl_minor_version = 0x821C;
constexpr gl_enum k_gl_context_profile_mask = 0x9126;

using gl_get_string_fn = const gl_ubyte *(*)(gl_enum);
using gl_get_integerv_fn = void (*)(gl_enum, gl_int *);

// egl_context_adapter.hpp's own class comment names the callback
// listener's own address-taking requirement - the SAME shape wayland_
// window_adapter's own three listeners already document one file over.
constexpr wl_callback_listener k_frame_callback_listener{
    .done = &wayland_egl_context_adapter::frame_callback_done,
};

// Local, BUDGETED variant of wayland_display_adapter's own manpage-
// blessed prepare_read/flush/poll/read_events sequence (display_
// adapter.cpp, one directory over) - duplicated here rather than
// shared, because this caller only ever has a raw wl_display* (wl_
// proxy_get_display() from the window's own wl_surface, egl_context_
// adapter.hpp's own class comment: this adapter never needs a separate
// wayland_display_adapter& reference, since gl_context_facade.cpp's
// own open() call only ever hands it the window). The mandatory
// prepare_read/cancel_read pairing (manpage ARMADILHA 2) is identical;
// only the poll() TIMEOUT differs - budgeted here, always 0 in display_
// adapter.cpp's own non-blocking pump.
//
// Returns false only when the wl_display connection itself is now
// unusable (a real protocol/socket failure) - "nothing arrived within
// budget" is NOT a failure, it is reported through frame_callback_
// sequence.hpp's own decide_after_wait() instead, read by the caller
// after this function returns.
[[nodiscard]] bool poll_and_dispatch_with_budget(wl_display *display,
                                                 std::uint32_t budget_ms) noexcept {
    while (wl_display_prepare_read(display) != 0) {
        if (wl_display_dispatch_pending(display) == -1) {
            return false;
        }
    }

    while (wl_display_flush(display) == -1) {
        if (errno != EAGAIN) {
            wl_display_cancel_read(display);
            return false;
        }
        pollfd pending_write{.fd = wl_display_get_fd(display), .events = POLLOUT, .revents = 0};
        if (poll(&pending_write, 1, -1) == -1) {
            wl_display_cancel_read(display);
            return false;
        }
    }

    pollfd incoming{.fd = wl_display_get_fd(display), .events = POLLIN, .revents = 0};
    const int poll_result = poll(&incoming, 1, static_cast<int>(budget_ms));
    if (poll_result <= 0 || (incoming.revents & POLLIN) == 0) {
        // Budget exhausted with nothing to read - the mandatory other
        // half of ARMADILHA 2's pairing, never a bare poll() left
        // hanging without its matching cancel_read().
        wl_display_cancel_read(display);
        return true;
    }

    if (wl_display_read_events(display) == -1) {
        return false;
    }
    return wl_display_dispatch_pending(display) != -1;
}

} // namespace

wayland_egl_context_adapter::wayland_egl_context_adapter(
    wayland_egl_context_adapter &&other) noexcept
    : m_egl_display(std::exchange(other.m_egl_display, nullptr)),
      m_egl_context(std::exchange(other.m_egl_context, nullptr)),
      m_egl_surface(std::exchange(other.m_egl_surface, nullptr)),
      m_egl_window(std::exchange(other.m_egl_window, nullptr)),
      m_surface(std::exchange(other.m_surface, nullptr)),
      m_window(std::exchange(other.m_window, nullptr)),
      m_pending_frame_callback(std::exchange(other.m_pending_frame_callback, nullptr)),
      m_frame_sequence(other.m_frame_sequence), m_gpu(other.m_gpu),
      m_buffer_width(other.m_buffer_width), m_buffer_height(other.m_buffer_height),
      m_vsync_on(other.m_vsync_on), m_msaa_supported(other.m_msaa_supported),
      m_srgb_supported(other.m_srgb_supported) {}

wayland_egl_context_adapter &
wayland_egl_context_adapter::operator=(wayland_egl_context_adapter &&other) noexcept {
    if (this != &other) {
        close();
        m_egl_display = std::exchange(other.m_egl_display, nullptr);
        m_egl_context = std::exchange(other.m_egl_context, nullptr);
        m_egl_surface = std::exchange(other.m_egl_surface, nullptr);
        m_egl_window = std::exchange(other.m_egl_window, nullptr);
        m_surface = std::exchange(other.m_surface, nullptr);
        m_window = std::exchange(other.m_window, nullptr);
        m_pending_frame_callback = std::exchange(other.m_pending_frame_callback, nullptr);
        m_frame_sequence = other.m_frame_sequence;
        m_gpu = other.m_gpu;
        m_buffer_width = other.m_buffer_width;
        m_buffer_height = other.m_buffer_height;
        m_vsync_on = other.m_vsync_on;
        m_msaa_supported = other.m_msaa_supported;
        m_srgb_supported = other.m_srgb_supported;
    }
    return *this;
}

wayland_egl_context_adapter::~wayland_egl_context_adapter() { close(); }

gltfx_rslt<void> wayland_egl_context_adapter::create_egl_display(wl_surface &surface) noexcept {
    // wl_surface IS a wl_proxy (every Wayland protocol object shares
    // that layout - the SAME cast every wayland-scanner-generated
    // request wrapper already performs internally); wl_proxy_get_
    // display() is how a caller that never opened the connection
    // itself (this adapter's own class comment) reaches the wl_display
    // it belongs to.
    wl_display *display = wl_proxy_get_display(reinterpret_cast<wl_proxy *>(&surface));
    if (display == nullptr) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::platform_failure).with_rejected_value("wl_display"));
    }

    void *egl_display = eglGetPlatformDisplay(EGL_PLATFORM_WAYLAND_KHR, display, nullptr);
    if (egl_display == EGL_NO_DISPLAY) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::platform_failure).with_rejected_value("egl_display"));
    }

    EGLint egl_major = 0;
    EGLint egl_minor = 0;
    if (eglInitialize(egl_display, &egl_major, &egl_minor) != EGL_TRUE) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::platform_failure).with_rejected_value("egl_display"));
    }

    // Per-THREAD state read by the NEXT eglCreateContext() call (this
    // fatia's own busca, docs/plano-w6b-placa-e-laco.md sec. 0) - has
    // to happen before create_context() below, never after.
    if (eglBindAPI(EGL_OPENGL_API) != EGL_TRUE) {
        eglTerminate(egl_display);
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::platform_failure).with_rejected_value("egl_bind_api"));
    }

    m_egl_display = egl_display;
    return gltfx_rslt<void>::ok();
}

gltfx_rslt<void>
wayland_egl_context_adapter::choose_config(std::span<const gltfx_gfx_option_entry> options,
                                           void *&out_config) noexcept {
    std::int64_t msaa_samples = 0;
    std::int64_t srgb_framebuffer = 0;
    for (const gltfx_gfx_option_entry &entry : options) {
        if (entry.id == gltfx_gfx_option::msaa_samples) {
            msaa_samples = entry.value;
        } else if (entry.id == gltfx_gfx_option::srgb_framebuffer) {
            srgb_framebuffer = entry.value;
        }
    }

    // RGBA8+stencil8 always (D-W6b-4); EGL_SAMPLES/EGL_GL_COLORSPACE_
    // KHR appended only when requested - a fixed-size array, never a
    // heap allocation, this fatia's own open()-time path can afford
    // (it runs once per context, never per frame).
    auto try_choose = [this](std::int64_t samples, bool srgb, void *&config_out) noexcept -> bool {
        EGLint attribs[16] = {
            EGL_SURFACE_TYPE,
            EGL_WINDOW_BIT,
            EGL_RENDERABLE_TYPE,
            EGL_OPENGL_BIT,
            EGL_RED_SIZE,
            8,
            EGL_GREEN_SIZE,
            8,
            EGL_BLUE_SIZE,
            8,
            EGL_ALPHA_SIZE,
            8,
            EGL_STENCIL_SIZE,
            8,
            EGL_NONE,
            EGL_NONE,
        };
        std::size_t next = 12;
        if (samples > 0) {
            attribs[next++] = EGL_SAMPLES;
            attribs[next++] = static_cast<EGLint>(samples);
        }
        if (srgb) {
            attribs[next++] = EGL_GL_COLORSPACE_KHR;
            attribs[next++] = EGL_GL_COLORSPACE_SRGB_KHR;
        }
        attribs[next] = EGL_NONE;

        EGLConfig config = nullptr;
        EGLint num_configs = 0;
        const bool ok =
            eglChooseConfig(m_egl_display, attribs, &config, 1, &num_configs) == EGL_TRUE &&
            num_configs > 0;
        if (ok) {
            config_out = config;
        }
        return ok;
    };

    void *full_config = nullptr;
    if (try_choose(msaa_samples, srgb_framebuffer != 0, full_config)) {
        out_config = full_config;
        m_msaa_supported = true;
        m_srgb_supported = true;
        return gltfx_rslt<void>::ok();
    }

    // D-W6b-17: never degrade in silence - isolate WHICH option this
    // driver could not honor before refusing, one attempt at a time,
    // rather than blaming the first one requested.
    void *without_msaa = nullptr;
    if (msaa_samples > 0 && try_choose(0, srgb_framebuffer != 0, without_msaa)) {
        m_msaa_supported = false;
        m_srgb_supported = true;
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::unsupported).with_rejected_value("msaa_samples"));
    }

    void *without_srgb = nullptr;
    if (srgb_framebuffer != 0 && try_choose(msaa_samples, false, without_srgb)) {
        m_msaa_supported = true;
        m_srgb_supported = false;
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::unsupported).with_rejected_value("srgb_framebuffer"));
    }

    if (msaa_samples > 0) {
        m_msaa_supported = false;
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::unsupported).with_rejected_value("msaa_samples"));
    }
    if (srgb_framebuffer != 0) {
        m_srgb_supported = false;
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::unsupported).with_rejected_value("srgb_framebuffer"));
    }
    return gltfx_rslt<void>::err(
        gltfx_err(gltfx_err_code::platform_failure).with_rejected_value("egl_config"));
}

gltfx_rslt<void>
wayland_egl_context_adapter::create_egl_window(wl_surface &surface, void *config,
                                               std::uint32_t pixel_width,
                                               std::uint32_t pixel_height) noexcept {
    // In PIXELS, never logical_size() (D-W6a-17's own distinction,
    // this file's own header comment) - the framebuffer a consumer
    // paints into is the window's own pixel_size(), read by open()
    // before this atom is ever called.
    wl_egl_window *egl_window = wl_egl_window_create(&surface, static_cast<int>(pixel_width),
                                                     static_cast<int>(pixel_height));
    if (egl_window == nullptr) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::platform_failure).with_rejected_value("wl_egl_window"));
    }
    m_egl_window = egl_window;
    m_buffer_width = pixel_width;
    m_buffer_height = pixel_height;

    // The EGLSurface a wl_egl_window is FOR is created right alongside
    // it, never as a separate top-level step (egl_probe_smoke.cpp's
    // own header comment: the two are one indivisible mechanical step,
    // both needing this SAME `config`).
    EGLSurface egl_surface = eglCreateWindowSurface(m_egl_display, config, m_egl_window, nullptr);
    if (egl_surface == EGL_NO_SURFACE) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::platform_failure).with_rejected_value("egl_surface"));
    }
    m_egl_surface = egl_surface;
    return gltfx_rslt<void>::ok();
}

gltfx_rslt<void> wayland_egl_context_adapter::create_context(void *config) noexcept {
    const EGLint context_attribs[] = {
        EGL_CONTEXT_MAJOR_VERSION,
        3,
        EGL_CONTEXT_MINOR_VERSION,
        3,
        EGL_CONTEXT_OPENGL_PROFILE_MASK,
        static_cast<EGLint>(k_gl_context_core_profile_bit),
        EGL_NONE,
    };
    EGLContext egl_context =
        eglCreateContext(m_egl_display, config, EGL_NO_CONTEXT, context_attribs);
    if (egl_context == EGL_NO_CONTEXT) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::platform_failure).with_rejected_value("egl_context"));
    }
    m_egl_context = egl_context;

    if (eglMakeCurrent(m_egl_display, m_egl_surface, m_egl_surface, m_egl_context) != EGL_TRUE) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::platform_failure).with_rejected_value("egl_make_current"));
    }

    // D-W6b-27 (docs/plano-w6b-fatias-5.md, F1 of that plan's own §0):
    // the Mesa Wayland platform starts a NEW EGL context at swap
    // interval 1 - in that mode, eglSwapBuffers() installs its own
    // wl_surface.frame callback and BLOCKS waiting for it (emersion.fr/
    // blog/2018/wayland-rendering-loop, Neil Roberts's own patch), a
    // wait with NO budget at all, sitting UNDERNEATH frame_callback_
    // sequence's own 100ms-budgeted wait above swap_buffers() below.
    // With a hidden window, this used to hang the whole process before
    // this adapter's own budget ever got a chance to say "skipped_
    // hidden" - the exact regression this fatia's own §3.2 case T
    // proves fixed by timing 60 swaps with `vsync=off`. Interval 0
    // here, ONCE, right after the first make_current(): from now on
    // the ONLY pacer on this Wayland side is frame_callback_sequence,
    // driven explicitly by swap_buffers() below - `vsync=off` becomes
    // what it always promised to be (D-W6b-18: "o mais rapido que o
    // driver deixa"), never a disguised interval-1 wait. A driver that
    // refuses this call (EGL_FALSE) is not a failure worth reporting:
    // the EGL 1.5 spec (sec. 3.10.3) allows an implementation to ignore
    // eglSwapInterval() outright, and this adapter's own budgeted wait
    // is the only pacer either way once vsync=on asks for one.
    eglSwapInterval(m_egl_display, 0);

    // Resolved the SAME way the public proc_address() below resolves
    // anything else - this atom is a caller of that exact mechanism,
    // one layer below any public handle (egl_probe_smoke.cpp's own
    // header comment names this same technique).
    auto get_string = reinterpret_cast<gl_get_string_fn>(eglGetProcAddress("glGetString"));
    auto get_integerv = reinterpret_cast<gl_get_integerv_fn>(eglGetProcAddress("glGetIntegerv"));
    if (get_string == nullptr || get_integerv == nullptr) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::platform_failure).with_rejected_value("gl_proc_address"));
    }

    gl_int major = 0;
    gl_int minor = 0;
    gl_int profile_mask = 0;
    get_integerv(k_gl_major_version, &major);
    get_integerv(k_gl_minor_version, &minor);
    get_integerv(k_gl_context_profile_mask, &profile_mask);

    if (const gltfx_rslt<void> version_ok = validate_gl_context_version(
            static_cast<std::uint32_t>(major), static_cast<std::uint32_t>(minor),
            static_cast<std::uint32_t>(profile_mask));
        version_ok.has_error()) {
        return version_ok;
    }

    const gl_ubyte *renderer = get_string(k_gl_renderer);
    m_gpu.learn(gltfx_gpu_kind::unknown, renderer != nullptr
                                             ? reinterpret_cast<const char *>(renderer)
                                             : std::string_view{});
    return gltfx_rslt<void>::ok();
}

void wayland_egl_context_adapter::attach_frame_listener() noexcept {
    if (m_pending_frame_callback != nullptr) {
        wl_callback_destroy(m_pending_frame_callback);
        m_pending_frame_callback = nullptr;
    }
    // wl_surface_frame() failing (allocation failure inside libwayland-
    // client) is not treated as a fatal open()/swap_buffers() error:
    // frame_callback_sequence's own arm_pending() still marks the next
    // swap as "waiting", and it will degrade to skipped_hidden once the
    // budget runs out on a callback that was never actually requested -
    // an honest degrade, never a crash, for a condition this project
    // has never measured happening.
    wl_callback *callback = wl_surface_frame(m_surface);
    if (callback != nullptr) {
        wl_callback_add_listener(callback, &k_frame_callback_listener, this);
        m_pending_frame_callback = callback;
    }
}

void wayland_egl_context_adapter::resize_surface_if_due() noexcept {
    const window_size pixel_size = m_window->state().pixel_size();
    const gltfx_rslt<gl_surface_size_decision> decision = resolve_gl_surface_size(
        pixel_size.width, pixel_size.height, m_buffer_width, m_buffer_height);
    if (decision.has_error() || !decision.value().should_resize) {
        return;
    }
    wl_egl_window_resize(m_egl_window, static_cast<int>(decision.value().width),
                         static_cast<int>(decision.value().height), 0, 0);
    m_buffer_width = decision.value().width;
    m_buffer_height = decision.value().height;
}

gltfx_rslt<void>
wayland_egl_context_adapter::open(wayland_window_adapter &window,
                                  std::span<const gltfx_gfx_option_entry> options) noexcept {
    if (!window.is_open()) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value("window"));
    }
    wl_surface *surface = window.surface();
    if (surface == nullptr) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value("window"));
    }

    // D-W6b-18: `adaptive` has no Wayland/EGL equivalent - refused BY
    // NAME here too, not only from a LATER set_option() call, since
    // `vsync` is `live` and this fatia's own resolved-options span may
    // already carry it if a consumer asked for `adaptive` straight in
    // the opening list (gl_context_desc_validation.cpp's own shape
    // check accepts any value in [0, 2] - refusing the THIRD one is
    // this adapter's own job, never silently downgraded to `on`).
    for (const gltfx_gfx_option_entry &entry : options) {
        if (entry.id == gltfx_gfx_option::vsync && entry.value == 2) {
            return gltfx_rslt<void>::err(
                gltfx_err(gltfx_err_code::unsupported).with_rejected_value("vsync"));
        }
    }

    if (const gltfx_rslt<void> display_ok = create_egl_display(*surface); display_ok.has_error()) {
        close();
        return display_ok;
    }

    void *config = nullptr;
    if (const gltfx_rslt<void> config_ok = choose_config(options, config); config_ok.has_error()) {
        close();
        return config_ok;
    }

    const window_size pixel_size = window.state().pixel_size();
    if (const gltfx_rslt<void> window_ok =
            create_egl_window(*surface, config, pixel_size.width, pixel_size.height);
        window_ok.has_error()) {
        close();
        return window_ok;
    }

    if (const gltfx_rslt<void> context_ok = create_context(config); context_ok.has_error()) {
        close();
        return context_ok;
    }

    for (const gltfx_gfx_option_entry &entry : options) {
        if (entry.id == gltfx_gfx_option::vsync) {
            m_vsync_on = entry.value != 0;
        }
    }

    m_surface = surface;
    m_window = &window;
    // MEDIDO (docs/plano-w6b-fatias-5.md, esta fatia, achado do
    // implementador contra kwin_wayland --virtual isolado, GODS_LAWS.md
    // L-44/L-50): open() NAO arma um wl_surface.frame aqui. Fazer isso
    // contradiz o proprio contrato que frame_callback_sequence.hpp ja
    // documenta (frame_wait_plan::present_immediately - "either this is
    // the very first frame this context has ever presented... the
    // adapter may call eglSwapBuffers() right now, no socket I/O needed
    // first") e o que frame_callback_sequence.cpp implementa (m_pending
    // nasce false). Pedir o callback e marcar m_pending=true AQUI, antes
    // de qualquer conteudo jamais commitado nesta superficie, e' pedir
    // um aviso que a superficie nunca ganha: medido via WAYLAND_DEBUG=1
    // que este compositor nunca da wl_callback.done para essa superficie
    // sem buffer - as cinco tentativas da primeira apresentacao
    // reprovavam com first_presented_attempt=0 antes deste conserto. O
    // rearme real (attach_frame_listener()+arm_pending()) que ja existe
    // no fim do ramo vsync=on de swap_buffers(), logo apos um eglSwap
    // Buffers() bem-sucedido, ja cobre o PROXIMO quadro - esta linha
    // aqui era redundante e, pela medicao, ativamente prejudicial.
    return gltfx_rslt<void>::ok();
}

void wayland_egl_context_adapter::close() noexcept {
    if (m_egl_context != nullptr) {
        eglMakeCurrent(m_egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(m_egl_display, m_egl_context);
        m_egl_context = nullptr;
    }
    if (m_egl_surface != nullptr) {
        eglDestroySurface(m_egl_display, m_egl_surface);
        m_egl_surface = nullptr;
    }
    if (m_egl_display != nullptr) {
        eglTerminate(m_egl_display);
        m_egl_display = nullptr;
    }
    if (m_pending_frame_callback != nullptr) {
        wl_callback_destroy(m_pending_frame_callback);
        m_pending_frame_callback = nullptr;
    }
    if (m_egl_window != nullptr) {
        wl_egl_window_destroy(m_egl_window);
        m_egl_window = nullptr;
    }
    m_surface = nullptr;
    m_window = nullptr;
    m_frame_sequence = frame_callback_sequence{};
    m_gpu = gpu_kind_state{};
    m_buffer_width = 0;
    m_buffer_height = 0;
    m_msaa_supported = false;
    m_srgb_supported = false;
}

gltfx_rslt<void> wayland_egl_context_adapter::make_current() noexcept {
    if (eglMakeCurrent(m_egl_display, m_egl_surface, m_egl_surface, m_egl_context) != EGL_TRUE) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::platform_failure).with_rejected_value("egl_make_current"));
    }
    return gltfx_rslt<void>::ok();
}

gltfx_rslt<gltfx_present_outcome> wayland_egl_context_adapter::swap_buffers() noexcept {
    resize_surface_if_due();

    // D-W6b-6's own number, cited verbatim in the plan: "espera o
    // callback do quadro anterior com orcamento (100 ms)".
    constexpr std::uint32_t k_frame_callback_budget_ms = 100;

    // D-W6b-28: every failure path below that COULD be the wl_display
    // connection itself dying (a protocol error the compositor sent
    // under our feet, or the socket going away) resolves through this
    // ONE wl_surface's own display - the same wl_proxy_get_display()
    // trick create_egl_display() already uses one function up, hoisted
    // here so none of the three call sites below has to recompute it.
    wl_display *display = wl_proxy_get_display(reinterpret_cast<wl_proxy *>(m_surface));

    if (!m_vsync_on) {
        // `vsync=off`: present as fast as the driver allows, never
        // gated on the previous frame's callback (D-W6b-18).
        if (eglSwapBuffers(m_egl_display, m_egl_surface) != EGL_TRUE) {
            // D-W6b-28: a dead connection (this SAME `eglSwapBuffers`
            // is exactly where the D-W6b-12 mutation - a removed
            // ack_configure() - surfaces once a real buffer attach
            // finally reaches the compositor) is named by the REAL
            // interface that reprovou, never the generic "egl_swap_
            // buffers" placeholder - checked first, since a display
            // already fatally errored makes the EGL call itself fail
            // for a reason this adapter did not cause.
            if (wl_display_get_error(display) != 0) {
                return gltfx_rslt<gltfx_present_outcome>::err(build_connection_failure(display));
            }
            return gltfx_rslt<gltfx_present_outcome>::err(
                gltfx_err(gltfx_err_code::platform_failure)
                    .with_rejected_value("egl_swap_buffers"));
        }
        return gltfx_rslt<gltfx_present_outcome>::ok(gltfx_present_outcome::presented);
    }

    const frame_wait_plan plan = m_frame_sequence.plan_before_wait(k_frame_callback_budget_ms);
    if (plan == frame_wait_plan::give_up_without_polling) {
        return gltfx_rslt<gltfx_present_outcome>::ok(gltfx_present_outcome::skipped_hidden);
    }

    if (plan == frame_wait_plan::poll_then_decide) {
        if (!poll_and_dispatch_with_budget(display, k_frame_callback_budget_ms)) {
            // D-W6b-28: poll_and_dispatch_with_budget() only ever
            // returns false when the wl_display connection itself is
            // now unusable (this file's own comment on that function) -
            // always a real connection failure, never conditional on
            // wl_display_get_error() the way the two eglSwapBuffers
            // sites above/below are.
            return gltfx_rslt<gltfx_present_outcome>::err(build_connection_failure(display));
        }
        if (m_frame_sequence.decide_after_wait() == gltfx_present_outcome::skipped_hidden) {
            return gltfx_rslt<gltfx_present_outcome>::ok(gltfx_present_outcome::skipped_hidden);
        }
    }

    if (eglSwapBuffers(m_egl_display, m_egl_surface) != EGL_TRUE) {
        // D-W6b-28: same reasoning as the vsync=off branch above - the
        // vsync=on path is where the D-W6b-12 mutation (ack_configure()
        // removed) actually gets exercised by gl_context_parity_test's
        // own 2nd swap.
        if (wl_display_get_error(display) != 0) {
            return gltfx_rslt<gltfx_present_outcome>::err(build_connection_failure(display));
        }
        return gltfx_rslt<gltfx_present_outcome>::err(
            gltfx_err(gltfx_err_code::platform_failure).with_rejected_value("egl_swap_buffers"));
    }
    attach_frame_listener();
    m_frame_sequence.arm_pending();
    return gltfx_rslt<gltfx_present_outcome>::ok(gltfx_present_outcome::presented);
}

void *wayland_egl_context_adapter::proc_address(std::string_view name) const noexcept {
    // eglGetProcAddress() needs a NUL-terminated name; `name` is a
    // string_view that may not own one - a short-lived std::string
    // pays for that terminator once per lookup, never per frame (a
    // consumer resolves a GL entry point once and caches the address
    // itself, the same convention SDL/GLFW already document for their
    // own equivalents).
    const std::string owned(name);
    using egl_proc_fn = void (*)();
    // Function-pointer-to-object-pointer conversion: the SAME
    // universally-supported (if not strictly standard-blessed)
    // technique every GL loader (dlsym, GLAD, GLEW) already relies on -
    // this project's own public proc_address() contract (context.hpp)
    // exists to hand a consumer exactly this kind of address.
    egl_proc_fn function = eglGetProcAddress(owned.c_str());
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) reason: the universal
    // dlsym-style function-to-object-pointer cast every GL loader already relies on.
    return reinterpret_cast<void *>(function);
}

gltfx_rslt<void> wayland_egl_context_adapter::apply_option(gltfx_gfx_option_entry entry) noexcept {
    if (entry.id == gltfx_gfx_option::vsync) {
        if (entry.value == 2) { // adaptive - D-W6b-18, no Wayland/EGL equivalent
            return gltfx_rslt<void>::err(
                gltfx_err(gltfx_err_code::unsupported).with_rejected_value("vsync"));
        }
        m_vsync_on = entry.value != 0;
        return gltfx_rslt<void>::ok();
    }

    // Every other `live` id (frame_rate_cap, preset) is accepted here
    // with no adapter-side effect yet - see this class's own header
    // comment on apply_option() for why that is honest, not a gap.
    return gltfx_rslt<void>::ok();
}

gltfx_gfx_option_support
wayland_egl_context_adapter::option_support(gltfx_gfx_option id) const noexcept {
    switch (id) {
    case gltfx_gfx_option::vsync:
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

void wayland_egl_context_adapter::frame_callback_done(void *data, wl_callback *callback,
                                                      std::uint32_t /*callback_data*/) noexcept {
    auto *self = static_cast<wayland_egl_context_adapter *>(data);
    if (self->m_pending_frame_callback == callback) {
        self->m_pending_frame_callback = nullptr;
    }
    wl_callback_destroy(callback);
    self->m_frame_sequence.mark_frame_done();
}

} // namespace glintfx::platform
