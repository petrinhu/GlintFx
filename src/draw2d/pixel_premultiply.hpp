// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <span>

#include <glintfx/core/color.hpp>
#include <glintfx/core/err.hpp>
#include <glintfx/core/image_view.hpp>

// draw2d/pixel_premultiply.hpp - R2D-TEXTURE, fatia T1: the conversion of a caller's image view
// into the texels the library stores. Pure function over plain data: no GL, no operating system.
//
// THE STORED TEXEL: each color channel is the sRGB encoding of the PREMULTIPLIED linear light,
// scaled to 8 bits (encode(linear * alpha) * 255, rounded). The alpha is stored as the caller gave
// it, never encoded (it is coverage, not light). The stored texel is therefore always sRGB encoded,
// whatever format the caller wrote, so one stored format serves every input.
//
// THE FOUR INPUT FORMATS (core/image_view.hpp). A STRAIGHT input has its color not multiplied by
// alpha: the channel is taken in the caller's encoding (sRGB or linear), and the conversion
// multiplies it by alpha in LINEAR light. A PREMULTIPLIED input had its color multiplied by alpha
// in the caller's encoding. That is undone in the same encoding: channel / alpha, limited to 1
// (a color above its alpha cannot belong to a pixel that exists). Then it goes to linear light if
// sRGB, and is multiplied by alpha again in linear light. An alpha of zero gives a zero color, so
// there is no division by zero.
//
// THE CURVE (IEC 61966-2-1), reused and never copied: the encode is srgb_encode() of
// draw2d/srgb_encoding.hpp, and the decode is srgb_channel_to_linear() of core/srgb_transfer.hpp,
// the same one core/color.cpp applies. The decode takes a FRACTIONAL encoded value, because
// premultiplied input is undone in its own encoding before it goes to linear light.
//
// NO ALLOCATION (docs/api-conventions.md R3, rung 1): the caller provides the texel storage, so the
// function never allocates and its noexcept is kept by construction, not by a try block.
//
// REFUSED BY NAME (invalid_argument, rejected_value()): an unspecified or unknown pixel_format, a
// bytes_per_row smaller than width * 4 ("bytes_per_row"), a pixels span shorter than
// height * bytes_per_row ("pixels"), and a texel storage whose size is not width * height
// ("texels"). Zero width or height is not refused here; the texture call refuses it.
namespace glintfx::draw2d {

// Writes the stored texels of the image into `texels`, row by row, left to right: texel (x, y) is
// at index y * width + x.
[[nodiscard]] glintfx::gltfx_rslt<void>
to_stored_texels(const glintfx::gltfx_image_view &view,
                 std::span<glintfx::gltfx_rgba8> texels) noexcept;

} // namespace glintfx::draw2d
