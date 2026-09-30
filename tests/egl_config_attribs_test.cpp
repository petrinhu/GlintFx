// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstddef>
#include <cstdint>
#include <print>

#include "platform/wayland/egl_config_attribs.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// egl_config_attribs_test.cpp - D-SRGB-1 (errata sec. 28): what the EGL adapter ASKS of EGL, proved
// without a driver. The colorspace is a SURFACE attribute (EGL_KHR_gl_colorspace, "New Tokens"):
// eglChooseConfig must never receive it (Mesa refuses it with EGL_BAD_ATTRIBUTE, and the adapter
// read that as "no sRGB"), and eglCreateWindowSurface must receive it exactly when the option is
// on.

using glintfx::platform::egl_attrib_list;
using glintfx::platform::egl_choose_config_attribs;
using glintfx::platform::egl_extension_listed;
using glintfx::platform::egl_window_surface_attribs;

namespace {

// The value paired with `name`, or -1 when `name` is not in the list (walking NAME VALUE pairs up
// to the terminator).
int value_of(const egl_attrib_list &list, EGLint name) {
    for (std::size_t i = 0; i + 1 < list.count; i += 2) {
        if (list.values[i] == EGL_NONE) {
            break;
        }
        if (list.values[i] == name) {
            return static_cast<int>(list.values[i + 1]);
        }
    }
    return -1;
}

bool terminated(const egl_attrib_list &list) {
    return list.count > 0 && list.count <= list.values.size() &&
           list.values[list.count - 1] == EGL_NONE;
}

} // namespace

GLINTFX_TEST(egl_config_attribs_choose_config_never_carries_a_colorspace) {
    for (const std::int64_t samples : {std::int64_t{0}, std::int64_t{4}, std::int64_t{16}}) {
        const egl_attrib_list list = egl_choose_config_attribs(samples);
        GLINTFX_CHECK(terminated(list));
        GLINTFX_CHECK_EQ(value_of(list, EGL_GL_COLORSPACE_KHR), -1);
    }
}

GLINTFX_TEST(egl_config_attribs_choose_config_asks_rgba8_stencil8_and_samples_only_when_positive) {
    const egl_attrib_list plain = egl_choose_config_attribs(0);
    GLINTFX_CHECK_EQ(value_of(plain, EGL_SURFACE_TYPE), static_cast<int>(EGL_WINDOW_BIT));
    GLINTFX_CHECK_EQ(value_of(plain, EGL_RENDERABLE_TYPE), static_cast<int>(EGL_OPENGL_BIT));
    GLINTFX_CHECK_EQ(value_of(plain, EGL_RED_SIZE), 8);
    GLINTFX_CHECK_EQ(value_of(plain, EGL_GREEN_SIZE), 8);
    GLINTFX_CHECK_EQ(value_of(plain, EGL_BLUE_SIZE), 8);
    GLINTFX_CHECK_EQ(value_of(plain, EGL_ALPHA_SIZE), 8);
    GLINTFX_CHECK_EQ(value_of(plain, EGL_STENCIL_SIZE), 8); // D-W6b-4: stencil8 is asked, always
    GLINTFX_CHECK_EQ(value_of(plain, EGL_SAMPLES), -1);
    const egl_attrib_list multisampled = egl_choose_config_attribs(4);
    GLINTFX_CHECK_EQ(value_of(multisampled, EGL_SAMPLES), 4);
    GLINTFX_CHECK_EQ(value_of(multisampled, EGL_STENCIL_SIZE),
                     8); // samples never overwrite the stencil
    GLINTFX_CHECK(terminated(multisampled));
}

GLINTFX_TEST(
    egl_config_attribs_window_surface_gets_the_srgb_colorspace_exactly_when_the_option_is_on) {
    const egl_attrib_list on = egl_window_surface_attribs(true);
    GLINTFX_CHECK(terminated(on));
    GLINTFX_CHECK_EQ(value_of(on, EGL_GL_COLORSPACE_KHR),
                     static_cast<int>(EGL_GL_COLORSPACE_SRGB_KHR));
    const egl_attrib_list off = egl_window_surface_attribs(false);
    GLINTFX_CHECK(terminated(off));
    GLINTFX_CHECK_EQ(value_of(off, EGL_GL_COLORSPACE_KHR),
                     -1);                        // the default, linear colorspace: nothing asked
    GLINTFX_CHECK_EQ(off.count, std::size_t{1}); // just the terminator
}

GLINTFX_TEST(egl_config_attribs_an_extension_is_listed_by_the_whole_token) {
    const char *list = "EGL_KHR_image_base EGL_KHR_gl_colorspace EGL_EXT_buffer_age";
    GLINTFX_CHECK(egl_extension_listed(list, "EGL_KHR_gl_colorspace"));
    GLINTFX_CHECK(egl_extension_listed(list, "EGL_KHR_image_base"));       // the first token
    GLINTFX_CHECK(egl_extension_listed(list, "EGL_EXT_buffer_age"));       // the last token
    GLINTFX_CHECK(!egl_extension_listed(list, "EGL_KHR_gl_colorspace_x")); // never a longer name
    GLINTFX_CHECK(!egl_extension_listed("EGL_KHR_gl_colorspace_x", "EGL_KHR_gl_colorspace"));
    GLINTFX_CHECK(!egl_extension_listed("EGL_KHR_gl", "EGL_KHR_gl_colorspace")); // never a prefix
    GLINTFX_CHECK(!egl_extension_listed(list, "EGL_KHR_gl")); // nor a prefix of a listed one
    GLINTFX_CHECK(!egl_extension_listed("", "EGL_KHR_gl_colorspace"));
    GLINTFX_CHECK(!egl_extension_listed(nullptr, "EGL_KHR_gl_colorspace"));
    GLINTFX_CHECK(!egl_extension_listed(list, ""));
    GLINTFX_CHECK(
        egl_extension_listed("  EGL_KHR_gl_colorspace  ", "EGL_KHR_gl_colorspace")); // extra spaces
}
