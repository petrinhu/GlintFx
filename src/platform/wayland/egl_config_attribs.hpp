// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <EGL/egl.h>
#include <EGL/eglext.h>

// platform/wayland/egl_config_attribs.hpp - D-SRGB-1 (errata sec. 28): the attribute lists the EGL
// adapter hands to EGL, and the test of the extension list, as PURE functions over plain data, so a
// test can prove what EGL is asked WITHOUT a driver. Header-only on purpose: every fixture line
// that links egl_context_adapter.cpp gets it with no source list to edit.
//
// WHY THE COLORSPACE IS NOT IN THE CHOOSE LIST: EGL_GL_COLORSPACE_KHR is an attribute of a SURFACE,
// accepted by eglCreateWindowSurface (and the Pbuffer and Pixmap variants), never by
// eglChooseConfig - EGL_KHR_gl_colorspace, "New Tokens"
// (https://registry.khronos.org/EGL/extensions/KHR/EGL_KHR_gl_colorspace.txt). Mesa answers a
// choose list that carries it with EGL_BAD_ATTRIBUTE, which this adapter used to read as "the
// driver has no sRGB": on every Mesa, srgb_framebuffer=on never worked on Linux.
namespace glintfx::platform {

// A list of EGL attributes, terminated by EGL_NONE, in a fixed array (this path runs once per
// context and never allocates).
struct egl_attrib_list {
    std::array<EGLint, 20> values{};
    std::size_t count = 0; // entries written, the terminating EGL_NONE included
};

// What eglChooseConfig is asked: a window surface, an OpenGL context, RGBA8 and stencil8 (D-W6b-4),
// plus EGL_SAMPLES when `samples` is positive. NEVER a colorspace.
[[nodiscard]] inline egl_attrib_list egl_choose_config_attribs(std::int64_t samples) noexcept {
    egl_attrib_list list;
    std::size_t next = 0;
    const auto put = [&](EGLint name, EGLint value) {
        list.values[next++] = name;
        list.values[next++] = value;
    };
    put(EGL_SURFACE_TYPE, EGL_WINDOW_BIT);
    put(EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT);
    put(EGL_RED_SIZE, 8);
    put(EGL_GREEN_SIZE, 8);
    put(EGL_BLUE_SIZE, 8);
    put(EGL_ALPHA_SIZE, 8);
    put(EGL_STENCIL_SIZE, 8);
    if (samples > 0) {
        put(EGL_SAMPLES, static_cast<EGLint>(samples));
    }
    list.values[next++] = EGL_NONE;
    list.count = next;
    return list;
}

// What eglCreateWindowSurface is asked: with `srgb`, the sRGB colorspace of EGL_KHR_gl_colorspace;
// without it, nothing (the default, linear, colorspace).
[[nodiscard]] inline egl_attrib_list egl_window_surface_attribs(bool srgb) noexcept {
    egl_attrib_list list;
    std::size_t next = 0;
    if (srgb) {
        list.values[next++] = EGL_GL_COLORSPACE_KHR;
        list.values[next++] = EGL_GL_COLORSPACE_SRGB_KHR;
    }
    list.values[next++] = EGL_NONE;
    list.count = next;
    return list;
}

// Whether `name` is one of the space-separated tokens of an EGL extension string: the WHOLE token,
// never a substring of a longer one (EGL_KHR_gl_colorspace_x is not EGL_KHR_gl_colorspace). A null
// list has none.
[[nodiscard]] inline bool egl_extension_listed(const char *extensions,
                                               std::string_view name) noexcept {
    if (extensions == nullptr || name.empty()) {
        return false;
    }
    const std::string_view list(extensions);
    std::size_t start = 0;
    while (start < list.size()) {
        while (start < list.size() && list[start] == ' ') {
            ++start;
        }
        std::size_t end = start;
        while (end < list.size() && list[end] != ' ') {
            ++end;
        }
        if (list.substr(start, end - start) == name) {
            return true;
        }
        start = end;
    }
    return false;
}

} // namespace glintfx::platform
