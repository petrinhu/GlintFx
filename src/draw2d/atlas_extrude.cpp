// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/atlas_extrude.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

#include <glintfx/core/image_view.hpp>

// draw2d/atlas_extrude.cpp - R2D-TEXTURE, fatia T6-B1; draw2d/atlas_extrude.hpp says what it does.
namespace glintfx::draw2d {

namespace {

constexpr std::size_t k_texel_bytes = 4;

// The image row or column nearest to the cell's `position`: 0 before the image, the last one after
// it, and position - margin inside it.
[[nodiscard]] std::size_t nearest_inside(std::uint32_t position, std::uint32_t margin,
                                         std::uint32_t image_side) noexcept {
    if (position < margin) {
        return 0;
    }
    const std::uint32_t inside = position - margin;
    return inside < image_side ? inside : image_side - 1U;
}

// One row of the cell, from one row of the image.
void extrude_row(const std::byte *image_row, std::uint32_t image_width, std::uint32_t margin,
                 std::span<std::byte> cell_row) noexcept {
    const std::size_t texels = cell_row.size() / k_texel_bytes;
    for (std::size_t x = 0; x < texels; ++x) {
        const std::size_t from = nearest_inside(static_cast<std::uint32_t>(x), margin, image_width);
        std::memcpy(cell_row.data() + x * k_texel_bytes, image_row + from * k_texel_bytes,
                    k_texel_bytes);
    }
}

} // namespace

void extrude_atlas_cell(const glintfx::gltfx_image_view &image, std::uint32_t margin,
                        const atlas_cell_bytes &cell) noexcept {
    const std::size_t row_bytes = std::size_t{cell.width} * k_texel_bytes;
    for (std::uint32_t y = 0; y < cell.height; ++y) {
        const std::size_t from = nearest_inside(y, margin, image.height);
        const std::byte *image_row = image.pixels.data() + from * image.bytes_per_row;
        extrude_row(image_row, image.width, margin,
                    cell.bytes.subspan(std::size_t{y} * row_bytes, row_bytes));
    }
}

} // namespace glintfx::draw2d
