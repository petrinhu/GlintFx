// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/port/display_connection_port.hpp"
#include "platform/wayland/selected_display_adapter.hpp"

// selected_display_adapter_check.cpp - ARCH-PORTS, CTO plan sec. 1.1.3:
// the internal TU that proves the SELECTION, not just the adapter
// type, is correct. If a future edit to selected_display_adapter.hpp
// ever aliases a type that no longer satisfies display_connection_port
// (a typo, a signature that drifted while the concept did not, or vice
// versa), THIS FILE fails to compile on every build that reaches
// src/platform/wayland/ - GODS_LAWS.md L-19/L-40: a build that silently
// links an empty or mismatched platform layer is exactly the failure
// mode this project's laws forbid, and a compile error here is how
// "the build breaks instead of compiling vazio" is enforced for the
// SELECTION mechanism specifically, as distinct from the adapter type
// itself (which display_connect_failure_test.cpp and the container
// integration case already exercise behaviorally).
//
// No behavior lives in this file - it contributes nothing to
// glintfx_library's own object code beyond the static_assert below,
// which the compiler discharges entirely at compile time.

namespace glintfx::platform {

static_assert(display_connection_port<selected_display_adapter>,
              "selected_display_adapter must satisfy display_connection_port - "
              "GODS_LAWS.md L-19/ARCH-PORTS: the compile-time selection wired in a "
              "type that does not satisfy the port contract");

} // namespace glintfx::platform
