// SPDX-License-Identifier: AGPL-3.0-or-later
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

#include <wayland-client.h>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

#include "platform/wayland/display_adapter.hpp"

// wait_events_smoke.cpp - LOOP-RUN fatia 7 (docs/plano-w6b-fatias-6-8.md
// sec. 8.2/§6.1, GODS_LAWS.md L-09): proves wayland_display_adapter::
// wait_events() genuinely SLEEPS for up to its budget instead of
// spinning, against a REAL compositor, and genuinely wakes up EARLY
// when a real event arrives - the two halves of D-W6b-50's own
// contract pump_events() alone never exercised (its own timeout is
// always zero, so it can never prove either bound).
//
// LEG 1 - no event, budget 50ms: must return false and take between
// [40, 500] ms. The LOWER bound is what proves the budget is actually
// READ (a poll(0) returns in microseconds - a wait_events() that
// ignores the argument and behaves like pump_events() would pass a
// "did it return false" check but fail this one); the UPPER bound is
// the winit #1610 dor (docs/plano-w6b-fatias-6-8.md sec. 1) this
// project refuses to reproduce - a wait that overshoots its own
// budget by 10x is not "eventually correct", it is the exact defect
// this fixture exists to catch.
//
// LEG 2 - a real event, provoked by THIS test (wl_display_sync's own
// round-trip reply, never anything spontaneous the environment might
// or might not send): wait_events(1000) must observe it and return in
// under 500ms.
//
// §6.2 (iii)'s own tree: a genuinely spurious event on an idle
// connection during leg 1 (something the compositor sent unprompted)
// retries ONCE, printing stray_event=1 - a second occurrence fails the
// fixture for real, never silently absorbed.

namespace {

struct sync_state {
    bool done = false;
};

void on_sync_done(void *data, wl_callback *callback, std::uint32_t /*serial*/) {
    auto *state = static_cast<sync_state *>(data);
    state->done = true;
    wl_callback_destroy(callback);
}

constexpr wl_callback_listener kSyncListener = {.done = &on_sync_done};

} // namespace

int main() {
    // Unbuffer stdout explicitly - same fix, same reason, as every
    // other fixture in this family (connect_smoke.cpp's own header
    // comment).
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    glintfx::platform::wayland_display_adapter adapter;
    const glintfx::gltfx_rslt<void> opened = adapter.open();
    if (opened.has_error()) {
        std::fprintf(stderr, "wait_events_smoke: open() failed: %s\n",
                     std::string(glintfx::gltfx_err_code_name(opened.err().code())).c_str());
        return EXIT_FAILURE;
    }

    int stray_events = 0;
    bool leg1_ok = false;
    long long no_event_ms = 0;
    for (int attempt = 0; attempt < 2 && !leg1_ok; ++attempt) {
        const auto start = std::chrono::steady_clock::now();
        const glintfx::gltfx_rslt<bool> waited = adapter.wait_events(50);
        const long long elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                         std::chrono::steady_clock::now() - start)
                                         .count();
        if (waited.has_error()) {
            std::fprintf(stderr, "wait_events_smoke: wait_events(50) failed: %s\n",
                         std::string(glintfx::gltfx_err_code_name(waited.err().code())).c_str());
            return EXIT_FAILURE;
        }
        if (waited.value() || elapsed_ms < 40) {
            ++stray_events;
            std::fprintf(stdout, "wait_events_smoke: stray_event=1 (attempt=%d elapsed_ms=%lld)\n",
                         attempt, elapsed_ms);
            continue;
        }
        if (elapsed_ms > 500) {
            std::fprintf(stderr,
                         "wait_events_smoke: no_event_ms=%lld (criterio 40..500) FAIL "
                         "(attempt=%d)\n",
                         elapsed_ms, attempt);
            return EXIT_FAILURE;
        }
        no_event_ms = elapsed_ms;
        leg1_ok = true;
    }
    if (!leg1_ok) {
        std::fprintf(stderr,
                     "wait_events_smoke: leg 1 never observed a clean no-event wait (stray_event "
                     "twice)\n");
        return EXIT_FAILURE;
    }

    sync_state state;
    wl_callback *sync_callback = wl_display_sync(adapter.native_display());
    if (sync_callback == nullptr) {
        std::fprintf(stderr, "wait_events_smoke: wl_display_sync() returned null\n");
        return EXIT_FAILURE;
    }
    wl_callback_add_listener(sync_callback, &kSyncListener, &state);

    const auto start2 = std::chrono::steady_clock::now();
    const glintfx::gltfx_rslt<bool> waited2 = adapter.wait_events(1000);
    const long long event_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::steady_clock::now() - start2)
                                   .count();
    if (waited2.has_error()) {
        std::fprintf(stderr, "wait_events_smoke: wait_events(1000) failed: %s\n",
                     std::string(glintfx::gltfx_err_code_name(waited2.err().code())).c_str());
        return EXIT_FAILURE;
    }
    if (!state.done) {
        std::fprintf(stderr,
                     "wait_events_smoke: the provoked sync callback never arrived within 1000ms\n");
        return EXIT_FAILURE;
    }
    if (event_ms > 500) {
        std::fprintf(stderr, "wait_events_smoke: event_ms=%lld (criterio <500) FAIL\n", event_ms);
        return EXIT_FAILURE;
    }

    // Camada 1 (§6.3): os numeros crus, sempre, mesmo quando o desfecho
    // e' zero.
    std::fprintf(stdout, "MEASURED wait_events_smoke.no_event_ms=%lld\n", no_event_ms);
    std::fprintf(stdout, "MEASURED wait_events_smoke.event_ms=%lld\n", event_ms);
    std::fprintf(stdout, "MEASURED wait_events_smoke.stray_event=%d\n", stray_events > 0 ? 1 : 0);
    // Camada 2 (§6.3): o criterio aplicado, com veredito, na mesma
    // linha - para o revisor ler sem recalcular.
    std::fprintf(stdout, "wait_events_smoke: no_event_ms=%lld (criterio 40..500) OK\n",
                 no_event_ms);
    std::fprintf(stdout, "wait_events_smoke: event_ms=%lld (criterio <500) OK\n", event_ms);
    std::fprintf(stdout, "wait_events_smoke: assercoes 2 de 2 avaliadas\n");

    if (adapter.has_fatal_error()) {
        std::fprintf(stderr,
                     "wait_events_smoke: has_fatal_error() true after an all-successful run\n");
        return EXIT_FAILURE;
    }

    adapter.close();
    if (adapter.is_open()) {
        std::fprintf(stderr, "wait_events_smoke: close() ran but is_open() is still true\n");
        return EXIT_FAILURE;
    }
    std::fprintf(stdout, "wait_events_smoke: closed cleanly\n");

    return EXIT_SUCCESS;
}
