// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstdio>
#include <cstdlib>
#include <string>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

#include "platform/wayland/display_adapter.hpp"
#include "platform/wayland/shell_adapter.hpp"

// shell_smoke.cpp - WL-WINDOW fatia W-B (docs/plano-w6a-janela.md
// fatia 5): the container-integration case for wayland_shell_adapter -
// binds wl_compositor and xdg_wm_base against a REAL compositor,
// proving check_shell_requirements() finds both (this same kwin_
// wayland --virtual container already proved it announces
// wl_compositor/xdg_wm_base for registry_smoke, one directory over)
// and that wl_registry_bind() itself succeeds for both. Same shape as
// connect_smoke.cpp/registry_smoke.cpp: wayland_display_adapter,
// wayland_shell_adapter, shell_requirements.cpp, global_catalog.cpp,
// err.cpp and err_code.cpp are compiled straight into this ONE
// executable (tests/container/prepare_arch_ports_fixture.sh stages
// the real sources) - no glintfx.so, no CMake, no .pc file.
//
// WHAT THIS FIXTURE DOES NOT PROVE, DECLARED (docs/plano-w6a-
// janela.md sec. 4 - "o que a L-09 impede esta onda de provar"):
// whether the compositor actually SENDS an xdg_wm_base.ping during
// this fixture's short lifetime is out of this fixture's control -
// kwin_wayland may or may not ping a client this briefly connected
// before it disconnects again. wayland_shell_adapter::xdg_wm_base_
// ping() answering correctly is proven by READING THE CODE (shell_
// adapter.cpp: it calls xdg_wm_base_pong() with the same serial,
// unconditionally, the moment the event arrives), never by this
// fixture's own exit code - a run that exits 0 without ever having
// received a ping proves bind() and the listener REGISTRATION
// succeeded, nothing about the pong path actually firing.

int main() {
    // Line-buffer stdout explicitly - same fix, same reason, applied
    // to all ten fixtures in this family (connect_smoke.cpp's own
    // header comment on this exact line, docs/plano-w6b-placa-e-laco.md
    // fatia 1, 06/09/2026).
    std::setvbuf(stdout, nullptr, _IOLBF, 0);

    glintfx::platform::wayland_display_adapter adapter;

    glintfx::gltfx_rslt<void> opened = adapter.open();
    if (opened.has_error()) {
        std::fprintf(stderr, "shell_smoke: open() failed: %s\n",
                     std::string(glintfx::gltfx_err_code_name(opened.error().code())).c_str());
        return EXIT_FAILURE;
    }

    glintfx::platform::wayland_shell_adapter shell;
    glintfx::gltfx_rslt<void> shell_opened = shell.open(adapter);
    if (shell_opened.has_error()) {
        std::fprintf(stderr, "shell_smoke: shell.open() failed: %s (rejected_value=%s)\n",
                     std::string(glintfx::gltfx_err_code_name(shell_opened.error().code())).c_str(),
                     std::string(shell_opened.error().rejected_value()).c_str());
        adapter.close();
        return EXIT_FAILURE;
    }
    if (!shell.is_open()) {
        std::fprintf(stderr, "shell_smoke: shell.open() reported success but is_open() is false\n");
        shell.close();
        adapter.close();
        return EXIT_FAILURE;
    }
    std::fprintf(stdout, "shell_smoke: wl_compositor and xdg_wm_base bound (is_open() == true)\n");

    shell.close();
    if (shell.is_open()) {
        std::fprintf(stderr, "shell_smoke: shell.close() ran but is_open() is still true\n");
        adapter.close();
        return EXIT_FAILURE;
    }

    adapter.close();
    if (adapter.is_open()) {
        std::fprintf(stderr, "shell_smoke: adapter.close() ran but is_open() is still true\n");
        return EXIT_FAILURE;
    }
    std::fprintf(stdout, "shell_smoke: disconnected (is_open() == false)\n");

    return EXIT_SUCCESS;
}
