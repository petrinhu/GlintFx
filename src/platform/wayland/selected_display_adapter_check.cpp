// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/port/display_backend_port.hpp"
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
// SECOND static_assert added by LOOP-RUN fatia 7 (docs/plano-w6b-
// fatias-6-8.md, F2 of that plan's own sec. 0): before this fatia, the
// Win32 side of this SAME check (src/platform/win32/selected_display_
// adapter_check.cpp) already asserted display_backend_port, and this
// Wayland side only asserted the narrower display_connection_port -
// what actually held the Linux build to the wider contract was
// display_facade.cpp's own static_assert, one layer up, never this
// file. GODS_LAWS.md L-04/L-17 (paridade: a boa pratica de um irmao so'
// se propaga quando alguem procura o gemeo): now that wait_events()
// (display_adapter.hpp/.cpp, this same fatia) exists, this check
// mirrors its Win32 sibling exactly, so a future edit that breaks
// display_backend_port on selected_display_adapter fails to compile
// HERE, at the file whose whole job is proving the selection, instead
// of only surfacing three layers away in display_facade.cpp.
//
// No behavior lives in this file - it contributes nothing to
// glintfx_library's own object code beyond the two static_asserts
// below, which the compiler discharges entirely at compile time.

namespace glintfx::platform {

static_assert(display_connection_port<selected_display_adapter>,
              "selected_display_adapter must satisfy display_connection_port - "
              "GODS_LAWS.md L-19/ARCH-PORTS: the compile-time selection wired in a "
              "type that does not satisfy the port contract");

static_assert(display_backend_port<selected_display_adapter>,
              "selected_display_adapter must satisfy display_backend_port - "
              "GODS_LAWS.md L-04/L-19: pump_events() and wait_events() must both exist, "
              "the exact capability display_backend_port.hpp's own header comment names "
              "LOOP-RUN fatia 7 as owing it on this side too");

} // namespace glintfx::platform
