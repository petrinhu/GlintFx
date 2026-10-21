// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include <glintfx/core/color.hpp>
#include <glintfx/core/ellipse.hpp>
#include <glintfx/draw2d/stroke.hpp>

#include "draw2d/arc_parameters.hpp"
#include "draw2d/ellipse_outline.hpp"
#include "draw2d/quad_vertices.hpp"
#include "draw2d/triangle_batch.hpp"

// draw2d/ellipse_pixels.hpp - R2D-SHAPES, fatia S5-B (D-W8-412 to D-W8-418): ONE accepted
// fill_ellipse() or stroke_ellipse() call turned into pixel triangles, ready for the piece list. It
// knows nothing of GL, of the frame or of the report: the frame machine asks, and counts what it is
// told.
//
// FILLED: the ellipse ON THE SCREEN (the world ellipse carried by the batch transform), its points
// at the arc's parameters (arc_parameters.hpp, with the call's largest error and no reach), as a
// fan from its centre (D-W8-272).
//
// OUTLINED: the band of ellipse_outline.hpp, built in the plane of the thickness unit (D-W8-273):
// in the world for world unit, then carried to the screen point by point; on the screen for pixel
// unit. The arc's parameters are those of the screen ellipse with HALF the largest error, and the
// reach of the band on the screen (half the thickness, by the largest stretch of the transform for
// world unit), so the surface cuts the band only where it does not show; the uniform normal angles
// take the other half (D-W8-416). A band with a hole is a ring, one without is a fan from the
// centre (ellipse_triangles.hpp).
//
// THE GUARANTEE (D-W8-418): on the surface, the edges drawn are at most the largest error from the
// true edges, under the two conditions of arc_parameters.hpp (the level the error asks for fits the
// room, and the largest radius on the screen is at most 2^40 pixels).
//
// REFUSED, with every list empty, when a point lands outside the pixel range the vertex program can
// scale or is not a finite number (S2b-fix1, I-7), and when the screen ellipse itself is too large
// to be measured (arc_parameters.hpp answers 0 only for a stretch past the largest double).
//
// NO MEMORY IS ASKED FOR: the storage is fixed, sized for the most angles an outline can have
// (k_max_outline_angles, twice for a ring); the object is large (about 240 KB), so the frame
// machine keeps it on the heap.
namespace glintfx::draw2d {

inline constexpr std::size_t k_max_ellipse_vertices = 2U * k_max_outline_angles;
inline constexpr std::size_t k_max_ellipse_indices = 6U * k_max_outline_angles;

enum class ellipse_pixels_outcome : std::uint8_t {
    built,
    pixel_position_out_of_range,
};

// The screen of one call: the batch transform, the surface in pixels, and the largest error
// (gltfx_renderer_2d_desc::curve_max_error_pixels, read since fatia S5-C1; open() refuses it unless
// it is finite and above zero).
struct ellipse_screen {
    pixel_affine affine{};
    pixel_box surface{};
    double max_error_pixels = 0.25;
};

class ellipse_pixels {
  public:
    // The values of the call were already checked (ellipse_check.hpp). Only this call is held
    // afterwards.
    [[nodiscard]] ellipse_pixels_outcome build_fill(const glintfx::gltfx_ellipse_world &ellipse,
                                                    glintfx::gltfx_rgba color,
                                                    const ellipse_screen &screen) noexcept;

    [[nodiscard]] ellipse_pixels_outcome build_stroke(const glintfx::gltfx_ellipse_world &ellipse,
                                                      const glintfx::gltfx_stroke &stroke,
                                                      glintfx::gltfx_rgba color,
                                                      const ellipse_screen &screen) noexcept;

    // The pixel vertices of the last build; empty when it did not answer `built`.
    [[nodiscard]] std::span<const colored_pixel_vertex> vertices() const noexcept;

    // The indices of the last build, into vertices(); empty when it did not answer `built`.
    [[nodiscard]] std::span<const std::uint32_t> indices() const noexcept;

  private:
    // Publishes the first `count` positions as the triangles of the build, IF every one is in the
    // pixel range (one out, or a NaN, refuses the whole ellipse: pixel_position_out_of_range); it
    // only sequences the check and the two atoms below (S5-B-fix2, D-W8-534).
    [[nodiscard]] ellipse_pixels_outcome
    publish_band_if_in_range(std::size_t count, glintfx::gltfx_rgba color, bool ring) noexcept;
    // The indices of a ring (outer then inner points) or of a fan (centre first) over `points`;
    // how many were written, 0 when they did not fit.
    [[nodiscard]] std::size_t write_band_indices(std::span<const pixel_position_wide> points,
                                                 bool ring) noexcept;
    // The first `count` positions narrowed to the vertex format, with `color`.
    void write_band_vertices(std::size_t count, glintfx::gltfx_rgba color) noexcept;

    std::array<double, k_max_arc_parameters> parameters{};
    std::array<double, k_max_outline_angles> angles{};
    std::array<pixel_position_wide, k_max_ellipse_vertices> positions{};
    std::array<colored_pixel_vertex, k_max_ellipse_vertices> pixels{};
    std::array<std::uint32_t, k_max_ellipse_indices> ellipse_indices{};
    std::size_t point_count = 0;
    std::size_t index_count = 0;
};

} // namespace glintfx::draw2d
