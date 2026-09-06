// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/port/gl_context_adapter_port.hpp"
#include "platform/wayland/selected_gl_context_adapter.hpp"

// selected_gl_context_adapter_check.cpp - W-EGL (docs/plano-w6b-placa-
// e-laco.md fatia 3, GODS_LAWS.md L-19/L-40), the exact sibling of
// selected_window_adapter_check.cpp one file over: the internal TU
// that proves the SELECTION, not just the adapter type, satisfies
// platform::gl_context_adapter_port. If a future edit to wayland_egl_
// context_adapter ever drifts from that concept (a signature typo, a
// method renamed on one side and not the other - the exact shape this
// project's own paridade findings keep catching), THIS FILE fails to
// compile on every build that reaches src/platform/wayland/ - the
// build breaks instead of linking a GL context handle whose gpu()/
// option_support() silently does nothing on this platform.
//
// No behavior lives in this file - it contributes nothing to glintfx_
// library's own object code beyond the static_assert below, which the
// compiler discharges entirely at compile time.

namespace glintfx::platform {

static_assert(gl_context_adapter_port<selected_gl_context_adapter>,
              "selected_gl_context_adapter must satisfy gl_context_adapter_port - GODS_LAWS.md "
              "L-19/L-40/W-EGL: the compile-time selection wired in a type that does not satisfy "
              "the port contract");

} // namespace glintfx::platform
