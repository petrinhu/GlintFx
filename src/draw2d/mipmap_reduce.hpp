// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <span>

#include <glintfx/core/color.hpp>

// draw2d/mipmap_reduce.hpp - R2D-TEXTURE, fatia T2a: one reduced level (mipmap) made from the
// level above it, on the CPU. Pure function over plain data: no GL, no operating system, no
// allocation.
//
// WHY THE CPU AND NOT glGenerateMipmap: the OpenGL specification does not fix the filter of
// glGenerateMipmap ("a box filter is recommended", not required), and drivers differ on sRGB
// textures (Chromium measured incorrect mips "in three different ways" on four bots). The library
// promises the same texels on every system, so it computes them itself.
//
// THE FILTER: each texel of the reduced level is the AREA average of the texels its rectangle
// covers in the level above (an exact box filter). A side of even length n becomes n / 2 and each
// texel averages two per axis, with weight 1/2 each. A side of odd length n > 1 becomes (n - 1) / 2
// and each texel covers n / m source texels (m the new length), so the texels it covers partly
// count by the part covered. A side of length 1 stays 1.
//
// IN LINEAR LIGHT, PREMULTIPLIED: a stored texel (pixel_premultiply.hpp) is the sRGB encoding of
// the premultiplied linear colour, with straight alpha bytes. Each colour channel is decoded to
// linear light (srgb_channel_to_linear() of core/srgb_transfer.hpp), averaged, and stored again by
// stored_channel_byte(); the alpha is averaged as a fraction of 255 and rounded. Averaging the
// encoded bytes instead would darken every edge between two colours.
namespace glintfx::draw2d {

// The source texels, along one axis, that destination texel `target` covers at all, both ends
// included: with n the source length and m the target length, source texel s spans
// [s * m, (s + 1) * m) and target texel t spans [t * n, (t + 1) * n) in units of 1 / (n * m), so
// first = t * n / m and last = ((t + 1) * n - 1) / m, in integers. A texel past `last` would be
// read with weight zero, past the end of the row on the last texel: the bound is exact.
struct covered_range {
    std::uint32_t first = 0U;
    std::uint32_t last = 0U;
};
[[nodiscard]] covered_range covered_texels(std::uint32_t target, std::uint32_t source_length,
                                           std::uint32_t target_length) noexcept;

// The length of reduced level `level` of a side of `side` texels: max(1, side >> level).
[[nodiscard]] std::uint32_t reduced_side(std::uint32_t side, std::uint32_t level) noexcept;

// Writes into `destination` the level below `source` (width x height stored texels, row by row,
// texel (x, y) at y * width + x). `destination` must hold exactly reduced_side(width, 1) *
// reduced_side(height, 1) texels, and `source` exactly width * height; otherwise nothing is
// written and the result is false.
[[nodiscard]] bool reduce_to_next_level(std::span<const glintfx::gltfx_rgba8> source,
                                        std::uint32_t width, std::uint32_t height,
                                        std::span<glintfx::gltfx_rgba8> destination) noexcept;

} // namespace glintfx::draw2d
