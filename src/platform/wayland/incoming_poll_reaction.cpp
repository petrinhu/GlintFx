// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/wayland/incoming_poll_reaction.hpp"

namespace glintfx::platform {

bool is_incoming_poll_connection_fatal(incoming_poll_outcome outcome,
                                       bool errno_is_eintr) noexcept {
    switch (outcome) {
    case incoming_poll_outcome::fatal:
        // POLLNVAL: an invalid fd is never something a read could make
        // sense of - always fatal, regardless of errno_is_eintr (this
        // outcome never comes from a failed ::poll() call, so there is
        // no errno to distinguish - incoming_poll_outcome.hpp's own
        // comment on this enumerator).
        return true;
    case incoming_poll_outcome::poll_call_failed:
        // A real ::poll() failure: EINTR is a signal arriving mid-wait,
        // never a reason to declare the connection unusable (the same
        // "transient, not fatal" distinction bounded_output_wait.hpp's
        // own header comment draws for the write-side twin); any OTHER
        // errno is a real, unusable connection.
        return !errno_is_eintr;
    case incoming_poll_outcome::nothing_yet:
    case incoming_poll_outcome::ready_to_read:
        // Budget exhausted with nothing reported, or proceed to a read
        // attempt - neither is a connection failure at THIS layer.
        return false;
    }
    // Unreachable (the switch above is exhaustive over incoming_poll_
    // outcome's four enumerators) - GODS_LAWS.md L-22 style safety net
    // only, never meant to be hit. Defaults to FALSE (not fatal), the
    // SAME default both call sites' own "unreachable" fallback already
    // returns (display_adapter.cpp::wait_for_incoming_data(), egl_
    // context_adapter.cpp::poll_and_dispatch_with_budget() - each ends
    // its own switch with `return ok(false)`/`return true` for exactly
    // this dead branch) - this atom's own default stays consistent with
    // the callers it now serves, rather than picking a new convention.
    return false;
}

} // namespace glintfx::platform
