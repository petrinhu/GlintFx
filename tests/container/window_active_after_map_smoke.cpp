// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/platform/gl/context.hpp>
#include <glintfx/platform/window/display.hpp>
#include <glintfx/platform/window/window.hpp>

// window_active_after_map_smoke.cpp - FACADE-PIN, T1 (docs/plano-
// conserto-fachadas-uaf.md sec. 8): the BEHAVIOR half of this fatia's
// own proof, alongside T0's own mechanism half. ONLY the public API
// (glintfx::gltfx_display/gltfx_window/gltfx_gl_context, the SAME "no
// #if of any kind" shape window_parity_test.cpp/gl_context_parity_
// test.cpp already use one directory over) - never an internal adapter
// directly.
//
// THE CRASH THIS FIXTURE REPRODUCES (docs/plano-conserto-fachadas-uaf.
// md sec. 1, measured by the implementer of fatia 5): a window mapped
// (a real buffer swapped at least once) and kept alive for a few more
// frames used to either crash the process inside wayland_window_
// adapter's own event dispatch, or never learn the compositor's late
// `configure` (activated/maximized/fullscreen) at all - both symptoms
// of the SAME defect (a listener registered on a stack address that
// moved). BEFORE this fatia's own conserto, this exact sequence (open,
// swap 640x480 a few times, then keep pumping) is what crashed inside
// wayland_window_adapter::xdg_surface_configure() the moment the
// compositor's SECOND configure (carrying `activated`) arrived after
// window_facade.cpp's own std::move(adapter) into the heap.
//
// THE 50-ROUNDTRIP BUDGET is the SAME one wayland_window_adapter::
// wait_first_configure() already uses internally (window_adapter.cpp) -
// this fixture does not invent a new number, it reuses the one this
// project already measured generous enough for a CI container slower
// than the leader's own machine.
//
// TWO VERMELHOS COLLAPSE INTO ONE HERE, ON PURPOSE (this plan's own
// sec. 8 table): a crash inside the dispatch (SIGSEGV, this process
// simply never reaches the final printf) and a budget exhausted
// without ever observing `active` (the late `configure` silently
// wrote into dead memory, so window.state(active) never turns true)
// are BOTH the same underlying defect, and this fixture's own exit
// code cannot tell them apart - the container job running this binary
// under `set -eu -o pipefail` already treats a crash exactly like any
// other non-zero exit, so the distinction does not need to be captured
// here.

namespace {

constexpr int kPumpRoundtripBudget = 50;

} // namespace

int main() {
    // Unbuffer stdout explicitly - same fix, same reason, applied to
    // every fixture in this family (window_smoke.cpp's own header
    // comment on this exact line).
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    glintfx::gltfx_rslt<glintfx::gltfx_display> display_opened = glintfx::gltfx_display::open();
    if (display_opened.has_error()) {
        std::fprintf(
            stderr, "window_active_after_map_smoke: gltfx_display::open() failed: %s\n",
            std::string(glintfx::gltfx_err_code_name(display_opened.err().code())).c_str());
        return EXIT_FAILURE;
    }
    glintfx::gltfx_display display = std::move(display_opened.value());

    const glintfx::gltfx_window_desc desc{
        .title = "window_active_after_map_smoke",
        .application_id = "org.glintfx.window_active_after_map_smoke",
        .logical_size = {.width = 640, .height = 480},
    };
    glintfx::gltfx_rslt<glintfx::gltfx_window> window_opened =
        glintfx::gltfx_window::open(display, desc);
    if (window_opened.has_error()) {
        std::fprintf(stderr,
                     "window_active_after_map_smoke: gltfx_window::open() failed: %s "
                     "(rejected_value=%s)\n",
                     std::string(glintfx::gltfx_err_code_name(window_opened.err().code())).c_str(),
                     std::string(window_opened.err().rejected_value()).c_str());
        return EXIT_FAILURE;
    }
    glintfx::gltfx_window window = std::move(window_opened.value());

    glintfx::gltfx_rslt<glintfx::gltfx_gl_context> context_opened =
        glintfx::gltfx_gl_context::open(window, glintfx::gltfx_gl_context_desc{});
    if (context_opened.has_error()) {
        std::fprintf(stderr,
                     "window_active_after_map_smoke: gltfx_gl_context::open() failed: %s "
                     "(rejected_value=%s)\n",
                     std::string(glintfx::gltfx_err_code_name(context_opened.err().code())).c_str(),
                     std::string(context_opened.err().rejected_value()).c_str());
        return EXIT_FAILURE;
    }
    glintfx::gltfx_gl_context context = std::move(context_opened.value());

    if (glintfx::gltfx_rslt<void> current = context.make_current(); current.has_error()) {
        std::fprintf(stderr, "window_active_after_map_smoke: make_current() failed: %s\n",
                     std::string(glintfx::gltfx_err_code_name(current.err().code())).c_str());
        return EXIT_FAILURE;
    }

    // D-W5-9's own reasoning, applied through the public API this time:
    // a window never mapped (no content ever swapped) never receives
    // the compositor's SECOND configure - swap_buffers() is what makes
    // the compositor treat this surface as real and worth activating.
    // A handful of swaps, not just one, the same "mais de um quadro"
    // shape this plan's own sec. 1 names for the original crash.
    for (int frame = 0; frame < 3; ++frame) {
        if (glintfx::gltfx_rslt<glintfx::gltfx_present_outcome> presented = context.swap_buffers();
            presented.has_error()) {
            std::fprintf(
                stderr, "window_active_after_map_smoke: swap_buffers() failed on frame %d: %s\n",
                frame, std::string(glintfx::gltfx_err_code_name(presented.err().code())).c_str());
            return EXIT_FAILURE;
        }
    }
    std::fprintf(stdout, "window_active_after_map_smoke: mapped (3 frames swapped)\n");

    // THE VERMELHO THIS FIXTURE EXISTS TO CATCH: pump until the late
    // `configure` (carrying `activated`) is observed, or the budget
    // runs out - never a bare sleep, never an unbounded loop.
    bool became_active = false;
    int roundtrips_used = 0;
    for (; roundtrips_used < kPumpRoundtripBudget && !became_active; ++roundtrips_used) {
        if (glintfx::gltfx_rslt<void> pumped = display.pump_events(); pumped.has_error()) {
            std::fprintf(stderr, "window_active_after_map_smoke: pump_events() failed: %s\n",
                         std::string(glintfx::gltfx_err_code_name(pumped.err().code())).c_str());
            return EXIT_FAILURE;
        }
        became_active = window.state(glintfx::gltfx_window_state_bit::active);
    }

    // MEASURED, never asserted as a fixed constant (this project's own
    // "medido, nunca suposto" convention, window_smoke.cpp's own
    // MEASURED lines) - a compositor's own timing to activate a window
    // is not this fixture's contract to fix at one exact number.
    std::fprintf(stdout, "MEASURED window_active_after_map_smoke.roundtrips=%d\n", roundtrips_used);

    if (!became_active) {
        std::fprintf(stderr,
                     "window_active_after_map_smoke: FAIL - orcamento de %d roundtrip(s) "
                     "esgotado sem window.state(active) nunca virar verdadeiro\n",
                     kPumpRoundtripBudget);
        return EXIT_FAILURE;
    }

    std::fprintf(stdout,
                 "window_active_after_map_smoke: OK - window.state(active) verdadeiro dentro do "
                 "orcamento\n");
    return EXIT_SUCCESS;
}
