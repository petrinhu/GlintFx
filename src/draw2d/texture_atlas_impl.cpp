// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/texture_atlas_impl.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <string_view>
#include <type_traits>

#include <glintfx/core/err_code.hpp>

#include "core/atlas_memory.hpp"
#include "draw2d/atlas_extrude.hpp"
#include "draw2d/mipmap_reduce.hpp"
#include "draw2d/pixel_premultiply.hpp"
#include "draw2d/texture_update.hpp"
#include "draw2d/texture_upload.hpp"

// draw2d/texture_atlas_impl.cpp - R2D-TEXTURE, fatia T6-B1; draw2d/texture_atlas_impl.hpp says what
// each function does.
namespace glintfx::draw2d {

namespace {

using glintfx::gltfx_atlas_rect;
using glintfx::gltfx_atlas_slot;
using glintfx::gltfx_err;
using glintfx::gltfx_err_code;
using glintfx::gltfx_image_view;
using glintfx::gltfx_pixel_format;
using glintfx::gltfx_rect_texel;
using glintfx::gltfx_rslt;
using glintfx::gltfx_texture_atlas_desc;
using glintfx::texture_2d_impl;
using glintfx::texture_atlas_impl;

constexpr std::size_t k_texel_bytes = 4;

// The interior lives in a block from the batch allocator (realloc-shaped), as the allocator's own
// interior does (core/atlas_allocator_impl.cpp).
static_assert(std::is_trivially_copyable_v<texture_atlas_impl>,
              "texture_atlas_impl lives in a realloc-shaped block: trivially copyable");

[[nodiscard]] gltfx_err refusal(gltfx_err_code code, std::string_view name) noexcept {
    return gltfx_err(code).with_rejected_value(name);
}

// The empty image the texture starts from: width x height texels of four zero bytes, from
// `memory`; nullptr when the memory refused or the size does not fit in memory at all.
[[nodiscard]] std::byte *zero_texels(const gltfx_texture_atlas_desc &desc,
                                     const batch_allocator &memory) noexcept {
    const std::size_t width = desc.width;
    const std::size_t height = desc.height;
    if (width > std::numeric_limits<std::size_t>::max() / k_texel_bytes / height) {
        return nullptr;
    }
    const std::size_t bytes = width * height * k_texel_bytes;
    auto *block = static_cast<std::byte *>(memory.reallocate(nullptr, bytes));
    if (block != nullptr) {
        std::memset(block, 0, bytes);
    }
    return block;
}

// The texture of the atlas: updatable, with the descriptor's filter and exactly its reduced levels,
// made from the empty image. Its refusals are create_texture()'s, as they came, but the empty image
// itself ("atlas_open").
[[nodiscard]] gltfx_rslt<texture_2d_impl *>
make_atlas_texture(const texture_gl_access &access, const gltfx_texture_atlas_desc &desc,
                   const batch_allocator &memory) noexcept {
    std::byte *zeros = zero_texels(desc, memory);
    if (zeros == nullptr) {
        return gltfx_rslt<texture_2d_impl *>::err(
            refusal(gltfx_err_code::out_of_memory, "atlas_open"));
    }
    const gltfx_image_view empty{.pixels = std::span<const std::byte>(
                                     zeros, std::size_t{desc.width} * desc.height * k_texel_bytes),
                                 .width = desc.width,
                                 .height = desc.height,
                                 .bytes_per_row = std::size_t{desc.width} * k_texel_bytes,
                                 .pixel_format = gltfx_pixel_format::rgba8_linear_premultiplied};
    const glintfx::gltfx_texture_2d_desc texture_desc{
        .filter = desc.filter, .mipmap_levels = desc.mipmap_levels, .updatable = true};
    gltfx_rslt<texture_2d_impl *> made = create_texture_object(access, empty, texture_desc, memory);
    memory.release(zeros);
    return made;
}

// The interior with its allocator, nothing taken yet; out_of_memory "atlas_open" when the memory
// refused the interior (the allocator's own refusal says the same name).
[[nodiscard]] gltfx_rslt<texture_atlas_impl *>
make_interior(const gltfx_texture_atlas_desc &desc, const atlas_layout &layout,
              const batch_allocator &memory) noexcept {
    gltfx_rslt<glintfx::atlas_allocator_impl *> cells = glintfx::make_atlas_allocator_impl(
        desc.width, desc.height, glintfx::default_atlas_memory());
    if (cells.has_error()) {
        return gltfx_rslt<texture_atlas_impl *>::err(cells.err());
    }
    void *block = memory.reallocate(nullptr, sizeof(texture_atlas_impl));
    if (block == nullptr) {
        glintfx::destroy_atlas_allocator_impl(cells.value());
        return gltfx_rslt<texture_atlas_impl *>::err(
            refusal(gltfx_err_code::out_of_memory, "atlas_open"));
    }
    auto *atlas = static_cast<texture_atlas_impl *>(block);
    *atlas = texture_atlas_impl{.cells = cells.value(), .layout = layout, .memory = memory};
    return gltfx_rslt<texture_atlas_impl *>::ok(atlas);
}

// "width", then "height", for an image with no texel; then the view itself.
[[nodiscard]] gltfx_rslt<void> check_atlas_image(const gltfx_image_view &image) noexcept {
    if (image.width == 0U) {
        return gltfx_rslt<void>::err(refusal(gltfx_err_code::invalid_argument, "width"));
    }
    if (image.height == 0U) {
        return gltfx_rslt<void>::err(refusal(gltfx_err_code::invalid_argument, "height"));
    }
    return check_image_view(image);
}

// The cell of `image` on this atlas: "width" or "height" when it is wider or taller than the atlas.
// The allocator refuses such a cell by the same names; the check is made here, in 64 bits, so that
// a side past 32 bits (an image near 2^32 texels wide with its margins) is never cut to 32 bits.
[[nodiscard]] gltfx_rslt<gltfx_atlas_rect> cell_of(const texture_atlas_impl &atlas,
                                                   const gltfx_image_view &image) noexcept {
    const std::uint64_t width = atlas_cell_side(image.width, atlas.layout);
    const std::uint64_t height = atlas_cell_side(image.height, atlas.layout);
    if (width > atlas.cells->geometry.width) {
        return gltfx_rslt<gltfx_atlas_rect>::err(
            refusal(gltfx_err_code::invalid_argument, "width"));
    }
    if (height > atlas.cells->geometry.height) {
        return gltfx_rslt<gltfx_atlas_rect>::err(
            refusal(gltfx_err_code::invalid_argument, "height"));
    }
    return gltfx_rslt<gltfx_atlas_rect>::ok(gltfx_atlas_rect{
        0, 0, static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)});
}

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
    const std::size_t bytes = std::size_t{cell.width} * cell.height * k_texel_bytes;
    return ensure_room(atlas.memory, atlas.cell_texels, bytes);
}

// The extruded texels of `image` written into the cell of `slot` on the card, by update_texture().
[[nodiscard]] gltfx_rslt<void> write_cell(texture_atlas_impl &atlas, texture_2d_impl &texture,
                                          const gltfx_image_view &image,
                                          const gltfx_atlas_rect &cell) noexcept {
    const std::size_t bytes = std::size_t{cell.width} * cell.height * k_texel_bytes;
    const std::span<std::byte> texels(atlas.cell_texels.data, bytes);
    extrude_atlas_cell(image, atlas.layout.margin,
                       atlas_cell_bytes{texels, cell.width, cell.height});
    const gltfx_image_view written{.pixels = texels,
                                   .width = cell.width,
                                   .height = cell.height,
                                   .bytes_per_row = std::size_t{cell.width} * k_texel_bytes,
                                   .pixel_format = image.pixel_format};
    const texel_rect place{covered_range{cell.x, cell.x + cell.width - 1U},
                           covered_range{cell.y, cell.y + cell.height - 1U}};
    return update_texture_object(texture, place, written, atlas.memory);
}

} // namespace

gltfx_rslt<texture_atlas_made> make_texture_atlas(const texture_gl_access &access,
                                                  const gltfx_texture_atlas_desc &desc,
                                                  const batch_allocator &memory) noexcept {
    const gltfx_rslt<atlas_layout> layout = check_atlas_desc(desc);
    if (layout.has_error()) {
        return gltfx_rslt<texture_atlas_made>::err(layout.err());
    }
    gltfx_rslt<texture_atlas_impl *> atlas = make_interior(desc, layout.value(), memory);
    if (atlas.has_error()) {
        return gltfx_rslt<texture_atlas_made>::err(atlas.err());
    }
    gltfx_rslt<texture_2d_impl *> texture = make_atlas_texture(access, desc, memory);
    if (texture.has_error()) {
        destroy_texture_atlas(atlas.value());
        return gltfx_rslt<texture_atlas_made>::err(texture.err());
    }
    return gltfx_rslt<texture_atlas_made>::ok(texture_atlas_made{atlas.value(), texture.value()});
}

void destroy_texture_atlas(texture_atlas_impl *atlas) noexcept {
    if (atlas == nullptr) {
        return;
    }
    glintfx::destroy_atlas_allocator_impl(atlas->cells);
    const batch_allocator memory = atlas->memory;
    memory.release(atlas->image_sizes.data);
    memory.release(atlas->cell_texels.data);
    memory.release(atlas);
}

gltfx_rslt<gltfx_atlas_slot> add_image_to_atlas(texture_atlas_impl *atlas, texture_2d_impl *texture,
                                                const gltfx_image_view &image) noexcept {
    if (atlas == nullptr) {
        return gltfx_rslt<gltfx_atlas_slot>::err(
            refusal(gltfx_err_code::invalid_argument, "atlas"));
    }
    const gltfx_rslt<void> checked = check_atlas_image(image);
    if (checked.has_error()) {
        return gltfx_rslt<gltfx_atlas_slot>::err(checked.err());
    }
    const gltfx_rslt<gltfx_atlas_rect> cell = cell_of(*atlas, image);
    if (cell.has_error()) {
        return gltfx_rslt<gltfx_atlas_slot>::err(cell.err());
    }
    if (!reserve_for_cell(*atlas, cell.value())) {
        return gltfx_rslt<gltfx_atlas_slot>::err(
            refusal(gltfx_err_code::out_of_memory, "atlas_add_image"));
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
            refusal(gltfx_err_code::invalid_argument, "atlas"));
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
        return gltfx_rslt<void>::err(refusal(gltfx_err_code::invalid_argument, "atlas"));
    }
    return glintfx::release_from_atlas(atlas->cells, slot);
}

} // namespace glintfx::draw2d
