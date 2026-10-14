// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>
#include <cstdint>
#include <span>

#include <glintfx/core/color.hpp>

#include "draw2d/mipmap_reduce.hpp"

// draw2d/texture_update_kept.hpp - R2D-TEXTURE, T3-fix1 (IMPORTANTE-5 and IMPORTANTE-2 of the N1
// review of T3): the kept texels of an updatable texture with reduced levels (kept_texels.hpp, laid
// out as mipmap_chain.hpp says) taking an update, and giving it back. Pure: no GL, no operating
// system, no allocation; texture_update.cpp writes to the card what this leaves in the block.
//
// THE SWAP, AND WHY: the texels of the rectangle in level 0 are EXCHANGED with the stored texels of
// the update, and the reduced texels the rectangle touches are recomputed from the level above,
// exactly as the making computes them (reduce_region_to_next_level()). After it, the block holds
// the new texture and the update's buffer holds what level 0 had there. Swapping the SAME rectangle
// with the SAME buffer again gives the block back exactly as it was: level 0 takes its old texels
// back, and every reduced texel the rectangle touches is recomputed from them (the texels it does
// not touch never changed). So a writing the card refuses leaves no refused texel in the library's
// memory, and no later update can send one to the card (renderer_2d.hpp, update_texture()).
namespace glintfx::draw2d {

// A side of uint32 texels allows at most 31 reduced levels: with level 0, 32 levels.
inline constexpr std::uint32_t k_max_chain_levels = 32U;

// The kept block of one texture as the swap reads it: every level in one span (mipmap_chain.hpp),
// the size of level 0 and the number of reduced levels.
struct kept_chain {
    std::span<glintfx::gltfx_rgba8> texels;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    std::uint32_t reduced_levels = 0U;
};

// The rectangle each level touches, level 0 first (the update's own rectangle): `count` is the
// number of levels, reduced_levels + 1.
struct touched_levels {
    std::array<texel_rect, k_max_chain_levels> rects{};
    std::uint32_t count = 0U;
};

// Exchanges the texels of `rect` in level 0 of `chain` with `stored` (the rectangle's texels, row
// by row, tightly packed), recomputes in `chain` the reduced texels the rectangle touches, level
// after level, and writes into `touched` the rectangle of each level. False, and nothing changed,
// when `chain` does not hold exactly every level (chain_texel_count()), has k_max_chain_levels
// reduced levels or more, or `rect` is empty, reaches past level 0, or is not the size of `stored`.
// Once those checks pass, the answer is true: level 0 is exchanged first, and the recomputation
// after it cannot fail, because the rectangle a non-empty rectangle of level 0 touches in each
// reduced level lies inside that level (T3-fix1 conference, COSMETICO-1). No false comes back with
// the block already changed.
[[nodiscard]] bool swap_update(const kept_chain &chain, const texel_rect &rect,
                               std::span<glintfx::gltfx_rgba8> stored,
                               touched_levels &touched) noexcept;

} // namespace glintfx::draw2d
