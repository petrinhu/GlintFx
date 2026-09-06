// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/core/err.hpp>

#include "platform/port/display_connection_port.hpp"

// display_connection.hpp - ARCH-PORTS: the ONE type any caller inside
// glintfx (WL-DISPLAY and everything built on top of it) programs
// against - never a concrete adapter directly. This is the whole point
// of having a port at all: swap the template argument for
// fake_display_adapter in a unit test, or for a future Win32 adapter,
// and every observable behavior of THIS type - open failed, open
// succeeded then closed - stays identical. tests/display_connection_
// fake_test.cpp is the proof: it runs the SAME sequence of calls
// against display_connection<fake_display_adapter> that a real caller
// would run against display_connection<wayland_display_adapter>, and
// nothing in this file's own code needs to know or care which one it
// got.
//
// FACADE-PIN, OPENS IN PLACE, NEVER BY VALUE (docs/plano-conserto-
// fachadas-uaf.md sec. 6/7.3, GODS_LAWS.md L-22): connect() - a
// fallible static factory that default-constructed the adapter on the
// STACK, opened it there, then moved the NOW-OPEN adapter into the
// display_connection it returned by value - is GONE. That move was
// exactly the class of defect this fatia exists to close: the adapter
// had already handed its own address to the operating system (T0, this
// plan's own measurement) before ever being moved. display_connection_
// port now requires pinned_adapter<A> (platform/port/pinned_adapter.
// hpp), which FORBIDS the type this factory needed to move. open()
// below replaces it: a caller default-constructs display_connection<A>
// IN PLACE (inside its own already-allocated opaque handle - display_
// facade.cpp's own header comment shows the exact sequence), then
// calls open() on THAT object, at the address it will live at for the
// rest of its life. The promise open() keeps is unchanged from
// connect()'s own: adapter.open()'s own gltfx_err is returned
// UNCHANGED (GODS_LAWS.md L-22: "o erro injetado chega ao chamador
// intacto") and the adapter is left exactly as A::open() itself left
// it on failure - there is no "half-built display_connection" here any
// more than there was with connect(), because default-construction
// (A's own default constructor, guaranteed closed by pinned_adapter's
// own std::default_initializable<A> requirement) can never fail.
namespace glintfx::platform {

template <typename A>
    requires display_connection_port<A>
class display_connection {
  public:
    // Starts closed (m_adapter default-constructed, is_open() reads
    // false) - the caller opens it in place with open() below, never
    // by receiving an already-open one from a factory.
    display_connection() noexcept = default;

    // Pinned, never movable NOR copyable (FACADE-PIN, D-UAF-1): a
    // display_connection OWNS the one open connection its adapter
    // holds, and closing it twice (once for the original, once for a
    // copy) is exactly the kind of double-free/double-close bug
    // ownership types exist to make unrepresentable - the same
    // reasoning this class already had for its deleted copy members.
    // Moving is deleted too, for the identical reason the adapter it
    // wraps is pinned: A itself no longer satisfies std::movable (it
    // cannot, under pinned_adapter<A>), so a display_connection<A>
    // that tried to move A would fail to compile at the member itself -
    // deleting the special member here makes that refusal explicit,
    // at the class that owns the decision, rather than an opaque
    // template-instantiation error one layer down.
    display_connection(const display_connection &) = delete;
    display_connection &operator=(const display_connection &) = delete;
    display_connection(display_connection &&) = delete;
    display_connection &operator=(display_connection &&) = delete;

    // Closes on scope exit, unconditionally attempted but harmless
    // when already closed (close_if_open() checks is_open() first).
    ~display_connection() { close_if_open(); }

    // Opens the adapter THIS object already owns, in place - never a
    // second object constructed elsewhere and moved in. adapter.open()'s
    // own gltfx_err is returned UNCHANGED on failure (GODS_LAWS.md
    // L-22, same promise connect() used to keep) - m_adapter is left
    // exactly as A::open() itself left it, closed, ready for a caller
    // to retry or to destroy this display_connection outright.
    [[nodiscard]] gltfx_rslt<void> open() noexcept { return m_adapter.open(); }

    [[nodiscard]] bool is_open() const noexcept { return m_adapter.is_open(); }

    // D-W5-10 (docs/plano-w6a-janela.md sec. 1): the ONE crack this
    // wrapper deliberately opens in its own opacity, and only for
    // internal callers - a future WL-WINDOW/WL-SEAT or this fatia's own
    // display_facade.cpp needs to reach the concrete adapter's OWN
    // surface beyond open/close/is_open (pump_events(), bind(), the
    // shell/seat this display composes) and display_connection<A> is
    // deliberately narrow (display_connection_port.hpp's own "porta
    // gorda" comment), so it never grows a forwarding method per
    // adapter capability. Both overloads exist because a caller with
    // only a `const display_connection&` still needs read access
    // (is_open() itself already does exactly this internally, through
    // m_adapter directly rather than through this accessor, because it
    // predates this fatia).
    [[nodiscard]] A &adapter() noexcept { return m_adapter; }
    [[nodiscard]] const A &adapter() const noexcept { return m_adapter; }

  private:
    void close_if_open() noexcept {
        if (m_adapter.is_open()) {
            m_adapter.close();
        }
    }

    A m_adapter;
};

} // namespace glintfx::platform
