// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/ellipse_pixels.hpp"

#include <cstddef>
#include <span>

#include "draw2d/arc_parameters.hpp"
#include "draw2d/arc_segments.hpp"
#include "draw2d/ellipse_frame.hpp"
#include "draw2d/ellipse_outline.hpp"
#include "draw2d/ellipse_triangles.hpp"
#include "draw2d/quad_vertices.hpp"

// draw2d/ellipse_pixels.cpp - R2D-SHAPES, fatia S5-B: the builds of ellipse_pixels.hpp.
namespace glintfx::draw2d {

namespace {

[[nodiscard]] pixel_ellipse pixel_ellipse_of(const conjugate_ellipse &screen) noexcept {
    return pixel_ellipse{{screen.center.x, screen.center.y},
                         {screen.axis_u.x, screen.axis_u.y},
                         {screen.axis_v.x, screen.axis_v.y}};
}

// A point of the plane of the thickness unit, on the screen: by the batch transform for world unit,
// as it is for pixel unit (the plane is the screen already).
[[nodiscard]] pixel_position_wide on_screen(const plane_point &point, bool world_plane,
                                            const pixel_affine &affine) noexcept {
    if (world_plane) {
        return pixel_position_of(gltfx_vec2_world{point.x, point.y}, affine);
    }
    return pixel_position_wide{point.x, point.y};
}

} // namespace

ellipse_pixels_outcome ellipse_pixels::build_fill(const glintfx::gltfx_ellipse_world &ellipse,
                                                  glintfx::gltfx_rgba color,
                                                  const ellipse_screen &screen) noexcept {
    point_count = 0;
    index_count = 0;
    const conjugate_ellipse on_screen_ellipse = screen_conjugate_ellipse(ellipse, screen.affine);
    const arc_request request{pixel_ellipse_of(on_screen_ellipse), arc_extent::whole,
                              screen.max_error_pixels, 0.0};
    const std::size_t count = arc_parameters(request, screen.surface, parameters);
    if (count < 2U) {
        return ellipse_pixels_outcome::pixel_position_out_of_range;
    }
    // The centre, then one point per parameter but the closing one (t = 2 pi is t = 0).
    const std::size_t rim = count - 1U;
    positions[0] = {on_screen_ellipse.center.x, on_screen_ellipse.center.y};
    for (std::size_t k = 0U; k < rim; ++k) {
        const plane_point offset = offset_at_parameter(on_screen_ellipse, parameters[k]);
        positions[k + 1U] = {on_screen_ellipse.center.x + offset.x,
                             on_screen_ellipse.center.y + offset.y};
    }
    return finish(rim + 1U, color, false);
}

ellipse_pixels_outcome ellipse_pixels::build_stroke(const glintfx::gltfx_ellipse_world &ellipse,
                                                    const glintfx::gltfx_stroke &stroke,
                                                    glintfx::gltfx_rgba color,
                                                    const ellipse_screen &screen) noexcept {
    point_count = 0;
    index_count = 0;
    const bool world_plane = stroke.unit == glintfx::gltfx_stroke_unit::world;
    const double half_thickness = 0.5 * stroke.thickness;
    const double reach =
        world_plane ? screen_radius(half_thickness, screen.affine) : half_thickness;
    const conjugate_ellipse on_screen_ellipse = screen_conjugate_ellipse(ellipse, screen.affine);
    const conjugate_ellipse plane =
        world_plane ? world_conjugate_ellipse(ellipse) : on_screen_ellipse;
    const arc_request request{pixel_ellipse_of(on_screen_ellipse), arc_extent::whole,
                              0.5 * screen.max_error_pixels, reach};
    const std::size_t count = arc_parameters(request, screen.surface, parameters);
    if (count < 2U) {
        return ellipse_pixels_outcome::pixel_position_out_of_range;
    }
    const principal_ellipse principal = principal_of(plane);
    const outline_angle_sources sources{std::span<const double>(parameters.data(), count - 1U),
                                        uniform_normal_angle_count(reach, screen.max_error_pixels)};
    const std::size_t n = write_outline_angles(plane, principal, sources, angles);
    const outline_band band{half_thickness, outline_shape_of(principal, half_thickness)};
    if (band.shape.hole == outline_hole::none) {
        positions[0] = on_screen(principal.center, world_plane, screen.affine);
        for (std::size_t k = 0U; k < n; ++k) {
            positions[k + 1U] = on_screen(outline_pair_at(principal, band, angles[k]).outer,
                                          world_plane, screen.affine);
        }
        return finish(n + 1U, color, false);
    }
    for (std::size_t k = 0U; k < n; ++k) {
        const outline_pair pair = outline_pair_at(principal, band, angles[k]);
        positions[k] = on_screen(pair.outer, world_plane, screen.affine);
        positions[n + k] = on_screen(pair.inner, world_plane, screen.affine);
    }
    return finish(2U * n, color, true);
}

ellipse_pixels_outcome ellipse_pixels::finish(std::size_t count, glintfx::gltfx_rgba color,
                                              bool ring) noexcept {
    const std::span<const pixel_position_wide> points(positions.data(), count);
    // Every point must be in the pixel range; one that is not refuses the whole ellipse (I-7,
    // S2b-fix1). A NaN is in no range, so it is refused too.
    if (!all_pixel_positions_in_range(points)) {
        return ellipse_pixels_outcome::pixel_position_out_of_range;
    }
    const std::size_t indices_written = ring ? write_ring_indices(points, ellipse_indices)
                                             : write_fan_indices(count - 1U, ellipse_indices);
    if (indices_written == 0U) {
        return ellipse_pixels_outcome::pixel_position_out_of_range;
    }
    for (std::size_t k = 0U; k < count; ++k) {
        pixels[k] = colored_pixel_vertex{narrowed_to_pixel(positions[k]), color};
    }
    point_count = count;
    index_count = indices_written;
    return ellipse_pixels_outcome::built;
}

std::span<const colored_pixel_vertex> ellipse_pixels::vertices() const noexcept {
    return std::span<const colored_pixel_vertex>(pixels.data(), point_count);
}

std::span<const std::uint32_t> ellipse_pixels::indices() const noexcept {
    return std::span<const std::uint32_t>(ellipse_indices.data(), index_count);
}

} // namespace glintfx::draw2d
