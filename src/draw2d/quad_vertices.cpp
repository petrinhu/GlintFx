// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/quad_vertices.hpp"

#include <cmath>
#include <limits>

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

pixel_position_wide pixel_position_of(const gltfx_vec2_world &world,
                                      const pixel_affine &affine) noexcept {
    return pixel_position_wide{
        affine.column0_x * world.x + affine.column1_x * world.y + affine.translation_x,
        affine.column0_y * world.x + affine.column1_y * world.y + affine.translation_y};
}

gltfx_vec2_screen narrowed_to_pixel(const pixel_position_wide &position) noexcept {
    return gltfx_vec2_screen{static_cast<float>(position.x), static_cast<float>(position.y)};
}

namespace {
// The largest pixel coordinate the library's vertex program takes without overflowing (S2b-fix1,
// I-7). Derived from the two lines of k_vertex_source in src/draw2d/embedded_program.cpp:
//     clip.x = a_position.x / u_viewport_pixels.x * 2.0 - 1.0
//     clip.y = 1.0 - a_position.y / u_viewport_pixels.y * 2.0
// The operation that overflows is the `* 2.0` after the division. The divisor is the surface size
// in pixels, at least 1 for a surface that has pixels, so the worst case is a surface 1 pixel wide,
// where the division hands the coordinate back: twice it must stay within FLT_MAX, so the
// coordinate within FLT_MAX / 2. The division is not exact on the card (GL_ARB_shader_precision:
// a / b within 2.5 units in the last place; a * b and a - b correctly rounded), and narrowing the
// double to float adds half a unit, so the bound is FLT_MAX / 2 less 4 units: a relative 2^-21.
// The `- 1.0` cannot overflow a value within FLT_MAX. The GLSL cannot read this constant:
// renderer_2d_impl_test checks the two lines word for word and runs them on the vertex the card
// receives at this edge, and quad_vertices_test checks both sides of it.
constexpr double k_largest_pixel_coordinate =
    0.5 * static_cast<double>(std::numeric_limits<float>::max()) * (1.0 - 0x1p-21);
} // namespace

bool pixel_position_in_range(const pixel_position_wide &position) noexcept {
    // A NaN fails both comparisons, and so does an infinity.
    return std::fabs(position.x) <= k_largest_pixel_coordinate &&
           std::fabs(position.y) <= k_largest_pixel_coordinate;
}

bool quad_in_pixel_range(const quad_corners_world &corners, const pixel_affine &affine) noexcept {
    for (const gltfx_vec2_world &corner : corners) {
        if (!pixel_position_in_range(pixel_position_of(corner, affine))) {
            return false;
        }
    }
    return true;
}

namespace {
[[nodiscard]] gltfx_vec2_screen pixel_of(const gltfx_vec2_world &world,
                                         const pixel_affine &affine) noexcept {
    return narrowed_to_pixel(pixel_position_of(world, affine));
}
} // namespace

quad_corners_pixel quad_vertices(const quad_corners_world &corners,
                                 const pixel_affine &affine) noexcept {
    return quad_corners_pixel{pixel_of(corners[0], affine), pixel_of(corners[1], affine),
                              pixel_of(corners[2], affine), pixel_of(corners[3], affine)};
}

} // namespace glintfx::draw2d
