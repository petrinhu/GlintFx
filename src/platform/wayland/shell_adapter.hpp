// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

#include <glintfx/core/err.hpp>

// platform/wayland/shell_adapter.hpp - WL-WINDOW fatia W-B (docs/
// plano-w6a-janela.md fatia 5): the adapter that gathers the two
// services a window needs to exist at all - wl_compositor (the
// factory a wl_surface is created from) and xdg_wm_base (the
// desktop-shell protocol xdg_surface/xdg_toplevel build on top of),
// plus the liveness check the compositor sends over xdg_wm_base and
// expects an answer to. WL-WINDOW fatia W-E (docs/plano-w6a-janela.md
// fatia 8, a later slice) is the one caller that reaches for
// compositor()/shell() below to actually create a surface; this
// adapter's own job stops at "the two services this project needs
// both exist and are bound".
//
// wl_compositor AND xdg_wm_base FORWARD-DECLARED here (display_
// adapter.hpp's own header comment names the same reasoning, one
// directory over): the only thing this header needs to know about
// either is that a POINTER to one exists. Keeping <wayland-client.h>
// and the generated <xdg-shell-client-protocol.h> out of this header
// means anything that only needs to know "a wayland_shell_adapter
// exists and has this shape" never has to put either include
// directory on its search path at all.
struct wl_compositor;
struct xdg_wm_base;

namespace glintfx::platform {

class wayland_display_adapter;

// GODS_LAWS.md L-19 armadilha 2 ("porta gorda"): this class takes a
// wayland_display_adapter& directly, not the display_connection_port
// concept (src/platform/port/) - display_connection<A>'s own
// adapter() accessor (D-W5-10) is docs/plano-w6a-janela.md fatia 6/
// W-D', a later slice this fatia does not depend on. Every internal
// caller today (fatia 8's window_adapter among them) already holds
// the wayland_display_adapter it opened directly, and this class asks
// for exactly that, no more.
class wayland_shell_adapter {
  public:
    // Starts CLOSED (m_compositor null) - is_open() reports false
    // until open() succeeds, same shape wayland_display_adapter
    // already has.
    wayland_shell_adapter() noexcept = default;

    // Move-only, same reasoning as wayland_display_adapter (display_
    // adapter.hpp): copying a live wl_compositor/xdg_wm_base proxy
    // pair would hand two owners the same protocol objects, and
    // destroying either proxy twice is a use-after-free in
    // libwayland-client's own bookkeeping.
    wayland_shell_adapter(const wayland_shell_adapter &) = delete;
    wayland_shell_adapter &operator=(const wayland_shell_adapter &) = delete;

    wayland_shell_adapter(wayland_shell_adapter &&other) noexcept;
    wayland_shell_adapter &operator=(wayland_shell_adapter &&other) noexcept;

    // Destroys whatever this adapter still owns - safe to run on a
    // moved-from or never-opened instance, same idempotent-safe
    // contract wayland_display_adapter::~wayland_display_adapter()
    // already documents.
    ~wayland_shell_adapter();

    // "ONDE ESTA O PERIGO" item 2 (docs/plano-w6a-janela.md, fatia
    // W-B): a compositor missing wl_compositor OR xdg_wm_base refuses
    // through check_shell_requirements() (shell_requirements.hpp)
    // BEFORE this method ever calls connection.bind() - the error
    // names EXACTLY which interface it looked for and did not find
    // (gltfx_err::rejected_value(), "wl_compositor" or
    // "xdg_wm_base"), never a null pointer surfacing three calls
    // later. `connection` must already be open() and have completed
    // its own initial roundtrip (wayland_display_adapter::open()'s
    // own contract) - this method reads connection.globals() and
    // calls connection.bind(), never opens the connection itself.
    //
    // Binds wl_compositor and xdg_wm_base at version 6 each (docs/
    // plano-w6a-janela.md decision D-W5-6 altered: wl_surface
    // inherits wl_compositor's own bound version, and the
    // preferred_buffer_scale event WL-WINDOW fatia W-E needs is
    // WL_SURFACE_PREFERRED_BUFFER_SCALE_SINCE_VERSION 6) -
    // wayland_display_adapter::bind()'s own clamp_version() call
    // means a compositor offering less never turns into a protocol
    // violation, it turns into a live, correctly reduced proxy
    // instead. On any bind failure, whatever was already bound is
    // torn down before returning - is_open() reads false either way.
    //
    // The listener attached to xdg_wm_base has exactly one callback -
    // "ONDE ESTA O PERIGO" item 1: xdg_wm_base_ping() answers with
    // xdg_wm_base_pong() IMMEDIATELY, using the SAME serial the event
    // carried. A compositor that pings and gets no pong back is
    // entitled to consider this client unresponsive and disconnect it -
    // there is no later point at which answering is still "in time".
    [[nodiscard]] gltfx_rslt<void> open(wayland_display_adapter &connection) noexcept;

    void close() noexcept;

    [[nodiscard]] bool is_open() const noexcept { return m_compositor != nullptr; }

    // Read-only accessors for WL-WINDOW fatia W-E (docs/plano-w6a-
    // janela.md fatia 8) - the one caller that actually creates a
    // wl_surface/xdg_surface/xdg_toplevel from these. Both null on a
    // closed adapter.
    [[nodiscard]] wl_compositor *compositor() const noexcept { return m_compositor; }
    [[nodiscard]] xdg_wm_base *shell() const noexcept { return m_xdg_wm_base; }

    // The xdg_wm_base listener's own single callback (C function-
    // pointer ABI). PUBLIC ONLY so shell_adapter.cpp's own anonymous-
    // namespace xdg_wm_base_listener constant can take its address
    // from outside the class - same shape and same GODS_LAWS.md L-19
    // reasoning wayland_display_adapter::registry_global() already
    // documents (this whole class is itself internal, never under
    // include/glintfx/).
    static void xdg_wm_base_ping(void *data, xdg_wm_base *shell, std::uint32_t serial) noexcept;

  private:
    wl_compositor *m_compositor = nullptr;
    xdg_wm_base *m_xdg_wm_base = nullptr;
};

} // namespace glintfx::platform
