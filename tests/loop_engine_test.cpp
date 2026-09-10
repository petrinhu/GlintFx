// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstddef>
#include <cstdint>
#include <print>
#include <vector>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/platform/gl/context.hpp>
#include <glintfx/platform/loop/loop.hpp>

#include "fake/fake_loop_ports.hpp"
#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/loop/loop_engine.hpp"
#include "platform/window/window_state.hpp"

// loop_engine_test.cpp - LOOP-RUN (cobertura), S2 (/var/tmp/glintfx-
// plan/loop-fix.md sec. S2.4, GODS_LAWS.md L-20/L-40): the twelve cases
// that give platform::loop_step()/loop_present()/loop_run() (loop_
// engine.hpp) their FIRST callers in this repository - the achado that
// opened this sub-fatia was exactly their absence (TODO.md, LOOP-RUN,
// reprovacao de 10/09/2026). Every case runs on all five systems, with
// NO operating system anywhere in sight (S2-F11): the doubles are
// tests/fake/fake_loop_ports.hpp's own fake_loop_display/fake_loop_
// context/fake_loop_clock, plus the REAL platform::window_state
// (S2-F3 - the window port needs no double at all).
//
// T10 IS EXPECTED RED IN THIS COMMIT, ON PURPOSE (LOOP-CLOSE-LATCH-
// SPIN): it is the machine-verified first sighting of the defect S3
// (the very next commit) fixes - see that case's own comment for the
// exact mechanism and why S3 makes it pass WITHOUT this file changing.

using glintfx::gltfx_loop_callbacks;
using glintfx::gltfx_present_outcome;
using glintfx::gltfx_rslt;
using glintfx::test::fake_loop_clock;
using glintfx::test::fake_loop_context;
using glintfx::test::fake_loop_display;
using glintfx::test::loop_call_log;
using glintfx::test::loop_event;

namespace {

// The default-constructed context + clock in every test's own log,
// wired together the "arranjo padrao" way (S2.4's own top paragraph):
// no cap, present_would_skip == false, swap -> presented, clock origin
// 0 stepping 1 ms per read, and the display's own wait_events()
// advances that SAME clock (so "a wait of N ms makes N ms pass" holds
// by default - a case that needs otherwise says so explicitly, T10).
struct engine_fixture {
    engine_fixture() noexcept : display(log), context(log), clock(log) {
        display.set_clock_to_advance(&clock);
    }

    loop_call_log log;
    glintfx::platform::window_state window;
    fake_loop_display display;
    fake_loop_context context;
    fake_loop_clock clock;
    glintfx::platform::loop_book book;

    using ports_t =
        glintfx::platform::loop_ports<fake_loop_display, glintfx::platform::window_state,
                                      fake_loop_context, fake_loop_clock>;

    [[nodiscard]] ports_t ports() noexcept {
        return ports_t{.display = display, .window = window, .context = context, .clock = clock};
    }
};

// The two callbacks every "context == a real pointer" case shares -
// convert `context` straight into the log the test built, write the
// call, in order, onto it (S2.3's own table).
bool log_on_frame(void *context, const glintfx::gltfx_frame_tick &tick) noexcept {
    auto *log = static_cast<loop_call_log *>(context);
    log->push(loop_event::on_frame, 0, tick.frame_index, context);
    return log->on_frame_returns;
}

void log_on_render(void *context, const glintfx::gltfx_frame_tick &tick) noexcept {
    auto *log = static_cast<loop_call_log *>(context);
    log->push(loop_event::on_render, 0, tick.frame_index, context);
}

// T9(b) ONLY: `context` must be nullptr in every record for that cell,
// so the log has to be reached a DIFFERENT way - a plain static
// pointer, set by the case right before running, unset right after
// (S2.3's own table names this as the one exception).
loop_call_log *g_static_log = nullptr;

bool log_on_frame_via_static(void *context, const glintfx::gltfx_frame_tick &tick) noexcept {
    g_static_log->push(loop_event::on_frame, 0, tick.frame_index, context);
    return g_static_log->on_frame_returns;
}

void log_on_render_via_static(void *context, const glintfx::gltfx_frame_tick &tick) noexcept {
    g_static_log->push(loop_event::on_render, 0, tick.frame_index, context);
}

void noop_destroy_context(void * /*context*/) noexcept {}

} // namespace

GLINTFX_TEST(refusal_by_name_touches_no_port) {
    int cells = 0;
    constexpr int total_cells = 2;

    {
        engine_fixture fx;
        gltfx_loop_callbacks callbacks{};
        callbacks.context = &fx.log;
        callbacks.on_render = &log_on_render;
        // on_frame LEFT EMPTY on purpose - the very first thing
        // validate_loop_callbacks() checks (loop_callbacks_validation.
        // hpp's own header comment on the fixed order).
        const gltfx_rslt<void> result = glintfx::platform::loop_run(fx.ports(), fx.book, callbacks);
        GLINTFX_CHECK(result.has_error());
        GLINTFX_CHECK(result.err().rejected_value() == "on_frame");
        GLINTFX_CHECK_EQ(fx.log.count(loop_event::pump), 0);
        GLINTFX_CHECK_EQ(fx.log.count(loop_event::clock), 0);
        GLINTFX_CHECK_EQ(fx.log.count(loop_event::probe), 0);
        ++cells;
    }
    {
        engine_fixture fx;
        gltfx_loop_callbacks callbacks{};
        callbacks.context = &fx.log;
        callbacks.on_frame = &log_on_frame;
        callbacks.on_render = &log_on_render;
        callbacks.destroy_context = &noop_destroy_context; // S1a: refused by name until S1b
        const gltfx_rslt<void> result = glintfx::platform::loop_run(fx.ports(), fx.book, callbacks);
        GLINTFX_CHECK(result.has_error());
        GLINTFX_CHECK(result.err().rejected_value() == "destroy_context");
        GLINTFX_CHECK_EQ(fx.log.count(loop_event::pump), 0);
        ++cells;
    }
    std::println("refusal_by_name_touches_no_port: {} of {} cell(s)", cells, total_cells);
}

GLINTFX_TEST(pump_is_the_first_thing_a_tick_does) {
    engine_fixture fx;
    fx.log.on_frame_returns = false; // ends after exactly one tick

    gltfx_loop_callbacks callbacks{};
    callbacks.context = &fx.log;
    callbacks.on_frame = &log_on_frame;
    callbacks.on_render = &log_on_render;

    const gltfx_rslt<void> result = glintfx::platform::loop_run(fx.ports(), fx.book, callbacks);
    GLINTFX_CHECK(result.has_value());

    const std::vector<loop_event> expected = {loop_event::pump, loop_event::clock,
                                              loop_event::probe, loop_event::on_frame};
    GLINTFX_CHECK(fx.log.sequence() == expected);
}

GLINTFX_TEST(on_frame_false_ends_ok_after_exactly_one_tick) {
    engine_fixture fx;
    fx.log.on_frame_returns = false;
    fx.display.close_on_pump(3, fx.window); // rede de seguranca, nunca dispara

    gltfx_loop_callbacks callbacks{};
    callbacks.context = &fx.log;
    callbacks.on_frame = &log_on_frame;
    callbacks.on_render = &log_on_render;

    const gltfx_rslt<void> result = glintfx::platform::loop_run(fx.ports(), fx.book, callbacks);
    GLINTFX_CHECK(result.has_value());
    GLINTFX_CHECK_EQ(fx.log.count(loop_event::on_frame), 1);
    GLINTFX_CHECK_EQ(fx.log.count(loop_event::on_render), 0);
    GLINTFX_CHECK_EQ(fx.log.count(loop_event::swap), 0);

    bool found = false;
    for (const glintfx::test::loop_call_record &record : fx.log.records()) {
        if (record.event == loop_event::on_frame) {
            GLINTFX_CHECK_EQ(record.frame_index, static_cast<std::uint64_t>(1));
            found = true;
        }
    }
    GLINTFX_CHECK(found);
}

GLINTFX_TEST(hidden_tick_runs_on_frame_skips_on_render_and_index_still_grows) {
    engine_fixture fx;
    fx.context.set_present_would_skip(true);
    fx.context.arm_swap_sequence(
        {gltfx_rslt<gltfx_present_outcome>::ok(gltfx_present_outcome::skipped_hidden)});
    fx.display.close_on_pump(3, fx.window); // rede de seguranca: 3 tiques observados

    gltfx_loop_callbacks callbacks{};
    callbacks.context = &fx.log;
    callbacks.on_frame = &log_on_frame;
    callbacks.on_render = &log_on_render;

    const gltfx_rslt<void> result = glintfx::platform::loop_run(fx.ports(), fx.book, callbacks);
    GLINTFX_CHECK(result.has_value());

    std::vector<std::uint64_t> on_frame_indices;
    for (const glintfx::test::loop_call_record &record : fx.log.records()) {
        if (record.event == loop_event::on_frame) {
            on_frame_indices.push_back(record.frame_index);
        }
    }
    const std::vector<std::uint64_t> expected_indices = {1, 2, 3};
    GLINTFX_CHECK(on_frame_indices == expected_indices);
    GLINTFX_CHECK_EQ(fx.log.count(loop_event::on_render), 1);
    GLINTFX_CHECK_EQ(fx.log.count(loop_event::swap), 1);
    std::println("hidden_tick_runs_on_frame_skips_on_render_and_index_still_grows: 2 of 2 cell(s)");
}

GLINTFX_TEST(render_tick_is_on_render_then_present_and_the_outcome_feeds_the_next_tick) {
    engine_fixture fx;
    fx.context.set_present_would_skip(true); // a sonda diz oculto o tempo todo
    fx.context.arm_swap_sequence({
        gltfx_rslt<gltfx_present_outcome>::ok(gltfx_present_outcome::skipped_hidden),
        gltfx_rslt<gltfx_present_outcome>::ok(gltfx_present_outcome::presented),
    });
    fx.display.close_on_pump(2, fx.window);

    gltfx_loop_callbacks callbacks{};
    callbacks.context = &fx.log;
    callbacks.on_frame = &log_on_frame;
    callbacks.on_render = &log_on_render;

    const gltfx_rslt<void> result = glintfx::platform::loop_run(fx.ports(), fx.book, callbacks);
    GLINTFX_CHECK(result.has_value());

    // Tique 1: on_frame -> on_render -> swap, nessa ordem.
    const std::vector<loop_event> full_sequence = fx.log.sequence();
    const std::vector<loop_event> first_six(full_sequence.begin(), full_sequence.begin() + 6);
    const std::vector<loop_event> expected_first_six = {loop_event::pump,      loop_event::clock,
                                                        loop_event::probe,     loop_event::on_frame,
                                                        loop_event::on_render, loop_event::swap};
    GLINTFX_CHECK(first_six == expected_first_six);

    // Tique 2: entregue (on_frame roda), mas NAO desenha - a formula de
    // P4 com o probe forcado oculto e last_present==skipped_hidden (do
    // present de tique 1) so' devolve should_render==false; provado
    // indiretamente (nenhum record novo de on_render/swap), porque
    // loop_call_record nao carrega o tick inteiro (S2.3's own table).
    GLINTFX_CHECK_EQ(fx.log.count(loop_event::on_frame), 2);
    GLINTFX_CHECK_EQ(fx.log.count(loop_event::on_render), 1);
    GLINTFX_CHECK_EQ(fx.log.count(loop_event::swap), 1);
    std::println(
        "render_tick_is_on_render_then_present_and_the_outcome_feeds_the_next_tick: 2 of 2 "
        "cell(s)");
}

GLINTFX_TEST(present_error_returns_unchanged_and_stops_the_loop) {
    engine_fixture fx;
    const glintfx::gltfx_err_code code = glintfx::gltfx_err_code::platform_failure;
    fx.context.arm_swap_sequence({gltfx_rslt<gltfx_present_outcome>::err(
        glintfx::gltfx_err(code).with_rejected_value("swap"))});
    fx.display.close_on_pump(3, fx.window); // rede de seguranca, nunca dispara

    gltfx_loop_callbacks callbacks{};
    callbacks.context = &fx.log;
    callbacks.on_frame = &log_on_frame;
    callbacks.on_render = &log_on_render;

    const gltfx_rslt<void> result = glintfx::platform::loop_run(fx.ports(), fx.book, callbacks);
    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK(result.err().code() == code);
    GLINTFX_CHECK(result.err().rejected_value() == "swap");
    GLINTFX_CHECK_EQ(fx.log.count(loop_event::on_frame), 1);
    GLINTFX_CHECK_EQ(fx.log.count(loop_event::pump), 1);
}

GLINTFX_TEST(pump_error_returns_unchanged_before_on_frame) {
    engine_fixture fx;
    const glintfx::gltfx_err_code code = glintfx::gltfx_err_code::io_failure;
    fx.display.fail_on_pump(2, code);
    fx.display.close_on_pump(5, fx.window); // rede de seguranca, nunca dispara

    gltfx_loop_callbacks callbacks{};
    callbacks.context = &fx.log;
    callbacks.on_frame = &log_on_frame;
    callbacks.on_render = &log_on_render;

    const gltfx_rslt<void> result = glintfx::platform::loop_run(fx.ports(), fx.book, callbacks);
    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK(result.err().code() == code);
    GLINTFX_CHECK(result.err().rejected_value() == "pump");
    GLINTFX_CHECK_EQ(fx.log.count(loop_event::on_frame), 1);
    GLINTFX_CHECK_EQ(fx.log.count(loop_event::clock), 1);
}

GLINTFX_TEST(close_request_ends_ok_after_the_full_tick_it_arrived_in) {
    engine_fixture fx;
    fx.display.close_on_pump(2, fx.window);

    gltfx_loop_callbacks callbacks{};
    callbacks.context = &fx.log;
    callbacks.on_frame = &log_on_frame;
    callbacks.on_render = &log_on_render;

    const gltfx_rslt<void> result = glintfx::platform::loop_run(fx.ports(), fx.book, callbacks);
    GLINTFX_CHECK(result.has_value());
    GLINTFX_CHECK_EQ(fx.log.count(loop_event::on_frame), 2);
    GLINTFX_CHECK_EQ(fx.log.count(loop_event::on_render), 2);
    GLINTFX_CHECK_EQ(fx.log.count(loop_event::swap), 2);
    GLINTFX_CHECK(fx.window.close_requested());
}

GLINTFX_TEST(context_pointer_is_byte_identical_in_every_callback) {
    int cells = 0;
    constexpr int total_cells = 2;

    {
        engine_fixture fx;
        fx.display.close_on_pump(3, fx.window);

        gltfx_loop_callbacks callbacks{};
        callbacks.context = &fx.log;
        callbacks.on_frame = &log_on_frame;
        callbacks.on_render = &log_on_render;

        const gltfx_rslt<void> result = glintfx::platform::loop_run(fx.ports(), fx.book, callbacks);
        GLINTFX_CHECK(result.has_value());
        const std::vector<glintfx::test::loop_call_record> &records = fx.log.records();
        int checked = 0;
        for (const glintfx::test::loop_call_record &record : records) {
            if (record.event == loop_event::on_frame || record.event == loop_event::on_render) {
                GLINTFX_CHECK(record.context == static_cast<const void *>(&fx.log));
                ++checked;
            }
        }
        GLINTFX_CHECK_EQ(checked, 6);
        ++cells;
    }
    {
        engine_fixture fx;
        fx.display.close_on_pump(3, fx.window);
        g_static_log = &fx.log;

        gltfx_loop_callbacks callbacks{};
        callbacks.context = nullptr;
        callbacks.on_frame = &log_on_frame_via_static;
        callbacks.on_render = &log_on_render_via_static;

        const gltfx_rslt<void> result = glintfx::platform::loop_run(fx.ports(), fx.book, callbacks);
        g_static_log = nullptr;
        GLINTFX_CHECK(result.has_value());
        const std::vector<glintfx::test::loop_call_record> &records = fx.log.records();
        int checked = 0;
        for (const glintfx::test::loop_call_record &record : records) {
            if (record.event == loop_event::on_frame || record.event == loop_event::on_render) {
                GLINTFX_CHECK(record.context == nullptr);
                ++checked;
            }
        }
        GLINTFX_CHECK_EQ(checked, 6);
        ++cells;
    }
    std::println("context_pointer_is_byte_identical_in_every_callback: {} of {} cell(s)", cells,
                 total_cells);
}

// LOOP-CLOSE-LATCH-SPIN (TODO.md; loop_engine.hpp's own comment on the
// `closing` read inside loop_step()) - EXPECTED RED IN THIS COMMIT, ON
// PURPOSE. The trinco is armed BEFORE the very first step(), and it
// never resets (window_state.hpp: "There is no unset_close_request()")
// - so `closing` reads true from step 1 onward and the hidden-window
// wait / frame-rate cap never run again, even though this test's own
// consumer keeps calling step() the way a veto (D-W6b-54) does. S3
// removes the `closing` read from gating these two steps; this test is
// NOT touched by that commit - it goes green because the code under it
// changed, exactly the L-20 shape this sub-fatia's own briefing names.
GLINTFX_TEST(close_latched_consumer_keeps_stepping_waits_still_happen) {
    engine_fixture fx;
    // The clock advances only by its own step (1 ms/read), never by
    // the wait's own budget - so wait_for_frame_cap()'s coarse loop
    // below (once S3 lets it run again) takes several genuine
    // iterations to reach the 10 ms deadline instead of overshooting
    // it in one jump, which is what actually produces a cap-wait
    // budget in the 1..9 ms range this test also checks for.
    fx.display.set_clock_to_advance(nullptr);
    fx.window.request_close();
    fx.book.last_present = gltfx_present_outcome::skipped_hidden;
    fx.context.set_present_would_skip(true);
    fx.context.set_frame_rate_cap_hz(100);

    for (int i = 0; i < 3; ++i) {
        const gltfx_rslt<glintfx::gltfx_frame_tick> stepped =
            glintfx::platform::loop_step(fx.ports(), fx.book);
        GLINTFX_CHECK(stepped.has_value());
    }

    GLINTFX_CHECK(fx.log.count(loop_event::wait) >= 3);

    const std::vector<std::uint32_t> wait_budgets = fx.log.budgets(loop_event::wait);
    int hidden_budget_count = 0;
    bool saw_cap_budget = false;
    for (std::uint32_t budget : wait_budgets) {
        if (budget == 100) {
            ++hidden_budget_count;
        } else if (budget >= 1 && budget <= 9) {
            saw_cap_budget = true;
        }
    }
    GLINTFX_CHECK(hidden_budget_count >= 3);
    GLINTFX_CHECK(saw_cap_budget);
}

GLINTFX_TEST(hidden_wait_is_100ms_then_pump_again_before_the_probe) {
    engine_fixture fx;
    fx.book.last_present = gltfx_present_outcome::skipped_hidden;

    const gltfx_rslt<glintfx::gltfx_frame_tick> stepped =
        glintfx::platform::loop_step(fx.ports(), fx.book);
    GLINTFX_CHECK(stepped.has_value());

    const std::vector<loop_event> expected = {loop_event::pump, loop_event::wait, loop_event::pump,
                                              loop_event::clock, loop_event::probe};
    GLINTFX_CHECK(fx.log.sequence() == expected);
    const std::vector<std::uint32_t> wait_budgets = fx.log.budgets(loop_event::wait);
    GLINTFX_CHECK_EQ(wait_budgets.size(), static_cast<std::size_t>(1));
    GLINTFX_CHECK_EQ(wait_budgets[0], static_cast<std::uint32_t>(100));
}

GLINTFX_TEST(frame_cap_waits_by_deadline_spins_the_last_ms_and_reads_the_cap_live) {
    engine_fixture fx;
    fx.context.set_frame_rate_cap_hz(100); // periodo de 10 ms

    // (a) Passo 1 arma o prazo sem esperar.
    const gltfx_rslt<glintfx::gltfx_frame_tick> step1 =
        glintfx::platform::loop_step(fx.ports(), fx.book);
    GLINTFX_CHECK(step1.has_value());
    GLINTFX_CHECK_EQ(fx.log.count(loop_event::wait), 0);

    // (b) Passo 2: elapsed >= 10 ms; toda espera de teto tem orcamento
    // entre 1 e 9; ao menos uma leitura de relogio depois da ultima
    // espera (o giro).
    const auto wait_count_before_step2 = static_cast<std::size_t>(fx.log.count(loop_event::wait));
    const gltfx_rslt<glintfx::gltfx_frame_tick> step2 =
        glintfx::platform::loop_step(fx.ports(), fx.book);
    GLINTFX_CHECK(step2.has_value());
    GLINTFX_CHECK(step2.value().elapsed.nanoseconds >= 10'000'000);

    const std::vector<std::uint32_t> wait_budgets_after_step2 = fx.log.budgets(loop_event::wait);
    GLINTFX_CHECK(wait_budgets_after_step2.size() > wait_count_before_step2);
    for (std::size_t i = wait_count_before_step2; i < wait_budgets_after_step2.size(); ++i) {
        GLINTFX_CHECK(wait_budgets_after_step2[i] >= 1 && wait_budgets_after_step2[i] <= 9);
    }
    // O giro: pelo menos uma leitura `clock` alem da que fecha o
    // tique - a que o coarse loop de wait_for_frame_cap() faz depois
    // de cada `wait` (loop_engine.hpp's own comment on this exact
    // shape).
    GLINTFX_CHECK(fx.log.count(loop_event::clock) >= 3);

    // (c) Antes do passo 3, desarma o teto ao vivo: passo 3 nao espera.
    const int wait_count_before_step3 = fx.log.count(loop_event::wait);
    fx.context.set_frame_rate_cap_hz(0);
    const gltfx_rslt<glintfx::gltfx_frame_tick> step3 =
        glintfx::platform::loop_step(fx.ports(), fx.book);
    GLINTFX_CHECK(step3.has_value());
    GLINTFX_CHECK_EQ(fx.log.count(loop_event::wait), wait_count_before_step3);

    // (d) Um segundo motor com teto 0 desde o inicio, tres passos.
    engine_fixture fx2;
    for (int i = 0; i < 3; ++i) {
        const gltfx_rslt<glintfx::gltfx_frame_tick> stepped =
            glintfx::platform::loop_step(fx2.ports(), fx2.book);
        GLINTFX_CHECK(stepped.has_value());
    }
    GLINTFX_CHECK_EQ(fx2.log.count(loop_event::wait), 0);

    std::println(
        "frame_cap_waits_by_deadline_spins_the_last_ms_and_reads_the_cap_live: 4 of 4 cell(s)");
}
