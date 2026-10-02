// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstddef>
#include <print>
#include <string_view>

#include "platform/extension_token.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// extension_token_test.cpp - D-SRGB-2 (D-SRGB2-6, slice S1): the one atom that asks "is this
// extension listed?" of a space-separated list, by the WHOLE token. It runs on every system (no
// platform guard): the Windows side reads WGL lists with the same atom. The case that matters most
// is the one a substring search gets wrong: EGL_MESA_device_software_x is a different extension
// from EGL_MESA_device_software.

using glintfx::platform::extension_token_listed;

GLINTFX_TEST(extension_token_a_token_is_listed_by_its_whole_name) {
    const char *list = "EGL_KHR_image_base EGL_KHR_gl_colorspace EGL_EXT_buffer_age";
    GLINTFX_CHECK(extension_token_listed(list, "EGL_KHR_gl_colorspace"));
    GLINTFX_CHECK(extension_token_listed(list, "EGL_KHR_image_base")); // the first token
    GLINTFX_CHECK(extension_token_listed(list, "EGL_EXT_buffer_age")); // the last token
    GLINTFX_CHECK(extension_token_listed("EGL_KHR_gl_colorspace", "EGL_KHR_gl_colorspace"));
}

GLINTFX_TEST(extension_token_a_longer_token_never_matches_a_shorter_name) {
    // The device query used to strstr() for EGL_MESA_device_software: this is the case it got
    // wrong.
    GLINTFX_CHECK(
        !extension_token_listed("EGL_MESA_device_software_x", "EGL_MESA_device_software"));
    GLINTFX_CHECK(!extension_token_listed("EGL_EXT_a EGL_MESA_device_software_x EGL_EXT_b",
                                          "EGL_MESA_device_software"));
    GLINTFX_CHECK(!extension_token_listed("EGL_KHR_gl_colorspace_x", "EGL_KHR_gl_colorspace"));
    GLINTFX_CHECK(!extension_token_listed("XEGL_KHR_gl_colorspace", "EGL_KHR_gl_colorspace"));
    // Tokens are case-sensitive: a token that differs only by case is a different name.
    GLINTFX_CHECK(!extension_token_listed("EGL_khr_gl_colorspace", "EGL_KHR_gl_colorspace"));
}

GLINTFX_TEST(extension_token_a_prefix_of_a_listed_token_never_matches) {
    const char *list = "EGL_KHR_image_base EGL_KHR_gl_colorspace";
    GLINTFX_CHECK(!extension_token_listed("EGL_KHR_gl", "EGL_KHR_gl_colorspace")); // list shorter
    GLINTFX_CHECK(!extension_token_listed(list, "EGL_KHR_gl"));                    // name shorter
    GLINTFX_CHECK(!extension_token_listed(list, "EGL_KHR_image_bas"));
    GLINTFX_CHECK(!extension_token_listed(list, "image_base")); // a suffix is no match either
}

GLINTFX_TEST(extension_token_repeated_and_edge_spaces_are_skipped) {
    GLINTFX_CHECK(extension_token_listed("  EGL_KHR_gl_colorspace  ", "EGL_KHR_gl_colorspace"));
    GLINTFX_CHECK(extension_token_listed("EGL_A    EGL_B", "EGL_B"));
    GLINTFX_CHECK(extension_token_listed("EGL_A    EGL_B", "EGL_A"));
    GLINTFX_CHECK(!extension_token_listed("   ", "EGL_A"));
}

GLINTFX_TEST(extension_token_a_null_or_empty_list_and_an_empty_name_list_nothing) {
    GLINTFX_CHECK(!extension_token_listed(nullptr, "EGL_KHR_gl_colorspace"));
    GLINTFX_CHECK(!extension_token_listed("", "EGL_KHR_gl_colorspace"));
    GLINTFX_CHECK(!extension_token_listed("EGL_A EGL_B", ""));
    GLINTFX_CHECK(!extension_token_listed("", ""));
    GLINTFX_CHECK(!extension_token_listed(nullptr, ""));
}

GLINTFX_TEST(extension_token_the_wgl_names_of_srgb_are_told_apart_by_whole_token) {
    // WGL_ARB_framebuffer_sRGB is the extension; WGL_ARB_framebuffer_sRGBx would be another.
    GLINTFX_CHECK(extension_token_listed("WGL_ARB_extensions_string WGL_EXT_framebuffer_sRGB",
                                         "WGL_EXT_framebuffer_sRGB"));
    GLINTFX_CHECK(!extension_token_listed("WGL_ARB_framebuffer_sRGBx", "WGL_ARB_framebuffer_sRGB"));
    GLINTFX_CHECK(!extension_token_listed("WGL_EXT_swap_control", "WGL_EXT_swap_control_tear"));
}

// L-40: a sweep that counts. Every (list, name, expected) cell below is checked and counted, and
// the test refuses to pass on fewer than the table has.
GLINTFX_TEST(extension_token_the_whole_table_is_swept_and_counted) {
    struct cell {
        const char *list = nullptr;
        std::string_view name{};
        bool expected = false;
    };
    const cell table[] = {
        {"A B C", "A", true},  {"A B C", "B", true},
        {"A B C", "C", true},  {"A B C", "D", false},
        {"AB", "A", false},    {"A", "AB", false},
        {"A_x B", "A", false}, {"B A_x", "A", false},
        {"A  B", "B", true},   {" A", "A", true},
        {"A ", "A", true},     {"", "A", false},
        {nullptr, "A", false}, {"A", "", false},
        {"A_B", "B", false},   {"EGL_khr_gl_colorspace", "EGL_KHR_gl_colorspace", false},
    };
    std::size_t swept = 0;
    for (const cell &c : table) {
        GLINTFX_CHECK_EQ(extension_token_listed(c.list, c.name), c.expected);
        ++swept;
    }
    std::println("extension_token_test: {} cell(s) swept of {}", swept,
                 sizeof(table) / sizeof(table[0]));
    GLINTFX_CHECK_EQ(swept, std::size_t{16}); // zero swept would be a broken sweep, never "clean"
}
