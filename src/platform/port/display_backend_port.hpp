// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <concepts>
#include <cstdint>

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
//
// wait_events() - LOOP-RUN fatia 7 (docs/plano-w6b-fatias-6-8.md,
// D-W6b-50): the SAME refinement pump_events() itself was, one fatia
// later - this concept grows again, in place, rather than a THIRD
// sibling concept, because every caller that already needs "bombeia
// sem esperar" (pump_events()) is also the caller LOOP-RUN's own
// wait_events(budget_ms) is for: gltfx_loop::step() (a LATER fatia,
// platform/loop/loop.hpp) is the one place both are ever called from,
// never two different consumers each needing only one half. `budget_ms
// == 0` is DEFINED to be the exact same request pump_events() already
// makes (D-W6b-50's own text: "budget_ms == 0 e' pump_events(), o
// mesmo atomo") - a concrete adapter is free to implement pump_
// events() as wait_events(0) with the bool result discarded, the SAME
// "narrow the new capability lands on the wider one" shape this
// header's own top comment already used for pump_events() over
// display_connection_port.
//
// RETURNS gltfx_rslt<bool>, NOT gltfx_rslt<void> (unlike pump_events()
// above): "did at least one event actually arrive and get dispatched
// before the budget ran out" is the one new fact a caller of THIS
// method needs that a caller of pump_events() never did - LOOP-RUN's
// own D-W6b-44 step 3 reads it to decide whether the oculto-wait tick
// woke up because something happened or because the budget simply
// expired. `false` is never an error (an exhausted budget with
// nothing to report is the ordinary, expected outcome of calling this
// on an idle connection) - only a genuinely dead connection reports
// through the err() channel, same as pump_events() already does.
namespace glintfx::platform {

template <typename A>
concept display_backend_port = display_connection_port<A> && requires(A &backend) {
    { backend.pump_events() } noexcept -> std::same_as<gltfx_rslt<void>>;
    { backend.wait_events(std::uint32_t{0}) } noexcept -> std::same_as<gltfx_rslt<bool>>;
};

} // namespace glintfx::platform
