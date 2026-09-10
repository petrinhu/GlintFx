// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <concepts>
#include <cstdint>

#include <glintfx/core/err.hpp>
#include <glintfx/core/time.hpp>
#include <glintfx/platform/gl/context.hpp>

// platform/loop/loop_ports.hpp - LOOP-RUN (cobertura), S2 (/var/tmp/
// glintfx-plan/loop-fix.md sec. S2.2, GODS_LAWS.md L-19: "portas em
// compile-time, fronteira opaca"): the four concepts loop_engine.hpp
// (the motor) is written against, and the aggregate of references that
// lets the motor take all four as ONE four-byte-pointer parameter
// (GODS_LAWS.md L-17: at most 4 parameters).
//
// WHY PORTS OF THE MOTOR'S OWN, NOT THE ONES ALREADY IN src/platform/
// port/ (D-LF-15): (a) the motor consumes exactly five operations and
// a clock (pump_events, wait_events, close_requested, swap_buffers,
// present_would_skip, plus the frame-rate cap) - display_backend_port
// and gl_context_adapter_port (src/platform/port/) ask for far more
// (open/close/make_current/proc_address/apply_option/...), and the cap
// does not even live on the adapter (it is read from gl_context_impl::
// current_values, one layer up - src/platform/loop/loop_context_view.
// hpp's own header comment); (b) the test doubles this sub-fatia needs
// (tests/fake/fake_loop_ports.hpp) do NOT satisfy the wider concepts -
// fake_display_adapter is the deliberate NEGATIVE control of display_
// backend_port (tests/display_backend_port_concept_test.cpp) and MUST
// NOT be given the missing member just to make it fit here; (c) a port
// names exactly what ITS OWN consumer needs, nothing more (GODS_LAWS.md
// L-19). The concepts in src/platform/port/ remain the fronteira with
// the operating system and are untouched by this file; these four are
// an internal seam of the platform/loop/ layer, and live beside the
// motor that is their only consumer.

namespace glintfx::platform {

template <class D>
concept loop_display_port = requires(D &display, std::uint32_t budget_ms) {
    { display.pump_events() } noexcept -> std::same_as<gltfx_rslt<void>>;
    { display.wait_events(budget_ms) } noexcept -> std::same_as<gltfx_rslt<bool>>;
};

template <class W>
concept loop_window_port = requires(const W &window) {
    { window.close_requested() } noexcept -> std::same_as<bool>;
};

template <class C>
concept loop_context_port = requires(C &context, const C &const_context) {
    { context.swap_buffers() } noexcept -> std::same_as<gltfx_rslt<gltfx_present_outcome>>;
    { const_context.present_would_skip() } noexcept -> std::same_as<bool>;
    { const_context.frame_rate_cap_hz() } noexcept -> std::same_as<std::uint32_t>;
};

template <class K>
concept loop_clock_port = requires(K &clock) {
    { clock.now() } noexcept -> std::same_as<gltfx_time_point>;
};

// The aggregate of REFERENCES that lets loop_step()/loop_present()/
// loop_run() (loop_engine.hpp) take all four ports as one trivial,
// pass-by-value parameter (four pointers under the hood - nothing here
// is ever copied, moved, or owned by this struct itself). `window` is
// `const` because the engine only ever READS the window's own close
// latch (loop_window_port above asks for nothing else) - flipping it
// is the operating system's job, done through the real window_state
// object the facade hands in, never through this view.
template <loop_display_port D, loop_window_port W, loop_context_port C, loop_clock_port K>
struct loop_ports {
    D &display;
    const W &window;
    C &context;
    K &clock;
};

} // namespace glintfx::platform
