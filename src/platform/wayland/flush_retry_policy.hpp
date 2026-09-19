// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "platform/wayland/bounded_output_wait.hpp"

// platform/wayland/flush_retry_policy.hpp - LOOP-RUN fatia 7, conserto
// pos-revisao (docs/plano-w6b-fatias-6-8.md, D-W6b-57), REABERTO por
// WL-WRITE-TIMEOUT-NAO-FATAL (TODO.md; ordem do lider, 08/09/2026, via
// AskUserQuestion; GODS_LAWS.md L-17/L-20): the ONE pure atom display_
// adapter.cpp::flush_with_retry() calls to decide what a write-side
// wait that did NOT resolve `ready` means for the connection - the
// same "policy pulled out of the syscall-touching function into its
// own testable atom" shape this project already uses for frame_
// callback_sequence.hpp (present-or-skip a frame) and gl_version_
// policy.hpp (accept-or-reject a driver's answer). Neither wl_display
// nor a real socket is reachable from this header or its .cpp - only
// bounded_output_wait.hpp's own outcome enum, exactly what a caller
// already has in hand right after calling wait_for_writable_until().
//
// A DECISAO NOVA (WL-WRITE-TIMEOUT-NAO-FATAL): pesquisa externa citada
// na propria linha da TODO.md mediu que NENHUM cliente de referencia
// mata a conexao por tempo de escrita esgotado - o manual do libwayland
// manda usar poll() e ESPERAR, o GLFW espera SEM LIMITE por POLLOUT
// liberado, o SDL espera o orcamento e SEGUE em frente sem marcar nada;
// matar por buffer cheio e' pratica do lado SERVIDOR (proteger contra
// cliente malicioso/travado), nunca do lado cliente que este arquivo
// implementa. `budget_ms` SAIU da assinatura porque deixou de
// significar qualquer coisa para este veredito: o limiar que ele media
// (D-W6b-57's own verdict, "orcamento diferente de zero esgotado e'
// fatal") era o proprio DEFEITO que a nova ordem nomeia - um limiar
// ACIDENTAL, literalmente "quanto tempo o chamador escolheu dormir"
// (100ms a quase 1s conforme o teto de quadros), nunca uma decisao. O
// SEGUNDO defeito que esta mudanca conserta, junto do primeiro: a
// ASSIMETRIA com o lado leitura - wait_for_incoming_data()
// (display_adapter.cpp) ja perdoava o MESMO tipo de timeout
// incondicionalmente, sob qualquer orcamento, muito antes desta fatia;
// so' o lado escrita ainda distinguia orcamento zero de orcamento real.
// Os dois lados agora seguem a mesma regra.
//
// bounded_wait_outcome::timed_out NUNCA e' mais fatal, para nenhum
// orcamento: o buffer do kernel ainda cheio ao fim da espera prova
// apenas que ele ainda nao drenou agora, nunca que o compositor do
// outro lado morreu - o mesmo raciocinio que o read-side ja aplicava.
// O QUE SE PERDE, DECLARADO (a propria linha da TODO.md): deixamos de
// detectar, por tempo de escrita, um compositor vivo mas travado -
// deteccao de conexao morta continua saindo por poll_failed
// (POLLERR/POLLHUP/POLLNVAL reais), nunca por um relogio de espera de
// escrita.
//
// bounded_wait_outcome::poll_failed continua fatal SEMPRE, sem
// excecao: significa que o SOCKET em si quebrou (POLLERR/POLLHUP/
// POLLNVAL, ou poll() devolvendo erro real - bounded_output_wait.hpp's
// own header comment), categoria diferente de "o buffer so' ainda nao
// drenou", e nenhum orcamento perdoava isso antes nem perdoa agora.
//
// PRECONDITION (not checked - the same "caller already knows" contract
// wait_for_writable_until() itself documents for `fd`): call this ONLY
// with an `outcome` that is NOT bounded_wait_outcome::ready - the
// caller's own retry loop already keeps flushing whenever it is.

namespace glintfx::platform {

[[nodiscard]] bool flush_write_wait_is_fatal(bounded_wait_outcome outcome) noexcept;

} // namespace glintfx::platform
