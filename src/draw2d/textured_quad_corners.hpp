// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <optional>

#include <glintfx/core/quad.hpp>
#include <glintfx/draw2d/texture_region.hpp>

#include "draw2d/texture_extent.hpp"
#include "draw2d/triangle_batch.hpp"

// draw2d/textured_quad_corners.hpp - R2D-TEXTURE, fatias T4b-B1-fix1 (D-W8-506) and T4b-A-FLOAT
// (D-W8-716 to D-W8-735): where each corner of a textured quad reads its texture, and with what
// weight q - the MAPPING of the textured drawing, an atom of its own, apart from the refusals of
// the frame machine (renderer_2d_impl_textures.cpp). Pure: no GL, no renderer, allocates nothing.
//
// THE CORNERS are the roles of core/quad.hpp. Each one gets the texture coordinate of the region
// at that role (texture_region_uv.hpp, with the mirroring) and a weight: 1 at every corner when
// `projective` is false (draw_texture(): a rectangle, read affinely), the weights of
// projective_quad.hpp when it is true (draw_texture_quad()). The quad stores (u q, v q, q)
// (triangle_batch.hpp).
//
// THE WEIGHTS THE VERTEX CARRIES (T4b-A-FLOAT): the four weights times ONE power of two, which
// leaves the mapping as it was (the program divides u q and v q by q, so a factor common to the
// four cancels). The power is 1 while the largest weight is below 2^32, so those weights reach the
// vertex as projective_quad.hpp gives them; above, it is the one that brings the largest below
// 2^32. Every carried weight is then between 2^-32 and 2^32: OpenGL promises texture coordinates
// up to 2^32 in magnitude (OpenGL 3.3 core, section 2.1.1), and a projective weight is above 1/2,
// so the smallest does not fall under 2^-32 while the largest is below 2^63. Each one is the
// exact weight times the power, within 2^-23 (the 2^-50 of projective_quad.hpp, then the one
// rounding to float). No weight reaches the float conversion out of the float range.
//
// NO CORNERS (std::nullopt) when the quadrilateral has no projective mapping, when it has one
// whose largest weight is 2^63 or more (a corner closer than about 2^-64 of its diagonal to the
// crossing; this includes the weight that is not a double, which projective_quad.hpp already
// refuses): the caller refuses both as degenerate_quad. Also when the region has no coordinates
// (a mirroring this build does not know, a texture of no size: both refused before the frame
// machine asks).
namespace glintfx::draw2d {

[[nodiscard]] std::optional<quad_texture_corners>
textured_quad_corners(const glintfx::gltfx_texture_region &region, texture_extent extent,
                      const glintfx::gltfx_quad_world &corners, bool projective) noexcept;

} // namespace glintfx::draw2d
