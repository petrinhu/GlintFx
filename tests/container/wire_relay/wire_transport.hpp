// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// wire_transport.hpp - the TRANSPORT atom of the wire relay
// (docs/plano-w7c.md SS3.A: "transporte (... controle de descritores
// dimensionado para o teto do nucleo e MSG_CTRUNC reprovando alto)").
//
// The only atom in this relay that touches a real file descriptor.
// GODS_LAWS.md L-09: every user of this class in sub-fatias A1/A2
// wraps one end of a socketpair() - never a real Wayland socket,
// never wayland-0, no compositor, no container.
namespace glintfx::test::wire_relay {

class wire_transport {
  public:
    // Linux SCM_MAX_FD (kernel-enforced ceiling on descriptors a
    // single sendmsg() can carry). Sizing this transport's ancillary
    // read buffer to less than that - not to the kernel ceiling - is
    // deliberate: it is what makes MSG_CTRUNC reachable at all from a
    // sender that is otherwise fully within its rights.
    static constexpr std::size_t max_fds_per_read = 28;

    struct read_result {
        std::vector<std::uint8_t> bytes;
        std::vector<int> fds;
        // The kernel could not fit every descriptor this read's
        // sender attached into our ancillary buffer - GODS_LAWS.md
        // L-40: fatal for the connection, never a silent fd drop.
        bool control_truncated = false;
        bool end_of_file = false;
    };

    explicit wire_transport(int fd) : m_fd(fd) {}

    [[nodiscard]] read_result read_once() const;

    // Sends bytes+fds verbatim in one sendmsg() call - the
    // transparent half of the relay's promise: what this writes is
    // byte-identical and fd-identical to what a caller read.
    void write_once(const std::uint8_t *bytes, std::size_t len, const std::vector<int> &fds) const;

    // Half-closes the write side. Callers must have already
    // forwarded every byte already read before calling this (D-A4 of
    // docs/plano-w7c-adendo-revalidacao.md: never close before
    // draining).
    void shutdown_write() const;

  private:
    int m_fd;
};

} // namespace glintfx::test::wire_relay
