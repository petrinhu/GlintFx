// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/stroke_geometry.hpp"

#include <algorithm>
#include <cmath>

// draw2d/stroke_geometry.cpp - R2D-SHAPES, fatia S3: the frame, the outline and the fan of
// stroke_geometry.hpp.
namespace glintfx::draw2d {

namespace {

// A unit vector: the direction of a line on the space of its unit.
struct unit_direction {
    double x = 1.0;
    double y = 0.0;
};

// The direction of a line from HALF of its displacement: halves do not overflow, and dividing by
// the larger component first keeps the length finite. A line of zero length takes +x (SVG 2).
[[nodiscard]] unit_direction direction_of(double half_dx, double half_dy) noexcept {
    const double larger = std::max(std::fabs(half_dx), std::fabs(half_dy));
    if (larger == 0.0) {
        return unit_direction{1.0, 0.0};
    }
    const double scaled_x = half_dx / larger;
    const double scaled_y = half_dy / larger;
    const double length = std::hypot(scaled_x, scaled_y);
    return unit_direction{scaled_x / length, scaled_y / length};
}

// The linear part of the transform applied to a vector: no translation, as for a direction.
[[nodiscard]] pixel_offset apply_linear(const pixel_affine &affine, double vector_x,
                                        double vector_y) noexcept {
    return pixel_offset{(affine.column0_x * vector_x) + (affine.column1_x * vector_y),
                        (affine.column0_y * vector_x) + (affine.column1_y * vector_y)};
}

// The points of a cap: its parameters strictly between 0 and pi, as many as there are; none for a
// cap of fewer than three.
[[nodiscard]] std::size_t cap_point_count(std::span<const double> parameters) noexcept {
    return parameters.size() < 3U ? 0U : parameters.size() - 2U;
}

// The half ellipse `disc` at parameters[1] .. parameters[size - 2], into `out`, one point each.
void write_half_disc(const pixel_ellipse &disc, std::span<const double> parameters,
                     std::span<pixel_position_wide> out) noexcept {
    for (std::size_t k = 0U; k < out.size(); ++k) {
        const double cosine = std::cos(parameters[k + 1U]);
        const double sine = std::sin(parameters[k + 1U]);
        out[k] =
            pixel_position_wide{disc.center.x + (disc.axis_u.x * cosine) + (disc.axis_v.x * sine),
                                disc.center.y + (disc.axis_u.y * cosine) + (disc.axis_v.y * sine)};
    }
}

} // namespace

stroke_frame stroke_frame_of(const glintfx::gltfx_segment_world &segment,
                             const glintfx::gltfx_stroke &stroke,
                             const pixel_affine &affine) noexcept {
    stroke_frame frame{};
    const double half_thickness = 0.5 * stroke.thickness;
    frame.start = pixel_position_of(segment.start, affine);
    frame.end = pixel_position_of(segment.end, affine);
    if (stroke.unit == glintfx::gltfx_stroke_unit::pixel) {
        const unit_direction on_screen = direction_of((0.5 * frame.end.x) - (0.5 * frame.start.x),
                                                      (0.5 * frame.end.y) - (0.5 * frame.start.y));
        frame.along = pixel_offset{on_screen.x * half_thickness, on_screen.y * half_thickness};
        frame.across = pixel_offset{-on_screen.y * half_thickness, on_screen.x * half_thickness};
        return frame;
    }
    const unit_direction in_world = direction_of((0.5 * segment.end.x) - (0.5 * segment.start.x),
                                                 (0.5 * segment.end.y) - (0.5 * segment.start.y));
    frame.along = apply_linear(affine, in_world.x * half_thickness, in_world.y * half_thickness);
    frame.across = apply_linear(affine, -in_world.y * half_thickness, in_world.x * half_thickness);
    return frame;
}

pixel_ellipse stroke_cap_ellipse(const stroke_frame &frame, stroke_end around) noexcept {
    if (around == stroke_end::end) {
        return pixel_ellipse{
            frame.end, {frame.across.x, frame.across.y}, {frame.along.x, frame.along.y}};
    }
    return pixel_ellipse{
        frame.start, {-frame.across.x, -frame.across.y}, {-frame.along.x, -frame.along.y}};
}

std::size_t stroke_outline_point_count(glintfx::gltfx_line_cap cap,
                                       const stroke_cap_parameters &caps) noexcept {
    if (cap != glintfx::gltfx_line_cap::rounded) {
        return 4U;
    }
    return 4U + cap_point_count(caps.end) + cap_point_count(caps.start);
}

void write_stroke_outline(const stroke_frame &frame, glintfx::gltfx_line_cap cap,
                          const stroke_cap_parameters &caps,
                          std::span<pixel_position_wide> points) noexcept {
    if (points.size() != stroke_outline_point_count(cap, caps)) {
        return;
    }
    const double reach = cap == glintfx::gltfx_line_cap::square ? 1.0 : 0.0;
    const pixel_position_wide start{frame.start.x - (reach * frame.along.x),
                                    frame.start.y - (reach * frame.along.y)};
    const pixel_position_wide end{frame.end.x + (reach * frame.along.x),
                                  frame.end.y + (reach * frame.along.y)};
    const pixel_offset across = frame.across;
    points[0] = pixel_position_wide{start.x + across.x, start.y + across.y};
    points[1] = pixel_position_wide{end.x + across.x, end.y + across.y};
    // Flat and square have no cap points: both counts are 0 and the two writes below write nothing.
    const bool rounded = cap == glintfx::gltfx_line_cap::rounded;
    const std::size_t at_end = rounded ? cap_point_count(caps.end) : 0U;
    const std::size_t at_start = rounded ? cap_point_count(caps.start) : 0U;
    write_half_disc(stroke_cap_ellipse(frame, stroke_end::end), caps.end,
                    points.subspan(2U, at_end));
    points[2U + at_end] = pixel_position_wide{end.x - across.x, end.y - across.y};
    points[3U + at_end] = pixel_position_wide{start.x - across.x, start.y - across.y};
    write_half_disc(stroke_cap_ellipse(frame, stroke_end::start), caps.start,
                    points.subspan(4U + at_end, at_start));
}

std::size_t fan_index_count(std::size_t point_count) noexcept {
    return point_count < 3U ? 0U : 3U * (point_count - 2U);
}

void write_fan_indices(std::span<std::uint32_t> indices) noexcept {
    const std::size_t triangles = indices.size() / 3U;
    for (std::size_t t = 0U; t < triangles; ++t) {
        indices[3U * t] = 0U;
        indices[(3U * t) + 1U] = static_cast<std::uint32_t>(t + 1U);
        indices[(3U * t) + 2U] = static_cast<std::uint32_t>(t + 2U);
    }
}

} // namespace glintfx::draw2d
