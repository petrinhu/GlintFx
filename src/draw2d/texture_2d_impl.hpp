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

struct texture_2d_impl;

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
    // The door of the renderer's release queue (T3, D-W8-288): `hold` takes the record of a texture
    // destroyed while that renderer has a frame open, and the renderer releases it when the frame
    // ends (texture_release_queue.hpp); it answers false when no frame is open, and the caller
    // releases at once. Null in a record made without a renderer (a test's).
    bool (*hold)(void *queue, texture_2d_impl *texture) noexcept = nullptr;
    void *queue = nullptr;
};

} // namespace draw2d

struct texture_2d_impl {
    draw2d::texture_gl_access access;
    std::uint32_t gl_name = 0; // the GL texture object
    std::uint32_t width = 0;   // texels of level 0
    std::uint32_t height = 0;
    gltfx_texture_filter filter = gltfx_texture_filter::linear;
    std::uint32_t reduced_levels = 0; // levels 1 to reduced_levels exist on the card
    // The next record held by its renderer for release (T3, texture_release_queue.hpp).
    texture_2d_impl *next_held = nullptr;
    // Whether update_texture() may change the texture: gltfx_texture_2d_desc::updatable, copied
    // by create_texture_object() (T3-C). False in a record that did not declare it, and the update
    // is refused by name (texture_update_check.hpp).
    bool updatable = false;
};

} // namespace glintfx
