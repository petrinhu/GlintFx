// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>
#include <optional>

#include <glintfx/draw2d/texture_region.hpp>

#include "draw2d/texture_extent.hpp"

// draw2d/texture_region_uv.hpp - R2D-TEXTURE, fatia T4b-A (D-W8-297, D-W8-456): where the four
// corners of a drawn piece read the texture, as texture coordinates (0 to 1 across the texture),
// from the region in texels and its mirroring. Pure: no GL, no window, allocates nothing.
//
// THE CORNERS are the roles of core/quad.hpp: top_left, top_right, bottom_right, bottom_left. With
// no mirroring the top-left corner of the piece reads the top-left of the source rectangle; the
// first row of an uploaded image is its top row, v = 0 (the convention of triangle_batch.hpp).
// A coordinate is a texel coordinate divided by the size: the EDGE of a texel, never its centre, so
// a source of the whole texture is exactly 0 to 1, and a filter samples the texels it covers.
// `flip_left_right` exchanges the left and right columns of the four, `flip_top_bottom` the top and
// bottom rows, `flip_left_right_and_top_bottom` both.
//
// NO COORDINATES (std::nullopt) for a mirroring this build does not know (it is refused as
// `unknown_option` before, texture_region_check.hpp; never taken for `none`), and for a texture
// with no width or no height.
namespace glintfx::draw2d {

struct texture_coordinate {
    double u = 0.0;
    double v = 0.0;
};

[[nodiscard]] std::optional<std::array<texture_coordinate, 4>>
region_corner_coordinates(const glintfx::gltfx_texture_region &region,
                          texture_extent extent) noexcept;

} // namespace glintfx::draw2d
