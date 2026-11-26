// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/platform/gl/context.hpp>

#include "platform/wayland/egl_present_wire.hpp"
#include "platform/wayland/frame_callback_sequence.hpp"
#include "platform/wayland/frame_callback_wait.hpp"
#include "platform/wayland/frame_request_present.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// egl_present_wiring_test.cpp - WL-FRAME-ORDER-ARM-TEST-fix1 (D-W8-F11): the BINDING of the ports
// to the wire, which the N1 review of e119be83 found without a unit proof (mutants M05 to M10
// survived). Every system call is a scripted function here; the wire objects are never touched
// (the pointers below are addresses of local markers, never dereferenced by the script). The
// templates' own order is proved in frame_request_present_test and frame_callback_wait_test;
// this file proves that each port passes on what its call answered, calls what it must, and
// that the defaults ARE the real calls.

using glintfx::gltfx_err;
using glintfx::gltfx_err_code;
using glintfx::platform::egl_present_calls;
using glintfx::platform::egl_present_target;
using glintfx::platform::egl_wire_present_ports;
using glintfx::platform::egl_wire_wait_ports;
using glintfx::platform::frame_callback_sequence;
using glintfx::platform::present_after_frame_request;
using glintfx::platform::wait_for_frame_callback;

namespace {

// What the scripted calls answer and what they saw. Each case starts with `s = script{}`.
struct script {
    unsigned int swap_answer = 1U; // EGL_TRUE
    bool frame_obtained = true;
    std::optional<gltfx_err> dead;
    bool poll_answer = true;
    std::int64_t clock_ns = 5'000;
    int swaps = 0;
    int frames = 0;
    int listeners = 0;
    int destroys = 0;
    int dead_checks = 0;
    int reports = 0;
    int polls = 0;
    std::uint32_t last_budget = 0;
    void *swap_display = nullptr;
    void *swap_surface = nullptr;
    wl_surface *frame_surface = nullptr;
    wl_callback *destroyed = nullptr;
    wl_display *dead_display = nullptr;
    wl_display *poll_display = nullptr;
    const wl_callback_listener *listener = nullptr;
    void *listener_data = nullptr;
};
script s; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables) reason: the C calls have no
          // context

std::array<char, 4> markers{};
void *marker(std::size_t i) { return &markers.at(i); }

wl_callback *fresh_callback() { return static_cast<wl_callback *>(marker(0)); }

unsigned int scripted_swap(void *display, void *surface) noexcept {
    ++s.swaps;
    s.swap_display = display;
    s.swap_surface = surface;
    return s.swap_answer;
}
wl_callback *scripted_frame(wl_surface *surface) noexcept {
    ++s.frames;
    s.frame_surface = surface;
    return s.frame_obtained ? fresh_callback() : nullptr;
}
int scripted_add_listener(wl_callback *, const wl_callback_listener *listener,
                          void *data) noexcept {
    ++s.listeners;
    s.listener = listener;
    s.listener_data = data;
    return 0;
}
void scripted_destroy(wl_callback *callback) noexcept {
    ++s.destroys;
    s.destroyed = callback;
}
std::optional<gltfx_err> scripted_dead(wl_display *display) noexcept {
    ++s.dead_checks;
    s.dead_display = display;
    return s.dead;
}
void scripted_report() noexcept { ++s.reports; }
std::int64_t scripted_clock() noexcept { return s.clock_ns; }
bool scripted_poll(wl_display *display, std::uint32_t budget_ms) noexcept {
    ++s.polls;
    s.poll_display = display;
    s.last_budget = budget_ms;
    // A poll takes its whole budget, as a timeout does: a wait that ignores the answer still ends.
    s.clock_ns += static_cast<std::int64_t>(budget_ms) * 1'000'000;
    return s.poll_answer;
}

constexpr egl_present_calls k_scripted{
    .swap_buffers = &scripted_swap,
    .surface_frame = &scripted_frame,
    .callback_add_listener = &scripted_add_listener,
    .callback_destroy = &scripted_destroy,
    .connection_failure_if_dead = &scripted_dead,
    .report_frame_callback_request_failed = &scripted_report,
    .now_ns = &scripted_clock,
};

// The adapter's two slots between presentations, owned by the case.
struct slots {
    wl_callback *pending = nullptr;
    bool reported = false;
};

[[nodiscard]] egl_wire_present_ports wired(slots &kept) {
    return egl_wire_present_ports{
        .target =
            egl_present_target{.egl_display = marker(1),
                               .egl_surface = marker(2),
                               .surface = static_cast<wl_surface *>(marker(3)),
                               .display = static_cast<wl_display *>(marker(1)),
                               .pending_callback = &kept.pending,
                               .request_failure_reported = &kept.reported,
                               .listener = static_cast<const wl_callback_listener *>(marker(2)),
                               .listener_data = marker(3)},
        .calls = k_scripted};
}

} // namespace

// M05 and M06 of the review: request_frame() passes on what wl_surface_frame() answered, and
// listens to the callback it got.
GLINTFX_TEST(egl_wire_request_frame_passes_on_the_callback_and_listens_to_it) {
    s = script{};
    slots kept;
    egl_wire_present_ports ports = wired(kept);
    GLINTFX_CHECK(ports.request_frame());
    GLINTFX_CHECK_EQ(s.frames, 1);
    GLINTFX_CHECK(s.frame_surface == static_cast<wl_surface *>(marker(3)));
    GLINTFX_CHECK_EQ(s.listeners, 1);
    GLINTFX_CHECK(s.listener == static_cast<const wl_callback_listener *>(marker(2)));
    GLINTFX_CHECK(s.listener_data == marker(3));
    GLINTFX_CHECK(kept.pending == fresh_callback());
    s.frame_obtained = false;
    GLINTFX_CHECK(!ports.request_frame());
    GLINTFX_CHECK_EQ(s.listeners, 1);
    GLINTFX_CHECK(kept.pending == nullptr);
}

// The orphan of the previous presentation is destroyed before a new callback is asked for.
GLINTFX_TEST(egl_wire_request_frame_destroys_the_orphan_first) {
    s = script{};
    slots kept;
    kept.pending = static_cast<wl_callback *>(marker(1));
    egl_wire_present_ports ports = wired(kept);
    GLINTFX_CHECK(ports.request_frame());
    GLINTFX_CHECK_EQ(s.destroys, 1);
    GLINTFX_CHECK(s.destroyed == static_cast<wl_callback *>(marker(1)));
    GLINTFX_CHECK(kept.pending == fresh_callback());
}

// M08: swap() is true only for EGL_TRUE, and swaps the adapter's own display and surface.
GLINTFX_TEST(egl_wire_swap_is_true_only_for_egl_true) {
    s = script{};
    slots kept;
    egl_wire_present_ports ports = wired(kept);
    GLINTFX_CHECK(ports.swap());
    GLINTFX_CHECK(s.swap_display == marker(1));
    GLINTFX_CHECK(s.swap_surface == marker(2));
    s.swap_answer = 0U; // EGL_FALSE
    GLINTFX_CHECK(!ports.swap());
    GLINTFX_CHECK_EQ(s.swaps, 2);
}

// M07: dead_after_swap() passes on the connection's verdict, asked of the adapter's display.
GLINTFX_TEST(egl_wire_dead_after_swap_passes_on_the_verdict) {
    s = script{};
    slots kept;
    egl_wire_present_ports ports = wired(kept);
    GLINTFX_CHECK(!ports.dead_after_swap().has_value());
    s.dead = gltfx_err(gltfx_err_code::platform_failure);
    const std::optional<gltfx_err> verdict = ports.dead_after_swap();
    GLINTFX_CHECK(verdict.has_value());
    GLINTFX_CHECK(verdict.has_value() && verdict->code() == gltfx_err_code::platform_failure);
    GLINTFX_CHECK(s.dead_display == static_cast<wl_display *>(marker(1)));
}

// M10 and D-W8-F12: the failure is reported on the edge - once for a run of failures, again
// after a request that succeeded.
GLINTFX_TEST(egl_wire_report_goes_out_once_per_run_of_failures) {
    s = script{};
    slots kept;
    egl_wire_present_ports ports = wired(kept);
    ports.report_frame_request_failed();
    ports.report_frame_request_failed();
    GLINTFX_CHECK_EQ(s.reports, 1);
    GLINTFX_CHECK(ports.request_frame());
    ports.report_frame_request_failed();
    GLINTFX_CHECK_EQ(s.reports, 2);
}

// The clock port reads the injected clock.
GLINTFX_TEST(egl_wire_now_reads_the_clock) {
    s = script{};
    slots kept;
    egl_wire_present_ports ports = wired(kept);
    s.clock_ns = 123'456;
    GLINTFX_CHECK_EQ(ports.now_ns(), std::int64_t{123'456});
}

// M09: poll_for() passes on what the poll answered, with the budget and display it was given.
GLINTFX_TEST(egl_wire_poll_for_passes_on_the_answer) {
    s = script{};
    egl_wire_wait_ports ports{.display = static_cast<wl_display *>(marker(1)),
                              .poll = &scripted_poll,
                              .clock = &scripted_clock};
    GLINTFX_CHECK(ports.poll_for(42U));
    GLINTFX_CHECK_EQ(s.last_budget, 42U);
    GLINTFX_CHECK(s.poll_display == static_cast<wl_display *>(marker(1)));
    s.poll_answer = false;
    GLINTFX_CHECK(!ports.poll_for(7U));
    s.clock_ns = 9;
    GLINTFX_CHECK_EQ(ports.now_ns(), std::int64_t{9});
}

// The wire through the real templates: a swap that answers EGL_FALSE is refused, a missing
// callback is reported and arms nothing, and a dead connection after a successful wait fails it.
GLINTFX_TEST(egl_wire_drives_the_templates) {
    s = script{};
    slots kept;
    frame_callback_sequence sequence;
    egl_wire_present_ports ports = wired(kept);
    s.swap_answer = 0U;
    const auto refused = present_after_frame_request(ports, sequence);
    GLINTFX_CHECK(refused.has_error());
    s.swap_answer = 1U;
    s.frame_obtained = false;
    const auto unpaced = present_after_frame_request(ports, sequence);
    GLINTFX_CHECK(!unpaced.has_error());
    GLINTFX_CHECK(!sequence.has_pending_callback());
    GLINTFX_CHECK_EQ(s.reports, 1);
    sequence.arm_pending(0);
    s.poll_answer = false;
    egl_wire_wait_ports waits{.display = static_cast<wl_display *>(marker(1)),
                              .poll = &scripted_poll,
                              .clock = &scripted_clock};
    GLINTFX_CHECK(!wait_for_frame_callback(waits, sequence, 100U));
}

// The defaults ARE the real calls: the adapter constructs both port objects with defaults.
GLINTFX_TEST(egl_wire_defaults_are_the_real_calls) {
    const egl_present_calls real{};
    GLINTFX_CHECK(real.swap_buffers == &glintfx::platform::real_egl_swap_buffers);
    GLINTFX_CHECK(real.surface_frame == &glintfx::platform::real_surface_frame);
    GLINTFX_CHECK(real.callback_add_listener == &glintfx::platform::real_callback_add_listener);
    GLINTFX_CHECK(real.callback_destroy == &glintfx::platform::real_callback_destroy);
    GLINTFX_CHECK(real.connection_failure_if_dead ==
                  &glintfx::platform::real_connection_failure_if_dead);
    GLINTFX_CHECK(real.report_frame_callback_request_failed ==
                  &glintfx::platform::real_report_frame_callback_request_failed);
    GLINTFX_CHECK(real.now_ns == &glintfx::platform::real_now_ns);
    const egl_wire_wait_ports waits{};
    GLINTFX_CHECK(waits.poll == &glintfx::platform::real_poll_for);
    GLINTFX_CHECK(waits.clock == &glintfx::platform::real_now_ns);
}
