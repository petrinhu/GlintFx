// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/gl_load_refusal.hpp"

#include <glintfx/core/err_code.hpp>

namespace glintfx::draw2d {

glintfx::gltfx_err gl_load_refusal(const glintfx::gltfx_err &loader_error) noexcept {
    if (loader_error.code() != glintfx::gltfx_err_code::not_found) {
        return loader_error;
    }
    return glintfx::gltfx_err(glintfx::gltfx_err_code::unsupported)
        .with_rejected_value(loader_error.rejected_value());
}

} // namespace glintfx::draw2d
