// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/texture_atlas_impl.hpp"

#include <cstddef>
#include <cstring>
#include <limits>
#include <span>
#include <type_traits>

#include <glintfx/core/err_code.hpp>

#include "core/atlas_memory.hpp"
#include "draw2d/texture_upload.hpp"

// draw2d/texture_atlas_impl.cpp - R2D-TEXTURE, fatia T6-B1: the life of the interior, from what
// open() makes to what is given back. The cells an open atlas hands out are
// draw2d/texture_atlas_cells.cpp; draw2d/texture_atlas_impl.hpp says what each function does.
namespace glintfx::draw2d {

namespace {

using glintfx::gltfx_err_code;
using glintfx::gltfx_image_view;
using glintfx::gltfx_pixel_format;
using glintfx::gltfx_rslt;
using glintfx::gltfx_texture_atlas_desc;
using glintfx::texture_2d_impl;
using glintfx::texture_atlas_impl;

// The interior lives in a block from the batch allocator (realloc-shaped), as the allocator's own
// interior does (core/atlas_allocator_impl.cpp).
static_assert(std::is_trivially_copyable_v<texture_atlas_impl>,
              "texture_atlas_impl lives in a realloc-shaped block: trivially copyable");

// The empty image the texture starts from: width x height texels of four zero bytes, from
// `memory`; nullptr when the memory refused or the size does not fit in memory at all.
[[nodiscard]] std::byte *zero_texels(const gltfx_texture_atlas_desc &desc,
                                     const batch_allocator &memory) noexcept {
    const std::size_t width = desc.width;
    const std::size_t height = desc.height;
    if (width > std::numeric_limits<std::size_t>::max() / k_atlas_texel_bytes / height) {
        return nullptr;
    }
    const std::size_t bytes = width * height * k_atlas_texel_bytes;
    auto *block = static_cast<std::byte *>(memory.reallocate(nullptr, bytes));
    if (block != nullptr) {
        std::memset(block, 0, bytes);
    }
    return block;
}

// The texture of the atlas: updatable, with the descriptor's filter and exactly its reduced levels,
// made from the empty image. Its refusals are create_texture()'s, as they came, but the empty image
// itself ("atlas_open").
[[nodiscard]] gltfx_rslt<texture_2d_impl *>
make_atlas_texture(const texture_gl_access &access, const gltfx_texture_atlas_desc &desc,
                   const batch_allocator &memory) noexcept {
    std::byte *zeros = zero_texels(desc, memory);
    if (zeros == nullptr) {
        return gltfx_rslt<texture_2d_impl *>::err(
            atlas_refusal(gltfx_err_code::out_of_memory, "atlas_open"));
    }
    const gltfx_image_view empty{
        .pixels = std::span<const std::byte>(zeros, std::size_t{desc.width} * desc.height *
                                                        k_atlas_texel_bytes),
        .width = desc.width,
        .height = desc.height,
        .bytes_per_row = std::size_t{desc.width} * k_atlas_texel_bytes,
        .pixel_format = gltfx_pixel_format::rgba8_linear_premultiplied};
    const glintfx::gltfx_texture_2d_desc texture_desc{
        .filter = desc.filter, .mipmap_levels = desc.mipmap_levels, .updatable = true};
    gltfx_rslt<texture_2d_impl *> made = create_texture_object(access, empty, texture_desc, memory);
    memory.release(zeros);
    return made;
}

// The interior with its allocator, nothing taken yet; out_of_memory "atlas_open" when the memory
// refused the interior (the allocator's own refusal says the same name).
[[nodiscard]] gltfx_rslt<texture_atlas_impl *>
make_interior(const gltfx_texture_atlas_desc &desc, const atlas_layout &layout,
              const batch_allocator &memory) noexcept {
    gltfx_rslt<glintfx::atlas_allocator_impl *> cells = glintfx::make_atlas_allocator_impl(
        desc.width, desc.height, glintfx::default_atlas_memory());
    if (cells.has_error()) {
        return gltfx_rslt<texture_atlas_impl *>::err(cells.err());
    }
    void *block = memory.reallocate(nullptr, sizeof(texture_atlas_impl));
    if (block == nullptr) {
        glintfx::destroy_atlas_allocator_impl(cells.value());
        return gltfx_rslt<texture_atlas_impl *>::err(
            atlas_refusal(gltfx_err_code::out_of_memory, "atlas_open"));
    }
    auto *atlas = static_cast<texture_atlas_impl *>(block);
    *atlas = texture_atlas_impl{.cells = cells.value(), .layout = layout, .memory = memory};
    return gltfx_rslt<texture_atlas_impl *>::ok(atlas);
}

} // namespace

gltfx_rslt<texture_atlas_made> make_texture_atlas(const texture_gl_access &access,
                                                  const gltfx_texture_atlas_desc &desc,
                                                  const batch_allocator &memory) noexcept {
    const gltfx_rslt<atlas_layout> layout = check_atlas_desc(desc);
    if (layout.has_error()) {
        return gltfx_rslt<texture_atlas_made>::err(layout.err());
    }
    gltfx_rslt<texture_atlas_impl *> atlas = make_interior(desc, layout.value(), memory);
    if (atlas.has_error()) {
        return gltfx_rslt<texture_atlas_made>::err(atlas.err());
    }
    gltfx_rslt<texture_2d_impl *> texture = make_atlas_texture(access, desc, memory);
    if (texture.has_error()) {
        destroy_texture_atlas(atlas.value());
        return gltfx_rslt<texture_atlas_made>::err(texture.err());
    }
    return gltfx_rslt<texture_atlas_made>::ok(texture_atlas_made{atlas.value(), texture.value()});
}

void destroy_texture_atlas(texture_atlas_impl *atlas) noexcept {
    if (atlas == nullptr) {
        return;
    }
    glintfx::destroy_atlas_allocator_impl(atlas->cells);
    const batch_allocator memory = atlas->memory;
    memory.release(atlas->image_sizes.data);
    memory.release(atlas->cell_texels.data);
    memory.release(atlas);
}

} // namespace glintfx::draw2d
