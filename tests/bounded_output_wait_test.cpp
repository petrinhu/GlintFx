// SPDX-License-Identifier: AGPL-3.0-or-later
#include <chrono>
#include <cstring>

#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/wayland/bounded_output_wait.hpp"

// bounded_output_wait_test.cpp - INBOX (drenagem 06/09/2026, GODS_LAWS.md
// L-17/L-20): the TDD red/green witness for glintfx::platform::
// wait_for_writable_until() (src/platform/wayland/bounded_output_wait.
// hpp) - the atom display_adapter.cpp::flush_with_retry() and egl_
// context_adapter.cpp::poll_and_dispatch_with_budget() now BOTH call
// instead of each carrying its own copy of "poll() for POLLOUT" (the
// duplication that let one copy's own infinite wait, poll(&pending_
// write, 1, -1), survive 72754af's fix of the other copy for hours).
//
// RED, SEEN (not reproduced by this binary - reproducing it live would
// require actually hanging a test process forever, which no CI budget
// can afford): tests/tools/check_wait_points.py's own --selftest and
// its real run against a scratch copy of this file's pre-fix ancestor
// (display_adapter.cpp::flush_with_retry() with poll(&pending_write, 1,
// -1) restored verbatim) reproved with the LITERAL line "sitios
// encontrados sem linha no manifesto: src/platform/wayland/display_
// adapter.cpp:244: if (poll(&pending_write, 1, -1) == -1) {" - the
// unbounded call, caught and cited by file:line. That transcript is
// this fatia's own real red; THIS file proves the atom that replaced
// it never blocks past its own budget.
//
// NO wl_display, NO EGL, NO COMPOSITOR, NO CONTAINER: bounded_output_
// wait.hpp's own header comment explains why a raw AF_UNIX SOCK_STREAM
// socketpair() exercises the EXACT same kernel mechanism (a full send
// buffer never draining, because nothing ever reads the peer) that a
// stuck compositor would put a real wl_display connection's fd through
// - the two are literally the same socket family and the same syscall
// path. This is the SAME class of "adaptador que so encaminha chamada
// ao sistema operacional" ARCH-PORTS' own declared TDD exception names
// (GODS_LAWS.md L-20) - proven here with the REAL syscall, never a
// fake, because unlike a wl_display connection this one needs no
// compositor to construct.

namespace {

// Creates a connected pair of blocking-on-read/non-blocking-on-write
// AF_UNIX stream sockets and fills the WRITE end's own kernel send
// buffer completely, so no further byte fits until something reads the
// other end - which this fixture's own caller never does. Returns the
// (now permanently unwritable, until read) write-end fd; the read end
// is intentionally leaked to the OS on process exit (a unit test
// process is short-lived, and closing it early would immediately make
// the write end writable again - the OPPOSITE of what every case below
// needs).
[[nodiscard]] int make_permanently_full_write_fd() noexcept {
    int fds[2] = {-1, -1};
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) {
        return -1;
    }
    const int write_fd = fds[0];
    const int read_fd = fds[1];
    // O_NONBLOCK on the WRITE end only: this fixture drives write()
    // itself, never poll()'s own return value, to reach EAGAIN and
    // stop - a blocking write() would hang this test's OWN setup step,
    // never reaching the code under test at all.
    const int flags = ::fcntl(write_fd, F_GETFL, 0);
    ::fcntl(write_fd, F_SETFL, flags | O_NONBLOCK);

    char buffer[4096];
    std::memset(buffer, 'x', sizeof(buffer));
    for (int attempts = 0; attempts < 100000; ++attempts) {
        const ssize_t written = ::write(write_fd, buffer, sizeof(buffer));
        if (written == -1) {
            break; // EAGAIN (or any other errno) - the buffer is full, exactly what this needs
        }
    }
    // read_fd is deliberately never closed and never read from here -
    // see this function's own header comment.
    (void)read_fd;
    return write_fd;
}

} // namespace

using glintfx::platform::bounded_wait_outcome;
using glintfx::platform::wait_for_writable_until;

GLINTFX_TEST(wait_for_writable_until_returns_ready_immediately_when_the_fd_already_is) {
    int fds[2] = {-1, -1};
    GLINTFX_CHECK(::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
    // Freshly connected, nothing written yet - both ends are writable
    // right now, so this must resolve WITHOUT waiting out any real
    // fraction of the budget below.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    const auto start = std::chrono::steady_clock::now();
    const bounded_wait_outcome outcome = wait_for_writable_until(fds[0], deadline);
    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - start)
                                .count();

    GLINTFX_CHECK(outcome == bounded_wait_outcome::ready);
    // Generous ceiling (CI can be slow) - the point is "did not wait
    // out anywhere NEAR the 5s budget", not a tight microbenchmark.
    GLINTFX_CHECK(elapsed_ms < 1000);

    ::close(fds[0]);
    ::close(fds[1]);
}

GLINTFX_TEST(wait_for_writable_until_times_out_within_budget_never_hangs) {
    const int write_fd = make_permanently_full_write_fd();
    GLINTFX_CHECK(write_fd != -1);

    // THIS is the case the old poll(&pending_write, 1, -1) call would
    // have blocked on FOREVER (see this file's own header comment) -
    // nothing ever reads the peer end, so the send buffer never
    // drains, so POLLOUT never arrives. A 150ms budget is short enough
    // that a genuine regression back to an unbounded wait would make
    // this single test case hang the whole suite - the same "provado
    // vermelho" property GODS_LAWS.md L-36 asks of a gate.
    constexpr auto k_budget = std::chrono::milliseconds(150);
    const auto deadline = std::chrono::steady_clock::now() + k_budget;
    const auto start = std::chrono::steady_clock::now();
    const bounded_wait_outcome outcome = wait_for_writable_until(write_fd, deadline);
    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - start)
                                .count();

    GLINTFX_CHECK(outcome == bounded_wait_outcome::timed_out);
    // Never negative (returned late) and never wildly over budget
    // (CI-slowness ceiling, the same generosity pump_smoke.cpp's own
    // header comment already documents for a shared-runner container).
    GLINTFX_CHECK(elapsed_ms >= 0);
    GLINTFX_CHECK(elapsed_ms < 5000);

    ::close(write_fd);
}

GLINTFX_TEST(wait_for_writable_until_reports_poll_failed_when_the_peer_is_gone) {
    int fds[2] = {-1, -1};
    GLINTFX_CHECK(::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
    ::close(fds[1]); // the peer is gone - fds[0] now only ever signals POLLHUP/POLLERR

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    const auto start = std::chrono::steady_clock::now();
    const bounded_wait_outcome outcome = wait_for_writable_until(fds[0], deadline);
    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - start)
                                .count();

    GLINTFX_CHECK(outcome == bounded_wait_outcome::poll_failed);
    // A gone peer is detected IMMEDIATELY (POLLHUP arrives right away,
    // this file's own header comment on wait_for_writable_until()'s
    // own error-revent branch) - never a wait out to the full budget.
    GLINTFX_CHECK(elapsed_ms < 1000);

    ::close(fds[0]);
}
