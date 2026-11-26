// SPDX-License-Identifier: AGPL-3.0-or-later
#include <glintfx/draw2d/texture_atlas.hpp>

#include <cassert>
#include <utility>

#include <glintfx/draw2d/renderer_2d.hpp>

#include "draw2d/pod_buffer.hpp"
#include "draw2d/renderer_2d_impl.hpp"
#include "draw2d/texture_atlas_impl.hpp"

// draw2d/texture_atlas_facade.cpp - R2D-TEXTURE, fatia T6-B1: the public handle
// gltfx_texture_atlas. Each member is a forward to draw2d/texture_atlas_impl.hpp, with the
// library's own memory; the handle holds the interior and the atlas's texture, and a moved-from
// handle holds neither.
namespace glintfx {

gltfx_rslt<gltfx_texture_atlas>
gltfx_texture_atlas::open(gltfx_renderer_2d &renderer,
                          const gltfx_texture_atlas_desc &desc) noexcept {
    renderer_2d_impl *interior = renderer_2d_internal_access::get(renderer);
    assert(interior != nullptr &&
           "gltfx_texture_atlas::open() called on a moved-from renderer - the object no longer "
           "owns its drawing state");
    gltfx_rslt<draw2d::texture_atlas_made> made = draw2d::make_texture_atlas(
        interior->texture_access(), desc, draw2d::default_batch_allocator());
    if (made.has_error()) {
        return gltfx_rslt<gltfx_texture_atlas>::err(made.err());
    }
    return gltfx_rslt<gltfx_texture_atlas>::ok(
        gltfx_texture_atlas(made.value().atlas, made.value().texture));
}

gltfx_texture_atlas::gltfx_texture_atlas(gltfx_texture_atlas &&other) noexcept
    : impl(std::exchange(other.impl, nullptr)), atlas_texture(std::move(other.atlas_texture)) {}

gltfx_texture_atlas &gltfx_texture_atlas::operator=(gltfx_texture_atlas &&other) noexcept {
    if (this != &other) {
        draw2d::destroy_texture_atlas(impl);
        impl = std::exchange(other.impl, nullptr);
        atlas_texture = std::move(other.atlas_texture);
    }
    return *this;
}

gltfx_texture_atlas::~gltfx_texture_atlas() { draw2d::destroy_texture_atlas(impl); }

bool gltfx_texture_atlas::is_open() const noexcept { return impl != nullptr; }

const gltfx_texture_2d &gltfx_texture_atlas::texture() const noexcept { return atlas_texture; }

// cppcheck-suppress-begin passedByValue
// reason: gltfx_image_view and gltfx_atlas_slot are VALUE types (a non-owning view, and seven
// 32-bit integers), and the public methods take them by value as docs/auditoria-api-texture.md
// fixes, like create_texture() and gltfx_atlas_allocator::release().
gltfx_rslt<gltfx_atlas_slot> gltfx_texture_atlas::add_image(gltfx_image_view image) noexcept {
    return draw2d::add_image_to_atlas(impl, atlas_texture.impl, image);
}

gltfx_rslt<gltfx_rect_texel> gltfx_texture_atlas::region_of(gltfx_atlas_slot slot) const noexcept {
    return draw2d::region_in_atlas(impl, slot);
}

gltfx_rslt<void> gltfx_texture_atlas::release_slot(gltfx_atlas_slot slot) noexcept {
    return draw2d::release_from_texture_atlas(impl, slot);
}
// cppcheck-suppress-end passedByValue

} // namespace glintfx
