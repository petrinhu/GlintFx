// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/mipmap_chain.hpp"

#include "draw2d/mipmap_reduce.hpp"

// draw2d/mipmap_chain.cpp - R2D-TEXTURE, fatia T3: the layout of mipmap_chain.hpp. Each level
// takes the texels of its two reduced sides, and the levels follow one another in order.
namespace glintfx::draw2d {

namespace {

// The texels of level `level` of a texture of `width` x `height` texels.
[[nodiscard]] std::size_t texels_of_level(std::uint32_t width, std::uint32_t height,
                                          std::uint32_t level) noexcept {
    return static_cast<std::size_t>(reduced_side(width, level)) * reduced_side(height, level);
}

} // namespace

std::size_t level_offset(std::uint32_t width, std::uint32_t height, std::uint32_t level) noexcept {
    std::size_t before = 0U;
    for (std::uint32_t k = 0U; k < level; ++k) {
        before += texels_of_level(width, height, k);
    }
    return before;
}

std::size_t chain_texel_count(std::uint32_t width, std::uint32_t height,
                              std::uint32_t reduced_levels) noexcept {
    return level_offset(width, height, reduced_levels) +
           texels_of_level(width, height, reduced_levels);
}

} // namespace glintfx::draw2d
