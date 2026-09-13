// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/core/time.hpp>

// platform/loop/steady_loop_clock.hpp - LOOP-RUN (cobertura), S2
// (/var/tmp/glintfx-plan/loop-fix.md sec. S2.2, D-LF-4): the PRODUCTION
// loop_clock_port (loop_ports.hpp) - one method, one call, the SAME
// gltfx_now() reading loop_facade.cpp's own step() used to make
// directly before this sub-fatia. Costs nothing over calling
// gltfx_now() directly: no state, a single inline forwarding call.
//
// WHY THE CLOCK IS A PORT AT ALL, not just gltfx_now() called inline
// inside the motor (three reasons, told in full in loop_engine.hpp's
// own header comment): tests/loop_engine_test.cpp's timing-sensitive
// cases (T10/T11/T12) need a clock a test can single-step and reason
// about by COUNT, never by wall time - a real clock would make them
// intermittent under a scheduler this project does not control.
//
// NO OTHER SITE UNDER src/platform/loop/ READS gltfx_now() DIRECTLY
// (LOOP-FIRST-TICK-ELAPSED, 13/09/2026): gltfx_loop::open() (loop_
// facade.cpp) used to be the one exception - it ran once, before any
// loop_step()/loop_present()/loop_run() call, stamping impl->book.
// previous_now with a real wall-clock instant. That write is gone: it
// was dead the moment compute_frame_tick() (frame_tick_state.cpp)
// started ignoring previous_now on the first tick (previous_frame_
// index == 0), and before that it let the consumer's own loading time
// leak into the loop's first elapsed as one large step. This port is
// now the ONLY reader of gltfx_now() under this directory.
namespace glintfx::platform {

struct steady_loop_clock {
    [[nodiscard]] gltfx_time_point now() const noexcept { return gltfx_now(); }
};

} // namespace glintfx::platform
