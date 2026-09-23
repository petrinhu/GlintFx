// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/wayland/incoming_poll_outcome.hpp"

#include <poll.h>

namespace glintfx::platform {

incoming_poll_outcome classify_incoming_poll(int poll_result, short revents) noexcept {
    if (poll_result == 0) {
        // Budget genuinely exhausted, nothing reported at all - the
        // ordinary "nothing arrived yet" outcome, never fatal by
        // itself.
        return incoming_poll_outcome::nothing_yet;
    }
    if ((revents & POLLNVAL) != 0) {
        // An invalid fd is never something a read could make sense of
        // - latched before any read is attempted, same as the write-
        // side twin (bounded_output_wait.cpp) folds its own POLLNVAL
        // into the same poll_failed outcome as POLLERR/POLLHUP there.
        return incoming_poll_outcome::fatal;
    }
    if ((revents & (POLLIN | POLLHUP | POLLERR)) == 0) {
        // A spurious wake with none of POLLIN/POLLHUP/POLLERR/POLLNVAL
        // set - nothing to read yet, same as a plain timeout.
        return incoming_poll_outcome::nothing_yet;
    }
    // POLLIN, or POLLHUP/POLLERR without POLLIN: poll(2)'s own
    // contract - "subsequent reads will return 0 (EOF) only after all
    // outstanding data in the channel has been consumed" - a hangup
    // can still have real buffered data ahead of the EOF, so this
    // falls through to a read attempt EXACTLY like a plain POLLIN
    // would; the caller's own read is what actually discovers whether
    // there was one more legitimate message, or nothing left at all.
    return incoming_poll_outcome::ready_to_read;
}

} // namespace glintfx::platform
