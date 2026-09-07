// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <cstdint>
#include <print>

#include <glintfx/core/fixed_step.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// fixed_step_test.cpp - LOOP-RUN fatia 6a (docs/plano-w6b-fatias-6-8.md
// D-W6b-47, GODS_LAWS.md L-19/L-20/L-40): eight cases proving core/
// fixed_step.hpp's own gltfx_fixed_step_accumulate() - see that
// header's own top comment for the "spiral of death" rule cases 4/5
// exist to prove, and core/time.hpp's own D8 rule for the TOTAL
// contract case 6 proves.

using glintfx::gltfx_duration;
using glintfx::gltfx_fixed_step;
using glintfx::gltfx_fixed_step_accumulate;
using glintfx::gltfx_fixed_step_result;

namespace {

constexpr gltfx_duration k_dt{.nanoseconds = 10};

} // namespace

GLINTFX_TEST(elapsed_below_dt_reports_zero_steps_and_carries_the_remainder) {
    gltfx_fixed_step step{.dt = k_dt};
    const gltfx_duration elapsed{.nanoseconds = 3};

    const gltfx_fixed_step_result result = gltfx_fixed_step_accumulate(step, elapsed);

    GLINTFX_CHECK_EQ(result.steps, static_cast<std::uint32_t>(0));
    GLINTFX_CHECK_EQ(result.remainder.nanoseconds, static_cast<std::int64_t>(3));
    GLINTFX_CHECK(result.alpha > 0.29 && result.alpha < 0.31); // 3 / 10
    GLINTFX_CHECK_EQ(step.accumulated.nanoseconds, static_cast<std::int64_t>(3));
}

GLINTFX_TEST(elapsed_equal_to_two_periods_reports_two_steps_and_zero_remainder) {
    gltfx_fixed_step step{.dt = k_dt};
    const gltfx_duration elapsed{.nanoseconds = 20};

    const gltfx_fixed_step_result result = gltfx_fixed_step_accumulate(step, elapsed);

    GLINTFX_CHECK_EQ(result.steps, static_cast<std::uint32_t>(2));
    GLINTFX_CHECK_EQ(result.remainder.nanoseconds, static_cast<std::int64_t>(0));
    GLINTFX_CHECK(result.alpha == 0.0);
}

GLINTFX_TEST(the_remainder_accumulates_across_calls) {
    // No cap: four calls of 3 ns each against a 10 ns step - 3, 6, 9,
    // then the fourth call's own 3 ns pushes the total to 12, which is
    // exactly one step with 2 ns left over. Nothing here is asserted
    // per-call except the FINAL one - the point is that the state
    // genuinely carries forward, not that any one call is special.
    gltfx_fixed_step step{.dt = k_dt};
    const gltfx_duration three_ns{.nanoseconds = 3};

    gltfx_fixed_step_result result{};
    for (int i = 0; i < 4; ++i) {
        result = gltfx_fixed_step_accumulate(step, three_ns);
    }

    GLINTFX_CHECK_EQ(result.steps, static_cast<std::uint32_t>(1));
    GLINTFX_CHECK_EQ(result.remainder.nanoseconds, static_cast<std::int64_t>(2));
    GLINTFX_CHECK_EQ(step.accumulated.nanoseconds, static_cast<std::int64_t>(2));
}

GLINTFX_TEST(the_spiral_of_death_is_capped_and_never_grows_the_accumulator) {
    // core/fixed_step.hpp's own top comment: max_steps discards the
    // excess debt rather than carrying it forward - ten calls of
    // elapsed == 4*dt each, capped at 2 steps, must report EXACTLY 2
    // steps every single time, and `accumulated` must never grow past
    // zero between calls (a naive accumulator that kept the excess
    // would instead report a GROWING step count call after call).
    gltfx_fixed_step step{.dt = k_dt, .max_steps = 2};
    const gltfx_duration four_periods{.nanoseconds = 40};

    int calls_checked = 0;
    for (int i = 0; i < 10; ++i) {
        const gltfx_fixed_step_result result = gltfx_fixed_step_accumulate(step, four_periods);
        GLINTFX_CHECK_EQ(result.steps, static_cast<std::uint32_t>(2));
        GLINTFX_CHECK_EQ(result.remainder.nanoseconds, static_cast<std::int64_t>(0));
        GLINTFX_CHECK_EQ(step.accumulated.nanoseconds, static_cast<std::int64_t>(0));
        ++calls_checked;
    }

    GLINTFX_CHECK_EQ(calls_checked, 10);
    std::println("the_spiral_of_death_is_capped_and_never_grows_the_accumulator: {} call(s) "
                 "checked, steps pinned at 2, accumulated pinned at 0",
                 calls_checked);
}

GLINTFX_TEST(max_steps_zero_means_no_cap_beyond_what_the_caller_already_limited) {
    gltfx_fixed_step step{.dt = k_dt, .max_steps = 0};
    const gltfx_duration four_periods{.nanoseconds = 40};

    const gltfx_fixed_step_result result = gltfx_fixed_step_accumulate(step, four_periods);

    GLINTFX_CHECK_EQ(result.steps, static_cast<std::uint32_t>(4));
    GLINTFX_CHECK_EQ(result.remainder.nanoseconds, static_cast<std::int64_t>(0));
}

GLINTFX_TEST(dt_not_positive_is_a_total_no_op_that_never_touches_state) {
    // core/time.hpp's own D8 rule, applied here (this header's own top
    // comment): a step size with no direction to divide by returns an
    // all-zero result and leaves `accumulated` untouched, never
    // undefined behavior, never a fallible envelope.
    gltfx_fixed_step zero_dt_step{.dt = gltfx_duration{.nanoseconds = 0}};
    zero_dt_step.accumulated = gltfx_duration{.nanoseconds = 7}; // poisoned, to prove it survives
    const gltfx_fixed_step_result zero_result =
        gltfx_fixed_step_accumulate(zero_dt_step, gltfx_duration{.nanoseconds = 100});
    GLINTFX_CHECK_EQ(zero_result.steps, static_cast<std::uint32_t>(0));
    GLINTFX_CHECK_EQ(zero_result.remainder.nanoseconds, static_cast<std::int64_t>(0));
    GLINTFX_CHECK(zero_result.alpha == 0.0);
    GLINTFX_CHECK_EQ(zero_dt_step.accumulated.nanoseconds, static_cast<std::int64_t>(7));

    gltfx_fixed_step negative_dt_step{.dt = gltfx_duration{.nanoseconds = -5}};
    negative_dt_step.accumulated = gltfx_duration{.nanoseconds = 11};
    const gltfx_fixed_step_result negative_result =
        gltfx_fixed_step_accumulate(negative_dt_step, gltfx_duration{.nanoseconds = 100});
    GLINTFX_CHECK_EQ(negative_result.steps, static_cast<std::uint32_t>(0));
    GLINTFX_CHECK_EQ(negative_dt_step.accumulated.nanoseconds, static_cast<std::int64_t>(11));
}

GLINTFX_TEST(alpha_is_always_in_the_half_open_interval_zero_one) {
    gltfx_fixed_step step{.dt = k_dt};

    int checked = 0;
    for (std::int64_t elapsed_ns = 0; elapsed_ns < 100; ++elapsed_ns) {
        const gltfx_fixed_step_result result =
            gltfx_fixed_step_accumulate(step, gltfx_duration{.nanoseconds = elapsed_ns});
        GLINTFX_CHECK(result.alpha >= 0.0);
        GLINTFX_CHECK(result.alpha < 1.0);
        ++checked;
    }

    GLINTFX_CHECK_EQ(checked, 100);
    std::println("alpha_is_always_in_the_half_open_interval_zero_one: {} value(s) swept, "
                 "alpha never left [0, 1)",
                 checked);
}

namespace {

// The closed matrix this fatia's own plan names (docs/plano-w6b-
// fatias-6-8.md sec. 8.1: "abaixo, igual, multiplo, estouro x com
// teto, sem teto") - eight cells, each built fresh (a fresh gltfx_
// fixed_step per cell, never one shared and mutated across cells, so a
// cell's own assertion is never polluted by a previous one's leftover
// accumulated debt). GODS_LAWS.md L-21: identifiers stay in English -
// below/equal/multiple/overflow x capped/uncapped are that plan's own
// four-by-two matrix, renamed.
enum class elapsed_shape : std::uint8_t { below, equal, multiple, overflow };
enum class cap_shape : std::uint8_t { capped, uncapped };

constexpr std::array<elapsed_shape, 4> k_elapsed_shapes{
    elapsed_shape::below,
    elapsed_shape::equal,
    elapsed_shape::multiple,
    elapsed_shape::overflow,
};
constexpr std::array<cap_shape, 2> k_cap_shapes{cap_shape::capped, cap_shape::uncapped};

} // namespace

GLINTFX_TEST(the_elapsed_by_cap_matrix_is_enumerated_in_full) {
    // GODS_LAWS.md L-40: the count is printed even when every cell
    // passes - a matrix that silently shrank to fewer than eight cells
    // would still look green without this.
    std::size_t cells_checked = 0;

    for (const elapsed_shape shape : k_elapsed_shapes) {
        for (const cap_shape cap : k_cap_shapes) {
            const std::uint32_t max_steps = (cap == cap_shape::capped) ? 2 : 0;
            gltfx_fixed_step step{.dt = k_dt, .max_steps = max_steps};

            std::int64_t elapsed_ns = 0;
            switch (shape) {
            case elapsed_shape::below:
                elapsed_ns = 5; // < 1 * dt
                break;
            case elapsed_shape::equal:
                elapsed_ns = 10; // == 1 * dt
                break;
            case elapsed_shape::multiple:
                elapsed_ns = 20; // == 2 * dt, still under any cap tested
                break;
            case elapsed_shape::overflow:
                elapsed_ns = 40; // == 4 * dt, over the capped case's own cap of 2
                break;
            }

            const gltfx_fixed_step_result result =
                gltfx_fixed_step_accumulate(step, gltfx_duration{.nanoseconds = elapsed_ns});

            const bool would_overflow_the_cap =
                cap == cap_shape::capped && shape == elapsed_shape::overflow;
            if (would_overflow_the_cap) {
                GLINTFX_CHECK_EQ(result.steps, max_steps);
                GLINTFX_CHECK_EQ(result.remainder.nanoseconds, static_cast<std::int64_t>(0));
            } else {
                GLINTFX_CHECK_EQ(result.steps,
                                 static_cast<std::uint32_t>(elapsed_ns / k_dt.nanoseconds));
                GLINTFX_CHECK_EQ(result.remainder.nanoseconds, elapsed_ns % k_dt.nanoseconds);
            }
            ++cells_checked;
        }
    }

    GLINTFX_CHECK_EQ(cells_checked, k_elapsed_shapes.size() * k_cap_shapes.size());
    std::println("the_elapsed_by_cap_matrix_is_enumerated_in_full: {} of {} cell(s) checked",
                 cells_checked, k_elapsed_shapes.size() * k_cap_shapes.size());
}
