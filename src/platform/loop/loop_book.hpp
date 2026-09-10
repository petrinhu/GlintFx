// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

#include <glintfx/core/time.hpp>
#include <glintfx/platform/gl/context.hpp>
#include <glintfx/platform/loop/loop.hpp>

#include "platform/loop/frame_cap_schedule.hpp"
#include "platform/loop/owned_loop_context.hpp"

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
// LOOP-CONTEXT-OWNERSHIP (S1b, /var/tmp/glintfx-plan/loop-fix.md sec.
// 3.2/S1b) ADDS `running`, `stored_callbacks` and `stored_context`
// HERE, not on the facade - loop_book is where the engine's own state
// lives, and posse-tracking (FORM 2, "posse pelo laço", loop.hpp's own
// (b2)) is exactly that kind of state: `running` is what platform::
// loop_run's own running_guard (loop_engine.hpp) arms and disarms
// every call, `stored_callbacks`/`stored_context` are what store_loop_
// callbacks() (store_loop_callbacks.hpp/.cpp) fills in on behalf of
// gltfx_loop::set_callbacks() - and what loop_run() itself reads back,
// UNCHANGED, when gltfx_loop::run() is called with no arguments
// (loop_facade.cpp). Grouped as three fields of the SAME struct, never
// three loose facade members, because store_loop_callbacks() always
// updates all three together (running is read, the other two are
// written as one unit) - splitting them across files would scatter one
// assunto (GODS_LAWS.md L-17).

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

    // LOOP-CONTEXT-OWNERSHIP (S1b), D-LF-6d: armed by platform::
    // running_guard (running_guard.hpp) for the duration of ONE outer
    // loop_run() call, disarmed unconditionally on every return path -
    // a re-entrant run()/run(callbacks)/set_callbacks() called from
    // INSIDE this consumer's own on_frame/on_render (loop_engine.hpp's
    // own loop_run(), store_loop_callbacks.hpp's own store_loop_
    // callbacks()) is refused BY NAME ("running") while this is true,
    // rather than destroying a context the outer call is still using.
    bool running = false;

    // LOOP-CONTEXT-OWNERSHIP (S1b), FORM 2 (loop.hpp's own (b2)): what
    // gltfx_loop::set_callbacks() last stored, through store_loop_
    // callbacks() - read back UNCHANGED by gltfx_loop::run() (no
    // arguments, loop_facade.cpp) and handed to platform::loop_run() by
    // value, the EXACT same way FORM 1 (run(callbacks)) hands its own
    // argument in. Empty (a default-constructed gltfx_loop_callbacks,
    // on_frame == nullptr) until the first successful set_callbacks().
    gltfx_loop_callbacks stored_callbacks{};

    // LOOP-CONTEXT-OWNERSHIP (S1b), FORM 2's own posse: the SAME atom
    // FORM 1 builds fresh on run(callbacks)'s own stack every call
    // (owned_loop_context.hpp's own header comment) - here, it lives as
    // long as this loop_book does, so the context it may own survives
    // any number of loop_run() calls and is destroyed only when store_
    // loop_callbacks() substitutes it (reset()) or when this loop_book
    // - and the loop_impl that owns it - is itself destroyed (F29: a
    // member with a destructor reaches both of loop_facade.cpp's own
    // delete sites without that file changing at all).
    owned_loop_context stored_context;
};

} // namespace glintfx::platform
