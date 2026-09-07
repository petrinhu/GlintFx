// SPDX-License-Identifier: AGPL-3.0-or-later
#include <glintfx/core/fixed_step.hpp>

#include <cstdint>
#include <limits>

// core/fixed_step.cpp - implementation of LOOP-RUN's gltfx_fixed_step
// (see fixed_step.hpp's own header comment for the frozen decision,
// the "spiral of death" rule this guards against, and the TOTAL
// contract this function honors). Pure arithmetic on the value types
// declared there; no OS call, no <chrono>, nothing but integers and
// one division (GODS_LAWS.md L-19).

namespace glintfx {

gltfx_fixed_step_result gltfx_fixed_step_accumulate(gltfx_fixed_step &step,
                                                    gltfx_duration elapsed) noexcept {
    // TOTAL, per core/time.hpp's own D8 rule (fixed_step.hpp's own top
    // comment): a step size with no direction to divide by is a
    // construction mistake this pure function reports by returning an
    // all-zero result, never by touching `step.accumulated` or reaching
    // for the division below.
    if (step.dt.nanoseconds <= 0) {
        return gltfx_fixed_step_result{};
    }

    // A negative `elapsed` (a caller feeding a duration this atom did
    // not itself measure) has no meaningful contribution to accumulate -
    // treated as zero, never subtracted, so `step.accumulated` can
    // never be driven negative by this function.
    const std::int64_t elapsed_ns = elapsed.nanoseconds > 0 ? elapsed.nanoseconds : 0;
    const auto dt_ns = static_cast<std::uint64_t>(step.dt.nanoseconds);

    // Unsigned throughout (the same "well-defined for every realistic
    // pair" discipline core/time.cpp's own gltfx_duration_between()
    // already uses one file over) - both operands are non-negative by
    // construction at this point, so this is plain, exact addition,
    // never a wraparound reinterpretation.
    const std::uint64_t accumulated_ns = static_cast<std::uint64_t>(step.accumulated.nanoseconds) +
                                         static_cast<std::uint64_t>(elapsed_ns);

    std::uint64_t whole_steps = accumulated_ns / dt_ns;
    const std::uint64_t remainder_ns = accumulated_ns % dt_ns;

    // Defensive saturation against an absurd (dt, elapsed) pair driving
    // `whole_steps` past what a std::uint32_t can name - not exercised
    // by any case this project's own plan enumerates (fixed_step_test.
    // cpp's closed {abaixo, igual, multiplo, estouro} x {com teto, sem
    // teto} matrix never approaches this magnitude), kept anyway
    // because this function's own header comment promises TOTAL, never
    // a silent modular wraparound on the reported step count.
    constexpr std::uint64_t k_max_reportable_steps = std::numeric_limits<std::uint32_t>::max();
    if (whole_steps > k_max_reportable_steps) {
        whole_steps = k_max_reportable_steps;
    }

    if (step.max_steps != 0 && whole_steps > step.max_steps) {
        // THE SPIRAL OF DEATH, SEGURADA (fixed_step.hpp's own top
        // comment): the debt beyond max_steps is DISCARDED, not carried
        // - `accumulated` resets to zero rather than keeping the excess
        // for a future call to report even more steps against.
        step.accumulated = gltfx_duration{.nanoseconds = 0};
        return gltfx_fixed_step_result{
            .steps = step.max_steps,
            .remainder = gltfx_duration{.nanoseconds = 0},
            .alpha = 0.0,
        };
    }

    step.accumulated = gltfx_duration{.nanoseconds = static_cast<std::int64_t>(remainder_ns)};

    // remainder_ns < dt_ns always (the definition of `%` for a
    // strictly-positive divisor), so this quotient is always in
    // [0, 1) - fixed_step_test.cpp's own "alpha sempre em [0,1)" case
    // is a property of this arithmetic, not something checked here.
    const double alpha = static_cast<double>(remainder_ns) / static_cast<double>(dt_ns);

    return gltfx_fixed_step_result{
        .steps = static_cast<std::uint32_t>(whole_steps),
        .remainder = step.accumulated,
        .alpha = alpha,
    };
}

} // namespace glintfx
