// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/stroke_line_pixels.hpp"

#include <span>

#include "draw2d/arc_parameters.hpp"
#include "draw2d/quad_vertices.hpp"
#include "draw2d/stroke_geometry.hpp"

// draw2d/stroke_line_pixels.cpp - R2D-SHAPES, fatia S3: the build of stroke_line_pixels.hpp.
namespace glintfx::draw2d {

bool stroke_line_pixels::write_cap_parameters(const stroke_frame &frame,
                                              glintfx::gltfx_line_cap cap,
                                              const ellipse_screen &screen,
                                              stroke_cap_parameters &caps) noexcept {
    caps = stroke_cap_parameters{};
    if (cap != glintfx::gltfx_line_cap::rounded) {
        return true;
    }
    // No reach: the edge of the cap is the edge of the shape (arc_parameters.hpp).
    const arc_request end_request{stroke_cap_ellipse(frame, stroke_end::end), arc_extent::half,
                                  screen.max_error_pixels, 0.0};
    const arc_request start_request{stroke_cap_ellipse(frame, stroke_end::start), arc_extent::half,
                                    screen.max_error_pixels, 0.0};
    const std::size_t at_end = arc_parameters(end_request, screen.surface, end_parameters);
    const std::size_t at_start = arc_parameters(start_request, screen.surface, start_parameters);
    caps = stroke_cap_parameters{std::span<const double>(end_parameters.data(), at_end),
                                 std::span<const double>(start_parameters.data(), at_start)};
    return at_end != 0U && at_start != 0U;
}

stroke_pixels_outcome stroke_line_pixels::build(const glintfx::gltfx_segment_world &segment,
                                                const glintfx::gltfx_line_style &style,
                                                glintfx::gltfx_rgba color,
                                                const ellipse_screen &screen) noexcept {
    // Only this call is held: the counts start again whatever the answer.
    point_count = 0;
    index_count = 0;
    const stroke_frame frame = stroke_frame_of(segment, style.stroke, screen.affine);
    stroke_cap_parameters caps{};
    if (!write_cap_parameters(frame, style.cap, screen, caps)) {
        return stroke_pixels_outcome::pixel_position_out_of_range; // a cap not measured (D-W8-833)
    }
    const std::size_t count = stroke_outline_point_count(style.cap, caps);
    const std::span<pixel_position_wide> points(outline.data(), count);
    write_stroke_outline(frame, style.cap, caps, points);
    // Every point must be in the pixel range; one that is not refuses the whole line (I-7,
    // S2b-fix1). A NaN is in no range, so it is refused too.
    if (!all_pixel_positions_in_range(points)) {
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
