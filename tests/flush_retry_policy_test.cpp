// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/wayland/bounded_output_wait.hpp"
#include "platform/wayland/flush_retry_policy.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// flush_retry_policy_test.cpp - LOOP-RUN fatia 7, conserto pos-revisao
// (docs/plano-w6b-fatias-6-8.md, D-W6b-57; GODS_LAWS.md L-20): the TDD
// red/green witness for glintfx::platform::flush_write_wait_is_fatal()
// (src/platform/wayland/flush_retry_policy.hpp) - the exact decision
// that was missing a test entirely before this fatia's own revisao
// adversarial (07/09/2026) named it: "nenhum teste existente
// exercitaria uma regressao aqui" (display_adapter.cpp::flush_with_
// retry() ignorava o orcamento do chamador e sempre trancava m_fatal,
// mesmo com budget_ms == 0).
//
// RED, SEEN: before flush_retry_policy.{hpp,cpp} existed, this file's
// own #include line for flush_retry_policy.hpp failed to compile -
// `fatal: platform/wayland/flush_retry_policy.hpp: No such file or
// directory` (this fatia's own commit message carries the literal
// compiler transcript). No socket, no wl_display, no container needed:
// bounded_wait_outcome is plain data the caller already has after its
// own call to wait_for_writable_until() (already proven bounded and
// never-hanging by tests/bounded_output_wait_test.cpp) - this atom
// only decides what that outcome MEANS for the connection, which is
// exactly the four cases below.

using glintfx::platform::bounded_wait_outcome;
using glintfx::platform::flush_write_wait_is_fatal;

GLINTFX_TEST(flush_retry_policy_zero_budget_timeout_is_never_fatal) {
    // D-W6b-57's own text, verbatim reasoning: pump_events()'s own
    // budget_ms == 0 call means "is the kernel send buffer free RIGHT
    // NOW" - a buffer still full is normal, transient backpressure
    // under load, never proof the compositor died. THIS is the exact
    // case that used to latch m_fatal with no test catching it.
    GLINTFX_CHECK(!flush_write_wait_is_fatal(0, bounded_wait_outcome::timed_out));
}

GLINTFX_TEST(flush_retry_policy_zero_budget_poll_failure_is_still_fatal) {
    // Zero budget forgives a merely-full buffer, never a genuinely
    // broken socket (POLLERR/POLLHUP/POLLNVAL, or poll() itself
    // failing) - bounded_output_wait.hpp's own "conexao inutilizavel"
    // verdict for poll_failed does not become "try again next time"
    // just because the caller only asked to check once.
    GLINTFX_CHECK(flush_write_wait_is_fatal(0, bounded_wait_outcome::poll_failed));
}

GLINTFX_TEST(flush_retry_policy_nonzero_budget_timeout_is_fatal) {
    // wait_events()'s own caller-chosen budget (e.g. 100ms) already
    // gave the compositor real time to drain the socket - exhausting
    // it is the same "connection no longer usable" verdict 72754af
    // already reaches for egl_context_adapter.cpp's twin write-wait,
    // always a non-zero budget there.
    GLINTFX_CHECK(flush_write_wait_is_fatal(100, bounded_wait_outcome::timed_out));
}

GLINTFX_TEST(flush_retry_policy_nonzero_budget_poll_failure_is_fatal) {
    GLINTFX_CHECK(flush_write_wait_is_fatal(100, bounded_wait_outcome::poll_failed));
}

// FRONTEIRA NAO EXERCITADA (revisao adversarial, 08/09/2026, GODS_LAWS.md
// L-17): os quatro casos acima so usam budget_ms 0 e 100 - a faixa 1..99
// nunca foi tocada por ninguem. A regra real (flush_retry_policy.cpp) tem
// UMA fronteira logica so: budget_ms == 0 (perdoado) contra qualquer
// budget_ms != 0 (fatal). Uma mutacao que trocasse `!= 0` por `> K` para
// qualquer K >= 1 sobreviveria aos quatro casos existentes sem que nada
// ficasse vermelho - provado por mutation testing contra `> 1`
// especificamente (relatorio da fatia, nao versionado aqui).
//
// Um unico caso fecha essa fronteira inteira: budget_ms == 1 e o valor
// nao-zero MAIS PROXIMO do zero ja coberto acima - e por isso o unico
// ponto onde a regra correta (fatal, porque 1 != 0) e QUALQUER mutante
// da familia `budget_ms > K` (K >= 1) divergem sempre (1 > K e falso
// para todo K >= 1). Nao ha necessidade de enumerar os demais valores de
// 2 a 99: nenhum outro mutante plausivel dessa comparacao introduz uma
// segunda fronteira ali - a unica fronteira que a regra em si declara e
// 0 contra nao-zero, e este caso e o ponto mais estreito dela.
GLINTFX_TEST(flush_retry_policy_boundary_budget_of_one_timeout_is_fatal) {
    GLINTFX_CHECK(flush_write_wait_is_fatal(1, bounded_wait_outcome::timed_out));
}
