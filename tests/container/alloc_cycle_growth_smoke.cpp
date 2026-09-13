// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <string>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/platform/gl/gfx_option.hpp>

#include "platform/wayland/display_adapter.hpp"
#include "platform/wayland/egl_context_adapter.hpp"
#include "platform/wayland/shell_adapter.hpp"
#include "platform/wayland/window_adapter.hpp"

#include "alloc_counter_snapshot.hpp"

// alloc_cycle_growth_smoke.cpp - CONTAINER-LEAK-COUNTER sub-fatia S4
// (/var/tmp/glintfx-plan/leak-counter.md sec. 5 S4, GODS_LAWS.md
// L-09/L-20/L-27/L-40/L-43): the 19th fixture, and the only place in
// this whole gate where the `third_party` class stops being MEASURED
// and starts being JUDGED. S1-S3 already prove this library's own code
// never leaves a block alive; this fixture asks a narrower, harder
// question - does opening and closing the SAME four resources
// (display, shell, window, GL context) more than once make the memory
// this library never owned grow WITHOUT BOUND, cycle after cycle.
//
// WHY THREE CYCLES, AND WHY THE FORMULA COMPARES CYCLE 2 TO CYCLE 3,
// NEVER 1 TO 2 (leak-counter.md sec. 4's own third_party_growth_c2_c3
// name, and sec. 6): libLLVM/libgallium (F9) hold GLOBAL, ONE-TIME
// initialization state that the FIRST ever EGL context on this process
// allocates and never frees before exit - cycle 1 always carries that
// one-time cost, so a delta measured from cycle 1 would be positive on
// a perfectly clean tree, for a reason that has nothing to do with a
// leak. Cycle 2 to cycle 3 is the first pair where that one-time cost
// has already happened on BOTH sides of the subtraction - a process
// that has reached steady state by then shows zero growth; a resource
// this library forgets to release EACH cycle (the mutation below)
// keeps growing every time, cycle 2 to cycle 3 included.
//
// THE MUTATION THIS FIXTURE EXISTS TO CATCH (the plan's own obrigatoria
// vermelho de estreia): commenting out the `eglDestroyContext()` call
// inside wayland_egl_context_adapter::close() (src/platform/wayland/
// egl_context_adapter.cpp) - proved in a COPY outside this tracked tree
// (GODS_LAWS.md L-27), never in place. A context leaked every cycle
// grows `third_party_growth_c2_c3` past zero; the service order has the
// raw numbers from that run.
//
// WHY THE RAW ADAPTERS, NOT gltfx_display/gltfx_window/gltfx_gl_context
// (the PUBLIC facade types tests/container/facade_pin_smoke.cpp already
// uses for the first two): the plan names FOUR resources this fixture
// opens and closes each cycle - display, SHELL, window, GL context -
// and the public gltfx_window::open() call binds the shell (xdg_wm_
// base) internally, invisibly to this file. Using the same raw shape
// tests/container/window_smoke.cpp already proves works in this exact
// container (wayland_display_adapter + wayland_shell_adapter +
// wayland_window_adapter, explicit open()/close() pairs for all three)
// makes "shell" a real, separate, visible step here - never folded
// into "window" the way the public facade would fold it. The GL
// context half reuses facade_pin_smoke.cpp's own shape instead
// (wayland_egl_context_adapter allocated on the heap, opened there,
// `delete` to close) - that adapter's own open() signature only ever
// accepts a `wayland_window_adapter&` (egl_context_adapter.hpp:128),
// the exact type this file's own raw `window` object already is.
//
// NO wl_shm BUFFER: unlike window_smoke.cpp, this fixture never
// attaches window_smoke's own real pixel buffer - EGL renders through
// its own EGL-owned Wayland surface, not wl_shm, and this fixture has
// nothing to prove about ack_configure()/protocol errors (window_
// smoke.cpp's own job). Fewer resources opened per cycle is also fewer
// resources whose OWN allocation pattern could be mistaken for the GL
// context's.
//
// THE CONTAINER RESTRICTS THIS TO SOFTWARE RENDERING (GODS_LAWS.md
// L-09 rule 4, no /dev/dri by default): this fixture renders through
// llvmpipe like every other GL fixture in this image - the classifier
// (S1) does not care WHICH driver third_party allocations came from,
// only that they came from outside this executable's own [__
// executable_start, etext) range.
//
// THE CONTAINER'S OWN check_alloc_report.sh (S3) IS WHERE
// third_party_growth_c2_c3 GETS JUDGED, NEVER HERE: this fixture only
// ever reports MEASURED lines and exits EXIT_SUCCESS on a clean run of
// its OWN open/close machinery - the same "o contador RELATA, quem
// reprova e o passo do job" division of labor S2/S3 already established
// (leak-counter.md sec. 7, decisao 5). A growth value is data, not this
// fixture's own verdict.

namespace {

constexpr int kCycles = 3;

struct cycle_result {
    bool ok;
    std::uint64_t third_party_live;
};

// run_one_cycle - opens display, shell, window and a GL context, in
// that order, then closes all four in reverse order, and returns the
// `third_party` live count immediately after the last close() returns
// - BEFORE the next cycle's display.open() can allocate anything new
// (leak-counter.md sec. 5 S4: "apos cada ciclo le os contadores").
[[nodiscard]] cycle_result run_one_cycle(int cycle_number) {
    glintfx::platform::wayland_display_adapter adapter;
    glintfx::gltfx_rslt<void> opened = adapter.open();
    if (opened.has_error()) {
        std::fprintf(stderr, "alloc_cycle_growth_smoke: cycle %d: display open() failed: %s\n",
                     cycle_number,
                     std::string(glintfx::gltfx_err_code_name(opened.err().code())).c_str());
        return {.ok = false, .third_party_live = 0};
    }

    glintfx::platform::wayland_shell_adapter shell;
    glintfx::gltfx_rslt<void> shell_opened = shell.open(adapter);
    if (shell_opened.has_error()) {
        std::fprintf(stderr, "alloc_cycle_growth_smoke: cycle %d: shell.open() failed: %s\n",
                     cycle_number,
                     std::string(glintfx::gltfx_err_code_name(shell_opened.err().code())).c_str());
        adapter.close();
        return {.ok = false, .third_party_live = 0};
    }

    glintfx::platform::wayland_window_adapter window;
    const glintfx::platform::wayland_window_desc desc{
        .logical_width = 320,
        .logical_height = 240,
        .title = "alloc_cycle_growth_smoke",
        .application_id = "org.glintfx.alloc_cycle_growth_smoke",
    };
    glintfx::gltfx_rslt<void> window_opened = window.open(adapter, shell, desc);
    if (window_opened.has_error()) {
        std::fprintf(stderr, "alloc_cycle_growth_smoke: cycle %d: window.open() failed: %s\n",
                     cycle_number,
                     std::string(glintfx::gltfx_err_code_name(window_opened.err().code())).c_str());
        shell.close();
        adapter.close();
        return {.ok = false, .third_party_live = 0};
    }

    auto *egl_ptr = new glintfx::platform::wayland_egl_context_adapter();
    const std::span<const glintfx::gltfx_gfx_option_entry> no_options{};
    if (glintfx::gltfx_rslt<void> egl_opened = egl_ptr->open(window, no_options);
        egl_opened.has_error()) {
        std::fprintf(stderr, "alloc_cycle_growth_smoke: cycle %d: egl adapter open() failed: %s\n",
                     cycle_number,
                     std::string(glintfx::gltfx_err_code_name(egl_opened.err().code())).c_str());
        delete egl_ptr;
        window.close();
        shell.close();
        adapter.close();
        return {.ok = false, .third_party_live = 0};
    }

    // make_current()+swap_buffers() (same pair tests/container/facade_
    // pin_smoke.cpp already calls on this same adapter): MEASURED,
    // live, before this call was added - egl_context_adapter::open()
    // alone never exercises llvmpipe's shader JIT path (libLLVM's own
    // operator-new-visible allocations, F9), so a context leaked by
    // open()/close() alone left third_party_growth_c2_c3 at 0 even
    // with the mutation below applied - the leaked context object
    // itself is malloc'd C state this gancho cannot see (leak-counter.
    // md sec. 6's own declared limitation), and nothing else third-
    // party ever allocated to leak visibly. make_current() is enough
    // to reach that path; swap_buffers() failing (a skipped/hidden
    // window, expected under this container's own budget) is NOT
    // itself a failure of this fixture - only open() failures are.
    if (glintfx::gltfx_rslt<void> current = egl_ptr->make_current(); current.has_error()) {
        std::fprintf(stderr, "alloc_cycle_growth_smoke: cycle %d: make_current() failed: %s\n",
                     cycle_number,
                     std::string(glintfx::gltfx_err_code_name(current.err().code())).c_str());
        delete egl_ptr;
        window.close();
        shell.close();
        adapter.close();
        return {.ok = false, .third_party_live = 0};
    }
    const glintfx::gltfx_rslt<glintfx::gltfx_present_outcome> presented = egl_ptr->swap_buffers();
    if (presented.has_error()) {
        std::fprintf(stderr,
                     "alloc_cycle_growth_smoke: cycle %d: swap_buffers() failed (not fatal): %s\n",
                     cycle_number,
                     std::string(glintfx::gltfx_err_code_name(presented.err().code())).c_str());
    }

    // Close order: GL context first (the resource this fixture exists
    // to watch), then window, shell, display - the reverse of open
    // order, same convention tests/container/window_smoke.cpp already
    // follows for its own three.
    delete egl_ptr; // ~wayland_egl_context_adapter() -> close() -> eglDestroyContext()
    window.close();
    shell.close();
    adapter.close();

    const glintfx_leak_counter::alloc_snapshot snapshot =
        glintfx_leak_counter::alloc_counter_snapshot();
    return {.ok = true, .third_party_live = snapshot.live_third_party};
}

} // namespace

int main() {
    // Unbuffer stdout explicitly - same fix, same reason, applied to
    // every fixture in this family (window_smoke.cpp's own header
    // comment on this exact line).
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    std::uint64_t after_cycle[kCycles] = {0, 0, 0};

    for (int cycle = 1; cycle <= kCycles; ++cycle) {
        const cycle_result result = run_one_cycle(cycle);
        if (!result.ok) {
            return EXIT_FAILURE;
        }
        after_cycle[cycle - 1] = result.third_party_live;
        std::fprintf(stdout, "MEASURED alloc_cycle_growth_smoke.third_party_after_cycle_%d=%llu\n",
                     cycle, static_cast<unsigned long long>(result.third_party_live));
    }

    // Signed: a cycle that FREES more third-party memory than it
    // allocates (some deferred driver cleanup finally running) is a
    // real, honest negative delta - never clamped to zero, GODS_LAWS.md
    // L-43's own "criterio fixado antes do dado" already fixes the
    // criterion as exact equality, in check_alloc_report.sh (S3), not
    // a one-sided inequality decided here.
    const std::int64_t growth =
        static_cast<std::int64_t>(after_cycle[2]) - static_cast<std::int64_t>(after_cycle[1]);
    std::fprintf(stdout, "MEASURED alloc_cycle_growth_smoke.third_party_growth_c2_c3=%lld\n",
                 static_cast<long long>(growth));

    std::fprintf(stdout,
                 "alloc_cycle_growth_smoke: three cycles completed - third_party_after_cycle: "
                 "%llu %llu %llu\n",
                 static_cast<unsigned long long>(after_cycle[0]),
                 static_cast<unsigned long long>(after_cycle[1]),
                 static_cast<unsigned long long>(after_cycle[2]));

    return EXIT_SUCCESS;
}
