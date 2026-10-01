// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/wayland/egl_surface_colorspace.hpp"

namespace glintfx::platform {

namespace {
constexpr std::int32_t k_egl_surface_type = 0x3033;
constexpr std::int32_t k_egl_window_bit = 0x0004;
constexpr std::int32_t k_egl_renderable_type = 0x3040;
constexpr std::int32_t k_egl_opengl_bit = 0x0008;
constexpr std::int32_t k_egl_red_size = 0x3024;
constexpr std::int32_t k_egl_green_size = 0x3023;
constexpr std::int32_t k_egl_blue_size = 0x3022;
constexpr std::int32_t k_egl_alpha_size = 0x3021;
constexpr std::int32_t k_egl_stencil_size = 0x3026;
} // namespace

choose_attribs make_choose_attribs(std::int64_t samples) noexcept {
    choose_attribs attribs{};
    attribs.fill(k_egl_none);
    const std::int32_t base[] = {
        k_egl_surface_type,    k_egl_window_bit,
        k_egl_renderable_type, k_egl_opengl_bit,
        k_egl_red_size,        8,
        k_egl_green_size,      8,
        k_egl_blue_size,       8,
        k_egl_alpha_size,      8,
        k_egl_stencil_size,    8,
    };
    std::size_t next = 0;
    for (const std::int32_t entry : base) {
        attribs[next++] = entry;
    }
    if (samples > 0) {
        attribs[next++] = k_egl_samples;
        attribs[next++] = static_cast<std::int32_t>(samples);
    }
    attribs[next] = k_egl_none;
    return attribs;
}

surface_attribs make_surface_attribs(bool srgb_requested, bool extension_present) noexcept {
    if (srgb_requested && extension_present) {
        return {k_egl_gl_colorspace, k_egl_gl_colorspace_srgb, k_egl_none};
    }
    return {k_egl_none, k_egl_none, k_egl_none};
}

bool has_egl_extension_token(std::string_view extensions, std::string_view token) noexcept {
    if (token.empty()) {
        return false;
    }
    std::size_t from = 0;
    while (from < extensions.size()) {
        const std::size_t found = extensions.find(token, from);
        if (found == std::string_view::npos) {
            return false;
        }
        const std::size_t end = found + token.size();
        const bool starts_token = found == 0 || extensions[found - 1] == ' ';
        const bool ends_token = end == extensions.size() || extensions[end] == ' ';
        if (starts_token && ends_token) {
            return true;
        }
        from = found + 1;
    }
    return false;
}

bool srgb_surface_honored(std::int32_t queried_colorspace) noexcept {
    return queried_colorspace == k_egl_gl_colorspace_srgb;
}

} // namespace glintfx::platform
