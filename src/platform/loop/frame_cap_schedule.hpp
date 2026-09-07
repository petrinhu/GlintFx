// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

#include <glintfx/core/time.hpp>

// platform/loop/frame_cap_schedule.hpp - LOOP-RUN fatia 6a (docs/
// plano-w6b-fatias-6-8.md D-W6b-49, GODS_LAWS.md L-17/L-19/L-20): the
// pure atom that decides HOW LONG to wait before the next frame a
// frame-rate cap allows, without ever sleeping or reading a real clock
// itself - the same "the caller supplies `now`, the atom only decides"
// division of labor src/platform/wayland/frame_callback_sequence.hpp
// (one directory over) already establishes for a different budget.
//
// GUARDS THE DEADLINE, NEVER THE INSTANT (the whole contract): plan()
// always advances from the PREVIOUS deadline plus one exact period -
// never from the `now` a caller happened to measure - so 300
// consecutive periods at 60 Hz sum to 300 times the SAME rounded
// period, not to 300 independently-measured intervals that could
// drift apart from it (frame_cap_schedule_test.cpp's own "sem deriva"
// case). Recomputing the deadline as "now + period" every call, the
// obvious-looking alternative, would reintroduce exactly that drift -
// each call's own measured `now` already carries whatever jitter the
// caller's own pump/wait took, and that jitter would compound.
//
// TWO SAFETY VALVES, BOTH READ FROM THE PRIOR-ART SURVEY (docs/plano-
// w6b-fatias-6-8.md sec. 1 - Godot #99728, raylib #5134/#1634): a
// frame arriving more than one whole period late reanchors on `now`
// directly, rather than advancing one period at a time (which would
// need one call per missed period, for no benefit - the loop would
// rather present the late frame now and resume the cadence from
// here); and a clock reading that steps BACKWARD relative to a
// deadline already armed never produces a wait longer than one period,
// so a clock glitch can never park a caller in an unbounded sleep.

namespace glintfx::platform {

class frame_cap_schedule {
  public:
    frame_cap_schedule() noexcept = default;

    // `cap_hz == 0` is "no cap": always returns 0 and forgets whatever
    // deadline was armed, so a cap re-enabled later starts a FRESH
    // schedule anchored on whatever `now` THAT later call supplies -
    // never a stale deadline computed under a cap that has not run in
    // a while. `cap_hz > 0`: the milliseconds the caller should wait
    // before it is time to present again - 0 means "present now".
    [[nodiscard]] std::uint32_t plan(gltfx_time_point now, std::uint32_t cap_hz) noexcept;

  private:
    bool m_has_deadline = false;
    gltfx_time_point m_next_deadline{};
};

} // namespace glintfx::platform
