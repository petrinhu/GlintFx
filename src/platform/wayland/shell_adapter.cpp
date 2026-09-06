// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/wayland/shell_adapter.hpp"

#include <wayland-client.h>
#include <xdg-shell-client-protocol.h>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

#include "platform/wayland/display_adapter.hpp"
#include "platform/wayland/global_catalog.hpp"
#include "platform/wayland/shell_requirements.hpp"

// shell_adapter.cpp - see shell_adapter.hpp's own header comment for
// scope. xdg_wm_base_ping() below is called back by libwayland-
// client's own C event-dispatch machinery - GODS_LAWS.md L-22: no
// exception may unwind across that C stack frame. xdg_wm_base_pong()
// itself is an ordinary wl_proxy_marshal_flags() request, the same
// noexcept C-API shape every other request in this project's Wayland
// backend already has - it cannot fail in a way this project reports
// (the request is fire-and-forget, like every other Wayland request
// with no reply).

namespace glintfx::platform {

namespace {

constexpr xdg_wm_base_listener kShellListener = {
    .ping = &wayland_shell_adapter::xdg_wm_base_ping,
};

} // namespace

wayland_shell_adapter::~wayland_shell_adapter() { close(); }

void wayland_shell_adapter::xdg_wm_base_ping(void * /*data*/, xdg_wm_base *shell,
                                             std::uint32_t serial) noexcept {
    xdg_wm_base_pong(shell, serial);
}

gltfx_rslt<void> wayland_shell_adapter::open(wayland_display_adapter &connection) noexcept {
    // "ONDE ESTA O PERIGO" item 2: names the missing interface BEFORE
    // a single wl_registry_bind() call happens - see shell_
    // requirements.hpp's own header comment.
    if (gltfx_rslt<void> requirements = check_shell_requirements(connection.globals());
        requirements.has_error()) {
        return requirements;
    }

    // Both lookups below can only ever find an entry: check_shell_
    // requirements() above already proved wl_compositor and
    // xdg_wm_base are both present in this SAME catalog, and nothing
    // between that call and these two mutates it (connection.bind()
    // only reads the catalog it is passed a record from).
    const wayland_global *compositor_global =
        connection.globals().find_by_interface("wl_compositor");
    gltfx_rslt<void *> compositor_proxy =
        connection.bind(*compositor_global, wl_compositor_interface, 6);
    if (compositor_proxy.has_error()) {
        return gltfx_rslt<void>::err(compositor_proxy.error());
    }

    const wayland_global *shell_global = connection.globals().find_by_interface("xdg_wm_base");
    gltfx_rslt<void *> shell_proxy = connection.bind(*shell_global, xdg_wm_base_interface, 6);
    if (shell_proxy.has_error()) {
        // Tear down what open() already created before returning -
        // is_open() has to read false on ANY failure path, the same
        // contract wayland_display_adapter::open() already documents.
        wl_compositor_destroy(static_cast<wl_compositor *>(compositor_proxy.value()));
        return gltfx_rslt<void>::err(shell_proxy.error());
    }

    m_compositor = static_cast<wl_compositor *>(compositor_proxy.value());
    m_xdg_wm_base = static_cast<xdg_wm_base *>(shell_proxy.value());
    xdg_wm_base_add_listener(m_xdg_wm_base, &kShellListener, this);
    return gltfx_rslt<void>::ok();
}

void wayland_shell_adapter::close() noexcept {
    // Reverse order of creation (same convention wayland_display_
    // adapter::close() already documents): xdg_wm_base was bound
    // AFTER wl_compositor, so it is destroyed first.
    if (m_xdg_wm_base != nullptr) {
        xdg_wm_base_destroy(m_xdg_wm_base);
        m_xdg_wm_base = nullptr;
    }
    if (m_compositor != nullptr) {
        wl_compositor_destroy(m_compositor);
        m_compositor = nullptr;
    }
}

} // namespace glintfx::platform
