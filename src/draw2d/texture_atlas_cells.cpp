// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/texture_atlas_impl.hpp"

#include <cstddef>
#include <span>

#include <glintfx/core/err_code.hpp>

#include "draw2d/atlas_extrude.hpp"
#include "draw2d/mipmap_reduce.hpp"
#include "draw2d/texture_update.hpp"

// draw2d/texture_atlas_cells.cpp - R2D-TEXTURE: the cells an open atlas hands out, each taken,
// read or given back by its receipt. The life of the interior is draw2d/texture_atlas_impl.cpp;
// draw2d/texture_atlas_impl.hpp says what each function does.
namespace glintfx::draw2d {

namespace {

using glintfx::gltfx_atlas_rect;
using glintfx::gltfx_atlas_slot;
using glintfx::gltfx_err_code;
using glintfx::gltfx_image_view;
using glintfx::gltfx_rect_texel;
using glintfx::gltfx_rslt;
using glintfx::texture_2d_impl;
using glintfx::texture_atlas_impl;

// The memory one add needs before its cell is taken: an image size for every number the allocator
// can hand out in this call (those it has, and one more), and the texels of the cell. A cell is
// never larger than the atlas, whose empty image fitted in memory at open(), so its byte count
// does not overflow. False, and nothing changed but capacity, when the memory refused.
[[nodiscard]] bool reserve_for_cell(texture_atlas_impl &atlas,
                                    const gltfx_atlas_rect &cell) noexcept {
    const std::size_t numbers = atlas.cells->slots.entries.size + 1U;
    if (numbers > atlas.image_sizes.size) {
        if (!ensure_room(atlas.memory, atlas.image_sizes, numbers - atlas.image_sizes.size)) {
            return false;
        }
        for (std::size_t i = atlas.image_sizes.size; i < numbers; ++i) {
            atlas.image_sizes.data[i] = atlas_image_size{};
        }
        atlas.image_sizes.size = numbers;
    }
    const std::size_t bytes = std::size_t{cell.width} * cell.height * k_atlas_texel_bytes;
    return ensure_room(atlas.memory, atlas.cell_texels, bytes);
}

// The extruded texels of `image` written into the cell of `slot` on the card, by update_texture().
[[nodiscard]] gltfx_rslt<void> write_cell(texture_atlas_impl &atlas, texture_2d_impl &texture,
                                          const gltfx_image_view &image,
                                          const gltfx_atlas_rect &cell) noexcept {
    const std::size_t bytes = std::size_t{cell.width} * cell.height * k_atlas_texel_bytes;
    const std::span<std::byte> texels(atlas.cell_texels.data, bytes);
    extrude_atlas_cell(image, atlas.layout.margin,
                       atlas_cell_bytes{texels, cell.width, cell.height});
    const gltfx_image_view written{.pixels = texels,
                                   .width = cell.width,
                                   .height = cell.height,
                                   .bytes_per_row = std::size_t{cell.width} * k_atlas_texel_bytes,
                                   .pixel_format = image.pixel_format};
    const texel_rect place{covered_range{cell.x, cell.x + cell.width - 1U},
                           covered_range{cell.y, cell.y + cell.height - 1U}};
    return update_texture_object(texture, place, written, atlas.memory);
}

} // namespace

gltfx_rslt<gltfx_atlas_slot> add_image_to_atlas(texture_atlas_impl *atlas, texture_2d_impl *texture,
                                                const gltfx_image_view &image) noexcept {
    if (atlas == nullptr) {
        return gltfx_rslt<gltfx_atlas_slot>::err(
            atlas_refusal(gltfx_err_code::invalid_argument, "atlas"));
    }
    const gltfx_rslt<void> checked = check_atlas_image(image);
    if (checked.has_error()) {
        return gltfx_rslt<gltfx_atlas_slot>::err(checked.err());
    }
    const gltfx_rslt<gltfx_atlas_rect> cell = atlas_cell_rect(
        image, atlas->layout, atlas->cells->geometry.width, atlas->cells->geometry.height);
    if (cell.has_error()) {
        return gltfx_rslt<gltfx_atlas_slot>::err(cell.err());
    }
    if (!reserve_for_cell(*atlas, cell.value())) {
        return gltfx_rslt<gltfx_atlas_slot>::err(
            atlas_refusal(gltfx_err_code::out_of_memory, "atlas_add_image"));
    }
    gltfx_rslt<gltfx_atlas_slot> taken =
        glintfx::allocate_in_atlas(atlas->cells, cell.value().width, cell.value().height);
    if (taken.has_error()) {
        return taken;
    }
    const gltfx_atlas_slot slot = taken.value();
    const gltfx_rslt<void> written = write_cell(*atlas, *texture, image, slot.rect);
    if (written.has_error()) {
        // The slot was handed out by this very call, so giving it back cannot be refused.
        static_cast<void>(glintfx::release_from_atlas(atlas->cells, slot));
        return gltfx_rslt<gltfx_atlas_slot>::err(written.err());
    }
    atlas->image_sizes.data[slot.id] = atlas_image_size{image.width, image.height};
    return gltfx_rslt<gltfx_atlas_slot>::ok(slot);
}

gltfx_rslt<gltfx_rect_texel> region_in_atlas(const texture_atlas_impl *atlas,
                                             const gltfx_atlas_slot &slot) noexcept {
    if (atlas == nullptr) {
        return gltfx_rslt<gltfx_rect_texel>::err(
            atlas_refusal(gltfx_err_code::invalid_argument, "atlas"));
    }
    const gltfx_rslt<void> current = glintfx::check_current_atlas_slot(atlas->cells, slot);
    if (current.has_error()) {
        return gltfx_rslt<gltfx_rect_texel>::err(current.err());
    }
    return gltfx_rslt<gltfx_rect_texel>::ok(
        atlas_image_region(slot.rect, atlas->image_sizes.data[slot.id], atlas->layout));
}

gltfx_rslt<void> release_from_texture_atlas(texture_atlas_impl *atlas,
                                            const gltfx_atlas_slot &slot) noexcept {
    if (atlas == nullptr) {
        return gltfx_rslt<void>::err(atlas_refusal(gltfx_err_code::invalid_argument, "atlas"));
    }
    return glintfx::release_from_atlas(atlas->cells, slot);
}

} // namespace glintfx::draw2d
