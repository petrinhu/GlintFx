// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

#include <glintfx/core/color.hpp>
#include <glintfx/core/segment.hpp>
#include <glintfx/core/transform.hpp>
#include <glintfx/draw2d/stroke.hpp>

#include "draw2d/quad_vertices.hpp"
#include "draw2d/stroke_line_pixels.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// stroke_line_pixels_test.cpp - R2D-SHAPES, fatia S3: one accepted stroke_line() call as pixel
// triangles (draw2d/stroke_line_pixels.hpp), with no window and no GL: the float positions, the
// colour of every vertex, the fan, the count of the rounded cap from the radius on the SCREEN, the
// largest outline the storage holds, and the refusal of a point the vertex program cannot scale.
// The expected points are computed by hand before the code (axis-aligned lines, exact in float).

using glintfx::gltfx_line_cap;
using glintfx::gltfx_line_style;
using glintfx::gltfx_rgba;
using glintfx::gltfx_segment_world;
using glintfx::gltfx_stroke_unit;
using glintfx::gltfx_transform;
using glintfx::draw2d::k_max_stroke_indices;
using glintfx::draw2d::k_max_stroke_points;
using glintfx::draw2d::pixel_affine;
using glintfx::draw2d::pixel_affine_from_transform;
using glintfx::draw2d::stroke_line_pixels;
using glintfx::draw2d::stroke_pixels_outcome;

namespace {

constexpr gltfx_segment_world k_line{{10.0, 10.0}, {50.0, 10.0}};
constexpr gltfx_rgba k_half_red{1.0F, 0.0F, 0.0F, 0.5F};

[[nodiscard]] gltfx_line_style style_of(double thickness, gltfx_stroke_unit unit,
                                        gltfx_line_cap cap) {
    gltfx_line_style style;
    style.stroke.thickness = thickness;
    style.stroke.unit = unit;
    style.cap = cap;
    return style;
}

[[nodiscard]] pixel_affine scaled(double factor) {
    return pixel_affine_from_transform(
        gltfx_transform{.translation = {0.0, 0.0}, .rotation = {0.0}, .scale = {factor, factor}});
}

// The storage is large (it never asks for memory); one per test, on the heap.
[[nodiscard]] std::unique_ptr<stroke_line_pixels> fresh() {
    return std::make_unique<stroke_line_pixels>();
}

} // namespace

GLINTFX_TEST(stroke_line_pixels_flat_line_is_four_vertices_with_its_colour_and_two_triangles) {
    const auto pixels = fresh();
    GLINTFX_CHECK(pixels->build(k_line,
                                style_of(4.0, gltfx_stroke_unit::world, gltfx_line_cap::flat),
                                k_half_red, pixel_affine{}) == stroke_pixels_outcome::built);
    const auto vertices = pixels->vertices();
    GLINTFX_CHECK_EQ(vertices.size(), std::size_t{4});
    const std::array<float, 8> want{10.0F, 12.0F, 50.0F, 12.0F, 50.0F, 8.0F, 10.0F, 8.0F};
    for (std::size_t i = 0; i < 4 && i < vertices.size(); ++i) {
        GLINTFX_CHECK_EQ(vertices[i].position.x, want[2 * i]);
        GLINTFX_CHECK_EQ(vertices[i].position.y, want[(2 * i) + 1]);
        GLINTFX_CHECK_EQ(vertices[i].color.red, 1.0F);
        GLINTFX_CHECK_EQ(vertices[i].color.alpha,
                         0.5F); // straight, as given: the batch premultiplies
    }
    const auto indices = pixels->indices();
    GLINTFX_CHECK_EQ(indices.size(), std::size_t{6});
    if (indices.size() == 6) {
        GLINTFX_CHECK(indices[0] == 0U && indices[1] == 1U && indices[2] == 2U);
        GLINTFX_CHECK(indices[3] == 0U && indices[4] == 2U && indices[5] == 3U);
    }
}

// The parity cell's line on the screen: pixel unit under zoom 3, still 4 pixels thick.
GLINTFX_TEST(stroke_line_pixels_pixel_unit_under_zoom_three_stays_four_pixels_thick) {
    const auto pixels = fresh();
    const gltfx_segment_world line{{10.0, 20.0}, {40.0, 20.0}};
    GLINTFX_CHECK(pixels->build(line, style_of(4.0, gltfx_stroke_unit::pixel, gltfx_line_cap::flat),
                                k_half_red, scaled(3.0)) == stroke_pixels_outcome::built);
    const auto vertices = pixels->vertices();
    GLINTFX_CHECK_EQ(vertices.size(), std::size_t{4});
    if (vertices.size() == 4) {
        GLINTFX_CHECK(vertices[0].position.x == 30.0F && vertices[0].position.y == 62.0F);
        GLINTFX_CHECK(vertices[1].position.x == 120.0F && vertices[1].position.y == 62.0F);
        GLINTFX_CHECK(vertices[2].position.x == 120.0F && vertices[2].position.y == 58.0F);
        GLINTFX_CHECK(vertices[3].position.x == 30.0F && vertices[3].position.y == 58.0F);
    }
}

// The rounded cap is counted from its radius ON THE SCREEN: 2 pixels gives 4 segments per half
// disc (10 points, 8 triangles); the same line in world unit under zoom 1000 has a radius of 2000
// pixels, 199 segments for the circle, 100 per half (202 points, 200 triangles).
GLINTFX_TEST(stroke_line_pixels_rounded_cap_is_counted_from_the_radius_on_the_screen) {
    const auto pixels = fresh();
    const gltfx_line_style rounded =
        style_of(4.0, gltfx_stroke_unit::world, gltfx_line_cap::rounded);
    GLINTFX_CHECK(pixels->build(k_line, rounded, k_half_red, pixel_affine{}) ==
                  stroke_pixels_outcome::built);
    GLINTFX_CHECK_EQ(pixels->vertices().size(), std::size_t{10});
    GLINTFX_CHECK_EQ(pixels->indices().size(), std::size_t{24});
    GLINTFX_CHECK(pixels->build(k_line, rounded, k_half_red, scaled(1000.0)) ==
                  stroke_pixels_outcome::built);
    GLINTFX_CHECK_EQ(pixels->vertices().size(), std::size_t{202});
    GLINTFX_CHECK_EQ(pixels->indices().size(), std::size_t{600});
}

// The largest outline the arc count allows (two half discs of 256 segments) fits the storage.
GLINTFX_TEST(stroke_line_pixels_the_largest_rounded_line_fits) {
    const auto pixels = fresh();
    GLINTFX_CHECK(pixels->build(k_line,
                                style_of(1e6, gltfx_stroke_unit::pixel, gltfx_line_cap::rounded),
                                k_half_red, pixel_affine{}) == stroke_pixels_outcome::built);
    GLINTFX_CHECK_EQ(pixels->vertices().size(), k_max_stroke_points);
    GLINTFX_CHECK_EQ(pixels->indices().size(), k_max_stroke_indices);
    GLINTFX_CHECK_EQ(k_max_stroke_points, std::size_t{514});
    GLINTFX_CHECK_EQ(k_max_stroke_indices, std::size_t{1536});
}

// A point out of the range the vertex program can scale (quad_vertices.hpp) refuses the call, and
// nothing is held; the next call is built from nothing left behind.
GLINTFX_TEST(stroke_line_pixels_a_point_out_of_the_pixel_range_holds_nothing) {
    const auto pixels = fresh();
    const gltfx_segment_world far{{1e38, 0.0}, {1.5e38, 0.0}};
    GLINTFX_CHECK(pixels->build(k_line,
                                style_of(4.0, gltfx_stroke_unit::world, gltfx_line_cap::rounded),
                                k_half_red, pixel_affine{}) == stroke_pixels_outcome::built);
    GLINTFX_CHECK(pixels->build(far, style_of(4.0, gltfx_stroke_unit::world, gltfx_line_cap::flat),
                                k_half_red,
                                scaled(2.0)) == stroke_pixels_outcome::pixel_position_out_of_range);
    GLINTFX_CHECK(pixels->vertices().empty());
    GLINTFX_CHECK(pixels->indices().empty());
    GLINTFX_CHECK(pixels->build(k_line,
                                style_of(4.0, gltfx_stroke_unit::world, gltfx_line_cap::flat),
                                k_half_red, pixel_affine{}) == stroke_pixels_outcome::built);
    GLINTFX_CHECK_EQ(pixels->vertices().size(), std::size_t{4});
    GLINTFX_CHECK_EQ(pixels->indices().size(), std::size_t{6});
}

// A line of zero thickness is built (a shape of zero area, drawn, covering no pixel): its four
// points lie on the line.
GLINTFX_TEST(stroke_line_pixels_zero_thickness_is_built_with_zero_area) {
    const auto pixels = fresh();
    GLINTFX_CHECK(pixels->build(k_line,
                                style_of(0.0, gltfx_stroke_unit::world, gltfx_line_cap::flat),
                                k_half_red, pixel_affine{}) == stroke_pixels_outcome::built);
    const auto vertices = pixels->vertices();
    GLINTFX_CHECK_EQ(vertices.size(), std::size_t{4});
    for (const auto &vertex : vertices) {
        GLINTFX_CHECK_EQ(vertex.position.y, 10.0F);
    }
}
