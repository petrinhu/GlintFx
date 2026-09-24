// SPDX-License-Identifier: AGPL-3.0-or-later
//
// wire_relay_main.cpp - the real wire relay binary (WL-ACK-SMOKE-
// BLUNT, sub-fatia A3, docs/plano-w7c.md SS3.A). Composes the five
// atoms (wire_transport, wire_decoder, wire_object_table, wire_rule_
// engine, wire_error_injector) plus wire_relay_pipeline's shared
// observe_and_evaluate() - the EXACT function A2's own selftest
// already proved case by case - into a standalone Unix-socket relay
// that tests/container/run_compositor.sh puts in front of a real
// KWin (A3b): fixtures connect to the socket THIS binary listens on;
// this binary is itself a client of the real compositor, on a
// separate, internal socket name run_compositor.sh chooses.
//
// GODS_LAWS.md L-09: this binary only ever runs INSIDE the isolated
// container run_compositor.sh already builds - it never touches the
// leader's live session, never opens a display of its own outside
// that container.
//
// Usage: wire_relay <upstream-socket-name> <downstream-socket-name>
// Both resolved against $XDG_RUNTIME_DIR, the same convention every
// Wayland client/server already uses.
#include "../checked_stdio.hpp"
#include "wire_decoder.hpp"
#include "wire_error_injector.hpp"
#include "wire_message.hpp"
#include "wire_object_table.hpp"
#include "wire_relay_pipeline.hpp"
#include "wire_transport.hpp"

#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <string>
#include <vector>

namespace {

using namespace glintfx::test::wire_relay;

// File descriptors received (via a direction's own reads) but not yet
// associated with the message that consumes them - see wire_object_
// table.hpp's own fd_argument_count(). FIFO: SCM_RIGHTS never crosses
// the boundary of the sendmsg() call that carried it (proved by A1's
// own "descritor no pedaco certo" control), so descriptors are always
// consumed in the same order they arrived.
class pending_fds {
  public:
    void push(const std::vector<int> &fds) {
        for (const int fd : fds) {
            m_queue.push_back(fd);
        }
    }

    std::vector<int> take(std::size_t count) {
        std::vector<int> out;
        while (count > 0 && !m_queue.empty()) {
            out.push_back(m_queue.front());
            m_queue.pop_front();
            --count;
        }
        return out;
    }

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

// One accepted client's whole session: the shared object table/rule
// engine (object ids are global to the connection) plus one decode
// state per direction (each direction frames its own messages
// independently - see wire_decoder.hpp's own comment on why poll()/
// take_message() never share state across callers).
struct relay_session {
    wire_relay_pipeline pipe;
    direction_state client_state;
    direction_state upstream_state;
    connection_stats stats;
};

[[noreturn]] void die(const char *what) {
    std::perror(what);
    std::exit(1);
}

std::string runtime_socket_path(const char *socket_name) {
    const char *dir = std::getenv("XDG_RUNTIME_DIR");
    if (dir == nullptr || dir[0] == '\0') {
        glintfx::container_fixture::checked_fprintf(stderr,
                                                    "wire_relay: XDG_RUNTIME_DIR not set\n");
        std::exit(1);
    }
    return std::string(dir) + "/" + socket_name;
}

void fill_unix_addr(struct sockaddr_un &addr, const std::string &path) {
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
}

int connect_upstream(const std::string &path) {
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        die("wire_relay: socket(upstream)");
    }
    struct sockaddr_un addr;
    fill_unix_addr(addr, path);
    if (::connect(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) != 0) {
        die("wire_relay: connect(upstream)");
    }
    return fd;
}

int listen_downstream(const std::string &path) {
    ::unlink(path.c_str()); // stale socket file from an earlier run, if any
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        die("wire_relay: socket(downstream)");
    }
    struct sockaddr_un addr;
    fill_unix_addr(addr, path);
    if (::bind(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) != 0) {
        die("wire_relay: bind(downstream)");
    }
    if (::listen(fd, 16) != 0) {
        die("wire_relay: listen(downstream)");
    }
    return fd;
}

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
// session must end (clean EOF, control truncation, or a protocol
// violation just injected the error and closed the client).
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

// True while the connection should keep running.
bool pump_client_direction(const wire_transport &client, const wire_transport &upstream,
                           relay_session &session) {
    const wire_transport::read_result chunk = client.read_once();
    if (chunk.control_truncated) {
        glintfx::container_fixture::checked_fprintf(
            stderr, "wire_relay: control truncated from client, closing\n");
        return false;
    }
    if (chunk.end_of_file) {
        upstream.shutdown_write();
        return false;
    }
    session.client_state.decoder.feed(chunk.bytes.data(), chunk.bytes.size());
    session.client_state.fds.push(chunk.fds);

    while (session.client_state.decoder.poll() == decode_outcome::message_ready) {
        const std::optional<decoded_message> message = session.client_state.decoder.take_message();
        if (!message) {
            break;
        }
        ++session.stats.messages_from_client;
        if (!forward_client_message(client, upstream, *message, session)) {
            return false;
        }
    }
    return true;
}

// Upstream -> client: transparent, but still tracks configure_sent
// (R2 needs to know which serials the compositor - real or, one day,
// this relay's own injector - actually sent).
bool pump_upstream_direction(const wire_transport &upstream, const wire_transport &client,
                             relay_session &session) {
    const wire_transport::read_result chunk = upstream.read_once();
    if (chunk.control_truncated) {
        glintfx::container_fixture::checked_fprintf(
            stderr, "wire_relay: control truncated from upstream, closing\n");
        return false;
    }
    if (chunk.end_of_file) {
        client.shutdown_write();
        return false;
    }
    session.upstream_state.decoder.feed(chunk.bytes.data(), chunk.bytes.size());
    session.upstream_state.fds.push(chunk.fds);

    while (session.upstream_state.decoder.poll() == decode_outcome::message_ready) {
        const std::optional<decoded_message> message =
            session.upstream_state.decoder.take_message();
        if (!message) {
            break;
        }
        ++session.stats.messages_from_upstream;
        (void)observe_and_evaluate(session.pipe, *message, false); // never violates this direction
        forward_message(client, *message, session.pipe, session.upstream_state.fds);
    }
    return true;
}

void poll_two(int client_fd, int upstream_fd, bool client_open, bool upstream_open,
              struct pollfd (&fds)[2], std::size_t &count) {
    count = 0;
    if (client_open) {
        fds[count] = {client_fd, POLLIN, 0};
        ++count;
    }
    if (upstream_open) {
        fds[count] = {upstream_fd, POLLIN, 0};
        ++count;
    }
}

connection_stats run_relay_loop(int client_fd, int upstream_fd) {
    const wire_transport client(client_fd);
    const wire_transport upstream(upstream_fd);
    relay_session session;

    bool client_open = true;
    bool upstream_open = true;
    struct pollfd fds[2];

    while (client_open || upstream_open) {
        std::size_t count = 0;
        poll_two(client_fd, upstream_fd, client_open, upstream_open, fds, count);
        if (::poll(fds, static_cast<nfds_t>(count), -1) < 0) {
            std::perror("wire_relay: poll");
            break;
        }
        for (std::size_t i = 0; i < count; ++i) {
            if ((fds[i].revents & (POLLIN | POLLHUP | POLLERR)) == 0) {
                continue;
            }
            if (fds[i].fd == client_fd) {
                client_open = pump_client_direction(client, upstream, session);
            } else {
                upstream_open = pump_upstream_direction(upstream, client, session);
            }
        }
    }
    return session.stats;
}

void serve_one_client(int client_fd, const std::string &upstream_path) {
    const int upstream_fd = connect_upstream(upstream_path);
    const connection_stats stats = run_relay_loop(client_fd, upstream_fd);
    glintfx::container_fixture::checked_fprintf(
        stdout,
        "wire_relay: connection closed - %zu message(s) from client, %zu from upstream, %zu "
        "violation(s)\n",
        stats.messages_from_client, stats.messages_from_upstream, stats.violations);
    (void)std::fflush(stdout);
    (void)::close(upstream_fd);
    (void)::close(client_fd);
}

} // namespace

int main(int argc, char **argv) {
    if (argc != 3) {
        glintfx::container_fixture::checked_fprintf(
            stderr, "usage: wire_relay <upstream-socket-name> <downstream-socket-name>\n");
        return 1;
    }
    const std::string upstream_path = runtime_socket_path(argv[1]);
    const std::string downstream_path = runtime_socket_path(argv[2]);

    const int listen_fd = listen_downstream(downstream_path);
    glintfx::container_fixture::checked_fprintf(stdout,
                                                "wire_relay: listening on %s, upstream %s\n",
                                                downstream_path.c_str(), upstream_path.c_str());
    (void)std::fflush(stdout);

    while (true) {
        const int client_fd = ::accept(listen_fd, nullptr, nullptr);
        if (client_fd < 0) {
            std::perror("wire_relay: accept");
            continue;
        }
        serve_one_client(client_fd, upstream_path);
    }
}
