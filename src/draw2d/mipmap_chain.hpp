// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>

// draw2d/mipmap_chain.hpp - R2D-TEXTURE, fatia T3: where each level of a texture lies when ALL its
// levels are kept in one block of texels: level 0 first, then level 1, and so on, each row by row,
// so texel (x, y) of level k sits at level_offset(width, height, k) + y * reduced_side(width, k) +
// x (mipmap_reduce.hpp gives the sides). Pure: no GL, no operating system, no allocation.
namespace glintfx::draw2d {

// How many texels come before level `level`: the texels of levels 0 to level - 1. Zero for level 0.
[[nodiscard]] std::size_t level_offset(std::uint32_t width, std::uint32_t height,
                                       std::uint32_t level) noexcept;

// How many texels the block holds for a texture with `reduced_levels` reduced levels: levels 0 to
// reduced_levels, all of them.
[[nodiscard]] std::size_t chain_texel_count(std::uint32_t width, std::uint32_t height,
                                            std::uint32_t reduced_levels) noexcept;

} // namespace glintfx::draw2d
