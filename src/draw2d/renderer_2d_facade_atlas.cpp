// SPDX-License-Identifier: AGPL-3.0-or-later
#include <glintfx/draw2d/renderer_2d.hpp>

#include <cassert>

#include <glintfx/draw2d/texture_atlas.hpp>

#include "draw2d/renderer_2d_impl.hpp"
#include "draw2d/texture_atlas_impl.hpp"

// renderer_2d_facade_atlas.cpp - R2D-TEXTURE, fatia T6-B2: the public method that draws the image
// of an atlas receipt, in its own file because it is the one place where the renderer meets an
// atlas: it reads the receipt in the interior of the atlas and hands the frame machine what it
// read, so renderer_2d_impl.hpp never knows what an atlas is. The same moved-from precondition as
// the rest of the facade.
namespace glintfx {

// cppcheck-suppress-begin passedByValue
// reason: gltfx_atlas_paint and gltfx_rect_world are VALUE types (a few numbers each), and the
// public draw_atlas_image() takes them by value as docs/auditoria-api-texture.md fixes, like
// draw_texture().
void gltfx_renderer_2d::draw_atlas_image(const gltfx_texture_atlas &atlas, gltfx_atlas_paint paint,
                                         gltfx_rect_world rect, gltfx_draw_layer layer) noexcept {
    assert(impl != nullptr &&
           "gltfx_renderer_2d::draw_atlas_image() called on a moved-from renderer - the object no "
           "longer owns its drawing state");
    const draw2d::atlas_receipt_read read = draw2d::read_atlas_receipt(atlas.impl, paint.slot);
    const draw2d::atlas_image_call call{
        atlas.atlas_texture.impl,
        gltfx_texture_paint{gltfx_texture_region{read.region, paint.mirror}, paint.tint},
        read.refusal};
    impl->draw_atlas_image(call, rect, layer);
}
// cppcheck-suppress-end passedByValue

} // namespace glintfx
