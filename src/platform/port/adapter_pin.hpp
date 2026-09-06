// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <concepts>
#include <type_traits>

// adapter_pin.hpp - FACADE-PIN (docs/plano-conserto-fachadas-uaf.md
// sec. 6/7.1, GODS_LAWS.md L-19/L-36): the compile-time contract that
// replaces std::movable<A> in the three port concepts (display_
// connection_port.hpp, window_adapter_port.hpp, gl_context_adapter_
// port.hpp) - an adapter that hands its own address to the operating
// system (wl_proxy_add_listener/CreateWindowExW's lpParam) is no
// longer allowed to move away from the address it already handed out.
//
// THE DEFECT THIS PORTAO CLOSES (docs/plano-conserto-fachadas-uaf.md
// sec. 1/2): every adapter under src/platform/wayland/ and src/
// platform/win32/ registers `this` with a C callback mechanism that
// reads it back later, at an unknown future time - a move that leaves
// the OLD address behind, with the system still pointing at it, is a
// use-after-free the compiler has no way to catch through std::
// movable<A> alone. `pinned_adapter<A>` makes the object's own
// immobility part of the CONTRACT a concrete adapter has to satisfy to
// even be SELECTABLE by a facade (display_facade.cpp/window_facade.cpp/
// gl_context_facade.cpp's own static_assert) - a future adapter that
// forgets to delete its own move members fails to COMPILE here, the
// same "the build breaks instead of linking something incomplete"
// contract GODS_LAWS.md L-19/L-40 already gives the narrower ports this
// widens.
//
// WHY THIS IS G1's OWN MECHANISM, AND WHY IT IS NOT ENOUGH BY ITSELF
// (docs/plano-conserto-fachadas-uaf.md sec. 9): this concept only
// reaches an adapter that sits BEHIND one of the three ports above -
// wayland_shell_adapter and wayland_seat_adapter (this plan's own
// varredura #2/#3) are never selected through a port at all, so this
// header alone would leave them uncovered. tests/tools/check_self_
// registration_pinned.py (G2, a lexical scan of src/platform/**) is
// the sibling portao that closes THAT gap - see this plan's own sec. 9
// table for the full division of labor between the two.
namespace glintfx::platform {

// default_initializable<A> is kept from the narrower concepts this
// replaces (display_connection_port.hpp's own reasoning still holds
// verbatim: a facade default-constructs the adapter INSIDE the
// already-allocated opaque handle, then opens it in place - see
// display_connection.hpp/window_facade.cpp/gl_context_facade.cpp's own
// header comments for the sequence this concept exists to make
// possible). The four negated traits are the whole of the new
// requirement: an adapter satisfying this concept can be constructed
// and destroyed, but never copied NOR moved, by any mechanism the
// language itself would reach for (a bare `= default`/`= delete`'d
// special member is exactly what these traits detect either way).
template <typename A>
concept pinned_adapter = std::default_initializable<A> && !std::is_copy_constructible_v<A> &&
                         !std::is_copy_assignable_v<A> && !std::is_move_constructible_v<A> &&
                         !std::is_move_assignable_v<A>;

} // namespace glintfx::platform
