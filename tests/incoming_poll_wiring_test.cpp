// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cerrno>
#include <cstddef>
#include <vector>

#include <sys/socket.h>
#include <unistd.h>

#include <wayland-client.h>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/wayland/display_adapter.hpp"
#include "platform/wayland/egl_incoming_poll_step.hpp"
#include "platform/wayland/incoming_poll_reaction.hpp"
#include "platform/wayland/incoming_poll_syscall.hpp"

// incoming_poll_wiring_test.cpp - CONT-WARMUP C-6, EMENDA (ordem do
// team-lead sobre o residual declarado da primeira rodada de C-6:
// GODS_LAWS.md L-12/L-17/L-20/L-27, "a ordem do lider e' o caminho mais
// completo, nao o mais simples"). tests/incoming_poll_reaction_test.cpp
// já prova o ÁTOMO (is_incoming_poll_connection_fatal()) direto - este
// arquivo prova as DUAS FUNÇÕES REAIS que o consomem
// (wayland_display_adapter::wait_for_incoming_data(),
// glintfx::platform::poll_and_dispatch_with_budget()), chamando-as de
// verdade, com um `::poll()` FABRICADO (a costura `poll_impl`,
// src/platform/wayland/incoming_poll_syscall.hpp) devolvendo cada
// desfecho sem depender do kernel nem de um compositor.
//
// NENHUM wl_display FALSO: `wl_display_connect_to_fd()` (API pública do
// libwayland-client, a mesma técnica que este projeto já usa para
// outros fixtures sem compositor) sobre um socketpair() real dá às duas
// funções um `wl_display*` genuinamente válido - `wl_display_prepare_
// read()`/`wl_display_cancel_read()`/`wl_display_get_error()` (as
// ÚNICAS chamadas reais que os ramos `fatal`/`poll_call_failed`/
// `nothing_yet` fazem) operam sobre estado LOCAL da biblioteca, sem
// nenhum I/O de rede - nunca precisam de um par respondendo do outro
// lado.
//
// RISCO REAL, NOMEADO, E A DECISÃO DE DESENHO QUE ELE FORÇA: fingir
// `POLLIN`/`POLLHUP` via `poll_impl` SEM que o socket real tenha dado
// nenhum é perigoso PARA O RAMO `ready_to_read` do lado EGL
// (poll_and_dispatch_with_budget() chama wl_display_read_events() DE
// VERDADE logo em seguida) - se o poll() fabricado mentir que há dado
// quando não há, essa leitura real pode BLOQUEAR o processo de teste
// inteiro (o socket da conexão Wayland é bloqueante por desenho: o
// padrão prepare_read/poll/read_events existe exatamente para nunca
// chamar read() sem antes confirmar, via poll() de verdade, que há
// dado). Por isso:
//   - Os quatro desfechos que NUNCA levam a uma leitura real depois
//     (poll_call_failed não-EINTR, poll_call_failed EINTR seguido de
//     outro desfecho seguro, fatal/POLLNVAL, nothing_yet/timeout) são
//     testados por INJEÇÃO PURA nas DUAS funções - é exatamente aqui
//     que vivia o mutante `m-fatal-swallowed` da revisão C-5.
//   - `ready_to_read` do lado `wait_for_incoming_data()` (display) É
//     seguro por injeção pura: essa função nunca chama uma leitura real
//     por si mesma, só devolve `ok(true)` - o chamador (dispatch_ready_
//     events(), fora do escopo deste teste) é quem decide se lê depois.
//     Testado por injeção, incluindo o caso POLLHUP sem POLLIN que o
//     kernel real nunca entrega sozinho (medido, quatro vezes, pela
//     C-2; incoming_poll_outcome.hpp's own header comment).
//   - `ready_to_read` do lado EGL É testado, mas com DADO REAL no
//     socket (não por injeção fabricada) - exatamente para não correr
//     o risco de travar acima. Prova o MESMO roteamento (não é tratado
//     como falha), só não isola o caso sintético POLLHUP-sem-POLLIN
//     especificamente para este adaptador: residual DECLARADO, pela
//     mesma razão de segurança, não por preguiça - ver o caso
//     `poll_and_dispatch_with_budget_ready_to_read_with_real_data_is_
//     not_fatal` abaixo.

namespace {

// Um passo do script: `poll_result < 0` significa "::poll() falhou",
// com `errno_value` sendo o que a função sob teste deve ler logo
// depois (a MESMA convenção que os dois adaptadores reais seguem -
// ler errno ANTES de qualquer outra chamada). `poll_result >= 0`
// significa sucesso, com `revents` sendo o que o kernel teria deixado
// no `pollfd`.
struct scripted_poll_step {
    int poll_result = 0;
    int errno_value = 0;
    short revents = 0;
};

std::vector<scripted_poll_step> g_script;
std::size_t g_script_index = 0;
int g_script_calls_made = 0;

void reset_script(std::vector<scripted_poll_step> steps) {
    g_script = std::move(steps);
    g_script_index = 0;
    g_script_calls_made = 0;
}

// A costura em si (incoming_poll_syscall_fn): assinatura idêntica a
// `::poll()`. NUNCA lança (contrato do tipo, que não é `noexcept` só
// porque `::poll()` em si também não é declarado `noexcept` em C) -
// mas esta implementação em si não lança de fato, então marcá-la
// `noexcept` é seguro e converte implicitamente para o tipo sem
// `noexcept` que a costura exige.
int scripted_poll(pollfd *fds, nfds_t /*nfds*/, int /*timeout_ms*/) noexcept {
    ++g_script_calls_made;
    if (g_script_index >= g_script.size()) {
        // Script esgotado sem que o teste tivesse previsto mais uma
        // chamada - devolve "nada a ler" (o desfecho mais inofensivo)
        // em vez de ler memória fora dos limites; o teste confere
        // `g_script_calls_made` contra o que esperava, então este
        // ramo vira uma asserção que falha por contagem errada, nunca
        // um crash silencioso.
        fds[0].revents = 0;
        return 0;
    }
    const scripted_poll_step &step = g_script[g_script_index];
    ++g_script_index;
    if (step.poll_result < 0) {
        errno = step.errno_value;
        fds[0].revents = 0;
        return -1;
    }
    fds[0].revents = step.revents;
    return step.poll_result;
}

// CONT-WARMUP C-8 (revisao-cont-warmup-c7.md S2.4/S2.5): um atomo que
// DISCORDA do real (is_incoming_poll_connection_fatal(), incoming_poll_
// reaction.hpp) de proposito, so' para o par (poll_call_failed,
// errno_is_eintr==true) - o real devolve "nao fatal" para esse par; este
// devolve "fatal". Serve so' para provar que poll_and_dispatch_with_
// budget() de fato CONSULTA o parametro reaction_impl em vez de decidir
// sozinho com um atalho hardcoded - o atalho historico
// (`if (errno_is_eintr) { return true; }`) e o atomo real CONCORDAM por
// coincidencia nesse par, entao so' um atomo que DISCORDA consegue
// distinguir "consultou o parametro" de "ignorou e decidiu por conta
// propria" (ver o comentario de poll_and_dispatch_with_budget() em egl_
// context_adapter.cpp).
bool divergent_eintr_exhausted_is_fatal(glintfx::platform::incoming_poll_outcome outcome,
                                        bool errno_is_eintr) noexcept {
    if (outcome == glintfx::platform::incoming_poll_outcome::poll_call_failed && errno_is_eintr) {
        return true;
    }
    return glintfx::platform::is_incoming_poll_connection_fatal(outcome, errno_is_eintr);
}

// Um `wl_display*` genuinamente válido, sem compositor: socketpair()
// real, `wl_display_connect_to_fd()` (API pública do libwayland-
// client) sobre a ponta [0]. A ponta [1] (par) fica aberta e SEM
// tráfego - as funções sob teste nunca tentam ler/escrever de
// verdade nos casos exercitados aqui (só nos dois casos "ready_to_
// read com dado real" abaixo, que escrevem nela de propósito).
struct fake_display {
    int peer_fd = -1;
    wl_display *display = nullptr;

    fake_display() {
        int fds[2] = {-1, -1};
        if (::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0) {
            display = wl_display_connect_to_fd(fds[0]);
            peer_fd = fds[1];
        }
    }

    fake_display(const fake_display &) = delete;
    fake_display &operator=(const fake_display &) = delete;

    ~fake_display() {
        // `display` NÃO é desconectado aqui: display_adapter_wiring_
        // access::attach() entrega o ponteiro para dentro de um
        // wayland_display_adapter de teste, cujo PRÓPRIO destrutor
        // (close()) já chama wl_display_disconnect() - desconectar
        // duas vezes seria um duplo-free do lado display. O lado EGL
        // (poll_and_dispatch_with_budget) não é dono de nada: quem
        // chama este construtor por conta própria PARA O LADO EGL
        // precisa desconectar explicitamente - ver egl_fake_display
        // abaixo.
        if (peer_fd != -1) {
            ::close(peer_fd);
        }
    }
};

// Variante para os casos do lado EGL, que NUNCA passam por um
// wayland_display_adapter (poll_and_dispatch_with_budget() e' uma
// função livre sobre um `wl_display*` nu) - este RAII é quem
// desconecta de verdade.
struct egl_fake_display {
    int peer_fd = -1;
    wl_display *display = nullptr;

    egl_fake_display() {
        int fds[2] = {-1, -1};
        if (::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0) {
            display = wl_display_connect_to_fd(fds[0]);
            peer_fd = fds[1];
        }
    }

    egl_fake_display(const egl_fake_display &) = delete;
    egl_fake_display &operator=(const egl_fake_display &) = delete;

    ~egl_fake_display() {
        if (display != nullptr) {
            wl_display_disconnect(display);
        }
        if (peer_fd != -1) {
            ::close(peer_fd);
        }
    }
};

} // namespace

namespace glintfx::platform {

// A ÚNICA razão deste struct existir (display_adapter.hpp's own
// `friend` declaration): wait_for_incoming_data() e drain_pending_and_
// prepare_read() são privados por desenho (nenhum consumidor externo
// deveria chamá-los fora da sequência de quatro passos que dispatch_
// ready_events() garante) - este acesso de teste é estreito de
// propósito, só os três métodos que este arquivo precisa, nunca um
// `friend` genérico.
struct incoming_poll_wiring_test_access {
    static void attach(wayland_display_adapter &adapter, wl_display *display) {
        adapter.m_display = display;
    }

    [[nodiscard]] static gltfx_rslt<void> prepare_read(wayland_display_adapter &adapter) {
        return adapter.drain_pending_and_prepare_read();
    }

    [[nodiscard]] static gltfx_rslt<bool>
    wait_for_incoming_data(wayland_display_adapter &adapter, std::uint32_t timeout_ms,
                           incoming_poll_syscall_fn poll_impl) {
        return adapter.wait_for_incoming_data(timeout_ms, poll_impl);
    }
};

} // namespace glintfx::platform

using glintfx::platform::incoming_poll_wiring_test_access;
using glintfx::platform::poll_and_dispatch_with_budget;
using glintfx::platform::wayland_display_adapter;

// --- wait_for_incoming_data() (display_adapter.cpp), injecao pura ----

GLINTFX_TEST(wait_for_incoming_data_poll_call_failed_non_eintr_is_fatal) {
    const fake_display fd;
    GLINTFX_CHECK(fd.display != nullptr);
    wayland_display_adapter adapter;
    incoming_poll_wiring_test_access::attach(adapter, fd.display);
    GLINTFX_CHECK(!incoming_poll_wiring_test_access::prepare_read(adapter).has_error());

    reset_script({{.poll_result = -1, .errno_value = EIO}});
    const auto result =
        incoming_poll_wiring_test_access::wait_for_incoming_data(adapter, 0, &scripted_poll);

    GLINTFX_CHECK(g_script_calls_made == 1);
    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK(adapter.has_fatal_error());
}

GLINTFX_TEST(wait_for_incoming_data_poll_call_failed_eintr_is_not_fatal) {
    const fake_display fd;
    GLINTFX_CHECK(fd.display != nullptr);
    wayland_display_adapter adapter;
    incoming_poll_wiring_test_access::attach(adapter, fd.display);
    GLINTFX_CHECK(!incoming_poll_wiring_test_access::prepare_read(adapter).has_error());

    reset_script({{.poll_result = -1, .errno_value = EINTR}});
    const auto result =
        incoming_poll_wiring_test_access::wait_for_incoming_data(adapter, 0, &scripted_poll);

    GLINTFX_CHECK(g_script_calls_made == 1);
    GLINTFX_CHECK(!result.has_error());
    GLINTFX_CHECK(result.value() == false);
    GLINTFX_CHECK(!adapter.has_fatal_error());
}

GLINTFX_TEST(wait_for_incoming_data_pollnval_is_fatal) {
    const fake_display fd;
    GLINTFX_CHECK(fd.display != nullptr);
    wayland_display_adapter adapter;
    incoming_poll_wiring_test_access::attach(adapter, fd.display);
    GLINTFX_CHECK(!incoming_poll_wiring_test_access::prepare_read(adapter).has_error());

    reset_script({{.poll_result = 1, .revents = POLLNVAL}});
    const auto result =
        incoming_poll_wiring_test_access::wait_for_incoming_data(adapter, 0, &scripted_poll);

    GLINTFX_CHECK(g_script_calls_made == 1);
    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK(adapter.has_fatal_error());
}

GLINTFX_TEST(wait_for_incoming_data_pollhup_without_pollin_is_ready_not_fatal) {
    // O caso que o kernel real nunca entrega sozinho (medido 4x pela
    // C-2) - só alcançável por injeção. wait_for_incoming_data() nunca
    // chama uma leitura real neste ramo (só devolve ok(true)), então é
    // seguro testar mesmo sem dado real no socket.
    const fake_display fd;
    GLINTFX_CHECK(fd.display != nullptr);
    wayland_display_adapter adapter;
    incoming_poll_wiring_test_access::attach(adapter, fd.display);
    GLINTFX_CHECK(!incoming_poll_wiring_test_access::prepare_read(adapter).has_error());

    reset_script({{.poll_result = 1, .revents = POLLHUP}});
    const auto result =
        incoming_poll_wiring_test_access::wait_for_incoming_data(adapter, 0, &scripted_poll);

    GLINTFX_CHECK(g_script_calls_made == 1);
    GLINTFX_CHECK(!result.has_error());
    GLINTFX_CHECK(result.value() == true);
    GLINTFX_CHECK(!adapter.has_fatal_error());
}

GLINTFX_TEST(wait_for_incoming_data_timeout_is_not_fatal) {
    const fake_display fd;
    GLINTFX_CHECK(fd.display != nullptr);
    wayland_display_adapter adapter;
    incoming_poll_wiring_test_access::attach(adapter, fd.display);
    GLINTFX_CHECK(!incoming_poll_wiring_test_access::prepare_read(adapter).has_error());

    reset_script({{.poll_result = 0}});
    const auto result =
        incoming_poll_wiring_test_access::wait_for_incoming_data(adapter, 0, &scripted_poll);

    GLINTFX_CHECK(g_script_calls_made == 1);
    GLINTFX_CHECK(!result.has_error());
    GLINTFX_CHECK(result.value() == false);
    GLINTFX_CHECK(!adapter.has_fatal_error());
}

// --- poll_and_dispatch_with_budget() (egl_context_adapter.cpp) -------

GLINTFX_TEST(poll_and_dispatch_with_budget_poll_call_failed_non_eintr_returns_false) {
    const egl_fake_display fd;
    GLINTFX_CHECK(fd.display != nullptr);

    reset_script({{.poll_result = -1, .errno_value = EIO}});
    const bool result = poll_and_dispatch_with_budget(fd.display, 0, &scripted_poll);

    GLINTFX_CHECK(g_script_calls_made == 1);
    GLINTFX_CHECK(result == false);
}

GLINTFX_TEST(poll_and_dispatch_with_budget_pollnval_returns_false) {
    const egl_fake_display fd;
    GLINTFX_CHECK(fd.display != nullptr);

    reset_script({{.poll_result = 1, .revents = POLLNVAL}});
    const bool result = poll_and_dispatch_with_budget(fd.display, 0, &scripted_poll);

    GLINTFX_CHECK(g_script_calls_made == 1);
    GLINTFX_CHECK(result == false);
}

GLINTFX_TEST(poll_and_dispatch_with_budget_timeout_returns_true) {
    const egl_fake_display fd;
    GLINTFX_CHECK(fd.display != nullptr);

    reset_script({{.poll_result = 0}});
    const bool result = poll_and_dispatch_with_budget(fd.display, 0, &scripted_poll);

    GLINTFX_CHECK(g_script_calls_made == 1);
    GLINTFX_CHECK(result == true);
}

GLINTFX_TEST(poll_and_dispatch_with_budget_eintr_retries_within_budget_then_times_out) {
    // Prova o `continue` de retry: a PRIMEIRA chamada devolve EINTR, a
    // SEGUNDA (ainda dentro do orcamento, generoso o bastante para nao
    // esgotar entre as duas chamadas) devolve timeout - duas chamadas
    // consumidas do script, resultado final "nada a ler", nunca falha.
    const egl_fake_display fd;
    GLINTFX_CHECK(fd.display != nullptr);

    reset_script({
        {.poll_result = -1, .errno_value = EINTR},
        {.poll_result = 0},
    });
    constexpr std::uint32_t k_generous_budget_ms = 5000;
    const bool result =
        poll_and_dispatch_with_budget(fd.display, k_generous_budget_ms, &scripted_poll);

    GLINTFX_CHECK(g_script_calls_made == 2);
    GLINTFX_CHECK(result == true);
}

GLINTFX_TEST(poll_and_dispatch_with_budget_eintr_with_budget_exhausted_is_not_fatal) {
    // CONT-WARMUP C-7 (revisao-cont-warmup-c6.md, achado m-eintr-
    // budget-bypass): antes desta fatia, o ramo "EINTR com o orcamento
    // ja esgotado" tinha um `return true;` HARDCODED em egl_context_
    // adapter.cpp, sem NUNCA passar por is_incoming_poll_connection_
    // fatal() - o UNICO dos tres caminhos deste `case` que nao ia pelo
    // atomo. Nenhum teste alcancava a combinacao exata que aciona esse
    // ramo (errno_is_eintr==true E remaining_ms<=0 no MESMO poll()) -
    // orcamento ZERO garante isso (remaining_ms nunca fica > 0, entao o
    // `continue` de retry nunca acontece, e um UNICO passo de script
    // basta). O conserto desta fatia REMOVEU o `return true;` hardcoded
    // - agora este ramo tambem chama is_incoming_poll_connection_
    // fatal(), a MESMA linha que os outros dois desfechos deste `case`
    // ja chamavam (ver o comentario no proprio call site) - nao ha mais
    // como reintroduzir o bug original sem tambem quebrar os testes
    // vizinhos (poll_call_failed_non_eintr_returns_false/pollnval_
    // returns_false), porque os tres desfechos agora compartilham o
    // MESMO `return`.
    const egl_fake_display fd;
    GLINTFX_CHECK(fd.display != nullptr);

    reset_script({{.poll_result = -1, .errno_value = EINTR}});
    const bool result = poll_and_dispatch_with_budget(fd.display, 0, &scripted_poll);

    GLINTFX_CHECK(g_script_calls_made == 1);
    GLINTFX_CHECK(result == true);
}

GLINTFX_TEST(poll_and_dispatch_with_budget_honors_injected_reaction_atom) {
    // CONT-WARMUP C-8 (revisao-cont-warmup-c7.md S2.4/S2.5, GODS_LAWS.md
    // L-17 "gemeo"): o mutante que reinsere o atalho historico
    // `if (errno_is_eintr) { return true; }` logo apos o cancel_read
    // SOBREVIVE ao teste vizinho (..._eintr_with_budget_exhausted_is_
    // not_fatal, acima), porque o atomo de producao concorda com o
    // atalho para ESSE par (poll_call_failed, EINTR) - mutante
    // EQUIVALENTE por coincidencia de valor, nao por ausencia de bug.
    // Este teste injeta um atomo que DISCORDA do real para o MESMO par
    // (divergent_eintr_exhausted_is_fatal, acima): se a fiacao de fato
    // consulta `reaction_impl` (em vez de decidir sozinha com o atalho
    // hardcoded), o resultado segue o atomo INJETADO (`false`, fatal) -
    // nunca o `true` que tanto o atalho quanto o atomo real
    // concordariam. Um atalho hardcoded reinserido no call site faria
    // este teste voltar a `true` e reprovar.
    const egl_fake_display fd;
    GLINTFX_CHECK(fd.display != nullptr);

    reset_script({{.poll_result = -1, .errno_value = EINTR}});
    const bool result = poll_and_dispatch_with_budget(fd.display, 0, &scripted_poll,
                                                      &divergent_eintr_exhausted_is_fatal);

    GLINTFX_CHECK(g_script_calls_made == 1);
    GLINTFX_CHECK(result == false);
}

GLINTFX_TEST(poll_and_dispatch_with_budget_ready_to_read_with_closed_peer_is_fatal) {
    // CONT-WARMUP C-7 (revisao-cont-warmup-c6.md, achado m-ready-
    // ignore-real-errors): o UNICO teste anterior deste `case ready_to_
    // read:` (..._with_real_data_is_not_fatal, abaixo) escrevia um byte
    // de verdade no par do socketpair - suficiente para provar que o
    // roteamento NAO trata "ha dado" como falha, mas NUNCA suficiente
    // para fazer wl_display_read_events()/wl_display_dispatch_pending()
    // falharem de verdade (a mensagem fica incompleta, sem erro
    // imediato) - uma mutacao que ignorasse o valor de retorno das duas
    // chamadas e sempre devolvesse `true` sobrevivia. Este teste fecha
    // fechando o PAR do socketpair SEM escrever NENHUM byte antes -
    // sondado ao vivo (GODS_LAWS.md L-44, nunca assumido): o kernel
    // real entrega POLLIN|POLLHUP para o lado do display (a leitura vai
    // devolver EOF), classify_incoming_poll() roteia para ready_to_read
    // (poll(2): "um hangup pode ter dado real antes do EOF" - aqui nao
    // ha dado nenhum, so' o EOF em si), e wl_display_read_events()
    // encontra um `read()` que devolve 0 - a biblioteca trata isso como
    // uma falha REAL de conexao (EPIPE) e devolve -1, o `return false;`
    // deste `case` que nenhum teste alcancava antes.
    egl_fake_display fd;
    GLINTFX_CHECK(fd.display != nullptr);
    GLINTFX_CHECK(fd.peer_fd != -1);
    ::close(fd.peer_fd);
    fd.peer_fd = -1; // evita fechar duas vezes no destrutor de egl_fake_display

    const bool result = poll_and_dispatch_with_budget(fd.display, 0);

    GLINTFX_CHECK(result == false);
}

GLINTFX_TEST(poll_and_dispatch_with_budget_default_poll_impl_reads_real_data) {
    // CONT-WARMUP C-7 (revisao-cont-warmup-c6.md, achado m-default-
    // poll-swap): uma mutacao que troca o `poll_impl` PADRAO de
    // producao (`&::poll`, egl_incoming_poll_step.hpp) por um stub que
    // nunca reporta dado disponivel sobrevivia ao teste antigo (..._
    // ready_to_read_with_real_data_is_not_fatal, abaixo): `nothing_yet`
    // e o sucesso de `ready_to_read` devolvem `true` os DOIS, entao a
    // asserção `result == true` nao distinguia "leu o dado real" de
    // "nunca tentou ler". Este teste confere um SEGUNDO sinal, fora da
    // funcao sob teste: depois da chamada com o poll_impl PADRAO
    // (omitido, exatamente como o unico chamador de producao,
    // swap_buffers(), sempre usa), o descritor cru do display
    // (wl_display_get_fd) nao pode ter mais NENHUM byte pendente no
    // buffer de leitura do kernel - sondado ao vivo (GODS_LAWS.md
    // L-44): isso so' acontece se wl_display_read_events() de fato leu
    // de verdade, o que so' acontece se o `::poll()` REAL reportou
    // POLLIN de verdade. Com o poll_impl PADRAO trocado por um stub
    // (a mutacao), wl_display_read_events() nunca seria chamada e o
    // byte ficaria pendente - `recv(MSG_PEEK)` devolveria o byte, nunca
    // EAGAIN.
    const egl_fake_display fd;
    GLINTFX_CHECK(fd.display != nullptr);
    const char byte = 'x';
    GLINTFX_CHECK(::write(fd.peer_fd, &byte, 1) == 1);

    const bool result = poll_and_dispatch_with_budget(fd.display, 0);
    GLINTFX_CHECK(result == true);

    const int display_fd = wl_display_get_fd(fd.display);
    char probe = 0;
    const ssize_t peeked = ::recv(display_fd, &probe, 1, MSG_DONTWAIT | MSG_PEEK);
    // Nenhum dado deveria sobrar no buffer do kernel: -1/EAGAIN e' a
    // UNICA leitura que prova que wl_display_read_events() de fato
    // consumiu o byte que este teste escreveu, via o `::poll()` REAL.
    GLINTFX_CHECK(peeked == -1);
    GLINTFX_CHECK(errno == EAGAIN || errno == EWOULDBLOCK);
}

GLINTFX_TEST(poll_and_dispatch_with_budget_ready_to_read_with_real_data_is_not_fatal) {
    // NAO por injecao pura (ver o cabecalho deste arquivo: fingir
    // POLLIN/POLLHUP sem dado real arrisca travar wl_display_read_
    // events()) - o `poll()` REAL (padrao, poll_impl omitido) confere
    // um byte de verdade escrito no par do socketpair. O byte nao e'
    // protocolo Wayland valido, mas isso nunca importa para ESTE teste:
    // o que se prova e' que o roteamento NAO trata como falha - nunca
    // que o dado em si e' bem formado.
    const egl_fake_display fd;
    GLINTFX_CHECK(fd.display != nullptr);
    const char byte = 'x';
    GLINTFX_CHECK(::write(fd.peer_fd, &byte, 1) == 1);

    const bool result = poll_and_dispatch_with_budget(fd.display, 0);

    GLINTFX_CHECK(result == true);
}
