// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/quad_vertices.hpp"

#include <cmath>

namespace glintfx::draw2d {

// cppcheck-suppress passedByValue ; reason: gltfx_transform is a VALUE type and value types go by
// value (docs/auditoria-api-draw2d.md B0-I10; the public begin_batch(gltfx_transform) of B4 takes
// it the same way): the copy is 5 doubles, and a const reference would make the caller's lifetime
// part of the contract.
pixel_affine pixel_affine_from_transform(gltfx_transform transform) noexcept {
    // Everything in DOUBLE - the world side. Nothing is narrowed here, the translation included
    // (B0-C1): the first and only narrowing is the finished pixel position in quad_vertices().
    const double cosine = std::cos(transform.rotation.radians);
    const double sine = std::sin(transform.rotation.radians);
    // Scale first, then rotation: the scale of an axis rides on the column that axis owns. The
    // translation is applied last and enters untouched by either.
    return pixel_affine{
        .column0_x = transform.scale.x * cosine,
        .column0_y = transform.scale.x * sine,
        .column1_x = -transform.scale.y * sine,
        .column1_y = transform.scale.y * cosine,
        .translation_x = transform.translation.x,
        .translation_y = transform.translation.y,
    };
}

namespace {
[[nodiscard]] gltfx_vec2_screen pixel_of(const gltfx_vec2_world &world,
                                         const pixel_affine &affine) noexcept {
    const double x = affine.column0_x * world.x + affine.column1_x * world.y + affine.translation_x;
    const double y = affine.column0_y * world.x + affine.column1_y * world.y + affine.translation_y;
    return gltfx_vec2_screen{static_cast<float>(x), static_cast<float>(y)};
}
} // namespace

quad_corners_pixel quad_vertices(const quad_corners_world &corners,
                                 const pixel_affine &affine) noexcept {
    return quad_corners_pixel{pixel_of(corners[0], affine), pixel_of(corners[1], affine),
                              pixel_of(corners[2], affine), pixel_of(corners[3], affine)};
}

} // namespace glintfx::draw2d
