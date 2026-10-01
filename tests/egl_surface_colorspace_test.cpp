// SPDX-License-Identifier: AGPL-3.0-or-later
#include <algorithm>
#include <cstdint>
#include <print>
#include <string_view>

#include "platform/wayland/egl_surface_colorspace.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// egl_surface_colorspace_test.cpp - D-A59: the pure decisions behind an
// sRGB EGL window surface (egl_surface_colorspace.hpp). EGL_GL_COLORSPACE_KHR
// is a surface attribute, never a config attribute: Mesa answers
// EGL_BAD_ATTRIBUTE (0x3004) when eglChooseConfig() receives it. Every
// test prints how many cells it swept and a zero count fails
// (GODS_LAWS.md L-40).
//
// RED, SEEN: against the pre-fix behaviour (skeleton reproducing the
// colorspace inside the choose list, a substring extension search, no
// read-back) the choose, surface, token and verdict tests all fail.

using namespace glintfx::platform;

namespace {

bool contains_value(const auto &list, std::int32_t value) {
    return std::ranges::find(list, value) != list.end();
}

// Walks key/value pairs up to the first EGL_NONE key.
std::int32_t value_of(const choose_attribs &list, std::int32_t key) {
    for (std::size_t i = 0; i + 1 < list.size() && list[i] != k_egl_none; i += 2) {
        if (list[i] == key) {
            return list[i + 1];
        }
    }
    return -1;
}

bool is_none_terminated(const choose_attribs &list) {
    for (std::size_t i = 0; i < list.size(); i += 2) {
        if (list[i] == k_egl_none) {
            return true;
        }
    }
    return false;
}

} // namespace

GLINTFX_TEST(egl_choose_attribs_never_carry_the_colorspace) {
    int swept = 0;
    for (const std::int64_t samples : {0, 1, 4, 8}) {
        const choose_attribs list = make_choose_attribs(samples);
        GLINTFX_CHECK(!contains_value(list, k_egl_gl_colorspace));
        GLINTFX_CHECK(!contains_value(list, k_egl_gl_colorspace_srgb));
        GLINTFX_CHECK(is_none_terminated(list));
        // RGBA8 + stencil8 always, even when MSAA is requested (D-W6b-4).
        GLINTFX_CHECK_EQ(value_of(list, 0x3025 /* EGL_STENCIL_SIZE */), 8);
        GLINTFX_CHECK_EQ(value_of(list, 0x3021 /* EGL_ALPHA_SIZE */), 8);
        GLINTFX_CHECK_EQ(value_of(list, k_egl_samples), samples > 0 ? samples : -1);
        ++swept;
    }
    std::println("egl_choose_attribs cells swept: {}", swept);
    GLINTFX_CHECK(swept > 0);
}

GLINTFX_TEST(egl_surface_attribs_carry_the_pair_only_when_requested_and_supported) {
    int swept = 0;
    for (const bool requested : {false, true}) {
        for (const bool extension : {false, true}) {
            const surface_attribs list = make_surface_attribs(requested, extension);
            if (requested && extension) {
                GLINTFX_CHECK_EQ(list[0], k_egl_gl_colorspace);
                GLINTFX_CHECK_EQ(list[1], k_egl_gl_colorspace_srgb);
                GLINTFX_CHECK_EQ(list[2], k_egl_none);
            } else {
                GLINTFX_CHECK_EQ(list[0], k_egl_none);
            }
            ++swept;
        }
    }
    std::println("egl_surface_attribs cells swept: {}", swept);
    GLINTFX_CHECK_EQ(swept, 4);
}

GLINTFX_TEST(egl_extension_token_search_matches_whole_tokens_only) {
    constexpr std::string_view token = k_egl_gl_colorspace_extension;
    struct cell {
        std::string_view extensions;
        bool expected;
    };
    const cell cells[] = {
        {"", false},
        {"EGL_KHR_gl_colorspace", true},
        {"EGL_KHR_image_base EGL_KHR_gl_colorspace EGL_KHR_surfaceless_context", true},
        {"EGL_KHR_gl_colorspace EGL_KHR_image_base", true},
        {"EGL_KHR_image_base EGL_KHR_gl_colorspace", true},
        {"EGL_KHR_gl_colorspace_bt2020_linear", false},
        {"EGL_EXT_gl_colorspace_bt2020_pq EGL_KHR_gl_colorspace_bt2020_linear", false},
        {"XEGL_KHR_gl_colorspace", false},
        {"EGL_KHR_gl_colorspace_bt2020_linear EGL_KHR_gl_colorspace", true},
        {"EGL_KHR_gl_colorspace EGL_KHR_gl_colorspace_bt2020_linear", true},
    };
    int swept = 0;
    for (const cell &c : cells) {
        GLINTFX_CHECK_EQ(has_egl_extension_token(c.extensions, token), c.expected);
        ++swept;
    }
    std::println("egl_extension_token cells swept: {}", swept);
    GLINTFX_CHECK(swept > 0);
}

GLINTFX_TEST(egl_srgb_surface_verdict_accepts_only_srgb) {
    int swept = 0;
    GLINTFX_CHECK(srgb_surface_honored(k_egl_gl_colorspace_srgb));
    ++swept;
    for (const std::int32_t other : {k_egl_gl_colorspace_linear, 0, -1, 0x3004, k_egl_none}) {
        GLINTFX_CHECK(!srgb_surface_honored(other));
        ++swept;
    }
    std::println("egl_srgb_surface_verdict cells swept: {}", swept);
    GLINTFX_CHECK(swept > 0);
}
