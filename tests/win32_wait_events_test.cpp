// SPDX-License-Identifier: AGPL-3.0-or-later
#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <chrono>
#include <cstdint>
#include <print>

#include <glintfx/core/err_code.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/win32/display_adapter.hpp"

// win32_wait_events_test.cpp - LOOP-RUN fatia 7 (docs/plano-w6b-fatias-
// 6-8.md sec. 8.2/§6.1, D-W6b-50, GODS_LAWS.md L-04): the Win32 mirror
// of tests/container/wait_events_smoke.cpp one directory over - proves
// win32_display_adapter::wait_events() genuinely sleeps for up to its
// budget instead of spinning, and genuinely wakes up early when a real
// message arrives, against the REAL windows-latest GitHub Actions
// runner. Same declared limitation every other win32/*_test.cpp
// fixture in this suite carries (GODS_LAWS.md L-27): this project has
// no Windows toolchain on the machine that wrote this file - the
// windows CI job is the first place any of this ever actually runs.
//
// win32_display_adapter has no GLINTFX_API (deliberately - GODS_LAWS.md
// L-19), so this fixture compiles it a SECOND time, directly into its
// own object set - the SAME technique win32_iconic_present_test.cpp's
// own header comment already documents for its own three adapters.

namespace {

[[nodiscard]] long long elapsed_ms_since(std::chrono::steady_clock::time_point start) noexcept {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                                                 start)
        .count();
}

} // namespace

// LEG 1 (§6.1): wait_events(50) with nothing pending must return false
// and take between [40, 500] ms - the lower bound proves the budget is
// actually READ (a mechanism that behaves like pump_events() would
// return instantly and fail only this half); the upper bound is the
// winit #1610 dor (docs/plano-w6b-fatias-6-8.md sec. 1) this project
// refuses to reproduce. §6.2 (iii)'s own tree: a genuinely spurious
// message on an idle queue retries ONCE, printing stray_event=1 - a
// second occurrence fails the case for real.
GLINTFX_TEST(win32_wait_events_no_message_takes_between_40_and_500ms) {
    glintfx::platform::win32_display_adapter display;
    GLINTFX_CHECK(!display.open().has_error());

    int stray_events = 0;
    bool leg_ok = false;
    long long no_event_ms = 0;
    for (int attempt = 0; attempt < 2 && !leg_ok; ++attempt) {
        const auto start = std::chrono::steady_clock::now();
        const glintfx::gltfx_rslt<bool> waited = display.wait_events(50);
        const long long elapsed = elapsed_ms_since(start);
        GLINTFX_CHECK(!waited.has_error());
        if (waited.value() || elapsed < 40) {
            ++stray_events;
            std::println("win32_wait_events_test: stray_event=1 (attempt={} elapsed_ms={})",
                         attempt, elapsed);
            continue;
        }
        GLINTFX_CHECK(elapsed <= 500);
        no_event_ms = elapsed;
        leg_ok = true;
    }
    GLINTFX_CHECK(leg_ok);

    std::println("MEASURED win32_wait_events_test.no_event_ms={}", no_event_ms);
    std::println("MEASURED win32_wait_events_test.stray_event={}", stray_events > 0 ? 1 : 0);
    std::println("win32_wait_events_test: no_event_ms={} (criterio 40..500) OK", no_event_ms);
}

// LEG 2 (§6.1): a real message, provoked by THIS test (PostMessageW
// with hWnd=NULL - a thread message, the SAME scope win32_display_
// adapter::pump_events()'s own header comment already documents for
// PeekMessageW(hWnd=NULL): "any messages on the calling thread's
// message queue whose hwnd value is NULL"), must wake wait_events(1000)
// in under 500ms - never relying on anything spontaneous the runner
// might or might not send.
GLINTFX_TEST(win32_wait_events_provoked_message_wakes_within_500ms) {
    glintfx::platform::win32_display_adapter display;
    GLINTFX_CHECK(!display.open().has_error());

    GLINTFX_CHECK(::PostMessageW(nullptr, WM_APP, 0, 0) != 0);

    const auto start = std::chrono::steady_clock::now();
    const glintfx::gltfx_rslt<bool> waited = display.wait_events(1000);
    const long long event_ms = elapsed_ms_since(start);

    GLINTFX_CHECK(!waited.has_error());
    GLINTFX_CHECK(waited.value());
    GLINTFX_CHECK(event_ms <= 500);

    std::println("MEASURED win32_wait_events_test.event_ms={}", event_ms);
    std::println("win32_wait_events_test: event_ms={} (criterio <500) OK", event_ms);
}

// D-W6b-50's own declared degradation (display_adapter.hpp's own
// header comment on high_resolution_wait()): whether THIS executor's
// OS actually granted the high-resolution waitable timer
// (CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, Windows 10 1803+) is a
// MEASURED, environment-dependent fact - printed, never asserted
// either way (GODS_LAWS.md L-44: this project has no Windows machine
// to measure it against locally).
GLINTFX_TEST(win32_wait_events_reports_high_resolution_wait_capability) {
    glintfx::platform::win32_display_adapter display;
    GLINTFX_CHECK(!display.open().has_error());

    std::println("MEASURED win32_wait_events_test.high_resolution_wait={}",
                 display.high_resolution_wait());
}

#endif // defined(_WIN32)
