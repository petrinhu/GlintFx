// SPDX-License-Identifier: AGPL-3.0-or-later
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

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
// Fatia S5-C2 (D-W8-830 to D-W8-835): the cap reads the largest error of the screen, keeps it past
// the 13280 pixels of before on the surface, and is refused when its arc cannot be measured. The
// counts come from n = ceil(pi / acos(1 - e / r)) for the whole circle, half of it rounded up for
// a cap, 4 + 2 (half - 1) points (computed outside the code, python, before it was run).

using glintfx::gltfx_line_cap;
using glintfx::gltfx_line_style;
using glintfx::gltfx_rgba;
using glintfx::gltfx_segment_world;
using glintfx::gltfx_stroke_unit;
using glintfx::gltfx_transform;
using glintfx::draw2d::colored_pixel_vertex;
using glintfx::draw2d::ellipse_screen;
using glintfx::draw2d::k_max_stroke_indices;
using glintfx::draw2d::k_max_stroke_points;
using glintfx::draw2d::pixel_affine;
using glintfx::draw2d::pixel_affine_from_transform;
using glintfx::draw2d::pixel_box;
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

// The 800 x 600 surface of the frame machine's tests, and the default largest error.
[[nodiscard]] ellipse_screen screen_of(const pixel_affine &affine, double max_error = 0.25) {
    return ellipse_screen{affine, pixel_box{0.0, 0.0, 800.0, 600.0}, max_error};
}

// The storage is large (it never asks for memory); one per test, on the heap.
[[nodiscard]] std::unique_ptr<stroke_line_pixels> fresh() {
    return std::make_unique<stroke_line_pixels>();
}

// The checks of the cases below stay at the top level of each case, within three levels (L-17):
// the comparisons that loop live here, each answering one bool.

// Every vertex at the (x, y) pairs of `want`, in order; false on any other count.
template <std::size_t N>
[[nodiscard]] bool positions_are(std::span<const colored_pixel_vertex> vertices,
                                 const std::array<float, N> &want) {
    if (vertices.size() * 2U != N) {
        return false;
    }
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        if (vertices[i].position.x != want[2 * i] || vertices[i].position.y != want[(2 * i) + 1]) {
            return false;
        }
    }
    return true;
}

// Every vertex with the red and the alpha of `color`, straight as given (the batch premultiplies).
[[nodiscard]] bool red_and_alpha_are(std::span<const colored_pixel_vertex> vertices,
                                     gltfx_rgba color) {
    return std::ranges::all_of(vertices, [color](const colored_pixel_vertex &vertex) {
        return vertex.color.red == color.red && vertex.color.alpha == color.alpha;
    });
}

// How many vertices lie on the row `y`.
[[nodiscard]] std::size_t count_on_row(std::span<const colored_pixel_vertex> vertices, float y) {
    return static_cast<std::size_t>(std::ranges::count_if(
        vertices, [y](const colored_pixel_vertex &vertex) { return vertex.position.y == y; }));
}

// A true circle on the screen, in pixels.
struct circle {
    double x = 0.0;
    double y = 0.0;
    double r = 0.0;
};

[[nodiscard]] bool on(const circle &c, const colored_pixel_vertex &v) {
    return std::abs(std::hypot(v.position.x - c.x, v.position.y - c.y) - c.r) < 0.05;
}

// How many vertices lie on `c` (within 0.05 pixel: the float positions are far finer than that).
[[nodiscard]] std::size_t points_on_circle(std::span<const colored_pixel_vertex> vertices,
                                           const circle &c) {
    return static_cast<std::size_t>(std::ranges::count_if(
        vertices, [&c](const colored_pixel_vertex &vertex) { return on(c, vertex); }));
}

// The chords between consecutive vertices on `c` whose box touches the 800 x 600 surface: how many,
// and the largest distance between such a chord and the circle, r minus the distance from the
// centre to the chord's line.
struct chords_seen {
    std::size_t touching = 0;
    double farthest = 0.0;
};

[[nodiscard]] chords_seen chords_on_surface(std::span<const colored_pixel_vertex> vertices,
                                            const circle &c) {
    chords_seen seen{};
    for (std::size_t i = 0; i + 1 < vertices.size(); ++i) {
        const colored_pixel_vertex &a = vertices[i];
        const colored_pixel_vertex &b = vertices[i + 1];
        const double ax = a.position.x;
        const double ay = a.position.y;
        const double bx = b.position.x;
        const double by = b.position.y;
        const bool touches = std::max(ay, by) >= 0.0 && std::min(ay, by) <= 600.0 &&
                             std::max(ax, bx) >= 0.0 && std::min(ax, bx) <= 800.0;
        if (on(c, a) && on(c, b) && touches) {
            const double to_chord = std::abs(((bx - ax) * (c.y - ay)) - ((by - ay) * (c.x - ax))) /
                                    std::hypot(bx - ax, by - ay);
            seen.farthest = std::max(seen.farthest, c.r - to_chord);
            ++seen.touching;
        }
    }
    return seen;
}

} // namespace

GLINTFX_TEST(stroke_line_pixels_flat_line_is_four_vertices_with_its_colour_and_two_triangles) {
    const auto pixels = fresh();
    GLINTFX_CHECK(
        pixels->build(k_line, style_of(4.0, gltfx_stroke_unit::world, gltfx_line_cap::flat),
                      k_half_red, screen_of(pixel_affine{})) == stroke_pixels_outcome::built);
    const auto vertices = pixels->vertices();
    GLINTFX_CHECK_EQ(vertices.size(), std::size_t{4});
    GLINTFX_CHECK(positions_are(
        vertices, std::array<float, 8>{10.0F, 12.0F, 50.0F, 12.0F, 50.0F, 8.0F, 10.0F, 8.0F}));
    GLINTFX_CHECK(red_and_alpha_are(vertices, k_half_red)); // straight: the batch premultiplies
    const auto indices = pixels->indices();
    GLINTFX_CHECK(
        std::ranges::equal(indices, std::array<std::uint32_t, 6>{0U, 1U, 2U, 0U, 2U, 3U}));
}

// The parity cell's line on the screen: pixel unit under zoom 3, still 4 pixels thick.
GLINTFX_TEST(stroke_line_pixels_pixel_unit_under_zoom_three_stays_four_pixels_thick) {
    const auto pixels = fresh();
    const gltfx_segment_world line{{10.0, 20.0}, {40.0, 20.0}};
    GLINTFX_CHECK(pixels->build(line, style_of(4.0, gltfx_stroke_unit::pixel, gltfx_line_cap::flat),
                                k_half_red,
                                screen_of(scaled(3.0))) == stroke_pixels_outcome::built);
    const auto vertices = pixels->vertices();
    GLINTFX_CHECK_EQ(vertices.size(), std::size_t{4});
    GLINTFX_CHECK(positions_are(
        vertices, std::array<float, 8>{30.0F, 62.0F, 120.0F, 62.0F, 120.0F, 58.0F, 30.0F, 58.0F}));
}

// The rounded cap is counted from its radius ON THE SCREEN: 2 pixels gives 4 segments per half
// disc (10 points, 8 triangles); the same line in world unit under zoom 1000 has a radius of 2000
// pixels, 199 segments for the circle, 100 per half (202 points, 200 triangles).
GLINTFX_TEST(stroke_line_pixels_rounded_cap_is_counted_from_the_radius_on_the_screen) {
    const auto pixels = fresh();
    const gltfx_line_style rounded =
        style_of(4.0, gltfx_stroke_unit::world, gltfx_line_cap::rounded);
    GLINTFX_CHECK(pixels->build(k_line, rounded, k_half_red, screen_of(pixel_affine{})) ==
                  stroke_pixels_outcome::built);
    GLINTFX_CHECK_EQ(pixels->vertices().size(), std::size_t{10});
    GLINTFX_CHECK_EQ(pixels->indices().size(), std::size_t{24});
    GLINTFX_CHECK(pixels->build(k_line, rounded, k_half_red, screen_of(scaled(1000.0))) ==
                  stroke_pixels_outcome::built);
    GLINTFX_CHECK_EQ(pixels->vertices().size(), std::size_t{202});
    GLINTFX_CHECK_EQ(pixels->indices().size(), std::size_t{600});
}

// The cap reads the largest error of its screen (Q4, D-W8-572, D-W8-830): the radius of 2000
// pixels gives 199 segments for the circle at 0.25 (202 points) and 36 at 8 pixels, 18 per half:
// 4 + 2 x 17 = 38 points, 36 triangles.
GLINTFX_TEST(stroke_line_pixels_the_rounded_cap_reads_the_largest_error_of_the_screen) {
    const auto pixels = fresh();
    const gltfx_line_style rounded =
        style_of(4.0, gltfx_stroke_unit::world, gltfx_line_cap::rounded);
    GLINTFX_CHECK(pixels->build(k_line, rounded, k_half_red, screen_of(scaled(1000.0), 8.0)) ==
                  stroke_pixels_outcome::built);
    GLINTFX_CHECK_EQ(pixels->vertices().size(), std::size_t{38});
    GLINTFX_CHECK_EQ(pixels->indices().size(), std::size_t{108});
}

// Past the 13280 pixels of before (fatia S3-fix1), on the surface: a cap of radius 50000 pixels
// whose far edge crosses the 800 x 600 surface. Before, its 256 equal steps per half left a chord
// 50000 (1 - cos(pi / 512)) = 0.94 pixel from the true circle; now every chord of the end cap that
// touches the surface is within the 0.25 pixel (the distance from the centre to the chord's line is
// at least r - 0.25), and at least two touch it (the floor, L-40: none would prove nothing). The
// start cap, off the surface, is cut on its own (D-W8-831): its two quarters miss the surface, so
// each is its chord, and three points lie on its circle (its two corners and t = pi / 2).
GLINTFX_TEST(stroke_line_pixels_a_cap_past_13280_pixels_stays_within_the_error_on_the_surface) {
    const auto pixels = fresh();
    const gltfx_segment_world down{{400.0, -100000.0}, {400.0, -49700.0}};
    GLINTFX_CHECK(
        pixels->build(down, style_of(100000.0, gltfx_stroke_unit::pixel, gltfx_line_cap::rounded),
                      k_half_red, screen_of(pixel_affine{})) == stroke_pixels_outcome::built);
    const chords_seen end_cap = chords_on_surface(pixels->vertices(), {400.0, -49700.0, 50000.0});
    GLINTFX_CHECK(end_cap.touching >= std::size_t{2});
    GLINTFX_CHECK(end_cap.farthest <= 0.25);
    GLINTFX_CHECK_EQ(points_on_circle(pixels->vertices(), {400.0, -100000.0, 50000.0}),
                     std::size_t{3});
}

// The largest outline: two caps of 1025 parameters (arc_parameters.hpp, level 9, nothing cut off a
// surface that holds the whole line), 4 + 2 x 1023 = 2050 points and 3 x 2048 = 6144 indices.
GLINTFX_TEST(stroke_line_pixels_the_largest_rounded_line_fits) {
    const auto pixels = fresh();
    const ellipse_screen whole_line{pixel_affine{}, pixel_box{-1e7, -1e7, 1e7, 1e7}, 0.25};
    GLINTFX_CHECK(pixels->build(k_line,
                                style_of(1e6, gltfx_stroke_unit::pixel, gltfx_line_cap::rounded),
                                k_half_red, whole_line) == stroke_pixels_outcome::built);
    GLINTFX_CHECK_EQ(pixels->vertices().size(), k_max_stroke_points);
    GLINTFX_CHECK_EQ(pixels->indices().size(), k_max_stroke_indices);
    GLINTFX_CHECK_EQ(k_max_stroke_points, std::size_t{2050});
    GLINTFX_CHECK_EQ(k_max_stroke_indices, std::size_t{6144});
}

// A rounded cap whose half ellipse cannot be measured is refused (D-W8-833), even though the four
// corners of the line are in range: a line of length zero at the origin, world thickness 4, under
// the scale (1.2e308, 1) turned by 45 degrees, has along = (1.697e308, 1.697e308), finite, whose
// length 2.4e308 is not, and across = (-1.41, 1.41). Before fatia S5-C2 the cap points, out of the
// pixel range, refused it. The same line with the flat cap has no cap and is built (zero area).
GLINTFX_TEST(stroke_line_pixels_a_cap_that_cannot_be_measured_is_refused) {
    const auto pixels = fresh();
    const gltfx_segment_world origin{{0.0, 0.0}, {0.0, 0.0}};
    const pixel_affine stretched = pixel_affine_from_transform(gltfx_transform{
        .translation = {0.0, 0.0}, .rotation = {0.78539816339744828}, .scale = {1.2e308, 1.0}});
    GLINTFX_CHECK(pixels->build(origin,
                                style_of(4.0, gltfx_stroke_unit::world, gltfx_line_cap::rounded),
                                k_half_red, screen_of(stretched)) ==
                  stroke_pixels_outcome::pixel_position_out_of_range);
    GLINTFX_CHECK(pixels->vertices().empty());
    GLINTFX_CHECK(pixels->build(origin,
                                style_of(4.0, gltfx_stroke_unit::world, gltfx_line_cap::flat),
                                k_half_red, screen_of(stretched)) == stroke_pixels_outcome::built);
    GLINTFX_CHECK_EQ(pixels->vertices().size(), std::size_t{4});
}

// A point out of the range the vertex program can scale (quad_vertices.hpp) refuses the call, and
// nothing is held; the next call is built from nothing left behind.
GLINTFX_TEST(stroke_line_pixels_a_point_out_of_the_pixel_range_holds_nothing) {
    const auto pixels = fresh();
    const gltfx_segment_world far{{1e38, 0.0}, {1.5e38, 0.0}};
    GLINTFX_CHECK(
        pixels->build(k_line, style_of(4.0, gltfx_stroke_unit::world, gltfx_line_cap::rounded),
                      k_half_red, screen_of(pixel_affine{})) == stroke_pixels_outcome::built);
    GLINTFX_CHECK(pixels->build(far, style_of(4.0, gltfx_stroke_unit::world, gltfx_line_cap::flat),
                                k_half_red, screen_of(scaled(2.0))) ==
                  stroke_pixels_outcome::pixel_position_out_of_range);
    GLINTFX_CHECK(pixels->vertices().empty());
    GLINTFX_CHECK(pixels->indices().empty());
    GLINTFX_CHECK(
        pixels->build(k_line, style_of(4.0, gltfx_stroke_unit::world, gltfx_line_cap::flat),
                      k_half_red, screen_of(pixel_affine{})) == stroke_pixels_outcome::built);
    GLINTFX_CHECK_EQ(pixels->vertices().size(), std::size_t{4});
    GLINTFX_CHECK_EQ(pixels->indices().size(), std::size_t{6});
}

// A line of zero thickness is built (a shape of zero area, drawn, covering no pixel): its four
// points lie on the line.
GLINTFX_TEST(stroke_line_pixels_zero_thickness_is_built_with_zero_area) {
    const auto pixels = fresh();
    GLINTFX_CHECK(
        pixels->build(k_line, style_of(0.0, gltfx_stroke_unit::world, gltfx_line_cap::flat),
                      k_half_red, screen_of(pixel_affine{})) == stroke_pixels_outcome::built);
    const auto vertices = pixels->vertices();
    GLINTFX_CHECK_EQ(vertices.size(), std::size_t{4});
    GLINTFX_CHECK_EQ(count_on_row(vertices, 10.0F), std::size_t{4});
}
