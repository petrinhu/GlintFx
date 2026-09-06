// SPDX-License-Identifier: AGPL-3.0-or-later
#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <utility>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/platform/window/display.hpp>
#include <glintfx/platform/window/window.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/window/display_impl.hpp"
#include "platform/window/window_impl.hpp"

// win32_facade_pin_test.cpp - FACADE-PIN, T4 (docs/plano-conserto-
// fachadas-uaf.md sec. 8): the Windows counterpart of facade_pin_
// smoke.cpp (tests/container/, T0) - the same measurement (does the
// pointer the operating system was told to call back into still point
// at the object that is actually alive), through GWLP_USERDATA/
// GetWindowLongPtrW instead of wl_proxy_get_user_data() (GODS_LAWS.md
// L-04: mechanism differs, coverage does not). Paired with T0 by name
// in tests/parity_aliases.txt ("facade_pin_smoke|win32_facade_pin_
// test").
//
// WHY THIS FILE DOES NOT LINK glintfx::glintfx (unlike window_parity_
// test.cpp/gl_context_parity_test.cpp one directory over): this test
// needs BOTH the public API (gltfx_display::open()/gltfx_window::
// open(), which DO carry GLINTFX_API and are exported from glintfx.
// dll) AND the two internal passkeys display_internal_access::get()/
// window_internal_access::get() (declared in the public header, but
// DEFINED only in display_facade.cpp/window_facade.cpp - GODS_LAWS.md
// L-19, "no GLINTFX_API on this line... never placed in its dynamic
// symbol table"). Linking against the packaged DLL would leave those
// two symbols permanently unresolved (a consumer's own TU can never
// call them, by design - display.hpp's own header comment). This file
// recompiles the WHOLE chain those two public factories and passkeys
// need instead - display_facade.cpp/window_facade.cpp themselves, the
// win32 adapters they PIMPL over, and their own shared dependencies -
// exactly the "no CMake/pacote, tudo standalone" shape facade_pin_
// smoke.cpp already uses on the Linux side (tests/container/
// Containerfile, plain g++, never glintfx::glintfx there either).
// tests/CMakeLists.txt's own comment on this target lists every file.

GLINTFX_TEST(window_gwlp_userdata_points_at_the_live_adapter) {
    glintfx::gltfx_rslt<glintfx::gltfx_display> display_opened = glintfx::gltfx_display::open();
    GLINTFX_CHECK(display_opened.has_value());
    glintfx::gltfx_display display = std::move(display_opened.value());

    const glintfx::gltfx_window_desc desc{
        .title = "win32_facade_pin_test",
        .application_id = "org.glintfx.win32_facade_pin_test",
        .logical_size = {.width = 640, .height = 480},
    };
    glintfx::gltfx_rslt<glintfx::gltfx_window> window_opened =
        glintfx::gltfx_window::open(display, desc);
    GLINTFX_CHECK(window_opened.has_value());
    glintfx::gltfx_window window = std::move(window_opened.value());

    glintfx::window_impl *w_impl = glintfx::window_internal_access::get(window);

    // GetWindowLongPtrW returns a LONG_PTR - the same reinterpret_cast
    // display_adapter.cpp's own window_proc already carries for the
    // identical Win32-mandated integer-typed return (see that file's
    // own comment on the WM_SIZE/WM_CLOSE/WM_ACTIVATE/WM_DPICHANGED
    // branch for why this cast is unavoidable at this boundary).
    // NOLINTNEXTLINE(performance-no-int-to-ptr) reason: see comment above
    const void *user_data = reinterpret_cast<const void *>(
        ::GetWindowLongPtrW(w_impl->adapter.native_handle(), GWLP_USERDATA));

    GLINTFX_CHECK(user_data == static_cast<const void *>(&w_impl->adapter));
}

GLINTFX_TEST(display_message_only_window_gwlp_userdata_points_at_the_live_adapter) {
    // T0's own #1/#8 pair, reproduced here through the display's own
    // message-only window (win32_display_adapter::native_handle(),
    // display_adapter.hpp:174 in this plan's own citation) - the FIRST
    // of the ten sites this plan's own varredura confirmed latent by
    // reading, never executed on a real Windows runner until this test.
    glintfx::gltfx_rslt<glintfx::gltfx_display> display_opened = glintfx::gltfx_display::open();
    GLINTFX_CHECK(display_opened.has_value());
    glintfx::gltfx_display display = std::move(display_opened.value());

    glintfx::display_impl *d_impl = glintfx::display_internal_access::get(display);

    // NOLINTNEXTLINE(performance-no-int-to-ptr) reason: see the sibling case above
    const void *user_data = reinterpret_cast<const void *>(
        ::GetWindowLongPtrW(d_impl->connection.adapter().native_handle(), GWLP_USERDATA));

    GLINTFX_CHECK(user_data == static_cast<const void *>(&d_impl->connection.adapter()));
}

#endif // defined(_WIN32)
