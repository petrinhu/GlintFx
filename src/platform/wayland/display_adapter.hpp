// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/core/err.hpp>

// display_adapter.hpp - ARCH-PORTS: wayland_display_adapter, the FIRST
// type to satisfy platform::display_connection_port
// (src/platform/port/display_connection_port.hpp). Scope frozen to
// exactly what this fatia owns (CTO plan sec. 1.2): open a real
// connection to the Wayland display server, close it. Nothing else -
// no registry, no roundtrip, no event pump. Those are WL-DISPLAY's own
// scope (TODO.md), and when that fatia lands it EXTENDS this same
// file (more methods on this same class, in the same directory,
// selected by the same src/platform/CMakeLists.txt branch) rather than
// reopening the port this fatia closes.
//
// wl_display FORWARD-DECLARED, wayland-client.h NOT included here: the
// only thing this header needs to know about wl_display is that a
// POINTER to one exists - the full type is opaque even in Wayland's
// own public API. Keeping <wayland-client.h> out of this header (it
// lives in display_adapter.cpp instead) means anything that only
// needs to know "a wayland_display_adapter exists and has this shape"
// - tests/display_port_concept_test.cpp among them - never has to put
// wayland-client's own include directory on its search path at all.
struct wl_display;

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
    // Wayland client uses). GODS_LAWS.md L-22: the system's refusal
    // (compositor absent, socket missing, permission denied) comes
    // back as gltfx_err_code::platform_failure through the ordinary
    // gltfx_rslt<void> return channel - no exception, no abort, no
    // process-ending signal for a condition this adapter's own caller
    // is expected to handle.
    [[nodiscard]] gltfx_rslt<void> open() noexcept;

    // Idempotent-safe: does nothing when already closed (is_open() is
    // false), so both the destructor above and display_connection<A>'s
    // own close_if_open() (display_connection.hpp) can call this
    // unconditionally without checking first themselves.
    void close() noexcept;

    [[nodiscard]] bool is_open() const noexcept { return m_display != nullptr; }

  private:
    wl_display *m_display = nullptr;
};

} // namespace glintfx::platform
