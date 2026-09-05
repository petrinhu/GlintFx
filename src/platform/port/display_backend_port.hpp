// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <concepts>

#include <glintfx/core/err.hpp>

#include "platform/port/display_connection_port.hpp"

// display_backend_port.hpp - W-D' (docs/plano-w6a-janela.md fatia 6):
// the SIBLING concept display_connection_port.hpp's own header comment
// promised - "whatever a caller needs beyond 'is a connection open'
// ... is WL-DISPLAY's own concern, and grows as a SIBLING concept next
// to this one when that fatia needs it - never bolted onto this one
// 'because it fits'". This is that sibling, grown for exactly one new
// need: the type display_connection<A> WRAPS to become the public
// gltfx_display (include/glintfx/platform/window/display.hpp) has to
// answer pump_events() too, the operation D-W5-1's own decision names
// as "o unico jeito que deixa LOOP-RUN bombear uma vez por quadro" - a
// display that only opened and closed would give a consumer's main
// loop nothing to call every frame.
//
// A REFINEMENT, NOT A REPLACEMENT: display_backend_port REQUIRES
// display_connection_port - every type that satisfies this concept
// already satisfies the narrower one, so display_connection<A> keeps
// working unchanged for a backend type, the ONLY thing this header
// adds is one more capability a caller reaching through display_
// connection<A>::adapter() (D-W5-10, display_connection.hpp) can rely
// on existing.
//
// WHAT "BACKEND" MEANS HERE, DISTINCT FROM THE RAW CONNECTION ADAPTER:
// on today's tree, platform::selected_display_adapter (wayland_
// display_adapter / win32_display_adapter) already satisfies this
// concept on Linux (WL-DISPLAY fatia D already shipped pump_events())
// and is EXPECTED to on Windows only once WIN-DISPLAY's own pump_
// events() lands (docs/plano-w6a-janela.md fatia 7/X-1', a sibling
// fatia of this one, in flight concurrently) - a build compiling
// gltfx_display's facade against a Windows selected_display_adapter
// that has not yet grown pump_events() is SUPPOSED to fail to compile
// here, loudly, the same GODS_LAWS.md L-19/L-40 "the build breaks
// instead of linking something incomplete" contract src/platform/
// wayland/selected_display_adapter_check.cpp's own static_assert
// already gives display_connection_port. A LATER backend (docs/plano-
// w6a-janela.md's own wayland_display_backend, composing connection +
// shell + seat) is free to be a DIFFERENT concrete type that ALSO
// satisfies this same concept - display_connection<A>/gltfx_display's
// own PIMPL means that swap needs no change to this file nor to the
// public header at all.
//
// noexcept, same reasoning display_connection_port.hpp's own header
// comment already gives for open(): every adapter this project ships
// wraps a plain C API reporting failure through a return value, never
// a C++ exception.

namespace glintfx::platform {

template <typename A>
concept display_backend_port = display_connection_port<A> && requires(A &backend) {
    { backend.pump_events() } noexcept -> std::same_as<gltfx_rslt<void>>;
};

} // namespace glintfx::platform
