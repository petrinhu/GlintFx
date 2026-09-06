// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <string>

#include <glintfx/core/err.hpp>

#include "platform/input/seat_capabilities.hpp"

// platform/wayland/seat_adapter.hpp - WL-SEAT fatia S-B (docs/plano-
// w6a-janela.md fatia 12): the Wayland MECHANISM that feeds the
// shared, OS-agnostic seat_capabilities value type (platform/input/
// seat_capabilities.hpp, S-A') - the one wl_seat this build binds
// (D-W5-5, "um seat so, o primeiro anunciado": wayland_display_
// adapter::globals().find_by_interface("wl_seat") already returns
// only the first match, the same rule display_adapter.cpp's own
// registry_global() comment already documents for wl_compositor/xdg_
// wm_base), translated through wl_seat.capabilities/wl_seat.name.
//
// "ONDE ESTA O PERIGO" item 1 of this fatia's own briefing: the
// compositor sends wl_seat.capabilities on bind AND WHENEVER a device
// class is later gained or lost - wl_seat_listener's own capabilities
// callback is the SAME callback both times, there is no separate
// "initial" event. seat_adapter_listener_test.cpp (tests/) proves
// BOTH shapes by calling wl_seat_capabilities() directly twice with
// two different bitmasks - a container fixture cannot force a real
// compositor to hot-plug a device mid-run, so the REPEATED-announcement
// path is proven at the listener level, never only the first call.
//
// wl_seat FORWARD-DECLARED here, <wayland-client.h> NOT included: the
// same reasoning wayland_shell_adapter's own header comment already
// documents one directory over - the only thing this header needs to
// know about wl_seat is that a POINTER to one exists.
struct wl_seat;

namespace glintfx::platform {

class wayland_display_adapter;

// GODS_LAWS.md L-19 armadilha 2 ("porta gorda"), same reasoning
// wayland_shell_adapter/wayland_window_adapter already document: this
// class takes a wayland_display_adapter& directly, not the
// display_connection_port concept - display_connection<A>::adapter()
// (D-W5-10) is W-D', a slice this fatia does not depend on.
class wayland_seat_adapter {
  public:
    // Starts CLOSED (m_seat null) - is_open() reports false until
    // open() succeeds, same shape every other adapter in this
    // directory already has.
    wayland_seat_adapter() noexcept = default;

    // PINNED, NEVER MOVABLE (FACADE-PIN, docs/plano-conserto-fachadas-
    // uaf.md sec. 6/7.2), same reasoning as wayland_shell_adapter/
    // wayland_window_adapter: copying a live wl_seat proxy would hand
    // two owners the same protocol object. Moving is deleted too -
    // open() (seat_adapter.cpp) registers this object's own address
    // with wl_seat_add_listener(..., this), and this class was LATENT
    // for the identical defect (no owner in src/ yet moved it, this
    // plan's own sec. 3 #3) - deleting the move closes it before the
    // W6b seat facade has a chance to repeat window_adapter.hpp's own
    // mistake, uniform with every other adapter in this fatia (D-UAF-2).
    wayland_seat_adapter(const wayland_seat_adapter &) = delete;
    wayland_seat_adapter &operator=(const wayland_seat_adapter &) = delete;
    wayland_seat_adapter(wayland_seat_adapter &&) = delete;
    wayland_seat_adapter &operator=(wayland_seat_adapter &&) = delete;

    // Destroys whatever this adapter still owns - safe to run on a
    // moved-from or never-opened instance, same idempotent-safe
    // contract close() below documents.
    ~wayland_seat_adapter();

    // A compositor with zero seats is a real environment this project
    // names, never guesses at as a null pointer three calls later
    // (same "onde esta o perigo" item 1 reasoning wayland_shell_
    // adapter::open() already documents for wl_compositor/xdg_wm_base):
    // no "wl_seat" entry in `connection.globals()` refuses with
    // gltfx_err_code::not_found and rejected_value() == "wl_seat"
    // BEFORE this method ever calls connection.bind().
    //
    // Binds wl_seat at version 5 (D-W5-6, unchanged by decision 15 -
    // only wl_compositor moved to 6): wl_seat_release (the polite
    // teardown request, as opposed to just destroying the local proxy)
    // is WL_SEAT_RELEASE_SINCE_VERSION 5, and this project always asks
    // for exactly the version whose teardown path close() below relies
    // on. `connection`'s own bind()/clamp_version() means a compositor
    // offering less never turns into a protocol violation - see
    // wayland_display_adapter::bind()'s own header comment, one
    // directory over.
    [[nodiscard]] gltfx_rslt<void> open(wayland_display_adapter &connection) noexcept;

    // Idempotent-safe, same contract every other adapter's close()
    // documents. VERSION-AWARE TEARDOWN (this fatia's own correctness
    // detail, the same class of risk docs/plano-w6a-janela.md sec. 5
    // catalogs for the rest of this onda): a compositor that only
    // offers wl_seat below version 5 makes clamp_version() bind a
    // proxy this project never actually asked for by exact number -
    // wl_seat_release() on THAT proxy is a protocol violation (the
    // request does not exist below v5), so close() reads the proxy's
    // own bound version (wl_seat_get_version(), the same wl_proxy_get_
    // version() every generated *_get_version() accessor calls) and
    // only calls wl_seat_release() when it is actually >= 5, falling
    // back to the plain wl_seat_destroy() (which only frees the local
    // proxy, never a protocol request) otherwise - the exact fallback
    // libwayland's own generated bindings use this pattern for
    // everywhere a request gains a MIN_VERSION after the interface
    // already shipped.
    void close() noexcept;

    [[nodiscard]] bool is_open() const noexcept { return m_seat != nullptr; }

    // D-W5-2's own shape, applied to the seat the same way window_
    // state.hpp already applies it to the window: consultable state,
    // never an event queue. Survives close() unlike m_seat itself -
    // same convention wayland_window_adapter::state() already keeps
    // (close() there never resets m_state either) - a caller reading
    // "what did the seat last report" after tearing down still gets a
    // real answer, not a silently-reset default.
    [[nodiscard]] const seat_capabilities &capabilities() const noexcept { return m_capabilities; }

    // wl_seat.name (WL_SEAT_NAME_SINCE_VERSION 2, always present at
    // this fatia's bound version 5): a UTF-8 string with no defined
    // convention (xdg-shell.xml's own sibling protocol, wayland.xml:
    // "In a multi-seat configuration the seat name can be used by
    // clients to help identify which physical devices the seat
    // represents"). Empty before the event ever arrives - a compositor
    // is not required to send it before the first capabilities event,
    // only "eventually" (wayland.xml itself gives no ordering
    // guarantee between the two).
    [[nodiscard]] const std::string &name() const noexcept { return m_name; }

    // OWN DECISION, undocumented by any surviving plan file for this
    // exact fatia (flagged in this fatia's own report, per the
    // briefing's "decida pelo caminho mais conservador e registre"):
    // a plain monotonic counter, incremented once per capabilities
    // event and once per name event, never reset by close(). Lets a
    // caller (or a container fixture printing "did anything change
    // since last read") detect that at least one update happened
    // without having to snapshot every field first - the smallest
    // shape that answers "onde esta o perigo" item 1 ("as capacidades
    // ... podem mudar depois") without inventing a second, competing
    // observable alongside capabilities()/name() themselves.
    [[nodiscard]] std::uint64_t last_change() const noexcept { return m_last_change; }

    // wl_seat_listener's own two callbacks (C function-pointer ABI).
    // PUBLIC ONLY so seat_adapter.cpp's own anonymous-namespace
    // wl_seat_listener constant can take their address from outside
    // the class - same shape and same GODS_LAWS.md L-19 reasoning
    // wayland_shell_adapter::xdg_wm_base_ping() already documents
    // (this whole class is itself internal, never under include/
    // glintfx/).
    static void wl_seat_capabilities(void *data, wl_seat *seat,
                                     std::uint32_t capabilities) noexcept;
    static void wl_seat_name(void *data, wl_seat *seat, const char *name) noexcept;

  private:
    wl_seat *m_seat = nullptr;
    seat_capabilities m_capabilities;
    std::string m_name;
    std::uint64_t m_last_change = 0;
};

} // namespace glintfx::platform
