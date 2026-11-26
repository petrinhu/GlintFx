// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/ellipse_frame.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

// draw2d/ellipse_frame.cpp - R2D-SHAPES, fatia S5-B: the forms and the points of ellipse_frame.hpp.
namespace glintfx::draw2d {

conjugate_ellipse world_conjugate_ellipse(const glintfx::gltfx_ellipse_world &ellipse) noexcept {
    const double cosine = std::cos(ellipse.rotation.radians);
    const double sine = std::sin(ellipse.rotation.radians);
    return conjugate_ellipse{
        .center = {ellipse.center.x, ellipse.center.y},
        .axis_u = {ellipse.radius_x * cosine, ellipse.radius_x * sine},
        .axis_v = {-ellipse.radius_y * sine, ellipse.radius_y * cosine},
    };
}

conjugate_ellipse screen_conjugate_ellipse(const glintfx::gltfx_ellipse_world &ellipse,
                                           const pixel_affine &affine) noexcept {
    const conjugate_ellipse world = world_conjugate_ellipse(ellipse);
    const pixel_position_wide center = pixel_position_of(ellipse.center, affine);
    return conjugate_ellipse{
        .center = {center.x, center.y},
        .axis_u = {(affine.column0_x * world.axis_u.x) + (affine.column1_x * world.axis_u.y),
                   (affine.column0_y * world.axis_u.x) + (affine.column1_y * world.axis_u.y)},
        .axis_v = {(affine.column0_x * world.axis_v.x) + (affine.column1_x * world.axis_v.y),
                   (affine.column0_y * world.axis_v.x) + (affine.column1_y * world.axis_v.y)},
    };
}

namespace {

// a d - b c with one rounding: the product b c is split into its rounded value and its exact
// error by a fused multiply-add, and the error is put back (W. Kahan's 2x2 determinant).
double determinant_of(double a, double b, double c, double d) noexcept {
    const double bc = b * c;
    const double bc_error = std::fma(-b, c, bc);
    const double ad_minus_bc = std::fma(a, d, -bc);
    return ad_minus_bc + bc_error;
}

} // namespace

principal_ellipse principal_of(const conjugate_ellipse &ellipse) noexcept {
    // The matrix [[a, b], [c, d]] whose columns are u and v.
    const double a = ellipse.axis_u.x;
    const double b = ellipse.axis_v.x;
    const double c = ellipse.axis_u.y;
    const double d = ellipse.axis_v.y;
    const double largest = std::max({std::fabs(a), std::fabs(b), std::fabs(c), std::fabs(d)});
    if (!(largest > 0.0)) {
        return principal_ellipse{.center = ellipse.center};
    }
    int exponent = 0;
    static_cast<void>(std::frexp(largest, &exponent));
    const double sa = std::ldexp(a, -exponent);
    const double sb = std::ldexp(b, -exponent);
    const double sc = std::ldexp(c, -exponent);
    const double sd = std::ldexp(d, -exponent);
    // The closed form of the 2x2 singular values (J. Blinn, learned and rewritten, GODS_LAWS.md
    // L-29): the larger is sqrt(e^2 + h^2) + sqrt(f^2 + g^2), and the turn of its direction is
    // half the sum of the angles of (f, g) and (e, h).
    const double e = 0.5 * (sa + sd);
    const double f = 0.5 * (sa - sd);
    const double g = 0.5 * (sc + sb);
    const double h = 0.5 * (sc - sb);
    const double larger = std::sqrt((e * e) + (h * h)) + std::sqrt((f * f) + (g * g));
    const double smaller = std::fabs(determinant_of(sa, sb, sc, sd)) / larger;
    const double turn = 0.5 * (std::atan2(g, f) + std::atan2(h, e));
    return principal_ellipse{
        .center = ellipse.center,
        .major_direction = {std::cos(turn), std::sin(turn)},
        .major = std::ldexp(larger, exponent),
        .minor = std::ldexp(smaller, exponent),
    };
}

plane_point offset_at_parameter(const conjugate_ellipse &ellipse, double t) noexcept {
    const double cosine = std::cos(t);
    const double sine = std::sin(t);
    return plane_point{(ellipse.axis_u.x * cosine) + (ellipse.axis_v.x * sine),
                       (ellipse.axis_u.y * cosine) + (ellipse.axis_v.y * sine)};
}

double normal_angle_of_offset(const principal_ellipse &principal,
                              const plane_point &offset) noexcept {
    if (!(principal.major > 0.0)) {
        return 0.0;
    }
    // The offset in the principal axes, divided by the longer half-axis, so nothing below
    // overflows. The outward normal of (x, y) is along (x / major^2, y / minor^2), that is along
    // (x ratio^2, y) with ratio = minor / major, at most 1.
    const plane_point &along = principal.major_direction;
    const double x = ((offset.x * along.x) + (offset.y * along.y)) / principal.major;
    const double y = ((offset.y * along.x) - (offset.x * along.y)) / principal.major;
    const double ratio = principal.minor / principal.major;
    const double phi = std::atan2(y, x * ratio * ratio);
    return phi < 0.0 ? phi + (2.0 * std::numbers::pi) : phi + 0.0;
}

plane_point local_point_at_normal_angle(const principal_ellipse &principal, double phi) noexcept {
    // In the principal axes the point of normal angle phi is
    // major (cos phi, ratio^2 sin phi) / sqrt(cos^2 phi + ratio^2 sin^2 phi).
    const double cosine = std::cos(phi);
    const double sine = std::sin(phi);
    const double ratio = principal.major > 0.0 ? principal.minor / principal.major : 0.0;
    const double norm = std::sqrt((cosine * cosine) + (ratio * ratio * sine * sine));
    if (!(norm > 0.0)) {
        return plane_point{};
    }
    return plane_point{principal.major * (cosine / norm),
                       principal.major * (ratio * ratio * sine / norm)};
}

plane_point plane_point_of_local(const principal_ellipse &principal,
                                 const plane_point &local) noexcept {
    const plane_point &along = principal.major_direction;
    return plane_point{principal.center.x + (local.x * along.x) - (local.y * along.y),
                       principal.center.y + (local.x * along.y) + (local.y * along.x)};
}

} // namespace glintfx::draw2d
