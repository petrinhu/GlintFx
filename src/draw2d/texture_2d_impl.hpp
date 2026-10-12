// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

#include <glintfx/core/err.hpp>
#include <glintfx/draw2d/texture_2d_desc.hpp>

#include "gl_functions.hpp"

// draw2d/texture_2d_impl.hpp - R2D-TEXTURE, fatia T2a: what a texture IS inside the library - the
// interior of the public handle gltfx_texture_2d (texture_2d.hpp declares it, opaque) - and what it
// needs of the renderer it belongs to. Plain records, no behaviour: texture_upload.hpp makes and
// releases them.
namespace glintfx {

namespace draw2d {

// What making or releasing a texture needs of its renderer: the context to make current before
// GL (the rule begin_frame() follows), and the renderer's own GL function table. `owner` names the
// renderer the texture belongs to; nothing reads through it, and nothing in this fatia compares it
// yet (the refusal of a texture of another renderer comes with the drawing calls).
struct texture_gl_access {
    void *context = nullptr;
    gltfx_rslt<void> (*make_current)(void *context) noexcept = nullptr;
    const render::gl_function_table *gl = nullptr;
    const void *owner = nullptr;
};

} // namespace draw2d

struct texture_2d_impl {
    draw2d::texture_gl_access access;
    std::uint32_t gl_name = 0; // the GL texture object
    std::uint32_t width = 0;   // texels of level 0
    std::uint32_t height = 0;
    gltfx_texture_filter filter = gltfx_texture_filter::linear;
    std::uint32_t reduced_levels = 0; // levels 1 to reduced_levels exist on the card
};

} // namespace glintfx
