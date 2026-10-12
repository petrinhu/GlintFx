// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

#include <glintfx/core/err.hpp>
#include <glintfx/core/image_view.hpp>
#include <glintfx/draw2d/texture_2d_desc.hpp>

// draw2d/texture_request_check.hpp - R2D-TEXTURE, fatia T2a: the PURE check of one request to make
// a texture, before any memory is reserved and before GL is touched. It knows nothing of GL or of
// the renderer; the size limit of the graphics card is read and checked by texture_upload.hpp.
//
// THE ORDER of the refusals (invalid_argument, rejected_value() naming the field), the order the
// public text of gltfx_renderer_2d::create_texture() lists them in:
//   1. "width", then "height": zero;
//   2. "pixel_format", "bytes_per_row", "pixels": the view itself (check_image_view() of
//      pixel_premultiply.hpp, the one definition of those three checks);
//   3. "filter": not a value of gltfx_texture_filter;
//   4. "mipmap_levels": more reduced levels than reduced_level_limit() allows, except the value
//      gltfx_all_mipmap_levels, which asks for exactly that limit.
namespace glintfx::draw2d {

// How many reduced levels a texture of this size can have: floor(log2(max(width, height))), the
// levels whose side, halved and rounded down, has not yet reached one texel on both axes. Zero for
// 1 x 1. Undefined for a zero side (the request check refuses it first).
[[nodiscard]] std::uint32_t reduced_level_limit(std::uint32_t width, std::uint32_t height) noexcept;

// The checked request: the number of reduced levels to make (gltfx_all_mipmap_levels resolved to
// the limit), or the first refusal in the order above.
[[nodiscard]] glintfx::gltfx_rslt<std::uint32_t>
check_texture_request(const glintfx::gltfx_image_view &image,
                      const glintfx::gltfx_texture_2d_desc &desc) noexcept;

} // namespace glintfx::draw2d
