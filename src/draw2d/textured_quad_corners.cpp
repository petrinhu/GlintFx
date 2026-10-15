// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/textured_quad_corners.hpp"

#include <array>
#include <cstddef>

#include "draw2d/projective_quad.hpp"
#include "draw2d/texture_region_uv.hpp"

// draw2d/textured_quad_corners.cpp - R2D-TEXTURE, fatia T4b-B1-fix1: the mapping of
// textured_quad_corners.hpp.
namespace glintfx::draw2d {

namespace {

// Where each corner reads the texture, with its weight.
[[nodiscard]] quad_texture_corners weighted(const std::array<texture_coordinate, 4> &reads,
                                            const std::array<double, 4> &weights) noexcept {
    quad_texture_corners corners{};
    for (std::size_t corner = 0; corner < corners.size(); ++corner) {
        corners[corner] = quad_texture_corner{static_cast<float>(reads[corner].u),
                                              static_cast<float>(reads[corner].v),
                                              static_cast<float>(weights[corner])};
    }
    return corners;
}

} // namespace

std::optional<quad_texture_corners>
textured_quad_corners(const glintfx::gltfx_texture_region &region, texture_extent extent,
                      const glintfx::gltfx_quad_world &corners, bool projective) noexcept {
    const std::optional<std::array<double, 4>> weights =
        projective ? projective_weights(corners)
                   : std::optional<std::array<double, 4>>{{1.0, 1.0, 1.0, 1.0}};
    const std::optional<std::array<texture_coordinate, 4>> reads =
        region_corner_coordinates(region, extent);
    if (!weights.has_value() || !reads.has_value()) {
        return std::nullopt;
    }
    return weighted(*reads, *weights);
}

} // namespace glintfx::draw2d
