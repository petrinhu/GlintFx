// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/gl/gl_surface_size_policy.hpp"

#include <glintfx/core/err_code.hpp>

namespace glintfx::platform {

gltfx_rslt<gl_surface_size_decision>
resolve_gl_surface_size(std::uint32_t window_pixel_width, std::uint32_t window_pixel_height,
                        std::uint32_t current_buffer_width,
                        std::uint32_t current_buffer_height) noexcept {
    if (window_pixel_width == 0 || window_pixel_height == 0) {
        return gltfx_rslt<gl_surface_size_decision>::err(
            gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value("pixel_size"));
    }

    if (window_pixel_width == current_buffer_width &&
        window_pixel_height == current_buffer_height) {
        return gltfx_rslt<gl_surface_size_decision>::ok(gl_surface_size_decision{
            .should_resize = false,
            .width = current_buffer_width,
            .height = current_buffer_height,
        });
    }

    return gltfx_rslt<gl_surface_size_decision>::ok(gl_surface_size_decision{
        .should_resize = true,
        .width = window_pixel_width,
        .height = window_pixel_height,
    });
}

} // namespace glintfx::platform
