// SPDX-License-Identifier: AGPL-3.0-or-later
//
// wire_relay_a3c_selftest.cpp - autoteste de WL-ACK-SMOKE-BLUNT, sub-
// fatia A3c: o rele serve clientes CONCORRENTES (docs/plano-w7c.md
// SS3.A, "uma conexao a montante POR CLIENTE" - achado do team-lead,
// 24/09/2026, que gl_context_parity_test, abrindo uma segunda conexao
// Wayland antes de fechar a primeira, travava atras do rele antigo,
// de-um-cliente-por-vez, e passava limpo direto no KWin).
//
// GODS_LAWS.md L-09: diferente de A1/A2 (socketpair puro), este caso
// precisa de accept() de verdade - e o proprio comportamento que muda
// nesta fatia. Usa dois soquetes AF_UNIX reais escutando em caminhos
// temporarios proprios (nunca wayland-0, nunca a sessao do lider,
// nunca compositor real): um "downstream" (onde os clientes de teste
// conectam, como as fixtures fariam no rele de verdade) e um
// "montante" falso que NUNCA chama accept() de proposito - um soquete
// AF_UNIX SOCK_STREAM aceita a conexao no backlog do kernel assim que
// o cliente conecta, mesmo sem accept() do lado servidor, e isso
// basta para provar que o RELE consegue abrir uma conexao a montante
// por cliente sem precisar de um KWin falso que responda nada.
#include "wire_relay_connection_set.hpp"
#include "wire_test_encoders.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <string>

using namespace glintfx::test::wire_relay;

namespace {

// Mesma forma que wire_relay_main.cpp's own listen_downstream(), sem
// `die()` (o teste prefere GLINTFX_CHECK e devolver -1).
int listen_unix(const std::string &path) {
    ::unlink(path.c_str());
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
    if (::bind(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) != 0) {
        return -1;
    }
    if (::listen(fd, 16) != 0) {
        return -1;
    }
    return fd;
}

int connect_unix(const std::string &path) {
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
    if (::connect(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) != 0) {
        return -1;
    }
    return fd;
}

std::string temp_socket_path(const char *suffix) {
    const char *tmpdir = std::getenv("TMPDIR");
    const std::string base = (tmpdir != nullptr && tmpdir[0] != '\0') ? tmpdir : "/tmp";
    return base + "/glintfx-wire-relay-a3c-" + std::to_string(::getpid()) + "-" + suffix;
}

} // namespace

GLINTFX_TEST(wire_relay_a3c_second_client_accepted_before_first_closes) {
    const std::string downstream_path = temp_socket_path("down");
    const std::string upstream_path = temp_socket_path("up");
    const int listen_fd = listen_unix(downstream_path);
    GLINTFX_CHECK(listen_fd >= 0);
    const int fake_kwin_fd = listen_unix(upstream_path); // nunca aceita de proposito
    GLINTFX_CHECK(fake_kwin_fd >= 0);

    const relay_endpoints endpoints{listen_fd, upstream_path};
    connection_list connections;

    const int client_a = connect_unix(downstream_path);
    GLINTFX_CHECK(client_a >= 0);
    run_relay_cycle(endpoints, connections);
    GLINTFX_CHECK_EQ(connections.size(), 1u);

    // A PROVA CENTRAL DE A3c: o cliente A continua ABERTO (nunca
    // fechado, nunca esvaziado) quando o cliente B se conecta. No
    // design serial de A3b, accept() so voltava a ser chamado depois
    // que a sessao INTEIRA anterior fechava - este connect() nunca
    // teria sido aceito antes disso, e o teste travaria aqui (ou
    // connections.size() nunca passaria de 1).
    const int client_b = connect_unix(downstream_path);
    GLINTFX_CHECK(client_b >= 0);
    run_relay_cycle(endpoints, connections);
    GLINTFX_CHECK_EQ(connections.size(), 2u);

    const std::vector<std::uint8_t> message_a = encode_registry_bind(1, 0, "wl_shm", 1, 2);
    GLINTFX_CHECK(::send(client_a, message_a.data(), message_a.size(), MSG_NOSIGNAL) ==
                  static_cast<ssize_t>(message_a.size()));
    run_relay_cycle(endpoints, connections);
    GLINTFX_CHECK_EQ(connections[0]->session.stats.messages_from_client, 1u);
    GLINTFX_CHECK_EQ(connections[1]->session.stats.messages_from_client, 0u);

    const std::vector<std::uint8_t> message_b = encode_registry_bind(1, 0, "wl_compositor", 1, 2);
    GLINTFX_CHECK(::send(client_b, message_b.data(), message_b.size(), MSG_NOSIGNAL) ==
                  static_cast<ssize_t>(message_b.size()));
    run_relay_cycle(endpoints, connections);
    GLINTFX_CHECK_EQ(connections[0]->session.stats.messages_from_client, 1u);
    GLINTFX_CHECK_EQ(connections[1]->session.stats.messages_from_client, 1u);

    ::close(connections[0]->client_fd);
    ::close(connections[0]->upstream_fd);
    ::close(connections[1]->client_fd);
    ::close(connections[1]->upstream_fd);
    ::close(client_a);
    ::close(client_b);
    ::close(listen_fd);
    ::close(fake_kwin_fd);
}
