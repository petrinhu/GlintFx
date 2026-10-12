// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/stroke_line_pixels.hpp"

#include <span>

#include "draw2d/quad_vertices.hpp"
#include "draw2d/stroke_geometry.hpp"

// draw2d/stroke_line_pixels.cpp - R2D-SHAPES, fatia S3: the build of stroke_line_pixels.hpp.
namespace glintfx::draw2d {

namespace {

// Every point of the outline must be in the pixel range; one that is not refuses the whole line
// (I-7, S2b-fix1). A NaN is in no range, so it is refused too.
[[nodiscard]] bool all_in_pixel_range(std::span<const pixel_position_wide> points) noexcept {
    for (const pixel_position_wide &point : points) {
        if (!pixel_position_in_range(point)) {
            return false;
        }
    }
    return true;
}

} // namespace

stroke_pixels_outcome stroke_line_pixels::build(const glintfx::gltfx_segment_world &segment,
                                                const glintfx::gltfx_line_style &style,
                                                glintfx::gltfx_rgba color,
                                                const pixel_affine &affine) noexcept {
    // Only this call is held: the counts start again whatever the answer.
    point_count = 0;
    index_count = 0;
    const std::uint32_t cap_segments = half_arc_segment_count(
        stroke_cap_radius_pixels(style.stroke, affine), k_cap_max_error_pixels);
    const std::size_t count = stroke_outline_point_count(style.cap, cap_segments);
    const stroke_frame frame = stroke_frame_of(segment, style.stroke, affine);
    const std::span<pixel_position_wide> points(outline.data(), count);
    write_stroke_outline(frame, style.cap, cap_segments, points);
    if (!all_in_pixel_range(points)) {
        return stroke_pixels_outcome::pixel_position_out_of_range;
    }
    for (std::size_t i = 0U; i < count; ++i) {
        pixels[i] = colored_pixel_vertex{narrowed_to_pixel(outline[i]), color};
    }
    point_count = count;
    index_count = fan_index_count(count);
    write_fan_indices(std::span<std::uint32_t>(fan.data(), index_count));
    return stroke_pixels_outcome::built;
}

std::span<const colored_pixel_vertex> stroke_line_pixels::vertices() const noexcept {
    return std::span<const colored_pixel_vertex>(pixels.data(), point_count);
}

std::span<const std::uint32_t> stroke_line_pixels::indices() const noexcept {
    return std::span<const std::uint32_t>(fan.data(), index_count);
}

} // namespace glintfx::draw2d
