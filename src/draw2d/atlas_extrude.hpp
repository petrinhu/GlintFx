// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include <glintfx/core/image_view.hpp>

// draw2d/atlas_extrude.hpp - R2D-TEXTURE, fatia T6-B1: the texels of one cell of the texture atlas
// (texture_atlas.hpp, THE CELL), pure: the image in its place, and every other texel of the cell a
// copy of the image texel nearest to it. The bytes are copied as they are, four to a texel, in the
// image's own pixel format (the conversion to stored texels comes after, in update_texture()): a
// copy of a texel converts to the copy of its conversion. No GL, no memory. NOT under
// include/glintfx/: internal, not frozen ABI.
namespace glintfx::draw2d {

// Where the cell's texels go: `width` x `height` texels of four bytes, rows packed tightly.
struct atlas_cell_bytes {
    std::span<std::byte> bytes;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

// Writes every texel of `cell`: texel (x, y) is the image texel (clamp(x - margin, 0, w - 1),
// clamp(y - margin, 0, h - 1)), w and h the image's size, so the image lands `margin` texels in
// from the left and top. Preconditions: `image` passed check_image_view() and has no zero side;
// cell.bytes holds cell.width x cell.height x 4 bytes.
void extrude_atlas_cell(const glintfx::gltfx_image_view &image, std::uint32_t margin,
                        const atlas_cell_bytes &cell) noexcept;

} // namespace glintfx::draw2d
