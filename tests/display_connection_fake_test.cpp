// SPDX-License-Identifier: AGPL-3.0-or-later
#include <utility>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

#include "fake/fake_display_adapter.hpp"
#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/port/display_connection.hpp"

// display_connection_fake_test.cpp - ARCH-PORTS, TDD case R2 (CTO plan
// sec. 2); rewritten for FACADE-PIN (docs/plano-conserto-fachadas-uaf.md
// sec. 7.3/8, T2): platform::display_connection<glintfx::test::fake_
// display_adapter> is the exact same template every real caller of
// display_connection<wayland_display_adapter> will use - this file is
// the proof that "o resto não percebe" which adapter is underneath
// (GODS_LAWS.md L-19). Every GLINTFX_TEST case here calls fake_display_
// adapter::reset() first (harness_main.cpp runs every case in one
// process, sequentially - see that file's own comment; the static
// counters this fixture uses are documented in fake_display_adapter.
// hpp's own header comment).
//
// connect() IS GONE (display_connection.hpp's own header comment): a
// pinned display_connection<A> cannot be returned by value inside a
// gltfx_rslt<display_connection<A>> (gltfx_rslt<T>::ok(T) needs T
// constructible from an rvalue, which a pinned type never is). Every
// case below default-constructs display_connection<fake_display_
// adapter> IN PLACE (a local variable is "in place" for a stack-lived
// test the same way a heap allocation is for display_impl - the
// address never changes after construction either way) and calls
// open() on it - the exact sequence display_facade.cpp's own gltfx_
// display::open() now uses one layer up.

GLINTFX_TEST(open_use_close_sequence_via_the_fake) {
    glintfx::test::fake_display_adapter::reset();

    glintfx::platform::display_connection<glintfx::test::fake_display_adapter> connection;
    const glintfx::gltfx_rslt<void> opened = connection.open();

    GLINTFX_CHECK(opened.has_value());
    GLINTFX_CHECK(connection.is_open());
    // "abre": open() reached the adapter exactly once for this one
    // open() call - not zero (the wrapper silently skipped it) and not
    // more than once (a retry loop nobody asked for).
    GLINTFX_CHECK(glintfx::test::fake_display_adapter::open_call_count() == 1);
    // "usa": the connection stays open across an ordinary read of its
    // own public surface - is_open() is not itself a mutating call.
    GLINTFX_CHECK(connection.is_open());
    GLINTFX_CHECK(glintfx::test::fake_display_adapter::close_call_count() == 0);

    // "fecha": destroying the display_connection value below closes the
    // wrapped adapter exactly once - RAII, not something the caller has
    // to remember to invoke by hand. Proved by scoping a SECOND
    // display_connection and letting it run out of scope, since this
    // one is pinned in place for the rest of this test's own body.
    {
        glintfx::platform::display_connection<glintfx::test::fake_display_adapter> scoped;
        const glintfx::gltfx_rslt<void> scoped_opened = scoped.open();
        GLINTFX_CHECK(scoped_opened.has_value());
    }
    GLINTFX_CHECK(glintfx::test::fake_display_adapter::close_call_count() == 1);
}

// FACADE-PIN, T2 (docs/plano-conserto-fachadas-uaf.md sec. 8): the
// vermelho this plan's own decision D1 exists to make pass. Before this
// fatia, connect() default-constructed the adapter on the stack, opened
// it THERE, and only afterward moved the now-open adapter into the
// display_connection it returned - fake_display_adapter::opened_at()
// would have recorded the STACK address, and this exact comparison
// would have failed against a caller reading `&connection.adapter()`
// after the move. Now that open() runs on the adapter AT ITS FINAL
// ADDRESS (display_connection<A>::open(), display_connection.hpp), the
// two addresses are the SAME address, by construction.
GLINTFX_TEST(opened_at_matches_the_final_address_of_the_wrapped_adapter) {
    glintfx::test::fake_display_adapter::reset();

    glintfx::platform::display_connection<glintfx::test::fake_display_adapter> connection;
    const glintfx::gltfx_rslt<void> opened = connection.open();
    GLINTFX_CHECK(opened.has_value());

    GLINTFX_CHECK(static_cast<const void *>(&connection.adapter()) ==
                  connection.adapter().opened_at());
}

GLINTFX_TEST(injected_refusal_reaches_the_caller_of_open_unchanged) {
    glintfx::test::fake_display_adapter::reset();
    glintfx::test::fake_display_adapter::arm_failure(glintfx::gltfx_err_code::platform_failure);

    glintfx::platform::display_connection<glintfx::test::fake_display_adapter> connection;
    const glintfx::gltfx_rslt<void> opened = connection.open();

    // GODS_LAWS.md L-22: the error the FAKE injected arrives at THIS
    // caller INTACT - open() never re-codes, wraps or swallows it.
    // Reaching this line at all is also part of the proof: a noexcept
    // function that let an exception escape would have called
    // std::terminate() before any of these checks ran ("nada lança").
    GLINTFX_CHECK(opened.has_error());
    GLINTFX_CHECK(opened.err().code() == glintfx::gltfx_err_code::platform_failure);

    // A refused open() still counts as "open() was called" - the
    // adapter's own is_open() correctly stays false, and close() is
    // never reached for THIS attempt (there is nothing successfully
    // open to have closed).
    GLINTFX_CHECK(!connection.is_open());
    GLINTFX_CHECK(glintfx::test::fake_display_adapter::open_call_count() == 1);
    GLINTFX_CHECK(glintfx::test::fake_display_adapter::close_call_count() == 0);
}

GLINTFX_TEST(adapter_accessor_reaches_the_same_wrapped_instance) {
    // D-W5-10 (docs/plano-w6a-janela.md sec. 1, W-D'): adapter() is the
    // one crack display_connection<A> deliberately opens in its own
    // opacity - a caller reaching through it has to observe the SAME
    // underlying fake_display_adapter the connection itself already
    // wraps, never a copy and never a second instance. open_call_count()
    // is a static counter on the ADAPTER TYPE, not per-instance
    // (fake_display_adapter.hpp's own header comment explains why), so
    // this proves identity indirectly: calling close() through the
    // accessor is observed by is_open() on the connection itself
    // (which reads through its OWN m_adapter, never through adapter()),
    // and by close_call_count() ticking exactly once - two different
    // paths agreeing is only possible if adapter() handed back the
    // real, live object, not a detached copy.
    glintfx::test::fake_display_adapter::reset();

    glintfx::platform::display_connection<glintfx::test::fake_display_adapter> connection;
    const glintfx::gltfx_rslt<void> opened = connection.open();
    GLINTFX_CHECK(opened.has_value());

    GLINTFX_CHECK(connection.adapter().is_open());
    GLINTFX_CHECK(std::as_const(connection).adapter().is_open());

    connection.adapter().close();

    GLINTFX_CHECK(!connection.is_open());
    GLINTFX_CHECK(glintfx::test::fake_display_adapter::close_call_count() == 1);
}

// LOOP-RUN fatia 7 (docs/plano-w6b-fatias-6-8.md sec. 8.2): the "afirma
// que mede" witness for wait_events() - the value 37 has to atravessar
// the fake's own wait_events() and come back through last_budget_ms()
// unchanged, the same proof this project's own paridade lens applies
// to every place a function claims to read a caller-supplied number.
// Called directly through connection.adapter() (fake_display_adapter
// still lacks pump_events() on purpose, this file's own header comment
// on the fixture explains why - it never satisfies display_backend_
// port itself, and this case does not need it to).
GLINTFX_TEST(wait_events_budget_reaches_the_adapter) {
    glintfx::test::fake_display_adapter::reset();

    glintfx::platform::display_connection<glintfx::test::fake_display_adapter> connection;
    const glintfx::gltfx_rslt<void> opened = connection.open();
    GLINTFX_CHECK(opened.has_value());

    connection.adapter().arm_wait_events_return(true);
    const glintfx::gltfx_rslt<bool> waited = connection.adapter().wait_events(37);

    GLINTFX_CHECK(waited.has_value());
    GLINTFX_CHECK(waited.value());
    GLINTFX_CHECK(connection.adapter().last_budget_ms() == 37);
}

GLINTFX_TEST(a_different_injected_code_still_arrives_unchanged) {
    // Same shape as the case above, with a DIFFERENT code - proves the
    // path is generic (the wrapper does not special-case one specific
    // gltfx_err_code value), not just correct for platform_failure.
    glintfx::test::fake_display_adapter::reset();
    glintfx::test::fake_display_adapter::arm_failure(glintfx::gltfx_err_code::unsupported);

    glintfx::platform::display_connection<glintfx::test::fake_display_adapter> connection;
    const glintfx::gltfx_rslt<void> opened = connection.open();

    GLINTFX_CHECK(opened.has_error());
    GLINTFX_CHECK(opened.err().code() == glintfx::gltfx_err_code::unsupported);
}
