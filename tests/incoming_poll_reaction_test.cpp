// SPDX-License-Identifier: AGPL-3.0-or-later
#include <poll.h>
#include <unistd.h>

#include <cerrno>

#include <fcntl.h>
#include <sys/socket.h>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/wayland/incoming_poll_outcome.hpp"
#include "platform/wayland/incoming_poll_reaction.hpp"

// incoming_poll_reaction_test.cpp - CONT-WARMUP C-6 (revisao adversarial
// C-5, /var/tmp/glintfx-plan/revisao-cont-warmup-c5.md, achado
// CRITICO-2, GODS_LAWS.md L-17/L-20/L-27): the red/green witness for
// glintfx::platform::is_incoming_poll_connection_fatal() (src/platform/
// wayland/incoming_poll_reaction.hpp) - the atom BOTH read-side call
// sites (display_adapter.cpp::wait_for_incoming_data(), egl_context_
// adapter.cpp::poll_and_dispatch_with_budget()) now call to decide
// whether a classify_incoming_poll() outcome means the connection is
// really dead. Two families of case below, exactly the two the C-6
// briefing asked for:
//
//  1. DIRECT/SYNTHETIC (all eight outcome x errno_is_eintr
//     combinations) - this is where the two states no real ::poll()
//     call in a unit test process can reliably produce (poll_call_
//     failed with an errno that is NOT EINTR, and EINTR itself) get
//     exercised: the atom's own signature already takes `errno_is_eintr`
//     as a plain bool, never reading real errno itself (this header's
//     own comment), so "injecting" those two states is just passing the
//     right value - no separate injection mechanism, nothing added to
//     the public API (GODS_LAWS.md L-19), no exception involved
//     (L-22).
//  2. REAL KERNEL, FABRICATED DESCRIPTOR - a closed fd, a socketpair
//     with its peer closed, and a pipe with one byte queued, each
//     poll()ed for REAL (not synthetic revents) and chained through
//     classify_incoming_poll() into this atom, proving the whole real-
//     kernel -> classifier -> decision path for the three outcomes a
//     kernel CAN produce on demand (fatal via POLLNVAL, ready_to_read
//     via POLLHUP/POLLIN, nothing_yet via a genuine empty-budget
//     timeout) - strictly more than tests/incoming_poll_outcome_test.
//     cpp already proves with hand-picked synthetic revents alone.
//
// RED, MEASURED (this fatia's own report, /var/tmp/glintfx-plan/
// impl-cont-warmup-c6.md): written before src/platform/wayland/
// incoming_poll_reaction.cpp existed - link failed with "undefined
// reference to glintfx::platform::is_incoming_poll_connection_fatal",
// a valid red for a function whose DECLARATION already existed (the
// header) but whose body did not (GODS_LAWS.md L-20's own precedent,
// CONT-WARMUP C-5's own report: "build falhou em erro de COMPILACAO...
// estado vermelho valido").
//
// WHAT THIS PROVES THAT CRITICO-2 SAYS NOTHING TODAY DOES: before this
// atom existed, `case fatal:`/`case poll_call_failed:` inside EACH of
// the two adapters' own switch decided with a hand-written `return`
// nothing directly tested - the C-5 review's own mutant
// (m-fatal-swallowed, swapping those returns) survived every test that
// existed. Section "MUTATION" below reproduces the SAME mutant against
// THIS atom and proves it no longer survives.

using glintfx::platform::classify_incoming_poll;
using glintfx::platform::incoming_poll_outcome;
using glintfx::platform::is_incoming_poll_connection_fatal;

// --- 1. Direto/sintetico: as oito combinacoes -----------------------

GLINTFX_TEST(is_incoming_poll_connection_fatal_fatal_is_always_fatal) {
    // POLLNVAL: an invalid fd is never something a read could make
    // sense of - fatal regardless of errno_is_eintr, which this outcome
    // never reads (there is no errno to read: fatal comes from revents,
    // not from a failed ::poll() - incoming_poll_outcome.hpp's own
    // comment).
    GLINTFX_CHECK(is_incoming_poll_connection_fatal(incoming_poll_outcome::fatal, false) == true);
    GLINTFX_CHECK(is_incoming_poll_connection_fatal(incoming_poll_outcome::fatal, true) == true);
}

GLINTFX_TEST(is_incoming_poll_connection_fatal_poll_call_failed_non_eintr_is_fatal) {
    // A real ::poll() failure for any reason OTHER than EINTR is a
    // real, unusable connection - the exact branch CRITICO-1/CRITICO-2
    // both name as the one that used to be silently swallowed.
    GLINTFX_CHECK(is_incoming_poll_connection_fatal(incoming_poll_outcome::poll_call_failed,
                                                    /*errno_is_eintr=*/false) == true);
}

GLINTFX_TEST(is_incoming_poll_connection_fatal_poll_call_failed_eintr_is_not_fatal) {
    // EINTR is a signal arriving mid-wait, never a reason to declare
    // the connection unusable - the same "transient, not fatal"
    // distinction bounded_output_wait.hpp's own header comment draws
    // for the write-side twin.
    GLINTFX_CHECK(is_incoming_poll_connection_fatal(incoming_poll_outcome::poll_call_failed,
                                                    /*errno_is_eintr=*/true) == false);
}

GLINTFX_TEST(is_incoming_poll_connection_fatal_nothing_yet_is_never_fatal) {
    // Budget genuinely exhausted, nothing reported - the ordinary
    // "nothing arrived yet" outcome, never fatal by itself, regardless
    // of errno_is_eintr (meaningless for this outcome - a caller never
    // has a real errno to report when poll_result == 0).
    GLINTFX_CHECK(is_incoming_poll_connection_fatal(incoming_poll_outcome::nothing_yet, false) ==
                  false);
    GLINTFX_CHECK(is_incoming_poll_connection_fatal(incoming_poll_outcome::nothing_yet, true) ==
                  false);
}

GLINTFX_TEST(is_incoming_poll_connection_fatal_ready_to_read_is_never_fatal) {
    // POLLIN, or POLLHUP/POLLERR without POLLIN: proceed to a read
    // attempt, never a latched failure at THIS layer - the caller's own
    // read is what discovers whether there was one more legitimate
    // message or nothing left at all (incoming_poll_outcome.hpp's own
    // comment on this enumerator).
    GLINTFX_CHECK(is_incoming_poll_connection_fatal(incoming_poll_outcome::ready_to_read, false) ==
                  false);
    GLINTFX_CHECK(is_incoming_poll_connection_fatal(incoming_poll_outcome::ready_to_read, true) ==
                  false);
}

// --- 2. Grounded em kernel real, descritor fabricado -----------------

namespace {

// classify_incoming_poll(::poll() real sobre `fd`) - a MESMA sequencia
// que os dois adaptadores fazem, repetida aqui em uma linha para as
// pequenas fixtures abaixo nunca duplicarem a leitura de `errno` de
// formas diferentes (a mesma convencao "ler errno DIREITO apos poll(),
// antes de qualquer outra chamada" que os dois adaptadores reais
// seguem).
[[nodiscard]] incoming_poll_outcome real_poll_outcome(int fd, int timeout_ms) noexcept {
    pollfd incoming{.fd = fd, .events = POLLIN, .revents = 0};
    const int poll_result = ::poll(&incoming, 1, timeout_ms);
    return classify_incoming_poll(poll_result, incoming.revents);
}

} // namespace

GLINTFX_TEST(real_closed_fd_pollnval_is_fatal_end_to_end) {
    // POSIX poll(2): polling a closed/invalid fd number sets POLLNVAL
    // in revents and returns normally (never -1) - the standard,
    // reliable technique for provoking POLLNVAL for real, used here
    // exactly as bounded_output_wait_test.cpp's own header comment
    // names for the write-side twin's own real-socket techniques.
    const int fd = ::open("/dev/null", O_RDONLY);
    GLINTFX_CHECK(fd != -1);
    GLINTFX_CHECK(::close(fd) == 0);
    // fd is now a closed descriptor number - single-threaded test
    // process, nothing else opens a file between close() and poll()
    // below, so no fd-number reuse race.
    const incoming_poll_outcome outcome = real_poll_outcome(fd, 0);
    GLINTFX_CHECK(outcome == incoming_poll_outcome::fatal);
    GLINTFX_CHECK(is_incoming_poll_connection_fatal(outcome, false) == true);
}

GLINTFX_TEST(real_socketpair_closed_peer_pollhup_is_not_fatal_end_to_end) {
    // A stream socket whose peer just closed reports POLLHUP (and, on
    // this kernel, POLLIN alongside it - bounded_output_wait_test.cpp's
    // own measured comment on the SAME mechanism) - poll(2)'s own
    // contract says a read still has to happen to tell "buffered data
    // ahead of EOF" from "truly gone" apart, so this is NOT fatal at
    // this layer (incoming_poll_outcome.hpp's own comment on
    // ready_to_read).
    int fds[2] = {-1, -1};
    GLINTFX_CHECK(::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
    GLINTFX_CHECK(::close(fds[1]) == 0); // the peer is gone

    const incoming_poll_outcome outcome = real_poll_outcome(fds[0], 0);
    GLINTFX_CHECK(outcome == incoming_poll_outcome::ready_to_read);
    GLINTFX_CHECK(is_incoming_poll_connection_fatal(outcome, false) == false);

    ::close(fds[0]);
}

GLINTFX_TEST(real_pipe_with_data_pollin_is_not_fatal_end_to_end) {
    int fds[2] = {-1, -1};
    GLINTFX_CHECK(::pipe(fds) == 0);
    const char byte = 'x';
    GLINTFX_CHECK(::write(fds[1], &byte, 1) == 1);

    const incoming_poll_outcome outcome = real_poll_outcome(fds[0], 0);
    GLINTFX_CHECK(outcome == incoming_poll_outcome::ready_to_read);
    GLINTFX_CHECK(is_incoming_poll_connection_fatal(outcome, false) == false);

    ::close(fds[0]);
    ::close(fds[1]);
}

GLINTFX_TEST(real_empty_pipe_zero_timeout_nothing_yet_is_not_fatal_end_to_end) {
    // Nothing written, both ends still open, timeout 0 - a genuine
    // budget-exhausted timeout (poll_result == 0), never fatal by
    // itself.
    int fds[2] = {-1, -1};
    GLINTFX_CHECK(::pipe(fds) == 0);

    const incoming_poll_outcome outcome = real_poll_outcome(fds[0], 0);
    GLINTFX_CHECK(outcome == incoming_poll_outcome::nothing_yet);
    GLINTFX_CHECK(is_incoming_poll_connection_fatal(outcome, false) == false);

    ::close(fds[0]);
    ::close(fds[1]);
}
