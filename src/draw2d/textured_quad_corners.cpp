// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/textured_quad_corners.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

#include "draw2d/projective_quad.hpp"
#include "draw2d/texture_region_uv.hpp"

// draw2d/textured_quad_corners.cpp - R2D-TEXTURE, fatias T4b-B1-fix1 and T4b-A-FLOAT: the mapping
// of textured_quad_corners.hpp.
namespace glintfx::draw2d {

namespace {

// The binary exponent of the largest weight the vertex carries: below 2^32 (T4b-A-FLOAT).
constexpr int k_carried_top_exponent = 31;

// The binary exponent from which the largest weight is refused: 2^63 or more. Every other weight
// is above 1/2, so after the shift that brings the largest below 2^32 it is still 2^-32 or more.
constexpr int k_refused_top_exponent = 63;

// The four weights the vertex carries: the same four times one power of two, the largest below
// 2^32 (unchanged when it already is); none when the largest is 2^63 or more.
[[nodiscard]] std::optional<std::array<double, 4>>
carried_weights(const std::array<double, 4> &weights) noexcept {
    const int top = std::ilogb(*std::ranges::max_element(weights));
    if (top >= k_refused_top_exponent) {
        return std::nullopt;
    }
    const int shift = std::min(0, k_carried_top_exponent - top);
    std::array<double, 4> carried{};
    for (std::size_t corner = 0; corner < carried.size(); ++corner) {
        carried[corner] = std::ldexp(weights[corner], shift);
    }
    return carried;
}

// The weight of each corner before it is carried: 1 everywhere for a rectangle, the projective
// weights for a quadrilateral (none when it has no mapping).
[[nodiscard]] std::optional<std::array<double, 4>>
weights_of(const glintfx::gltfx_quad_world &corners, bool projective) noexcept {
    if (!projective) {
        return std::array<double, 4>{1.0, 1.0, 1.0, 1.0};
    }
    return projective_weights(corners);
}

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
        weights_of(corners, projective).and_then(carried_weights);
    const std::optional<std::array<texture_coordinate, 4>> reads =
        region_corner_coordinates(region, extent);
    if (!weights.has_value() || !reads.has_value()) {
        return std::nullopt;
    }
    return weighted(*reads, *weights);
}

} // namespace glintfx::draw2d
