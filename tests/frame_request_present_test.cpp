// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/core/log/event.hpp>
#include <glintfx/core/log/severity.hpp>
#include <glintfx/core/log/sink.hpp>
#include <glintfx/platform/gl/context.hpp>

#include "platform/wayland/frame_callback_sequence.hpp"
#include "platform/wayland/frame_request_present.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// frame_request_present_test.cpp - WL-FRAME-ORDER-ARM-TEST (D-W8-226, D-W8-F01): the cases that
// make the swap fail, which no container fixture can do (llvmpipe always returns EGL_TRUE), and
// the case where libwayland could not allocate the frame callback. The five ports are a script;
// the call log is a fixed array, never a std::vector, because the harness runs these cases on
// every system and a container built inside a noexcept function dies in the Microsoft debug
// runtime. Call letters: r request_frame, f report_frame_request_failed, s swap, d
// dead_after_swap, n now_ns.

using glintfx::gltfx_err;
using glintfx::gltfx_err_code;
using glintfx::gltfx_log_event;
using glintfx::gltfx_log_set_sink;
using glintfx::gltfx_log_severity;
using glintfx::gltfx_log_sink;
using glintfx::gltfx_present_outcome;
using glintfx::platform::frame_callback_sequence;
using glintfx::platform::frame_wait_plan;
using glintfx::platform::k_frame_callback_request_failed_event;
using glintfx::platform::present_after_frame_request;
using glintfx::platform::report_frame_callback_request_failed;

namespace {

constexpr std::uint32_t k_budget_ms = 100;

struct scripted_present_ports {
    bool frame_obtained = true;
    bool swap_succeeds = true;
    std::optional<gltfx_err> dead;
    std::array<char, 8> calls{};
    std::size_t call_count = 0;
    std::size_t report_count = 0;

    void record(char call) noexcept {
        if (call_count < calls.size()) {
            calls[call_count] = call;
        }
        ++call_count;
    }
    bool request_frame() noexcept {
        record('r');
        return frame_obtained;
    }
    void report_frame_request_failed() noexcept {
        record('f');
        ++report_count;
    }
    bool swap() noexcept {
        record('s');
        return swap_succeeds;
    }
    std::optional<gltfx_err> dead_after_swap() noexcept {
        record('d');
        return dead;
    }
    std::int64_t now_ns() noexcept {
        record('n');
        return 7'000;
    }
};

struct received_event {
    std::size_t count = 0;
    gltfx_log_severity severity = gltfx_log_severity::trace;
    bool category_matches = false;
    bool name_matches = false;
    std::size_t field_count = 99;
};

void recording_sink(void *sink_context, const gltfx_log_event &event) noexcept {
    auto *received = static_cast<received_event *>(sink_context);
    ++received->count;
    received->severity = event.severity();
    received->category_matches = event.category() == "platform.wayland";
    received->name_matches = event.name() == k_frame_callback_request_failed_event;
    received->field_count = event.fields().size();
}

} // namespace

GLINTFX_TEST(frame_request_present_requests_the_frame_before_the_swap) {
    scripted_present_ports ports;
    frame_callback_sequence sequence;
    const auto outcome = present_after_frame_request(ports, sequence);
    GLINTFX_CHECK(!outcome.has_error());
    GLINTFX_CHECK(outcome.value() == gltfx_present_outcome::presented);
    GLINTFX_CHECK(ports.call_count == 4);
    GLINTFX_CHECK(ports.calls[0] == 'r');
    GLINTFX_CHECK(ports.calls[1] == 's');
    GLINTFX_CHECK(ports.calls[2] == 'd');
    GLINTFX_CHECK(ports.calls[3] == 'n');
    GLINTFX_CHECK(ports.report_count == 0);
    GLINTFX_CHECK(sequence.has_pending_callback());
    GLINTFX_CHECK(sequence.plan_before_wait(k_budget_ms) == frame_wait_plan::poll_then_decide);
}

// I-2: one transient swap failure must not freeze the window. Armed before the swap (the M3
// mutant of the WL-FRAME-ORDER review), the next present would wait for a callback no commit
// carries; armed after, it presents immediately.
GLINTFX_TEST(frame_request_present_failed_swap_leaves_nothing_to_wait_for) {
    scripted_present_ports ports;
    ports.swap_succeeds = false;
    frame_callback_sequence sequence;
    const auto outcome = present_after_frame_request(ports, sequence);
    GLINTFX_CHECK(outcome.has_error());
    GLINTFX_CHECK(outcome.err().code() == gltfx_err_code::platform_failure);
    GLINTFX_CHECK(ports.report_count == 0);
    GLINTFX_CHECK(!sequence.has_pending_callback());
    GLINTFX_CHECK(sequence.plan_before_wait(k_budget_ms) == frame_wait_plan::present_immediately);
}

GLINTFX_TEST(frame_request_present_recovers_on_the_swap_after_a_failed_one) {
    scripted_present_ports ports;
    ports.swap_succeeds = false;
    frame_callback_sequence sequence;
    static_cast<void>(present_after_frame_request(ports, sequence));
    ports.swap_succeeds = true;
    const auto outcome = present_after_frame_request(ports, sequence);
    GLINTFX_CHECK(!outcome.has_error());
    GLINTFX_CHECK(outcome.value() == gltfx_present_outcome::presented);
    GLINTFX_CHECK(sequence.has_pending_callback());
}

// I-5: a null wl_surface_frame() must degrade, never freeze, and never in silence (D-W8-F01).
// Nothing was requested, so nothing is awaited and the next present goes out at once; the
// failure is reported once, right after the request and before the swap.
GLINTFX_TEST(frame_request_present_without_a_callback_arms_nothing) {
    scripted_present_ports ports;
    ports.frame_obtained = false;
    frame_callback_sequence sequence;
    const auto outcome = present_after_frame_request(ports, sequence);
    GLINTFX_CHECK(!outcome.has_error());
    GLINTFX_CHECK(outcome.value() == gltfx_present_outcome::presented);
    GLINTFX_CHECK(ports.report_count == 1);
    GLINTFX_CHECK(ports.call_count == 4);
    GLINTFX_CHECK(ports.calls[0] == 'r');
    GLINTFX_CHECK(ports.calls[1] == 'f');
    GLINTFX_CHECK(ports.calls[2] == 's');
    GLINTFX_CHECK(ports.calls[3] == 'd');
    GLINTFX_CHECK(!sequence.has_pending_callback());
    GLINTFX_CHECK(sequence.plan_before_wait(k_budget_ms) == frame_wait_plan::present_immediately);
}

// D-W8-F01: the report is made before the swap, so a swap that then fails does not swallow it.
GLINTFX_TEST(frame_request_present_reports_a_missing_callback_even_when_the_swap_fails) {
    scripted_present_ports ports;
    ports.frame_obtained = false;
    ports.swap_succeeds = false;
    frame_callback_sequence sequence;
    const auto outcome = present_after_frame_request(ports, sequence);
    GLINTFX_CHECK(outcome.has_error());
    GLINTFX_CHECK(outcome.err().code() == gltfx_err_code::platform_failure);
    GLINTFX_CHECK(ports.report_count == 1);
    GLINTFX_CHECK(!sequence.has_pending_callback());
}

// EGL-DEAD-DISPLAY-GUARD S2: the connection is asked whatever the swap answered, and a dead one
// wins over both answers.
GLINTFX_TEST(frame_request_present_dead_connection_wins_over_the_swap_answer) {
    scripted_present_ports ports;
    ports.dead = gltfx_err(gltfx_err_code::invalid_argument);
    frame_callback_sequence sequence;
    const auto after_success = present_after_frame_request(ports, sequence);
    GLINTFX_CHECK(after_success.has_error());
    GLINTFX_CHECK(after_success.err().code() == gltfx_err_code::invalid_argument);
    ports.swap_succeeds = false;
    const auto after_failure = present_after_frame_request(ports, sequence);
    GLINTFX_CHECK(after_failure.has_error());
    GLINTFX_CHECK(after_failure.err().code() == gltfx_err_code::invalid_argument);
    GLINTFX_CHECK(!sequence.has_pending_callback());
}

// D-W8-F01: what the adapter's port emits, read back through a registered sink, as a consumer
// would read it.
GLINTFX_TEST(frame_callback_request_failed_reaches_a_registered_sink) {
    gltfx_log_set_sink(gltfx_log_sink{});
    received_event received;
    gltfx_log_set_sink(gltfx_log_sink{&recording_sink, &received, gltfx_log_severity::trace});
    report_frame_callback_request_failed();
    gltfx_log_set_sink(gltfx_log_sink{});
    GLINTFX_CHECK(received.count == 1);
    GLINTFX_CHECK(received.severity == gltfx_log_severity::warn);
    GLINTFX_CHECK(received.category_matches);
    GLINTFX_CHECK(received.name_matches);
    GLINTFX_CHECK(received.field_count == 0);
}
