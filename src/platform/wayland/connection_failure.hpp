// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/core/err.hpp>

// platform/wayland/connection_failure.hpp - GL-CONTEXT fatia 5 (docs/
// plano-w6b-fatias-5.md, D-W6b-28, GODS_LAWS.md L-17/L-33): the ONE
// atom that turns "this wl_display connection just died" into a
// gltfx_err naming WHICH interface's request the compositor rejected -
// display_adapter.cpp's own build_fatal_error() (WL-DISPLAY fatia C,
// namespace-anonymous there since it had exactly one caller) MOVED
// here, verbatim, the moment a SECOND caller appeared: F2 of this
// fatia's own plan measured wayland_egl_context_adapter reporting a
// dead connection as `rejected_value() == "wl_display"` instead of the
// real interface that reprovou, because it had no way to reach
// display_adapter.cpp's own anonymous-namespace function. Extracted,
// never copied (GODS_LAWS.md L-33's own "regra de 3" does not apply
// here - two callers that both need the IDENTICAL logic, not similar
// logic, is exactly the case the mutation test for this fatia proves:
// egl_protocol_error_smoke.cpp asserts the SAME rejected_value shape
// this atom already gave display_adapter.cpp's own callers).
//
// wl_display FORWARD-DECLARED, <wayland-client.h> NOT included here -
// the same reasoning display_adapter.hpp/egl_context_adapter.hpp
// already document one file over: a pointer is all this header needs,
// and the real wl_display_get_error()/wl_display_get_protocol_error()
// calls live entirely in connection_failure.cpp, where the real header
// is.
struct wl_display;

namespace glintfx::platform {

// Reads the CURRENT state of `display` (never touches the connection
// itself - wl_display_get_error() is libwayland's own documented,
// read-only accessor, safe to call even on a connection this caller
// already knows is fatally errored) and builds the token-vocabulary
// diagnostic (docs/api-conventions.md R7: "name always a stable
// identifier, never a sentence") a dead wl_display carries.
// os_error_code() carries the raw errno-shaped value (EPROTO for a
// protocol violation, an ordinary errno like ECONNRESET for a
// transport failure); when it IS EPROTO, wl_display_get_protocol_
// err() additionally names WHICH interface's request the compositor
// rejected, attached as rejected_value() - an interface name
// ("wl_surface", "xdg_surface") is itself already an identifier token.
[[nodiscard]] gltfx_err build_connection_failure(wl_display *display) noexcept;

} // namespace glintfx::platform
