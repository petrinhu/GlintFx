// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cassert>
#include <cstdint>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/core/time.hpp>
#include <glintfx/platform/gl/context.hpp>
#include <glintfx/platform/gl/gfx_option.hpp>
#include <glintfx/platform/loop/loop.hpp>
#include <glintfx/platform/window/display.hpp>
#include <glintfx/platform/window/window.hpp>

#include "platform/gl/gl_context_impl.hpp"
#include "platform/loop/frame_tick_state.hpp"
#include "platform/loop/loop_callbacks_validation.hpp"
#include "platform/loop/loop_impl.hpp"
#include "platform/window/display_impl.hpp"
#include "platform/window/window_impl.hpp"

// loop_facade.cpp - LOOP-RUN fatia 6b (docs/plano-w6b-fatias-6-8.md
// sec. 8.3, D-W6b-41/42/43/44/45/49/54/55, GODS_LAWS.md L-17/L-19/
// L-22): the ONE translation unit that turns the frozen surface
// include/glintfx/platform/loop/loop.hpp declares into a real handle -
// the exact same "allocation and deallocation on the SAME side of the
// boundary" shape display_facade.cpp/window_facade.cpp/gl_context_
// facade.cpp already give their own opaque handles.
//
// FACADE-PIN, ALLOCATE FIRST, VALIDATE THEN FILL IN (the same shape
// window_facade.cpp's own open() already uses, one directory over):
// open() below checks the three preconditions (P10) BEFORE ever
// allocating - display_impl/window_impl/gl_context_impl.hpp's own
// selected_*_adapter types are all pinned (platform::pinned_adapter<A>,
// FACADE-PIN), but loop_impl itself owns none of the three foreign
// adapters directly - it only ever stores POINTERS into them
// (loop_impl.hpp's own header comment) - so there is no "adapter
// registers its own address with the operating system before the move
// completes" hazard here at all, the class of bug FACADE-PIN exists to
// close one layer down. allocate_loop_impl() (loop_impl.hpp/.cpp) is
// still its own atom, extracted purely for TESTABILITY - see that
// header's own comment on the function for the full reasoning.
//
// EACH NUMBERED STEP OF D-W6b-44's OWN ORDER IS ITS OWN FUNCTION,
// UNDER 40 LINES (GODS_LAWS.md L-17): step() below is the orchestrator
// that calls them in the fixed order the plan names - never inlines
// the pump/wait/cap logic itself, so a reviewer reads the ORDER at
// step()'s own call site and the MECHANISM of each step at its own,
// single-purpose function.
//
// PRESENT() IS THE ONLY WAY TO PRESENT INSIDE THIS LOOP'S CONTRACT
// (D-W6b-41): it writes m_impl->last_present ONLY when swap_buffers()
// actually returned a value - the NEXT step()'s own compute_frame_
// tick() call reads that field, never re-derives it from anything else
// (frame_tick_state.hpp's own header comment, P4). Calling gltfx_gl_
// context::swap_buffers() directly instead (still public, P10) never
// reaches this loop's own bookkeeping at all - the loop simply never
// finds out, exactly as the header promises.
//
// RUN() CHECKS close_requested() AT THE END OF EACH ITERATION, AFTER
// on_frame/on_render/present() ALREADY RAN FOR THAT TICK, NOT BEFORE
// (P3's own order lists step() -> on_frame -> on_render -> present(),
// with no separate "check close" slot in between - P9 only says run()
// ENDS when the system asks to close, never that it skips the tick
// where that request first arrives): a consumer's own on_frame/on_
// render still gets one full, ordinary tick to react to the close
// request (save state, play an exit animation) before run() actually
// returns - never a request silently swallowed on the tick it arrived.
// step() ITSELF also reads close_requested() (D-W6b-44 step 2), for a
// DIFFERENT reason: skipping the oculto/teto waits (steps 3/4) once
// the consumer is about to close saves exactly the wait budget a
// window that is closing anyway would otherwise spend for nothing -
// the two reads are independent, at two different call sites, for two
// different questions.

namespace glintfx {

namespace {

// D-W6b-44 step 3's own budget - the SAME number egl_context_adapter.
// cpp's own frame-callback budget already uses (D-W6b-6), so a
// consumer reads exactly ONE "hidden cadence" number across this
// loop's own header and that adapter's - never two budgets that could
// silently drift apart from each other.
constexpr std::uint32_t k_hidden_wait_budget_ms = 100;

// Reads the CURRENT frame_rate_cap value this context's own current_
// values table holds (gl_context_impl.hpp's own header comment: the
// per-context mirror gltfx_gl_context::set_option()/option() already
// read and write, through the public facade) - reached DIRECTLY here,
// never through gltfx_gl_context::option() itself, because loop_impl
// only ever stores a gl_context_impl* (D-W6b-55), never a gltfx_gl_
// context& (this loop was never handed one to keep - open() below only
// ever reads the reference it was called with, once). A negative value
// never reaches this table today (gfx_option_validation.hpp's own
// range check on frame_rate_cap's row), clamped to 0 here anyway - the
// same "never trust a stored value blindly, verify locally" discipline
// gl_context_facade.cpp's own open() already applies to a DIFFERENT
// invariant (its own header comment on the unchecked-optional-access
// finding).
[[nodiscard]] std::uint32_t read_frame_rate_cap_hz(const gl_context_impl &context) noexcept {
    for (const gltfx_gfx_option_entry &entry : context.current_values) {
        if (entry.id == gltfx_gfx_option::frame_rate_cap) {
            return entry.value > 0 ? static_cast<std::uint32_t>(entry.value) : 0;
        }
    }
    return 0;
}

// D-W6b-44 step 3: the tick right after a present() skipped a hidden
// window waits with a fixed budget instead of spinning, then pumps
// once more - giving the compositor a chance to deliver whatever woke
// the connection (a frame callback, a `configure` clearing `suspended`
// - both are events ON THE DISPLAY'S OWN FILE DESCRIPTOR,
// D-W6b-46/D-W6b-50's own header comments) before this tick's own
// probe (step() below, present_would_skip()) decides whether the
// window is visible again. Never re-presents here: present() (below)
// is the only place that ever happens.
[[nodiscard]] gltfx_rslt<void>
wait_while_hidden(platform::selected_display_adapter &display) noexcept {
    if (const gltfx_rslt<bool> waited = display.wait_events(k_hidden_wait_budget_ms);
        waited.has_error()) {
        return gltfx_rslt<void>::err(waited.error());
    }
    return display.pump_events();
}

// D-W6b-44 step 4 / D-W6b-49: the hybrid frame-rate-cap wait -
// wait_events(grosso) (grosso = what frame_cap_schedule::plan() itself
// already reports minus the last 1 ms, D-W6b-49's own header comment)
// repeated while more than 1 ms remains, then a tight spin on gltfx_
// now() for the last <= 1 ms - never a sleep for that final sliver
// (the grain a plain `wait_events` alone cannot guarantee on every
// platform, D-W6b-49's own "the perfect Sleep()" citation). plan()
// itself only ADVANCES its own deadline once `now` has actually
// reached it (frame_cap_schedule.hpp's own header comment, "guards the
// DEADLINE, never the instant") - calling it repeatedly with a fresh
// `now` while the deadline has not arrived yet is side-effect-free, so
// both loops below are safe to spin.
[[nodiscard]] gltfx_rslt<void> wait_for_frame_cap(platform::selected_display_adapter &display,
                                                  platform::frame_cap_schedule &schedule,
                                                  std::uint32_t cap_hz) noexcept {
    std::uint32_t wait_ms = schedule.plan(gltfx_now(), cap_hz);
    while (wait_ms > 1) {
        if (const gltfx_rslt<bool> waited = display.wait_events(wait_ms - 1); waited.has_error()) {
            return gltfx_rslt<void>::err(waited.error());
        }
        if (const gltfx_rslt<void> pumped = display.pump_events(); pumped.has_error()) {
            return pumped;
        }
        wait_ms = schedule.plan(gltfx_now(), cap_hz);
    }
    // Last sliver (<= 1 ms): a tight spin on the exact clock, never a
    // sleep (GODS_LAWS.md L-04/D-W6b-49/tests/wait_points.txt, `teto-
    // nosso`: a real, finite budget - `wait_ms` cannot exceed one
    // period entering this loop, frame_cap_schedule_test.cpp's own
    // "sem deriva" case already proves plan()'s own deadline only ever
    // advances, never regresses). Named `spin_now`, a DIFFERENT literal
    // expression from the two `schedule.plan(gltfx_now(), ...)` calls
    // above (deliberately - tests/tools/check_wait_points.py's own
    // manifest matches by exact substring per FILE, and this spin's
    // own role - the actual bounded wait, `teto-nosso` - is NOT the
    // same as the two calls above, which never themselves wait,
    // `nao-espera`: identical text would force one shared classification
    // that could not honestly be both). plan() itself is what tells
    // this loop the deadline was finally reached (return value 0),
    // consuming/advancing its own internal deadline in the SAME call
    // that ends this spin.
    while (wait_ms > 0) {
        const gltfx_time_point spin_now = gltfx_now();
        wait_ms = schedule.plan(spin_now, cap_hz);
    }
    return gltfx_rslt<void>::ok();
}

} // namespace

gltfx_rslt<gltfx_loop> gltfx_loop::open(gltfx_display &display, gltfx_window &window,
                                        gltfx_gl_context &context) noexcept {
    // P10: refuses BY NAME whichever of the three is not open - the
    // same precondition shape window_facade.cpp's own open() already
    // uses for its own single `display` parameter, one layer up.
    if (!display.is_open()) {
        return gltfx_rslt<gltfx_loop>::err(
            gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value("display"));
    }
    if (!window.is_open()) {
        return gltfx_rslt<gltfx_loop>::err(
            gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value("window"));
    }
    if (!context.is_open()) {
        return gltfx_rslt<gltfx_loop>::err(
            gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value("context"));
    }

    gltfx_rslt<loop_impl *> allocated = allocate_loop_impl();
    if (allocated.has_error()) {
        return gltfx_rslt<gltfx_loop>::err(allocated.error());
    }

    // Borrowed pointers only (D-W6b-55/loop_impl.hpp's own header
    // comment) - obtained through the SAME three internal-access
    // passkeys display_facade.cpp/window_facade.cpp/gl_context_facade.
    // cpp already establish for every other cross-facade caller.
    loop_impl *impl = allocated.value();
    impl->display = display_internal_access::get(display);
    impl->window = window_internal_access::get(window);
    impl->context = gl_context_internal_access::get(context);
    impl->previous_now = gltfx_now();
    impl->last_present = gltfx_present_outcome::presented;
    impl->frame_index = 0;

    return gltfx_rslt<gltfx_loop>::ok(gltfx_loop(impl));
}

gltfx_loop::gltfx_loop(gltfx_loop &&other) noexcept : m_impl(other.m_impl) {
    other.m_impl = nullptr;
}

gltfx_loop &gltfx_loop::operator=(gltfx_loop &&other) noexcept {
    if (this != &other) {
        delete m_impl;
        m_impl = other.m_impl;
        other.m_impl = nullptr;
    }
    return *this;
}

// Never closes display/window/context (P10) - only this loop's own
// scheduling state (loop_impl.hpp's own header comment) is this
// handle's to destroy.
gltfx_loop::~gltfx_loop() { delete m_impl; }

bool gltfx_loop::is_open() const noexcept { return m_impl != nullptr; }

gltfx_rslt<gltfx_frame_tick> gltfx_loop::step() noexcept {
    assert(m_impl != nullptr &&
           "gltfx_loop::step() called on a moved-from loop - the object no longer owns an impl");

    platform::selected_display_adapter &display_adapter = m_impl->display->connection.adapter();

    // Step 1 (D-W6b-44): pump is always the FIRST thing a step() does
    // (P3) - the state on_frame reads below already includes whatever
    // the system delivered before this tick.
    if (const gltfx_rslt<void> pumped = display_adapter.pump_events(); pumped.has_error()) {
        return gltfx_rslt<gltfx_frame_tick>::err(pumped.error());
    }

    // Step 2 (D-W6b-44): read close_requested() to skip steps 3/4
    // below when the consumer is about to close anyway - see this
    // file's own top comment, "RUN() CHECKS close_requested()...", for
    // why this is a SEPARATE read from the one run() does after this
    // call returns.
    const bool closing = m_impl->window->adapter.state().close_requested();

    if (!closing) {
        // Step 3: last present() skipped a hidden window - wait with a
        // fixed budget, pump again, before this tick's own probe
        // (below) decides visibility.
        if (m_impl->last_present == gltfx_present_outcome::skipped_hidden) {
            if (const gltfx_rslt<void> waited = wait_while_hidden(display_adapter);
                waited.has_error()) {
                return gltfx_rslt<gltfx_frame_tick>::err(waited.error());
            }
        }

        // Step 4: a frame-rate cap, if the consumer asked for one
        // (P7) - coexists with vsync (whichever is slower wins, by
        // construction: this loop never touches vsync at all).
        const std::uint32_t cap_hz = read_frame_rate_cap_hz(*m_impl->context);
        if (cap_hz > 0) {
            if (const gltfx_rslt<void> capped =
                    wait_for_frame_cap(display_adapter, m_impl->cap_schedule, cap_hz);
                capped.has_error()) {
                return gltfx_rslt<gltfx_frame_tick>::err(capped.error());
            }
        }
    }

    // Steps 5/6: read the clock, build the tick through the pure atom
    // (P2/P4/P5) - present_would_skip() is the "would the NEXT
    // present() skip" sonda D-W6b-46 gives this loop, never a real
    // presentation of its own.
    const gltfx_time_point now = gltfx_now();
    const bool probe_says_visible = !m_impl->context->adapter.present_would_skip();
    const gltfx_frame_tick tick =
        platform::compute_frame_tick(m_impl->previous_now, now, m_impl->frame_index,
                                     m_impl->last_present, probe_says_visible);

    m_impl->previous_now = now;
    m_impl->frame_index = tick.frame_index;

    return gltfx_rslt<gltfx_frame_tick>::ok(tick);
}

gltfx_rslt<gltfx_present_outcome> gltfx_loop::present() noexcept {
    assert(m_impl != nullptr &&
           "gltfx_loop::present() called on a moved-from loop - the object no longer owns an "
           "impl");

    gltfx_rslt<gltfx_present_outcome> presented = m_impl->context->adapter.swap_buffers();
    if (presented.has_value()) {
        // P4: last_present is copied straight through, never
        // re-derived - the NEXT step()'s own compute_frame_tick() call
        // reads exactly this value.
        m_impl->last_present = presented.value();
    }
    return presented;
}

gltfx_rslt<void> gltfx_loop::run(const gltfx_loop_callbacks &callbacks) noexcept {
    assert(m_impl != nullptr &&
           "gltfx_loop::run() called on a moved-from loop - the object no longer owns an impl");

    if (const gltfx_rslt<void> validated = platform::validate_loop_callbacks(callbacks);
        validated.has_error()) {
        return validated;
    }

    for (;;) {
        const gltfx_rslt<gltfx_frame_tick> ticked = step();
        if (ticked.has_error()) {
            return gltfx_rslt<void>::err(ticked.error());
        }
        const gltfx_frame_tick &tick = ticked.value();

        // (on_event delivery is RESERVED here, D-W6b-24/P3 - lands
        // with INPUT-EVENTS, W7; validate_loop_callbacks() above
        // already refused a caller that filled it in.)

        if (!callbacks.on_frame(tick)) {
            return gltfx_rslt<void>::ok();
        }

        if (tick.should_render) {
            callbacks.on_render(tick);
            const gltfx_rslt<gltfx_present_outcome> presented = present();
            if (presented.has_error()) {
                return gltfx_rslt<void>::err(presented.error());
            }
        }

        // P9: ends when the system asks to close - checked AFTER this
        // tick's own on_frame/on_render/present already ran, never
        // before (this file's own top comment, "RUN() CHECKS
        // close_requested()...").
        if (m_impl->window->adapter.state().close_requested()) {
            return gltfx_rslt<void>::ok();
        }
    }
}

// loop_internal_access::get() - the ONLY definition of this symbol in
// the whole library, the exact same reasoning display_internal_access::
// get()/window_internal_access::get()/gl_context_internal_access::
// get() already document for themselves. No GLINTFX_API on this line,
// for the identical reason.
loop_impl *loop_internal_access::get(gltfx_loop &loop) noexcept { return loop.m_impl; }

} // namespace glintfx
