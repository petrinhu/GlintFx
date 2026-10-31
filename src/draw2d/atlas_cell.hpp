// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

#include <glintfx/core/atlas_allocator.hpp>
#include <glintfx/core/err.hpp>
#include <glintfx/draw2d/texture_atlas.hpp>
#include <glintfx/draw2d/texture_region.hpp>

// draw2d/atlas_cell.hpp - R2D-TEXTURE, fatia T6-B1: the arithmetic of the texture atlas
// (texture_atlas.hpp, THE CELL and MARGIN AND REDUCED LEVELS), pure: which descriptor open()
// refuses and by which name, how large the cell of an image is, and where the image sits in its
// cell. No GL, no memory, no allocator. NOT under include/glintfx/: internal, not frozen ABI.
namespace glintfx::draw2d {

// A descriptor that passed check_atlas_desc(): the extruded edge on each side of an image, the
// number of reduced levels k, and 2^k, the multiple every cell side (and so every cell start) is
// rounded to.
struct atlas_layout {
    std::uint32_t margin = 0;
    std::uint32_t reduced_levels = 0;
    std::uint32_t alignment = 1;
};

// The refusals of open() that need nothing but the descriptor, in the order of texture_atlas.hpp:
// "width", "height" (zero), "filter", "mipmap_levels", "margin_pixels" (less than 2^k, or
// 2 * margin + 1 more than a side), then "width" and "height" (not a multiple of 2^k). Each is
// invalid_argument with that name in rejected_value().
[[nodiscard]] glintfx::gltfx_rslt<atlas_layout>
check_atlas_desc(const glintfx::gltfx_texture_atlas_desc &desc) noexcept;

// One side of the cell of an image side of `image_side` texels: image_side + 2 * margin, rounded up
// to a multiple of the alignment. In 64 bits, so it never wraps; the caller compares it with the
// atlas side.
[[nodiscard]] std::uint64_t atlas_cell_side(std::uint32_t image_side,
                                            const atlas_layout &layout) noexcept;

// What a cell holds: the image's size, in texels.
struct atlas_image_size {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

// The rectangle of the image in the cell `cell`, in texels of the atlas texture: margin texels in
// from the cell's left and top edges, of the image's size.
[[nodiscard]] glintfx::gltfx_rect_texel atlas_image_region(const glintfx::gltfx_atlas_rect &cell,
                                                           atlas_image_size image,
                                                           const atlas_layout &layout) noexcept;

} // namespace glintfx::draw2d
