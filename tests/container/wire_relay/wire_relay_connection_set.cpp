// SPDX-License-Identifier: AGPL-3.0-or-later
#include "wire_relay_connection_set.hpp"

#include "../checked_stdio.hpp"
#include "wire_error_injector.hpp"
#include "wire_message.hpp"
#include "wire_object_table.hpp"
#include "wire_transport.hpp"

#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace glintfx::test::wire_relay {

void pending_fds::push(const std::vector<int> &fds) {
    for (const int fd : fds) {
        m_queue.push_back(fd);
    }
}

std::vector<int> pending_fds::take(std::size_t count) {
    std::vector<int> out;
    while (count > 0 && !m_queue.empty()) {
        out.push_back(m_queue.front());
        m_queue.pop_front();
        --count;
    }
    return out;
}

namespace {

// Consumes exactly the fds `message` needs (wire_object_table's own
// fixed table) from `fds`, and forwards the message byte-exact to
// `target` (encode_raw is the proven-lossless inverse of decode).
void forward_message(const wire_transport &target, const decoded_message &message,
                     const wire_relay_pipeline &pipe, pending_fds &fds) {
    const known_interface interface = pipe.table.interface_of(message.header.object_id);
    const std::vector<int> message_fds =
        fds.take(fd_argument_count(interface, message.header.opcode));
    const std::vector<std::uint8_t> raw = encode_raw(message);
    target.write_once(raw.data(), raw.size(), message_fds);
}

// Client -> upstream: rule-checked (R1/R2). Returns false when the
// session must end (a protocol violation just injected the error and
// closed both sides).
bool forward_client_message(const wire_transport &client, const wire_transport &upstream,
                            const decoded_message &message, relay_session &session) {
    const std::optional<rule_violation> violation =
        observe_and_evaluate(session.pipe, message, true);
    if (!violation) {
        forward_message(upstream, message, session.pipe, session.client_state.fds);
        return true;
    }
    ++session.stats.violations;
    const std::vector<std::uint8_t> error_bytes =
        encode_display_error(*violation, "protocol error injected by wire_relay");
    client.write_once(error_bytes.data(), error_bytes.size(), {});
    client.shutdown_write();
    upstream.shutdown_write();
    return false;
}

void fill_unix_addr(struct sockaddr_un &addr, const std::string &path) {
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
}

// -1 on failure (never aborts the process - a montante indisponivel
// para UM cliente novo nao pode derrubar as conexoes ja em curso).
int connect_upstream(const std::string &path) {
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        std::perror("wire_relay: socket(upstream)");
        return -1;
    }
    struct sockaddr_un addr;
    fill_unix_addr(addr, path);
    if (::connect(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) != 0) {
        std::perror("wire_relay: connect(upstream)");
        (void)::close(fd);
        return -1;
    }
    return fd;
}

struct poll_entry_meta {
    active_connection *conn; // nullptr = o proprio listen_fd
    bool is_client;          // ignorado quando conn == nullptr
};

// Acrescenta, para uma conexao ainda aberta em pelo menos uma
// direcao, uma entrada por direcao ABERTA aos dois vetores paralelos
// (mesmo idioma que poll_two() de A3b ja usava para uma sessao so).
void append_connection_poll_entries(active_connection &conn, std::vector<struct pollfd> &fds,
                                    std::vector<poll_entry_meta> &meta) {
    if (conn.client_open) {
        fds.push_back({conn.client_fd, POLLIN, 0});
        meta.push_back({&conn, true});
    }
    if (conn.upstream_open) {
        fds.push_back({conn.upstream_fd, POLLIN, 0});
        meta.push_back({&conn, false});
    }
}

void build_poll_lists(const relay_endpoints &endpoints, connection_list &connections,
                      std::vector<struct pollfd> &out_fds, std::vector<poll_entry_meta> &out_meta) {
    out_fds.push_back({endpoints.listen_fd, POLLIN, 0});
    out_meta.push_back({nullptr, false});
    for (const std::unique_ptr<active_connection> &conn : connections) {
        append_connection_poll_entries(*conn, out_fds, out_meta);
    }
}

bool entry_ready(const struct pollfd &pfd) {
    return (pfd.revents & (POLLIN | POLLHUP | POLLERR)) != 0;
}

void dispatch_one_entry(const struct pollfd &pfd, const poll_entry_meta &entry,
                        const relay_endpoints &endpoints, connection_list &connections) {
    if (!entry_ready(pfd)) {
        return;
    }
    if (entry.conn == nullptr) {
        accept_new_connection(endpoints, connections);
        return;
    }
    if (entry.is_client) {
        pump_client_direction(*entry.conn);
    } else {
        pump_upstream_direction(*entry.conn);
    }
}

void dispatch_ready_entries(const std::vector<struct pollfd> &fds,
                            const std::vector<poll_entry_meta> &meta,
                            const relay_endpoints &endpoints, connection_list &connections) {
    for (std::size_t i = 0; i < fds.size(); ++i) {
        dispatch_one_entry(fds[i], meta[i], endpoints, connections);
    }
}

void remove_finished_connections(connection_list &connections) {
    connections.erase(std::remove_if(connections.begin(), connections.end(),
                                     [](const std::unique_ptr<active_connection> &conn) {
                                         if (!connection_finished(*conn)) {
                                             return false;
                                         }
                                         close_and_report(*conn);
                                         return true;
                                     }),
                      connections.end());
}

} // namespace

void pump_client_direction(active_connection &conn) {
    const wire_transport client(conn.client_fd);
    const wire_transport upstream(conn.upstream_fd);
    const wire_transport::read_result chunk = client.read_once();
    if (chunk.control_truncated) {
        glintfx::container_fixture::checked_fprintf(
            stderr, "wire_relay: control truncated from client, closing\n");
        conn.client_open = false;
        return;
    }
    if (chunk.end_of_file) {
        upstream.shutdown_write();
        conn.client_open = false;
        return;
    }
    conn.session.client_state.decoder.feed(chunk.bytes.data(), chunk.bytes.size());
    conn.session.client_state.fds.push(chunk.fds);

    while (conn.session.client_state.decoder.poll() == decode_outcome::message_ready) {
        const std::optional<decoded_message> message =
            conn.session.client_state.decoder.take_message();
        if (!message) {
            break;
        }
        ++conn.session.stats.messages_from_client;
        if (!forward_client_message(client, upstream, *message, conn.session)) {
            conn.client_open = false;
            return;
        }
    }
}

void pump_upstream_direction(active_connection &conn) {
    const wire_transport upstream(conn.upstream_fd);
    const wire_transport client(conn.client_fd);
    const wire_transport::read_result chunk = upstream.read_once();
    if (chunk.control_truncated) {
        glintfx::container_fixture::checked_fprintf(
            stderr, "wire_relay: control truncated from upstream, closing\n");
        conn.upstream_open = false;
        return;
    }
    if (chunk.end_of_file) {
        client.shutdown_write();
        conn.upstream_open = false;
        return;
    }
    conn.session.upstream_state.decoder.feed(chunk.bytes.data(), chunk.bytes.size());
    conn.session.upstream_state.fds.push(chunk.fds);

    while (conn.session.upstream_state.decoder.poll() == decode_outcome::message_ready) {
        const std::optional<decoded_message> message =
            conn.session.upstream_state.decoder.take_message();
        if (!message) {
            break;
        }
        ++conn.session.stats.messages_from_upstream;
        (void)observe_and_evaluate(conn.session.pipe, *message,
                                   false); // never violates this direction
        forward_message(client, *message, conn.session.pipe, conn.session.upstream_state.fds);
    }
}

bool connection_finished(const active_connection &conn) {
    return !conn.client_open && !conn.upstream_open;
}

void close_and_report(const active_connection &conn) {
    glintfx::container_fixture::checked_fprintf(
        stdout,
        "wire_relay: connection closed - %zu message(s) from client, %zu from upstream, %zu "
        "violation(s)\n",
        conn.session.stats.messages_from_client, conn.session.stats.messages_from_upstream,
        conn.session.stats.violations);
    (void)std::fflush(stdout);
    (void)::close(conn.upstream_fd);
    (void)::close(conn.client_fd);
}

void accept_new_connection(const relay_endpoints &endpoints, connection_list &connections) {
    const int client_fd = ::accept(endpoints.listen_fd, nullptr, nullptr);
    if (client_fd < 0) {
        std::perror("wire_relay: accept");
        return;
    }
    // D-A2 (docs/plano-w7c.md SS3.A): uma conexao a montante NOVA e
    // PROPRIA para este cliente - nunca compartilhada com outra
    // conexao ja ativa, mesmo padrao que serve_one_client() de A3b ja
    // seguia por sessao.
    const int upstream_fd = connect_upstream(endpoints.upstream_path);
    if (upstream_fd < 0) {
        glintfx::container_fixture::checked_fprintf(
            stderr, "wire_relay: falha ao conectar montante para cliente novo, recusando-o\n");
        (void)::close(client_fd);
        return;
    }
    auto conn = std::make_unique<active_connection>();
    conn->client_fd = client_fd;
    conn->upstream_fd = upstream_fd;
    connections.push_back(std::move(conn));
}

void run_relay_cycle(const relay_endpoints &endpoints, connection_list &connections) {
    std::vector<struct pollfd> fds;
    std::vector<poll_entry_meta> meta;
    build_poll_lists(endpoints, connections, fds, meta);
    if (::poll(fds.data(), static_cast<nfds_t>(fds.size()), -1) < 0) {
        std::perror("wire_relay: poll");
        return;
    }
    dispatch_ready_entries(fds, meta, endpoints, connections);
    remove_finished_connections(connections);
}

} // namespace glintfx::test::wire_relay
