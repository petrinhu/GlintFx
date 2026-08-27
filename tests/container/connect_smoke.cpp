// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstdio>
#include <cstdlib>
#include <string>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

#include "platform/wayland/display_adapter.hpp"

// connect_smoke.cpp - ARCH-PORTS, TDD case R4 (GODS_LAWS.md L-09/L-20:
// "adaptador que so encaminha chamada ao sistema operacional... o
// teste honesto e de integracao, e nao ha rebaixamento aqui - o
// container roda um kwin_wayland REAL"). Runs INSIDE
// tests/container/'s own image, against the SAME socket
// run_compositor.sh (this directory) starts for TEST-WLCONT's own
// smoke.sh - the difference from that script is WHAT is proven:
// smoke.sh proves the COMPOSITOR speaks Wayland (a generic
// wayland-info CLI call); this proves glintfx's OWN adapter - the exact
// same wayland_display_adapter.cpp production code path display_
// connect_failure_test.cpp already exercises for the REFUSAL half -
// opens and closes a REAL connection to it.
//
// wayland_display_adapter, err.cpp and err_code.cpp are compiled
// straight into this ONE executable (tests/container/prepare_arch_
// ports_fixture.sh stages the REAL sources; see that script's own
// header for why, and for glintfx/export.hpp's stub) - no glintfx.so,
// no CMake, no .pc file: this fixture proves the ADAPTER, the same
// production translation unit the rest of this fatia's tests already
// build, not the library's packaging.
//
// Reads WAYLAND_DISPLAY/XDG_RUNTIME_DIR the ordinary way (wayland_
// display_adapter::open() -> wl_display_connect(nullptr)) - the
// container's own run_compositor.sh (this directory) sets
// XDG_RUNTIME_DIR=/run/glintfx-test before starting kwin_wayland, and
// the docker exec invocation that runs THIS binary passes
// WAYLAND_DISPLAY=<socket-name> (matching run_compositor.sh's own
// CMD default, "glintfx-test") and the SAME XDG_RUNTIME_DIR - see
// .github/workflows/ci.yml's own "wayland-container" job for the exact
// docker exec invocation.

int main() {
    glintfx::platform::wayland_display_adapter adapter;

    glintfx::gltfx_rslt<void> opened = adapter.open();
    if (opened.has_error()) {
        std::fprintf(stderr, "connect_smoke: open() failed: %s\n",
                     std::string(glintfx::gltfx_err_code_name(opened.error().code())).c_str());
        return EXIT_FAILURE;
    }
    if (!adapter.is_open()) {
        std::fprintf(stderr, "connect_smoke: open() reported success but is_open() is false\n");
        return EXIT_FAILURE;
    }
    std::fprintf(stdout, "connect_smoke: connected (is_open() == true)\n");

    adapter.close();
    if (adapter.is_open()) {
        std::fprintf(stderr, "connect_smoke: close() ran but is_open() is still true\n");
        return EXIT_FAILURE;
    }
    std::fprintf(stdout, "connect_smoke: disconnected (is_open() == false)\n");

    return EXIT_SUCCESS;
}
