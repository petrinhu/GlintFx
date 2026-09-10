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
#include "platform/loop/loop_impl.hpp"
#include "platform/loop/owned_loop_context.hpp"
#include "platform/loop/store_loop_callbacks.hpp"
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

    // LOOP-CONTEXT-OWNERSHIP (S1b): loop_run()'s own fourth parameter -
    // T1..T9 below never test posse itself (T13..T16 do, each building
    // its OWN owned_loop_context with a logging destroy_context), so a
    // single empty (default-constructed, two nulls) atom per fixture is
    // enough: its destructor is a no-op either way.
    glintfx::platform::owned_loop_context owned;

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
        const gltfx_rslt<void> result =
            glintfx::platform::loop_run(fx.ports(), fx.book, callbacks, fx.owned);
        GLINTFX_CHECK(result.has_error());
        GLINTFX_CHECK(result.err().rejected_value() == "on_frame");
        GLINTFX_CHECK_EQ(fx.log.count(loop_event::pump), 0);
        GLINTFX_CHECK_EQ(fx.log.count(loop_event::clock), 0);
        GLINTFX_CHECK_EQ(fx.log.count(loop_event::probe), 0);
        ++cells;
    }
    {
        // LOOP-CONTEXT-OWNERSHIP (S1b), D-LF-7: destroy_context WITH a
        // context is no longer refused (S1a's own cell here used to
        // test exactly that refusal - it no longer holds, see
        // loop_callbacks_validation.cpp's own header comment). What
        // STILL refuses by name is destroy_context set with NO context
        // to destroy - `context` is left nullptr on purpose below.
        engine_fixture fx;
        gltfx_loop_callbacks callbacks{};
        callbacks.on_frame = &log_on_frame;
        callbacks.on_render = &log_on_render;
        callbacks.destroy_context = &noop_destroy_context;
        const gltfx_rslt<void> result =
            glintfx::platform::loop_run(fx.ports(), fx.book, callbacks, fx.owned);
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

    const gltfx_rslt<void> result =
        glintfx::platform::loop_run(fx.ports(), fx.book, callbacks, fx.owned);
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

    const gltfx_rslt<void> result =
        glintfx::platform::loop_run(fx.ports(), fx.book, callbacks, fx.owned);
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

    const gltfx_rslt<void> result =
        glintfx::platform::loop_run(fx.ports(), fx.book, callbacks, fx.owned);
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

    const gltfx_rslt<void> result =
        glintfx::platform::loop_run(fx.ports(), fx.book, callbacks, fx.owned);
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

    const gltfx_rslt<void> result =
        glintfx::platform::loop_run(fx.ports(), fx.book, callbacks, fx.owned);
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

    const gltfx_rslt<void> result =
        glintfx::platform::loop_run(fx.ports(), fx.book, callbacks, fx.owned);
    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK(result.err().code() == code);
    GLINTFX_CHECK(result.err().rejected_value() == "pump");
    GLINTFX_CHECK_EQ(fx.log.count(loop_event::on_frame), 1);
    GLINTFX_CHECK_EQ(fx.log.count(loop_event::clock), 1);
}

// LOOP-CONTEXT-OWNERSHIP (S1b), achado de revisao (buraco de cobertura,
// nao defeito vivo): running_guard.hpp's own RAII e' o que hoje garante
// que book.running volta a false em QUALQUER saida de loop_run(), os
// dois caminhos de erro (present_error_returns_unchanged_and_stops_the_
// loop, pump_error_returns_unchanged_before_on_frame, ambos acima)
// inclusive - mas nenhum dos dois olhava o estado DEPOIS do erro. Uma
// mutacao plausivel (um refactor que troca o guard RAII por um desarme
// manual so' nos dois caminhos de sucesso) passava os 16 casos antigos
// sem que nenhum reprovasse. Prova pelo EFEITO observavel, nunca
// espiando `book.running` diretamente (running_guard.hpp's own
// comentario: a decisao de recusar mora em loop_run(), nao no atomo) -
// uma SEGUNDA chamada, no MESMO fx.book, depois do erro: se `running`
// tivesse ficado travado em true, essa segunda chamada seria recusada
// por nome ("running", T16's own shape) antes de tocar qualquer porta,
// nunca chegaria a rodar o tique unico que ela pede.
GLINTFX_TEST(running_flag_clears_after_an_error_path_so_a_second_call_is_accepted) {
    int cells = 0;
    constexpr int total_cells = 2;

    // (1) O mesmo caminho de present_error_returns_unchanged_and_stops_
    // the_loop (acima): swap_buffers() falha, loop_run() retorna erro.
    {
        engine_fixture fx;
        const glintfx::gltfx_err_code code = glintfx::gltfx_err_code::platform_failure;
        fx.context.arm_swap_sequence({gltfx_rslt<gltfx_present_outcome>::err(
            glintfx::gltfx_err(code).with_rejected_value("swap"))});
        fx.display.close_on_pump(3, fx.window); // rede de seguranca, nunca dispara

        gltfx_loop_callbacks callbacks{};
        callbacks.context = &fx.log;
        callbacks.on_frame = &log_on_frame;
        callbacks.on_render = &log_on_render;

        const gltfx_rslt<void> first_result =
            glintfx::platform::loop_run(fx.ports(), fx.book, callbacks, fx.owned);
        GLINTFX_CHECK(first_result.has_error());
        GLINTFX_CHECK(first_result.err().rejected_value() == "swap");

        // on_frame devolve false agora - a segunda chamada encerra apos
        // UM tique, sem nunca chegar a on_render/swap (o outcome de erro
        // armado acima nunca e' consumido de novo).
        fx.log.on_frame_returns = false;
        const gltfx_rslt<void> second_result =
            glintfx::platform::loop_run(fx.ports(), fx.book, callbacks, fx.owned);
        GLINTFX_CHECK(second_result.has_value());
        ++cells;
    }

    // (2) O mesmo caminho de pump_error_returns_unchanged_before_on_
    // frame (acima): pump_events() falha na 2a chamada contada,
    // loop_run() retorna erro antes de chamar on_frame daquele tique.
    {
        engine_fixture fx;
        const glintfx::gltfx_err_code code = glintfx::gltfx_err_code::io_failure;
        fx.display.fail_on_pump(2, code);
        fx.display.close_on_pump(5, fx.window); // rede de seguranca, nunca dispara

        gltfx_loop_callbacks callbacks{};
        callbacks.context = &fx.log;
        callbacks.on_frame = &log_on_frame;
        callbacks.on_render = &log_on_render;

        const gltfx_rslt<void> first_result =
            glintfx::platform::loop_run(fx.ports(), fx.book, callbacks, fx.owned);
        GLINTFX_CHECK(first_result.has_error());
        GLINTFX_CHECK(first_result.err().rejected_value() == "pump");

        // fail_on_pump(2, ...) so' dispara na chamada CONTADA de numero
        // 2 - a contagem so' cresce, entao a segunda loop_run() nunca
        // reve aquele numero e nunca refalha por essa mesma armadilha.
        fx.log.on_frame_returns = false;
        const gltfx_rslt<void> second_result =
            glintfx::platform::loop_run(fx.ports(), fx.book, callbacks, fx.owned);
        GLINTFX_CHECK(second_result.has_value());
        ++cells;
    }

    std::println(
        "running_flag_clears_after_an_error_path_so_a_second_call_is_accepted: {} of {} cell(s)",
        cells, total_cells);
}

GLINTFX_TEST(close_request_ends_ok_after_the_full_tick_it_arrived_in) {
    engine_fixture fx;
    fx.display.close_on_pump(2, fx.window);

    gltfx_loop_callbacks callbacks{};
    callbacks.context = &fx.log;
    callbacks.on_frame = &log_on_frame;
    callbacks.on_render = &log_on_render;

    const gltfx_rslt<void> result =
        glintfx::platform::loop_run(fx.ports(), fx.book, callbacks, fx.owned);
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

        const gltfx_rslt<void> result =
            glintfx::platform::loop_run(fx.ports(), fx.book, callbacks, fx.owned);
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

        const gltfx_rslt<void> result =
            glintfx::platform::loop_run(fx.ports(), fx.book, callbacks, fx.owned);
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

// ======================================================================
// T13..T16 - LOOP-CONTEXT-OWNERSHIP (S1b, /var/tmp/glintfx-plan/
// loop-fix.md sec. S1b): platform::loop_run()'s own fourth parameter
// (owned_loop_context&, loop_engine.hpp) and platform::
// store_loop_callbacks() (store_loop_callbacks.hpp), both forms of
// posse (P11/P12, include/glintfx/platform/loop/loop.hpp).
// ======================================================================

namespace {

using glintfx::platform::owned_loop_context;

// Pushed into the SAME shared log every other callback in this file
// writes to (S2.3's own table) - `context` here is ALWAYS the log
// itself (the same convention log_on_frame/log_on_render already use),
// so ordering ("destroyed AFTER the last callback") is provable by
// reading loop_call_log::records() back, never by a second registry.
void log_destroy_context(void *context) noexcept {
    auto *log = static_cast<loop_call_log *>(context);
    log->push(loop_event::destroy, 0, 0, context);
}

// T15(b): a destroy_context that must NEVER fire in that cell - a
// plain counter, deliberately NOT cast through loop_call_log (the
// context it is armed with there is not one), so a mistaken call
// would corrupt nothing, only miscount.
int g_marker_destroy_count = 0;

void counting_destroy_context(void * /*context*/) noexcept { ++g_marker_destroy_count; }

// T16: the nested call a case's own on_frame makes needs state a bare
// function pointer cannot capture - the SAME exception T9(b)'s own
// g_static_log already documents for this file.
engine_fixture *g_reentrant_fx = nullptr;
glintfx::loop_impl *g_reentrant_impl = nullptr;
int g_reentrant_which = 0; // 1 = run(cb), 2 = run() [forma 2], 3 = set_callbacks()'s own engine
bool g_reentrant_nested_called = false;
gltfx_rslt<void> g_reentrant_nested_result =
    gltfx_rslt<void>::err(glintfx::gltfx_err(glintfx::gltfx_err_code::unknown));

bool reentrant_on_frame(void * /*context*/, const glintfx::gltfx_frame_tick & /*tick*/) noexcept {
    g_reentrant_nested_called = true;
    if (g_reentrant_which == 1) {
        gltfx_loop_callbacks nested{};
        nested.context = &g_reentrant_fx->log;
        nested.on_frame = &log_on_frame;
        nested.on_render = &log_on_render;
        owned_loop_context nested_owned{};
        g_reentrant_nested_result = glintfx::platform::loop_run(
            g_reentrant_fx->ports(), g_reentrant_impl->book, nested, nested_owned);
    } else if (g_reentrant_which == 2) {
        owned_loop_context nested_owned{};
        g_reentrant_nested_result =
            glintfx::platform::loop_run(g_reentrant_fx->ports(), g_reentrant_impl->book,
                                        g_reentrant_impl->book.stored_callbacks, nested_owned);
    } else {
        gltfx_loop_callbacks nested{};
        nested.context = &g_reentrant_fx->log;
        nested.on_frame = &log_on_frame;
        nested.on_render = &log_on_render;
        g_reentrant_nested_result =
            glintfx::platform::store_loop_callbacks(*g_reentrant_impl, nested);
    }
    return false; // ends the OUTER loop after this one tick, whichever branch ran.
}

} // namespace

GLINTFX_TEST(context_ownership_form1_destroys_exactly_once_after_the_last_callback_on_every_exit) {
    int cells = 0;
    constexpr int total_cells = 5;

    // (1) Refusal: destroy_context is called even though on_frame is
    // missing and validate_loop_callbacks() never let any port run.
    {
        engine_fixture fx;
        gltfx_loop_callbacks callbacks{};
        callbacks.context = &fx.log;
        callbacks.on_render = &log_on_render;
        callbacks.destroy_context = &log_destroy_context;
        {
            owned_loop_context owned{callbacks.context, callbacks.destroy_context};
            const gltfx_rslt<void> result =
                glintfx::platform::loop_run(fx.ports(), fx.book, callbacks, owned);
            GLINTFX_CHECK(result.has_error());
            GLINTFX_CHECK(result.err().rejected_value() == "on_frame");
            GLINTFX_CHECK_EQ(fx.log.count(loop_event::destroy), 0); // owned still alive here
        }
        GLINTFX_CHECK_EQ(fx.log.count(loop_event::destroy), 1);
        GLINTFX_CHECK(fx.log.records().back().event == loop_event::destroy);
        ++cells;
    }

    // (2) on_frame returns false - ok().
    {
        engine_fixture fx;
        fx.log.on_frame_returns = false;
        gltfx_loop_callbacks callbacks{};
        callbacks.context = &fx.log;
        callbacks.on_frame = &log_on_frame;
        callbacks.on_render = &log_on_render;
        callbacks.destroy_context = &log_destroy_context;
        {
            owned_loop_context owned{callbacks.context, callbacks.destroy_context};
            const gltfx_rslt<void> result =
                glintfx::platform::loop_run(fx.ports(), fx.book, callbacks, owned);
            GLINTFX_CHECK(result.has_value());
            GLINTFX_CHECK_EQ(fx.log.count(loop_event::destroy), 0);
        }
        GLINTFX_CHECK_EQ(fx.log.count(loop_event::destroy), 1);
        GLINTFX_CHECK(fx.log.records().back().event == loop_event::destroy);
        ++cells;
    }

    // (3) present() error - the SAME arrangement T6 already uses.
    {
        engine_fixture fx;
        const glintfx::gltfx_err_code code = glintfx::gltfx_err_code::platform_failure;
        fx.context.arm_swap_sequence({gltfx_rslt<gltfx_present_outcome>::err(
            glintfx::gltfx_err(code).with_rejected_value("swap"))});
        fx.display.close_on_pump(3, fx.window); // rede de seguranca, nunca dispara
        gltfx_loop_callbacks callbacks{};
        callbacks.context = &fx.log;
        callbacks.on_frame = &log_on_frame;
        callbacks.on_render = &log_on_render;
        callbacks.destroy_context = &log_destroy_context;
        {
            owned_loop_context owned{callbacks.context, callbacks.destroy_context};
            const gltfx_rslt<void> result =
                glintfx::platform::loop_run(fx.ports(), fx.book, callbacks, owned);
            GLINTFX_CHECK(result.has_error());
            GLINTFX_CHECK_EQ(fx.log.count(loop_event::destroy), 0);
        }
        GLINTFX_CHECK_EQ(fx.log.count(loop_event::destroy), 1);
        GLINTFX_CHECK(fx.log.records().back().event == loop_event::destroy);
        ++cells;
    }

    // (4) pump() error - the SAME arrangement T7 already uses.
    {
        engine_fixture fx;
        const glintfx::gltfx_err_code code = glintfx::gltfx_err_code::io_failure;
        fx.display.fail_on_pump(2, code);
        fx.display.close_on_pump(5, fx.window); // rede de seguranca, nunca dispara
        gltfx_loop_callbacks callbacks{};
        callbacks.context = &fx.log;
        callbacks.on_frame = &log_on_frame;
        callbacks.on_render = &log_on_render;
        callbacks.destroy_context = &log_destroy_context;
        {
            owned_loop_context owned{callbacks.context, callbacks.destroy_context};
            const gltfx_rslt<void> result =
                glintfx::platform::loop_run(fx.ports(), fx.book, callbacks, owned);
            GLINTFX_CHECK(result.has_error());
            GLINTFX_CHECK_EQ(fx.log.count(loop_event::destroy), 0);
        }
        GLINTFX_CHECK_EQ(fx.log.count(loop_event::destroy), 1);
        GLINTFX_CHECK(fx.log.records().back().event == loop_event::destroy);
        ++cells;
    }

    // (5) close_requested() - the SAME arrangement T8 already uses.
    {
        engine_fixture fx;
        fx.display.close_on_pump(2, fx.window);
        gltfx_loop_callbacks callbacks{};
        callbacks.context = &fx.log;
        callbacks.on_frame = &log_on_frame;
        callbacks.on_render = &log_on_render;
        callbacks.destroy_context = &log_destroy_context;
        {
            owned_loop_context owned{callbacks.context, callbacks.destroy_context};
            const gltfx_rslt<void> result =
                glintfx::platform::loop_run(fx.ports(), fx.book, callbacks, owned);
            GLINTFX_CHECK(result.has_value());
            GLINTFX_CHECK_EQ(fx.log.count(loop_event::destroy), 0);
        }
        GLINTFX_CHECK_EQ(fx.log.count(loop_event::destroy), 1);
        GLINTFX_CHECK(fx.log.records().back().event == loop_event::destroy);
        ++cells;
    }

    std::println("context_ownership_form1_destroys_exactly_once_after_the_last_callback_on_every_"
                 "exit: {} of {} cell(s)",
                 cells, total_cells);
}

GLINTFX_TEST(context_ownership_form1_never_calls_a_null_destroy_context) {
    engine_fixture fx;
    fx.log.on_frame_returns = false;
    gltfx_loop_callbacks callbacks{};
    callbacks.context = &fx.log;
    callbacks.on_frame = &log_on_frame;
    callbacks.on_render = &log_on_render;
    // destroy_context LEFT NULL on purpose - `context` stays BORROWED.
    owned_loop_context owned{callbacks.context, callbacks.destroy_context};
    const gltfx_rslt<void> result =
        glintfx::platform::loop_run(fx.ports(), fx.book, callbacks, owned);
    GLINTFX_CHECK(result.has_value());
    GLINTFX_CHECK_EQ(fx.log.count(loop_event::destroy), 0);
}

GLINTFX_TEST(context_ownership_form2_survives_repeated_run_calls_form1_ignores_it) {
    int cells = 0;
    constexpr int total_cells = 2;

    // (a) FORM 2: stored ONCE, loop_run() called TWICE against the
    // SAME impl->book - zero destructions across both calls, and the
    // SAME context reported both times (P12: "survives any number of
    // run() calls in between"). Destroyed only when impl itself is.
    {
        engine_fixture fx;
        fx.log.on_frame_returns = false; // ends after exactly one tick, both times
        glintfx::loop_impl *impl = glintfx::allocate_loop_impl().value();

        gltfx_loop_callbacks callbacks{};
        callbacks.context = &fx.log;
        callbacks.on_frame = &log_on_frame;
        callbacks.on_render = &log_on_render;
        callbacks.destroy_context = &log_destroy_context;
        const gltfx_rslt<void> stored = glintfx::platform::store_loop_callbacks(*impl, callbacks);
        GLINTFX_CHECK(stored.has_value());

        owned_loop_context none_a{};
        const gltfx_rslt<void> first_run = glintfx::platform::loop_run(
            fx.ports(), impl->book, impl->book.stored_callbacks, none_a);
        GLINTFX_CHECK(first_run.has_value());
        GLINTFX_CHECK_EQ(fx.log.count(loop_event::destroy), 0);

        owned_loop_context none_b{};
        const gltfx_rslt<void> second_run = glintfx::platform::loop_run(
            fx.ports(), impl->book, impl->book.stored_callbacks, none_b);
        GLINTFX_CHECK(second_run.has_value());
        GLINTFX_CHECK_EQ(fx.log.count(loop_event::destroy), 0);
        GLINTFX_CHECK(impl->book.stored_callbacks.context == static_cast<void *>(&fx.log));

        delete impl;
        GLINTFX_CHECK_EQ(fx.log.count(loop_event::destroy), 1); // only now, F29
        ++cells;
    }

    // (b) FORM 1 alongside a stored FORM 2 callback (D-LF-6b,
    // ortogonalidade): the stored one is left completely untouched,
    // and the per-call one still destroys on its own exit.
    {
        engine_fixture fx;
        glintfx::loop_impl *impl = glintfx::allocate_loop_impl().value();
        g_marker_destroy_count = 0;

        int stored_marker = 0;
        gltfx_loop_callbacks stored_callbacks{};
        stored_callbacks.context = &stored_marker;
        stored_callbacks.on_frame = &log_on_frame; // never exercised in this cell
        stored_callbacks.on_render = &log_on_render;
        stored_callbacks.destroy_context = &counting_destroy_context;
        const gltfx_rslt<void> stored =
            glintfx::platform::store_loop_callbacks(*impl, stored_callbacks);
        GLINTFX_CHECK(stored.has_value());

        fx.log.on_frame_returns = false;
        gltfx_loop_callbacks per_call{};
        per_call.context = &fx.log;
        per_call.on_frame = &log_on_frame;
        per_call.on_render = &log_on_render;
        per_call.destroy_context = &log_destroy_context;
        {
            owned_loop_context owned{per_call.context, per_call.destroy_context};
            const gltfx_rslt<void> ran =
                glintfx::platform::loop_run(fx.ports(), impl->book, per_call, owned);
            GLINTFX_CHECK(ran.has_value());
            GLINTFX_CHECK_EQ(fx.log.count(loop_event::destroy), 0); // owned still alive here
        }
        GLINTFX_CHECK_EQ(fx.log.count(loop_event::destroy), 1); // per-call destroyed on exit
        GLINTFX_CHECK_EQ(g_marker_destroy_count, 0);            // stored one untouched
        GLINTFX_CHECK(impl->book.stored_callbacks.context == static_cast<void *>(&stored_marker));

        delete impl;
        GLINTFX_CHECK_EQ(g_marker_destroy_count, 1); // only now, F29
        ++cells;
    }

    std::println("context_ownership_form2_survives_repeated_run_calls_form1_ignores_it: {} of {} "
                 "cell(s)",
                 cells, total_cells);
}

GLINTFX_TEST(reentrant_calls_from_inside_on_frame_are_refused_by_name) {
    int cells = 0;
    constexpr int total_cells = 3;

    // (1) A FORM 1 run(cb) whose own on_frame calls run(cb) AGAIN.
    {
        engine_fixture fx;
        glintfx::loop_impl *impl = glintfx::allocate_loop_impl().value();
        g_reentrant_fx = &fx;
        g_reentrant_impl = impl;
        g_reentrant_which = 1;
        g_reentrant_nested_called = false;

        gltfx_loop_callbacks outer{};
        outer.context = &fx.log;
        outer.on_frame = &reentrant_on_frame;
        outer.on_render = &log_on_render;
        outer.destroy_context = &log_destroy_context;
        {
            owned_loop_context outer_owned{outer.context, outer.destroy_context};
            const gltfx_rslt<void> outer_result =
                glintfx::platform::loop_run(fx.ports(), impl->book, outer, outer_owned);
            GLINTFX_CHECK(outer_result.has_value());
        }
        GLINTFX_CHECK(g_reentrant_nested_called);
        GLINTFX_CHECK(g_reentrant_nested_result.has_error());
        GLINTFX_CHECK(g_reentrant_nested_result.err().rejected_value() == "running");
        GLINTFX_CHECK_EQ(fx.log.count(loop_event::destroy), 1); // only the outer's own, once

        g_reentrant_fx = nullptr;
        g_reentrant_impl = nullptr;
        delete impl;
        ++cells;
    }

    // (2) A FORM 1 run(cb) whose own on_frame calls run() (FORM 2's
    // no-argument shape) AGAIN, on the SAME impl->book.
    {
        engine_fixture fx;
        glintfx::loop_impl *impl = glintfx::allocate_loop_impl().value();
        g_reentrant_fx = &fx;
        g_reentrant_impl = impl;
        g_reentrant_which = 2;
        g_reentrant_nested_called = false;

        gltfx_loop_callbacks outer{};
        outer.context = &fx.log;
        outer.on_frame = &reentrant_on_frame;
        outer.on_render = &log_on_render;
        {
            owned_loop_context outer_owned{};
            const gltfx_rslt<void> outer_result =
                glintfx::platform::loop_run(fx.ports(), impl->book, outer, outer_owned);
            GLINTFX_CHECK(outer_result.has_value());
        }
        GLINTFX_CHECK(g_reentrant_nested_called);
        GLINTFX_CHECK(g_reentrant_nested_result.has_error());
        GLINTFX_CHECK(g_reentrant_nested_result.err().rejected_value() == "running");

        g_reentrant_fx = nullptr;
        g_reentrant_impl = nullptr;
        delete impl;
        ++cells;
    }

    // (3) FORM 2's own run() calls set_callbacks()'s own engine
    // (store_loop_callbacks()) from INSIDE the stored on_frame - the
    // exact scenario running_guard exists for (loop.hpp's own header
    // comment on set_callbacks()): without it, this would destroy the
    // very context THIS call is still using.
    {
        engine_fixture fx;
        glintfx::loop_impl *impl = glintfx::allocate_loop_impl().value();
        gltfx_loop_callbacks stored{};
        stored.context = &fx.log;
        stored.on_frame = &reentrant_on_frame;
        stored.on_render = &log_on_render;
        stored.destroy_context = &log_destroy_context;
        const gltfx_rslt<void> store_result =
            glintfx::platform::store_loop_callbacks(*impl, stored);
        GLINTFX_CHECK(store_result.has_value());

        g_reentrant_fx = &fx;
        g_reentrant_impl = impl;
        g_reentrant_which = 3;
        g_reentrant_nested_called = false;

        owned_loop_context none{};
        const gltfx_rslt<void> outer_result =
            glintfx::platform::loop_run(fx.ports(), impl->book, impl->book.stored_callbacks, none);
        GLINTFX_CHECK(outer_result.has_value());
        GLINTFX_CHECK(g_reentrant_nested_called);
        GLINTFX_CHECK(g_reentrant_nested_result.has_error());
        GLINTFX_CHECK(g_reentrant_nested_result.err().rejected_value() == "running");
        // The context THIS call was still using survives - the nested
        // store_loop_callbacks() was refused BEFORE it could destroy it.
        GLINTFX_CHECK_EQ(fx.log.count(loop_event::destroy), 0);
        GLINTFX_CHECK(impl->book.stored_callbacks.context == static_cast<void *>(&fx.log));

        g_reentrant_fx = nullptr;
        g_reentrant_impl = nullptr;
        delete impl;
        GLINTFX_CHECK_EQ(fx.log.count(loop_event::destroy), 1); // only now, F29
        ++cells;
    }

    std::println("reentrant_calls_from_inside_on_frame_are_refused_by_name: {} of {} cell(s)",
                 cells, total_cells);
}
