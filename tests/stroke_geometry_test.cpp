// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <print>

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

using glintfx::gltfx_line_cap;
using glintfx::gltfx_segment_world;
using glintfx::gltfx_stroke;
using glintfx::gltfx_stroke_unit;
using glintfx::gltfx_transform;
using glintfx::draw2d::fan_index_count;
using glintfx::draw2d::pixel_affine;
using glintfx::draw2d::pixel_affine_from_transform;
using glintfx::draw2d::pixel_offset;
using glintfx::draw2d::pixel_position_wide;
using glintfx::draw2d::stroke_cap_radius_pixels;
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

// The outline of the identity frame of k_line with `cap`, compared point by point with `want`.
template <std::size_t N>
[[nodiscard]] bool outline_is(const stroke_frame &frame, gltfx_line_cap cap, std::uint32_t segments,
                              const std::array<point, N> &want) {
    if (stroke_outline_point_count(cap, segments) != N) {
        return false;
    }
    std::array<pixel_position_wide, N> got{};
    write_stroke_outline(frame, cap, segments, got);
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

GLINTFX_TEST(stroke_cap_radius_is_half_the_thickness_on_the_screen) {
    GLINTFX_CHECK(near(
        stroke_cap_radius_pixels(stroke_of(4.0, gltfx_stroke_unit::pixel), scaled(3.0, 1.0)), 2.0));
    GLINTFX_CHECK(near(
        stroke_cap_radius_pixels(stroke_of(4.0, gltfx_stroke_unit::world), pixel_affine{}), 2.0));
    GLINTFX_CHECK(near(
        stroke_cap_radius_pixels(stroke_of(4.0, gltfx_stroke_unit::world), scaled(3.0, 1.0)), 6.0));
}

GLINTFX_TEST(stroke_outline_point_count_closed_table) {
    GLINTFX_CHECK_EQ(stroke_outline_point_count(gltfx_line_cap::flat, 7U), std::size_t{4});
    GLINTFX_CHECK_EQ(stroke_outline_point_count(gltfx_line_cap::square, 7U), std::size_t{4});
    GLINTFX_CHECK_EQ(stroke_outline_point_count(gltfx_line_cap::rounded, 4U), std::size_t{10});
    GLINTFX_CHECK_EQ(stroke_outline_point_count(gltfx_line_cap::rounded, 2U), std::size_t{6});
    GLINTFX_CHECK_EQ(stroke_outline_point_count(gltfx_line_cap::rounded, 1U), std::size_t{4});
    GLINTFX_CHECK_EQ(stroke_outline_point_count(gltfx_line_cap::rounded, 0U), std::size_t{4});
    GLINTFX_CHECK_EQ(stroke_outline_point_count(gltfx_line_cap::rounded, 256U), std::size_t{514});
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
    write_stroke_outline(frame, gltfx_line_cap::rounded, 4U, got);
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
    for (pixel_position_wide &p : got) {
        p = pixel_position_wide{-7.0, -7.0};
    }
    write_stroke_outline(frame, gltfx_line_cap::flat, 4U, got);
    for (const pixel_position_wide &p : got) {
        GLINTFX_CHECK(p.x == -7.0 && p.y == -7.0);
    }
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
