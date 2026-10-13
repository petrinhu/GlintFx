// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <optional>

#include <glintfx/core/err.hpp>
#include <glintfx/core/image_view.hpp>

#include "draw2d/mipmap_reduce.hpp"
#include "draw2d/texture_2d_impl.hpp"

// draw2d/texture_update_check.hpp - R2D-TEXTURE, fatia T3: the PURE check of one update_texture()
// call, before the context is touched and before any memory is reserved. It knows nothing of GL.
//
// THE ORDER of the refusals (invalid_argument, rejected_value() naming what is wrong), the order
// gltfx_renderer_2d::update_texture() lists them in:
//   1. "texture": the handle is not open (it has no record), or the texture belongs to another
//      renderer (its owner is not the renderer the call was made on);
//   2. "updatable": the texture was not made with gltfx_texture_2d_desc::updatable (the record's
//      `updatable` is false);
//   3. "pixel_format", "bytes_per_row", "pixels": the view itself (check_image_view() of
//      pixel_premultiply.hpp, the one definition of those three checks);
//   4. "rect": the rectangle that starts at texel (x, y), of the image's size, reaches past the
//      texture. The sums are made in 64 bits, so no wrap of 32 bits lets a rectangle pass; a
//      rectangle with no width may start at x = the texture's width (and likewise for y).
// An image with no width or no height that passes 1 to 4 is ACCEPTED with no rectangle
// (std::nullopt): an empty update succeeds and writes nothing (decided in the tenth consultation).
namespace glintfx::draw2d {

// One call, as the frame machine gives it: the texture's record (null for a handle that is not
// open), the renderer the call was made on, the corner and the image.
struct texture_update_request {
    const glintfx::texture_2d_impl *texture = nullptr;
    const void *renderer = nullptr;
    std::uint32_t x = 0U;
    std::uint32_t y = 0U;
    glintfx::gltfx_image_view image{};
};

// The rectangle of texels of level 0 the update overwrites (columns x to x + width - 1, rows y to
// y + height - 1); std::nullopt for an empty update; or the first refusal in the order above.
[[nodiscard]] glintfx::gltfx_rslt<std::optional<texel_rect>>
check_texture_update(const texture_update_request &request) noexcept;

} // namespace glintfx::draw2d
