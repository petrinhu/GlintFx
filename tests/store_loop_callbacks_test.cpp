// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstddef>
#include <vector>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/platform/loop/loop.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/loop/loop_impl.hpp"
#include "platform/loop/store_loop_callbacks.hpp"

// store_loop_callbacks_test.cpp - LOOP-CONTEXT-OWNERSHIP (S1b, /var/tmp/
// glintfx-plan/loop-fix.md sec. S1b): the six cases that prove
// store_loop_callbacks() (store_loop_callbacks.hpp) - FORM 2's own
// entry point, gltfx_loop::set_callbacks() (loop_facade.cpp) - stores,
// substitutes, refuses, and, through loop_impl's own default
// destructor (F29 - a member with a destructor runs at BOTH of the
// facade's own delete sites, without loop_facade.cpp changing at all),
// eventually destroys a guarded callbacks context correctly.
//
// NO SO ANYWHERE: allocate_loop_impl() (loop_impl.hpp's own header
// comment) builds a loop_impl on the heap touching no display/window/
// GL context - the SAME technique tests/loop_open_alloc_failure_test.
// cpp already uses, one file over. This file recompiles loop_impl.cpp
// AND store_loop_callbacks.cpp straight into its own binary (tests/
// CMakeLists.txt's own target_sources()) - neither carries GLINTFX_API,
// so there is no DLL boundary to cross.

using glintfx::gltfx_loop_callbacks;
using glintfx::gltfx_rslt;
using glintfx::loop_impl;

namespace {

std::vector<void *> g_destroyed;

void log_destroy(void *context) noexcept { g_destroyed.push_back(context); }

bool noop_on_frame(void * /*context*/, const glintfx::gltfx_frame_tick & /*tick*/) noexcept {
    return true;
}

void noop_on_render(void * /*context*/, const glintfx::gltfx_frame_tick & /*tick*/) noexcept {}

int g_marker_a = 0;
int g_marker_b = 0;
int g_marker_c = 0;

// A struct that PASSES validate_loop_callbacks() (on_frame/on_render
// both set, on_event left null) - store_loop_callbacks() calls that
// same validation (store_loop_callbacks.hpp's own header comment), so
// every case here needs a struct that clears it to exercise the
// posse-specific behavior this file is actually about.
[[nodiscard]] gltfx_loop_callbacks
valid_callbacks(void *context, glintfx::gltfx_loop_context_destroy_fn destroy) noexcept {
    gltfx_loop_callbacks callbacks{};
    callbacks.context = context;
    callbacks.on_frame = &noop_on_frame;
    callbacks.on_render = &noop_on_render;
    callbacks.destroy_context = destroy;
    return callbacks;
}

} // namespace

GLINTFX_TEST(accepted_callbacks_are_stored_and_not_destroyed) {
    g_destroyed.clear();
    gltfx_rslt<loop_impl *> allocated = glintfx::allocate_loop_impl();
    GLINTFX_CHECK(allocated.has_value());
    loop_impl *impl = allocated.value();

    const gltfx_rslt<void> stored =
        glintfx::platform::store_loop_callbacks(*impl, valid_callbacks(&g_marker_a, &log_destroy));
    GLINTFX_CHECK(stored.has_value());
    GLINTFX_CHECK(impl->book.stored_callbacks.context == &g_marker_a);
    GLINTFX_CHECK_EQ(g_destroyed.size(), static_cast<std::size_t>(0));

    delete impl;
}

GLINTFX_TEST(replacing_stored_callbacks_destroys_the_previous_one_after_the_new_is_stored) {
    g_destroyed.clear();
    loop_impl *impl = glintfx::allocate_loop_impl().value();

    const gltfx_rslt<void> first =
        glintfx::platform::store_loop_callbacks(*impl, valid_callbacks(&g_marker_a, &log_destroy));
    GLINTFX_CHECK(first.has_value());

    const gltfx_rslt<void> second =
        glintfx::platform::store_loop_callbacks(*impl, valid_callbacks(&g_marker_b, &log_destroy));
    GLINTFX_CHECK(second.has_value());

    // The OLD pair (marker_a) is destroyed exactly once, and the NEW
    // one (marker_b) is already the one impl->book reports - the black-
    // box order proof, the same shape owned_loop_context_test.cpp's own
    // reset_keeps_the_new_pair_owned_until_its_own_destruction uses.
    GLINTFX_CHECK_EQ(g_destroyed.size(), static_cast<std::size_t>(1));
    GLINTFX_CHECK(g_destroyed[0] == static_cast<void *>(&g_marker_a));
    GLINTFX_CHECK(impl->book.stored_callbacks.context == &g_marker_b);

    delete impl;
    GLINTFX_CHECK_EQ(g_destroyed.size(), static_cast<std::size_t>(2));
    GLINTFX_CHECK(g_destroyed[1] == static_cast<void *>(&g_marker_b));
}

GLINTFX_TEST(a_refused_replacement_destroys_only_the_new_pair_and_keeps_the_old_one) {
    g_destroyed.clear();
    loop_impl *impl = glintfx::allocate_loop_impl().value();

    const gltfx_rslt<void> first =
        glintfx::platform::store_loop_callbacks(*impl, valid_callbacks(&g_marker_a, &log_destroy));
    GLINTFX_CHECK(first.has_value());

    // on_frame left empty - validate_loop_callbacks() refuses it BY
    // NAME (loop_callbacks_validation.hpp's own fixed order).
    gltfx_loop_callbacks invalid{};
    invalid.context = &g_marker_b;
    invalid.on_render = &noop_on_render;
    invalid.destroy_context = &log_destroy;

    const gltfx_rslt<void> refused = glintfx::platform::store_loop_callbacks(*impl, invalid);
    GLINTFX_CHECK(refused.has_error());
    GLINTFX_CHECK(refused.err().rejected_value() == "on_frame");

    // The pair THIS call was just handed (marker_b) is destroyed;
    // marker_a - already stored - is left completely untouched.
    GLINTFX_CHECK_EQ(g_destroyed.size(), static_cast<std::size_t>(1));
    GLINTFX_CHECK(g_destroyed[0] == static_cast<void *>(&g_marker_b));
    GLINTFX_CHECK(impl->book.stored_callbacks.context == &g_marker_a);

    delete impl;
    GLINTFX_CHECK_EQ(g_destroyed.size(), static_cast<std::size_t>(2));
    GLINTFX_CHECK(g_destroyed[1] == static_cast<void *>(&g_marker_a));
}

GLINTFX_TEST(a_running_loop_refuses_store_by_name_and_destroys_nothing) {
    g_destroyed.clear();
    loop_impl *impl = glintfx::allocate_loop_impl().value();

    // Simulates what platform::loop_run's own running_guard would have
    // armed (loop_engine.hpp) - store_loop_callbacks() only ever READS
    // this flag (running_guard.hpp's own header comment), so setting it
    // directly here is a faithful, SO-free stand-in for "called from
    // inside on_frame of a run() already in progress" (tests/loop_
    // engine_test.cpp's own T16 exercises the SAME refusal live,
    // through loop_run() itself).
    impl->book.running = true;

    const gltfx_rslt<void> result =
        glintfx::platform::store_loop_callbacks(*impl, valid_callbacks(&g_marker_c, &log_destroy));
    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK(result.err().rejected_value() == "running");

    // D-LF-6d: the pair THIS call was just handed is destroyed too -
    // "recusado = não aceito = nada substituído" applies here exactly
    // as it does to the validate_loop_callbacks() refusal above; only
    // `running` itself makes this ONE cell's refusal different.
    GLINTFX_CHECK_EQ(g_destroyed.size(), static_cast<std::size_t>(1));
    GLINTFX_CHECK(g_destroyed[0] == static_cast<void *>(&g_marker_c));
    GLINTFX_CHECK(impl->book.stored_callbacks.context == nullptr);

    impl->book.running = false;
    delete impl;
    GLINTFX_CHECK_EQ(g_destroyed.size(), static_cast<std::size_t>(1)); // nothing was ever stored
}

GLINTFX_TEST(deleting_the_impl_destroys_the_guarded_context_exactly_once) {
    g_destroyed.clear();
    loop_impl *impl = glintfx::allocate_loop_impl().value();

    const gltfx_rslt<void> stored =
        glintfx::platform::store_loop_callbacks(*impl, valid_callbacks(&g_marker_a, &log_destroy));
    GLINTFX_CHECK(stored.has_value());
    GLINTFX_CHECK_EQ(g_destroyed.size(), static_cast<std::size_t>(0));

    delete impl; // F29: loop_impl's own default destructor reaches book.stored_context here.
    GLINTFX_CHECK_EQ(g_destroyed.size(), static_cast<std::size_t>(1));
    GLINTFX_CHECK(g_destroyed[0] == static_cast<void *>(&g_marker_a));
}

GLINTFX_TEST(deleting_an_impl_with_nothing_stored_destroys_nothing) {
    g_destroyed.clear();
    loop_impl *impl = glintfx::allocate_loop_impl().value();

    delete impl; // never stored anything - owned_loop_context stays empty, a no-op destructor.
    GLINTFX_CHECK_EQ(g_destroyed.size(), static_cast<std::size_t>(0));
}
