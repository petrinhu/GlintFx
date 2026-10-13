// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include <glintfx/core/color.hpp>
#include <glintfx/core/rect.hpp>
#include <glintfx/draw2d/stroke.hpp>

#include "draw2d/quad_vertices.hpp"
#include "draw2d/stroke_rect_geometry.hpp"
#include "draw2d/triangle_batch.hpp"

// draw2d/stroke_rect_pixels.hpp - R2D-SHAPES, fatia S4: ONE accepted stroke_rect() call turned into
// pixel triangles, ready for the piece list: the outline of stroke_rect_geometry.hpp, each point
// checked against the pixel range the vertex program can scale (quad_vertices.hpp), narrowed to
// float, with the call's colour, and the triangles that fill it. It knows nothing of GL, of the
// frame or of the report: the frame machine asks, and counts what it is told.
//
// NO MEMORY IS ASKED FOR: the storage is fixed, sized for the outline with a hole (8 points and 24
// indices), so building an outline cannot fail for memory; only the piece list, which copies it,
// can.
namespace glintfx::draw2d {

// What build() did with one call.
enum class stroke_rect_pixels_outcome : std::uint8_t {
    built, // vertices() and indices() hold the call's triangles
    // A point of the outline lands outside the pixel range the vertex program can scale, or is not
    // a finite number (S2b-fix1, I-7): vertices() and indices() are empty.
    pixel_position_out_of_range,
};

class stroke_rect_pixels {
  public:
    // Builds the triangles of `rect` drawn with `style` in `color`, by `affine`. The call's values
    // were already checked (stroke_rect_check.hpp). Only this call is held afterwards.
    [[nodiscard]] stroke_rect_pixels_outcome build(const glintfx::gltfx_rect_world &rect,
                                                   const glintfx::gltfx_rect_outline_style &style,
                                                   glintfx::gltfx_rgba color,
                                                   const pixel_affine &affine) noexcept;

    // The pixel vertices of the last build(), in the order of stroke_rect_geometry.hpp; empty when
    // it did not answer `built`.
    [[nodiscard]] std::span<const colored_pixel_vertex> vertices() const noexcept;

    // The indices of the last build(), into vertices(); empty when it did not answer `built`.
    [[nodiscard]] std::span<const std::uint32_t> indices() const noexcept;

  private:
    std::array<pixel_position_wide, k_rect_outline_max_points> outline{};
    std::array<colored_pixel_vertex, k_rect_outline_max_points> pixels{};
    std::array<std::uint32_t, k_rect_outline_max_indices> outline_indices{};
    std::size_t point_count = 0;
    std::size_t index_count = 0;
};

} // namespace glintfx::draw2d
