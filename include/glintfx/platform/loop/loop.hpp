// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <functional>
#include <type_traits>

#include <glintfx/core/err.hpp>
#include <glintfx/core/time.hpp>
#include <glintfx/export.hpp>
#include <glintfx/platform/gl/context.hpp>

// platform/loop/loop.hpp - LOOP-RUN (docs/plano-w6b-fatias-6-8.md,
// GODS_LAWS.md L-17/L-19/L-20/L-22/L-35): the main loop every consumer
// of this library organizes its own frame around. gltfx_loop is the
// FOURTH handle this library freezes (after gltfx_display, gltfx_window
// and gltfx_gl_context) - the one that turns "pump the system, measure
// the frame, decide whether to draw, present" into one call a
// consumer's own `while` can wrap, or into three calls (step/on_frame/
// present) a consumer with its own loop shape calls by hand.
//
// THIS FATIA (6a, docs/plano-w6b-fatias-6-8.md sec. 8.1) FREEZES THE
// SURFACE BELOW, BUT DOES NOT IMPLEMENT IT: gltfx_loop::open()/step()/
// present()/run() are declared here so the promises P1..P10 below have
// a real signature to attach to, and so the pure atoms this fatia DOES
// ship (core/fixed_step.hpp, platform/loop/frame_tick_state.hpp,
// platform/loop/frame_cap_schedule.hpp, platform/loop/loop_callbacks_
// validation.hpp) have a public vocabulary to build gltfx_frame_tick
// values against. The actual PIMPL (loop_impl, defined in a later
// fatia's own loop_impl.hpp) and the facade that defines every method
// below land in fatia 6b, once fatia 7's own wait_events()/present_
// would_skip() ports exist for the facade to call - the same "header
// freezes the vocabulary before the concrete handle needs it" sequence
// this library already used once, for gltfx_window (include/glintfx/
// platform/window/window.hpp's own top comment).
//
// ============================================================
// THE EFFECT, IN ONE SENTENCE (for whoever reads this header without
// having read any plan): a consumer writes "prepare, then each frame
// do this and draw that", and the library pumps the system's own
// events, measures how much time really passed (bounded, never
// negative), tells the consumer whether THIS frame should be drawn,
// presents at whatever cadence the display can actually sustain,
// honors a frame-rate cap if one was requested, and sleeps instead of
// spinning the CPU while the window is not visible. A consumer that
// prefers its own `while` calls step() and present() by hand and gets
// every one of those same guarantees.
// ============================================================

namespace glintfx {

class gltfx_display;
class gltfx_window;

// The ceiling gltfx_frame_tick::elapsed below is clamped to - public,
// named, so a consumer reads the ACTUAL number this library enforces
// instead of a literal copied into their own code (docs/plano-w6b-
// fatias-6-8.md D-W6b-48: 250 ms is the value the prior-art survey in
// that plan's own sec. 1 names as the industry reference for "how long
// can a single frame's elapsed time be, before treating it as a
// genuine stall instead of a real frame"). A gltfx_time_point read
// after a machine suspend/resume, or after a debugger pause, produces
// an elapsed span far larger than this - clamping it here is what
// keeps core::gltfx_fixed_step_accumulate() (core/fixed_step.hpp) from
// ever seeing a multi-second `elapsed` and reporting a spiral-sized
// step count on the very next call.
inline constexpr gltfx_duration k_gltfx_max_frame_elapsed{.nanoseconds = 250'000'000};

// What one gltfx_loop::step() (or one iteration of gltfx_loop::run())
// hands the consumer - see this header's own "THE PROMISES" table
// below for what each field means and which test proves it. Value
// type of the platform layer, PLAIN, PUBLIC LAYOUT (the same opacity
// exception include/glintfx/platform/window/window.hpp's own top
// comment already documents for gltfx_window_size/gltfx_window_desc):
// this struct carries no resource and no hidden invariant a caller
// could violate by reading a field - the stable layout IS the
// contract.
struct gltfx_frame_tick {
    // Wall time since the PREVIOUS tick, per the SAME monotonic clock
    // core/time.hpp's own gltfx_now() reads - never negative (a clock
    // that appears to go backward reports zero, the same "no direction,
    // no meaningful span" rule core/time.hpp's own D8 paragraph already
    // applies to a pure conversion), never larger than
    // k_gltfx_max_frame_elapsed above. Zero on the very first tick.
    // Zero-initialized by default - never left indeterminate if a
    // caller default-constructs a gltfx_frame_tick before an atom
    // fills it in (this constructor is never called by the library
    // itself; every real tick this project builds uses full designated
    // initialization).
    gltfx_duration elapsed{};

    // The gltfx_now() reading that closed THIS tick - the same reading
    // `elapsed` above was computed against, and the one the NEXT tick's
    // own `elapsed` will be measured from.
    gltfx_time_point now{};

    // 1 on the first tick, +1 every subsequent tick, with no gap -
    // independent of `should_render` below (a hidden window still
    // advances this counter; only drawing stops). 64 bits: at 1000
    // ticks per second, a 32-bit counter would overflow in 49 days,
    // well inside a long-running server's own realistic uptime.
    std::uint64_t frame_index = 0;

    // Whether THIS tick should call on_render()/present() - see this
    // header's own P4 below for the exact rule (the two implications
    // it proves, and why it is NOT a plain equality with `last_present`
    // below).
    bool should_render = false;

    // What the LAST present() call (this tick's own, if it ran, or a
    // previous one) returned - `presented` before the very first
    // present() this handle ever makes. Copied straight through,
    // never re-derived from anything else (the same discipline
    // include/glintfx/platform/gl/context.hpp's own gltfx_present_
    // outcome comment already documents for this exact field).
    gltfx_present_outcome last_present = gltfx_present_outcome::presented;
};

static_assert(std::is_standard_layout_v<gltfx_frame_tick>,
              "GODS_LAWS.md L-19 item 3: gltfx_frame_tick's layout is the contract itself");
static_assert(std::is_trivially_copyable_v<gltfx_frame_tick>,
              "gltfx_frame_tick is a value type, safe to copy across the ABI boundary");

// Reserved for INPUT-EVENTS (W7, docs/plano-w6b-fatias-6-8.md sec. 10,
// item 6) - only DECLARED here, never defined by this fatia. See
// gltfx_loop_callbacks::on_event below for the one thing this fatia
// DOES decide about it: the field exists in the frozen struct layout
// today, but run() below refuses a caller that has already filled it
// in (GODS_LAWS.md L-35: a delivery guarantee this library has not
// built yet is never promised by accepting the callback silently).
struct gltfx_input_event;

// The three callbacks gltfx_loop::run() below drives - see this
// header's own P3/P9 for the exact order and the exact refusal rules.
// `std::function` in a public struct ties a consumer to the same
// standard library this whole ABI already assumes (the same tradeoff
// include/glintfx/platform/window/window.hpp's own gltfx_window_desc
// already accepts for `std::string_view`), stated here rather than
// left implicit.
struct gltfx_loop_callbacks {
    // Runs once per tick, BEFORE on_render() below - returning `false`
    // ends run() (P9). MUST be set: run() refuses `invalid_argument`
    // (rejected_value() == "on_frame") when this is empty, because a
    // loop with nothing to run each tick is not a loop a consumer
    // meant to call run() for.
    std::function<bool(const gltfx_frame_tick &)> on_frame;

    // Runs once per tick, ONLY when gltfx_frame_tick::should_render is
    // true, immediately before present(). MUST be set: run() refuses
    // `invalid_argument` (rejected_value() == "on_render") when this is
    // empty - a tick with should_render true and no drawing code would
    // present whatever the back buffer already held, undefined content
    // from the consumer's own point of view.
    std::function<void(const gltfx_frame_tick &)> on_render;

    // RESERVED for INPUT-EVENTS (W7) - MUST be empty in this version.
    // run() refuses `invalid_argument` (rejected_value() == "on_event")
    // when this is filled in, rather than accepting it and silently
    // never calling it (GODS_LAWS.md L-35's own delivery guarantee has
    // not been built yet, so this library never pretends to honor one).
    std::function<void(const gltfx_input_event &)> on_event;
};

// loop_impl - the opaque implementation gltfx_loop below PIMPLs over,
// defined ONLY in a later fatia's own src/platform/loop/loop_impl.hpp
// (the exact same "full layout is a private implementation detail"
// shape include/glintfx/platform/window/window.hpp's own window_impl
// forward declaration already documents, one directory over). No
// consumer of this header ever sees this type; gltfx_loop below only
// ever carries a pointer to one.
struct loop_impl;

// loop_internal_access - the SAME passkey idiom every other handle in
// this library already uses (display_internal_access, display.hpp;
// window_internal_access, window.hpp) - a friend struct outside
// gltfx_loop itself, get() declared here but DEFINED only in a later
// fatia's own loop_facade.cpp, so a consumer's own translation unit
// can never synthesize the access itself, only ask the linker for a
// symbol this library never exports.
struct loop_internal_access {
    [[nodiscard]] static loop_impl *get(class gltfx_loop &loop) noexcept;
};

// gltfx_loop - LOOP-RUN's own handle (docs/plano-w6b-fatias-6-8.md
// D-W6b-41). Named-constructor idiom, the same shape gltfx_display::
// open()/gltfx_window::open()/gltfx_gl_context::open() already use:
// open() is a FALLIBLE static factory returning gltfx_rslt<gltfx_loop>,
// never a bare constructor that could fail.
//
// ============================================================
// THE PROMISES (each proven by the test named alongside it; docs/
// plano-w6b-fatias-6-8.md sec. 3.2 is the canonical, longer text this
// summarizes - read that section before changing any of the ten):
// ============================================================
//
//   P1. step() and present() never block indefinitely, with the
//       exceptions this library declares AND COUNTS (never audits from
//       memory - tests/wait_points.txt, a later fatia's own manifest,
//       is what makes that claim provable instead of asserted): with
//       vsync on, on Windows, present() waits at most one display
//       refresh period; on a driver that refuses to honor vsync being
//       turned OFF, present() waits for the compositor's own frame
//       callback with no budget of this library's own, and a hidden
//       window can hold it until it is shown again.
//
//   P2. frame_index starts at 1 and grows by exactly 1 every step(),
//       with no gap, independent of should_render.
//
//   P3. Order inside one step(): pump the system's own events, wait
//       (only when hidden or a frame-rate cap says so), pump again,
//       read the clock, return the tick. Order inside run(): step() ->
//       (on_event, reserved) -> on_frame -> if should_render:
//       on_render -> present().
//
//   P4. should_render is true whenever the last present() actually
//       presented; it is false ONLY when the last present() skipped a
//       hidden window AND the library's own probe still says hidden -
//       the moment the window is shown again, should_render returns to
//       true with NOTHING the consumer has to do. on_frame keeps
//       running every tick regardless; only on_render stops.
//
//   P5. elapsed is read from the monotonic clock, never negative,
//       never larger than k_gltfx_max_frame_elapsed above, zero on the
//       first tick. The real cadence (mean and worst case) is
//       MEASURED and reported, never promised as a number.
//
//   P6. This loop never simulates anything: elapsed is fine for
//       animation and for logic with no physics; physics and game
//       rules go through core/fixed_step.hpp's own gltfx_fixed_step,
//       or the result depends on the machine it happened to run on
//       (this header's own "WHAT THIS FATIA DOES NOT PROMISE" list
//       below is explicit that there is no automatic fixed step here).
//
//   P7. A frame-rate cap (the context's own frame_rate_cap option,
//       include/glintfx/platform/gl/gfx_option.hpp) is honored by THIS
//       loop: with a cap of N, run() never calls present() more than N
//       times per second, measured by DEADLINE (never accumulating
//       drift), and changing the cap live takes effect on the very
//       next tick. Coexists with vsync being on - whichever of the two
//       is slower wins, by construction.
//
//   P8. A hidden window (minimized, on either platform) never draws
//       and never spins the CPU: present() returns skipped_hidden WITH
//       OR WITHOUT vsync, on_render is not called, and the process
//       sleeps between ticks instead of polling in a tight loop.
//
//   P9. run() ends when the system asks to close the window, or when
//       on_frame returns false - ok() in both cases, and whatever
//       error a failing step()/present() produced, unchanged, in every
//       other case. Refuses (by name, invalid_argument): on_frame
//       empty ("on_frame"), on_render empty ("on_render"), on_event
//       filled in ("on_event").
//
//   P10. display, window and context must outlive the loop - open()
//        refuses (by name, invalid_argument) whichever of the three is
//        not open. Presenting through the context directly
//        (gltfx_gl_context::swap_buffers()) is still allowed; the loop
//        simply never finds out.
//
// ============================================================
// WHAT THIS FATIA DOES NOT PROMISE, stated so nobody has to infer it
// from an absence:
// ============================================================
//
//   - A frame rate in a NUMBER - only measured, printed cadence.
//   - Input event delivery (reserved for INPUT-EVENTS, W7).
//   - An automatic fixed step - core/fixed_step.hpp's own
//     gltfx_fixed_step is opt-in, never called by this loop on a
//     consumer's behalf (P6 above, and GODS_LAWS.md L-02).
//   - Occlusion by ANOTHER window, on a compositor that does not
//     throttle frame callbacks for it (docs/plano-w6b-fatias-6-8.md
//     sec. 5's own matrix - minimizing is promised identical on both
//     platforms; being merely covered by another window is not).
//   - More than one thread - v1 is single-thread only, the same
//     restriction gltfx_gl_context::make_current() already documents.
class gltfx_loop {
  public:
    // Opens a loop over an already-open display, window and context -
    // this class's own "P10" above names the three preconditions this
    // refuses by name. `window` must have been opened FROM `display`,
    // and `context` must have been opened FROM `window` - the same
    // chain gltfx_window::open()/gltfx_gl_context::open() already
    // require of their own single parent, one level up.
    [[nodiscard]] GLINTFX_API static gltfx_rslt<gltfx_loop>
    open(gltfx_display &display, gltfx_window &window, gltfx_gl_context &context) noexcept;

    // Move-only - a live loop is a resource (it holds pointers into the
    // three handles above, and its own scheduling state), the same
    // reasoning every other handle in this library already gives for a
    // live resource.
    gltfx_loop(const gltfx_loop &) = delete;
    gltfx_loop &operator=(const gltfx_loop &) = delete;

    GLINTFX_API gltfx_loop(gltfx_loop &&other) noexcept;
    GLINTFX_API gltfx_loop &operator=(gltfx_loop &&other) noexcept;

    // Closes on scope exit - RAII, the same contract every other
    // handle in this library already gives. Never closes display,
    // window or context themselves (P10 above: the loop never owns
    // them).
    GLINTFX_API ~gltfx_loop();

    [[nodiscard]] GLINTFX_API bool is_open() const noexcept;

    // One tick: pumps the system, waits when appropriate, and returns
    // the tick that describes it - see this class's own P2/P3/P4/P5
    // above. Never calls on_frame/on_render/present() itself - those
    // are run()'s own job, below; a consumer calling step() directly
    // reads gltfx_frame_tick::should_render itself and decides whether
    // to draw and call present() below.
    [[nodiscard]] GLINTFX_API gltfx_rslt<gltfx_frame_tick> step() noexcept;

    // Presents the current frame through this loop - the ONLY way to
    // present INSIDE this loop's own contract (P1 above): unlike
    // calling gltfx_gl_context::swap_buffers() directly, this call's
    // own result feeds the NEXT step()'s gltfx_frame_tick::last_present
    // and should_render (P4 above). Calling the context directly
    // instead is still allowed (P10) - this loop simply never learns
    // the outcome.
    [[nodiscard]] GLINTFX_API gltfx_rslt<gltfx_present_outcome> present() noexcept;

    // Sugar over step()/on_frame/on_render/present() - see this class's
    // own P3/P9 above for the exact order and the exact refusal rules.
    [[nodiscard]] GLINTFX_API gltfx_rslt<void> run(const gltfx_loop_callbacks &callbacks) noexcept;

  private:
    explicit gltfx_loop(loop_impl *impl) noexcept : m_impl(impl) {}

    // loop_internal_access::get() is the ONLY thing outside this class
    // ever granted access to m_impl through the passkey - see that
    // struct's own header comment, above, for why.
    friend struct loop_internal_access;

    loop_impl *m_impl = nullptr;
};

} // namespace glintfx
