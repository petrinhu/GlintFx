// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstdio>
#include <cstdlib>
#include <string>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

#include "platform/wayland/display_adapter.hpp"
#include "platform/wayland/global_catalog.hpp"

// registry_smoke.cpp - WL-DISPLAY fatia B, TDD case B (w4-plano.md
// sec. 3.1.B): the container-integration red/green case the plan
// requires - "teste de integracao no container TEST-WLCONT que abre,
// enumera e exige presenca de wl_compositor no catalogo". Runs INSIDE
// tests/container/'s own image, against the SAME socket
// run_compositor.sh (this directory) starts - same shape as
// connect_smoke.cpp (ARCH-PORTS, TDD case R4), one directory over:
// that file proves open()/close() alone; this one proves the registry
// burst this fatia adds on top of it actually reaches a REAL
// compositor's own globals, not a fake/mocked list.
//
// wayland_display_adapter, global_catalog, err.cpp and err_code.cpp
// are compiled straight into this ONE executable
// (tests/container/prepare_arch_ports_fixture.sh stages the REAL
// sources - see that script's own header for why) - no glintfx.so, no
// CMake, no .pc file: this fixture proves the ADAPTER's registry path,
// the same production translation units the rest of this fatia's
// tests already build, not the library's packaging.

int main() {
    glintfx::platform::wayland_display_adapter adapter;

    glintfx::gltfx_rslt<void> opened = adapter.open();
    if (opened.has_error()) {
        std::fprintf(stderr, "registry_smoke: open() failed: %s\n",
                     std::string(glintfx::gltfx_err_code_name(opened.error().code())).c_str());
        return EXIT_FAILURE;
    }
    if (!adapter.is_open()) {
        std::fprintf(stderr, "registry_smoke: open() reported success but is_open() is false\n");
        return EXIT_FAILURE;
    }

    // The initial roundtrip inside open() must have already closed the
    // burst of `global` events a real compositor emits right after
    // get_registry() - by the time we get here, both wl_compositor and
    // xdg_wm_base (the same two globals smoke.sh's own wayland-info
    // call proves the COMPOSITOR announces) must already be in the
    // catalog, with NO further roundtrip or dispatch call from this
    // file.
    const glintfx::platform::global_catalog &globals = adapter.globals();
    if (globals.size() == 0) {
        std::fprintf(stderr, "registry_smoke: catalog is empty after open() (varredura vazia)\n");
        return EXIT_FAILURE;
    }
    if (globals.find_by_interface("wl_compositor") == nullptr) {
        std::fprintf(stderr, "registry_smoke: wl_compositor absent from the catalog\n");
        return EXIT_FAILURE;
    }
    if (globals.find_by_interface("xdg_wm_base") == nullptr) {
        std::fprintf(stderr, "registry_smoke: xdg_wm_base absent from the catalog\n");
        return EXIT_FAILURE;
    }
    std::fprintf(stdout, "registry_smoke: catalog has %zu global(s), wl_compositor and xdg_wm_base present\n",
                 globals.size());

    adapter.close();
    if (adapter.is_open()) {
        std::fprintf(stderr, "registry_smoke: close() ran but is_open() is still true\n");
        return EXIT_FAILURE;
    }
    std::fprintf(stdout, "registry_smoke: disconnected (is_open() == false)\n");

    return EXIT_SUCCESS;
}
