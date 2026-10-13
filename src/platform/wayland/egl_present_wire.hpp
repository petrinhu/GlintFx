// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <optional>

#include <glintfx/core/err.hpp>

struct wl_callback;
struct wl_callback_listener;
struct wl_display;
struct wl_surface;

// platform/wayland/egl_present_wire.hpp - WL-FRAME-ORDER-ARM-TEST-fix1 (D-W8-F11): the ports of
// present_after_frame_request() and wait_for_frame_callback() bound to the wire, as named types
// whose every system call is injectable. The default of each call is the real one
// (egl_context_adapter.cpp defines the real_* functions next to the rest of the adapter), so the
// adapter changes nothing by constructing them with defaults, and
// tests/egl_present_wiring_test.cpp passes scripted calls to prove the binding itself: a port
// that ignores what its call answered, or calls nothing, fails an assertion. The same seam as
// incoming_poll_syscall.hpp (CONT-WARMUP C-6): a parameter of the call's own type, real by
// default. Internal by design (src/, never include/glintfx/, GODS_LAWS.md L-19). No EGL type is
// reachable from here, the same rule as egl_context_adapter.hpp: an EGLDisplay and an EGLSurface
// are `void *`, and an EGLBoolean is `unsigned int` (egl_context_adapter.cpp asserts both).
namespace glintfx::platform {

// The real calls, defined in egl_context_adapter.cpp.
[[nodiscard]] unsigned int real_egl_swap_buffers(void *egl_display, void *egl_surface) noexcept;
[[nodiscard]] wl_callback *real_surface_frame(wl_surface *surface) noexcept;
int real_callback_add_listener(wl_callback *callback, const wl_callback_listener *listener,
                               void *data) noexcept;
void real_callback_destroy(wl_callback *callback) noexcept;
[[nodiscard]] std::optional<gltfx_err>
real_connection_failure_if_dead(wl_display *display) noexcept;
void real_report_frame_callback_request_failed() noexcept;
[[nodiscard]] bool real_poll_for(wl_display *display, std::uint32_t budget_ms) noexcept;
[[nodiscard]] std::int64_t real_now_ns() noexcept;

// Every system call one presentation makes; real by default.
struct egl_present_calls {
    unsigned int (*swap_buffers)(void *, void *) noexcept = &real_egl_swap_buffers;
    wl_callback *(*surface_frame)(wl_surface *) noexcept = &real_surface_frame;
    int (*callback_add_listener)(wl_callback *, const wl_callback_listener *,
                                 void *) noexcept = &real_callback_add_listener;
    void (*callback_destroy)(wl_callback *) noexcept = &real_callback_destroy;
    std::optional<gltfx_err> (*connection_failure_if_dead)(wl_display *) noexcept =
        &real_connection_failure_if_dead;
    void (*report_frame_callback_request_failed)() noexcept =
        &real_report_frame_callback_request_failed;
    std::int64_t (*now_ns)() noexcept = &real_now_ns;
};

// What one presentation acts on: the adapter's own handles and the two slots it keeps between
// presentations (the pending callback, and whether a failure to request one was already
// reported).
struct egl_present_target {
    void *egl_display = nullptr;
    void *egl_surface = nullptr;
    wl_surface *surface = nullptr;
    wl_display *display = nullptr;
    wl_callback **pending_callback = nullptr;
    bool *request_failure_reported = nullptr;
    const wl_callback_listener *listener = nullptr;
    void *listener_data = nullptr;
};

// The five ports of present_after_frame_request() (frame_request_present.hpp) on the wire.
struct egl_wire_present_ports {
    egl_present_target target;
    egl_present_calls calls;

    // Destroys the orphan of the previous presentation, asks for this frame's callback and
    // listens to it; false only when wl_surface_frame() returned null.
    [[nodiscard]] bool request_frame() noexcept;
    // D-W8-F12: the event goes out on the EDGE, once per run of failures on this surface (the
    // next request that succeeds re-arms it), never once per frame.
    void report_frame_request_failed() noexcept;
    // True only for EGL_TRUE.
    [[nodiscard]] bool swap() noexcept;
    [[nodiscard]] std::optional<gltfx_err> dead_after_swap() noexcept;
    [[nodiscard]] std::int64_t now_ns() noexcept;
};

// The two ports of wait_for_frame_callback() (frame_callback_wait.hpp) on the wire.
struct egl_wire_wait_ports {
    wl_display *display = nullptr;
    bool (*poll)(wl_display *, std::uint32_t) noexcept = &real_poll_for;
    std::int64_t (*clock)() noexcept = &real_now_ns;

    [[nodiscard]] std::int64_t now_ns() noexcept;
    [[nodiscard]] bool poll_for(std::uint32_t budget_ms) noexcept;
};

} // namespace glintfx::platform
