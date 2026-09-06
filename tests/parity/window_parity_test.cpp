// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/platform/window/display.hpp>
#include <glintfx/platform/window/window.hpp>

// window_parity_test.cpp - WL-WINDOW-HANDLE (docs/plano-w6a-janela.md
// fatia 11, absorbed into one fatia by the CTO's own decision): ONE
// file, ONLY the public API (glintfx::gltfx_display/gltfx_window), NO
// #if of any kind - the proof that the same behavior holds on both
// systems through EACH system's own mechanism (GODS_LAWS.md L-04).
// Plain int main(), the same "no harness, no ctest-framework
// dependency" shape every other container fixture in this project
// already uses (registry_smoke.cpp among them) - the Windows side
// links this SAME file into a normal add_executable()/add_test()
// target instead (tests/CMakeLists.txt, if(WIN32)); ctest itself is
// the only thing that differs, never the source.
//
// NAMED "window_parity_test", not a per-platform prefix, same P-0
// "mesmo nome" mechanism two_displays_test/seat_test already use
// (tests/tools/check_test_parity.py): the Linux side is staged and run
// as a container fixture of this exact name (tests/container/
// prepare_arch_ports_fixture.sh, tests/container/Containerfile, .github/
// workflows/ci.yml's own wayland-container job) - see each of those
// three files' own comments for the staging/build/exec chain.
//
// WINDOW-SIZE-REFUSE (docs/plano-w6a-janela.md, decided after a probe
// this same fatia measured a real kwin_wayland --virtual compositor
// echoing a mixed-zero request straight back instead of substituting a
// system-chosen size): a zero in EITHER dimension is refused by the
// common validation layer, window.hpp's own "WHAT THIS FATIA FREEZES"
// item 5 - see the case below.

int main() {
    glintfx::gltfx_rslt<glintfx::gltfx_display> display_opened = glintfx::gltfx_display::open();
    if (display_opened.has_error()) {
        std::fprintf(
            stderr, "window_parity_test: gltfx_display::open() failed: %s\n",
            std::string(glintfx::gltfx_err_code_name(display_opened.error().code())).c_str());
        return EXIT_FAILURE;
    }
    glintfx::gltfx_display display = std::move(display_opened.value());

    // Accented title (D-W6a-23's own validation path, both backends):
    // proves the SAME UTF-8 bytes that would be rejected if malformed
    // are accepted end to end, real system call included, not just at
    // the pure validator level window_desc_validation_test already
    // covers.
    const glintfx::gltfx_window_desc desc{
        .title = "janela de paridade \xc3\xa1\xc3\xa9\xc3\xad", // "janela de paridade áéí"
        .application_id = "org.glintfx.window_parity_test",
        .logical_size = {.width = 800, .height = 600},
    };

    glintfx::gltfx_rslt<glintfx::gltfx_window> window_opened =
        glintfx::gltfx_window::open(display, desc);
    if (window_opened.has_error()) {
        std::fprintf(
            stderr, "window_parity_test: gltfx_window::open() failed: %s (rejected_value=%s)\n",
            std::string(glintfx::gltfx_err_code_name(window_opened.error().code())).c_str(),
            std::string(window_opened.error().rejected_value()).c_str());
        return EXIT_FAILURE;
    }
    glintfx::gltfx_window window = std::move(window_opened.value());

    // logical_size() == the REQUEST, not just "not zero" - the check
    // that catches the first-WM_SIZE-lost defect docs/plano-w6a-
    // janela.md sec. 5 risk 2 names by number: a library that silently
    // copied the request into state without ever listening to the
    // system would pass a weaker "non-zero" check too.
    const glintfx::gltfx_window_size logical = window.logical_size();
    if (logical.width != 800 || logical.height != 600) {
        std::fprintf(stderr, "window_parity_test: logical_size() is %ux%u, expected 800x600\n",
                     logical.width, logical.height);
        return EXIT_FAILURE;
    }

    const glintfx::gltfx_window_size pixel = window.pixel_size();
    if (pixel.width == 0 || pixel.height == 0) {
        std::fprintf(stderr, "window_parity_test: pixel_size() is %ux%u, expected non-zero\n",
                     pixel.width, pixel.height);
        return EXIT_FAILURE;
    }

    if (window.close_requested()) {
        std::fprintf(stderr, "window_parity_test: close_requested() is true right after open()\n");
        return EXIT_FAILURE;
    }

    // 50 pumps, same budget wayland_window_adapter::wait_first_
    // configure() already uses internally - LOOP-RUN's own contract
    // (display.hpp), exercised here through the PUBLIC gltfx_display
    // itself.
    for (int i = 0; i < 50; ++i) {
        glintfx::gltfx_rslt<void> pumped = display.pump_events();
        if (pumped.has_error()) {
            std::fprintf(stderr, "window_parity_test: pump_events() failed on iteration %d: %s\n",
                         i,
                         std::string(glintfx::gltfx_err_code_name(pumped.error().code())).c_str());
            return EXIT_FAILURE;
        }
    }

    glintfx::gltfx_rslt<void> retitled = window.set_title("titulo trocado em tempo de execucao");
    if (retitled.has_error()) {
        std::fprintf(stderr, "window_parity_test: set_title() failed: %s\n",
                     std::string(glintfx::gltfx_err_code_name(retitled.error().code())).c_str());
        return EXIT_FAILURE;
    }

    std::fprintf(stdout,
                 "window_parity_test: logical=%ux%u pixel=%ux%u, title changed, 50 pumps ok\n",
                 logical.width, logical.height, pixel.width, pixel.height);

    // WINDOW-SIZE-REFUSE, through the PUBLIC API: a zero in either
    // dimension is refused before either backend ever sees it - proven
    // here against the real `display` this file already opened, not
    // just at the pure validator level window_desc_validation_test.cpp
    // already covers.
    const glintfx::gltfx_window_desc zero_width_desc{
        .title = "janela largura zero",
        .application_id = "org.glintfx.window_parity_test",
        .logical_size = {.width = 0, .height = 600},
    };
    glintfx::gltfx_rslt<glintfx::gltfx_window> zero_width_opened =
        glintfx::gltfx_window::open(display, zero_width_desc);
    if (!zero_width_opened.has_error()) {
        std::fprintf(stderr,
                     "window_parity_test: gltfx_window::open() with 0x600 succeeded, expected "
                     "invalid_argument\n");
        return EXIT_FAILURE;
    }
    if (zero_width_opened.error().rejected_value() != std::string_view{"logical_size"}) {
        std::fprintf(
            stderr,
            "window_parity_test: 0x600 refused with rejected_value=%s, expected logical_size\n",
            std::string(zero_width_opened.error().rejected_value()).c_str());
        return EXIT_FAILURE;
    }
    std::fprintf(stdout, "window_parity_test: 0x600 request refused (rejected_value=logical_size, "
                         "as expected)\n");

    // No explicit close() call on either object - GODS_LAWS.md L-22's
    // own RAII shape: `window` (declared after `display`) is destroyed
    // FIRST when this scope ends, `display` second, the exact "reverse
    // of creation" teardown order this project's own adapters already
    // enforce by hand one layer down. Proving that order held is what
    // window.hpp's own "WHAT THIS FATIA FREEZES" item 2 (the display
    // must outlive the window) actually depends on at runtime - this
    // scope's own end is the live demonstration, not a separate
    // assertion this file could make after the fact.
    return EXIT_SUCCESS;
}
