// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <print>

#include <glintfx/core/time.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// time_test.cpp - CORE-TIME (TODO.md W3, GODS_LAWS.md L-19/L-20/L-26):
// the core layer's public representation of elapsed time. ESCOPO.md
// SS2, decision 3 (27/08/2026, verbatim "Exato, com atalho") ratifies
// the shape: an EXACT integer nanosecond count is the canonical form
// (it never accumulates error), plus a to-seconds conversion sitting
// ALONGSIDE it, never replacing it. That decision fixes the WHAT; the
// concrete names below (gltfx_duration/gltfx_time_point/the three
// gltfx_duration_* functions) are an implementer inference
// (GODS_LAWS.md L-27), not a leader decision, chosen for the same
// reason CORE-ERROR/CORE-COLOR already prefix their public names: an
// unprefixed `duration`/`time_point` would spell EXACTLY
// std::chrono::duration/std::chrono::time_point, a real, likely
// collision for a 2D engine consumer who commonly writes
// `using namespace std::chrono;` for their own timing code
// (docs/api-conventions.md R6). A plain named function
// (gltfx_duration_between) is used instead of operator- for the same
// reason: this whole codebase has zero precedent for a public operator
// overload today, and tests/tools/check_public_name_collision.sh's own
// awk scanner (see that script's header comment) has never been
// exercised against one either - staying inside the pattern every
// other public function in this project already follows is the
// smaller, better-understood surface.
//
// RED, proved by real compilation (GODS_LAWS.md L-20): this file was
// written and built BEFORE include/glintfx/core/time.hpp existed at
// all - the build failed with "glintfx/core/time.hpp: No such file or
// directory", the same shape CORE-COLOR's own first slice documents
// (commit 97eb4c9's message: "Vermelho provado por compilacao real
// (header ausente) antes de o tipo existir").

namespace {

// ENUMERATION, not directed search (GODS_LAWS.md L-27/L-40, "enumere o
// espaco pequeno quando ele for fechado" - the exact practice
// err_code_test.cpp's own header comment names): time.hpp declares
// EXACTLY THREE public free functions - this list is hand-written and
// edited in lockstep with the header, which states the same count in
// its own top comment.
enum class time_operation : std::uint8_t {
    duration_between,
    duration_to_seconds,
    duration_from_seconds
};

constexpr std::array<time_operation, 3> k_all_operations{
    time_operation::duration_between,
    time_operation::duration_to_seconds,
    time_operation::duration_from_seconds,
};

} // namespace

GLINTFX_TEST(every_public_time_operation_is_exercised) {
    // GODS_LAWS.md L-40: a closed enumeration whose printed count
    // never matches its own declared total is exactly the "portal that
    // scans nothing and prints green" this law exists to forbid - so
    // the loop below counts what it actually exercised instead of
    // trusting the array's own size.
    std::size_t exercised = 0;

    for (const time_operation op : k_all_operations) {
        switch (op) {
        case time_operation::duration_between: {
            const glintfx::gltfx_time_point earlier{.ticks = 5};
            const glintfx::gltfx_time_point later{.ticks = 20};
            const glintfx::gltfx_duration elapsed = glintfx::gltfx_duration_between(earlier, later);
            GLINTFX_CHECK_EQ(elapsed.nanoseconds, static_cast<std::int64_t>(15));
            ++exercised;
            break;
        }
        case time_operation::duration_to_seconds: {
            const glintfx::gltfx_duration two_seconds{.nanoseconds = 2'000'000'000};
            const double seconds = glintfx::gltfx_duration_to_seconds(two_seconds);
            GLINTFX_CHECK(std::abs(seconds - 2.0) < 1e-9);
            ++exercised;
            break;
        }
        case time_operation::duration_from_seconds: {
            const glintfx::gltfx_duration built = glintfx::gltfx_duration_from_seconds(1.5);
            GLINTFX_CHECK_EQ(built.nanoseconds, static_cast<std::int64_t>(1'500'000'000));
            ++exercised;
            break;
        }
        }
    }

    GLINTFX_CHECK_EQ(exercised, k_all_operations.size());
    // L-40: the count is printed even when everything passes.
    std::println("every_public_time_operation_is_exercised: {} of {} public operation(s) exercised",
                 exercised, k_all_operations.size());
}

GLINTFX_TEST(time_point_difference_is_never_negative_for_monotonic_readings) {
    // A synthetic monotonic sequence, non-decreasing WITH REPEATS
    // (delta == 0 is a legal pair - two readings can land in the same
    // clock tick) - not a hand-picked pair, GODS_LAWS.md L-27/L-40. The
    // property under test: for ANY (earlier_index <= later_index) pair
    // drawn from a non-decreasing sequence - the only order a
    // monotonic clock's own contract allows - the elapsed duration is
    // never negative.
    constexpr std::array<std::int64_t, 7> k_readings{0, 0, 5, 5, 5, 1'000, 1'000'000'000};

    int pairs_checked = 0;
    for (std::size_t earlier_index = 0; earlier_index < k_readings.size(); ++earlier_index) {
        for (std::size_t later_index = earlier_index; later_index < k_readings.size();
             ++later_index) {
            const glintfx::gltfx_time_point earlier{.ticks = k_readings[earlier_index]};
            const glintfx::gltfx_time_point later{.ticks = k_readings[later_index]};
            const glintfx::gltfx_duration elapsed = glintfx::gltfx_duration_between(earlier, later);
            GLINTFX_CHECK(elapsed.nanoseconds >= 0);
            ++pairs_checked;
        }
    }

    // 7 readings, pairs with later_index >= earlier_index: sum(7..1) = 28.
    GLINTFX_CHECK_EQ(pairs_checked, 28);
    std::println("time_point_difference_is_never_negative_for_monotonic_readings: {} monotonic "
                 "reading pair(s) checked, none negative",
                 pairs_checked);
}

GLINTFX_TEST(duration_seconds_round_trip_does_not_accumulate_error) {
    // Directed round trip (integer -> seconds -> integer), GODS_LAWS.md
    // L-20's own service order: proves the canonical nanosecond count
    // survives to_seconds()/from_seconds() exactly, not just
    // approximately - the property decision 3 exists to guarantee
    // ("nunca acumula erro"). Samples span from zero, through
    // sub-microsecond spans, to a full day, chosen so every one stays
    // well inside a double's ~15-17 significant decimal digits
    // (verified independently in Python before writing this list, not
    // assumed): the largest sample here needs 14 significant digits
    // (86400000000000), comfortably inside that budget.
    constexpr std::array<std::int64_t, 12> k_nanosecond_samples{
        0,
        1,
        7,
        500,
        999,
        123'456'789,
        999'999'999,
        1'000'000'000,
        1'500'000'000,
        3'600'000'000'000,  // one hour
        86'400'000'000'000, // one day
        9'223'372'036,
    };

    int checked = 0;
    for (const std::int64_t sample : k_nanosecond_samples) {
        const glintfx::gltfx_duration original{.nanoseconds = sample};
        const double seconds = glintfx::gltfx_duration_to_seconds(original);
        const glintfx::gltfx_duration round_tripped = glintfx::gltfx_duration_from_seconds(seconds);
        GLINTFX_CHECK_EQ(round_tripped.nanoseconds, original.nanoseconds);
        ++checked;
    }

    GLINTFX_CHECK_EQ(checked, static_cast<int>(k_nanosecond_samples.size()));
    std::println(
        "duration_seconds_round_trip_does_not_accumulate_error: {} sample(s) round-tripped "
        "with zero drift",
        checked);
}
