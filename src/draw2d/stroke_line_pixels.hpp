// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include <glintfx/core/color.hpp>
#include <glintfx/core/segment.hpp>
#include <glintfx/draw2d/stroke.hpp>

#include "draw2d/arc_segments.hpp"
#include "draw2d/quad_vertices.hpp"
#include "draw2d/triangle_batch.hpp"

// draw2d/stroke_line_pixels.hpp - R2D-SHAPES, fatia S3: ONE accepted stroke_line() call turned into
// pixel triangles, ready for the piece list: the outline of stroke_geometry.hpp, each point checked
// against the pixel range the vertex program can scale (quad_vertices.hpp), narrowed to float, with
// the call's colour, and the fan that fills it. It knows nothing of GL, of the frame or of the
// report: the frame machine asks, and counts what it is told.
//
// NO MEMORY IS ASKED FOR: the storage is fixed, sized for the largest outline the arc count allows
// (two half discs of 256 segments: 514 points, 1536 indices), so building a line cannot fail for
// memory; only the piece list, which copies it, can.
//
// THE CAP TOLERANCE is the default largest error of a curve on the screen, 0.25 pixel (D-W8-07):
// the rounded cap is tessellated by arc_segments.hpp with it, from the cap radius on the screen.
namespace glintfx::draw2d {

inline constexpr double k_cap_max_error_pixels = 0.25;
inline constexpr std::size_t k_max_stroke_points = 4U + (2U * ((k_max_arc_segments / 2U) - 1U));
inline constexpr std::size_t k_max_stroke_indices = 3U * (k_max_stroke_points - 2U);

// What build() did with one call.
enum class stroke_pixels_outcome : std::uint8_t {
    built, // vertices() and indices() hold the call's triangles
    // A point of the outline lands outside the pixel range the vertex program can scale, or is not
    // a finite number (S2b-fix1, I-7): vertices() and indices() are empty.
    pixel_position_out_of_range,
};

class stroke_line_pixels {
  public:
    // Builds the triangles of `segment` drawn with `style` in `color`, by `affine`. The call's
    // values were already checked (stroke_line_check.hpp). Only this call is held afterwards.
    [[nodiscard]] stroke_pixels_outcome build(const glintfx::gltfx_segment_world &segment,
                                              const glintfx::gltfx_line_style &style,
                                              glintfx::gltfx_rgba color,
                                              const pixel_affine &affine) noexcept;

    // The pixel vertices of the last build(), in the order of the outline; empty when it did not
    // answer `built`.
    [[nodiscard]] std::span<const colored_pixel_vertex> vertices() const noexcept;

    // The fan indices of the last build(), into vertices(); empty when it did not answer `built`.
    [[nodiscard]] std::span<const std::uint32_t> indices() const noexcept;

  private:
    std::array<pixel_position_wide, k_max_stroke_points> outline{};
    std::array<colored_pixel_vertex, k_max_stroke_points> pixels{};
    std::array<std::uint32_t, k_max_stroke_indices> fan{};
    std::size_t point_count = 0;
    std::size_t index_count = 0;
};

} // namespace glintfx::draw2d
