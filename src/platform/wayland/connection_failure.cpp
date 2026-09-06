// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/wayland/connection_failure.hpp"

#include <cerrno>
#include <cstdint>

#include <wayland-client.h>

#include <glintfx/core/err_code.hpp>

// connection_failure.cpp - GL-CONTEXT fatia 5 (docs/plano-w6b-fatias-
// 5.md, D-W6b-28): the body MOVED, verbatim, from display_adapter.cpp's
// own anonymous-namespace build_fatal_error() (WL-DISPLAY fatia C) -
// see connection_failure.hpp's own header comment for why this is an
// extraction, not a rewrite. display_adapter.cpp's twelve call sites
// now call THIS function; egl_context_adapter.cpp (this same fatia) is
// the second, and reason, caller.

namespace glintfx::platform {

gltfx_err build_connection_failure(wl_display *display) noexcept {
    gltfx_err error(gltfx_err_code::platform_failure);
    const int raw = wl_display_get_error(display);
    error.with_os_error_code(raw);
    if (raw == EPROTO) {
        const wl_interface *interface = nullptr;
        std::uint32_t object_id = 0;
        wl_display_get_protocol_error(display, &interface, &object_id);
        if (interface != nullptr && interface->name != nullptr) {
            error.with_rejected_value(interface->name);
        }
    }
    return error;
}

} // namespace glintfx::platform
