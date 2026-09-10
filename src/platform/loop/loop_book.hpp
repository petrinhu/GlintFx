// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

#include <glintfx/core/time.hpp>
#include <glintfx/platform/gl/context.hpp>

#include "platform/loop/frame_cap_schedule.hpp"

// platform/loop/loop_book.hpp - LOOP-RUN (cobertura), S2 (/var/tmp/
// glintfx-plan/loop-fix.md sec. S2.2, GODS_LAWS.md L-17/L-19): the four
// pieces of bookkeeping the loop engine (loop_engine.hpp) remembers
// BETWEEN calls - previously four separate fields directly on loop_impl
// (src/platform/loop/loop_impl.hpp), grouped here under one name
// because they are read and written by the SAME functions (loop_step()
// /loop_present(), loop_engine.hpp) for the SAME reason: they are the
// engine's own memory of "what happened last time", never touched by
// anything outside it. loop_impl still owns the STORAGE (a single
// `loop_book book;` member) - only the GROUPING moved; nothing about
// lifetime or ownership changed.
//
// LOOP-CONTEXT-OWNERSHIP (S1b, a LATER sub-fatia, not this one) adds
// `running` and `stored_context` HERE, not on the facade - loop_book is
// where the engine's own state lives, and posse-tracking is exactly
// that kind of state (/var/tmp/glintfx-plan/loop-fix.md sec. S1b's own
// table names this file directly).

namespace glintfx::platform {

struct loop_book {
    // P7 (D-W6b-49): the one piece of state frame_cap_schedule's own
    // class carries (m_has_deadline/m_next_deadline).
    frame_cap_schedule cap_schedule;

    // The previous tick's own clock reading - compute_frame_tick()'s
    // own `previous_now` argument every loop_step() (P5: elapsed is
    // measured from here). Zero-valued (the same "zero on the very
    // first tick" gltfx_frame_tick::elapsed already promises) until the
    // first real loop_step() overwrites it.
    gltfx_time_point previous_now{};

    // What the last loop_present() call actually returned - `presented`
    // before the very first one (the same default gltfx_frame_tick::
    // last_present already documents, platform/loop/loop.hpp), fed
    // straight into compute_frame_tick()'s own `last_present` argument
    // every loop_step().
    gltfx_present_outcome last_present = gltfx_present_outcome::presented;

    // The previous tick's own frame_index - 0 before the very first
    // loop_step() (P2: the first tick this loop ever produces reports
    // 1, this field plus one).
    std::uint64_t frame_index = 0;
};

} // namespace glintfx::platform
