// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/wayland/display_adapter.hpp"

#include <wayland-client.h>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

// display_adapter.cpp - see display_adapter.hpp's own header comment
// for scope (connect/disconnect only, TDD case R3, GODS_LAWS.md L-20's
// declared exception: "adaptador que só encaminha chamada ao sistema
// operacional... o teste honesto é de integração e não unitário" -
// this file's own open()/close() are exactly that forwarding shape,
// proven by tests/display_connect_failure_test.cpp (the no-compositor
// failure path, runnable on any Linux CI leg) and by the container
// integration case (TEST-WLCONT, the connect-to-a-real-compositor
// path, GODS_LAWS.md L-09).

namespace glintfx::platform {

wayland_display_adapter::wayland_display_adapter(wayland_display_adapter &&other) noexcept
    : m_display(other.m_display) {
    other.m_display = nullptr;
}

wayland_display_adapter &
wayland_display_adapter::operator=(wayland_display_adapter &&other) noexcept {
    if (this != &other) {
        close();
        m_display = other.m_display;
        other.m_display = nullptr;
    }
    return *this;
}

wayland_display_adapter::~wayland_display_adapter() { close(); }

gltfx_rslt<void> wayland_display_adapter::open() noexcept {
    // nullptr: resolve the DEFAULT socket the same way every Wayland
    // client does (reads WAYLAND_DISPLAY, falling back to the literal
    // name "wayland-0" resolved INSIDE XDG_RUNTIME_DIR) - never a
    // hand-picked socket name. GODS_LAWS.md L-09 armadilha conhecida,
    // named explicitly so the next reader of this file does not
    // re-derive it the hard way: UNSETTING WAYLAND_DISPLAY does NOT
    // protect a test from reaching a real compositor, because the
    // fallback name is embedded in libwayland-client itself, not read
    // from that variable's absence. The only real protection is
    // pointing XDG_RUNTIME_DIR at an empty, private directory - which
    // is exactly what tests/display_connect_failure_test.cpp does, and
    // exactly why THIS function never has to know it is being tested
    // that way: from here, a private empty XDG_RUNTIME_DIR and a
    // machine that genuinely has no compositor running look identical -
    // wl_display_connect() returns nullptr either way.
    wl_display *display = wl_display_connect(nullptr);
    if (display == nullptr) {
        return gltfx_rslt<void>::err(gltfx_err(gltfx_err_code::platform_failure));
    }
    m_display = display;
    return gltfx_rslt<void>::ok();
}

void wayland_display_adapter::close() noexcept {
    if (m_display != nullptr) {
        wl_display_disconnect(m_display);
        m_display = nullptr;
    }
}

} // namespace glintfx::platform
