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
// A3c (docs/plano-w7c.md SS3.A, "uma conexao a montante POR CLIENTE"):
// this file used to hold the whole one-client-at-a-time engine
// (accept() blocked inside serve_one_client() until that WHOLE
// session closed). Promoted to wire_relay_connection_set.hpp/.cpp,
// which serves every accepted client CONCURRENTLY over a single
// poll() loop, no thread - see that header's own comment for the
// live-measured defect this fixes (gl_context_parity_test, which
// opens a SECOND Wayland connection before closing the first, hung
// behind the old serial relay and passed clean straight against
// KWin). What is left here is process setup and main() only.
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
#include "wire_relay_connection_set.hpp"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

using namespace glintfx::test::wire_relay;

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
    const relay_endpoints endpoints{listen_fd, upstream_path};

    glintfx::container_fixture::checked_fprintf(stdout,
                                                "wire_relay: listening on %s, upstream %s\n",
                                                downstream_path.c_str(), upstream_path.c_str());
    (void)std::fflush(stdout);

    connection_list connections;
    while (true) {
        run_relay_cycle(endpoints, connections);
    }
}
