// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

#include <glintfx/core/err.hpp>
#include <glintfx/core/time.hpp>
#include <glintfx/platform/loop/loop.hpp>

#include "platform/loop/frame_tick_state.hpp"
#include "platform/loop/loop_book.hpp"
#include "platform/loop/loop_callbacks_validation.hpp"
#include "platform/loop/loop_ports.hpp"
#include "platform/loop/owned_loop_context.hpp"
#include "platform/loop/running_guard.hpp"

// platform/loop/loop_engine.hpp - LOOP-RUN (cobertura, S2) +
// LOOP-CLOSE-LATCH-SPIN (fixed here, S3) (/var/tmp/glintfx-plan/
// loop-fix.md sec. S2.2/S3, GODS_LAWS.md L-17/L-19/L-20): the loop's
// own body, extracted out of the facade (loop_facade.cpp) into a
// function-TEMPLATE over the four ports (loop_ports.hpp) - chamável sem
// sistema operacional, sem display, sem janela, sem contexto GL,
// exercitada diretamente por tests/loop_engine_test.cpp.
//
// FRASE SEM "E" (L-17): o corpo do laço sai da fachada e vira função-
// modelo sobre portas de compilação.
//
// LOOP-CLOSE-LATCH-SPIN, FIXED (S3, this commit): loop_step() used to
// read close_requested() on every tick and skip the hidden-window wait
// and the frame-rate cap once it read true. Since the latch never
// resets (window_state.hpp: "There is no unset_close_request()"), a
// consumer that VETOES the close (its own on_frame keeps returning
// true after the system asked to close - the point of D-W6b-54's own
// veto path) made every tick from the first close request onward skip
// BOTH waits forever: the loop kept spinning, pumping and rebuilding
// ticks with no wait in between, burning CPU and battery for as long
// as that process ran. The read is REMOVED here, not narrowed or
// rate-limited - the plan's own S3 measured that removing it costs
// nothing on the path that already worked: loop_run() (below) already
// returns at the END of the very tick in which the close request
// arrived (its own close_requested() check runs AFTER on_frame/
// on_render/loop_present() for that tick, never before), so the read
// this file used to do inside loop_step() only ever saved ONE wait,
// once, on the way out - never more, because there never was a second
// tick to skip a wait in in the non-veto path. tests/loop_engine_test.
// cpp's own T10 (close_latched_consumer_keeps_stepping_waits_still_
// happen) is the machine proof: it is UNCHANGED by this commit and
// goes green because the code under it changed (L-20's own red-before-
// green shape).
//
// LOOP-CONTEXT-OWNERSHIP (S1b, this commit, /var/tmp/glintfx-plan/
// loop-fix.md sec. 3.2/S1b): loop_run() below now takes a FOURTH
// parameter, `owned_loop_context &owned` (owned_loop_context.hpp) -
// the SAME atom that guards a handed-over context, whichever of the
// two forms (loop.hpp's own (b1)/(b2)) the caller is exercising. This
// function's own BODY never reads or writes `owned` - it exists purely
// so the atom's own LIFETIME is provably pinned to this exact call, on
// the CALLER's stack (FORM 1, gltfx_loop::run(callbacks), builds one
// fresh every call; FORM 2, gltfx_loop::run() with no arguments, hands
// in an EMPTY one - the real atom lives in loop_book::stored_context
// instead, loop_book.hpp's own header comment). C++'s own rule for
// local objects - destroyed only after the return EXPRESSION that
// names them has fully evaluated - is what already guarantees
// `owned`'s own destructor (whichever form built it) runs strictly
// AFTER every callback THIS call makes, on every one of its five
// return paths (tests/loop_engine_test.cpp's own T13); nothing inside
// this function needs to, or should, act on that fact itself.
// Re-entrance is the ONE thing this function DOES actively guard
// (D-LF-6d, below) - see running_guard.hpp's own header comment for
// why that check cannot be left to the caller's stack the way
// destruction can.
//
// WHAT THIS FILE STILL DOES NOT DO, stated so nobody infers it from an
// absence: does NOT change any other observable behavior of gltfx_
// loop's own public methods; does NOT decide which of the two forms it
// is serving - `callbacks`/`owned` arrive already shaped by the
// caller, and this function reads neither differently (§3.2's own
// "de modo que o motor nunca sabe em que forma está").
//
// ORDER INSIDE loop_step() (D-W6b-44 steps 1/3/4/5/6 - step 2, the
// close_requested() read above, is GONE as of S3, so this list keeps
// the surviving steps' own original numbers rather than renumbering
// and silently hiding that a step was removed): (1) pump the system's
// own events; (3) if the LAST present() skipped a hidden window, wait
// a fixed budget, pump again - unconditionally, whether or not a close
// is pending; (4) if a frame-rate cap is set, wait for it (hybrid
// budget: wait_events() in coarse chunks, then a bounded spin for the
// last <= 1 ms) - same, unconditionally; (5) read the clock; (6) build
// the tick via the pure atom (frame_tick_state.hpp). ORDER INSIDE
// loop_run() (D-W6b-54): loop_step() -> (on_event, reserved) ->
// on_frame -> if should_render: on_render -> loop_present() -> only
// THEN check close_requested().
//
// THE CLOCK IS INJECTED (D-LF-4) - a fourth port, loop_clock_port
// (loop_ports.hpp), with a single `now()` member. Production:
// steady_loop_clock.hpp (one line over gltfx_now(), zero cost). Test:
// tests/fake/fake_loop_ports.hpp's own fake_loop_clock. THREE REASONS:
//   1. T10/T11/T12 (tests/loop_engine_test.cpp) count what a wait DID,
//      never how long it took - with a real clock, the frame-rate-cap
//      tests would depend on the scheduler's own grain on five
//      different systems and become intermittent, which in a suite
//      that runs on all five is the same as not existing.
//   2. The final, <= 1 ms spin proves FINITE by counting reads, not by
//      a stopwatch: a fake clock that advances a fixed step per read
//      makes the `while (wait_ms > 0)` loop end after a KNOWN number of
//      turns; an engine that spun forever would show up as a test that
//      never terminates, never as "took longer than expected".
//   3. It is the SAME seam the wait-point manifest needs (tests/
//      wait_points.txt): the motor's own clock-reading needle becomes
//      `clock.now(` and the sites live, named, in THIS file (tests/
//      tools/check_wait_points.py's own updated TARGET_FILES/
//      SIMPLE_NEEDLES).
// WHAT THE INJECTION DOES NOT CHANGE: gltfx_loop::open() (loop_facade.
// cpp) still reads gltfx_now() directly into loop_book::previous_now -
// it runs once, outside any loop_step()/loop_present()/loop_run() call,
// so it has no ports object to go through; and no consumer of this
// library ever sees a clock port - none of this is public.

namespace glintfx::platform {

// D-W6b-44 step 3's own budget - the SAME number egl_context_adapter.
// cpp's own frame-callback budget already uses (D-W6b-6), so a
// consumer reads exactly ONE "hidden cadence" number across this
// library's own loop.hpp header and that adapter's - never two budgets
// that could silently drift apart from each other.
inline constexpr std::uint32_t k_hidden_wait_budget_ms = 100;

// D-W6b-44 step 3: the tick right after a present() skipped a hidden
// window waits with a fixed budget instead of spinning, then pumps
// once more - giving the compositor a chance to deliver whatever woke
// the connection (a frame callback, a `configure` clearing `suspended`)
// before this tick's own probe (loop_step(), below) decides whether the
// window is visible again. Never re-presents here: loop_present()
// (below) is the only place that ever happens.
template <loop_display_port D>
[[nodiscard]] gltfx_rslt<void> wait_while_hidden(D &display) noexcept {
    if (const gltfx_rslt<bool> waited = display.wait_events(k_hidden_wait_budget_ms);
        waited.has_error()) {
        return gltfx_rslt<void>::err(waited.err());
    }
    return display.pump_events();
}

// D-W6b-44 step 4 / D-W6b-49: the hybrid frame-rate-cap wait -
// wait_events(grosso) (grosso = what frame_cap_schedule::plan() itself
// already reports minus the last 1 ms) repeated while more than 1 ms
// remains, then a tight spin on clock.now() for the last <= 1 ms -
// never a sleep for that final sliver. plan() itself only ADVANCES its
// own deadline once `now` has actually reached it (frame_cap_schedule.
// hpp's own header comment, "guards the DEADLINE, never the instant") -
// calling it repeatedly with a fresh `now` while the deadline has not
// arrived yet is side-effect-free, so both loops below are safe to
// spin.
template <loop_display_port D, loop_clock_port K>
[[nodiscard]] gltfx_rslt<void> wait_for_frame_cap(D &display, frame_cap_schedule &schedule,
                                                  std::uint32_t cap_hz, K &clock) noexcept {
    std::uint32_t wait_ms = schedule.plan(clock.now(), cap_hz);
    while (wait_ms > 1) {
        if (const gltfx_rslt<bool> waited = display.wait_events(wait_ms - 1); waited.has_error()) {
            return gltfx_rslt<void>::err(waited.err());
        }
        if (const gltfx_rslt<void> pumped = display.pump_events(); pumped.has_error()) {
            return pumped;
        }
        wait_ms = schedule.plan(clock.now(), cap_hz);
    }
    // Last sliver (<= 1 ms): a tight spin on the exact clock, never a
    // sleep (GODS_LAWS.md L-04/D-W6b-49/tests/wait_points.txt, `teto-
    // nosso`: a real, finite budget - `wait_ms` cannot exceed one
    // period entering this loop, frame_cap_schedule_test.cpp's own
    // "sem deriva" case already proves plan()'s own deadline only ever
    // advances, never regresses). Named `spin_now`, a DIFFERENT literal
    // expression from the two `wait_ms = schedule.plan(clock.now(), ...)`
    // calls above (deliberately - tests/tools/check_wait_points.py's
    // own manifest matches by exact substring per FILE, and this
    // spin's own role - the actual bounded wait, `teto-nosso` - is NOT
    // the same as the two calls above, which never themselves wait,
    // `nao-espera`: identical text would force one shared
    // classification that could not honestly be both). plan() itself
    // is what tells this loop the deadline was finally reached (return
    // value 0), consuming/advancing its own internal deadline in the
    // SAME call that ends this spin.
    while (wait_ms > 0) {
        const gltfx_time_point spin_now = clock.now();
        wait_ms = schedule.plan(spin_now, cap_hz);
    }
    return gltfx_rslt<void>::ok();
}

// One tick over `ports` - see this file's own top comment for the
// exact, numbered order. Never calls on_frame/on_render/loop_present()
// itself - that is loop_run()'s own job, below.
template <class D, class W, class C, class K>
[[nodiscard]] gltfx_rslt<gltfx_frame_tick> loop_step(loop_ports<D, W, C, K> ports,
                                                     loop_book &book) noexcept {
    // Step 1 (D-W6b-44): pump is always the FIRST thing a tick does
    // (P3) - the state read below already includes whatever the system
    // delivered before this tick.
    if (const gltfx_rslt<void> pumped = ports.display.pump_events(); pumped.has_error()) {
        return gltfx_rslt<gltfx_frame_tick>::err(pumped.err());
    }

    // Step 2 used to be a close_requested() read here that gated steps
    // 3/4 below - REMOVED by LOOP-CLOSE-LATCH-SPIN (S3, this file's own
    // top comment has the full "why": close_requested() is a one-way
    // latch (window_state.hpp), so gating on it here made both waits
    // stop running FOREVER after the first close request, even for a
    // consumer that vetoes the close and keeps stepping - loop_run()'s
    // own close check (below, after this tick's on_frame/on_render/
    // loop_present() already ran) is already what ends the loop on the
    // non-veto path, so this read never bought more than one skipped
    // wait, once, on the way out.

    // Step 3 (D-W6b-44): last present() skipped a hidden window - wait
    // with a fixed budget, pump again, before this tick's own probe
    // (below) decides visibility. Runs on EVERY tick now, close pending
    // or not - a pending-but-vetoed close still needs the compositor
    // fed while it waits for the consumer to decide.
    if (book.last_present == gltfx_present_outcome::skipped_hidden) {
        if (const gltfx_rslt<void> waited = wait_while_hidden(ports.display); waited.has_error()) {
            return gltfx_rslt<gltfx_frame_tick>::err(waited.err());
        }
    }

    // Step 4 (D-W6b-44): a frame-rate cap, if the consumer asked for
    // one (P7) - coexists with vsync (whichever is slower wins, by
    // construction: this loop never touches vsync at all). Also runs
    // on every tick now, for the same reason as step 3 above.
    const std::uint32_t cap_hz = ports.context.frame_rate_cap_hz();
    if (cap_hz > 0) {
        if (const gltfx_rslt<void> capped =
                wait_for_frame_cap(ports.display, book.cap_schedule, cap_hz, ports.clock);
            capped.has_error()) {
            return gltfx_rslt<gltfx_frame_tick>::err(capped.err());
        }
    }

    // Steps 5/6: read the clock, build the tick through the pure atom
    // (P2/P4/P5) - present_would_skip() is the "would the NEXT
    // loop_present() skip" sonda D-W6b-46 gives this loop, never a real
    // presentation of its own.
    const gltfx_time_point now = ports.clock.now();
    const bool probe_says_visible = !ports.context.present_would_skip();
    const gltfx_frame_tick tick = compute_frame_tick(book.previous_now, now, book.frame_index,
                                                     book.last_present, probe_says_visible);

    book.previous_now = now;
    book.frame_index = tick.frame_index;

    return gltfx_rslt<gltfx_frame_tick>::ok(tick);
}

// Presents through `ports` - the ONLY way to present INSIDE this loop's
// own contract (P1): the result feeds the NEXT loop_step()'s own
// gltfx_frame_tick::last_present and should_render (P4).
template <class D, class W, class C, class K>
[[nodiscard]] gltfx_rslt<gltfx_present_outcome> loop_present(loop_ports<D, W, C, K> ports,
                                                             loop_book &book) noexcept {
    gltfx_rslt<gltfx_present_outcome> presented = ports.context.swap_buffers();
    if (presented.has_value()) {
        // P4: last_present is copied straight through, never
        // re-derived - the NEXT loop_step()'s own compute_frame_tick()
        // call reads exactly this value.
        book.last_present = presented.value();
    }
    return presented;
}

// Sugar over loop_step()/on_frame/on_render/loop_present() - see this
// file's own top comment for the exact order and P9's own refusal
// rules (validate_loop_callbacks(), loop_callbacks_validation.hpp).
// `callbacks` is received BY VALUE (five trivial pointers, cheap to
// copy - the same frozen layout LOOP-CALLBACK-THROW's own S1a
// congelou). `owned` is this file's own top comment's fourth
// parameter - accepted, never touched in this function's own body
// ([[maybe_unused]] below is honest about that, not a placeholder for
// work not yet written).
template <class D, class W, class C, class K>
[[nodiscard]] gltfx_rslt<void> loop_run(loop_ports<D, W, C, K> ports, loop_book &book,
                                        gltfx_loop_callbacks callbacks,
                                        [[maybe_unused]] owned_loop_context &owned) noexcept {
    // D-LF-6d: re-entrance is refused BY NAME, before validate_loop_
    // callbacks() and before ANY port is touched - the FIRST thing this
    // function does, ahead even of T1's own "recusa por nome acontece
    // antes de qualquer porta ser tocada". A consumer whose on_frame/
    // on_render calls run()/run(callbacks)/set_callbacks() again, on
    // the SAME loop, would otherwise - on FORM 2 - destroy the very
    // context THIS call is still executing (running_guard.hpp's own
    // header comment has the full reasoning; tests/loop_engine_test.
    // cpp's own T16 is the proof). Checked directly against `book.
    // running` HERE, before constructing the guard below - see running_
    // guard.hpp's own header comment for why the refusal decision never
    // lives inside that class itself.
    if (book.running) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value("running"));
    }
    running_guard guard{book.running};

    // T1: recusa por nome acontece ANTES de qualquer porta ser tocada -
    // nenhum `ports.*` é lido nesta função antes desta chamada.
    if (const gltfx_rslt<void> validated = validate_loop_callbacks(callbacks);
        validated.has_error()) {
        return validated;
    }

    for (;;) {
        const gltfx_rslt<gltfx_frame_tick> ticked = loop_step(ports, book);
        if (ticked.has_error()) {
            return gltfx_rslt<void>::err(ticked.err());
        }
        const gltfx_frame_tick &tick = ticked.value();

        // (on_event delivery is RESERVED here, D-W6b-24/P3 - lands with
        // INPUT-EVENTS, W7; validate_loop_callbacks() above already
        // refused a caller that filled it in.)

        if (!callbacks.on_frame(callbacks.context, tick)) {
            return gltfx_rslt<void>::ok();
        }

        if (tick.should_render) {
            callbacks.on_render(callbacks.context, tick);
            const gltfx_rslt<gltfx_present_outcome> presented = loop_present(ports, book);
            if (presented.has_error()) {
                return gltfx_rslt<void>::err(presented.err());
            }
        }

        // P9: ends when the system asks to close - checked AFTER this
        // tick's own on_frame/on_render/loop_present() already ran,
        // never before (a consumer's own on_frame/on_render still gets
        // one full, ordinary tick to react to the close request before
        // run() actually returns - never a request silently swallowed
        // on the tick it arrived).
        if (ports.window.close_requested()) {
            return gltfx_rslt<void>::ok();
        }
    }
}

} // namespace glintfx::platform
