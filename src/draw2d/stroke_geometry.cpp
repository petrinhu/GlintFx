// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/stroke_geometry.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "draw2d/arc_segments.hpp"

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

// Half a disc around `centre`: the points k = 1 .. segments - 1, from `across` to `-across`, on
// the side the offset `along` points to.
struct half_disc {
    pixel_position_wide centre{};
    pixel_offset across{};
    pixel_offset along{};
    std::uint32_t segments = 0U;
};

void write_half_disc(const half_disc &disc, std::span<pixel_position_wide> out) noexcept {
    for (std::uint32_t k = 1U; k < disc.segments; ++k) {
        const double angle =
            std::numbers::pi * static_cast<double>(k) / static_cast<double>(disc.segments);
        const double cosine = std::cos(angle);
        const double sine = std::sin(angle);
        out[k - 1U] =
            pixel_position_wide{disc.centre.x + (disc.across.x * cosine) + (disc.along.x * sine),
                                disc.centre.y + (disc.across.y * cosine) + (disc.along.y * sine)};
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

double stroke_cap_radius_pixels(const glintfx::gltfx_stroke &stroke,
                                const pixel_affine &affine) noexcept {
    const double half_thickness = 0.5 * stroke.thickness;
    if (stroke.unit == glintfx::gltfx_stroke_unit::pixel) {
        return half_thickness;
    }
    return screen_radius(half_thickness, affine);
}

std::size_t stroke_outline_point_count(glintfx::gltfx_line_cap cap,
                                       std::uint32_t cap_segments) noexcept {
    if (cap != glintfx::gltfx_line_cap::rounded || cap_segments < 2U) {
        return 4U;
    }
    return 4U + (2U * (static_cast<std::size_t>(cap_segments) - 1U));
}

void write_stroke_outline(const stroke_frame &frame, glintfx::gltfx_line_cap cap,
                          std::uint32_t cap_segments,
                          std::span<pixel_position_wide> points) noexcept {
    if (points.size() != stroke_outline_point_count(cap, cap_segments)) {
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
    if (cap != glintfx::gltfx_line_cap::rounded || cap_segments < 2U) {
        points[2] = pixel_position_wide{end.x - across.x, end.y - across.y};
        points[3] = pixel_position_wide{start.x - across.x, start.y - across.y};
        return;
    }
    const std::size_t half = static_cast<std::size_t>(cap_segments) - 1U;
    write_half_disc(half_disc{end, across, frame.along, cap_segments}, points.subspan(2U, half));
    points[2U + half] = pixel_position_wide{end.x - across.x, end.y - across.y};
    points[3U + half] = pixel_position_wide{start.x - across.x, start.y - across.y};
    const pixel_offset back{-across.x, -across.y};
    const pixel_offset back_along{-frame.along.x, -frame.along.y};
    write_half_disc(half_disc{start, back, back_along, cap_segments},
                    points.subspan(4U + half, half));
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
