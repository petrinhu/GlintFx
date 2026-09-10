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
// THE ONE OTHER SITE UNDER src/platform/loop/ THAT STILL READS gltfx_
// now() DIRECTLY: gltfx_loop::open() (loop_facade.cpp) - it runs once,
// before any loop_step()/loop_present()/loop_run() call, so it has no
// ports object to go through yet (tests/wait_points.txt's own updated
// comment on this file names the same split).
namespace glintfx::platform {

struct steady_loop_clock {
    [[nodiscard]] gltfx_time_point now() const noexcept { return gltfx_now(); }
};

} // namespace glintfx::platform
