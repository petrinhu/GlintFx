// SPDX-License-Identifier: AGPL-3.0-or-later
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <print>
#include <vector>

#include <glintfx/core/segment.hpp>
#include <glintfx/core/transform.hpp>
#include <glintfx/draw2d/stroke.hpp>

#include "draw2d/quad_vertices.hpp"
#include "draw2d/stroke_geometry.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// stroke_geometry_test.cpp - R2D-SHAPES, fatia S3 (D-W8-269, D-W8-270, D-W8-273): the frame of a
// stroked line under each thickness unit, and its outline under each cap, on the line (10,10) to
// (50,10) with thickness 4 unless a case says otherwise. Every expected point is computed BEFORE
// the code and outside it (by hand for the axis-aligned ones; python, cos and sin of k pi / m, for
// the half discs and the turned square), never read from the code's output. No GL, no window.
// Fatia S5-C2 (D-W8-830, D-W8-831): a rounded cap is written at the parameters it is given, one set
// per cap; here the sets are the uniform ones of arc_parameters.hpp, t_k = pi k / n, so the points
// of before are the same.

using glintfx::gltfx_line_cap;
using glintfx::gltfx_segment_world;
using glintfx::gltfx_stroke;
using glintfx::gltfx_stroke_unit;
using glintfx::gltfx_transform;
using glintfx::draw2d::fan_index_count;
using glintfx::draw2d::pixel_affine;
using glintfx::draw2d::pixel_affine_from_transform;
using glintfx::draw2d::pixel_ellipse;
using glintfx::draw2d::pixel_offset;
using glintfx::draw2d::pixel_position_wide;
using glintfx::draw2d::stroke_cap_ellipse;
using glintfx::draw2d::stroke_cap_parameters;
using glintfx::draw2d::stroke_end;
using glintfx::draw2d::stroke_frame;
using glintfx::draw2d::stroke_frame_of;
using glintfx::draw2d::stroke_outline_point_count;
using glintfx::draw2d::write_fan_indices;
using glintfx::draw2d::write_stroke_outline;

namespace {

constexpr gltfx_segment_world k_line{{10.0, 10.0}, {50.0, 10.0}};
constexpr double k_tolerance = 1e-12;
constexpr double k_quarter_turn = 1.5707963267948966;
constexpr double k_twelfth_turn = 0.5235987755982988; // 30 degrees

[[nodiscard]] gltfx_stroke stroke_of(double thickness, gltfx_stroke_unit unit) {
    return gltfx_stroke{.thickness = thickness, .unit = unit};
}

[[nodiscard]] pixel_affine scaled(double sx, double sy) {
    return pixel_affine_from_transform(
        gltfx_transform{.translation = {0.0, 0.0}, .rotation = {0.0}, .scale = {sx, sy}});
}

[[nodiscard]] pixel_affine turned(double radians) {
    return pixel_affine_from_transform(
        gltfx_transform{.translation = {0.0, 0.0}, .rotation = {radians}, .scale = {1.0, 1.0}});
}

[[nodiscard]] bool near(double got, double want) { return std::abs(got - want) <= k_tolerance; }

[[nodiscard]] bool at(const pixel_position_wide &got, double x, double y) {
    return near(got.x, x) && near(got.y, y);
}

[[nodiscard]] bool is(const pixel_offset &got, double x, double y) {
    return near(got.x, x) && near(got.y, y);
}

// The four points of a frame, in one check each.
[[nodiscard]] bool frame_is(const stroke_frame &got, const std::array<double, 8> &want) {
    return at(got.start, want[0], want[1]) && at(got.end, want[2], want[3]) &&
           is(got.along, want[4], want[5]) && is(got.across, want[6], want[7]);
}

struct point {
    double x = 0.0;
    double y = 0.0;
};

// The uniform parameters of a half of `segments` steps: t_k = pi k / segments, k = 0 .. segments
// (arc_parameters.hpp, the uniform regime, in the same order of operations).
[[nodiscard]] std::vector<double> uniform_half(std::uint32_t segments) {
    std::vector<double> parameters;
    for (std::uint32_t k = 0U; k <= segments; ++k) {
        parameters.push_back(std::numbers::pi * static_cast<double>(k) /
                             static_cast<double>(segments));
    }
    return parameters;
}

// `count` parameters, whatever their values: only the sizes are read by the count.
[[nodiscard]] std::vector<double> sized(std::size_t count) { return std::vector<double>(count); }

[[nodiscard]] bool axes_are(const pixel_ellipse &got, const std::array<double, 6> &want) {
    return at(got.center, want[0], want[1]) && at(got.axis_u, want[2], want[3]) &&
           at(got.axis_v, want[4], want[5]);
}

// The outline of the identity frame of k_line with `cap`, compared point by point with `want`;
// both caps at the uniform parameters of `segments` steps.
template <std::size_t N>
[[nodiscard]] bool outline_is(const stroke_frame &frame, gltfx_line_cap cap, std::uint32_t segments,
                              const std::array<point, N> &want) {
    const std::vector<double> half = uniform_half(segments);
    const stroke_cap_parameters caps{half, half};
    if (stroke_outline_point_count(cap, caps) != N) {
        return false;
    }
    std::array<pixel_position_wide, N> got{};
    write_stroke_outline(frame, cap, caps, got);
    for (std::size_t i = 0; i < N; ++i) {
        if (!at(got[i], want[i].x, want[i].y)) {
            std::println("stroke_geometry_test: ponto {} veio ({}, {}), esperado ({}, {})", i,
                         got[i].x, got[i].y, want[i].x, want[i].y);
            return false;
        }
    }
    return true;
}

} // namespace

GLINTFX_TEST(stroke_frame_world_unit_is_the_world_frame_carried_by_the_transform) {
    const gltfx_stroke world = stroke_of(4.0, gltfx_stroke_unit::world);
    GLINTFX_CHECK(frame_is(stroke_frame_of(k_line, world, pixel_affine{}),
                           {10.0, 10.0, 50.0, 10.0, 2.0, 0.0, 0.0, 2.0}));
    GLINTFX_CHECK(frame_is(stroke_frame_of(k_line, world, scaled(2.0, 2.0)),
                           {20.0, 20.0, 100.0, 20.0, 4.0, 0.0, 0.0, 4.0}));
    // A non-uniform scale: 4 pixels thick across, the caps reach 6 pixels along.
    GLINTFX_CHECK(frame_is(stroke_frame_of(k_line, world, scaled(3.0, 1.0)),
                           {30.0, 10.0, 150.0, 10.0, 6.0, 0.0, 0.0, 2.0}));
    GLINTFX_CHECK(frame_is(stroke_frame_of(k_line, world, turned(k_quarter_turn)),
                           {-10.0, 10.0, -10.0, 50.0, 0.0, 2.0, -2.0, 0.0}));
}

GLINTFX_TEST(stroke_frame_pixel_unit_keeps_the_thickness_on_the_screen) {
    const gltfx_stroke pixel = stroke_of(4.0, gltfx_stroke_unit::pixel);
    GLINTFX_CHECK(frame_is(stroke_frame_of(k_line, pixel, scaled(2.0, 2.0)),
                           {20.0, 20.0, 100.0, 20.0, 2.0, 0.0, 0.0, 2.0}));
    GLINTFX_CHECK(frame_is(stroke_frame_of(k_line, pixel, scaled(3.0, 1.0)),
                           {30.0, 10.0, 150.0, 10.0, 2.0, 0.0, 0.0, 2.0}));
    GLINTFX_CHECK(frame_is(stroke_frame_of(k_line, pixel, turned(k_quarter_turn)),
                           {-10.0, 10.0, -10.0, 50.0, 0.0, 2.0, -2.0, 0.0}));
}

// D-W8-270: a line of length zero takes +x of the space of its unit. World unit, turned by 30
// degrees: +x of the WORLD, turned with it. Pixel unit: +x of the screen.
GLINTFX_TEST(stroke_frame_of_a_line_of_length_zero_takes_plus_x_of_its_unit) {
    const gltfx_segment_world point{{10.0, 10.0}, {10.0, 10.0}};
    GLINTFX_CHECK(frame_is(
        stroke_frame_of(point, stroke_of(4.0, gltfx_stroke_unit::world), turned(k_twelfth_turn)),
        {3.6602540378443882, 13.660254037844386, 3.6602540378443882, 13.660254037844386,
         1.7320508075688774, 0.99999999999999989, -0.99999999999999989, 1.7320508075688774}));
    GLINTFX_CHECK(frame_is(
        stroke_frame_of(point, stroke_of(4.0, gltfx_stroke_unit::pixel), turned(k_twelfth_turn)),
        {3.6602540378443882, 13.660254037844386, 3.6602540378443882, 13.660254037844386, 2.0, 0.0,
         0.0, 2.0}));
}

// A line whose ends are finite but whose difference is not (2e308): the direction is still +x.
GLINTFX_TEST(stroke_frame_direction_does_not_overflow_on_a_finite_line) {
    const gltfx_segment_world huge{{-1e308, 0.0}, {1e308, 0.0}};
    const stroke_frame frame =
        stroke_frame_of(huge, stroke_of(4.0, gltfx_stroke_unit::world), pixel_affine{});
    GLINTFX_CHECK(is(frame.along, 2.0, 0.0));
    GLINTFX_CHECK(is(frame.across, 0.0, 2.0));
}

// The cap is a half ellipse on the screen centred on its end: axis_u = across and axis_v = along
// around the end, both negated around the start. k_line, thickness 4: across (0, 2), along (2, 0);
// world unit under the scale (3, 1): along (6, 0), a half ellipse; pixel unit under it: (2, 0).
GLINTFX_TEST(stroke_cap_ellipse_is_the_half_ellipse_of_across_and_along_on_the_screen) {
    const stroke_frame world =
        stroke_frame_of(k_line, stroke_of(4.0, gltfx_stroke_unit::world), pixel_affine{});
    GLINTFX_CHECK(
        axes_are(stroke_cap_ellipse(world, stroke_end::end), {50.0, 10.0, 0.0, 2.0, 2.0, 0.0}));
    GLINTFX_CHECK(
        axes_are(stroke_cap_ellipse(world, stroke_end::start), {10.0, 10.0, 0.0, -2.0, -2.0, 0.0}));
    const stroke_frame stretched =
        stroke_frame_of(k_line, stroke_of(4.0, gltfx_stroke_unit::world), scaled(3.0, 1.0));
    GLINTFX_CHECK(axes_are(stroke_cap_ellipse(stretched, stroke_end::end),
                           {150.0, 10.0, 0.0, 2.0, 6.0, 0.0}));
    const stroke_frame pixel =
        stroke_frame_of(k_line, stroke_of(4.0, gltfx_stroke_unit::pixel), scaled(3.0, 1.0));
    GLINTFX_CHECK(
        axes_are(stroke_cap_ellipse(pixel, stroke_end::start), {30.0, 10.0, 0.0, -2.0, -2.0, 0.0}));
}

// Each cap adds its parameters strictly between 0 and pi, on its own: sizes (5, 5) give 3 + 3
// points over the 4 corners, (5, 3) give 3 + 1, a cap of 2 or none adds nothing; the largest, two
// caps of 1025 (arc_parameters.hpp), gives 4 + 2 x 1023 = 2050. Flat and square never read them.
GLINTFX_TEST(stroke_outline_point_count_closed_table) {
    const std::vector<double> five = sized(5U);
    const std::vector<double> three = sized(3U);
    const std::vector<double> two = sized(2U);
    const std::vector<double> largest = sized(1025U);
    GLINTFX_CHECK_EQ(stroke_outline_point_count(gltfx_line_cap::flat, {five, five}),
                     std::size_t{4});
    GLINTFX_CHECK_EQ(stroke_outline_point_count(gltfx_line_cap::square, {five, five}),
                     std::size_t{4});
    GLINTFX_CHECK_EQ(stroke_outline_point_count(gltfx_line_cap::rounded, {five, five}),
                     std::size_t{10});
    GLINTFX_CHECK_EQ(stroke_outline_point_count(gltfx_line_cap::rounded, {five, three}),
                     std::size_t{8});
    GLINTFX_CHECK_EQ(stroke_outline_point_count(gltfx_line_cap::rounded, {three, three}),
                     std::size_t{6});
    GLINTFX_CHECK_EQ(stroke_outline_point_count(gltfx_line_cap::rounded, {two, two}),
                     std::size_t{4});
    GLINTFX_CHECK_EQ(stroke_outline_point_count(gltfx_line_cap::rounded, {}), std::size_t{4});
    GLINTFX_CHECK_EQ(stroke_outline_point_count(gltfx_line_cap::rounded, {largest, largest}),
                     std::size_t{2050});
}

// The two caps are written each at its own parameters (D-W8-831): the end at 0, pi/2, pi (one
// point, the far point end + along = (52, 10)), the start at the uniform 4 steps (three points).
// Hand-computed: start - across = (10, 8), then the start's half disc of radius 2 from (10, 8)
// through (8, 10) to (10, 12).
GLINTFX_TEST(stroke_outline_writes_each_cap_at_its_own_parameters) {
    const stroke_frame frame =
        stroke_frame_of(k_line, stroke_of(4.0, gltfx_stroke_unit::world), pixel_affine{});
    const std::vector<double> end_half = uniform_half(2U);
    const std::vector<double> start_half = uniform_half(4U);
    std::array<pixel_position_wide, 8> got{};
    write_stroke_outline(frame, gltfx_line_cap::rounded, {end_half, start_half}, got);
    GLINTFX_CHECK(at(got[1], 50.0, 12.0));
    GLINTFX_CHECK(at(got[2], 52.0, 10.0));
    GLINTFX_CHECK(at(got[3], 50.0, 8.0));
    GLINTFX_CHECK(at(got[4], 10.0, 8.0));
    GLINTFX_CHECK(at(got[5], 8.5857864376269042, 8.5857864376269042));
    GLINTFX_CHECK(at(got[6], 8.0, 10.0));
    GLINTFX_CHECK(at(got[7], 8.5857864376269042, 11.414213562373096));
}

GLINTFX_TEST(stroke_outline_of_each_cap_on_the_identity) {
    const stroke_frame frame =
        stroke_frame_of(k_line, stroke_of(4.0, gltfx_stroke_unit::world), pixel_affine{});
    GLINTFX_CHECK(outline_is<4>(frame, gltfx_line_cap::flat, 4U,
                                {{{10.0, 12.0}, {50.0, 12.0}, {50.0, 8.0}, {10.0, 8.0}}}));
    GLINTFX_CHECK(outline_is<4>(frame, gltfx_line_cap::square, 4U,
                                {{{8.0, 12.0}, {52.0, 12.0}, {52.0, 8.0}, {8.0, 8.0}}}));
    // r = 2 pixels: 7 segments for the whole circle, 4 for each half (arc_segments_test).
    GLINTFX_CHECK(outline_is<10>(frame, gltfx_line_cap::rounded, 4U,
                                 {{{10.0, 12.0},
                                   {50.0, 12.0},
                                   {51.414213562373092, 11.414213562373096},
                                   {52.0, 10.0},
                                   {51.414213562373092, 8.5857864376269042},
                                   {50.0, 8.0},
                                   {10.0, 8.0},
                                   {8.5857864376269042, 8.5857864376269042},
                                   {8.0, 10.0},
                                   {8.5857864376269042, 11.414213562373096}}}));
}

// D-W8-270 on the screen: the square cap of a point, world unit turned by 30 degrees, is the
// square of side 4 around it, turned with the world.
GLINTFX_TEST(stroke_outline_of_a_point_with_the_square_cap_is_a_square_turned_with_the_world) {
    const gltfx_segment_world point{{10.0, 10.0}, {10.0, 10.0}};
    const stroke_frame frame =
        stroke_frame_of(point, stroke_of(4.0, gltfx_stroke_unit::world), turned(k_twelfth_turn));
    GLINTFX_CHECK(outline_is<4>(frame, gltfx_line_cap::square, 4U,
                                {{{0.92820323027551088, 14.392304845413262},
                                  {4.3923048454132658, 16.392304845413264},
                                  {6.3923048454132658, 12.928203230275509},
                                  {2.9282032302755105, 10.928203230275509}}}));
}

// A point with the rounded cap is a whole disc: every point at the radius from it.
GLINTFX_TEST(stroke_outline_of_a_point_with_the_rounded_cap_is_a_disc) {
    const gltfx_segment_world point{{10.0, 10.0}, {10.0, 10.0}};
    const stroke_frame frame =
        stroke_frame_of(point, stroke_of(4.0, gltfx_stroke_unit::pixel), pixel_affine{});
    std::array<pixel_position_wide, 10> got{};
    const std::vector<double> half = uniform_half(4U);
    write_stroke_outline(frame, gltfx_line_cap::rounded, {half, half}, got);
    std::size_t on_circle = 0;
    for (const pixel_position_wide &p : got) {
        if (near(std::hypot(p.x - 10.0, p.y - 10.0), 2.0)) {
            ++on_circle;
        }
    }
    GLINTFX_CHECK_EQ(on_circle, std::size_t{10});
}

GLINTFX_TEST(stroke_outline_of_the_wrong_size_writes_nothing) {
    const stroke_frame frame =
        stroke_frame_of(k_line, stroke_of(4.0, gltfx_stroke_unit::world), pixel_affine{});
    std::array<pixel_position_wide, 5> got{};
    got.fill(pixel_position_wide{-7.0, -7.0});
    write_stroke_outline(frame, gltfx_line_cap::flat, {}, got);
    GLINTFX_CHECK(std::ranges::all_of(
        got, [](const pixel_position_wide &p) { return p.x == -7.0 && p.y == -7.0; }));
}

GLINTFX_TEST(stroke_fan_of_a_convex_polygon) {
    GLINTFX_CHECK_EQ(fan_index_count(0U), std::size_t{0});
    GLINTFX_CHECK_EQ(fan_index_count(2U), std::size_t{0});
    GLINTFX_CHECK_EQ(fan_index_count(4U), std::size_t{6});
    GLINTFX_CHECK_EQ(fan_index_count(514U), std::size_t{1536});
    std::array<std::uint32_t, 6> four{};
    write_fan_indices(four);
    GLINTFX_CHECK(four == (std::array<std::uint32_t, 6>{0U, 1U, 2U, 0U, 2U, 3U}));
    std::array<std::uint32_t, 9> five{};
    write_fan_indices(five);
    GLINTFX_CHECK(five == (std::array<std::uint32_t, 9>{0U, 1U, 2U, 0U, 2U, 3U, 0U, 3U, 4U}));
}
