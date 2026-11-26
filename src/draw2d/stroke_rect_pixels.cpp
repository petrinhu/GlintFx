// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/stroke_rect_pixels.hpp"

#include <cstddef>
#include <span>

#include "draw2d/quad_vertices.hpp"
#include "draw2d/stroke_rect_geometry.hpp"

// draw2d/stroke_rect_pixels.cpp - R2D-SHAPES, fatia S4: the build of stroke_rect_pixels.hpp.
namespace glintfx::draw2d {

stroke_rect_pixels_outcome stroke_rect_pixels::build(const glintfx::gltfx_rect_world &rect,
                                                     const glintfx::gltfx_rect_outline_style &style,
                                                     glintfx::gltfx_rgba color,
                                                     const pixel_affine &affine) noexcept {
    // Only this call is held: the counts start again whatever the answer.
    point_count = 0;
    index_count = 0;
    const rect_outline shape = rect_outline_of(rect, style, affine);
    const std::size_t count = write_rect_outline_points(shape, outline);
    const std::span<const pixel_position_wide> points(outline.data(), count);
    // Every point must be in the pixel range; one that is not refuses the whole outline (I-7,
    // S2b-fix1). A NaN is in no range, so it is refused too.
    if (!all_pixel_positions_in_range(points)) {
        return stroke_rect_pixels_outcome::pixel_position_out_of_range;
    }
    for (std::size_t i = 0U; i < count; ++i) {
        pixels[i] = colored_pixel_vertex{narrowed_to_pixel(outline[i]), color};
    }
    point_count = count;
    index_count = write_rect_outline_indices(shape.has_hole, outline_indices);
    return stroke_rect_pixels_outcome::built;
}

std::span<const colored_pixel_vertex> stroke_rect_pixels::vertices() const noexcept {
    return std::span<const colored_pixel_vertex>(pixels.data(), point_count);
}

std::span<const std::uint32_t> stroke_rect_pixels::indices() const noexcept {
    return std::span<const std::uint32_t>(outline_indices.data(), index_count);
}

} // namespace glintfx::draw2d
