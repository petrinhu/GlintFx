// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/wayland/bounded_output_wait.hpp"

#include <poll.h>

#include <cerrno>
#include <optional>

namespace glintfx::platform {

namespace {

// What one poll() wake says, by its revents (W8 deep verification C2-1, D-W8-2022: out of
// wait_for_writable_until() into its own atom, L-17). No value: a spurious wake. Why the order:
// Checked BEFORE POLLOUT below, on purpose: measured (this
// file's own tests/bounded_output_wait_test.cpp), a stream
// socket whose peer just closed reports BOTH bits set at
// once (POLLOUT because the kernel still has local send-
// buffer room, POLLHUP because the peer is gone) - the
// kernel is not lying about buffer space, but a write()
// here would still fail with EPIPE, so this is never
// writable in any sense a caller cares about. Reported
// exactly like a real poll() failure (this header's own
// comment: "conexao inutilizavel").
// A spurious wake with neither POLLOUT nor an error revent -
// loop and let the shrinking `remaining_ms` above re-poll with
// whatever time is left, rather than treating it as either
// outcome.
[[nodiscard]] std::optional<bounded_wait_outcome> outcome_of_wake(short revents) noexcept {
    // NOLINTNEXTLINE(bugprone-signed-bitwise) reason: poll(2) types these flags as signed
    if ((revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
        return bounded_wait_outcome::poll_failed;
    }
    // NOLINTNEXTLINE(bugprone-signed-bitwise) reason: poll(2) types these flags as signed
    if ((revents & POLLOUT) != 0) {
        return bounded_wait_outcome::ready;
    }
    return std::nullopt;
}

} // namespace

bounded_wait_outcome
wait_for_writable_until(int fd, std::chrono::steady_clock::time_point deadline) noexcept {
    for (;;) {
        const auto remaining_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                      deadline - std::chrono::steady_clock::now())
                                      .count();
        if (remaining_ms <= 0) {
            return bounded_wait_outcome::timed_out;
        }
        pollfd pending_write{.fd = fd, .events = POLLOUT, .revents = 0};
        const int poll_result = poll(&pending_write, 1, static_cast<int>(remaining_ms));
        if (poll_result == -1) {
            if (errno == EINTR) {
                continue; // this file's own header comment: retry with whatever time remains
            }
            return bounded_wait_outcome::poll_failed;
        }
        if (poll_result == 0) {
            return bounded_wait_outcome::timed_out;
        }
        if (const std::optional<bounded_wait_outcome> said = outcome_of_wake(pending_write.revents);
            said.has_value()) {
            return *said;
        }
        // A spurious wake: loop and re-poll with whatever time is left.
    }
}

} // namespace glintfx::platform
