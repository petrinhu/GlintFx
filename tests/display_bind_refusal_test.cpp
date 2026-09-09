// SPDX-License-Identifier: AGPL-3.0-or-later
#include <wayland-client.h>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/wayland/display_adapter.hpp"
#include "platform/wayland/global_catalog.hpp"

// display_bind_refusal_test.cpp - WL-WINDOW fatia W-A (docs/plano-w6a-
// janela.md fatia 3; plano W5 sec. 1.2): proves wayland_display_
// adapter::bind()'s refusal path on an adapter NEVER OPENED - no
// compositor required, no container required. The real bind() against
// a live compositor (the "compositor offers less than requested"
// path, clamped through global_catalog::clamp_version - already fully
// proven by clamp_version_picks_the_smaller_of_supported_and_announced
// in tests/global_catalog_test.cpp) is fatia 5/W-B's own shell_smoke,
// inside the container (GODS_LAWS.md L-09): this file touches no real
// Wayland socket, and reaches no code path past the closed-adapter
// check - the fake_global and interface reference below are never
// dereferenced.
//
// wayland_global is a plain value the CALLER already obtained from a
// catalog (typically global_catalog::find_by_interface) before ever
// calling bind() - this test builds one BY HAND, with a name that
// would never resolve on a real registry, precisely BECAUSE bind() on
// a closed adapter must refuse BEFORE wl_registry_bind() is ever
// called: the numeric name and the interface passed here are read by
// nothing on this path.
//
// wl_compositor_interface (an ordinary libwayland-client symbol -
// wayland-client.h's own generated binding for the wl_compositor
// protocol, not a glintfx type) stands in for "some real wl_interface"
// only because bind()'s signature takes one by reference: an
// incomplete struct cannot be instantiated, and this refusal path
// never reads a single field of it.
//
// wayland_display_adapter is compiled a SECOND time directly into this
// executable's own object set (tests/CMakeLists.txt) - the same
// technique display_connect_failure_test.cpp and global_catalog_test.
// cpp already use for a symbol with no GLINTFX_API (GODS_LAWS.md
// L-19/ARCH-PORTS, "nada e exportado" - glintfx.so's own
// -fvisibility=hidden never puts wayland_display_adapter's methods in
// the dynamic symbol table, so a separate executable linking only
// against glintfx::glintfx could never resolve them across that .so
// boundary).

GLINTFX_TEST(bind_on_a_never_opened_adapter_returns_invalid_argument) {
    glintfx::platform::wayland_display_adapter adapter;
    GLINTFX_CHECK(!adapter.is_open());

    const glintfx::platform::wayland_global fake_global{
        .name = 1,
        .interface = "wl_compositor",
        .version = 6,
    };

    // "sem abort, sem excecao": reaching the checks below at all
    // already proves bind() is noexcept (an escaped exception would
    // have called std::terminate() before this line) and never
    // touches libwayland-client on a closed adapter (a real call
    // against name 1 - unbound on any real registry - would be a
    // protocol violation the compositor is entitled to kill the
    // connection over, which this test has no compositor to survive).
    const glintfx::gltfx_rslt<void *> bound = adapter.bind(fake_global, wl_compositor_interface, 6);

    GLINTFX_CHECK(bound.has_error());
    GLINTFX_CHECK(bound.err().code() == glintfx::gltfx_err_code::invalid_argument);
}
