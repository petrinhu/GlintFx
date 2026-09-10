// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/core/time.hpp>
#include <glintfx/platform/gl/context.hpp>

#include "platform/window/window_state.hpp"

// tests/fake/fake_loop_ports.hpp - LOOP-RUN (cobertura), S2 (/var/tmp/
// glintfx-plan/loop-fix.md sec. S2.3): the test-only doubles that
// satisfy loop_engine.hpp's own loop_display_port/loop_context_port/
// loop_clock_port concepts (platform/loop/loop_ports.hpp), plus the
// ONE shared registry (loop_call_log) every one of them - and the
// on_frame/on_render callbacks tests/loop_engine_test.cpp itself
// declares - writes into, IN THE ORDER things happened.
//
// ONE FILE, DELIBERATELY (GODS_LAWS.md L-17): the ORDER between ports
// and callbacks is what tests/loop_engine_test.cpp's own T2/T5/T8/T11
// prove, and order only exists in a SINGLE registry - splitting the
// three doubles across three files with a log shared between them
// would be exactly the fragmentation L-17 forbids.
//
// NAMED WITHOUT "_adapter" ON PURPOSE: tests/tools/check_port_privacy.
// sh's own (b) rule only scans `*_adapter.hpp` files against its
// closed list - these are motor-internal doubles, not adapters of
// anything, and calling them "_adapter" would put them in that scan
// for no reason (S2-F9).
//
// TWO EXISTING DOUBLES ARE DELIBERATELY *NOT* REUSED HERE, AND NEITHER
// IS EXTENDED (S2.3's own table):
//   - fake_display_adapter (fake_display_adapter.hpp) is the NEGATIVE
//     control of display_backend_port (display_backend_port_concept_
//     test.cpp) - it is MISSING pump_events() on purpose, and giving it
//     one here would break that proof.
//   - fake_gl_context_adapter (fake_gl_context_adapter.hpp) cannot arm
//     an error or a skipped_hidden outcome from swap_buffers(), and
//     does not know about the frame-rate cap at all (it does not live
//     on the adapter, platform/loop/loop_context_view.hpp's own header
//     comment) - reusing it would either lie about what the real
//     adapter offers, or mix two unrelated files' own single subject
//     (L-17).
//
// WHAT IS *NOT* HERE, ON PURPOSE: platform::window_state itself is the
// window port - there is no fake_loop_window in this file. Its
// close_requested() is a real, mutable, one-way latch, the SAME object
// the real Wayland/Win32 adapter would flip - tests/loop_engine_test.
// cpp's own T10 exists to exercise exactly that trinco, and a double
// with an "unlatchable" close_requested() could hide the very defect
// T10 exists to catch.

namespace glintfx::test {

enum class loop_event : std::uint8_t { pump, wait, probe, swap, clock, on_frame, on_render };

struct loop_call_record {
    loop_event event;
    std::uint32_t budget_ms = 0;
    std::uint64_t frame_index = 0;
    const void *context = nullptr;
};

// The shared registry. `on_frame_returns` is read by the on_frame
// callback tests/loop_engine_test.cpp itself declares (S2.3's own
// table: "on_frame devolve log->on_frame_returns, padrao true") -
// living here, not as a free-standing test global, keeps it reset
// automatically every time a case constructs its own fresh log.
class loop_call_log {
  public:
    bool on_frame_returns = true;

    void push(loop_event event, std::uint32_t budget_ms = 0, std::uint64_t frame_index = 0,
              const void *context = nullptr) {
        m_records.push_back(loop_call_record{event, budget_ms, frame_index, context});
    }

    [[nodiscard]] int count(loop_event event) const noexcept {
        int total = 0;
        for (const loop_call_record &record : m_records) {
            if (record.event == event) {
                ++total;
            }
        }
        return total;
    }

    [[nodiscard]] std::vector<loop_event> sequence() const {
        std::vector<loop_event> events;
        events.reserve(m_records.size());
        for (const loop_call_record &record : m_records) {
            events.push_back(record.event);
        }
        return events;
    }

    [[nodiscard]] std::vector<std::uint32_t> budgets(loop_event event) const {
        std::vector<std::uint32_t> result;
        for (const loop_call_record &record : m_records) {
            if (record.event == event) {
                result.push_back(record.budget_ms);
            }
        }
        return result;
    }

    [[nodiscard]] const std::vector<loop_call_record> &records() const noexcept {
        return m_records;
    }

  private:
    std::vector<loop_call_record> m_records;
};

// loop_clock_port double: a STEP COUNTER, deliberately never the wall
// clock (loop_engine.hpp's own header comment, D-LF-4, has the three
// reasons in full). now() registers `clock` on the shared log, returns
// the CURRENT reading, THEN advances by `step_ns` (the FIRST read
// returns `origin` unchanged).
class fake_loop_clock {
  public:
    explicit fake_loop_clock(loop_call_log &log, glintfx::gltfx_time_point origin = {},
                             std::int64_t step_ns = 1'000'000) noexcept
        : m_log(log), m_now(origin), m_step_ns(step_ns) {}

    [[nodiscard]] glintfx::gltfx_time_point now() noexcept {
        m_log.push(loop_event::clock);
        const glintfx::gltfx_time_point reading = m_now;
        m_now.ticks += m_step_ns;
        return reading;
    }

    // Simulates time passing OUTSIDE a now() read - fake_loop_display's
    // own wait_events(), below, calls this (when wired via set_clock_
    // to_advance()) so that "a wait of N ms makes N ms pass" (S2.3's
    // own table), the rede de seguranca that keeps T11/T12's own
    // budgets legible against the very next clock reading.
    void advance(std::int64_t ns) noexcept { m_now.ticks += ns; }

  private:
    loop_call_log &m_log;
    glintfx::gltfx_time_point m_now;
    std::int64_t m_step_ns;
};

// loop_display_port double.
class fake_loop_display {
  public:
    explicit fake_loop_display(loop_call_log &log) noexcept : m_log(log) {}

    // T8/T10 (via loop_run's own safety net, S2.3's own table): arms
    // `window.request_close()` to fire on the Nth pump_events() call
    // that is NOT itself the "pump again" half of loop_engine.hpp's
    // own wait_while_hidden() - a call immediately preceded by a `wait`
    // log entry is that recovery pump, and never advances or trips this
    // counter (it is still logged as `pump`, same as any other call):
    // without this distinction, a case that also exercises a hidden-
    // window wait (T4/T5) would see the request fire ONE TICK EARLIER
    // than intended, because that single tick makes TWO raw pump_
    // events() calls, not one. 0 (the default) never fires.
    void close_on_pump(int n, glintfx::platform::window_state &window) noexcept {
        m_close_on_pump = n;
        m_window_to_close = &window;
    }

    // T7: fail_on_pump(n, code) arms the Nth COUNTED (same rule as
    // close_on_pump above) pump_events() call to return `code` instead
    // of ok().
    void fail_on_pump(int n, glintfx::gltfx_err_code code) noexcept {
        m_fail_on_pump = n;
        m_fail_code = code;
    }

    [[nodiscard]] glintfx::gltfx_rslt<void> pump_events() noexcept {
        const bool is_recovery_pump =
            !m_log.records().empty() && m_log.records().back().event == loop_event::wait;
        m_log.push(loop_event::pump);
        if (!is_recovery_pump) {
            ++m_counted_pump_calls;
        }
        if (!is_recovery_pump && m_fail_on_pump == m_counted_pump_calls) {
            return glintfx::gltfx_rslt<void>::err(
                glintfx::gltfx_err(m_fail_code).with_rejected_value("pump"));
        }
        if (!is_recovery_pump && m_close_on_pump == m_counted_pump_calls &&
            m_window_to_close != nullptr) {
            m_window_to_close->request_close();
        }
        return glintfx::gltfx_rslt<void>::ok();
    }

    // T11/T12: an armed clock makes wait_events(ms) advance that SAME
    // fake_loop_clock by `ms` (converted to nanoseconds) - see fake_
    // loop_clock::advance()'s own header comment above.
    void set_clock_to_advance(fake_loop_clock *clock) noexcept { m_clock_to_advance = clock; }

    void arm_wait_events_return(bool ready) noexcept { m_wait_events_returns = ready; }

    [[nodiscard]] glintfx::gltfx_rslt<bool> wait_events(std::uint32_t budget_ms) noexcept {
        m_log.push(loop_event::wait, budget_ms);
        if (m_clock_to_advance != nullptr) {
            m_clock_to_advance->advance(static_cast<std::int64_t>(budget_ms) * 1'000'000);
        }
        return glintfx::gltfx_rslt<bool>::ok(m_wait_events_returns);
    }

  private:
    loop_call_log &m_log;
    int m_counted_pump_calls = 0;
    int m_close_on_pump = 0;
    glintfx::platform::window_state *m_window_to_close = nullptr;
    int m_fail_on_pump = 0;
    glintfx::gltfx_err_code m_fail_code = glintfx::gltfx_err_code::unknown;
    fake_loop_clock *m_clock_to_advance = nullptr;
    bool m_wait_events_returns = true;
};

// loop_context_port double.
class fake_loop_context {
  public:
    explicit fake_loop_context(loop_call_log &log) noexcept : m_log(log) {}

    // T4/T5/T6: arms the SEQUENCE swap_buffers() returns, one call per
    // element - the sequence's LAST element repeats once exhausted (a
    // case that calls swap_buffers() more times than it armed keeps
    // getting the last-armed outcome, never runs off the end). An empty
    // sequence (the default) always returns ok(presented) - S2.4's own
    // "arranjo padrao".
    void
    arm_swap_sequence(std::vector<glintfx::gltfx_rslt<glintfx::gltfx_present_outcome>> outcomes) {
        m_swap_outcomes = std::move(outcomes);
    }

    [[nodiscard]] glintfx::gltfx_rslt<glintfx::gltfx_present_outcome> swap_buffers() noexcept {
        m_log.push(loop_event::swap);
        if (m_swap_outcomes.empty()) {
            return glintfx::gltfx_rslt<glintfx::gltfx_present_outcome>::ok(
                glintfx::gltfx_present_outcome::presented);
        }
        const std::size_t index =
            m_swap_calls < m_swap_outcomes.size() ? m_swap_calls : m_swap_outcomes.size() - 1;
        ++m_swap_calls;
        return m_swap_outcomes[index];
    }

    void set_present_would_skip(bool value) noexcept { m_present_would_skip = value; }

    [[nodiscard]] bool present_would_skip() const noexcept {
        m_log.push(loop_event::probe);
        return m_present_would_skip;
    }

    // S2-F2/S2-F7: the frame-rate cap does not live on the real
    // adapter either - it is read live, every step, exactly as
    // loop_context_view.hpp's own frame_rate_cap_hz() reads gl_context_
    // impl::current_values. Mutable between test steps ON PURPOSE
    // (T12's own cell (c) turns it off mid-scenario, live).
    void set_frame_rate_cap_hz(std::uint32_t hz) noexcept { m_frame_rate_cap_hz = hz; }

    [[nodiscard]] std::uint32_t frame_rate_cap_hz() const noexcept { return m_frame_rate_cap_hz; }

  private:
    loop_call_log &m_log;
    std::vector<glintfx::gltfx_rslt<glintfx::gltfx_present_outcome>> m_swap_outcomes;
    std::size_t m_swap_calls = 0;
    bool m_present_would_skip = false;
    std::uint32_t m_frame_rate_cap_hz = 0;
};

} // namespace glintfx::test
