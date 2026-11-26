// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>
#include <vector>

// wire_fd_plan.hpp - WIRE-RELAY-FD-PASSTHROUGH-fix1 (D-W8-F13): which descriptors each message of
// one batch carries on the wire. A batch is every message completed by one read, in order, and
// `pending` is every descriptor received and not yet forwarded. libwayland reads descriptors as
// ONE queue, taking the next one for each 'h' argument of a message's signature (wayland
// connection.c, wl_connection_demarshal), so the wire is right whenever the descriptors keep
// their order and none arrives later than the message that needs it, whichever message carries
// it. What the relay cannot know is the signature of an interface it does not model. The plan:
// - a message of a modelled interface takes exactly its own count (`demand`, never more);
// - the descriptors no modelled message claims (the SURPLUS) go, all of them, with the first
//   message of an interface the relay does not model, the earliest that may need them;
// - with no such message in the batch, the surplus waits for a later batch (a message split
//   across reads), never thrown away;
// - a modelled message after that first unmodelled one, with surplus present, is not CERTAIN:
//   the queue order says its descriptor may be an unmodelled message's. The wire is still right;
//   only the capture, which needs create_pool's own descriptor, must not guess.
// The review of d0291f99 found the rule this replaces (every pending descriptor to any unmodelled
// message) taking create_pool's descriptor away from the capture when an unmodelled message
// WITHOUT descriptor came first in the same batch.
// Test-only relay code (tests/container/), header-only so the image build lists stay as they are.
namespace glintfx::test::wire_relay {

struct fd_share {
    std::size_t count = 0; // descriptors this message carries on the wire
    bool certain = true;   // false: a modelled message whose descriptors may be another's
};

struct fd_plan {
    std::vector<fd_share> shares; // one per message, in batch order
    std::size_t carried = 0;      // left pending for a later batch
    std::size_t missing = 0;      // modelled demand with no descriptor to give
};

// `demands`: per message, its modelled count, or std::nullopt for an interface not modelled.
[[nodiscard]] inline fd_plan plan_fd_shares(const std::vector<std::optional<std::size_t>> &demands,
                                            std::size_t pending) {
    std::size_t known = 0;
    for (const std::optional<std::size_t> &demand : demands) {
        known += demand.value_or(0);
    }
    const std::size_t surplus = pending > known ? pending - known : 0;
    const auto first_unmodelled = std::find(demands.begin(), demands.end(), std::nullopt);
    fd_plan plan;
    plan.missing = known > pending ? known - pending : 0;
    std::size_t remaining = pending;
    for (auto it = demands.begin(); it != demands.end(); ++it) {
        fd_share share;
        if (it->has_value()) {
            share.count = std::min(**it, remaining);
            share.certain = share.count == **it && !(surplus > 0 && it > first_unmodelled);
        } else if (it == first_unmodelled) {
            share.count = std::min(surplus, remaining);
        }
        remaining -= share.count;
        plan.shares.push_back(share);
    }
    plan.carried = remaining;
    return plan;
}

} // namespace glintfx::test::wire_relay
