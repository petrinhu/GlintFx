// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/wayland/display_adapter.hpp"

#include <wayland-client.h>

#include <poll.h>

#include <cerrno>
#include <string>
#include <utility>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

// display_adapter.cpp - see display_adapter.hpp's own header comment
// for scope. ARCH-PORTS's own connect/disconnect (TDD case R3,
// GODS_LAWS.md L-20's declared exception: "adaptador que só encaminha
// chamada ao sistema operacional... o teste honesto é de integração e
// não unitário") is now EXTENDED by WL-DISPLAY fatia B with the
// registry and its listener - proven the same way ARCH-PORTS proved
// its own half: tests/display_connect_failure_test.cpp for the
// no-compositor refusal path (runnable on any Linux CI leg, unchanged
// by this fatia - the refusal happens at wl_display_connect(), before
// any registry code below is ever reached) and the container
// integration case (TEST-WLCONT, GODS_LAWS.md L-09) for the
// connect-to-a-real-compositor path, now also enumerating the globals
// a real compositor announces.
//
// registry_global()/registry_global_remove() below are called back by
// libwayland-client's own C event-dispatch machinery (wl_display_
// roundtrip(), later the pump fatia D adds) with a bare `void *data`
// it does nothing with except hand back unchanged - GODS_LAWS.md L-22:
// NO EXCEPTION may unwind across that C stack frame (global_catalog::
// insert() can throw std::bad_alloc via its std::string/std::vector
// members), so both callbacks wrap their own body in try/catch and
// degrade to "this one global did not get cataloged" rather than let
// anything escape into a translation unit that gave it no
// exception-handling contract at all.

namespace glintfx::platform {

namespace {

constexpr wl_registry_listener kRegistryListener = {
    .global = &wayland_display_adapter::registry_global,
    .global_remove = &wayland_display_adapter::registry_global_remove,
};

// WL-DISPLAY fatia C: builds the token-vocabulary diagnostic (docs/
// api-conventions.md R7, ESCOPO.md's own canonical copy - "name always
// a stable identifier, never a sentence") a fatal wl_display carries.
// wl_display_get_error() is the ONE libwayland call this file's own
// "never touch a fatally-errored display again" rule (display_adapter.hpp's
// roundtrip() comment) does NOT apply to - it is libwayland's own
// documented accessor for exactly this situation, read-only, never a
// protocol request. os_error_code() carries the raw errno-shaped value
// (EPROTO for a protocol violation, an ordinary errno like ECONNRESET
// for a transport failure); when it IS EPROTO, wl_display_get_
// protocol_error() additionally names WHICH interface's request the
// compositor rejected, attached as rejected_value() - an interface
// name ("wl_compositor") is itself already an identifier token, never
// a sentence.
gltfx_err build_fatal_error(wl_display *display) noexcept {
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

} // namespace

wayland_display_adapter::wayland_display_adapter(wayland_display_adapter &&other) noexcept
    : m_display(other.m_display), m_registry(other.m_registry), m_globals(std::move(other.m_globals)),
      m_fatal(other.m_fatal) {
    other.m_display = nullptr;
    other.m_registry = nullptr;
    other.m_fatal = false;
}

wayland_display_adapter &
wayland_display_adapter::operator=(wayland_display_adapter &&other) noexcept {
    if (this != &other) {
        close();
        m_display = other.m_display;
        m_registry = other.m_registry;
        m_globals = std::move(other.m_globals);
        m_fatal = other.m_fatal;
        other.m_display = nullptr;
        other.m_registry = nullptr;
        other.m_fatal = false;
    }
    return *this;
}

wayland_display_adapter::~wayland_display_adapter() { close(); }

void wayland_display_adapter::registry_global(void *data, wl_registry * /*registry*/,
                                               std::uint32_t name, const char *interface,
                                               std::uint32_t version) noexcept {
    auto *self = static_cast<wayland_display_adapter *>(data);
    try {
        self->m_globals.insert(name, interface != nullptr ? std::string(interface) : std::string(),
                                version);
    } catch (...) {
        // Best-effort (see this file's own header comment): a global
        // this catalog fails to record because std::string/std::vector
        // could not allocate is a degraded catalog, not a crash across
        // a C ABI boundary that gave it no exception contract.
    }
}

void wayland_display_adapter::registry_global_remove(void *data, wl_registry * /*registry*/,
                                                       std::uint32_t name) noexcept {
    auto *self = static_cast<wayland_display_adapter *>(data);
    self->m_globals.remove(name);
}

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

    wl_registry *registry = wl_display_get_registry(display);
    if (registry == nullptr) {
        wl_display_disconnect(display);
        return gltfx_rslt<void>::err(gltfx_err(gltfx_err_code::platform_failure));
    }

    wl_registry_add_listener(registry, &kRegistryListener, this);

    // ONE roundtrip closes the initial burst of `global` events every
    // compositor emits right after get_registry() (w4-plano.md
    // sec. 1.2/3.1.B, wayland-book's own registry/binding chapter) -
    // by the time this call returns successfully, m_globals holds
    // every global that was already known when this connection opened.
    // A -1 return here is this fatia's OWN open()-failure path only:
    // WL-DISPLAY fatia C is what latches the ADAPTER ITSELF into a
    // permanently-unusable state for a roundtrip failing on an
    // ALREADY-open connection - this call happens strictly before
    // m_display is ever assigned below, so open() simply reports
    // failure and tears down what it had already created, the same
    // shape a refused connect() or get_registry() above already has.
    if (wl_display_roundtrip(display) == -1) {
        wl_registry_destroy(registry);
        wl_display_disconnect(display);
        return gltfx_rslt<void>::err(gltfx_err(gltfx_err_code::platform_failure));
    }

    m_display = display;
    m_registry = registry;
    return gltfx_rslt<void>::ok();
}

gltfx_rslt<void> wayland_display_adapter::roundtrip() noexcept {
    if (!is_open()) {
        return gltfx_rslt<void>::err(gltfx_err(gltfx_err_code::invalid_argument));
    }
    if (m_fatal) {
        // Already latched by an EARLIER call: report the SAME
        // diagnostic without touching m_display again (see this
        // method's own header comment - "never a second real
        // roundtrip attempt on a connection already known to be
        // dead"). wl_display_get_error() itself is always safe to
        // call, even here - see build_fatal_error()'s own comment.
        return gltfx_rslt<void>::err(build_fatal_error(m_display));
    }
    if (wl_display_roundtrip(m_display) == -1) {
        m_fatal = true;
        return gltfx_rslt<void>::err(build_fatal_error(m_display));
    }
    return gltfx_rslt<void>::ok();
}

gltfx_rslt<void> wayland_display_adapter::pump_events() noexcept {
    if (!is_open()) {
        return gltfx_rslt<void>::err(gltfx_err(gltfx_err_code::invalid_argument));
    }
    if (m_fatal) {
        return gltfx_rslt<void>::err(build_fatal_error(m_display));
    }

    // ARMADILHA 3 (manpage, w4-plano.md sec. 1.1): prepare_read()
    // itself REFUSES (returns nonzero) while another thread already
    // has a read prepared, or while THIS thread's own pending queue
    // still has undispatched events in it - draining that queue first
    // is exactly what the loop below does, and it is why this comes
    // BEFORE poll(), never a bare poll() first.
    while (wl_display_prepare_read(m_display) != 0) {
        if (wl_display_dispatch_pending(m_display) == -1) {
            m_fatal = true;
            return gltfx_rslt<void>::err(build_fatal_error(m_display));
        }
    }

    // ARMADILHA 6 (w4-plano.md sec. 3.4): EAGAIN here means the
    // KERNEL's own socket send buffer is full, not that flush()
    // failed - waiting for POLLOUT and retrying is the only correct
    // response; giving up would leave an outgoing request stuck
    // forever the first time this ran under real load, while every
    // test that never fills the buffer would keep passing.
    while (wl_display_flush(m_display) == -1) {
        if (errno != EAGAIN) {
            // ARMADILHA 2 (prepare_read without its matching read_
            // events/cancel_read is a deadlock the NEXT time this is
            // called): cancel_read() here is that mandatory pairing
            // for the failure path.
            wl_display_cancel_read(m_display);
            m_fatal = true;
            return gltfx_rslt<void>::err(build_fatal_error(m_display));
        }
        pollfd pending_write{.fd = wl_display_get_fd(m_display), .events = POLLOUT, .revents = 0};
        if (poll(&pending_write, 1, -1) == -1) {
            wl_display_cancel_read(m_display);
            m_fatal = true;
            return gltfx_rslt<void>::err(build_fatal_error(m_display));
        }
    }

    // NON-BLOCKING BY DEFAULT (w4-plano.md sec. 3.0/3.1.D): timeout
    // ZERO asks "is there anything to read RIGHT NOW", never waits for
    // it - this is the whole point of a pump a consumer calls every
    // frame from its own loop. ARMADILHA 1 (manpage): a BLOCKING
    // dispatch call between prepare_read and read_events/cancel_read
    // is a deadlock - poll() with a bounded timeout is deliberately
    // NOT that; it is the one call this sequence is allowed to wait
    // on, and here it does not even wait.
    pollfd incoming{.fd = wl_display_get_fd(m_display), .events = POLLIN, .revents = 0};
    const int poll_result = poll(&incoming, 1, 0);
    if (poll_result == -1) {
        wl_display_cancel_read(m_display);
        m_fatal = true;
        return gltfx_rslt<void>::err(build_fatal_error(m_display));
    }
    if (poll_result == 0 || (incoming.revents & POLLIN) == 0) {
        // Nothing arrived: ARMADILHA 2 again, the "found nothing" half
        // of the same pairing requirement - cancel_read() releases the
        // prepared-read state cleanly instead of leaving it dangling
        // for the NEXT call to trip over.
        wl_display_cancel_read(m_display);
        return gltfx_rslt<void>::ok();
    }

    if (wl_display_read_events(m_display) == -1) {
        // ARMADILHA 4 (manpage: "erros sao fatais"): a failed read_
        // events() here is exactly the same class of unusable-display
        // condition roundtrip() latches above - the DIFFERENT call
        // site does not make it a different kind of failure.
        m_fatal = true;
        return gltfx_rslt<void>::err(build_fatal_error(m_display));
    }

    if (wl_display_dispatch_pending(m_display) == -1) {
        m_fatal = true;
        return gltfx_rslt<void>::err(build_fatal_error(m_display));
    }

    return gltfx_rslt<void>::ok();
}

void wayland_display_adapter::close() noexcept {
    if (m_display != nullptr) {
        // Reverse order of creation (w4-plano.md sec. 1.2, "teardown
        // do SDL3 e ordem inversa da criacao"): the registry is an
        // object OWNED BY the connection, so it is destroyed before
        // the connection itself is disconnected. Safe to do even on a
        // fatally-errored display (m_fatal true): wl_registry has no
        // "destroy" REQUEST in the protocol at all - wl_registry_
        // destroy() only frees this process's own local proxy memory,
        // it never sends anything over the (possibly already dead)
        // wire, and wl_display_disconnect() itself is documented safe
        // to call on an errored display - it only releases local
        // resources (the socket fd among them), never sends a further
        // protocol request either.
        if (m_registry != nullptr) {
            wl_registry_destroy(m_registry);
            m_registry = nullptr;
        }
        wl_display_disconnect(m_display);
        m_display = nullptr;
        m_globals = global_catalog{};
        m_fatal = false;
    }
}

} // namespace glintfx::platform
