// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <concepts>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/core/log/field.hpp>
#include <glintfx/core/log/severity.hpp>
#include <glintfx/platform/gl/context.hpp>

#include "core/log/emit.hpp"
#include "platform/wayland/frame_callback_sequence.hpp"

// platform/wayland/frame_request_present.hpp - WL-FRAME-ORDER-ARM-TEST (D-W8-226): the order of
// one Wayland EGL presentation, kept apart from the wire so a test can make the swap fail.
// llvmpipe never fails a swap (dri2_wl_swrast_swap_buffers_with_damage() returns EGL_TRUE every
// time), so no container fixture can reach the failure branch; Mesa's hardware path can
// (dri2_wl_swap_buffers_with_damage() answers EGL_BAD_ALLOC when create_wl_buffer() returns null,
// mesa issue 4426). The adapter (egl_context_adapter.cpp, present_through_egl()) binds the five
// ports to the real calls; tests/frame_request_present_test.cpp binds them to a script.
//
// No EGL and no Wayland type is reachable from this header, the same rule as
// frame_callback_sequence.hpp: it registers unguarded and runs on every system.

namespace glintfx::platform {

// The name of the event a missing frame callback emits: category "platform.wayland", severity
// warn, no fields (tests/frame_request_present_test.cpp reads it back through a sink).
inline constexpr std::string_view k_frame_callback_request_failed_event =
    "frame_callback_request_failed";

// D-W8-F01 (CONTRACT.md: a system call that can fail is checked, never ignored in silence): the
// report the adapter's report_frame_request_failed() port makes when wl_surface_frame() returned
// null. The presentation itself goes on, paced like vsync=off; the event is how a consumer learns
// that this frame was not paced by the compositor.
inline void report_frame_callback_request_failed() noexcept {
    log_emit_fields(gltfx_log_severity::warn, "platform.wayland",
                    k_frame_callback_request_failed_event, std::span<const gltfx_log_field>{});
}

// The five calls one presentation makes, in the order present_after_frame_request() makes them.
template <class Ports>
concept frame_present_ports = requires(Ports &ports) {
    // wl_surface_frame() plus its listener; false when libwayland returned null.
    { ports.request_frame() } noexcept -> std::same_as<bool>;
    // Called once, right after a request_frame() that returned false, and never otherwise.
    { ports.report_frame_request_failed() } noexcept -> std::same_as<void>;
    // eglSwapBuffers(); true only for EGL_TRUE.
    { ports.swap() } noexcept -> std::same_as<bool>;
    // The connection's own verdict, asked whatever the swap answered (EGL-DEAD-DISPLAY-GUARD S2).
    { ports.dead_after_swap() } noexcept -> std::same_as<std::optional<gltfx_err>>;
    // The clock frame_callback_sequence::pending_older_than() is later compared against.
    { ports.now_ns() } noexcept -> std::same_as<std::int64_t>;
};

// WL-FRAME-ORDER (D-W8-208): the frame request goes out BEFORE the swap, because the swap is the
// wl_surface.commit it takes effect on (wayland.xml, wl_surface.frame). The wait is armed only
// after a swap that succeeded AND a callback that exists:
// - a failed swap returns without arming, so the next present plans present_immediately and the
//   adapter destroys the orphan proxy; armed before the swap, one transient failure would leave
//   a wait for a callback no commit carries, and every later present would answer skipped_hidden;
// - a null wl_surface_frame() (only proxy_create() failing inside libwayland-client, with the
//   connection still alive) is reported at once, before the swap, so the report survives a swap
//   that then fails; and it arms nothing, so the frame is paced like vsync=off instead of
//   waiting forever for a callback that was never requested.
template <frame_present_ports Ports>
[[nodiscard]] gltfx_rslt<gltfx_present_outcome>
present_after_frame_request(Ports &ports, frame_callback_sequence &sequence) noexcept {
    const bool requested = ports.request_frame();
    if (!requested) {
        ports.report_frame_request_failed();
    }
    const bool swapped = ports.swap();
    if (const std::optional<gltfx_err> dead = ports.dead_after_swap(); dead.has_value()) {
        return gltfx_rslt<gltfx_present_outcome>::err(*dead);
    }
    if (!swapped) {
        return gltfx_rslt<gltfx_present_outcome>::err(
            gltfx_err(gltfx_err_code::platform_failure).with_rejected_value("egl_swap_buffers"));
    }
    if (requested) {
        sequence.arm_pending(ports.now_ns());
    }
    return gltfx_rslt<gltfx_present_outcome>::ok(gltfx_present_outcome::presented);
}

} // namespace glintfx::platform
