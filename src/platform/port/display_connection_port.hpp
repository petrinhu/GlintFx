// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <concepts>
#include <utility>

#include <glintfx/core/err.hpp>

// display_connection_port.hpp - ARCH-PORTS (TODO.md, GODS_LAWS.md
// L-19 item 2): the compile-time contract every display-server adapter
// (wayland_display_adapter today, a Win32 adapter later - WIN-WINDOW)
// has to satisfy so display_connection.hpp's own display_connection<A>
// can wrap ANY of them with zero runtime dispatch - no `virtual`, no
// function pointer, no std::variant of adapters. This is what "porta e
// concept de C++23, resolvida em compile-time" means in practice: the
// adapter is chosen by src/platform/CMakeLists.txt at CONFIGURE time
// (which directory even enters the build), and this concept is the
// thing that build-time choice is checked against.
//
// INTERNAL, NEVER PUBLIC (GODS_LAWS.md L-19 opacity clause does not
// even reach this file - it never crosses the library boundary at
// all): lives under src/platform/, not include/glintfx/. Nothing here
// is installed, nothing is exported as a dynamic symbol (both proven
// mechanically by tests/tools/check_port_privacy.sh, not just declared
// in this comment), and no adapter satisfying this concept carries any
// global or static state (the third leg of the CTO plan's "gatilho de
// parada" - reviewed by the adversarial reviewer, not machine-checked,
// same as any other absence-of-a-thing property).
//
// DELIBERATELY NARROW (GODS_LAWS.md L-19 armadilha 2, "porta gorda"):
// open, close, is_open - nothing about a file descriptor, poll() or
// any other POSIX/Wayland-specific detail belongs on this SHARED
// contract, because a future Win32 adapter (WIN-WINDOW) has to satisfy
// it too, and Win32 has no file descriptor to wait on. Whatever a
// caller needs beyond "is a connection open" (registry, roundtrip, the
// event pump) is WL-DISPLAY's own concern, and grows as a SIBLING
// concept next to this one when that fatia needs it - never bolted
// onto this one "because it fits" (GODS_LAWS.md L-17).
//
// noexcept on open() itself, not only on close()/is_open(): every
// adapter this project ships wraps a plain C API (wl_display_connect,
// later a Win32 API call) that reports failure through a return value,
// never a C++ exception - there is nothing for open() to let escape.
// Keeping it noexcept here, at the CONTRACT level, means
// display_connection<A>::connect() (display_connection.hpp) can stay
// noexcept too, without a try/catch anywhere in this port - the same
// "noexcept at the boundary, exceptions permitted only strictly
// inside" shape gltfx_rslt<T> itself documents (GODS_LAWS.md L-22),
// applied one layer earlier because this specific class of adapter
// code never needs the exception half at all.
namespace glintfx::platform {

template <typename A>
concept display_connection_port =
    std::default_initializable<A> && std::movable<A> && requires(A &adapter) {
        { adapter.open() } noexcept -> std::same_as<gltfx_rslt<void>>;
        { adapter.close() } noexcept -> std::same_as<void>;
        // is_open() is checked through a CONST reference on purpose:
        // display_connection<A>::is_open() const (display_connection.hpp)
        // reads m_adapter through a const A&, so a port whose is_open()
        // only exists as a non-const member would compile display_
        // connection<A>'s OWN definition and then fail, confusingly, at
        // the call site inside is_open() const - catching that here
        // instead keeps the failure at the one place a reviewer already
        // expects to read it: the concept itself.
        { std::as_const(adapter).is_open() } noexcept -> std::same_as<bool>;
    };

} // namespace glintfx::platform
