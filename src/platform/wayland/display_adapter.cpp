// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/wayland/display_adapter.hpp"

#include <wayland-client.h>

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

} // namespace

wayland_display_adapter::wayland_display_adapter(wayland_display_adapter &&other) noexcept
    : m_display(other.m_display), m_registry(other.m_registry), m_globals(std::move(other.m_globals)) {
    other.m_display = nullptr;
    other.m_registry = nullptr;
}

wayland_display_adapter &
wayland_display_adapter::operator=(wayland_display_adapter &&other) noexcept {
    if (this != &other) {
        close();
        m_display = other.m_display;
        m_registry = other.m_registry;
        m_globals = std::move(other.m_globals);
        other.m_display = nullptr;
        other.m_registry = nullptr;
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

void wayland_display_adapter::close() noexcept {
    if (m_display != nullptr) {
        // Reverse order of creation (w4-plano.md sec. 1.2, "teardown
        // do SDL3 e ordem inversa da criacao"): the registry is an
        // object OWNED BY the connection, so it is destroyed before
        // the connection itself is disconnected.
        if (m_registry != nullptr) {
            wl_registry_destroy(m_registry);
            m_registry = nullptr;
        }
        wl_display_disconnect(m_display);
        m_display = nullptr;
        m_globals = global_catalog{};
    }
}

} // namespace glintfx::platform
