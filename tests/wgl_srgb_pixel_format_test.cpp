// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstddef>
#include <iterator>
#include <print>

#include "platform/gl/gfx_format_decision.hpp"
#include "platform/win32/wgl_srgb_pixel_format.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// wgl_srgb_pixel_format_test.cpp - D-SRGB-2 (D-SRGB2-9): the pure rule the Windows adapter applies
// to the sRGB framebuffer. It sees no Windows header, so it runs on every system. The expected
// values are written from the specification (WGL_ARB_framebuffer_sRGB, WGL_EXT_framebuffer_sRGB)
// and from D-A62, never read back from the code.

using glintfx::gltfx_gfx_option_support;
using glintfx::platform::decide_gfx_format;
using glintfx::platform::gfx_format_facts;
using glintfx::platform::gfx_format_refusal;
using glintfx::platform::k_wgl_framebuffer_srgb_capable_arb;
using glintfx::platform::srgb_option_support;
using glintfx::platform::wgl_framebuffer_srgb_advertised;
using glintfx::platform::wgl_srgb_confirmed;

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
    constexpr int k_lists_written = 4; // declared apart: the loop must reach every one of them
    int checked = 0;
    for (std::size_t i = 0; i < std::size(lists); ++i) {
        GLINTFX_CHECK(wgl_framebuffer_srgb_advertised(lists[i]) == expected[i]);
        ++checked;
    }
    std::println("wgl_srgb_pixel_format_test: {} list(s) checked", checked);
    GLINTFX_CHECK(checked == k_lists_written);
}

GLINTFX_TEST(wgl_srgb_the_attribute_is_the_value_of_the_specification) {
    // WGL_ARB_framebuffer_sRGB, "New Tokens": WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB 0x20A9
    // https://registry.khronos.org/OpenGL/extensions/ARB/WGL_ARB_framebuffer_sRGB.txt
    GLINTFX_CHECK(k_wgl_framebuffer_srgb_capable_arb == 0x20A9);
}

GLINTFX_TEST(wgl_srgb_only_a_successful_query_answering_true_confirms) {
    GLINTFX_CHECK(wgl_srgb_confirmed(true, 1));
    GLINTFX_CHECK(!wgl_srgb_confirmed(true, 0));
    GLINTFX_CHECK(!wgl_srgb_confirmed(false, 1)); // the call failed: the value is not an answer
    GLINTFX_CHECK(!wgl_srgb_confirmed(false, 0));
    GLINTFX_CHECK(!wgl_srgb_confirmed(true, 2)); // anything but TRUE (1) confirms nothing
    GLINTFX_CHECK(!wgl_srgb_confirmed(true, -1));
}

// The WGL facts the adapter hands to the neutral decision (gfx_format_decision.hpp), in the
// scenarios D-A62 names. The adapter calls the SAME decide_gfx_format() as the EGL one.
GLINTFX_TEST(wgl_srgb_the_mesa_runner_is_refused_by_name) {
    // Mesa 26.2.0 (the CI driver): the WGL list has no framebuffer_sRGB and the choose ignores the
    // attribute, so a format is "found" and could be opened. The refusal must come from the
    // announcement, never from the choose.
    const char *const list = "WGL_ARB_extensions_string WGL_ARB_pixel_format WGL_ARB_multisample";
    const gfx_format_facts facts{
        .msaa_requested = false,
        .srgb_requested = true,
        .srgb_advertised = wgl_framebuffer_srgb_advertised(list),
        .format_found = true,
        .format_found_without_srgb = true,
        .srgb_confirmed = false,
    };
    GLINTFX_CHECK(decide_gfx_format(facts) == gfx_format_refusal::srgb_framebuffer);
}

GLINTFX_TEST(wgl_srgb_the_announcement_alone_refuses_even_if_the_format_is_confirmed) {
    // The literal order of the header: the absent announcement refuses BEFORE the choose and the
    // confirmation are consulted. A format confirmed sRGB-capable does not override a driver that
    // does not announce the extension (rule 1 of decide_gfx_format, not rule 3).
    const gfx_format_facts facts{
        .msaa_requested = false,
        .srgb_requested = true,
        .srgb_advertised = wgl_framebuffer_srgb_advertised("WGL_ARB_pixel_format"),
        .format_found = true,
        .format_found_without_srgb = true,
        .srgb_confirmed = true,
    };
    GLINTFX_CHECK(decide_gfx_format(facts) == gfx_format_refusal::srgb_framebuffer);
}

GLINTFX_TEST(wgl_srgb_an_announced_extension_with_an_unconfirmed_format_is_refused) {
    // The choose accepted the attribute, the driver announces the extension, and the query on the
    // format that will be bound says FALSE: still refused as sRGB.
    const gfx_format_facts facts{
        .msaa_requested = false,
        .srgb_requested = true,
        .srgb_advertised = wgl_framebuffer_srgb_advertised("WGL_EXT_framebuffer_sRGB"),
        .format_found = true,
        .format_found_without_srgb = true,
        .srgb_confirmed = wgl_srgb_confirmed(true, 0),
    };
    GLINTFX_CHECK(decide_gfx_format(facts) == gfx_format_refusal::srgb_framebuffer);
}

GLINTFX_TEST(wgl_srgb_an_announced_and_confirmed_format_opens) {
    const gfx_format_facts facts{
        .msaa_requested = false,
        .srgb_requested = true,
        .srgb_advertised = wgl_framebuffer_srgb_advertised("WGL_ARB_framebuffer_sRGB"),
        .format_found = true,
        .format_found_without_srgb = true,
        .srgb_confirmed = wgl_srgb_confirmed(true, 1),
    };
    GLINTFX_CHECK(decide_gfx_format(facts) == gfx_format_refusal::none);
    GLINTFX_CHECK(srgb_option_support(true, facts.srgb_advertised, facts.srgb_confirmed) ==
                  gltfx_gfx_option_support::supported);
}

GLINTFX_TEST(wgl_srgb_not_asked_the_support_is_the_announcement) {
    // D-SRGB2-1: the same rule as the EGL side, so option_support(srgb_framebuffer) cannot differ.
    GLINTFX_CHECK(srgb_option_support(false,
                                      wgl_framebuffer_srgb_advertised("WGL_ARB_framebuffer_sRGB"),
                                      false) == gltfx_gfx_option_support::supported);
    GLINTFX_CHECK(srgb_option_support(false,
                                      wgl_framebuffer_srgb_advertised("WGL_ARB_pixel_format"),
                                      true) == gltfx_gfx_option_support::unsupported_here);
}
