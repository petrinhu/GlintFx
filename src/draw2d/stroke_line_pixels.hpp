// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include <glintfx/core/color.hpp>
#include <glintfx/core/segment.hpp>
#include <glintfx/draw2d/stroke.hpp>

#include "draw2d/arc_parameters.hpp"
#include "draw2d/quad_vertices.hpp"
#include "draw2d/stroke_geometry.hpp"
#include "draw2d/triangle_batch.hpp"

// draw2d/stroke_line_pixels.hpp - R2D-SHAPES, fatia S3: ONE accepted stroke_line() call turned into
// pixel triangles, ready for the piece list: the outline of stroke_geometry.hpp, each point checked
// against the pixel range the vertex program can scale (quad_vertices.hpp), narrowed to float, with
// the call's colour, and the fan that fills it. It knows nothing of GL, of the frame or of the
// report: the frame machine asks, and counts what it is told.
//
// THE ROUNDED CAP (fatia S5-C2, D-W8-830): each cap is the half ellipse of stroke_cap_ellipse()
// (stroke_geometry.hpp) on the screen, its points at the parameters arc_parameters.hpp gives it
// for the half extent, with the call's largest error (the screen's max_error_pixels, the
// curve_max_error_pixels given to open()), no reach (the cap's edge is the edge of the shape), and
// the surface of the frame. The two caps are cut each on its own (D-W8-831): one may lie on the
// surface and be cut finely while the other lies off it and is drawn as a few chords. So the
// guarantee of the cap is the one of arc_parameters.hpp, with its two conditions: on the surface,
// within the largest error while the level the error asks for fits the 1025 parameters of a cap,
// and while the cap's largest radius on the screen is at most 2^40 pixels.
//
// REFUSED as pixel_position_out_of_range, with every list empty, when a point lands outside the
// pixel range the vertex program can scale or is not a finite number (S2b-fix1, I-7), and when a
// rounded cap's half ellipse cannot be measured (arc_parameters.hpp answers 0: its largest radius
// on the screen passes the largest double), even if the four corners of the line are in range: a
// cap is never dropped in silence (D-W8-833).
//
// NO MEMORY IS ASKED FOR: the storage is fixed, sized for the largest outline the arc parameters
// allow (two caps of 1025 parameters: 2050 points, 6144 indices, and the two lists of parameters),
// so building a line cannot fail for memory; only the piece list, which copies it, can. The object
// is large (122992 bytes on x86-64 Linux, 26720 before fatia S5-C2; D-W8-835): the frame machine
// keeps it inside its interior, which create() puts on the heap, and the tests keep it on the heap.
namespace glintfx::draw2d {

inline constexpr std::size_t k_max_stroke_points = 4U + (2U * (k_max_arc_parameters - 2U));
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
    // Builds the triangles of `segment` drawn with `style` in `color`, on `screen` (its batch
    // transform, its surface and its largest error). The call's values were already checked
    // (stroke_line_check.hpp). Only this call is held afterwards.
    [[nodiscard]] stroke_pixels_outcome build(const glintfx::gltfx_segment_world &segment,
                                              const glintfx::gltfx_line_style &style,
                                              glintfx::gltfx_rgba color,
                                              const ellipse_screen &screen) noexcept;

    // The pixel vertices of the last build(), in the order of the outline; empty when it did not
    // answer `built`.
    [[nodiscard]] std::span<const colored_pixel_vertex> vertices() const noexcept;

    // The fan indices of the last build(), into vertices(); empty when it did not answer `built`.
    [[nodiscard]] std::span<const std::uint32_t> indices() const noexcept;

  private:
    // The parameters of both caps of `frame`, into end_parameters and start_parameters, from
    // arc_parameters.hpp on `screen`; empty spans for `flat` and `square`. False when a rounded
    // cap's arc cannot be measured (D-W8-833).
    [[nodiscard]] bool write_cap_parameters(const stroke_frame &frame, glintfx::gltfx_line_cap cap,
                                            const ellipse_screen &screen,
                                            stroke_cap_parameters &caps) noexcept;

    std::array<double, k_max_arc_parameters> end_parameters{};
    std::array<double, k_max_arc_parameters> start_parameters{};
    std::array<pixel_position_wide, k_max_stroke_points> outline{};
    std::array<colored_pixel_vertex, k_max_stroke_points> pixels{};
    std::array<std::uint32_t, k_max_stroke_indices> fan{};
    std::size_t point_count = 0;
    std::size_t index_count = 0;
};

} // namespace glintfx::draw2d
