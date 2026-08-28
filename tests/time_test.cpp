// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
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

GLINTFX_TEST(duration_between_extremes_of_type_are_well_defined_in_both_orders) {
    // HOSTILE (adversarial review, 28/08/2026, REPRODUCED against this
    // slice's own first commit): the ORIGINAL gltfx_duration_between()
    // computed `later.ticks - earlier.ticks` as plain signed
    // std::int64_t subtraction - well-defined for realistic pairs, but
    // UNDEFINED BEHAVIOR for a CORRECTLY ORDERED pair whose true span
    // exceeds what int64_t can hold. Reproduced live, under UBSan,
    // before this fix (earlier=INT64_MIN, later=INT64_MAX, the only
    // order this function's own header alleges to cover):
    //   runtime error: signed integer overflow: 9223372036854775807 -
    //   -9223372036854775808 cannot be represented in type 'long int'
    // Nothing in gltfx_time_point's signature stops a caller from
    // constructing exactly this pair - GODS_LAWS.md L-23's sanitizer
    // gate does not currently fail the build on this trap either (a
    // separate, already-open finding), so there is NO safety net
    // catching this class of bug today; the fix below has to be
    // correct on its own, not "probably fine because CI would catch
    // it".
    //
    // THE FIX (CTO decision, this file's own commit): compute the
    // subtraction in std::uint64_t - well-defined BY THE STANDARD for
    // EVERY pair of std::int64_t inputs, including the swapped order
    // below - then convert the result back to std::int64_t, ALSO
    // well-defined by the standard since C++20 mandates two's-
    // complement (an exact bit-pattern reinterpretation, not
    // implementation-defined guesswork pre-C++20). This closes the UB
    // without reopening the frozen signature: still two
    // gltfx_time_point in, one gltfx_duration out, still noexcept -
    // the fix lives entirely inside the function body.
    //
    // ENUMERATION (GODS_LAWS.md L-40), extremes of the type in BOTH
    // orders: every expected value below was computed INDEPENDENTLY,
    // in Python, via the same two's-complement modular arithmetic the
    // fix uses - never derived from this file's own C++ formula, so
    // this is not tautological with the implementation. The first two
    // rows are the exact pair the trap above was reproduced against,
    // in each order; the note next to each row is the honest reading
    // of the result, not a claim that it is a meaningful elapsed span
    // (that claim - "never negative" - is scoped to a REALISTIC same-
    // clock precondition, proved separately by
    // time_point_difference_is_never_negative_for_monotonic_readings
    // above, which never approaches this function's own int64_t
    // domain limit).
    struct case_t {
        std::int64_t earlier_ticks;
        std::int64_t later_ticks;
        std::int64_t expected_nanoseconds;
    };
    const std::int64_t type_min = std::numeric_limits<std::int64_t>::min();
    const std::int64_t type_max = std::numeric_limits<std::int64_t>::max();
    const std::array<case_t, 7> k_extreme_cases{{
        {type_min, type_max, -1}, // the exact pair UBSan trapped on
        {type_max, type_min, 1},  // swapped order of the same pair
        {type_min, type_min, 0},  // same reading twice, zero span
        {type_max, type_max, 0},  // same reading twice, zero span
        {0, type_max, type_max},  // largest span that DOES fit in int64_t
        {type_min, 0, type_min},  // largest negative-ticks span that fits
        {-1, 1, 2},               // ordinary small pair straddling zero
    }};

    int checked = 0;
    for (const case_t &c : k_extreme_cases) {
        const glintfx::gltfx_time_point earlier{.ticks = c.earlier_ticks};
        const glintfx::gltfx_time_point later{.ticks = c.later_ticks};
        const glintfx::gltfx_duration elapsed = glintfx::gltfx_duration_between(earlier, later);
        GLINTFX_CHECK_EQ(elapsed.nanoseconds, c.expected_nanoseconds);
        ++checked;
    }

    GLINTFX_CHECK_EQ(checked, static_cast<int>(k_extreme_cases.size()));
    std::println("duration_between_extremes_of_type_are_well_defined_in_both_orders: {} extreme "
                 "pair(s) checked, all well-defined",
                 checked);
}

GLINTFX_TEST(duration_from_seconds_is_a_total_saturating_conversion) {
    // HOSTILE (adversarial review, 28/08/2026, REPRODUCED against this
    // slice's own first commit): the ORIGINAL gltfx_duration_from_seconds()
    // called std::llround(seconds * 1'000'000'000.0) UNCONDITIONALLY.
    // For NaN, +-infinity, or any magnitude whose scaled product
    // cannot fit in std::int64_t, glibc's std::llround - an
    // UNINSTRUMENTED system math-library call, GODS_LAWS.md L-23's
    // sanitizer gate does not see inside it - returned the SAME
    // sentinel bit pattern for every one of these cases. Reproduced
    // live, before this fix: NaN, +infinity, -infinity, 1e300 and
    // -1e300 ALL produced -9223372036854775808, silently, with no
    // diagnostic, REGARDLESS of which direction the true answer should
    // saturate toward - "always the same value, always apparent
    // success" is the exact defect, not merely an imprecise number.
    //
    // THE FIX (CTO decision): gltfx_duration_from_seconds() is now a
    // TOTAL, SATURATING conversion - see this header's own "precedent"
    // comment for the general rule this establishes for every pure
    // math/conversion function in the project from here on. Fails ANY
    // comparison against the representable range (NaN, the only value
    // for which every comparison is false) -> zero duration. A finite
    // value out of range, or +-infinity (both "have a direction") ->
    // saturates to the closest representable gltfx_duration IN THAT
    // DIRECTION. The signature is UNCHANGED - still returns
    // gltfx_duration by value, never a fallible envelope: diagnosing
    // invalid input is the ingestion boundary's job (a file/text
    // reader), never a pure conversion's.
    //
    // THE BOUNDARY PAIRS BELOW ARE DELIBERATE, not redundant with a
    // single edge-exact case (GODS_LAWS.md L-27's own lesson: "testar
    // so na fronteira exata nao basta - alargar o limite mantem a
    // borda dentro do intervalo alargado"): an off-by-one in the range
    // comparison (`>` where `>=` was meant, or vice versa) would pass
    // a boundary-only test silently. Pairing "one second inside the
    // boundary, exact expected value" with "one second outside,
    // saturates" is what actually exercises the DIRECTION of the
    // comparison, for both the max and the min side.
    struct case_t {
        double seconds;
        std::int64_t expected_nanoseconds;
    };
    const std::int64_t max_ns = std::numeric_limits<std::int64_t>::max();
    const std::int64_t min_ns = std::numeric_limits<std::int64_t>::min();
    const std::array<case_t, 9> k_hostile_cases{{
        {std::numeric_limits<double>::quiet_NaN(), 0},      // NaN
        {std::numeric_limits<double>::infinity(), max_ns},  // +infinity
        {-std::numeric_limits<double>::infinity(), min_ns}, // -infinity
        {1e300, max_ns},                                    // huge finite positive
        {-1e300, min_ns},                                   // huge finite negative
        {9223372036.0, 9223372036000000000LL},              // one second inside the max boundary
        {9223372037.0, max_ns},                             // one second outside the max boundary
        {-9223372036.0, -9223372036000000000LL},            // one second inside the min boundary
        {-9223372037.0, min_ns},                            // one second outside the min boundary
    }};

    int checked = 0;
    for (const case_t &c : k_hostile_cases) {
        const glintfx::gltfx_duration result = glintfx::gltfx_duration_from_seconds(c.seconds);
        GLINTFX_CHECK_EQ(result.nanoseconds, c.expected_nanoseconds);
        ++checked;
    }

    GLINTFX_CHECK_EQ(checked, static_cast<int>(k_hostile_cases.size()));
    std::println(
        "duration_from_seconds_is_a_total_saturating_conversion: {} hostile input(s) checked, "
        "all saturated/zeroed deterministically",
        checked);
}
