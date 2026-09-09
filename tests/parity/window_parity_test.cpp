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
//
// WIN-SIZE-AT-OPEN (docs/plano-w6a-janela.md, achado do time-lead
// 06/09/2026): this file's own logical_size() check below is what
// found the real divergence this fatia exists to close - Linux
// reported the request, Windows reported 0x0, right after open()
// itself had already returned success. window.hpp's own "WHAT EACH
// ACCESSOR MEANS THE MOMENT open() RETURNS" item is the contract this
// check enforces on both systems now.
//
// PARITY-STATE-PRINT (same fatia, "mede antes de prometer"): pixel_
// size() and the three state() bits are printed further down, never
// asserted equal across platforms - neither has been measured on real
// divergent hardware yet (this project's own CI executors all run at
// scale 1, and a never-shown window is never activated on Windows).
// Promising a value nobody measured is exactly the citation-rule defect
// this fatia's own DISPLAY-PASSKEY/WINDOW-SIZE-REFUSE writeup already
// forbids in public-header prose - this file holds itself to the same
// standard.

int main() {
    // Unbuffer stdout explicitly - same fix, same reason, applied
    // to all ten fixtures in this family (tests/container/connect_
    // smoke.cpp's own header comment on this exact line, docs/plano-
    // w6b-placa-e-laco.md fatia 1, 06/09/2026). `std::setvbuf` itself
    // is plain C89, available on every compiler this project targets
    // (including MSVC, which registers this ctest directly on Windows -
    // this file's own header comment on why it has no #if
    // defined(_WIN32) anywhere), but the MODE this line asks for is
    // not portable in practice: the original `_IOLBF` with `size` 0
    // crashed the Windows CI job with 0xC0000409, because MSVC's
    // setvbuf requires 2 <= size <= INT_MAX for the `_IOFBF`/`_IOLBF`
    // modes and aborts the process outside that range (learn.
    // microsoft.com/cpp/c-runtime-library/reference/setvbuf) - glibc
    // never validated that range, which is why the same line built and
    // ran clean on Linux. `_IONBF` ignores `size` and `buffer`
    // entirely, so no range applies on either system, and it is the
    // one mode MSVC's own docs confirm actually disables buffering on
    // Win32 (`_IOLBF` there behaves exactly like `_IOFBF`, full
    // buffering, never line-by-line).
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    glintfx::gltfx_rslt<glintfx::gltfx_display> display_opened = glintfx::gltfx_display::open();
    if (display_opened.has_error()) {
        std::fprintf(
            stderr, "window_parity_test: gltfx_display::open() failed: %s\n",
            std::string(glintfx::gltfx_err_code_name(display_opened.err().code())).c_str());
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
        std::fprintf(stderr,
                     "window_parity_test: gltfx_window::open() failed: %s (rejected_value=%s)\n",
                     std::string(glintfx::gltfx_err_code_name(window_opened.err().code())).c_str(),
                     std::string(window_opened.err().rejected_value()).c_str());
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
    // MEASURED-COLLECTOR: pixel_size() is the OTHER accessor this
    // file's own PARITY-STATE-PRINT paragraph names as divergent by
    // reading, never yet measured for real (both CI executors run at
    // scale 1 today) - collected the same way the state bits are.
    std::fprintf(stdout, "MEASURED window_parity_test.pixel_width_after_open=%u\n", pixel.width);
    std::fprintf(stdout, "MEASURED window_parity_test.pixel_height_after_open=%u\n", pixel.height);

    if (window.close_requested()) {
        std::fprintf(stderr, "window_parity_test: close_requested() is true right after open()\n");
        return EXIT_FAILURE;
    }

    // PARITY-STATE-PRINT (docs/plano-w6a-janela.md, achado do time-lead
    // 06/09/2026, "mede antes de prometer"): the CTO's own sweep of the
    // five public accessors right after a successful open() found
    // logical_size() genuinely divergent (WIN-SIZE-AT-OPEN fixes that
    // one, checked above) and TWO MORE divergent by reading the code,
    // never yet measured against real hardware - pixel_size() (hidden
    // today because both CI executors run at scale 1; a HiDPI display
    // would show it) and these three state bits (a window this project
    // never maps/shows is never activated on Windows, but Wayland's own
    // compositor may activate an unmapped toplevel differently). Printed
    // here, on both systems, through the SAME name every other
    // "measured, not asserted" fixture in this project already uses
    // (seat_test.cpp's own header comment) - a promise that `active`
    // agrees across platforms is exactly the kind of measurement-free
    // claim this fatia's own citation rule exists to forbid; this line
    // is what a future fatia reads before ever writing one.
    const int active_after_open =
        static_cast<int>(window.state(glintfx::gltfx_window_state_bit::active));
    const int maximized_after_open =
        static_cast<int>(window.state(glintfx::gltfx_window_state_bit::maximized));
    const int fullscreen_after_open =
        static_cast<int>(window.state(glintfx::gltfx_window_state_bit::fullscreen));
    std::fprintf(stdout,
                 "window_parity_test: state right after open() - active=%d maximized=%d "
                 "fullscreen=%d (measured, not asserted)\n",
                 active_after_open, maximized_after_open, fullscreen_after_open);
    // MEASURED-COLLECTOR (tests/tools/collect_measured.py): three
    // separate lines, one per key - the parity job (PARITY-GATE) reads
    // these back and lines them up against whatever the Windows leg
    // wrote for the SAME three keys, this exact file being the one
    // that runs on both systems (this file's own top comment).
    std::fprintf(stdout, "MEASURED window_parity_test.active_after_open=%d\n", active_after_open);
    std::fprintf(stdout, "MEASURED window_parity_test.maximized_after_open=%d\n",
                 maximized_after_open);
    std::fprintf(stdout, "MEASURED window_parity_test.fullscreen_after_open=%d\n",
                 fullscreen_after_open);

    // 50 pumps, same budget wayland_window_adapter::wait_first_
    // configure() already uses internally - LOOP-RUN's own contract
    // (display.hpp), exercised here through the PUBLIC gltfx_display
    // itself.
    for (int i = 0; i < 50; ++i) {
        glintfx::gltfx_rslt<void> pumped = display.pump_events();
        if (pumped.has_error()) {
            std::fprintf(stderr, "window_parity_test: pump_events() failed on iteration %d: %s\n",
                         i, std::string(glintfx::gltfx_err_code_name(pumped.err().code())).c_str());
            return EXIT_FAILURE;
        }
    }

    glintfx::gltfx_rslt<void> retitled = window.set_title("titulo trocado em tempo de execucao");
    if (retitled.has_error()) {
        std::fprintf(stderr, "window_parity_test: set_title() failed: %s\n",
                     std::string(glintfx::gltfx_err_code_name(retitled.err().code())).c_str());
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
    if (zero_width_opened.err().rejected_value() != std::string_view{"logical_size"}) {
        std::fprintf(
            stderr,
            "window_parity_test: 0x600 refused with rejected_value=%s, expected logical_size\n",
            std::string(zero_width_opened.err().rejected_value()).c_str());
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
