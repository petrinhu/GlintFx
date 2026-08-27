// SPDX-License-Identifier: AGPL-3.0-or-later
#include <utility>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

#include "fake/fake_display_adapter.hpp"
#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/port/display_connection.hpp"

// display_connection_fake_test.cpp - ARCH-PORTS, TDD case R2 (CTO plan
// sec. 2): platform::display_connection<glintfx::test::fake_display_
// adapter> is the exact same template every real caller of display_
// connection<wayland_display_adapter> will use - this file is the
// proof that "o resto não percebe" which adapter is underneath
// (GODS_LAWS.md L-19). Every GLINTFX_TEST case here calls
// fake_display_adapter::reset() first (harness_main.cpp runs every
// case in one process, sequentially - see that file's own comment;
// the static counters this fixture uses are documented in fake_
// display_adapter.hpp's own header comment).

GLINTFX_TEST(connect_open_use_close_sequence_via_the_fake) {
    glintfx::test::fake_display_adapter::reset();

    glintfx::gltfx_rslt<glintfx::platform::display_connection<glintfx::test::fake_display_adapter>>
        connected =
            glintfx::platform::display_connection<glintfx::test::fake_display_adapter>::connect();

    GLINTFX_CHECK(connected.has_value());
    GLINTFX_CHECK(connected.value().is_open());
    // "abre": open() reached the adapter exactly once for this one
    // connect() call - not zero (the factory silently skipped it) and
    // not more than once (a retry loop nobody asked for).
    GLINTFX_CHECK(glintfx::test::fake_display_adapter::open_call_count() == 1);
    // "usa": the connection stays open across an ordinary read of its
    // own public surface - is_open() is not itself a mutating call.
    GLINTFX_CHECK(connected.value().is_open());
    GLINTFX_CHECK(glintfx::test::fake_display_adapter::close_call_count() == 0);

    // "fecha": destroying the display_connection value closes the
    // wrapped adapter exactly once - RAII, not something the caller
    // has to remember to invoke by hand.
    {
        const glintfx::gltfx_rslt<
            glintfx::platform::display_connection<glintfx::test::fake_display_adapter>>
            scoped = std::move(connected);
    }
    GLINTFX_CHECK(glintfx::test::fake_display_adapter::close_call_count() == 1);
}

GLINTFX_TEST(injected_refusal_reaches_the_caller_of_connect_unchanged) {
    glintfx::test::fake_display_adapter::reset();
    glintfx::test::fake_display_adapter::arm_failure(glintfx::gltfx_err_code::platform_failure);

    const glintfx::gltfx_rslt<
        glintfx::platform::display_connection<glintfx::test::fake_display_adapter>>
        connected =
            glintfx::platform::display_connection<glintfx::test::fake_display_adapter>::connect();

    // GODS_LAWS.md L-22: the error the FAKE injected arrives at THIS
    // caller INTACT - connect() never re-codes, wraps or swallows it.
    // Reaching this line at all is also part of the proof: a noexcept
    // function that let an exception escape would have called
    // std::terminate() before any of these checks ran ("nada lança").
    GLINTFX_CHECK(connected.has_error());
    GLINTFX_CHECK(connected.error().code() == glintfx::gltfx_err_code::platform_failure);

    // A refused open() still counts as "open() was called" - the
    // adapter's own is_open() correctly stays false, and connect()
    // never constructs a display_connection object at all when the
    // underlying open() failed (there is no successful value to have
    // closed, so close() is never reached for THIS attempt).
    GLINTFX_CHECK(glintfx::test::fake_display_adapter::open_call_count() == 1);
    GLINTFX_CHECK(glintfx::test::fake_display_adapter::close_call_count() == 0);
}

GLINTFX_TEST(a_different_injected_code_still_arrives_unchanged) {
    // Same shape as the case above, with a DIFFERENT code - proves the
    // path is generic (the factory does not special-case one specific
    // gltfx_err_code value), not just correct for platform_failure.
    glintfx::test::fake_display_adapter::reset();
    glintfx::test::fake_display_adapter::arm_failure(glintfx::gltfx_err_code::unsupported);

    const glintfx::gltfx_rslt<
        glintfx::platform::display_connection<glintfx::test::fake_display_adapter>>
        connected =
            glintfx::platform::display_connection<glintfx::test::fake_display_adapter>::connect();

    GLINTFX_CHECK(connected.has_error());
    GLINTFX_CHECK(connected.error().code() == glintfx::gltfx_err_code::unsupported);
}
