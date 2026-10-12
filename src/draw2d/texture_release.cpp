// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/texture_release.hpp"

#include <glintfx/core/err.hpp>

#include "gl_functions.hpp"

// draw2d/texture_release.cpp - R2D-TEXTURE, fatia T2a-fix1: the release of one texture's GL object
// and its record. The release makes the texture's context current before it deletes the GL object;
// if that context cannot be made current, nothing is deleted through it (the object goes with the
// context). The record is freed in every case.
namespace glintfx::draw2d {

void release_texture_object(glintfx::texture_2d_impl *texture) noexcept {
    if (texture == nullptr) {
        return;
    }
    const glintfx::gltfx_rslt<void> current = texture->access.make_current(texture->access.context);
    if (current.has_value()) {
        const render::GLuint name = texture->gl_name;
        texture->access.gl->glDeleteTextures(1, &name);
    }
    delete texture;
}

} // namespace glintfx::draw2d
