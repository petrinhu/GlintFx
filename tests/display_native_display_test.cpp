// SPDX-License-Identifier: AGPL-3.0-or-later
#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/wayland/display_adapter.hpp"

// display_native_display_test.cpp - GL-CONTEXT fatia 1 (docs/plano-
// w6b-placa-e-laco.md, G-1): the EGL probe egl_probe_smoke.cpp needs
// the raw wl_display* a wayland_display_adapter owns to call
// eglGetPlatformDisplay(EGL_PLATFORM_WAYLAND_KHR, ...) - the same
// pointer window_adapter.hpp's own surface() and win32's own
// native_handle() already expose for the analogous "hand the raw
// platform handle to something outside this adapter's own port
// contract" need (F2, docs/plano-w6b-placa-e-laco.md sec. 0).
//
// No compositor needed for THIS file - same shape as
// display_bind_refusal_test.cpp one directory over (its own header
// comment: "no compositor required, no container required"): the ONE
// thing provable without a real Wayland socket is that a NEVER-OPENED
// adapter reports the null handle is_open() already reports false
// for. The non-null case (native_display() after a REAL open()) is
// what egl_probe_smoke.cpp itself proves, inside the container
// (GODS_LAWS.md L-09) - this file is not the place a real connection
// could be opened from anyway (that needs a live compositor socket).
//
// wayland_display_adapter is compiled a SECOND time directly into
// this executable's own object set (tests/CMakeLists.txt), same
// technique display_bind_refusal_test.cpp/global_catalog_test.cpp
// already use for a symbol with no GLINTFX_API (GODS_LAWS.md
// L-19/ARCH-PORTS).

GLINTFX_TEST(native_display_is_null_before_open) {
    const glintfx::platform::wayland_display_adapter adapter;
    GLINTFX_CHECK(!adapter.is_open());
    GLINTFX_CHECK(adapter.native_display() == nullptr);
}
