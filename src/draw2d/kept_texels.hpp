// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <span>

#include <glintfx/core/color.hpp>

#include "draw2d/pod_buffer.hpp"

// draw2d/kept_texels.hpp - R2D-TEXTURE, fatia T3: the texels of every level of a texture with
// reduced levels, kept in the library's memory (mipmap_chain.hpp gives where each level lies), so
// update_texture() can recompute the reduced texels a change touches exactly as create_texture()
// computed them. It OWNS the block: the block is freed, through the allocator that made it, when
// this value goes, so a texture record (texture_2d_impl.hpp) frees it with the record, whoever
// deletes the record. Not copyable and not movable: one block, one owner, freed once.
namespace glintfx::draw2d {

class kept_texels {
  public:
    kept_texels() = default;
    kept_texels(const kept_texels &) = delete;
    kept_texels &operator=(const kept_texels &) = delete;
    kept_texels(kept_texels &&) = delete;
    kept_texels &operator=(kept_texels &&) = delete;
    ~kept_texels();

    // Takes `block` (texels made by `allocator`), and frees the block it held before, if any.
    void adopt(const pod_buffer<glintfx::gltfx_rgba8> &block,
               const batch_allocator &allocator) noexcept;

    // The kept texels, all levels in one span; empty when nothing is kept.
    [[nodiscard]] std::span<glintfx::gltfx_rgba8> texels() noexcept;

  private:
    void free_block() noexcept;

    pod_buffer<glintfx::gltfx_rgba8> block_{};
    batch_allocator allocator_ = default_batch_allocator();
};

} // namespace glintfx::draw2d
