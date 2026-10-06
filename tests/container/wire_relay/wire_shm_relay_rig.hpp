// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "wire_shm_test_support.hpp"

#include "harness/check.hpp"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <string>

// wire_shm_relay_rig.hpp - test-only: unix test sockets and relay_rig, a
// client connected to a REAL relay_endpoints/run_relay_cycle with a fake
// compositor behind it (QA-SCREEN-CAPTURE P1). Not an atom of the relay.
namespace glintfx::test::wire_relay {

inline int listen_unix(const std::string &path) {
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return fd;
    }
    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
    GLINTFX_CHECK(::bind(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) == 0);
    GLINTFX_CHECK(::listen(fd, 4) == 0);
    return fd;
}

inline int connect_unix(const std::string &path) {
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return fd;
    }
    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
    GLINTFX_CHECK(::connect(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) == 0);
    return fd;
}

// A client connected to a REAL relay_endpoints/run_relay_cycle with a
// fake compositor behind it, capture on, in a private directory.
struct relay_rig {
    relay_rig()
        : listen_fd(listen_unix(dir.path() + "/down")), fake_kwin(listen_unix(dir.path() + "/up")),
          endpoints{listen_fd.get(), dir.path() + "/up", capture}, client(-1), compositor_side(-1) {
        GLINTFX_CHECK(::mkdir(capture.c_str(), 0755) == 0);
        connect_client();
    }

    // A new client connects (the relay accepts it and opens its own
    // upstream connection, which the fake compositor accepts). Call
    // again after finish() for a second, consecutive client.
    void connect_client() {
        client.reset(connect_unix(dir.path() + "/down"));
        run_relay_cycle(endpoints, connections); // accept
        GLINTFX_CHECK_EQ(connections.size(), std::size_t{1});
        compositor_side.reset(::accept(fake_kwin.get(), nullptr, nullptr));
    }

    void send(const std::vector<std::uint8_t> &bytes, const std::vector<int> &fds = {}) {
        wire_transport(client.get()).write_once(bytes.data(), bytes.size(), fds);
        run_relay_cycle(endpoints, connections);
    }

    // The client leaves, then the compositor side closes: only now is
    // the connection finished, and only now are the files written.
    void finish() {
        client.reset();
        run_relay_cycle(endpoints, connections);
        compositor_side.reset();
        run_relay_cycle(endpoints, connections);
    }

    [[nodiscard]] std::size_t serial() const { return connections.at(0)->serial; }

    scratch_dir dir;
    std::string capture = dir.path() + "/capture";
    fd_holder listen_fd;
    fd_holder fake_kwin;
    relay_endpoints endpoints;
    connection_list connections;
    fd_holder client;
    fd_holder compositor_side;
};

} // namespace glintfx::test::wire_relay
