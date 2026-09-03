// SPDX-License-Identifier: AGPL-3.0-or-later
#if defined(_WIN32)

#include <utility>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/port/display_connection.hpp"
#include "platform/win32/display_adapter.hpp"

// win32_display_connect_test.cpp - WIN-DISPLAY fatia 3 (TODO.md,
// GODS_LAWS.md L-04/L-09/L-20/L-22, fechar-w1.md sec. 2.2): the
// Windows counterpart of tests/display_connect_failure_test.cpp - the
// success path this time, proved for real, on the real runner. Unlike
// the Wayland refusal test, there is no known way to force
// RegisterClassExW/CreateWindowExW to fail on the windows-latest
// runner (it hands out a real, interactive window station - the same
// fact tests/win32_runner_probe_test.cpp already measured live there,
// run 33643748402); fechar-w1.md sec. 7.1 names this open question and
// defers it rather than guessing. The refusal path a caller of
// display_connection<win32_display_adapter>::connect() would see IS
// already proved, generically, by tests/display_connection_fake_test.cpp
// (GODS_LAWS.md L-22: "o erro injetado chega ao chamador intacto" -
// that proof never depended on which adapter sits behind the port).
//
// win32_display_adapter is compiled a SECOND time directly into THIS
// executable's own object set (tests/CMakeLists.txt), the same
// technique tests/display_connect_failure_test.cpp already uses for
// wayland_display_adapter: glintfx's own shared build hides everything
// without GLINTFX_API (deliberately - GODS_LAWS.md L-19/WIN-DISPLAY,
// "nada e exportado"), so a separate executable linking only against
// glintfx::glintfx could never resolve these methods across that
// boundary.
//
// Two cases: the adapter itself, directly (proves open() AND close()
// each observably, is_open() true then false again - PORT-PARITY-WIN's
// own "abre, usa, fecha" shape, fechar-w1.md sec. 1.3), and the same
// sequence through display_connection<win32_display_adapter> (proves
// the PORT wraps this adapter exactly like it wraps wayland_display_
// adapter and fake_display_adapter - GODS_LAWS.md L-19, "o resto não
// percebe" which adapter is underneath).

GLINTFX_TEST(win32_adapter_opens_and_closes_a_real_message_only_window) {
    glintfx::platform::win32_display_adapter adapter;
    GLINTFX_CHECK(!adapter.is_open());

    const glintfx::gltfx_rslt<void> opened = adapter.open();

    // "abre": reaching this line at all already proves open() is
    // noexcept (an escaped exception would have called std::terminate()
    // before this line) and did not abort the process.
    GLINTFX_CHECK(opened.has_value());
    GLINTFX_CHECK(adapter.is_open());

    // "fecha": close() is void, so "sem erro" is exactly what
    // display_connect_failure_test.cpp's own closing comment already
    // documents for its destructor path - reaching the check below at
    // all, with is_open() now false, is the proof.
    adapter.close();
    GLINTFX_CHECK(!adapter.is_open());

    // Idempotent: a second close() on an already-closed adapter must
    // not crash or double-free the OS resources it already released -
    // same idempotence wayland_display_adapter::close() documents.
    adapter.close();
    GLINTFX_CHECK(!adapter.is_open());
}

GLINTFX_TEST(win32_adapter_connects_through_the_port_wrapper) {
    glintfx::gltfx_rslt<
        glintfx::platform::display_connection<glintfx::platform::win32_display_adapter>>
        connected = glintfx::platform::display_connection<
            glintfx::platform::win32_display_adapter>::connect();

    GLINTFX_CHECK(connected.has_value());
    GLINTFX_CHECK(connected.value().is_open());

    // Destroying the display_connection value closes the wrapped
    // adapter via RAII - same shape display_connection_fake_test.cpp's
    // own scoped-move block proves for the fake adapter, minus the
    // static call counters this production adapter has no need for.
    {
        const glintfx::gltfx_rslt<
            glintfx::platform::display_connection<glintfx::platform::win32_display_adapter>>
            scoped = std::move(connected);
    }
}

#endif // defined(_WIN32)
