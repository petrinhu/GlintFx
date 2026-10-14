// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <print>

#include <glintfx/core/angle.hpp>
#include <glintfx/core/ellipse.hpp>
#include <glintfx/core/transform.hpp>

#include "draw2d/ellipse_frame.hpp"
#include "draw2d/quad_vertices.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// ellipse_frame_test.cpp - R2D-SHAPES, fatia S5-B (D-W8-411): the two forms of an ellipse and the
// points of its curve by the angle of their outward normal (draw2d/ellipse_frame.hpp). The
// expected values are written BEFORE the code, outside it: by hand (a circle, a turn of a quarter,
// the half-axes of the house), by python (the shear (1, 0), (1, 1): its singular values are the
// golden ratio and its inverse, and the direction of the longer is 0.5535743588970453 rad), or by
// CONSTRUCTION (an ellipse made as R(turn) diag(major, minor) R(g), whose principal form is known
// before it is computed). No GL, no window.

using glintfx::gltfx_angle;
using glintfx::gltfx_ellipse_world;
using glintfx::gltfx_transform;
using glintfx::draw2d::conjugate_ellipse;
using glintfx::draw2d::local_point_at_normal_angle;
using glintfx::draw2d::normal_angle_of_offset;
using glintfx::draw2d::offset_at_parameter;
using glintfx::draw2d::pixel_affine_from_transform;
using glintfx::draw2d::plane_point;
using glintfx::draw2d::plane_point_of_local;
using glintfx::draw2d::principal_ellipse;
using glintfx::draw2d::principal_of;
using glintfx::draw2d::screen_conjugate_ellipse;
using glintfx::draw2d::world_conjugate_ellipse;

namespace {

constexpr double k_pi = std::numbers::pi;

[[nodiscard]] bool near(double got, double want, double tolerance) {
    return std::abs(got - want) <= tolerance;
}

// The point of normal angle phi, back in the plane.
[[nodiscard]] plane_point point_at_normal_angle(const principal_ellipse &e, double phi) {
    return plane_point_of_local(e, local_point_at_normal_angle(e, phi));
}

// True when `direction` lies along (x, y), either way: the axis has no sign.
[[nodiscard]] bool along(const plane_point &direction, double x, double y, double tolerance) {
    return std::abs((direction.x * y) - (direction.y * x)) <= tolerance &&
           std::abs(std::abs((direction.x * x) + (direction.y * y)) - 1.0) <= tolerance;
}

// A linear congruential generator with a fixed seed, so every system draws the same ellipses.
struct ellipse_draw {
    std::uint64_t state = 20261014U;
    [[nodiscard]] double next_unit() {
        state = (state * 6364136223846793005ULL) + 1442695040888963407ULL;
        return static_cast<double>(state >> 11U) * 0x1.0p-53;
    }
};

// The conjugate ellipse R(turn) diag(major, minor) R(g), with the minor axis mirrored when
// `mirrored`: its columns are the two conjugate half-axes, and its principal form is known.
[[nodiscard]] conjugate_ellipse constructed(double major, double minor, double turn, double g,
                                            bool mirrored) {
    const double signed_minor = mirrored ? -minor : minor;
    const double ct = std::cos(turn);
    const double st = std::sin(turn);
    const double cg = std::cos(g);
    const double sg = std::sin(g);
    // R(turn) diag(major, signed_minor) = [[major ct, -signed_minor st], [major st, signed_minor
    // ct]] times R(g) = [[cg, -sg], [sg, cg]].
    return conjugate_ellipse{
        .center = {3.0, -4.0},
        .axis_u = {(major * ct * cg) - (signed_minor * st * sg),
                   (major * st * cg) + (signed_minor * ct * sg)},
        .axis_v = {(-major * ct * sg) - (signed_minor * st * cg),
                   (-major * st * sg) + (signed_minor * ct * cg)},
    };
}

} // namespace

GLINTFX_TEST(ellipse_frame_the_world_and_the_screen_forms) {
    // rx 20, ry 10, turned a quarter: u = (0, 20), v = (-10, 0), within the rounding of cos(pi/2).
    const conjugate_ellipse quarter = world_conjugate_ellipse(
        gltfx_ellipse_world{{30.0, 20.0}, 20.0, 10.0, gltfx_angle{k_pi / 2.0}});
    GLINTFX_CHECK(near(quarter.center.x, 30.0, 0.0) && near(quarter.center.y, 20.0, 0.0));
    GLINTFX_CHECK(near(quarter.axis_u.x, 0.0, 1e-14) && near(quarter.axis_u.y, 20.0, 1e-14));
    GLINTFX_CHECK(near(quarter.axis_v.x, -10.0, 1e-14) && near(quarter.axis_v.y, 0.0, 1e-14));
    // Scale 2 and a translation (7, -5): the centre (30, 20) lands on (67, 35), u = (40, 0),
    // v = (0, 20).
    const conjugate_ellipse screen = screen_conjugate_ellipse(
        gltfx_ellipse_world{{30.0, 20.0}, 20.0, 10.0, gltfx_angle{0.0}},
        pixel_affine_from_transform(
            gltfx_transform{.translation = {7.0, -5.0}, .rotation = {0.0}, .scale = {2.0, 2.0}}));
    GLINTFX_CHECK(near(screen.center.x, 67.0, 0.0) && near(screen.center.y, 35.0, 0.0));
    GLINTFX_CHECK(near(screen.axis_u.x, 40.0, 0.0) && near(screen.axis_u.y, 0.0, 0.0));
    GLINTFX_CHECK(near(screen.axis_v.x, 0.0, 0.0) && near(screen.axis_v.y, 20.0, 0.0));
}

GLINTFX_TEST(ellipse_frame_principal_form_of_closed_cases) {
    const principal_ellipse circle =
        principal_of(conjugate_ellipse{{1.0, 2.0}, {10.0, 0.0}, {0.0, 10.0}});
    GLINTFX_CHECK(near(circle.major, 10.0, 1e-14) && near(circle.minor, 10.0, 1e-14));
    GLINTFX_CHECK(near(circle.center.x, 1.0, 0.0) && near(circle.center.y, 2.0, 0.0));
    // The quarter turn of 20 by 10: the longer half-axis lies along y.
    const principal_ellipse quarter = principal_of(world_conjugate_ellipse(
        gltfx_ellipse_world{{0.0, 0.0}, 20.0, 10.0, gltfx_angle{k_pi / 2.0}}));
    GLINTFX_CHECK(near(quarter.major, 20.0, 1e-13) && near(quarter.minor, 10.0, 1e-13));
    GLINTFX_CHECK(along(quarter.major_direction, 0.0, 1.0, 1e-14));
    // ry longer than rx: the longer half-axis is ry, along the turned y.
    const principal_ellipse tall = principal_of(
        world_conjugate_ellipse(gltfx_ellipse_world{{0.0, 0.0}, 5.0, 8.0, gltfx_angle{0.0}}));
    GLINTFX_CHECK(near(tall.major, 8.0, 1e-14) && near(tall.minor, 5.0, 1e-14));
    GLINTFX_CHECK(along(tall.major_direction, 0.0, 1.0, 1e-14));
    // The shear: the golden ratio and its inverse.
    const principal_ellipse shear =
        principal_of(conjugate_ellipse{{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}});
    GLINTFX_CHECK(near(shear.major, 1.6180339887498949, 1e-15));
    GLINTFX_CHECK(near(shear.minor, 0.6180339887498949, 1e-15));
    GLINTFX_CHECK(along(shear.major_direction, 0.8506508083520399, 0.5257311121191337, 1e-14));
    // A long thin ellipse keeps its thickness: the minor half-axis is the determinant, 1e-12,
    // over the major, never a difference that cancels.
    const principal_ellipse thin =
        principal_of(conjugate_ellipse{{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0e-12}});
    GLINTFX_CHECK(near(thin.major, 1.4142135623730951, 1e-15));
    GLINTFX_CHECK(std::abs(thin.minor - 7.071067811865475e-13) <= 1e-14 * 7.071067811865475e-13);
    // Both half-axes zero: nothing to turn, the direction (1, 0).
    const principal_ellipse point =
        principal_of(conjugate_ellipse{{4.0, 5.0}, {0.0, 0.0}, {0.0, 0.0}});
    GLINTFX_CHECK(point.major == 0.0 && point.minor == 0.0);
    GLINTFX_CHECK(point.major_direction.x == 1.0 && point.major_direction.y == 0.0);
    // Half-axes of 1e200 and of 1e-200: no square overflows, none underflows.
    const principal_ellipse huge =
        principal_of(conjugate_ellipse{{0.0, 0.0}, {-3.0e200, 0.0}, {0.0, 1.0e200}});
    GLINTFX_CHECK(std::abs(huge.major - 3.0e200) <= 1e-15 * 3.0e200);
    GLINTFX_CHECK(std::abs(huge.minor - 1.0e200) <= 1e-15 * 1.0e200);
    const principal_ellipse tiny =
        principal_of(conjugate_ellipse{{0.0, 0.0}, {0.0, 2.0e-200}, {1.0e-200, 0.0}});
    GLINTFX_CHECK(std::abs(tiny.major - 2.0e-200) <= 1e-15 * 2.0e-200);
    GLINTFX_CHECK(std::abs(tiny.minor - 1.0e-200) <= 1e-15 * 1.0e-200);
    GLINTFX_CHECK(along(tiny.major_direction, 0.0, 1.0, 1e-14));
}

// 2000 ellipses made by construction: major from 1e-3 to 1e3, the ratio of the two from 0 to 1,
// any turn, any start of the parameter, half of them mirrored. The principal form must give the
// constructed half-axes and, when they differ, the constructed direction.
GLINTFX_TEST(ellipse_frame_principal_form_sweep_against_the_construction) {
    ellipse_draw draw;
    std::size_t counted = 0;
    std::size_t wrong = 0;
    for (std::size_t i = 0; i < 2000U; ++i) {
        const double major = std::pow(10.0, -3.0 + (6.0 * draw.next_unit()));
        const double minor = major * draw.next_unit();
        const double turn = 2.0 * k_pi * draw.next_unit();
        const double g = 2.0 * k_pi * draw.next_unit();
        const bool mirrored = draw.next_unit() < 0.5;
        const principal_ellipse got = principal_of(constructed(major, minor, turn, g, mirrored));
        bool good = std::abs(got.major - major) <= 1e-13 * major &&
                    std::abs(got.minor - minor) <= 1e-13 * major;
        if (major - minor > 1e-6 * major) {
            good = good && along(got.major_direction, std::cos(turn), std::sin(turn), 1e-6);
        }
        ++counted;
        if (!good) {
            ++wrong;
            if (wrong <= 4U) {
                std::println("ellipse_frame_test: FALHOU varredura i={} maior={} menor={} "
                             "obtido {} {}",
                             i, major, minor, got.major, got.minor);
            }
        }
    }
    std::println("ellipse_frame_test: varredura casos={} erradas={}", counted, wrong);
    GLINTFX_CHECK_EQ(counted, std::size_t{2000});
    GLINTFX_CHECK_EQ(wrong, std::size_t{0});
}

GLINTFX_TEST(ellipse_frame_points_by_the_normal_angle_closed_cases) {
    const principal_ellipse e{{5.0, 7.0}, {1.0, 0.0}, 20.0, 10.0};
    const plane_point right = point_at_normal_angle(e, 0.0);
    GLINTFX_CHECK(near(right.x, 25.0, 1e-13) && near(right.y, 7.0, 1e-13));
    const plane_point top = point_at_normal_angle(e, k_pi / 2.0);
    GLINTFX_CHECK(near(top.x, 5.0, 1e-13) && near(top.y, 17.0, 1e-13));
    const plane_point left = point_at_normal_angle(e, k_pi);
    GLINTFX_CHECK(near(left.x, -15.0, 1e-13) && near(left.y, 7.0, 1e-13));
    // At 45 degrees: 20 (1, 1/4) / sqrt(1 + 1/4), that is (17.88854381999832, 4.47213595499958),
    // and the normal there, (x / 400, y / 100), is at 45 degrees indeed.
    const plane_point diagonal = point_at_normal_angle(e, k_pi / 4.0);
    GLINTFX_CHECK(near(diagonal.x, 5.0 + 17.88854381999832, 1e-13));
    GLINTFX_CHECK(near(diagonal.y, 7.0 + 4.47213595499958, 1e-13));
    // In the principal axes, and back in the plane for the major axis along y: local (20, 0) is
    // (0, 20), local (0, 10) is (-10, 0).
    const plane_point local = local_point_at_normal_angle(e, 0.0);
    GLINTFX_CHECK(near(local.x, 20.0, 1e-13) && near(local.y, 0.0, 1e-13));
    const principal_ellipse upright{{0.0, 0.0}, {0.0, 1.0}, 20.0, 10.0};
    const plane_point up = plane_point_of_local(upright, plane_point{20.0, 0.0});
    GLINTFX_CHECK(near(up.x, 0.0, 1e-15) && near(up.y, 20.0, 1e-15));
    const plane_point side = plane_point_of_local(upright, plane_point{0.0, 10.0});
    GLINTFX_CHECK(near(side.x, -10.0, 1e-15) && near(side.y, 0.0, 1e-15));
    // The normal angle of the four ends of the axes.
    const conjugate_ellipse c{{5.0, 7.0}, {20.0, 0.0}, {0.0, 10.0}};
    GLINTFX_CHECK(near(normal_angle_of_offset(e, offset_at_parameter(c, 0.0)), 0.0, 1e-15));
    GLINTFX_CHECK(
        near(normal_angle_of_offset(e, offset_at_parameter(c, k_pi / 2.0)), k_pi / 2.0, 1e-15));
    GLINTFX_CHECK(near(normal_angle_of_offset(e, offset_at_parameter(c, k_pi)), k_pi, 1e-15));
    GLINTFX_CHECK(
        near(normal_angle_of_offset(e, offset_at_parameter(c, 1.5 * k_pi)), 1.5 * k_pi, 1e-15));
}

// The curves with no thickness (D-W8-414): both half-axes zero, every phi names the centre and
// the normal angle is 0; the shorter one zero (a segment), every phi names the end of the segment
// its normal leans to (the sign of cos phi; cos(pi / 2) in double is 6.1e-17, not zero, so a
// quarter turn leans to the end along major_direction).
GLINTFX_TEST(ellipse_frame_points_of_the_curves_with_no_thickness) {
    const principal_ellipse point{{4.0, 5.0}, {1.0, 0.0}, 0.0, 0.0};
    const plane_point p = point_at_normal_angle(point, 1.2);
    GLINTFX_CHECK(p.x == 4.0 && p.y == 5.0);
    GLINTFX_CHECK(normal_angle_of_offset(point, plane_point{0.0, 0.0}) == 0.0);
    const principal_ellipse segment{{4.0, 5.0}, {0.0, 1.0}, 3.0, 0.0};
    const plane_point side = point_at_normal_angle(segment, k_pi / 2.0);
    GLINTFX_CHECK(near(side.x, 4.0, 1e-15) && near(side.y, 8.0, 1e-15));
    const plane_point past_side = point_at_normal_angle(segment, (k_pi / 2.0) + 1e-9);
    GLINTFX_CHECK(near(past_side.x, 4.0, 1e-15) && near(past_side.y, 2.0, 1e-15));
    const plane_point facing = point_at_normal_angle(segment, 0.3);
    GLINTFX_CHECK(near(facing.x, 4.0, 1e-15) && near(facing.y, 8.0, 1e-15));
    const plane_point behind = point_at_normal_angle(segment, 2.0);
    GLINTFX_CHECK(near(behind.x, 4.0, 1e-15) && near(behind.y, 2.0, 1e-15));
}

// The round trip, over 1000 constructed ellipses (the ratio of the half-axes from 1e-3 to 1) and
// a parameter each: the point whose normal angle is the one of the point at t IS the point at t.
GLINTFX_TEST(ellipse_frame_the_normal_angle_names_the_point_it_came_from) {
    ellipse_draw draw;
    std::size_t counted = 0;
    std::size_t wrong = 0;
    for (std::size_t i = 0; i < 1000U; ++i) {
        const double major = std::pow(10.0, -2.0 + (5.0 * draw.next_unit()));
        const double minor = major * std::pow(10.0, -3.0 * draw.next_unit());
        const conjugate_ellipse c =
            constructed(major, minor, 2.0 * k_pi * draw.next_unit(), 2.0 * k_pi * draw.next_unit(),
                        draw.next_unit() < 0.5);
        const principal_ellipse e = principal_of(c);
        const double t = 2.0 * k_pi * draw.next_unit();
        const plane_point offset = offset_at_parameter(c, t);
        const double phi = normal_angle_of_offset(e, offset);
        const plane_point back = point_at_normal_angle(e, phi);
        const bool good = phi >= 0.0 && phi < 2.0 * k_pi &&
                          std::abs(back.x - (c.center.x + offset.x)) <= 1e-9 * major &&
                          std::abs(back.y - (c.center.y + offset.y)) <= 1e-9 * major;
        ++counted;
        if (!good) {
            ++wrong;
            if (wrong <= 4U) {
                std::println("ellipse_frame_test: FALHOU ida e volta i={} t={} phi={}", i, t, phi);
            }
        }
    }
    std::println("ellipse_frame_test: ida e volta casos={} erradas={}", counted, wrong);
    GLINTFX_CHECK_EQ(counted, std::size_t{1000});
    GLINTFX_CHECK_EQ(wrong, std::size_t{0});
}
