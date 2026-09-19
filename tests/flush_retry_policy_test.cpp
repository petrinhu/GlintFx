// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/wayland/bounded_output_wait.hpp"
#include "platform/wayland/flush_retry_policy.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// flush_retry_policy_test.cpp - LOOP-RUN fatia 7, conserto pos-revisao
// (docs/plano-w6b-fatias-6-8.md, D-W6b-57), REABERTO por
// WL-WRITE-TIMEOUT-NAO-FATAL (TODO.md; ordem do lider, 08/09/2026, via
// AskUserQuestion): the TDD red/green witness for glintfx::platform::
// flush_write_wait_is_fatal() (src/platform/wayland/flush_retry_
// policy.hpp).
//
// A DECISAO NOVA, E O QUE ELA REVERTE: pesquisa externa (citada na
// propria linha da TODO.md) mediu que NENHUM cliente de referencia mata
// a conexao por tempo de escrita esgotado - o manual do libwayland
// manda usar poll() e esperar, o GLFW espera SEM LIMITE por POLLOUT, o
// SDL segue adiante sem marcar nada. D-W6b-57 (07/09/2026) ja tinha
// consertado UM defeito real (m_fatal travando mesmo com budget_ms ==
// 0), mas manteve um segundo: qualquer orcamento != 0 esgotado
// continuava fatal, uma regra que a propria TODO.md nomeia como
// acidental (o limiar era literalmente "quanto tempo o chamador
// escolheu dormir", 100ms a quase 1s conforme o teto de quadros) e
// assimetrica (o mesmo timeout do lado LEITURA, em
// wait_for_incoming_data() dentro de display_adapter.cpp, ja era
// perdoado incondicionalmente, sob qualquer orcamento). Esta fatia
// fecha as duas: agora `budget_ms` NAO EXISTE MAIS na assinatura - a
// unica coisa que decide se a espera de escrita e' fatal e' o
// `outcome` em si. timed_out nunca e' fatal (buffer do kernel cheio
// agora prova so' isso, nunca que o par do outro lado morreu);
// poll_failed continua fatal SEMPRE (POLLERR/POLLHUP/POLLNVAL, ou
// poll() falhando de verdade, e' a conexao QUEBRADA, categoria
// diferente de "ainda nao drenou").
//
// RED, SEEN (WL-WRITE-TIMEOUT-NAO-FATAL): antes deste conserto,
// flush_retry_policy_nonzero_budget_timeout_is_never_fatal chamava
// `flush_write_wait_is_fatal(100, bounded_wait_outcome::timed_out)` e
// exigia falso - a implementacao antiga devolvia verdadeiro para
// qualquer budget_ms != 0, reprovando o caso. Ver o relato da fatia
// para o codigo de saida medido do binario antes e depois.
//
// No socket, no wl_display, no container needed: bounded_wait_outcome
// is plain data the caller already has after its own call to wait_
// for_writable_until() (already proven bounded and never-hanging by
// tests/bounded_output_wait_test.cpp) - this atom only decides what
// that outcome MEANS for the connection, which is exactly the two
// cases below (the precondition on flush_write_wait_is_fatal()'s own
// header comment excludes bounded_wait_outcome::ready - the caller's
// retry loop already keeps flushing whenever it is).

using glintfx::platform::bounded_wait_outcome;
using glintfx::platform::flush_write_wait_is_fatal;

GLINTFX_TEST(flush_retry_policy_timeout_is_never_fatal) {
    // WL-WRITE-TIMEOUT-NAO-FATAL: nem pump_events() (antigo budget_ms
    // == 0) nem wait_events() (antigo budget_ms > 0) marcam a conexao
    // como morta so' porque o tempo de escrita esgotou - a mesma
    // clemencia que wait_for_incoming_data() (display_adapter.cpp) ja
    // dava ao lado leitura, agora simetrica dos dois lados.
    GLINTFX_CHECK(!flush_write_wait_is_fatal(bounded_wait_outcome::timed_out));
}

GLINTFX_TEST(flush_retry_policy_poll_failure_is_always_fatal) {
    // Perdoar um buffer que so' ainda nao drenou nunca perdoa um
    // socket genuinamente quebrado - bounded_output_wait.hpp's own
    // "conexao inutilizavel" verdict for poll_failed continua valendo
    // sem excecao, e' a UNICA coisa que ainda derruba a conexao por
    // este caminho.
    GLINTFX_CHECK(flush_write_wait_is_fatal(bounded_wait_outcome::poll_failed));
}
