// SPDX-License-Identifier: AGPL-3.0-or-later
#include <glintfx/draw2d/renderer_2d.hpp>

#include <cassert>

#include "draw2d/renderer_2d_impl.hpp"
#include "draw2d/texture_2d_impl.hpp"
#include "draw2d/texture_upload.hpp"

// renderer_2d_facade_textures.cpp - R2D-TEXTURE, fatias T2a and T3: the public methods that make a
// texture (create_texture) and overwrite a part of one (update_texture), in their own file as the
// renderer's texture family opened. They only forward: the renderer's context and GL table are
// read from the frame machine, the making is texture_upload.hpp's and the writing
// texture_update.hpp's. Their refusals reach the caller as they came.
namespace glintfx {

// cppcheck-suppress-begin passedByValue
// reason: gltfx_image_view is a VALUE type (a non-owning view, core/image_view.hpp), and the
// public create_texture() takes it by value as docs/auditoria-api-texture.md fixes; a const
// reference would make the caller's lifetime part of the contract.
gltfx_rslt<gltfx_texture_2d>
gltfx_renderer_2d::create_texture(gltfx_image_view image, gltfx_texture_2d_desc desc) noexcept {
    assert(impl != nullptr &&
           "gltfx_renderer_2d::create_texture() called on a moved-from renderer - the object no "
           "longer owns its drawing state");
    const draw2d::texture_gl_access access = impl->texture_access();
    gltfx_rslt<texture_2d_impl *> made =
        draw2d::create_texture_object(access, image, desc, draw2d::default_batch_allocator());
    if (made.has_error()) {
        return gltfx_rslt<gltfx_texture_2d>::err(made.err());
    }
    return gltfx_rslt<gltfx_texture_2d>::ok(gltfx_texture_2d(made.value()));
}
// cppcheck-suppress-end passedByValue

// cppcheck-suppress-begin passedByValue
// reason: gltfx_image_view is a VALUE type (a non-owning view, core/image_view.hpp), and the
// public update_texture() takes it by value as docs/auditoria-api-texture.md fixes, like
// create_texture().
gltfx_rslt<void> gltfx_renderer_2d::update_texture(gltfx_texture_2d &texture, std::uint32_t x,
                                                   std::uint32_t y,
                                                   gltfx_image_view image) noexcept {
    assert(impl != nullptr &&
           "gltfx_renderer_2d::update_texture() called on a moved-from renderer - the object no "
           "longer owns its drawing state");
    return impl->update_texture(texture.impl, x, y, image);
}
// cppcheck-suppress-end passedByValue

} // namespace glintfx
