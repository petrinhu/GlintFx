// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>

#include <glintfx/core/atlas_allocator.hpp>
#include <glintfx/core/err.hpp>
#include <glintfx/core/image_view.hpp>
#include <glintfx/draw2d/texture_atlas.hpp>
#include <glintfx/draw2d/texture_region.hpp>

#include "core/atlas_allocator_impl.hpp"
#include "draw2d/atlas_cell.hpp"
#include "draw2d/pod_buffer.hpp"
#include "draw2d/texture_2d_impl.hpp"

// draw2d/texture_atlas_impl.hpp - R2D-TEXTURE, fatia T6-B1: the interior of the public handle
// gltfx_texture_atlas (texture_atlas.hpp) and what it does: make, add an image, read a receipt,
// release one, give everything back. Each call is checked first (atlas_cell.hpp, check_image_view()
// of pixel_premultiply.hpp, the allocator's own checks) and then carried out on three things it
// does not own the rules of: the WHERE is the allocator's interior (core/atlas_allocator_impl.hpp,
// reached through its internal functions, so a receipt has one definition and no copy of it is
// kept here), the TEXELS of a cell are atlas_extrude.hpp's, and the WRITING is update_texture()'s
// (texture_update.hpp). The texture itself is held by the public handle; here it is only written.
// All memory of the interior comes from the batch_allocator given to make_texture_atlas(), which is
// how texture_atlas_test arms a failure at each request. NOT under include/glintfx/: internal, not
// frozen ABI.
//
// THE ALIGNMENT IS THE ALLOCATOR'S PLACEMENT. Every cell is asked with sides that are multiples of
// 2^k, and the allocator of this version puts each rectangle at the left end of a free run of a
// shelf whose top is a sum of the heights it was asked for (core/atlas_allocator.hpp, HOW A
// RECTANGLE IS PLACED): so every cell starts on a multiple of 2^k. Nothing here checks it at each
// call, because nothing could fail it; texture_atlas_test locks it over a long sequence of adds and
// releases, and a later placement that broke it would fail there.
namespace glintfx {

struct texture_atlas_impl {
    // Where each cell goes and which receipt is current.
    atlas_allocator_impl *cells = nullptr;
    draw2d::atlas_layout layout{};
    draw2d::batch_allocator memory{};
    // The image size of each receipt number, indexed by the number (the allocator knows the cell,
    // not the image in it). Grown BEFORE a cell is taken to one entry more than the numbers the
    // allocator has, the most one add can need, so nothing asks for memory after the cell is taken.
    draw2d::pod_buffer<draw2d::atlas_image_size> image_sizes{};
    // The texels of the cell being written, kept between calls (size 0, capacity grown).
    draw2d::pod_buffer<std::byte> cell_texels{};
};

namespace draw2d {

// What open() makes: the interior, and the texture the public handle will hold.
struct texture_atlas_made {
    texture_atlas_impl *atlas = nullptr;
    texture_2d_impl *texture = nullptr;
};

// open(renderer, desc), on the renderer's texture access. Refusals: those of texture_atlas.hpp, in
// its order; a refused call leaves nothing behind.
[[nodiscard]] glintfx::gltfx_rslt<texture_atlas_made>
make_texture_atlas(const texture_gl_access &access, const glintfx::gltfx_texture_atlas_desc &desc,
                   const batch_allocator &memory) noexcept;

// Gives back everything `atlas` holds, and `atlas` itself, but not the texture (the public handle
// releases it). Harmless on nullptr.
void destroy_texture_atlas(glintfx::texture_atlas_impl *atlas) noexcept;

// add_image(image) on `atlas` (nullptr for a moved-from atlas), writing into `texture`.
[[nodiscard]] glintfx::gltfx_rslt<glintfx::gltfx_atlas_slot>
add_image_to_atlas(glintfx::texture_atlas_impl *atlas, glintfx::texture_2d_impl *texture,
                   const glintfx::gltfx_image_view &image) noexcept;

// region_of(slot) on `atlas` (nullptr for a moved-from atlas).
[[nodiscard]] glintfx::gltfx_rslt<glintfx::gltfx_rect_texel>
region_in_atlas(const glintfx::texture_atlas_impl *atlas,
                const glintfx::gltfx_atlas_slot &slot) noexcept;

// release_slot(slot) on `atlas` (nullptr for a moved-from atlas).
[[nodiscard]] glintfx::gltfx_rslt<void>
release_from_texture_atlas(glintfx::texture_atlas_impl *atlas,
                           const glintfx::gltfx_atlas_slot &slot) noexcept;

} // namespace draw2d

} // namespace glintfx
