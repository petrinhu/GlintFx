// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <chrono>
#include <cstdint>

// platform/wayland/bounded_output_wait.hpp - INBOX (drenagem 06/09/2026):
// "a promessa publica de que o desenho e o bombeamento de eventos nunca
// prendem o aplicativo... esta furada em pelo menos um ponto ainda
// vivo" - display_adapter.cpp::flush_with_retry() carregava exatamente
// o poll(&pending_write, 1, -1) (espera infinita por POLLOUT) que
// 72754af ja tinha consertado do lado de egl_context_adapter.cpp's own
// poll_and_dispatch_with_budget() - o MESMO defeito, sobrevivendo aqui
// porque aquele conserto auditou o ponto conhecido em vez de enumerar
// o espaco inteiro (GODS_LAWS.md L-17; a enumeracao completa, feita
// depois, esta em tests/wait_points.txt).
//
// ESTE ARQUIVO E O ATOMO QUE OS DOIS SITIOS AGORA COMPARTILHAM, em vez
// de manterem duas copias da mesma logica de "orcamento total, poll()
// bounded ao que resta" (a duplicacao e' exatamente o que deixou o
// gemeo vivo por horas da primeira vez). Recebe um `int fd` NU - nunca
// um `wl_display *` - e por isso nunca inclui <wayland-client.h> nem
// <EGL/egl.h>, a MESMA razao pela qual frame_callback_sequence.hpp
// (este mesmo diretorio) nao conhece o tipo concreto por cima dele.
// Isto o torna testavel com um socketpair() de verdade (tests/bounded_
// output_wait_test.cpp) sem compositor e sem container: o mecanismo do
// kernel que faz POLLOUT nunca chegar (buffer de envio cheio, ninguem
// do outro lado le) e' IDENTICO ao de uma conexao wl_display real,
// porque as duas sao o MESMO tipo de socket (AF_UNIX SOCK_STREAM) - a
// wl_display nunca entra neste arquivo, so' o fd que ela ja' expoe via
// wl_display_get_fd() (lido pelo lado real, nunca por este atomo).
//
// ORCAMENTO TOTAL, NUNCA POR TENTATIVA (a MESMA regra que 72754af ja'
// aplicou ao lado de apresentar): o relogio comeca a contar UMA VEZ, na
// entrada (o `deadline` e' calculado pelo CHAMADOR, nunca aqui dentro),
// e cada poll() interno espera so' o que resta ate ele - um par que
// libera um byte de cada vez no buffer de envio nao pode rearmar um
// numero ilimitado de esperas do tamanho do orcamento inteiro.

namespace glintfx::platform {

enum class bounded_wait_outcome : std::uint8_t {
    ready,       // POLLOUT chegou antes do prazo
    timed_out,   // o prazo venceu sem POLLOUT
    poll_failed, // poll() devolveu erro real, ou o par sinalizou
                 // POLLERR/POLLHUP/POLLNVAL - conexao inutilizavel,
                 // nunca tratado como "espera mais um pouco"
};

// `fd` PRECISA continuar valido durante a chamada inteira (o mesmo
// contrato de qualquer wrapper fino de poll() deste projeto - o
// chamador e' quem possui o descritor). EINTR nunca vira poll_failed:
// e' o unico retorno de erro que este atomo trata como "tenta de novo
// com o tempo que sobrou", nunca como falha real (a mesma regra que
// todo outro poll() deste projeto ja segue implicitamente por nunca
// ter sido observado sob sinal - aqui e' explicita porque o laco
// agora pode rodar por ate `deadline - agora`, tempo suficiente para
// um sinal chegar no meio).
[[nodiscard]] bounded_wait_outcome
wait_for_writable_until(int fd, std::chrono::steady_clock::time_point deadline) noexcept;

} // namespace glintfx::platform
