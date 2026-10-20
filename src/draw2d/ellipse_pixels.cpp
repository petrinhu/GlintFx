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

// The outline of one stroke call in the plane of its thickness unit (D-W8-273): the screen ellipse
// (the arc's parameters come from it), the ellipse in the plane in both forms, the half thickness
// and its reach on the screen (by the largest stretch of the transform for world unit).
struct stroke_plane {
    conjugate_ellipse on_screen_ellipse{};
    conjugate_ellipse plane{};
    principal_ellipse principal{};
    double half_thickness = 0.0;
    double reach_pixels = 0.0;
    bool world_plane = false;
};

[[nodiscard]] stroke_plane stroke_plane_of(const glintfx::gltfx_ellipse_world &ellipse,
                                           const glintfx::gltfx_stroke &stroke,
                                           const pixel_affine &affine) noexcept {
    const bool world_plane = stroke.unit == glintfx::gltfx_stroke_unit::world;
    const double half_thickness = 0.5 * stroke.thickness;
    const conjugate_ellipse on_screen_ellipse = screen_conjugate_ellipse(ellipse, affine);
    const conjugate_ellipse plane =
        world_plane ? world_conjugate_ellipse(ellipse) : on_screen_ellipse;
    return stroke_plane{
        .on_screen_ellipse = on_screen_ellipse,
        .plane = plane,
        .principal = principal_of(plane),
        .half_thickness = half_thickness,
        .reach_pixels = world_plane ? screen_radius(half_thickness, affine) : half_thickness,
        .world_plane = world_plane,
    };
}

// The normal angles of the outline (D-W8-416), in increasing order, into `angles`: the arc's
// parameters of the SCREEN ellipse, with HALF the largest error and the band's reach on the screen
// (so the surface cuts the band only where it does not show), joined to the uniform normal angles,
// which take the other half. 0 when the screen ellipse cannot be measured.
[[nodiscard]] std::size_t write_stroke_angles(const stroke_plane &frame,
                                              const ellipse_screen &screen,
                                              std::span<double> parameters,
                                              std::span<double> angles) noexcept {
    const arc_request request{pixel_ellipse_of(frame.on_screen_ellipse), arc_extent::whole,
                              0.5 * screen.max_error_pixels, frame.reach_pixels};
    const std::size_t count = arc_parameters(request, screen.surface, parameters);
    if (count < 2U) {
        return 0U;
    }
    const outline_angle_sources sources{
        parameters.first(count - 1U),
        uniform_normal_angle_count(frame.reach_pixels, screen.max_error_pixels)};
    return merge_outline_angles(frame.plane, frame.principal, sources, angles);
}

// How many points write_band_positions() wrote, and whether they are a ring.
struct band_points {
    std::size_t count = 0;
    bool ring = false;
};

// The points of the band at `angles`, carried to the screen, into `positions`: with no hole, a fan
// (the centre, then the outer points); with a hole, a ring (the outer points, then the inner ones).
[[nodiscard]] band_points write_band_positions(const stroke_plane &frame,
                                               std::span<const double> angles,
                                               const pixel_affine &affine,
                                               std::span<pixel_position_wide> positions) noexcept {
    const outline_band band{frame.half_thickness,
                            outline_shape_of(frame.principal, frame.half_thickness)};
    const std::size_t n = angles.size();
    if (band.shape.hole == outline_hole::none) {
        positions[0] = on_screen(frame.principal.center, frame.world_plane, affine);
        for (std::size_t k = 0U; k < n; ++k) {
            positions[k + 1U] = on_screen(outline_pair_at(frame.principal, band, angles[k]).outer,
                                          frame.world_plane, affine);
        }
        return band_points{n + 1U, false};
    }
    for (std::size_t k = 0U; k < n; ++k) {
        const outline_pair pair = outline_pair_at(frame.principal, band, angles[k]);
        positions[k] = on_screen(pair.outer, frame.world_plane, affine);
        positions[n + k] = on_screen(pair.inner, frame.world_plane, affine);
    }
    return band_points{2U * n, true};
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
    return publish_band_if_in_range(rim + 1U, color, false);
}

ellipse_pixels_outcome ellipse_pixels::build_stroke(const glintfx::gltfx_ellipse_world &ellipse,
                                                    const glintfx::gltfx_stroke &stroke,
                                                    glintfx::gltfx_rgba color,
                                                    const ellipse_screen &screen) noexcept {
    point_count = 0;
    index_count = 0;
    const stroke_plane frame = stroke_plane_of(ellipse, stroke, screen.affine);
    const std::size_t n = write_stroke_angles(frame, screen, parameters, angles);
    if (n == 0U) {
        return ellipse_pixels_outcome::pixel_position_out_of_range;
    }
    const band_points band = write_band_positions(frame, std::span<const double>(angles.data(), n),
                                                  screen.affine, positions);
    return publish_band_if_in_range(band.count, color, band.ring);
}

std::size_t ellipse_pixels::write_band_indices(std::span<const pixel_position_wide> points,
                                               bool ring) noexcept {
    return ring ? write_ring_indices(points, ellipse_indices)
                : write_fan_indices(points.size() - 1U, ellipse_indices);
}

void ellipse_pixels::write_band_vertices(std::size_t count, glintfx::gltfx_rgba color) noexcept {
    for (std::size_t k = 0U; k < count; ++k) {
        pixels[k] = colored_pixel_vertex{narrowed_to_pixel(positions[k]), color};
    }
}

ellipse_pixels_outcome ellipse_pixels::publish_band_if_in_range(std::size_t count,
                                                                glintfx::gltfx_rgba color,
                                                                bool ring) noexcept {
    const std::span<const pixel_position_wide> points(positions.data(), count);
    // Every point must be in the pixel range; one that is not refuses the whole ellipse (I-7,
    // S2b-fix1). A NaN is in no range, so it is refused too.
    if (!all_pixel_positions_in_range(points)) {
        return ellipse_pixels_outcome::pixel_position_out_of_range;
    }
    const std::size_t indices_written = write_band_indices(points, ring);
    if (indices_written == 0U) {
        return ellipse_pixels_outcome::pixel_position_out_of_range;
    }
    write_band_vertices(count, color);
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
