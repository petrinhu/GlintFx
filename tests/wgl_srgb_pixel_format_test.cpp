// SPDX-License-Identifier: AGPL-3.0-or-later
#include <print>

#include "platform/win32/wgl_srgb_pixel_format.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// wgl_srgb_pixel_format_test.cpp - D-SRGB-2 (D-SRGB2-9): the pure rule the Windows adapter applies
// to the sRGB framebuffer. It sees no Windows header, so it runs on every system. The expected
// values are written from the specification (WGL_ARB_framebuffer_sRGB, WGL_EXT_framebuffer_sRGB)
// and from D-A62, never read back from the code.

using glintfx::platform::wgl_framebuffer_srgb_advertised;

GLINTFX_TEST(wgl_srgb_the_arb_token_is_an_announcement) {
    GLINTFX_CHECK(wgl_framebuffer_srgb_advertised("WGL_ARB_framebuffer_sRGB"));
    GLINTFX_CHECK(wgl_framebuffer_srgb_advertised(
        "WGL_ARB_extensions_string WGL_ARB_framebuffer_sRGB WGL_ARB_pixel_format"));
}

GLINTFX_TEST(wgl_srgb_the_ext_token_alone_is_an_announcement) {
    // The older twin: a driver that lists only the EXT name announces the sRGB framebuffer.
    GLINTFX_CHECK(wgl_framebuffer_srgb_advertised("WGL_EXT_framebuffer_sRGB"));
    GLINTFX_CHECK(wgl_framebuffer_srgb_advertised(
        "WGL_ARB_pixel_format WGL_EXT_swap_control WGL_EXT_framebuffer_sRGB"));
}

GLINTFX_TEST(wgl_srgb_a_longer_or_shorter_name_is_not_an_announcement) {
    GLINTFX_CHECK(!wgl_framebuffer_srgb_advertised("WGL_ARB_framebuffer_sRGBx"));
    GLINTFX_CHECK(!wgl_framebuffer_srgb_advertised("WGL_EXT_framebuffer_sRGB_x"));
    GLINTFX_CHECK(!wgl_framebuffer_srgb_advertised("XWGL_ARB_framebuffer_sRGB"));
    GLINTFX_CHECK(!wgl_framebuffer_srgb_advertised("WGL_ARB_framebuffer_sRG"));
    GLINTFX_CHECK(!wgl_framebuffer_srgb_advertised("WGL_ARB_framebuffer_srgb"));
}

GLINTFX_TEST(wgl_srgb_a_list_without_the_extension_announces_nothing) {
    // The Mesa 26.2.0 WGL list the CI driver gives has neither name (plan D-SRGB-2, F13).
    GLINTFX_CHECK(!wgl_framebuffer_srgb_advertised(
        "WGL_ARB_extensions_string WGL_ARB_pixel_format WGL_ARB_multisample WGL_EXT_swap_control"));
    GLINTFX_CHECK(!wgl_framebuffer_srgb_advertised(""));
    GLINTFX_CHECK(!wgl_framebuffer_srgb_advertised(nullptr));
}

GLINTFX_TEST(wgl_srgb_sweep_floor_counts_what_it_checked) {
    // L-40: a sweep that checked nothing is a broken sweep, not a clean one.
    const char *const lists[] = {
        "WGL_ARB_framebuffer_sRGB",
        "WGL_EXT_framebuffer_sRGB",
        "WGL_ARB_framebuffer_sRGBx",
        "",
    };
    const bool expected[] = {true, true, false, false};
    int checked = 0;
    for (int i = 0; i < 4; ++i) {
        GLINTFX_CHECK(wgl_framebuffer_srgb_advertised(lists[i]) == expected[i]);
        ++checked;
    }
    std::println("wgl_srgb_pixel_format_test: {} list(s) checked", checked);
    GLINTFX_CHECK(checked == 4);
}
