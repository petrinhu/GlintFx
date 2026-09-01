// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

#include <glintfx/core/err.hpp>

#include "platform/wayland/global_catalog.hpp"

// display_adapter.hpp - ARCH-PORTS (open/close/is_open) EXTENDED by
// WL-DISPLAY fatia B (TODO.md, GODS_LAWS.md L-05): the same class now
// also owns the wl_registry created right after connecting, and the
// global_catalog (global_catalog.hpp) that registry's listener
// populates. This is the extension ARCH-PORTS's own header comment
// promised - same file, same directory, same src/platform/CMakeLists.txt
// branch - rather than a second, competing type.
//
// wl_display AND wl_registry both FORWARD-DECLARED, wayland-client.h
// NOT included here: the only thing this header needs to know about
// either is that a POINTER to one exists - both are opaque even in
// Wayland's own public API. Keeping <wayland-client.h> out of this
// header (it lives in display_adapter.cpp instead) means anything that
// only needs to know "a wayland_display_adapter exists and has this
// shape" - tests/display_port_concept_test.cpp among them - never has
// to put wayland-client's own include directory on its search path at
// all.
struct wl_display;
struct wl_registry;

namespace glintfx::platform {

class wayland_display_adapter {
  public:
    // Starts CLOSED (m_display null) - default_initializable<A>, the
    // first half of display_connection_port, requires exactly this: a
    // default-constructed adapter that is_open() reports false for,
    // ready for open() to be called on it.
    wayland_display_adapter() noexcept = default;

    // Move-only (display_connection_port requires std::movable<A>, not
    // copyable): copying a live wl_display handle would hand two
    // owners the same connection, and closing it twice is a
    // use-after-free in libwayland-client's own bookkeeping.
    wayland_display_adapter(const wayland_display_adapter &) = delete;
    wayland_display_adapter &operator=(const wayland_display_adapter &) = delete;

    wayland_display_adapter(wayland_display_adapter &&other) noexcept;
    wayland_display_adapter &operator=(wayland_display_adapter &&other) noexcept;

    // Closes whatever this adapter still owns - safe to run on a
    // moved-from or never-opened instance, because close() itself only
    // acts when m_display is non-null.
    ~wayland_display_adapter();

    // Attempts wl_display_connect(nullptr) (display_adapter.cpp: reads
    // the standard WAYLAND_DISPLAY/XDG_RUNTIME_DIR resolution every
    // Wayland client uses), then WL-DISPLAY fatia B: creates the
    // wl_registry, attaches this adapter's own listener, and performs
    // ONE wl_display_roundtrip() to close the initial burst of
    // `global` events every compositor emits right after get_registry
    // (w4-plano.md sec. 1.2/3.1.B) - by the time open() returns
    // successfully, globals() already holds every global that was
    // announced before this connection opened. GODS_LAWS.md L-22: any
    // refusal along this path (connect refused, get_registry refused,
    // the roundtrip itself failing) comes back as
    // gltfx_err_code::platform_failure through the ordinary
    // gltfx_rslt<void> return channel - no exception, no abort, no
    // process-ending signal for a condition this adapter's own caller
    // is expected to handle. A failed open() tears down whatever it
    // had already created (registry destroyed, display disconnected)
    // before returning - is_open() reads false either way.
    [[nodiscard]] gltfx_rslt<void> open() noexcept;

    // Idempotent-safe: does nothing when already closed (is_open() is
    // false), so both the destructor above and display_connection<A>'s
    // own close_if_open() (display_connection.hpp) can call this
    // unconditionally without checking first themselves. Tears down in
    // the REVERSE order open() built things in (w4-plano.md sec. 1.2,
    // "teardown do SDL3 e ordem inversa da criacao"): the registry -
    // an object the connection owns - is destroyed BEFORE the
    // connection itself is disconnected.
    void close() noexcept;

    [[nodiscard]] bool is_open() const noexcept { return m_display != nullptr; }

    // The catalog fatia A's global_catalog.hpp defines, populated by
    // the initial roundtrip above and kept current afterward by
    // global_remove events the (still out of scope for this fatia,
    // fatia D's own) event pump will dispatch. Read-only: nothing
    // outside this adapter is meant to insert or remove a global by
    // hand.
    [[nodiscard]] const global_catalog &globals() const noexcept { return m_globals; }

    // WL-DISPLAY fatia C: re-synchronizes with the compositor -
    // flushes any pending request, then blocks until every event
    // already in flight has been dispatched (wl_display_roundtrip's
    // own contract). Unlike the ONE roundtrip open() performs
    // internally to close the initial registry burst (fatia B), THIS
    // is the operation any later caller (WL-WINDOW, WL-SEAT) reaches
    // for whenever it needs to know a request has already been
    // processed by the compositor before moving on.
    //
    // FATAL IS FATAL (manpage wl_display(3), w4-plano.md sec. 3.1.C):
    // once ANY protocol operation on this connection returns -1, the
    // underlying wl_display becomes permanently unusable by
    // libwayland's own documented contract - calling anything else on
    // it is undefined behavior. This adapter LATCHES that fact
    // (has_fatal_error() below) the FIRST time it happens and never
    // calls into libwayland on this wl_display again afterward: every
    // subsequent call to this method short-circuits straight to
    // returning the SAME class of error, without a second real
    // roundtrip attempt on a connection already known to be dead.
    // GODS_LAWS.md L-22/CORE-ERROR decision 1: NEVER aborts - the
    // refusal always comes back through gltfx_rslt<void>.
    [[nodiscard]] gltfx_rslt<void> roundtrip() noexcept;

    // True once a protocol operation on this connection has returned
    // -1 (see roundtrip()'s own comment above). Deliberately SEPARATE
    // from is_open(): the connection HANDLE is still owned and still
    // needs close() to free it either way - "can this connection still
    // be used for anything" and "is a handle still owned" are two
    // different questions, and is_open()'s own contract (ARCH-PORTS,
    // display_connection_port) does not change under this fatia.
    [[nodiscard]] bool has_fatal_error() const noexcept { return m_fatal; }

    // WL-DISPLAY fatia D: the NON-BLOCKING event pump - safe to call
    // every frame from a consumer's own loop without ever stalling it,
    // unlike roundtrip() above (which blocks until the compositor
    // answers). Implements the canonical prepare/flush/poll/read
    // sequence wl_display(3) documents as the ONLY safe way to
    // integrate Wayland's own event delivery into a caller-owned loop
    // (w4-plano.md sec. 1.1/3.1.D):
    //   1. wl_display_prepare_read(), looping through wl_display_
    //      dispatch_pending() first whenever the pending queue is
    //      non-empty (prepare_read() itself refuses while it is);
    //   2. wl_display_flush() - on EAGAIN (kernel send buffer full,
    //      NOT a failure), poll() for POLLOUT and retry, never give up
    //      silently (w4-plano.md sec. 3.4 armadilha 6: an unflushed
    //      request under load would sit stuck forever otherwise);
    //   3. poll() the connection fd for POLLIN with timeout ZERO - "is
    //      there anything to read RIGHT NOW", never wait for it;
    //   4. nothing ready -> wl_display_cancel_read() (the mandatory
    //      pairing for a prepare_read() that found nothing); something
    //      ready -> wl_display_read_events(), then wl_display_dispatch_
    //      pending() to process it.
    // Every one of the FOUR documented deadlock traps this sequence
    // exists to avoid (blocking dispatch between prepare_read and
    // read_events/cancel_read; prepare_read without its matching
    // read_events/cancel_read; manual poll() before dispatch without
    // this whole pattern; a fatal error silently ignored) is named,
    // by the trap it avoids, in display_adapter.cpp's own comment on
    // this method's body.
    [[nodiscard]] gltfx_rslt<void> pump_events() noexcept;

    // registry_global()/registry_global_remove() are the wl_registry_
    // listener's own two callbacks (C function-pointer ABI). PUBLIC
    // ONLY so display_adapter.cpp's own anonymous-namespace
    // wl_registry_listener constant (`.global = &wayland_display_
    // adapter::registry_global`) can take their address from outside
    // the class - nothing else is meant to call these directly, and
    // this whole class is itself internal (src/platform/, never under
    // include/glintfx/), so this is not a GODS_LAWS.md L-19 "porta
    // gorda" concern the way it would be on the actual public API
    // surface.
    static void registry_global(void *data, wl_registry *registry, std::uint32_t name,
                                const char *interface, std::uint32_t version) noexcept;
    static void registry_global_remove(void *data, wl_registry *registry,
                                       std::uint32_t name) noexcept;

  private:
    // pump_events()'s own four documented steps (display_adapter.cpp's
    // header comment on pump_events() names each by the deadlock trap
    // it avoids), split into one private method per step - GODS_LAWS.md
    // L-17: pump_events() itself measured 47 real lines against this
    // project's 40-line ceiling, and the comment above it already
    // numbered the four steps by name, so the split follows exactly
    // that boundary rather than an arbitrary cut. Each method keeps the
    // m_fatal-latching and wl_display_cancel_read() pairing local to
    // the step that owns it; pump_events() itself is left as the
    // four-call sequence.
    [[nodiscard]] gltfx_rslt<void> drain_pending_and_prepare_read() noexcept;
    [[nodiscard]] gltfx_rslt<void> flush_with_retry() noexcept;
    // Returns whether data is ready to read (true) or nothing arrived
    // (false, wl_display_cancel_read() already called) - the one step
    // whose "nothing to do" outcome is success, not an error, which is
    // why this is the one of the four returning gltfx_rslt<bool> rather
    // than gltfx_rslt<void>.
    [[nodiscard]] gltfx_rslt<bool> wait_for_incoming_data() noexcept;
    [[nodiscard]] gltfx_rslt<void> read_and_dispatch_incoming() noexcept;

    wl_display *m_display = nullptr;
    wl_registry *m_registry = nullptr;
    global_catalog m_globals;
    bool m_fatal = false;
};

} // namespace glintfx::platform
