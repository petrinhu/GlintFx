// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstddef>
#include <vector>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/loop/owned_loop_context.hpp"

// owned_loop_context_test.cpp - LOOP-CONTEXT-OWNERSHIP (S1b, /var/tmp/
// glintfx-plan/loop-fix.md sec. S1b): the six cases that prove
// platform::owned_loop_context (owned_loop_context.hpp) destroys
// whatever it holds EXACTLY ONCE - on its own destruction, and on
// reset() - and never touches a null half of the (context, destroy)
// pair. No SO anywhere here: the atom is pure C++, no adapter, no
// display/window/context (S2-F11's own "pure enough to test directly"
// property, applied one layer down).
//
// SIX CASES, EACH ITS OWN GLINTFX_TEST (never one case with six
// sub-cells): every property below is independent of every other -
// there is no shared setup or shared assertion between them - so
// splitting keeps each case's own PASS/FAIL line naming exactly which
// property broke, rather than a single "N of N cell(s)" line a reader
// would have to cross-reference against this file's own comments
// (that packed-cell shape, used elsewhere in this sub-fatia's own
// tests/loop_engine_test.cpp, exists there because THOSE cells share
// one arranged scenario; these six do not).

namespace {

std::vector<void *> g_destroyed;

void log_destroy(void *context) noexcept { g_destroyed.push_back(context); }

int g_marker_a = 0;
int g_marker_b = 0;

} // namespace

GLINTFX_TEST(destructor_calls_destroy_exactly_once) {
    g_destroyed.clear();
    {
        glintfx::platform::owned_loop_context owned{&g_marker_a, &log_destroy};
        GLINTFX_CHECK_EQ(g_destroyed.size(), static_cast<std::size_t>(0)); // not yet
    }
    GLINTFX_CHECK_EQ(g_destroyed.size(), static_cast<std::size_t>(1));
    GLINTFX_CHECK(g_destroyed[0] == static_cast<void *>(&g_marker_a));
}

GLINTFX_TEST(null_destroy_fn_is_never_called) {
    g_destroyed.clear();
    {
        glintfx::platform::owned_loop_context owned{&g_marker_a, nullptr};
    }
    GLINTFX_CHECK_EQ(g_destroyed.size(), static_cast<std::size_t>(0));
}

GLINTFX_TEST(null_context_never_reaches_destroy) {
    g_destroyed.clear();
    {
        glintfx::platform::owned_loop_context owned{nullptr, &log_destroy};
    }
    GLINTFX_CHECK_EQ(g_destroyed.size(), static_cast<std::size_t>(0));
}

GLINTFX_TEST(reset_destroys_the_previous_pair_exactly_once) {
    g_destroyed.clear();
    glintfx::platform::owned_loop_context owned{&g_marker_a, &log_destroy};
    owned.reset(&g_marker_b, &log_destroy);
    GLINTFX_CHECK_EQ(g_destroyed.size(), static_cast<std::size_t>(1));
    GLINTFX_CHECK(g_destroyed[0] == static_cast<void *>(&g_marker_a));
}

// The NEW pair reset() stores must survive intact - `owned` still owns
// marker_b after reset() returned, and destroys it (only it, exactly
// once) when ITS OWN scope ends, strictly AFTER the previous pair was
// already destroyed by reset() itself above. The log's own order -
// marker_a first (at reset()), marker_b second (at final destruction)
// - is the black-box proof that reset() stores before it destroys
// (owned_loop_context.hpp's own D-LF-6e comment): had reset() destroyed
// the old pair BEFORE overwriting the members, or dropped the new pair
// on the floor, this case would see marker_b never destroyed at all,
// not merely out of order.
GLINTFX_TEST(reset_keeps_the_new_pair_owned_until_its_own_destruction) {
    g_destroyed.clear();
    {
        glintfx::platform::owned_loop_context owned{&g_marker_a, &log_destroy};
        owned.reset(&g_marker_b, &log_destroy);
    }
    GLINTFX_CHECK_EQ(g_destroyed.size(), static_cast<std::size_t>(2));
    GLINTFX_CHECK(g_destroyed[0] == static_cast<void *>(&g_marker_a));
    GLINTFX_CHECK(g_destroyed[1] == static_cast<void *>(&g_marker_b));
}

GLINTFX_TEST(reset_over_an_empty_atom_destroys_nothing) {
    g_destroyed.clear();
    glintfx::platform::owned_loop_context owned;
    owned.reset(&g_marker_a, &log_destroy);
    GLINTFX_CHECK_EQ(g_destroyed.size(), static_cast<std::size_t>(0));
}
