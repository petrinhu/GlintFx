// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <cstdint>

#include "platform/wayland/frame_callback_sequence.hpp"
#include "platform/wayland/frame_callback_wait.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// frame_callback_wait_test.cpp - WL-FRAME-ORDER-ARM-TEST (D-W8-227): the wait for the frame
// callback ends when its one budget ends (the ceiling of D-W8-213), and polls again with what is
// left when a batch of events was not ours. The clock and the poll are a script. Before this file,
// a wait with no ceiling only died by the 400 s timeout of loop_hidden_test (mutant M4 of the
// WL-FRAME-ORDER review); here it dies by an assertion.

using glintfx::platform::frame_callback_sequence;
using glintfx::platform::wait_for_frame_callback;

namespace {

constexpr std::uint32_t k_budget_ms = 100;
constexpr std::int64_t k_ns_per_ms = 1'000'000;
// The script reads the clock this many times past the deadline before it gives up on the code
// under test: a correct wait reads it once there and returns.
constexpr int k_reads_past_deadline_allowed = 2;

struct scripted_wait_ports {
    frame_callback_sequence *sequence = nullptr;
    std::int64_t clock_ns = 0;
    std::int64_t deadline_ns = static_cast<std::int64_t>(k_budget_ms) * k_ns_per_ms;
    // Per poll, in order: how long it took, and whether it delivered our callback. A poll past the
    // end of the script is a timeout: it takes its whole budget and delivers nothing.
    std::array<std::int64_t, 4> elapsed_ms{};
    std::array<bool, 4> delivers{};
    std::size_t scripted_polls = 0;
    bool connection_alive = true;
    std::array<std::uint32_t, 8> budgets{};
    std::size_t poll_count = 0;
    int reads_past_deadline = 0;
    bool spun_past_deadline = false;

    std::int64_t now_ns() noexcept {
        if (clock_ns >= deadline_ns && ++reads_past_deadline > k_reads_past_deadline_allowed) {
            // Stands in for the wait never giving up: record it and end the loop from outside.
            spun_past_deadline = true;
            sequence->mark_frame_done();
        }
        return clock_ns;
    }
    bool poll_for(std::uint32_t budget_ms) noexcept {
        if (poll_count < budgets.size()) {
            budgets[poll_count] = budget_ms;
        }
        const std::size_t index = poll_count++;
        if (index < scripted_polls) {
            clock_ns += elapsed_ms[index] * k_ns_per_ms;
            if (delivers[index]) {
                sequence->mark_frame_done();
            }
            return connection_alive;
        }
        clock_ns += static_cast<std::int64_t>(budget_ms) * k_ns_per_ms;
        return connection_alive;
    }
};

} // namespace

GLINTFX_TEST(frame_callback_wait_does_not_poll_with_nothing_pending) {
    frame_callback_sequence sequence;
    scripted_wait_ports ports;
    ports.sequence = &sequence;
    GLINTFX_CHECK(wait_for_frame_callback(ports, sequence, k_budget_ms));
    GLINTFX_CHECK(ports.poll_count == 0);
}

// The ceiling (D-W8-213), the M4 mutant of the WL-FRAME-ORDER review: nothing ever arrives, and the
// wait returns once the budget is spent, with one poll of the whole budget.
GLINTFX_TEST(frame_callback_wait_returns_when_the_budget_ends) {
    frame_callback_sequence sequence;
    sequence.arm_pending(0);
    scripted_wait_ports ports;
    ports.sequence = &sequence;
    GLINTFX_CHECK(wait_for_frame_callback(ports, sequence, k_budget_ms));
    GLINTFX_CHECK(!ports.spun_past_deadline);
    GLINTFX_CHECK(sequence.has_pending_callback());
    GLINTFX_CHECK(ports.poll_count == 1);
    GLINTFX_CHECK(ports.budgets[0] == k_budget_ms);
}

// The M2 mutant of the WL-FRAME-ORDER review: two batches that are not ours, then our callback,
// each poll given only what is left of the one budget.
GLINTFX_TEST(frame_callback_wait_polls_again_with_what_is_left) {
    frame_callback_sequence sequence;
    sequence.arm_pending(0);
    scripted_wait_ports ports;
    ports.sequence = &sequence;
    ports.scripted_polls = 3;
    ports.elapsed_ms = {10, 10, 5, 0};
    ports.delivers = {false, false, true, false};
    GLINTFX_CHECK(wait_for_frame_callback(ports, sequence, k_budget_ms));
    GLINTFX_CHECK(!sequence.has_pending_callback());
    GLINTFX_CHECK(ports.poll_count == 3);
    GLINTFX_CHECK(ports.budgets[0] == 100);
    GLINTFX_CHECK(ports.budgets[1] == 90);
    GLINTFX_CHECK(ports.budgets[2] == 80);
}

GLINTFX_TEST(frame_callback_wait_reports_a_dead_connection) {
    frame_callback_sequence sequence;
    sequence.arm_pending(0);
    scripted_wait_ports ports;
    ports.sequence = &sequence;
    ports.connection_alive = false;
    GLINTFX_CHECK(!wait_for_frame_callback(ports, sequence, k_budget_ms));
    GLINTFX_CHECK(ports.poll_count == 1);
}
