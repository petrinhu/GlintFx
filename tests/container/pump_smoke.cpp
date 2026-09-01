// SPDX-License-Identifier: AGPL-3.0-or-later
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

#include "platform/wayland/display_adapter.hpp"

// pump_smoke.cpp - WL-DISPLAY fatia D, TDD case D (w4-plano.md
// sec. 3.1.D): "teste em container que faz o pump processar a rajada
// do registry sem bloquear (limite de tempo no teste)". Same staging
// shape as the other WL-DISPLAY container fixtures.
//
// SCOPE, DECLARED HONESTLY: open() (fatia B) already drains the
// initial registry burst through its OWN internal roundtrip() before
// this binary ever calls pump_events() - there is no SECOND burst
// left for the pump to discover against a compositor that announces
// nothing further on its own. What this fixture proves against a REAL
// compositor, and what fatia D's own central claim actually is, is the
// NON-BLOCKING property itself: pump_events() must return promptly
// (bounded wall-clock budget below) whether or not anything is
// pending, repeatedly, safe to call every frame from a consumer's own
// loop - never the classic blocking `while (wl_display_dispatch(...)
// != -1)` shape wl_display(3)'s own manpage warns against.

int main() {
    glintfx::platform::wayland_display_adapter adapter;

    glintfx::gltfx_rslt<void> opened = adapter.open();
    if (opened.has_error()) {
        std::fprintf(stderr, "pump_smoke: open() failed: %s\n",
                     std::string(glintfx::gltfx_err_code_name(opened.error().code())).c_str());
        return EXIT_FAILURE;
    }

    // Budget generous on purpose (w4-plano.md sec. 3.4, risk 5 - "teste
    // com tempo-limite em container lento: o CI compartilhado e mais
    // lento que a maquina local"): 50 pump_events() calls against an
    // idle connection (nothing new pending after open()'s own burst)
    // must ALL return well inside this budget if the pump is genuinely
    // non-blocking - a single BLOCKING call under the old `while
    // (dispatch(...) != -1)` shape would hang here forever (no more
    // events ever arrive on an idle socket), which is exactly the
    // failure this fixture is built to catch.
    constexpr int kIterations = 50;
    constexpr auto kBudget = std::chrono::seconds(5);

    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < kIterations; ++i) {
        glintfx::gltfx_rslt<void> pumped = adapter.pump_events();
        if (pumped.has_error()) {
            std::fprintf(stderr, "pump_smoke: pump_events() iteration %d failed: %s\n", i,
                         std::string(glintfx::gltfx_err_code_name(pumped.error().code())).c_str());
            return EXIT_FAILURE;
        }
        const auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed > kBudget) {
            std::fprintf(stderr,
                         "pump_smoke: iteration %d exceeded the non-blocking budget - "
                         "pump_events() is blocking\n",
                         i);
            return EXIT_FAILURE;
        }
    }
    const auto total_elapsed = std::chrono::steady_clock::now() - start;
    const auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(total_elapsed).count();
    std::fprintf(stdout, "pump_smoke: %d pump_events() call(s) completed in %lldms (budget %lldms)\n",
                 kIterations, static_cast<long long>(total_ms),
                 static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(kBudget).count()));

    if (adapter.has_fatal_error()) {
        std::fprintf(stderr, "pump_smoke: has_fatal_error() true after an all-successful pump loop\n");
        return EXIT_FAILURE;
    }

    adapter.close();
    if (adapter.is_open()) {
        std::fprintf(stderr, "pump_smoke: close() ran but is_open() is still true\n");
        return EXIT_FAILURE;
    }
    std::fprintf(stdout, "pump_smoke: closed cleanly\n");

    return EXIT_SUCCESS;
}
