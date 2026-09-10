// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

#include <glintfx/core/err.hpp>
#include <glintfx/platform/gl/context.hpp>
#include <glintfx/platform/gl/gfx_option.hpp>

#include "platform/gl/gl_context_impl.hpp"

// platform/loop/loop_context_view.hpp - LOOP-RUN (cobertura), S2
// (/var/tmp/glintfx-plan/loop-fix.md sec. S2.2): the ONE header under
// src/platform/loop/ that reaches the operating system - transitively,
// through gl_context_impl.hpp's own #include of the real GL/EGL/WGL
// backend. loop_context_view satisfies loop_context_port (loop_ports.
// hpp) by forwarding straight to a REAL, already-open gl_context_impl's
// own adapter.
//
// ONLY loop_facade.cpp EVER INCLUDES THIS HEADER - the motor itself
// (loop_engine.hpp) never does: it only ever sees the CONCEPT (loop_
// context_port), never this concrete type, which is exactly what lets
// loop_engine.hpp stay "sem SO" and run in tests/loop_engine_test.cpp
// with no display, window or GL context anywhere in sight (S2-F11).
namespace glintfx::platform {

struct loop_context_view {
    gl_context_impl &impl;

    [[nodiscard]] gltfx_rslt<gltfx_present_outcome> swap_buffers() noexcept {
        return impl.adapter.swap_buffers();
    }

    [[nodiscard]] bool present_would_skip() const noexcept {
        return impl.adapter.present_would_skip();
    }

    // The frame-rate cap does NOT live on the adapter (S2-F2, the
    // plan's own fact): it lives in current_values, the per-context
    // mirror gltfx_gl_context::set_option()/option() already read and
    // write (gl_context_impl.hpp's own header comment) - reached
    // DIRECTLY here, the exact same loop loop_facade.cpp's own former
    // read_frame_rate_cap_hz() used, moved to this file verbatim.
    [[nodiscard]] std::uint32_t frame_rate_cap_hz() const noexcept {
        for (const gltfx_gfx_option_entry &entry : impl.current_values) {
            if (entry.id == gltfx_gfx_option::frame_rate_cap) {
                return entry.value > 0 ? static_cast<std::uint32_t>(entry.value) : 0;
            }
        }
        return 0;
    }
};

} // namespace glintfx::platform
