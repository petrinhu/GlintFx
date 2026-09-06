// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <glintfx/core/err.hpp>

#include "platform/wayland/window_configure_sequence.hpp"
#include "platform/window/window_state.hpp"

// platform/wayland/window_adapter.hpp - WL-WINDOW fatia W-E (docs/
// plano-w6a-janela.md fatia 8): the one caller that actually turns a
// wayland_shell_adapter (W-B, wl_compositor + xdg_wm_base already
// bound) into a real, on-screen-eligible window - wl_surface,
// xdg_surface, xdg_toplevel, in that order, and nothing past
// "configured, acked, no content" (D-W5-8: mapping is whoever attaches
// a buffer's job, never this adapter's).
//
// wl_surface, wl_output, xdg_surface, xdg_toplevel AND wl_array all
// FORWARD-DECLARED here, <wayland-client.h> NOT included: the same
// reasoning display_adapter.hpp and shell_adapter.hpp already document
// one directory over - a POINTER is all this header needs to know
// about any of them (wl_array only ever appears here as a pointer
// parameter on a callback declaration; xdg_toplevel_configure() below
// is defined in window_adapter.cpp, where the real layout is visible).
struct wl_surface;
struct wl_output;
struct xdg_surface;
struct xdg_toplevel;
struct wl_array;

namespace glintfx::platform {

class wayland_display_adapter;
class wayland_shell_adapter;

// The INTERNAL descriptor this fatia's own open() takes - NOT the
// public gltfx_window_desc (decision D-W5-4/15, docs/plano-w6a-
// janela.md), which is W-D' (fatia 6, headers publicos comuns) and has
// not been built yet by anyone as of this fatia (another agent is
// building include/glintfx/ concurrently, GODS_LAWS.md L-32: this
// fatia does not reach into that directory to wire the two together -
// that mapping is W-D''s own job, one layer up). Every field here
// already carries the SAME name and unit the plan's decision 15 gives
// the public struct (logical_size, never a bare "size"), so W-D''s
// eventual translation is a field-for-field copy, not a redesign.
struct wayland_window_desc {
    std::uint32_t logical_width = 0;
    std::uint32_t logical_height = 0;
    std::string_view title;
    std::string_view application_id;
};

// GODS_LAWS.md L-19 armadilha 2 ("porta gorda"), same reasoning
// shell_adapter.hpp already documents: this class takes a
// wayland_display_adapter& and a wayland_shell_adapter& directly, not
// the display_connection_port concept - display_connection<A>::
// adapter() (D-W5-10) is W-D', a later slice this fatia does not
// depend on.
class wayland_window_adapter {
  public:
    // Starts CLOSED (m_surface null) - is_open() reports false until
    // open() succeeds, same shape wayland_display_adapter/wayland_
    // shell_adapter already have.
    wayland_window_adapter() noexcept = default;

    // PINNED, NEVER MOVABLE (FACADE-PIN, docs/plano-conserto-fachadas-
    // uaf.md sec. 1/6/7.2) - THE MEASURED DEFECT THIS FATIA EXISTS TO
    // FIX: copying a live wl_surface/xdg_surface/xdg_toplevel proxy
    // triple would hand two owners the same protocol objects, same
    // reasoning as before. Moving used to be allowed, and open()
    // (window_adapter.cpp) registers this object's own address with
    // three listeners (wl_surface_add_listener/xdg_surface_add_
    // listener/xdg_toplevel_add_listener, all `this`) - the move
    // constructor never re-pointed any of them, so window_facade.cpp's
    // own std::move(adapter) into the heap left all three pointing at a
    // dead stack address (T0, this plan's own measurement: this was
    // the crash the implementer of fatia 5 hit, first-hand). Deleting
    // the move is the fix: window_adapter_port now requires pinned_
    // adapter<A> instead of std::movable<A>, so window_facade.cpp
    // cannot select a movable adapter here even by accident.
    wayland_window_adapter(const wayland_window_adapter &) = delete;
    wayland_window_adapter &operator=(const wayland_window_adapter &) = delete;
    wayland_window_adapter(wayland_window_adapter &&) = delete;
    wayland_window_adapter &operator=(wayland_window_adapter &&) = delete;

    // Destroys whatever this adapter still owns - safe to run on a
    // moved-from or never-opened instance, same idempotent-safe
    // contract close() below documents.
    ~wayland_window_adapter();

    // Runs the seven atoms in order (docs/plano-w6a-janela.md fatia 8):
    // create_surface, create_xdg_surface, create_toplevel, apply_desc,
    // commit_initial, wait_first_configure. `connection` must already
    // be open() (this method calls connection.roundtrip(), never opens
    // the connection itself); `shell` must already be open() (this
    // method reads shell.compositor()/shell(), never binds them
    // itself, same division of labor shell_adapter::open() already has
    // with wayland_display_adapter).
    //
    // TEARDOWN ON ANY FAILURE (same contract wayland_display_adapter::
    // open()/wayland_shell_adapter::open() already document): whatever
    // this call had already created is destroyed before returning -
    // is_open() reads false either way.
    //
    // WAIT_FIRST_CONFIGURE IS WHERE THE ACK LIVES (D-W5-7, "ONDE ESTA O
    // PERIGO" of this fatia's own briefing): a compositor sends the
    // first xdg_surface.configure only after commit_initial()'s bare
    // wl_surface_commit() (no buffer attached yet, D-W5-8) - and NEVER
    // complains about a missing ack_configure on its own, because
    // nothing was ever attached for it to reject. The ack call inside
    // wait_first_configure() is REMOVED by the adversarial reviewer's
    // own mutation (window_smoke.cpp, tests/container/): only a fixture
    // that goes on to attach a REAL wl_shm buffer and commit again
    // (D-W5-9, the fixture's own job, never this adapter's) makes that
    // missing ack surface as a protocol error
    // (xdg_surface.error.unconfigured_buffer, xdg-shell.xml) - which
    // display_adapter.cpp's own protocol_error() attaches as
    // rejected_value() == "xdg_surface", the exact string this fatia's
    // briefing names.
    [[nodiscard]] gltfx_rslt<void> open(wayland_display_adapter &connection,
                                        wayland_shell_adapter &shell,
                                        const wayland_window_desc &desc) noexcept;

    // Idempotent-safe: destroys toplevel, xdg_surface, surface, in
    // that REVERSE order of creation - same convention wayland_display_
    // adapter::close()/wayland_shell_adapter::close() already document.
    void close() noexcept;

    [[nodiscard]] bool is_open() const noexcept { return m_surface != nullptr; }

    // WL-WINDOW-HANDLE (docs/plano-w6a-janela.md, achado do CTO por
    // leitura): the ONE method win32_window_adapter already had
    // (src/platform/win32/window_adapter.hpp) that this adapter did
    // not - apply_desc() above only ever calls xdg_toplevel_set_title()
    // ONCE, at open() time; nothing here let a caller change it
    // afterward. gltfx_window::set_title() (window_facade.cpp) needs
    // BOTH sides to answer to the same public method, and GODS_LAWS.md
    // L-04 forbids a public capability that only works on one system.
    // Same validation, same "refused when not open" shape win32_
    // window_adapter::set_title() already documents - xdg_toplevel_
    // set_title() (xdg-shell.xml) is a fire-and-forget request with no
    // reply to wait for, so this never blocks on a roundtrip.
    [[nodiscard]] gltfx_rslt<void> set_title(std::string_view title) noexcept;

    // D-W5-2 (confirmed): consultable state, never an event queue - see
    // window_state.hpp's own header comment for the full reasoning.
    [[nodiscard]] const window_state &state() const noexcept { return m_state; }

    [[nodiscard]] wl_surface *surface() const noexcept { return m_surface; }

    // FACADE-PIN (docs/plano-conserto-fachadas-uaf.md, T0): the second
    // of the four proxies tests/container/facade_pin_smoke.cpp's own
    // T0 compares against wl_proxy_get_user_data() - same "internal,
    // never installed" visibility every other adapter accessor in this
    // file already has, added ONLY so that fixture can read it back;
    // nothing inside this class needs it exposed for its own sake.
    [[nodiscard]] xdg_surface *xdg_surface_proxy() const noexcept { return m_xdg_surface; }

    // Reserved for fatia 8 (P-LOOP, docs/plano-w6b-placa-e-laco.md
    // sec. 14.2): `loop_hidden_test` minimizes a REAL toplevel via
    // xdg_toplevel_set_minimized() to prove D-W6b-6's own "janela
    // oculta nao computa nada" - this accessor is the one line that
    // makes the toplevel reachable from outside this class for that
    // purpose, the same "internal, never installed" visibility every
    // other adapter accessor in this file already has.
    [[nodiscard]] xdg_toplevel *toplevel() const noexcept { return m_xdg_toplevel; }

    // The three listeners' own callbacks (C function-pointer ABI).
    // PUBLIC ONLY so window_adapter.cpp's own anonymous-namespace
    // listener constants can take their address from outside the class -
    // same shape and same GODS_LAWS.md L-19 reasoning wayland_display_
    // adapter::registry_global()/wayland_shell_adapter::xdg_wm_base_ping()
    // already document (this whole class is itself internal, never
    // under include/glintfx/).
    //
    // wl_surface_enter()/wl_surface_leave() are DELIBERATE NO-OPS,
    // NAMED rather than omitted (docs/plano-w6a-janela.md fatia 8,
    // D-W6a-17's own header comment on fatia 4): tracking WHICH output
    // a surface is on is WL-SCALE's job (fractional scale needs it), a
    // later fatia this one does not reach into.
    static void wl_surface_enter(void *data, wl_surface *surface, wl_output *output) noexcept;
    static void wl_surface_leave(void *data, wl_surface *surface, wl_output *output) noexcept;

    // wl_surface.preferred_buffer_scale (v6, D-W6a-17): feeds window_
    // state::apply_buffer_scale() directly - the ONE factor pixel_
    // size()'s own formula (window_state.hpp) needs from this side.
    static void wl_surface_preferred_buffer_scale(void *data, wl_surface *surface,
                                                  std::int32_t factor) noexcept;

    // wl_surface.preferred_buffer_transform (v6, wl_surface's own
    // sibling event to preferred_buffer_scale) is a DELIBERATE NO-OP,
    // NAMED rather than nullptr - same "out of scope, but never a null
    // C function pointer" reasoning xdg_toplevel_configure_bounds()/
    // xdg_toplevel_wm_capabilities() above now document: wl_surface
    // created via create_surface() inherits wl_compositor's own bound
    // version (shell_adapter.cpp binds it at 6, same as xdg_wm_base),
    // so the compositor is entitled to send this event - measured
    // against tests/container/'s own kwin_wayland --virtual right
    // after the wm_capabilities fix above, same "listener function for
    // opcode N of INTERFACE is NULL" abort, this time opcode 3 of wl_
    // surface. Decision 15 (docs/plano-w6a-janela.md) still tracks
    // scale, never transform - this callback stores nothing, only
    // refuses to be null.
    static void wl_surface_preferred_buffer_transform(void *data, wl_surface *surface,
                                                      std::uint32_t transform) noexcept;

    // xdg_toplevel.configure carries width/height/states but no
    // serial; xdg_surface.configure (below) carries the serial but no
    // size - this callback only CACHES the pending width/height/states
    // (m_pending_*), applied together with the serial once xdg_surface.
    // configure fires (xdg-shell.xml's own "configure sequence"
    // paragraph: xdg_surface.configure marks the end of the sequence
    // and is what a client acks).
    static void xdg_toplevel_configure(void *data, xdg_toplevel *toplevel, std::int32_t width,
                                       std::int32_t height, wl_array *states) noexcept;

    // The one-way close latch (D-W5-3/window_state.hpp's own
    // request_close()) - a click on client-side decorations or a
    // compositor-issued close request, whichever the real desktop
    // shell sends.
    static void xdg_toplevel_close(void *data, xdg_toplevel *toplevel) noexcept;

    // xdg_toplevel.configure_bounds (v4) and xdg_toplevel.wm_
    // capabilities (v5) are DELIBERATE NO-OPS, NAMED rather than left
    // out of the listener struct - same reasoning wl_surface_enter()/
    // wl_surface_leave() above already document, but here the "named,
    // not omitted" choice is not optional: shell_adapter.cpp binds
    // xdg_wm_base at version 6 (shell_adapter.hpp's own header
    // comment), and every xdg_toplevel this fatia creates inherits
    // that same bound version (a child object's version is capped by
    // the interface it was created from, xdg-shell.xml's own
    // versioning rule) - so a compositor is entitled to send either
    // event, and wm_capabilities is not even optional: the protocol
    // text on this event itself says "Compositors must send this
    // event once before the first xdg_surface.configure event." A
    // NULL entry here is not "this fatia does not track it", it is a
    // NULL C function pointer libwayland-client's own dispatcher
    // calls the moment that event arrives - wl_closure_invoke() has no
    // safe way to skip an event with a null listener slot, so it logs
    // "listener function for opcode N of xdg_toplevel is NULL" and
    // calls abort() (SIGABRT), measured for wm_capabilities against
    // tests/container/'s own kwin_wayland --virtual compositor on
    // window_smoke's very first xdg_toplevel it ever creates. Neither
    // callback stores anything (D-W5-3: "v1 congela so o minimo" - the
    // scope this fatia tracks stays exactly the same, window_state
    // never gains a bounds/capabilities field from this), only
    // refuses to be null.
    static void xdg_toplevel_configure_bounds(void *data, xdg_toplevel *toplevel,
                                              std::int32_t width, std::int32_t height) noexcept;
    static void xdg_toplevel_wm_capabilities(void *data, xdg_toplevel *toplevel,
                                             wl_array *capabilities) noexcept;

    // Applies the cached toplevel configure through window_
    // configure_sequence::apply_configure() (window_configure_
    // sequence.hpp: this is the ONE call site where the raw
    // XDG_TOPLEVEL_STATE_* integers the generated header names and the
    // plain integers that file hardcodes have to agree - see that
    // header's own comment), writes the result into window_state, and
    // marks the pending serial ready for wait_first_configure() to ack.
    static void xdg_surface_configure(void *data, xdg_surface *surface,
                                      std::uint32_t serial) noexcept;

  private:
    [[nodiscard]] gltfx_rslt<void> create_surface(wayland_shell_adapter &shell) noexcept;
    [[nodiscard]] gltfx_rslt<void> create_xdg_surface(wayland_shell_adapter &shell) noexcept;
    [[nodiscard]] gltfx_rslt<void> create_toplevel() noexcept;
    [[nodiscard]] gltfx_rslt<void> apply_desc(const wayland_window_desc &desc) noexcept;
    void commit_initial() noexcept;
    [[nodiscard]] gltfx_rslt<void>
    wait_first_configure(wayland_display_adapter &connection) noexcept;

    wl_surface *m_surface = nullptr;
    xdg_surface *m_xdg_surface = nullptr;
    xdg_toplevel *m_xdg_toplevel = nullptr;

    window_state m_state;
    window_configure_sequence m_configure_sequence;
    bool m_configured = false;

    // Cached between xdg_toplevel.configure and xdg_surface.configure
    // (see xdg_toplevel_configure()'s own comment above). 16 slots: the
    // xdg-shell.xml `state` enum defines codes 0..13 today (xdg_
    // toplevel_state_maximized=1 .. constrained_bottom=13,
    // window_configure_sequence.hpp's own comment already names the
    // three this project tracks); a compositor is not expected to ever
    // repeat a code within one event, so 16 is headroom, not a tight
    // fit. A states array LONGER than this silently truncates the
    // extra entries rather than allocating in a noexcept C callback -
    // GODS_LAWS.md L-22, the same "never allocate from a callback with
    // no exception-handling contract" reasoning global_catalog::
    // insert()'s own header comment already documents for registry_
    // global().
    std::uint32_t m_pending_width = 0;
    std::uint32_t m_pending_height = 0;
    std::array<std::int32_t, 16> m_pending_states{};
    std::size_t m_pending_states_count = 0;
};

} // namespace glintfx::platform
