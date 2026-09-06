// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstdio>
#include <cstdlib>
#include <string>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

#include "platform/wayland/display_adapter.hpp"
#include "platform/wayland/global_catalog.hpp"

// two_displays_test.cpp - WIN-WINDOW fatia 7 (X-1', docs/plano-w6a-
// janela.md fatia 7, D-W6a-16, "Prova Linux" column: "two_displays_test
// (container, dois displays abertos no mesmo processo)"; sec. 5 item 8:
// "Nenhum teste de hoje abre dois... two_displays_test nos dois
// lados"). NAMED "two_displays_test", not a per-platform prefix - the
// EXACT ctest name the Win32 side's own tests/two_displays_test.cpp
// already uses (tests/CMakeLists.txt, if(WIN32)), so tests/tools/
// check_test_parity.py's P-0 inventory lines the two up under ONE
// shared name (same shape registry_smoke.cpp/seat_test.cpp already use
// for their own Windows-side ctest counterparts).
//
// WHAT THIS PROVES, and why it took until now to write: two
// independent wayland_display_adapter instances, opened in the SAME
// process against the SAME compositor, must not collide or share
// state - the Wayland-side analogue of the Windows F2 defect this same
// fatia fixed (a fixed, per-process window-class name that made a
// SECOND win32_display_adapter::open() fail with ERROR_CLASS_ALREADY_
// EXISTS). tests/two_displays_test.cpp's own header comment used to
// claim "the Wayland side already proves two independent connections
// in the same process work with no special handling at all" - FALSE
// by measurement: grepping tests/*.cpp, tests/container/*.cpp and
// src/platform/wayland/*.cpp for a second wayland_display_adapter
// instance in any one file found zero (every existing fixture opens
// exactly one). This file is what makes that claim true.
//
// WHERE THE WINDOWS TEST COMPARES CLASS NAMES, THIS ONE CANNOT, AND
// SHOULDN'T: win32_display_adapter needs class_name_for(this) because
// RegisterClassExW registers a PROCESS-WIDE named resource - two
// adapters sharing one name is a real collision. wayland_display_
// adapter has no such resource: wl_display_connect(nullptr) hands back
// an independent connection every call, and the registry listener
// (kRegistryListener, display_adapter.cpp) is a stateless constexpr
// struct - the per-instance routing is the `this` pointer libwayland's
// own wl_registry_add_listener() call already carries as its `data`
// argument, never a name this adapter registers anywhere. wayland_
// display_adapter also exposes no raw wl_display* accessor (nothing
// outside display_adapter.cpp needs one - window/seat/shell adapters
// all take a `const wayland_display_adapter &`), and adding one here
// purely to compare two pointers would be a production accessor with
// exactly one consumer, a test (CONTRACT.md's YAGNI, §12.2). The real
// equivalent of "not secretly aliased" this fixture uses instead:
// close ONE adapter mid-test and confirm the OTHER survives untouched
// - is_open() still true, its own catalog still intact. A shared or
// aliased connection under the hood would fail exactly this check
// (closing one would tear down or corrupt the other); two genuinely
// independent ones cannot.

namespace {

// One open() + one non-empty-catalog check, reused for both adapters
// below (GODS_LAWS.md L-17 "regra de 3" does not apply - exactly two
// call sites, WET is the house default under three - but "open this
// one and check its catalog" is a real atom with its own name, not a
// convenience wrapper, and the failure message needs the caller's own
// label to say WHICH of the two adapters failed).
bool open_and_check_catalog(glintfx::platform::wayland_display_adapter &adapter,
                            const char *label) {
    const glintfx::gltfx_rslt<void> opened = adapter.open();
    if (opened.has_error()) {
        std::fprintf(stderr, "two_displays_test: %s.open() failed: %s\n", label,
                     std::string(glintfx::gltfx_err_code_name(opened.error().code())).c_str());
        return false;
    }
    if (!adapter.is_open()) {
        std::fprintf(stderr,
                     "two_displays_test: %s.open() reported success but is_open() is false\n",
                     label);
        return false;
    }

    const glintfx::platform::global_catalog &globals = adapter.globals();
    if (globals.size() == 0) {
        std::fprintf(stderr,
                     "two_displays_test: %s's catalog is empty after open() (varredura vazia)\n",
                     label);
        return false;
    }
    if (globals.find_by_interface("wl_compositor") == nullptr) {
        std::fprintf(stderr, "two_displays_test: wl_compositor absent from %s's catalog\n", label);
        return false;
    }
    if (globals.find_by_interface("xdg_wm_base") == nullptr) {
        std::fprintf(stderr, "two_displays_test: xdg_wm_base absent from %s's catalog\n", label);
        return false;
    }
    std::fprintf(stdout, "two_displays_test: %s open, catalog has %zu global(s)\n", label,
                 globals.size());
    // MEASURED-COLLECTOR: `label` ("first"/"second") folds into the key
    // itself, so the two independent connections this fixture opens
    // never collide under one owner.
    std::fprintf(stdout, "MEASURED two_displays_test.%s_global_count=%zu\n", label, globals.size());
    return true;
}

} // namespace

int main() {
    glintfx::platform::wayland_display_adapter first;
    glintfx::platform::wayland_display_adapter second;

    if (!open_and_check_catalog(first, "first")) {
        return EXIT_FAILURE;
    }
    if (!open_and_check_catalog(second, "second")) {
        return EXIT_FAILURE;
    }

    // Close SECOND first (reverse of creation order, same "teardown in
    // reverse" shape this project's own close() methods document
    // elsewhere), then prove FIRST survived untouched - the real
    // analogue of the Windows side's class-name inequality check; see
    // this file's own header comment for why a raw handle comparison
    // is neither available nor meaningful here.
    second.close();
    if (second.is_open()) {
        std::fprintf(stderr, "two_displays_test: second.close() ran but is_open() is still true\n");
        return EXIT_FAILURE;
    }
    if (!first.is_open()) {
        std::fprintf(stderr,
                     "two_displays_test: closing second left first closed too (shared state)\n");
        return EXIT_FAILURE;
    }
    if (first.globals().find_by_interface("wl_compositor") == nullptr ||
        first.globals().find_by_interface("xdg_wm_base") == nullptr) {
        std::fprintf(stderr, "two_displays_test: closing second corrupted first's own catalog\n");
        return EXIT_FAILURE;
    }
    std::fprintf(stdout, "two_displays_test: closing second left first untouched (is_open() still "
                         "true, catalog intact)\n");

    first.close();
    if (first.is_open()) {
        std::fprintf(stderr, "two_displays_test: first.close() ran but is_open() is still true\n");
        return EXIT_FAILURE;
    }
    std::fprintf(stdout, "two_displays_test: both adapters closed cleanly\n");

    return EXIT_SUCCESS;
}
