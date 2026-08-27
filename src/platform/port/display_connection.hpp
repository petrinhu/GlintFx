// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <utility>

#include <glintfx/core/err.hpp>

#include "platform/port/display_connection_port.hpp"

// display_connection.hpp - ARCH-PORTS: the ONE type any caller inside
// glintfx (WL-DISPLAY and everything built on top of it) programs
// against - never a concrete adapter directly. This is the whole point
// of having a port at all: swap the template argument for
// fake_display_adapter in a unit test, or for a future Win32 adapter,
// and every observable behavior of THIS type - open failed, open
// succeeded then closed, moved-from no longer owns anything - stays
// identical. tests/display_connection_fake_test.cpp is the proof: it
// runs the SAME sequence of calls against display_connection<fake_
// display_adapter> that a real caller would run against display_
// connection<wayland_display_adapter>, and nothing in this file's own
// code needs to know or care which one it got.
//
// RAII, named-constructor idiom (GODS_LAWS.md L-22): connect() is a
// FALLIBLE static factory that returns gltfx_rslt<display_connection>,
// never a bare constructor that could fail. A bare constructor has
// exactly two ways to report "the connection did not open" - throw (an
// exception this project's internal code is free to raise, but which
// display_connection is explicitly designed to sit BELOW a noexcept
// public-API wrapper someday, so a throwing constructor here would just
// push the try/catch onto every future caller instead of paying for it
// once) or leave a half-built object alive for the caller to
// misuse - and this design refuses to hand out either. A caller either
// gets a display_connection that is genuinely open, or gets a
// gltfx_err and no object at all.
namespace glintfx::platform {

template <typename A>
    requires display_connection_port<A>
class display_connection {
  public:
    // Default-constructs the adapter, opens it, and on success wraps
    // the NOW-OPEN adapter in a display_connection the caller owns. On
    // failure, adapter.open()'s own gltfx_err is returned UNCHANGED
    // (GODS_LAWS.md L-22: "o erro injetado chega ao chamador intacto" -
    // this factory never re-codes, wraps or replaces it) and no
    // display_connection is ever constructed at all.
    [[nodiscard]] static gltfx_rslt<display_connection> connect() noexcept {
        A adapter;
        const gltfx_rslt<void> opened = adapter.open();
        if (opened.has_error()) {
            return gltfx_rslt<display_connection>::err(opened.error());
        }
        return gltfx_rslt<display_connection>::ok(display_connection(std::move(adapter)));
    }

    // Move-only: a display_connection OWNS the one open connection its
    // adapter holds, and closing it twice (once for the original, once
    // for a copy) is exactly the kind of double-free/double-close bug
    // ownership types exist to make unrepresentable. The moved-from
    // side ends up holding a moved-from A - is_open() on it reads
    // false, because a well-formed adapter's own move constructor
    // steals its underlying handle (wayland_display_adapter and
    // fake_display_adapter both do; a movable() adapter that failed to
    // do this would be a bug in THAT adapter, not in this wrapper).
    display_connection(const display_connection &) = delete;
    display_connection &operator=(const display_connection &) = delete;

    display_connection(display_connection &&other) noexcept
        : m_adapter(std::move(other.m_adapter)) {}

    display_connection &operator=(display_connection &&other) noexcept {
        if (this != &other) {
            close_if_open();
            m_adapter = std::move(other.m_adapter);
        }
        return *this;
    }

    // Closes on scope exit, unconditionally attempted but harmless
    // when already closed (close_if_open() checks is_open() first) -
    // covers both the normal case and the moved-from case above.
    ~display_connection() { close_if_open(); }

    [[nodiscard]] bool is_open() const noexcept { return m_adapter.is_open(); }

  private:
    explicit display_connection(A adapter) noexcept : m_adapter(std::move(adapter)) {}

    void close_if_open() noexcept {
        if (m_adapter.is_open()) {
            m_adapter.close();
        }
    }

    A m_adapter;
};

} // namespace glintfx::platform
