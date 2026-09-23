// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "platform/wayland/incoming_poll_outcome.hpp"

// platform/wayland/incoming_poll_reaction.hpp - CONT-WARMUP C-6 (revisao
// adversarial C-5, /var/tmp/glintfx-plan/revisao-cont-warmup-c5.md,
// achado CRITICO-2, GODS_LAWS.md L-17 "gemeo"): the ONE pure atom both
// read-side call sites (display_adapter.cpp::wait_for_incoming_data(),
// egl_context_adapter.cpp::poll_and_dispatch_with_budget()) now call to
// decide what a classify_incoming_poll() outcome MEANS for the
// connection - the SAME "policy pulled out of the syscall-touching
// function into its own testable atom" shape this project already uses
// on the write side (flush_retry_policy.hpp, this same directory:
// flush_write_wait_is_fatal() decides what a bounded_wait_outcome means,
// consumed by flush_with_retry() without a wiring test of its own).
//
// WHY THIS EXISTS (the C-5 review's own finding, CRITICO-2): before this
// fatia, each of the two switches decided "is this really fatal?" with
// its OWN hand-written `return false`/`return true` (or ok/err) inside
// `case fatal:`/`case poll_call_failed:` - nothing directly tested THAT
// decision. A mutant that swapped those two returns (m-fatal-swallowed,
// the review's own name) survived every test that existed: the unit
// test only ever touched classify_incoming_poll() itself (unchanged by
// that mutation), and the four e2e fixtures that call swap_buffers()
// never reach those two cases live (this header's own note below on
// WHY). Centralizing the decision here means the ONLY way to reintroduce
// that exact bug is to either (a) edit THIS file, which THIS file's own
// direct unit test (tests/incoming_poll_reaction_test.cpp) now catches,
// or (b) invert the call at a call site - a residual, DECLARED
// limitation, not silently accepted: see this header's own note below
// and egl_context_adapter.cpp's own comment on poll_and_dispatch_with_
// budget() for the honest accounting of what is and is not proven.
//
// No `wl_display`, no `int fd`, no real `::poll()` call reachable from
// this header or its .cpp - only the plain outcome a caller's own
// classify_incoming_poll() already produced, plus ONE boolean the
// caller reads from `errno` right after its own `::poll()` returns
// (this project's convention, unchanged by this fatia: WHICH errno it
// was is real OS state this atom never reads itself - incoming_poll_
// outcome.hpp's own comment on poll_call_failed already draws this
// line, and this atom draws it the same way one level up).
// `errno_is_eintr` only matters when `outcome ==
// incoming_poll_outcome::poll_call_failed` - a caller passes whatever it
// likes for the other three outcomes (this atom never reads it for
// them), but always passing the real value is simplest and is what both
// call sites do.
//
// PROVA POR SEAM PARA OS DOIS ESTADOS QUE O KERNEL NAO PRODUZ DE FORMA
// CONFIAVEL (tests/incoming_poll_reaction_test.cpp's own header
// comment): `poll_call_failed` com `errno` diferente de `EINTR`, e o
// proprio `EINTR`, nunca sao passados por um `::poll()` real de teste -
// esta funcao ja os recebe como VALOR PURO (`errno_is_eintr`), entao
// "injetar" e' so' passar o parametro certo, sem mecanismo de injecao
// separado (nada entra na API PUBLICA da biblioteca - L-19 - e nenhuma
// excecao cruza nada - L-22, esta funcao nem lanca).
namespace glintfx::platform {

[[nodiscard]] bool is_incoming_poll_connection_fatal(incoming_poll_outcome outcome,
                                                     bool errno_is_eintr) noexcept;

// incoming_poll_reaction_fn - CONT-WARMUP C-8 (revisao-cont-warmup-c7.md
// S2.4/S2.5, GODS_LAWS.md L-17 "gemeo"): a MESMA costura que incoming_
// poll_syscall.hpp ja da a `::poll()` em si (um ponteiro de funcao com o
// atomo real como padrao), agora um nivel acima, para a DECISAO que um
// caller toma com o resultado do poll(). Sem esta costura, o mutante que
// reinsere o atalho historico `if (errno_is_eintr) { return true; }`
// direto no call site e' EQUIVALENTE ao atomo de hoje para o par
// (poll_call_failed, errno_is_eintr==true) - as duas respostas coincidem
// por valor, entao nenhum teste de caixa-preta contra o atomo real
// alcanca a diferenca. Com a decisao por tras de um parametro, um teste
// pode injetar um atomo DIVERGENTE (que discorda do real por proposito)
// e provar que o call site de fato CONSULTA o parametro, em vez de
// decidir sozinho com um atalho hardcoded - a unica forma de o mutante
// sobreviver a ESSE teste seria ignorar o parametro por completo, o que
// e' exatamente o defeito que ele existe para detectar.
using incoming_poll_reaction_fn = bool (*)(incoming_poll_outcome, bool) noexcept;

} // namespace glintfx::platform
