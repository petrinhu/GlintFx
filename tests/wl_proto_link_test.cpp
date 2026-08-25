// SPDX-License-Identifier: AGPL-3.0-or-later
//
// wl_proto_link_test.cpp - proves the wayland-scanner build step
// (WL-PROTO, GODS_LAWS.md L-05/L-07) actually produces a binding this
// library can link against, from the SYSTEM xdg-shell.xml, never
// vendored.
//
// Scope, deliberately narrow: this slice is the binding and the build
// step only, not connection/window/event handling (those are
// WL-DISPLAY, a later fatia). The check below proves two things and
// nothing more:
//   1. the generated client header is reachable from a plain C++ TU,
//      wrapped in extern "C" (the private-code half compiled into
//      glintfx_library is C, GODS_LAWS.md L-03/L-20: 100% generated,
//      no hand-written line, so TDD's red/green does not apply to the
//      generated .c itself - only to this hand-written probe of it);
//   2. the symbol xdg_wm_base_interface, defined by the generated
//      private-code and linked into glintfx_library, resolves at link
//      time and carries the real protocol data (name "xdg_wm_base"),
//      not just a non-null address (a bare `!= nullptr` on the address
//      of a named object is always true and would trip -Waddress under
//      GLINTFX_WERROR for proving nothing).
extern "C" {
#include <xdg-shell-client-protocol.h>
}

#include <string_view>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

GLINTFX_TEST(wl_proto_link_test) {
    GLINTFX_CHECK(std::string_view{xdg_wm_base_interface.name} == "xdg_wm_base");
}
