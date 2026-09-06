// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cassert>

#include <glintfx/core/err.hpp>
#include <glintfx/platform/window/display.hpp>

#include "platform/port/display_backend_port.hpp"
#include "platform/port/display_connection.hpp"
#include "platform/window/display_impl.hpp"

// display_facade.cpp - W-D' (docs/plano-w6a-janela.md fatia 6): the
// ONE translation unit that knows what glintfx::display_impl actually
// IS - the exact same "allocation and deallocation on the SAME side of
// the boundary" shape docs/api-conventions.md R5 already gives
// glintfx::gltfx_err's own m_context, applied here to a whole display
// connection. Every other TU in this library, and every consumer,
// only ever sees the opaque glintfx::display_impl declared in
// include/glintfx/platform/window/display.hpp.
//
// display_impl.hpp (not this file - moved there by WL-WINDOW-HANDLE so
// window_facade.cpp can share the same definition) is where display_
// impl's own layout actually lives now: on Wayland it composes the
// connection WITH an already-open wayland_shell_adapter (gltfx_window::
// open() needs both); on Windows it is still only the raw connection -
// see that header's own comment for the full reasoning. Neither half
// of that composition changes what THIS file's own static_assert
// checks: platform::display_connection<platform::selected_display_
// adapter> alone still has to satisfy platform::display_backend_port
// (open/close/is_open/pump_events) regardless of what else display_
// impl carries alongside it - the only thing gltfx_display's own v1
// surface needs from the connection itself.
//
// selected_display_adapter CHOSEN BY #if defined(_WIN32), NOT BY
// CMAKE SUBDIRECTORY SELECTION: this file itself is compiled on every
// platform (src/platform/window/'s own CMakeLists.txt adds it
// unconditionally, the same reasoning window_state.cpp/window_desc_
// validation.cpp/utf8_validation.cpp already document), so it needs
// the SAME per-platform branch tests/header_hygiene_test.cpp and
// src/platform/win32/display_adapter.hpp already use for exactly this
// reason - GODS_LAWS.md L-04's "so(UNIX)/elseif(WIN32)" shape src/
// platform/CMakeLists.txt itself already commits to, mirrored here at
// the preprocessor level because a SINGLE TU, not a directory, is what
// needs to pick a side this time.

namespace glintfx {

namespace {

static_assert(platform::display_backend_port<platform::selected_display_adapter>,
              "selected_display_adapter must satisfy display_backend_port - GODS_LAWS.md "
              "L-19/L-40: gltfx_display's own facade wraps it directly, and a selection that "
              "does not satisfy pump_events() has to fail to COMPILE here, never link a display "
              "handle whose pump_events() silently does nothing");

} // namespace

gltfx_rslt<gltfx_display> gltfx_display::open() noexcept {
    // FACADE-PIN (docs/plano-conserto-fachadas-uaf.md sec. 7.3/7.4):
    // impl is allocated FIRST, default-constructed (connection - and,
    // on Wayland, shell - both start closed) and THEN opened AT ITS
    // FINAL ADDRESS. There is no longer an adapter built on the stack
    // and moved into this allocation afterward: platform::display_
    // connection<A> lost connect()/its own move members for exactly
    // this reason (that header's own comment) - the sequence below is
    // what replaces "build by value, move in" with "allocate the home,
    // open in place".
    auto *impl = new (std::nothrow) display_impl{};
    if (impl == nullptr) {
        return gltfx_rslt<gltfx_display>::err(gltfx_err(gltfx_err_code::out_of_memory));
    }

    if (const gltfx_rslt<void> connected = impl->connection.open(); connected.has_error()) {
        delete impl;
        return gltfx_rslt<gltfx_display>::err(connected.error());
    }

#if !defined(_WIN32)
    // WL-WINDOW-HANDLE: gltfx_window::open() (window_facade.cpp) needs
    // an already-open wayland_shell_adapter to build a window on top of
    // (window_smoke.cpp's own composition, tests/container/, is the
    // measured proof this ordering - connection, then shell, then
    // window - is real) - opened here, once, right after the
    // connection, so a consumer that never opens a gltfx_window never
    // even notices the extra Wayland-only step (display_impl.hpp's own
    // header comment). Torn down before returning on failure, same
    // "whatever this call already created is destroyed" contract every
    // open() in this project already documents.
    if (const gltfx_rslt<void> shell_opened = impl->shell.open(impl->connection.adapter());
        shell_opened.has_error()) {
        delete impl;
        return gltfx_rslt<gltfx_display>::err(shell_opened.error());
    }
#endif

    return gltfx_rslt<gltfx_display>::ok(gltfx_display(impl));
}

gltfx_display::gltfx_display(gltfx_display &&other) noexcept : m_impl(other.m_impl) {
    other.m_impl = nullptr;
}

gltfx_display &gltfx_display::operator=(gltfx_display &&other) noexcept {
    if (this != &other) {
        delete m_impl;
        m_impl = other.m_impl;
        other.m_impl = nullptr;
    }
    return *this;
}

gltfx_display::~gltfx_display() { delete m_impl; }

bool gltfx_display::is_open() const noexcept {
    return m_impl != nullptr && m_impl->connection.is_open();
}

gltfx_rslt<void> gltfx_display::pump_events() noexcept {
    // Precondition: this gltfx_display was not moved-from (docs/api-
    // conventions.md's own precondition-violation category, the same
    // shape gltfx_rslt<T>::value()/error() already document - a
    // moved-from display has nothing left to pump, and this is caller
    // misuse, not a recoverable library-side failure with a
    // gltfx_err_code that would name it). Debug catches this with a
    // named message before touching m_impl; Release has the guard
    // compile away, same "zero cost, still not a promise" contract
    // that precondition already carries project-wide.
    assert(m_impl != nullptr &&
           "gltfx_display::pump_events() called on a moved-from display - the object no longer "
           "owns a connection");
    return m_impl->connection.adapter().pump_events();
}

// display_internal_access::get() - the ONLY definition of this symbol
// in the whole library (display.hpp's own "WHY get() IS DECLARED HERE
// BUT DEFINED ONLY IN display_facade.cpp" paragraph explains why the
// definition living HERE, out-of-line, is what makes the passkey work
// at all: a consumer's translation unit sees only the declaration in
// the public header, never this body, so it cannot compile the access
// itself - it can only ask the LINKER for a symbol this project
// deliberately never exports). No GLINTFX_API on this line: the same
// per-symbol dllexport convention this project's own header comments
// document elsewhere means this stays out of the dynamic symbol table
// of the public glintfx::glintfx target.
display_impl *display_internal_access::get(gltfx_display &display) noexcept {
    return display.m_impl;
}

} // namespace glintfx
