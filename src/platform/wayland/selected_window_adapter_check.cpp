// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/port/window_adapter_port.hpp"
#include "platform/wayland/selected_window_adapter.hpp"

// selected_window_adapter_check.cpp - WL-WINDOW-HANDLE, the exact
// sibling of selected_display_adapter_check.cpp one file over (GODS_
// LAWS.md L-19/L-40): the internal TU that proves the SELECTION, not
// just the adapter type, is correct. If a future edit to selected_
// window_adapter.hpp ever aliases a type that no longer satisfies
// window_adapter_port (a typo, a signature that drifted while the
// concept did not, or vice versa - set_title() being added to ONE
// side and not the other, this fatia's own paridade finding, is
// exactly the shape this static_assert exists to catch permanently),
// THIS FILE fails to compile on every build that reaches src/platform/
// wayland/ - the build breaks instead of linking a window handle whose
// set_title() silently does nothing on this platform.
//
// No behavior lives in this file - it contributes nothing to glintfx_
// library's own object code beyond the static_assert below, which the
// compiler discharges entirely at compile time.

namespace glintfx::platform {

static_assert(window_adapter_port<selected_window_adapter>,
              "selected_window_adapter must satisfy window_adapter_port - "
              "GODS_LAWS.md L-19/WL-WINDOW-HANDLE: the compile-time selection wired in a "
              "type that does not satisfy the port contract");

} // namespace glintfx::platform
