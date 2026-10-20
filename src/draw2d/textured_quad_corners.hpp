// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <optional>

#include <glintfx/core/quad.hpp>
#include <glintfx/draw2d/texture_region.hpp>

#include "draw2d/texture_extent.hpp"
#include "draw2d/triangle_batch.hpp"

// draw2d/textured_quad_corners.hpp - R2D-TEXTURE, fatia T4b-B1-fix1 (D-W8-506): where each corner
// of a textured quad reads its texture, and with what weight q - the MAPPING of the textured
// drawing, an atom of its own, apart from the refusals of the frame machine
// (renderer_2d_impl_textures.cpp). Pure: no GL, no renderer, allocates nothing.
//
// THE CORNERS are the roles of core/quad.hpp. Each one gets the texture coordinate of the region
// at that role (texture_region_uv.hpp, with the mirroring) and a weight: 1 at every corner when
// `projective` is false (draw_texture(): a rectangle, read affinely), the weights of
// projective_quad.hpp when it is true (draw_texture_quad()). The quad stores (u q, v q, q)
// (triangle_batch.hpp).
//
// NO CORNERS (std::nullopt) when the quadrilateral has no projective mapping, or has one with a
// weight that is not a double (`projective` true and projective_quad.hpp answers nothing: the
// caller refuses it as degenerate_quad), and when the
// region has no coordinates (a mirroring this build does not know, a texture of no size: both
// refused before the frame machine asks).
namespace glintfx::draw2d {

[[nodiscard]] std::optional<quad_texture_corners>
textured_quad_corners(const glintfx::gltfx_texture_region &region, texture_extent extent,
                      const glintfx::gltfx_quad_world &corners, bool projective) noexcept;

} // namespace glintfx::draw2d
