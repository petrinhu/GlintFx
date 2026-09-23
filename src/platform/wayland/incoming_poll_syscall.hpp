// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <poll.h>

// platform/wayland/incoming_poll_syscall.hpp - CONT-WARMUP C-6, EMENDA
// (ordem do team-lead sobre o residual declarado da primeira rodada de
// C-6: "a ordem do lider e' o caminho mais completo, nao o mais
// simples" - testar a funcao REAL, nao so' o atomo extraido dela).
//
// A ÚNICA razão deste arquivo existir: `wait_for_incoming_data()`
// (display_adapter.cpp) e `poll_and_dispatch_with_budget()` (egl_
// context_adapter.cpp) chamavam `::poll()` DIRETO, sem nenhum ponto de
// substituição - por isso um teste só conseguia exercitar o átomo que
// decide o que FAZER com o resultado (incoming_poll_reaction.hpp),
// nunca a fiação real em volta dele (o `switch` inteiro, os `case`
// que chamam `wl_display_cancel_read()`/`m_fatal = true`/o `continue`
// de retry). O mutante "o ponto de uso inverte ou ignora o resultado"
// não tinha como ser pego nesse nível - exatamente o CRÍTICO-2 que a
// revisão da C-5 cobrou, e que a primeira rodada da C-6 só fechou pela
// metade (o átomo, não a fiação).
//
// A costura é a ASSINATURA da própria chamada: um parâmetro do MESMO
// tipo de `::poll()`, com `&::poll` como padrão - GODS_LAWS.md L-19
// ("nada na API pública") e L-22 (nenhuma exceção): nada aqui entra em
// `include/glintfx/`, e a indireção em si não lança. Custo no caminho
// normal: uma chamada por ponteiro de função em vez de uma chamada
// direta, no MESMO lugar que já fazia uma chamada de sistema - não é
// caminho quente de desenho (janela/entrada, não render por quadro),
// e o valor por padrão É a chamada real, então nada muda em produção
// além dessa indireção.
namespace glintfx::platform {

// Exatamente a assinatura de `::poll()` (POSIX). `wait_for_incoming_
// data()`/`poll_and_dispatch_with_budget()` chamam ESTE tipo em vez de
// `::poll()` diretamente; um teste no mesmo build interno (nunca a API
// pública) pode substituir por uma função script que devolve qualquer
// desfecho sem depender do kernel.
using incoming_poll_syscall_fn = int (*)(pollfd *, nfds_t, int);

} // namespace glintfx::platform
