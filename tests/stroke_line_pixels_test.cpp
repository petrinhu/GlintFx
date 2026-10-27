// SPDX-License-Identifier: AGPL-3.0-or-later
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <numbers>
#include <span>
#include <utility>

#include <glintfx/core/color.hpp>
#include <glintfx/core/segment.hpp>
#include <glintfx/core/transform.hpp>
#include <glintfx/draw2d/stroke.hpp>

#include "draw2d/quad_vertices.hpp"
#include "draw2d/stroke_line_pixels.hpp"

#include "harness/check.hpp"
#include "harness/curve_oracle.hpp"
#include "harness/sweep_lcg.hpp"
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
// Fatia S5-C2-fix1 (the S5-C2 review: I1, C2, C3, C4): the cap is swept on the surface across
// radii, units, transforms and errors; the cap just off the surface is cut with no reach; and the
// uniform count of a cap is swept against the formula of before.

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
using glintfx::draw2d::pixel_ellipse;
using glintfx::draw2d::pixel_position_wide;
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
GLINTFX_TEST(stroke_line_pixels_a_cap_of_radius_50000_stays_within_the_error_on_the_surface) {
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

// THE REACH OF A CAP IS ZERO (fatia S5-C2-fix1; the S5-C2 review, C2 and its mutant RC). The edge
// of a cap is the edge of the shape, so its cut grows the surface by the margin of
// arc_parameters.hpp only, never by half the thickness. A cap of radius 20000 pixels whose far
// point stops 30 pixels above the 800 x 600 surface misses it: its two quarters are chords, one
// point between its corners, 6 points for the line (the other cap lies 100000 pixels away). Grown
// by the half thickness it would touch the surface and be cut into 512 pieces: 516 points (python
// model modelo_arco2.py, outside the code, before it was run). Both caps: the line, and the same
// line drawn the other way.
GLINTFX_TEST(stroke_line_pixels_a_cap_just_off_the_surface_is_cut_with_no_reach) {
    const auto pixels = fresh();
    const gltfx_line_style wide =
        style_of(40000.0, gltfx_stroke_unit::pixel, gltfx_line_cap::rounded);
    const gltfx_segment_world down{{400.0, -100000.0}, {400.0, -20030.0}};
    const gltfx_segment_world up{{400.0, -20030.0}, {400.0, -100000.0}};
    GLINTFX_CHECK(pixels->build(down, wide, k_half_red, screen_of(pixel_affine{})) ==
                  stroke_pixels_outcome::built);
    GLINTFX_CHECK_EQ(pixels->vertices().size(), std::size_t{6});
    GLINTFX_CHECK(pixels->build(up, wide, k_half_red, screen_of(pixel_affine{})) ==
                  stroke_pixels_outcome::built);
    GLINTFX_CHECK_EQ(pixels->vertices().size(), std::size_t{6});
}

namespace {

constexpr double k_pi = std::numbers::pi;

// The fixed integer generator of the sweeps below (the same numbers on every system), shared with
// arc_parameters_test.cpp and ellipse_pixels_test.cpp, with seeds of its own.
using glintfx::test_support::lcg;

// A batch transform turned by a drawn angle and scaled unevenly, the larger scale from 1/8 to 8 and
// the other up to 64 times smaller (down to 1/512; the review of fatia S5-C2-fix1, C-1), with no
// translation yet: scale first, then rotation, as
// pixel_affine_from_transform builds it (quad_vertices.cpp). `larger` is its largest stretch, the
// larger of the two scales.
struct drawn_linear {
    pixel_affine affine{};
    double larger = 1.0;
};

drawn_linear drawn_linear_part(lcg &g) {
    const double turn = 2.0 * k_pi * g.next();
    double sx = std::exp2((6.0 * g.next()) - 3.0);
    double sy = sx * std::exp2(-6.0 * g.next());
    if (g.next() < 0.5) {
        std::swap(sx, sy);
    }
    const pixel_affine linear{sx * std::cos(turn),
                              sx * std::sin(turn),
                              -sy * std::sin(turn),
                              sy * std::cos(turn),
                              0.0,
                              0.0};
    return drawn_linear{linear, std::max(sx, sy)};
}

// The linear part of `affine` applied to (x, y).
pixel_position_wide linear_of(const pixel_affine &affine, double x, double y) {
    return pixel_position_wide{(affine.column0_x * x) + (affine.column1_x * y),
                               (affine.column0_y * x) + (affine.column1_y * y)};
}

} // namespace

// THE UNIFORM CAP IS COUNTED AS BEFORE (fatia S5-C2-fix1; the S5-C2 review, C3). Below the cut, the
// count of a cap is the one of fatia S3 at the screen's error: n = ceil(pi / acos(1 - e / r))
// segments for the whole circle (4 at least, and 4 when r does not exceed e), half of it rounded up
// for the cap, 4 + 2 (half - 1) points, with r half the thickness in pixel unit and half the
// thickness times the larger scale in world unit (the largest stretch of a turned, scaled
// transform). Fatia S5-C2 measures r from the cap's two axes on the screen instead; this sweep
// checks that the count did not move: 400 lines of a fixed generator, both units, turned and scaled
// unevenly, errors from 1/16 to 8 pixels, r drawn below the cut. A case whose n lies within 1e-9 of
// a whole number would let the last bit of the radius decide it: it is skipped and counted.
namespace {

// The count of fatia S3 for a cap of radius r at error e, written out (never read from the code).
std::size_t points_of_the_uniform_cap(double r, double e) {
    if (!(r > e)) {
        return 6U; // 4 segments for the circle, 2 for the cap: 4 + 2 (2 - 1) points
    }
    const long double quotient = std::numbers::pi_v<long double> / std::acos(1.0L - (e / r));
    const auto whole = std::max<std::size_t>(4U, static_cast<std::size_t>(std::ceil(quotient)));
    const std::size_t half = (whole + 1U) / 2U;
    return 4U + (2U * (half - 1U));
}

// True when r is near enough to a boundary of the count (r = e, or n a whole number) for the last
// bit of the radius to decide it.
bool near_a_boundary_of_the_count(double r, double e) {
    if (std::fabs(r - e) <= 1.0e-9 * e) {
        return true;
    }
    if (!(r > e)) {
        return false;
    }
    const long double quotient = std::numbers::pi_v<long double> / std::acos(1.0L - (e / r));
    return std::fabs(quotient - std::round(quotient)) <= 1.0e-9L * quotient;
}

struct uniform_cap_case {
    gltfx_segment_world segment{};
    gltfx_line_style style{};
    ellipse_screen screen{};
    double radius = 0.0; // the cap's radius on the screen, by the formula of before
};

uniform_cap_case drawn_uniform_cap(lcg &g, bool world) {
    const drawn_linear linear = drawn_linear_part(g);
    const double error = std::exp2(-4.0 + (7.0 * g.next()));
    const double below_the_cut = 0.999 * error / (1.0 - std::cos(k_pi / 512.0));
    const double radius = below_the_cut * std::exp2(-20.0 * g.next());
    const double heading = 2.0 * k_pi * g.next();
    const double half_thickness = world ? radius / linear.larger : radius;
    const gltfx_segment_world segment{{0.0, 0.0},
                                      {50.0 * std::cos(heading), 50.0 * std::sin(heading)}};
    pixel_affine affine = linear.affine;
    affine.translation_x = 400.0;
    affine.translation_y = 300.0;
    return uniform_cap_case{segment,
                            style_of(2.0 * half_thickness,
                                     world ? gltfx_stroke_unit::world : gltfx_stroke_unit::pixel,
                                     gltfx_line_cap::rounded),
                            screen_of(affine, error), radius};
}

} // namespace

GLINTFX_TEST(stroke_line_pixels_the_uniform_cap_is_counted_by_the_formula_of_before) {
    const auto pixels = fresh();
    lcg g{20261026U};
    int checked = 0;
    int skipped = 0;
    int failures = 0;
    for (int i = 0; i < 400; ++i) {
        const uniform_cap_case c = drawn_uniform_cap(g, i % 2 == 1);
        if (near_a_boundary_of_the_count(c.radius, c.screen.max_error_pixels)) {
            ++skipped;
            continue;
        }
        ++checked;
        const std::size_t want = points_of_the_uniform_cap(c.radius, c.screen.max_error_pixels);
        const bool built =
            pixels->build(c.segment, c.style, k_half_red, c.screen) == stroke_pixels_outcome::built;
        if (!built || pixels->vertices().size() != want) {
            ++failures;
            std::printf("[FAIL UNIFORM CAP] case %d: r=%.17g e=%.17g points=%zu, expected %zu\n", i,
                        c.radius, c.screen.max_error_pixels, pixels->vertices().size(), want);
        }
    }
    std::printf("ponta uniforme casos=%d pulados=%d falhas=%d\n", checked, skipped, failures);
    GLINTFX_CHECK(checked + skipped == 400);
    GLINTFX_CHECK(checked >= 396);
    GLINTFX_CHECK(failures == 0);
}

// THE SWEEP OF THE CAP ON THE SURFACE (fatia S5-C2-fix1, the S5-C2 review, I1; up to 2^40 since
// fatia CURVE-FLOAT-NARROW). 400 lines of a fixed generator, in both units, under a batch transform
// turned and scaled unevenly, with the largest error from 1/16 to 8 pixels and the cap's larger
// radius on the screen from 1 to 2^40 pixels (both regimes); each line puts one of its caps (the
// end on half the cases, the start on the other half) across the 800 x 600 surface, through a point
// drawn on it. For every chord of that cap whose hull touches the surface grown by half a pixel
// (the code grows it by one: the half left over absorbs the test's reading of the parameters back
// from float vertices): the largest distance from the true half ellipse, built by hand from the
// line's own frame (in pixel unit with the cap at the end, the error put at the edge of its level,
// so that the chords of that level are as far from the curve as asked), to the chord between the
// two FLOAT vertices is within the error, plus a thousandth of it (the rounding of the double
// evaluation, as in arc_parameters_test), plus the rounding the public text of fill_ellipse()
// allows (renderer_2d.hpp): 2^-24 of the distance from the origin of the farthest point the ends
// may reach, the surface grown by 1 + 1024 pixels, or by 1 + 652 times the error when that is more
// (a cap is the edge itself: no thickness to add). And every end of those chords lies within that
// reach (the mechanism behind the bound). The floor (GODS_LAWS.md L-40): every case has at least
// one chord on the surface, or it measured nothing.
namespace {

struct cap_sweep_case {
    gltfx_segment_world segment{};
    gltfx_line_style style{};
    ellipse_screen screen{};
    pixel_ellipse cap{}; // the true half ellipse of the cap that crosses the surface
    double radius = 0.0; // the larger radius of the cap on the screen, as drawn
};

// The two axes of a cap on the screen, across then along, for a line heading `heading` in the
// world, half the thickness `half`: in world unit the world's two vectors carried by the linear
// part; in pixel unit the direction of the line on the screen, turned a quarter, at half the
// thickness.
std::array<pixel_position_wide, 2> cap_axes(const pixel_affine &linear, double heading, double half,
                                            bool world) {
    const double cx = std::cos(heading);
    const double cy = std::sin(heading);
    if (world) {
        return {linear_of(linear, -cy * half, cx * half), linear_of(linear, cx * half, cy * half)};
    }
    const pixel_position_wide d = linear_of(linear, cx, cy);
    const double length = std::hypot(d.x, d.y);
    const double ux = d.x / length;
    const double uy = d.y / length;
    return {pixel_position_wide{-uy * half, ux * half}, pixel_position_wide{ux * half, uy * half}};
}

// The error that puts the chords of a pixel-unit cap (a circle of `radius` on the screen) right at
// the edge of their level: the sagitta 2 r sin^2(w / 4) of a piece w = (pi / 2) / 2^j wide, plus
// 1e-9 of it, for the first level j from 8 whose pieces are no wider than the drawn error allows.
// Then the chords of that level are as far from the curve as asked, and the rounding of their
// corners is what is left to show (fatia CURVE-FLOAT-NARROW: the probe of the S5-C2-fix1 found the
// excess there). In the uniform regime there is no level, and the drawn error stays.
double error_at_level_edge(double radius, double drawn) {
    if (!(radius > drawn) || k_pi / std::acos(1.0 - (drawn / radius)) <= 512.0) {
        return drawn;
    }
    const double widest = 2.0 * std::acos(1.0 - (drawn / radius));
    double width = k_pi / 512.0;
    for (int level = 8; level < 40 && width > widest; ++level) {
        width *= 0.5;
    }
    const double s = std::sin(0.25 * width);
    return 2.0 * radius * s * s * (1.0 + 1.0e-9);
}

cap_sweep_case drawn_cap_case(lcg &g, bool world, bool at_end) {
    const drawn_linear linear = drawn_linear_part(g);
    const double drawn_error = std::exp2(-4.0 + (7.0 * g.next()));
    const double radius = std::exp2(40.0 * g.next());
    const double error = world || !at_end ? drawn_error : error_at_level_edge(radius, drawn_error);
    const double heading = 2.0 * k_pi * g.next();
    const double t0 = k_pi * (0.02 + (0.96 * g.next()));
    const pixel_position_wide target{800.0 * g.next(), 600.0 * g.next()};
    const double half = world ? radius / linear.larger : radius;
    const pixel_position_wide on_screen =
        linear_of(linear.affine, std::cos(heading), std::sin(heading));
    // The other end lies 8 half thicknesses back along the line, measured in the space of its unit.
    const double length = world ? 8.0 * half : 8.0 * half / std::hypot(on_screen.x, on_screen.y);
    const std::array<pixel_position_wide, 2> axes = cap_axes(linear.affine, heading, half, world);
    const double sign = at_end ? 1.0 : -1.0;
    const pixel_ellipse unplaced{
        {}, {sign * axes[0].x, sign * axes[0].y}, {sign * axes[1].x, sign * axes[1].y}};
    const pixel_position_wide centre{
        target.x - ((unplaced.axis_u.x * std::cos(t0)) + (unplaced.axis_v.x * std::sin(t0))),
        target.y - ((unplaced.axis_u.y * std::cos(t0)) + (unplaced.axis_v.y * std::sin(t0)))};
    // The capped end is the world origin, which the translation carries to `centre`.
    const double ox = length * std::cos(heading) * (at_end ? -1.0 : 1.0);
    const double oy = length * std::sin(heading) * (at_end ? -1.0 : 1.0);
    const gltfx_segment_world segment = at_end ? gltfx_segment_world{{ox, oy}, {0.0, 0.0}}
                                               : gltfx_segment_world{{0.0, 0.0}, {ox, oy}};
    pixel_affine affine = linear.affine;
    affine.translation_x = centre.x;
    affine.translation_y = centre.y;
    const gltfx_stroke_unit unit = world ? gltfx_stroke_unit::world : gltfx_stroke_unit::pixel;
    return cap_sweep_case{segment, style_of(2.0 * half, unit, gltfx_line_cap::rounded),
                          screen_of(affine, error),
                          pixel_ellipse{centre, unplaced.axis_u, unplaced.axis_v}, radius};
}

// A point of the cap read back from a float vertex: the coordinates of p - centre in the basis of
// the two axes are (cos t, sin t). `on` when the point lies on the ellipse within a thousandth of
// its size (the other points of the outline lie 8 half thicknesses away); t from 0 to pi, a sine
// below zero by rounding read as zero.
struct cap_point {
    bool on = false;
    long double t = 0.0L;
};

cap_point read_on_cap(const pixel_ellipse &cap, const colored_pixel_vertex &vertex) {
    const long double px = static_cast<long double>(vertex.position.x) - cap.center.x;
    const long double py = static_cast<long double>(vertex.position.y) - cap.center.y;
    const long double ux = cap.axis_u.x;
    const long double uy = cap.axis_u.y;
    const long double vx = cap.axis_v.x;
    const long double vy = cap.axis_v.y;
    const long double det = (ux * vy) - (vx * uy);
    const long double c = ((px * vy) - (py * vx)) / det;
    const long double s = ((py * ux) - (px * uy)) / det;
    return cap_point{std::fabs(std::hypot(c, s) - 1.0L) <= 1.0e-3L,
                     std::atan2(std::max(s, 0.0L), c)};
}

using glintfx::test_support::allowed_rounding;
using glintfx::test_support::chord_on_curve;
using glintfx::test_support::hull_touches;
using glintfx::test_support::larger_radius;
using glintfx::test_support::largest_distance_to_chord;
using glintfx::test_support::near_bound;
using glintfx::test_support::offset_wide;
using glintfx::test_support::outside_of;

// The 800 x 600 surface of the sweep, the band of a cap (one pixel: a cap has no reach), and the
// box a chord is measured in: the surface grown by half a pixel, inside the band, so that the
// oracle's own rounding never counts a piece the library drew coarse (harness/curve_oracle.hpp).
constexpr pixel_box k_sweep_surface{0.0, 0.0, 800.0, 600.0};
constexpr pixel_box k_cap_band{-1.0, -1.0, 801.0, 601.0};
constexpr pixel_box k_measured_box{-0.5, -0.5, 800.5, 600.5};

// The chord between two float vertices of the cap, with the parameters read back from them.
struct cap_chord {
    const colored_pixel_vertex *a = nullptr;
    const colored_pixel_vertex *b = nullptr;
    long double ta = 0.0L;
    long double tb = 0.0L;
};

offset_wide offset_of_vertex(const pixel_ellipse &cap, const colored_pixel_vertex &vertex) {
    return offset_wide{static_cast<long double>(vertex.position.x) - cap.center.x,
                       static_cast<long double>(vertex.position.y) - cap.center.y};
}

// The largest distance from the true half ellipse between the two ends of `chord` to the line
// through its two float vertices (harness/curve_oracle.hpp).
double chord_error(const pixel_ellipse &cap, const cap_chord &chord) {
    return largest_distance_to_chord(cap, chord_on_curve{chord.ta, chord.tb,
                                                         offset_of_vertex(cap, *chord.a),
                                                         offset_of_vertex(cap, *chord.b)});
}

// How far `vertex` lies outside the band of the cap, in multiples of the reach the ends may have.
double outside_fraction(const colored_pixel_vertex &vertex, double e) {
    const pixel_position_wide p{vertex.position.x, vertex.position.y};
    return outside_of(k_cap_band, p) / near_bound(e);
}

// What one case measured: the chords of the cap on the surface, the largest error past the asked
// distance, the farthest end (a fraction of its reach), and whether every chord held.
struct cap_measure {
    std::size_t chords = 0U;
    double excess = -1.0e300;
    double farthest = 0.0;
    bool holds = true;
};

void measure_chord(const cap_sweep_case &c, const cap_chord &chord, cap_measure &m) {
    const double e = c.screen.max_error_pixels;
    const double error = chord_error(c.cap, chord);
    const double farthest = std::max(outside_fraction(*chord.a, e), outside_fraction(*chord.b, e));
    ++m.chords;
    m.excess = std::max(m.excess, error - e);
    m.farthest = std::max(m.farthest, farthest);
    m.holds = m.holds &&
              error <= (e * (1.0 + 1.0e-3)) + allowed_rounding(k_sweep_surface, 0.0, e) &&
              farthest <= 1.0;
}

cap_measure measured_cap(const cap_sweep_case &c, std::span<const colored_pixel_vertex> vertices) {
    cap_measure m{};
    const std::size_t n = vertices.size();
    for (std::size_t k = 0U; k < n; ++k) {
        const cap_point a = read_on_cap(c.cap, vertices[k]);
        const cap_point b = read_on_cap(c.cap, vertices[(k + 1U) % n]);
        if (a.on && b.on &&
            hull_touches(c.cap, static_cast<double>(a.t), static_cast<double>(b.t),
                         k_measured_box)) {
            measure_chord(c, cap_chord{&vertices[k], &vertices[(k + 1U) % n], a.t, b.t}, m);
        }
    }
    return m;
}

} // namespace

GLINTFX_TEST(stroke_line_pixels_the_cap_meets_the_error_on_the_surface_swept) {
    const auto pixels = fresh();
    lcg g{20261027U};
    int failures = 0;
    cap_measure all{};
    double largest_radius = 0.0;
    for (int i = 0; i < 400; ++i) {
        const cap_sweep_case c = drawn_cap_case(g, i % 2 == 1, (i / 2) % 2 == 0);
        largest_radius = std::max(largest_radius, larger_radius(c.cap));
        const bool built =
            pixels->build(c.segment, c.style, k_half_red, c.screen) == stroke_pixels_outcome::built;
        const cap_measure m = built ? measured_cap(c, pixels->vertices()) : cap_measure{};
        all.chords += m.chords;
        all.excess = std::max(all.excess, m.excess);
        all.farthest = std::max(all.farthest, m.farthest);
        if (!built || m.chords == 0U || !m.holds) {
            ++failures;
            std::printf("[FAIL CAP SWEEP] case %d: built=%d chords=%zu excess=%.6g\n", i,
                        built ? 1 : 0, m.chords, m.excess);
        }
    }
    std::printf("varredura da ponta casos=400 cordas_medidas=%zu maior_excesso=%.6g "
                "maior_fracao_da_ponta=%.6g maior_raio=%.6g falhas=%d\n",
                all.chords, all.excess, all.farthest, largest_radius, failures);
    GLINTFX_CHECK(failures == 0);
    // The range the public text cites is reached, not only allowed by the generator (the review of
    // fatia S5-C2-fix1, C-2: drawn up to 2^10 instead, the sweep stayed green).
    GLINTFX_CHECK(largest_radius >= 0x1p39);
}
