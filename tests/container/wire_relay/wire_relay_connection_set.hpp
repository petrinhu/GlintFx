// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "wire_decoder.hpp"
#include "wire_relay_pipeline.hpp"

#include <cstddef>
#include <deque>
#include <memory>
#include <string>
#include <vector>

// wire_relay_connection_set.hpp - WL-ACK-SMOKE-BLUNT sub-fatia A3c
// (docs/plano-w7c.md SS3.A, arquitetura interna: "uma conexao a
// montante POR CLIENTE"). Ate A3b o rele servia UM cliente por vez -
// main()'s own antigo `while(true) { accept(); serve_one_client(); }`
// so aceitava a PROXIMA conexao depois que a sessao INTEIRA anterior
// fechava. Isso e uma lacuna do RELE, nao do produto: medido ao vivo
// (achado do team-lead, 24/09/2026) que gl_context_parity_test - que
// abre DUAS conexoes Wayland, a segunda ANTES de fechar a primeira -
// travava indefinidamente atras do rele antigo (poll_schedule_
// timeout, >10min), e passava limpo em segundos direto no KWin.
//
// Este atomo separa "uma sessao" (relay_session, promovida de dentro
// de wire_relay_main.cpp) de "o CONJUNTO de sessoes ativas
// simultaneas" (connection_list) - o motor de poll() multi-fd que
// main() passa a rodar, sem thread nenhuma: cada active_connection e
// so mais dois file descriptors no mesmo vetor de pollfd.
namespace glintfx::test::wire_relay {

// File descriptors recebidos (via read_once() de uma direcao) mas
// ainda nao associados a mensagem que os consome - ver wire_object_
// table.hpp's own fd_argument_count(). FIFO: SCM_RIGHTS nunca
// atravessa a fronteira da chamada sendmsg() que os carregou (provado
// pelo controle "descritor no pedaco certo" de A1), entao descritores
// sao sempre consumidos na mesma ordem em que chegaram.
class pending_fds {
  public:
    void push(const std::vector<int> &fds);
    [[nodiscard]] std::vector<int> take(std::size_t count);

  private:
    std::deque<int> m_queue;
};

struct connection_stats {
    std::size_t messages_from_client = 0;
    std::size_t messages_from_upstream = 0;
    std::size_t violations = 0;
};

struct direction_state {
    wire_decoder decoder;
    pending_fds fds;
};

// A sessao inteira de UM cliente aceito: a tabela de objetos/motor de
// regras compartilhados (ids de objeto sao globais a conexao) mais um
// estado de decodificacao por direcao (cada direcao enquadra as
// proprias mensagens de forma independente).
struct relay_session {
    wire_relay_pipeline pipe;
    direction_state client_state;
    direction_state upstream_state;
    connection_stats stats;
};

// Uma conexao ja aceita: os dois file descriptors crus (cliente e a
// conexao a montante ligada SO PRA ELA) mais a sessao de protocolo.
// `*_open` por direcao imita exatamente o client_open/upstream_open
// que o laco de uma-sessao-so ja tinha - a direcao continua sendo
// bombeada ate ela propria terminar (EOF, truncagem, ou violacao),
// nunca as duas juntas so porque uma acabou (D-A4: esvaziar antes de
// fechar).
struct active_connection {
    int client_fd = -1;
    int upstream_fd = -1;
    bool client_open = true;
    bool upstream_open = true;
    relay_session session;
};

using connection_list = std::vector<std::unique_ptr<active_connection>>;

// listen_fd e o caminho do montante agrupados (GODS_LAWS.md L-17,
// teto de 4 parametros) - todo ponto de entrada abaixo que precisa
// aceitar uma conexao nova recebe os dois juntos.
struct relay_endpoints {
    int listen_fd = -1;
    std::string upstream_path;
};

// Bombeia a direcao CLIENTE de uma conexao ja aceita (chamador ja
// filtrou os revents). Atualiza conn.client_open para false quando a
// direcao terminou - EOF, truncagem de controle no MSG_CTRUNC, ou uma
// violacao de regra que acabou de fechar os dois lados.
void pump_client_direction(active_connection &conn);

// O par acima, para o lado MONTANTE - transparente, sem regra, mas
// ainda alimenta o motor de regras (R2 precisa saber que serials o
// compositor de fato enviou).
void pump_upstream_direction(active_connection &conn);

// True quando as DUAS direcoes de `conn` ja terminaram - o sinal para
// o chamador fechar os fds, imprimir o veredito e remover `conn` de
// connection_list.
[[nodiscard]] bool connection_finished(const active_connection &conn);

// Fecha os dois fds de `conn` e imprime a MESMA linha "connection
// closed" que a versao de-um-cliente-por-vez ja imprimia (formato
// intocado: check_isolation.sh's own wait_for_relay_ready() e run_
// compositor.sh dependem dele).
void close_and_report(const active_connection &conn);

// Aceita, no maximo, UMA conexao nova em endpoints.listen_fd (o
// chamador so invoca isto quando poll() ja disse que o listen_fd
// esta pronto): conecta a montante NUMA CONEXAO PROPRIA para este
// cliente (D-A2, "uma conexao a montante por cliente") e acrescenta a
// nova active_connection a `connections`. accept()/connect() que
// falham sao reportados e ignorados (o proximo ciclo tenta de novo),
// nunca derrubam o processo inteiro.
void accept_new_connection(const relay_endpoints &endpoints, connection_list &connections);

// Roda UM CICLO do rele: monta a lista de pollfd (listen_fd mais toda
// direcao ainda aberta de toda conexao ativa), um UNICO poll()
// bloqueante, despacha tudo que ficou pronto (accept_new_connection()
// para o listen_fd, pump_*_direction() para cada conexao), e remove
// do conjunto as que connection_finished() ja considera encerradas.
// main() chama isto dentro de um `while(true)` que nunca retorna; um
// selftest pode chamar repetidamente, controlando a ordem exata dos
// eventos, sem thread nem subprocesso - e como A3c prova concorrencia
// sem depender de accept() de verdade estar pronto num timing
// especifico.
void run_relay_cycle(const relay_endpoints &endpoints, connection_list &connections);

} // namespace glintfx::test::wire_relay
