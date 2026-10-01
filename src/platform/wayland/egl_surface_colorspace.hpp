// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>
#include <cstdint>
#include <string_view>

// platform/wayland/egl_surface_colorspace.hpp - D-A59: the PURE
// decisions behind an sRGB EGL window surface. EGL_KHR_gl_colorspace
// defines EGL_GL_COLORSPACE_KHR as an attribute of eglCreateWindow
// Surface() ONLY; eglChooseConfig() rejects it with EGL_BAD_ATTRIBUTE
// (0x3004, measured on Mesa/llvmpipe). So the config choice never
// mentions the colorspace, the surface creation does, and the result is
// CONFIRMED by eglQuerySurface() - a linear surface is never labelled
// sRGB, and a request is never degraded in silence (D-W6b-16/17).
//
// Takes and returns plain integers only: no EGL call, no EGL header, so
// it compiles and is tested on all five platforms. The constants below
// are the Khronos values (EGL 1.5 egl.h and EGL_KHR_gl_colorspace
// registry text), spelled here so this file needs no <EGL/eglext.h>.

namespace glintfx::platform {

inline constexpr std::int32_t k_egl_none = 0x3038;                 // EGL_NONE
inline constexpr std::int32_t k_egl_samples = 0x3031;              // EGL_SAMPLES
inline constexpr std::int32_t k_egl_gl_colorspace = 0x309D;        // EGL_GL_COLORSPACE_KHR
inline constexpr std::int32_t k_egl_gl_colorspace_srgb = 0x3089;   // EGL_GL_COLORSPACE_SRGB_KHR
inline constexpr std::int32_t k_egl_gl_colorspace_linear = 0x308A; // EGL_GL_COLORSPACE_LINEAR_KHR

inline constexpr std::string_view k_egl_gl_colorspace_extension = "EGL_KHR_gl_colorspace";

// RGBA8 + stencil8 always (D-W6b-4), plus EGL_SAMPLES when samples > 0,
// EGL_NONE-terminated. Never carries EGL_GL_COLORSPACE_KHR.
inline constexpr std::size_t k_choose_attrib_capacity = 18;
using choose_attribs = std::array<std::int32_t, k_choose_attrib_capacity>;

// Either {EGL_GL_COLORSPACE_KHR, EGL_GL_COLORSPACE_SRGB_KHR, EGL_NONE}
// or {EGL_NONE, EGL_NONE, EGL_NONE} (the empty list).
using surface_attribs = std::array<std::int32_t, 3>;

[[nodiscard]] choose_attribs make_choose_attribs(std::int64_t samples) noexcept;

[[nodiscard]] surface_attribs make_surface_attribs(bool srgb_requested,
                                                   bool extension_present) noexcept;

// Whole-token search in the space-separated EGL_EXTENSIONS string:
// "EGL_KHR_gl_colorspace_bt2020_linear" does not contain the token
// "EGL_KHR_gl_colorspace".
[[nodiscard]] bool has_egl_extension_token(std::string_view extensions,
                                           std::string_view token) noexcept;

// True only for EGL_GL_COLORSPACE_SRGB_KHR as read back by
// eglQuerySurface(EGL_GL_COLORSPACE_KHR).
[[nodiscard]] bool srgb_surface_honored(std::int32_t queried_colorspace) noexcept;

} // namespace glintfx::platform
