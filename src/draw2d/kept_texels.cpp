// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/kept_texels.hpp"

// draw2d/kept_texels.cpp - R2D-TEXTURE, fatia T3: the ownership of kept_texels.hpp. One block at a
// time: taking a new one frees the old one through the allocator that made it, and the destructor
// frees the block it holds, so the block is freed once.
namespace glintfx::draw2d {

kept_texels::~kept_texels() { free_block(); }

void kept_texels::adopt(const pod_buffer<glintfx::gltfx_rgba8> &block,
                        const batch_allocator &allocator) noexcept {
    free_block();
    block_ = block;
    allocator_ = allocator;
}

std::span<glintfx::gltfx_rgba8> kept_texels::texels() noexcept {
    return std::span<glintfx::gltfx_rgba8>(block_.data, block_.size);
}

void kept_texels::free_block() noexcept {
    if (block_.data != nullptr) {
        allocator_.release(block_.data);
    }
    block_ = pod_buffer<glintfx::gltfx_rgba8>{};
}

} // namespace glintfx::draw2d
