// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <type_traits>

#include <glintfx/core/time.hpp>
#include <glintfx/export.hpp>

// core/fixed_step.hpp - LOOP-RUN fatia 6a (docs/plano-w6b-fatias-6-8.md
// D-W6b-47, GODS_LAWS.md L-02/L-19/L-20/L-26): the ONE optional atom a
// consumer reaches for when it wants a passo fixo (physics, game
// rules) instead of the loop's own raw, variable gltfx_frame_tick::
// elapsed (platform/loop/loop.hpp).
//
// WHY THIS EXISTS AT ALL, INSTEAD OF THE LOOP JUST IMPOSING A FIXED
// STEP (docs/plano-w6b-fatias-6-8.md sec. 1/4, prior art read to learn
// from, GODS_LAWS.md L-29, nothing copied): GODS_LAWS.md L-02 forbids
// application-level RULES inside this library, and "which dt the
// physics uses" is exactly that - a game rule, not a framework
// concern. A loop that imposed a fixed step on every consumer would
// force even a consumer that only animates a UI (no physics at all) to
// pay for an accumulator and an interpolation fraction it never
// wanted. The classic reference (Glenn Fiedler, "Fix Your Timestep",
// 2004, still the mainstream answer in 2025 surveys) accumulates
// elapsed time and consumes it in fixed-size chunks, with a remainder
// carried to the next call - this header carries EXACTLY that
// arithmetic as a pure, optional value type, never wired into
// gltfx_loop itself.
//
// THE SPIRAL OF DEATH, SEGURADA (docs/plano-w6b-fatias-6-8.md sec. 1):
// a step that costs more wall time than `dt` makes a naive accumulator
// grow without bound, and the next call produces even more steps,
// which cost even more time - the classic runaway. `max_steps` caps
// how many steps a single gltfx_fixed_step_accumulate() call ever
// reports; when the true count would exceed it, the excess debt is
// DISCARDED (accumulated resets to zero, remainder is zero) rather
// than carried forward - the simulation falls behind (visibly, once)
// instead of the process locking up trying to catch up. `max_steps ==
// 0` means "no cap beyond whatever the caller already limited `elapsed`
// to" - gltfx_loop itself already limits its own gltfx_frame_tick::
// elapsed to k_gltfx_max_frame_elapsed (platform/loop/loop.hpp), so a
// consumer feeding THAT value through a `dt` of a few milliseconds
// still gets a bounded, small step count without setting this field at
// all; a consumer that feeds its own, unbounded duration needs the cap
// this field provides, because nothing about gltfx_fixed_step itself
// knows where that duration came from.
//
// TOTAL, per core/time.hpp's own D8 rule (the same precedent that
// rule's own paragraph names for "every future pure math/conversion
// function this project writes"): `dt <= 0` has no meaningful step
// size to divide by - gltfx_fixed_step_accumulate() below returns an
// all-zero result and leaves `step.accumulated` UNTOUCHED, never
// undefined behavior, never a fallible gltfx_rslt<T> (an invalid `dt`
// is a construction mistake the CALLER made, diagnosable by reading it
// back, not a runtime failure this pure function reports).
namespace glintfx {

// The accumulator itself - a consumer owns exactly one of these per
// independent fixed-rate subsystem (physics, a deterministic
// simulation, and so on each get their own, the same way a consumer
// owns one gltfx_loop per window). Value type of the core layer
// (GODS_LAWS.md L-19's opacity clause is scoped to "handle e
// subsistema com estado" - this carries state, but no RESOURCE and no
// hidden invariant a caller could violate by touching a field
// directly, the same class core/time.hpp's own two value types and
// glintfx::version already are).
struct gltfx_fixed_step {
    // The fixed step size. `<= 0` turns gltfx_fixed_step_accumulate()
    // below into a total no-op (see this header's own top comment).
    gltfx_duration dt;

    // Caps how many steps ONE gltfx_fixed_step_accumulate() call ever
    // reports - `0` means no cap beyond whatever `elapsed` the caller
    // already limited (this header's own top comment explains why a
    // second, independent cap here is still worth having).
    std::uint32_t max_steps = 0;

    // The carried remainder between calls - starts at zero, and is the
    // ONLY field gltfx_fixed_step_accumulate() ever writes.
    gltfx_duration accumulated{.nanoseconds = 0};
};

static_assert(std::is_standard_layout_v<gltfx_fixed_step>,
              "GODS_LAWS.md L-19 item 3: gltfx_fixed_step's layout is the contract itself");
static_assert(std::is_trivially_copyable_v<gltfx_fixed_step>,
              "gltfx_fixed_step is a value type, safe to copy across the ABI boundary");

// What one gltfx_fixed_step_accumulate() call answers.
struct gltfx_fixed_step_result {
    // How many `dt`-sized steps the caller should run right now.
    std::uint32_t steps = 0;

    // What is left over after `steps` whole steps - the EXACT
    // remainder (`0 <= remainder < dt` whenever `dt > 0`, `0` when
    // `steps == max_steps` and debt was discarded - see this header's
    // own "spiral of death" paragraph above), never re-derived from
    // `alpha` below (a double cannot represent every nanosecond count
    // exactly; this field is the one a caller re-feeds into the next
    // gltfx_fixed_step_accumulate() call, and it never does).
    gltfx_duration remainder{.nanoseconds = 0};

    // The same remainder, as a ready-made interpolation fraction
    // (`remainder.nanoseconds / dt.nanoseconds`, always in `[0, 1)`
    // when `dt > 0`) - decision 3's own "atalho" (core/time.hpp's top
    // comment) applied to this atom: precision (`remainder`) for
    // whoever needs it, convenience (`alpha`) for whoever does not.
    double alpha = 0.0;
};

// The one function this header exists to ship - see this header's own
// top comment for the full contract (what it guards against, and why
// it never imposes a step on gltfx_loop itself).
[[nodiscard]] GLINTFX_API gltfx_fixed_step_result
gltfx_fixed_step_accumulate(gltfx_fixed_step &step, gltfx_duration elapsed) noexcept;

} // namespace glintfx
