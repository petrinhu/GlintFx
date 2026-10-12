// SPDX-License-Identifier: AGPL-3.0-or-later
#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <string_view>

#include <glintfx/core/angle.hpp>
#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/core/light_2d.hpp>
#include <glintfx/core/projected_shadow.hpp>
#include <glintfx/core/quad.hpp>
#include <glintfx/core/vec2.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// projected_shadow_test.cpp - R2D-SPRITE-SHADE, H1 (docs/auditoria-api-sprite-shade.md, sections
// 2 and 4.1; decisions D-W8-323 to D-W8-326 and D-W8-370; GODS_LAWS.md L-17, L-20, L-21).
//
// The expected values are FIXED here, written before the implementation existed. They come from
// the parecer's examples and the r2 section 7 table, never from the program's output.
//
// The tolerance is the one of D-W8-323: each component is within 4 * ulp(M) of the expected value,
// M being the largest absolute value among the fixed inputs (coordinates and heights, angles
// excluded) and the fixed expected outputs of that case. There is no absolute floor.
//
// The one exception is an elevation of exactly 90 degrees: its shadow must be EXACTLY zero length,
// compared with plain ==. A tolerance of 4 * ulp(20) would also accept the cosine residue of
// 1.2e-15, so the tolerance cannot tell an explicit zero from a near-zero.
//
// The last three cases (H1-fix, point 4) prove the "shadow_extent" refusal: inputs that are all
// accepted, but whose shadow would have a corner that is not a finite double.

namespace {

using glintfx::gltfx_angle_from_degrees;
using glintfx::gltfx_directional_light_2d;
using glintfx::gltfx_err_code;
using glintfx::gltfx_point_light_2d;
using glintfx::gltfx_quad_world;
using glintfx::gltfx_rslt;
using glintfx::gltfx_standing_piece_2d;
using glintfx::gltfx_vec2_world;

using quad_result = gltfx_rslt<gltfx_quad_world>;

struct point_xy {
    double x;
    double y;
};

struct quad_xy {
    point_xy top_left;
    point_xy top_right;
    point_xy bottom_right;
    point_xy bottom_left;
};

// The scale M of D-W8-323, from the fixed numbers of one case.
double scale_of(std::initializer_list<double> values) {
    double largest = 0.0;
    for (const double v : values) {
        largest = std::fmax(largest, std::fabs(v));
    }
    return largest;
}

// Bit-for-bit equality of two doubles: the same pattern, so -0.0 and 0.0 differ.
bool same_bits(double a, double b) {
    return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b);
}

// Four units in the last place of the scale M.
bool within_four_ulps(double got, double expected, double scale) {
    const double ulp = std::nextafter(scale, std::numeric_limits<double>::infinity()) - scale;
    return std::fabs(got - expected) <= 4.0 * ulp;
}

void expect_point_near(const gltfx_vec2_world &got, const point_xy &want, double scale) {
    GLINTFX_CHECK(within_four_ulps(got.x, want.x, scale));
    GLINTFX_CHECK(within_four_ulps(got.y, want.y, scale));
}

void expect_quad_near(const gltfx_quad_world &got, const quad_xy &want, double scale) {
    expect_point_near(got.top_left, want.top_left, scale);
    expect_point_near(got.top_right, want.top_right, scale);
    expect_point_near(got.bottom_right, want.bottom_right, scale);
    expect_point_near(got.bottom_left, want.bottom_left, scale);
}

// Returns the value of an ok result, or fails the case if it holds an error.
gltfx_quad_world value_or_fail(const quad_result &result) {
    GLINTFX_CHECK(result.has_value());
    return result.value();
}

// A refusal must name the field in rejected_value() and carry invalid_argument.
void expect_refused(const quad_result &result, std::string_view field) {
    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK(result.err().code() == gltfx_err_code::invalid_argument);
    GLINTFX_CHECK(result.err().rejected_value() == field);
}

gltfx_directional_light_2d sun(double azimuth_degrees, double elevation_degrees) {
    return gltfx_directional_light_2d{
        .azimuth = gltfx_angle_from_degrees(azimuth_degrees),
        .elevation = gltfx_angle_from_degrees(elevation_degrees),
    };
}

// A piece whose base runs along the y axis, from (0,0) to (0,10), standing 20 high.
gltfx_standing_piece_2d upright_piece_on_y_axis(double height) {
    return gltfx_standing_piece_2d{
        .base_start = {.x = 0.0, .y = 0.0},
        .base_end = {.x = 0.0, .y = 10.0},
        .height = height,
    };
}

gltfx_point_light_2d lamp_at(double x, double y, double height) {
    return gltfx_point_light_2d{.position = {.x = x, .y = y}, .height = height};
}

} // namespace

// Directional light, azimuth 0 (light from +x), elevation 45: the shadow moves 20 towards -x.
// The corners follow the roles of gltfx_quad_world (top_left, top_right, bottom_right,
// bottom_left): the top of base_start is the far end of the shadow.
GLINTFX_TEST(directional_azimuth_0_elevation_45_shifts_the_top_towards_minus_x) {
    const auto result =
        glintfx::gltfx_project_directional_shadow(sun(0.0, 45.0), upright_piece_on_y_axis(20.0));
    expect_quad_near(value_or_fail(result),
                     quad_xy{.top_left = {-20.0, 0.0},
                             .top_right = {-20.0, 10.0},
                             .bottom_right = {0.0, 10.0},
                             .bottom_left = {0.0, 0.0}},
                     scale_of({20.0, 10.0, 20.0}));
}

// Azimuth 90 (light from +y): the shadow moves 20 towards -y. If the azimuth were not inverted,
// the shadow would land on the lamp's side, at +20.
GLINTFX_TEST(directional_azimuth_90_elevation_45_shifts_the_top_towards_minus_y) {
    const auto result =
        glintfx::gltfx_project_directional_shadow(sun(90.0, 45.0), upright_piece_on_y_axis(20.0));
    expect_quad_near(value_or_fail(result),
                     quad_xy{.top_left = {0.0, -20.0},
                             .top_right = {0.0, -10.0},
                             .bottom_right = {0.0, 10.0},
                             .bottom_left = {0.0, 0.0}},
                     scale_of({20.0, 10.0, 20.0}));
}

// Elevation 30: the length is 20 / tan(30 degrees) = 20 * sqrt(3). The product 20 * tan(30) would
// be 11.547, so this case is the one that kills a tan-for-cot mutant. The expected value is the
// literal constant, not a call to std::tan.
GLINTFX_TEST(directional_elevation_30_length_is_height_over_tan) {
    const auto result =
        glintfx::gltfx_project_directional_shadow(sun(0.0, 30.0), upright_piece_on_y_axis(20.0));
    expect_quad_near(value_or_fail(result),
                     quad_xy{.top_left = {-34.641016151377546, 0.0},
                             .top_right = {-34.641016151377546, 10.0},
                             .bottom_right = {0.0, 10.0},
                             .bottom_left = {0.0, 0.0}},
                     scale_of({20.0, 10.0, 34.641016151377546}));
}

// Elevation 90 is ACCEPTED (the range is (0, 90], closed at 90), and the shadow is exactly zero
// length: the corners at the top coincide bit for bit with the corners on the ground.
GLINTFX_TEST(directional_elevation_90_is_accepted_and_has_exactly_zero_length) {
    const auto quad = value_or_fail(
        glintfx::gltfx_project_directional_shadow(sun(0.0, 90.0), upright_piece_on_y_axis(20.0)));
    GLINTFX_CHECK(quad.top_left.x == quad.bottom_left.x);
    GLINTFX_CHECK(quad.top_left.y == quad.bottom_left.y);
    GLINTFX_CHECK(quad.top_right.x == quad.bottom_right.x);
    GLINTFX_CHECK(quad.top_right.y == quad.bottom_right.y);
}

// Point light at (10,-30), height 40. The piece's base runs from (5,0) to (15,0) and stands 20
// high. Ray factor H / (H - h) = 40 / 20 = 2, so the top of (5,0) lands on (0,30) and the top of
// (15,0) on (20,30). Corners in the parecer's order: top_left, top_right, bottom_right,
// bottom_left. The factor h / H would put the top of (5,0) on (7.5,-15), so this case kills that
// mutant.
GLINTFX_TEST(point_light_projects_the_top_through_the_ray_to_the_ground) {
    const auto result = glintfx::gltfx_project_point_shadow(lamp_at(10.0, -30.0, 40.0),
                                                            gltfx_standing_piece_2d{
                                                                .base_start = {.x = 5.0, .y = 0.0},
                                                                .base_end = {.x = 15.0, .y = 0.0},
                                                                .height = 20.0,
                                                            });
    expect_quad_near(
        value_or_fail(result),
        quad_xy{.top_left = {0.0, 30.0},
                .top_right = {20.0, 30.0},
                .bottom_right = {15.0, 0.0},
                .bottom_left = {5.0, 0.0}},
        scale_of({10.0, 30.0, 40.0, 5.0, 15.0, 20.0, 0.0, 30.0, 20.0, 30.0, 15.0, 5.0}));
}

// A projection is a pure function: the same inputs give the same bits on the same binary.
GLINTFX_TEST(same_inputs_give_bit_identical_shadows_on_repeated_calls) {
    const auto light = sun(35.0, 52.0);
    const auto piece = upright_piece_on_y_axis(13.5);
    const auto first = value_or_fail(glintfx::gltfx_project_directional_shadow(light, piece));
    const auto second = value_or_fail(glintfx::gltfx_project_directional_shadow(light, piece));
    GLINTFX_CHECK(same_bits(first.top_left.x, second.top_left.x));
    GLINTFX_CHECK(same_bits(first.top_left.y, second.top_left.y));
    GLINTFX_CHECK(same_bits(first.top_right.x, second.top_right.x));
    GLINTFX_CHECK(same_bits(first.top_right.y, second.top_right.y));
    GLINTFX_CHECK(same_bits(first.bottom_right.x, second.bottom_right.x));
    GLINTFX_CHECK(same_bits(first.bottom_right.y, second.bottom_right.y));
    GLINTFX_CHECK(same_bits(first.bottom_left.x, second.bottom_left.x));
    GLINTFX_CHECK(same_bits(first.bottom_left.y, second.bottom_left.y));
}

// A zero-height piece is NOT refused: its shadow degenerates onto the base, covering no pixel.
GLINTFX_TEST(zero_height_piece_is_accepted_and_its_top_stays_on_the_base) {
    const auto quad = value_or_fail(
        glintfx::gltfx_project_directional_shadow(sun(0.0, 45.0), upright_piece_on_y_axis(0.0)));
    GLINTFX_CHECK(quad.top_left.x == quad.bottom_left.x);
    GLINTFX_CHECK(quad.top_right.y == quad.bottom_right.y);
}

// Every elevation outside (0, 90] is refused by name. NaN falls outside the range too.
GLINTFX_TEST(directional_refuses_elevation_outside_zero_to_ninety_by_name) {
    const double bad[] = {0.0, -5.0, 90.5, std::numeric_limits<double>::quiet_NaN()};
    for (const double degrees : bad) {
        expect_refused(glintfx::gltfx_project_directional_shadow(sun(0.0, degrees),
                                                                 upright_piece_on_y_axis(20.0)),
                       "elevation");
    }
}

// A non-finite azimuth is refused by name.
GLINTFX_TEST(directional_refuses_non_finite_azimuth_by_name) {
    const double bad[] = {std::numeric_limits<double>::quiet_NaN(),
                          std::numeric_limits<double>::infinity()};
    for (const double degrees : bad) {
        expect_refused(glintfx::gltfx_project_directional_shadow(sun(degrees, 45.0),
                                                                 upright_piece_on_y_axis(20.0)),
                       "azimuth");
    }
}

// A negative or non-finite piece height is refused by name.
GLINTFX_TEST(directional_refuses_negative_or_non_finite_piece_height_by_name) {
    const double bad[] = {-1.0, std::numeric_limits<double>::infinity(),
                          std::numeric_limits<double>::quiet_NaN()};
    for (const double height : bad) {
        expect_refused(glintfx::gltfx_project_directional_shadow(sun(0.0, 45.0),
                                                                 upright_piece_on_y_axis(height)),
                       "piece_height");
    }
}

// A non-finite base coordinate is refused by name.
GLINTFX_TEST(directional_refuses_non_finite_base_coordinate_by_name) {
    auto piece = upright_piece_on_y_axis(20.0);
    piece.base_end.y = std::numeric_limits<double>::quiet_NaN();
    expect_refused(glintfx::gltfx_project_directional_shadow(sun(0.0, 45.0), piece), "position");
}

// A zero-length base is NOT refused: it degenerates to a segment and covers no pixel.
GLINTFX_TEST(zero_length_base_is_accepted) {
    auto piece = upright_piece_on_y_axis(20.0);
    piece.base_end = piece.base_start;
    static_cast<void>(
        value_or_fail(glintfx::gltfx_project_directional_shadow(sun(0.0, 45.0), piece)));
}

// A point light whose height is equal to or below the piece's top is refused by name.
GLINTFX_TEST(point_refuses_light_height_not_greater_than_piece_height_by_name) {
    const gltfx_standing_piece_2d piece{
        .base_start = {.x = 5.0, .y = 0.0},
        .base_end = {.x = 15.0, .y = 0.0},
        .height = 20.0,
    };
    expect_refused(glintfx::gltfx_project_point_shadow(lamp_at(10.0, -30.0, 20.0), piece),
                   "light_height");
    expect_refused(glintfx::gltfx_project_point_shadow(lamp_at(10.0, -30.0, 15.0), piece),
                   "light_height");
    expect_refused(glintfx::gltfx_project_point_shadow(
                       lamp_at(10.0, -30.0, std::numeric_limits<double>::quiet_NaN()), piece),
                   "light_height");
}

// A non-finite light position is refused by name.
GLINTFX_TEST(point_refuses_non_finite_light_position_by_name) {
    const gltfx_standing_piece_2d piece{
        .base_start = {.x = 5.0, .y = 0.0},
        .base_end = {.x = 15.0, .y = 0.0},
        .height = 20.0,
    };
    expect_refused(glintfx::gltfx_project_point_shadow(
                       lamp_at(std::numeric_limits<double>::infinity(), -30.0, 40.0), piece),
                   "position");
}

// H1-fix, point 4. The smallest positive elevation is inside (0, 90], but 1 / tan of it is not a
// finite double: the shadow would be longer than a double can hold. The projection refuses it by
// name; it never returns an infinite or NaN corner.
GLINTFX_TEST(directional_grazing_elevation_is_refused_as_shadow_extent) {
    const gltfx_directional_light_2d grazing{
        .azimuth = gltfx_angle_from_degrees(0.0),
        .elevation = glintfx::gltfx_angle{.radians = std::numeric_limits<double>::denorm_min()},
    };
    expect_refused(
        glintfx::gltfx_project_directional_shadow(grazing, upright_piece_on_y_axis(20.0)),
        "shadow_extent");
}

// H1-fix2 (review IMPORTANT-1). The public contract says a grazing elevation is refused
// "even for a piece of height zero". With height 0 the far corners are 0 * inf, which is NaN, so
// this refusal comes from the result check and not from the input check. This case proves that
// promise on its own: a shortcut that accepts a zero height cannot pass the suite.
GLINTFX_TEST(directional_grazing_elevation_is_refused_even_for_a_piece_of_height_zero) {
    const gltfx_directional_light_2d grazing{
        .azimuth = gltfx_angle_from_degrees(0.0),
        .elevation = glintfx::gltfx_angle{.radians = std::numeric_limits<double>::denorm_min()},
    };
    expect_refused(glintfx::gltfx_project_directional_shadow(grazing, upright_piece_on_y_axis(0.0)),
                   "shadow_extent");
}

// H1-fix, point 4. Every input is finite and accepted, but the piece is as tall as the largest
// doubles, and base_end sits at -1e308: its top moves another 1e308 towards -x and overflows. Only
// top_right overflows (top_left lands on -1e308), so this case alone proves top_right is checked.
GLINTFX_TEST(directional_top_right_overflowing_the_double_range_is_refused_as_shadow_extent) {
    const gltfx_standing_piece_2d piece{
        .base_start = {.x = 0.0, .y = 0.0},
        .base_end = {.x = -1.0e308, .y = 0.0},
        .height = 1.0e308,
    };
    expect_refused(glintfx::gltfx_project_directional_shadow(sun(0.0, 45.0), piece),
                   "shadow_extent");
}

// H1-fix, point 4. The lamp at the origin, height 40, and a base_start at 1e308: the ray factor is
// 2, so the top of base_start lands at 2e308 and overflows. The top of base_end, (0,10), lands at
// (0,20), finite: this case alone proves top_left is checked.
GLINTFX_TEST(point_top_left_overflowing_the_double_range_is_refused_as_shadow_extent) {
    const gltfx_standing_piece_2d piece{
        .base_start = {.x = 1.0e308, .y = 0.0},
        .base_end = {.x = 0.0, .y = 10.0},
        .height = 20.0,
    };
    expect_refused(glintfx::gltfx_project_point_shadow(lamp_at(0.0, 0.0, 40.0), piece),
                   "shadow_extent");
}
