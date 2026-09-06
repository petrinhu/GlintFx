// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/port/window_adapter_port.hpp"
#include "platform/win32/selected_window_adapter.hpp"

// selected_window_adapter_check.cpp - WL-WINDOW-HANDLE, the exact
// win32/ sibling of src/platform/wayland/selected_window_adapter_
// check.cpp (GODS_LAWS.md L-19/L-40): the internal TU that proves the
// SELECTION, not just the adapter type, is correct - this project has
// no Windows toolchain to compile this file locally (GODS_LAWS.md
// L-27, the same declared limitation every other win32/ file in this
// project already carries), so the windows CI job is what actually
// discharges the compiler on it.
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
