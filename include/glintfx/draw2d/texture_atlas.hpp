// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

#include <glintfx/core/atlas_allocator.hpp>
#include <glintfx/core/err.hpp>
#include <glintfx/core/image_view.hpp>
#include <glintfx/draw2d/texture_2d.hpp>
#include <glintfx/draw2d/texture_2d_desc.hpp>
#include <glintfx/draw2d/texture_region.hpp>
#include <glintfx/export.hpp>

// draw2d/texture_atlas.hpp - R2D-TEXTURE. Many images in ONE texture: a texture of the renderer,
// the allocator of core/atlas_allocator.hpp that decides where each image goes, and the writing of
// each image into its place. The caller hands an image and gets back the receipt of the cell that
// holds it; region_of() says where the image is in texture(), and the image is drawn from there by
// the textured calls of renderer_2d.hpp, for example
//     renderer.draw_texture(atlas.texture(), {{atlas.region_of(slot).value(), mirror}, tint},
//     rect);
// A drawing call that takes the receipt itself, and refuses one whose cell was given back, is not
// part of this version.
//
// THE CELL. An image of w x h texels takes a cell of (w + 2 * margin_pixels) x
// (h + 2 * margin_pixels) texels, each side rounded UP to a multiple of 2^k, k being
// mipmap_levels. The image sits margin_pixels in from the cell's left and top edges, and every
// other texel of the cell is a copy of the image texel nearest to it (the edge extruded): a sample
// that reaches past the image's edge reads the image's own colour, never a neighbour's and never a
// transparent border. Every cell side is a multiple of 2^k, and the allocator places each
// rectangle at a sum of sides it was asked for, so every cell starts on a multiple of 2^k.
//
// MARGIN AND REDUCED LEVELS. A texel of level j covers 2^j x 2^j texels of level 0 and is their
// average (texture_2d_desc.hpp), and the linear filter reads, on each axis, the two texels of a
// level nearest the sample. Because a cell starts on a multiple of 2^k, and the atlas width and
// height are multiples of 2^k, every level up to k is made of whole texels of one cell. With
//     margin_pixels >= 2^k
// every sample of the image, at every level up to k, reads only texels of its own cell, with half a
// texel of level k to spare. The spare is for what OpenGL leaves to the driver: how a texel
// coordinate is rounded, and the precision the filter weighs the two texels with. With
// 2^k - 1 the spare is less than half a texel of level k, and open() refuses it: the rule is 2^k,
// not the least margin that fits on paper. With no reduced levels the rule reads
// margin_pixels >= 1.
//
// THE RECEIPT. add_image() returns the gltfx_atlas_slot the allocator handed out for the CELL: its
// rect is the whole cell, margin and rounding included. region_of(slot) gives the rectangle of the
// IMAGE in texels of texture(), which is what a drawing call reads. A receipt is current from
// add_image() until release_slot(); region_of() and release_slot() refuse any other, with the
// allocator's names (core/atlas_allocator.hpp, A SLOT IS A RECEIPT): a receipt of another atlas
// is "slot_other_allocator", a released, changed or never handed out one is "stale_slot".
// A region is a value: one taken from region_of() before release_slot() still names the place, and
// a drawing call reads whatever is there then.
//
// NOT A BARRIER. add_image() writes into texture() as update_texture() writes (renderer_2d.hpp): it
// does not send the batch, and the texture is read when the batch is sent. An image added into a
// cell released earlier in the same frame changes what the pieces not yet sent show; call flush()
// before add_image() to keep them as they were.
//
// THE COST. texture() is made updatable (gltfx_texture_2d_desc::updatable): with no reduced
// levels, the library keeps none of its texels; with k reduced levels, it keeps the texels of every
// level, the count gltfx_texture_2d_desc::updatable gives (at least width x height x 4 bytes, less
// than twice that). open() asks, while it makes the texture, for 4 bytes per texel of the empty
// image the texture starts from, besides what create_texture() asks for an image of that size.
// add_image() keeps, between calls, a block for the texels of a cell (at least 4 bytes per texel
// of the largest cell written so far, grown by doubling, so less than twice that) and a table of
// the image sizes (8 bytes per receipt number the allocator has handed out, plus one, grown the
// same way), besides what update_texture() asks for the cell.
//
// THE REFUSALS, each a gltfx_err, in the order in which they are checked:
//   - open(renderer, desc): invalid_argument, rejected_value() "width" or "height" when it is zero;
//     "filter" when it is not a value of gltfx_texture_filter; "mipmap_levels" when it is more than
//     floor(log2(max(width, height))) (gltfx_all_mipmap_levels among them: an atlas asks for an
//     exact count); "margin_pixels" when it is less than 2^k, or when 2 * margin_pixels + 1 is
//     more than the width or the height (no image would fit); "width" or "height" when it is not a
//     multiple of 2^k; then out_of_memory, rejected_value() "atlas_open", when the library ran out
//     of memory (os_error_code() zero); then what create_texture() returns for an image of this
//     size, as it came (the context's error, unsupported "width" or "height", out_of_memory or
//     platform_failure "texture_create"). Nothing is left behind by a refused open().
//   - add_image(image): invalid_argument "atlas" for a moved-from atlas; "width" or "height" when
//     the image has none; "pixel_format", "bytes_per_row" or "pixels" as create_texture() refuses
//     them; "width" or "height" when the cell is wider or taller than the atlas, which no release
//     can make fit; out_of_memory "atlas_add_image" when the library ran out of memory; then the
//     allocator's: no_free_space "atlas_full" when no free place of the cell's size is left now
//     (releasing cells, or a second atlas, can make room), out_of_memory "atlas_allocate"; then
//     what update_texture() returns for the cell, as it came. A refused call takes no cell. Up to
//     the allocator's refusals it touches nothing; after a refused write the cell is given back,
//     what the card holds in it is unspecified (as update_texture() says), and the receipt number
//     the call drew may later be handed out again with a larger generation.
//   - region_of(slot) and release_slot(slot): invalid_argument "atlas" for a moved-from atlas;
//     then the allocator's refusals of a receipt, in its order ("stale_slot" for an owner of 0,
//     "slot_other_allocator", "stale_slot"). release_slot() gives the cell back to the allocator;
//     its texels stay in the texture until a later cell is written over them.
//
// LIFETIME. The atlas belongs to the renderer it was opened on and does not outlive it, as a
// texture does not (texture_2d.hpp, whose release rules texture() follows). A moved-from atlas
// holds nothing: is_open() is false, texture() is a handle that is not open, and add_image(),
// region_of() and release_slot() are refused as above. An atlas that receives another by move
// assignment gives its own cells and texture back first. One atlas must not be used from two
// threads at the same time.
//
// Proved by: atlas_cell_test (each refusal of open() by name and in this order; the margin at its
// border, 2^k accepted and 2^k - 1 refused, for every k from 0 to 12; the multiple of 2^k at its
// border; the side of a cell and the place of its image; and, on the bilinear rule of each level,
// enumerated: every sample of an image keeps half a texel of level k from the neighbour's texels
// with 2^k, less with 2^k - 1, and a cell that did not start on a multiple of 2^k reaches the
// neighbour).
// Proved by: atlas_extrude_test (every texel of a cell is the image texel nearest to it, the bytes
// copied as they are, for every image side from 1 to 3, margin 1, 2 and 4 and rounding 1, 2 and 4).
// Proved by: texture_atlas_test (over a fake GL: the texture made updatable with its levels from an
// empty image; each cell written at its receipt's place with the extruded texels; region_of();
// each refusal of add_image() by name, and nothing taken after one, the library's memory failing
// at each request and a write the card refuses included; release, and a smaller image reusing the
// place reading only itself; a receipt of another atlas; every cell of a long sequence on a
// multiple of 2^k).
// Proved by: draw2d_texture_parity_test (on a real context, in both values of srgb_framebuffer: a
// cell magnified 1.5 times at a fractional position reads only its own colour, edge pixels
// included; a cell minified 0.5 times with one reduced level and 0.25 times with two, next to a
// cell of another colour, reads only its own; a cell released and taken by a smaller image reads
// only the new image).

namespace glintfx {

class gltfx_renderer_2d;
struct texture_atlas_impl;

struct gltfx_texture_atlas_desc {
    // Size of the atlas texture in texels; zero refused (see above).
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    // Texels of extruded edge on every side of each image (see MARGIN AND REDUCED LEVELS).
    std::uint32_t margin_pixels = 1;
    // How texture() is sampled, as gltfx_texture_2d_desc::filter.
    gltfx_texture_filter filter = gltfx_texture_filter::linear;
    // The number of reduced levels of texture(), exactly; zero: none (see above).
    std::uint32_t mipmap_levels = 0;
};

class gltfx_texture_atlas {
  public:
    // An atlas on `renderer`, all free. Refusals: see above.
    [[nodiscard]] GLINTFX_API static gltfx_rslt<gltfx_texture_atlas>
    open(gltfx_renderer_2d &renderer, const gltfx_texture_atlas_desc &desc) noexcept;

    gltfx_texture_atlas(const gltfx_texture_atlas &) = delete;
    gltfx_texture_atlas &operator=(const gltfx_texture_atlas &) = delete;
    GLINTFX_API gltfx_texture_atlas(gltfx_texture_atlas &&other) noexcept;
    GLINTFX_API gltfx_texture_atlas &operator=(gltfx_texture_atlas &&other) noexcept;
    GLINTFX_API ~gltfx_texture_atlas();

    // False only for a moved-from atlas.
    [[nodiscard]] GLINTFX_API bool is_open() const noexcept;

    // The texture every image of this atlas is in (see above).
    [[nodiscard]] GLINTFX_API const gltfx_texture_2d &texture() const noexcept;

    // Writes `image` into a cell of its own and returns the receipt of the cell. The pixels are
    // read only during this call. Refusals: see above.
    [[nodiscard]] GLINTFX_API gltfx_rslt<gltfx_atlas_slot>
    add_image(gltfx_image_view image) noexcept;

    // Where the image of `slot` is in texture(), in texels. Refusals: see above.
    [[nodiscard]] GLINTFX_API gltfx_rslt<gltfx_rect_texel>
    region_of(gltfx_atlas_slot slot) const noexcept;

    // Gives the cell of `slot` back. Refusals: see above.
    [[nodiscard]] GLINTFX_API gltfx_rslt<void> release_slot(gltfx_atlas_slot slot) noexcept;

  private:
    gltfx_texture_atlas(texture_atlas_impl *interior, texture_2d_impl *made) noexcept
        : impl(interior), atlas_texture(made) {}
    texture_atlas_impl *impl = nullptr;
    gltfx_texture_2d atlas_texture;
};

} // namespace glintfx
