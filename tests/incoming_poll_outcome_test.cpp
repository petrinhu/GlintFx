// SPDX-License-Identifier: AGPL-3.0-or-later
#include <poll.h>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/wayland/incoming_poll_outcome.hpp"

// incoming_poll_outcome_test.cpp - CONT-WARMUP C-2 (docs/plano-fecho-w7b.md
// D-10, GODS_LAWS.md L-20/L-36): the TDD red/green witness for
// glintfx::platform::classify_incoming_poll() (src/platform/wayland/
// incoming_poll_outcome.hpp) - the pure atom that decides what a
// completed read-side poll() means, pulled out of display_adapter.cpp::
// wait_for_incoming_data() so it is testable WITHOUT a real socket, a
// real wl_display, or a container (see that header's own comment for
// why the real kwin_wayland kill-timing never exercises the branch
// this file proves).
//
// RED, MEASURED (this fatia's own report, /var/tmp/glintfx-plan/
// impl-cont-warmup.md): with the OLD composite condition this atom
// replaces (`poll_result == 0 || (revents & POLLIN) == 0` => nothing
// to read), classify_incoming_poll_pollhup_alone_is_ready_to_read and
// classify_incoming_poll_pollnval_is_fatal below both FAILED - a
// hangup with no trailing data, and an invalid fd, were both silently
// absorbed as "nothing to read yet", the exact defect D-10 names.
// Restoring the old body locally and re-running this file reproduces
// that red; the version committed here is the GREEN one.

using glintfx::platform::classify_incoming_poll;
using glintfx::platform::incoming_poll_outcome;

GLINTFX_TEST(classify_incoming_poll_budget_exhausted_is_nothing_yet) {
    // poll_result == 0: the timeout expired with nothing reported at
    // all - revents is meaningless here (the kernel never touched it),
    // 0 is what a caller's own zero-initialized pollfd already has.
    GLINTFX_CHECK(classify_incoming_poll(0, 0) == incoming_poll_outcome::nothing_yet);
}

GLINTFX_TEST(classify_incoming_poll_spurious_wake_is_nothing_yet) {
    // poll_result > 0 (something woke the call) but NONE of POLLIN/
    // POLLHUP/POLLERR/POLLNVAL are set - a spurious wake, same
    // treatment as a plain timeout.
    GLINTFX_CHECK(classify_incoming_poll(1, 0) == incoming_poll_outcome::nothing_yet);
}

GLINTFX_TEST(classify_incoming_poll_plain_pollin_is_ready_to_read) {
    GLINTFX_CHECK(classify_incoming_poll(1, POLLIN) == incoming_poll_outcome::ready_to_read);
}

GLINTFX_TEST(classify_incoming_poll_pollin_and_pollhup_is_ready_to_read) {
    // The shape MEASURED against the real compositor (this fatia's own
    // report): POLLIN|POLLHUP together, one last legitimately-buffered
    // message ahead of the true EOF - both the OLD and the NEW logic
    // agree here (POLLIN alone already routes to ready_to_read), this
    // case exists to prove the OR does not regress it.
    GLINTFX_CHECK(classify_incoming_poll(1, static_cast<short>(POLLIN | POLLHUP)) ==
                  incoming_poll_outcome::ready_to_read);
}

GLINTFX_TEST(classify_incoming_poll_pollhup_alone_is_ready_to_read) {
    // THE CORE FIX (D-10): a hangup with NO trailing data set - poll(2)'s
    // own contract says a read still has to happen to tell "buffered
    // data ahead of EOF" from "truly gone" apart. The OLD composite
    // condition (`poll_result == 0 || (revents & POLLIN) == 0`) took
    // this straight to nothing_yet, forever, on an idle connection
    // whose peer died without one last message queued - the exact
    // "wait_events() spins without ever discovering the fatal error"
    // inference this fatia's own plan (§1.3) names.
    GLINTFX_CHECK(classify_incoming_poll(1, POLLHUP) == incoming_poll_outcome::ready_to_read);
}

GLINTFX_TEST(classify_incoming_poll_pollerr_alone_is_ready_to_read) {
    // Same reasoning as POLLHUP alone - the write-side twin
    // (bounded_output_wait.cpp) already folds POLLERR into the same
    // "connection unusable" bucket as POLLHUP; the read side reaches
    // the same verdict via a read attempt instead of a direct latch,
    // because POLLERR alone does not by itself prove there is nothing
    // left to read (this class's own header comment on read_and_
    // dispatch_incoming()).
    GLINTFX_CHECK(classify_incoming_poll(1, POLLERR) == incoming_poll_outcome::ready_to_read);
}

GLINTFX_TEST(classify_incoming_poll_pollnval_alone_is_fatal) {
    // POLLNVAL: the fd itself is invalid - never something a read
    // could make sense of, so this latches DIRECTLY, before any read
    // is attempted, exactly like the write-side twin folds its own
    // POLLNVAL into poll_failed. The OLD composite condition had NO
    // POLLNVAL branch at all: `(revents & POLLIN) == 0` is true for
    // POLLNVAL alone, so it silently returned "nothing to read" for an
    // invalid file descriptor - the second half of D-10's own defect.
    GLINTFX_CHECK(classify_incoming_poll(1, POLLNVAL) == incoming_poll_outcome::fatal);
}

GLINTFX_TEST(classify_incoming_poll_pollnval_takes_priority_over_pollhup) {
    // POLLNVAL combined with other bits still latches fatal - an
    // invalid fd is invalid regardless of what else the kernel also
    // happened to set.
    GLINTFX_CHECK(classify_incoming_poll(1, static_cast<short>(POLLNVAL | POLLHUP)) ==
                  incoming_poll_outcome::fatal);
}
