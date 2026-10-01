// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/wayland/egl_surface_colorspace.hpp"

// RED SKELETON (D-A59, L-20): reproduces the behaviour in force before
// the fix - colorspace inside the choose list, no surface attributes,
// substring extension search, no read-back. Replaced by the real
// implementation in the fix commit.

namespace glintfx::platform {

choose_attribs make_choose_attribs(std::int64_t samples) noexcept {
    choose_attribs attribs{};
    attribs.fill(k_egl_none);
    std::size_t next = 14;
    if (samples > 0) {
        attribs[next++] = k_egl_samples;
        attribs[next++] = static_cast<std::int32_t>(samples);
    }
    attribs[next++] = k_egl_gl_colorspace; // old form, sRGB requested
    attribs[next++] = k_egl_gl_colorspace_srgb;
    return attribs;
}

surface_attribs make_surface_attribs(bool, bool) noexcept {
    return {k_egl_none, k_egl_none, k_egl_none};
}

bool has_egl_extension_token(std::string_view extensions, std::string_view token) noexcept {
    return extensions.find(token) != std::string_view::npos;
}

bool srgb_surface_honored(std::int32_t) noexcept { return true; }

} // namespace glintfx::platform
