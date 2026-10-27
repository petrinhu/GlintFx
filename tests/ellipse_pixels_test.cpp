// SPDX-License-Identifier: AGPL-3.0-or-later
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <numbers>
#include <print>
#include <span>
#include <utility>

#include <glintfx/core/angle.hpp>
#include <glintfx/core/color.hpp>
#include <glintfx/core/ellipse.hpp>
#include <glintfx/core/transform.hpp>
#include <glintfx/draw2d/stroke.hpp>

#include "draw2d/arc_parameters.hpp"
#include "draw2d/ellipse_pixels.hpp"
#include "draw2d/quad_vertices.hpp"
#include "draw2d/triangle_batch.hpp"

#include "harness/check.hpp"
#include "harness/curve_oracle.hpp"
#include "harness/sweep_lcg.hpp"
#include "harness/test_registry.hpp"

// ellipse_pixels_test.cpp - R2D-SHAPES, fatia S5-B (D-W8-272, D-W8-412 to D-W8-418): one ellipse
// call turned into pixel triangles (draw2d/ellipse_pixels.hpp). The counts are the acceptance of
// the plan written before the code (plano-fechamento-w8-r2.md, S5: the circle of radius 10 has 15
// segments, 16 vertices and 45 indices, 45 segments under a scale of 10, the ellipse 20 by 10 has
// 20; arc_segments_test holds the table they come from); the positions are by hand (the outer and
// inner radius of a ring, the ends of the axes). No GL, no window.

using glintfx::gltfx_angle;
using glintfx::gltfx_ellipse_world;
using glintfx::gltfx_rgba;
using glintfx::gltfx_stroke;
using glintfx::gltfx_stroke_unit;
using glintfx::gltfx_transform;
using glintfx::draw2d::colored_pixel_vertex;
using glintfx::draw2d::ellipse_pixels;
using glintfx::draw2d::ellipse_pixels_outcome;
using glintfx::draw2d::ellipse_screen;
using glintfx::draw2d::pixel_affine;
using glintfx::draw2d::pixel_affine_from_transform;
using glintfx::draw2d::pixel_box;
using glintfx::draw2d::pixel_ellipse;
using glintfx::draw2d::pixel_position_wide;

namespace {

constexpr double k_pi = std::numbers::pi;
constexpr gltfx_rgba k_red{1.0F, 0.0F, 0.0F, 1.0F};

[[nodiscard]] ellipse_screen screen_of(const pixel_affine &affine) {
    return ellipse_screen{affine, pixel_box{0.0, 0.0, 800.0, 600.0}, 0.25};
}

[[nodiscard]] double distance(const colored_pixel_vertex &v, double x, double y) {
    return std::hypot(static_cast<double>(v.position.x) - x, static_cast<double>(v.position.y) - y);
}

// True when some vertex of `vertices` lies within 1e-4 of (x, y) (the positions are floats).
[[nodiscard]] bool has_vertex(std::span<const colored_pixel_vertex> vertices, double x, double y) {
    for (const colored_pixel_vertex &v : vertices) {
        if (distance(v, x, y) <= 1e-4) {
            return true;
        }
    }
    return false;
}

// True when every triangle of the last build turns the same way, or has no area.
[[nodiscard]] bool all_turn_alike(const ellipse_pixels &built) {
    const auto v = built.vertices();
    const auto i = built.indices();
    int positive = 0;
    int negative = 0;
    for (std::size_t t = 0; t + 2U < i.size(); t += 3U) {
        const auto &p = v[i[t]].position;
        const auto &q = v[i[t + 1U]].position;
        const auto &r = v[i[t + 2U]].position;
        const double turn = (static_cast<double>(q.x - p.x) * static_cast<double>(r.y - p.y)) -
                            (static_cast<double>(q.y - p.y) * static_cast<double>(r.x - p.x));
        positive += turn > 1e-3 ? 1 : 0;
        negative += turn < -1e-3 ? 1 : 0;
    }
    return positive == 0 || negative == 0;
}

// A band to measure against: the ellipse at the world origin, major by minor, with its major axis
// along x, or along y when `upright`, and the half thickness h.
struct true_band {
    double major = 0.0;
    double minor = 0.0;
    double h = 0.0;
    bool upright = false;
};

// The point of normal angle phi of the band's outer edge, in the world: written here from the
// geometry (the normal of (x, y) is along (x / major^2, y / minor^2); a segment's side is its
// centre), not read from the code.
[[nodiscard]] glintfx::gltfx_vec2_world true_outer(const true_band &band, double phi) {
    const double ratio = band.minor / band.major;
    const double c = std::cos(phi);
    const double s = std::sin(phi);
    const double norm = std::sqrt((c * c) + (ratio * ratio * s * s));
    const double x = norm > 0.0 ? band.major * c / norm : 0.0;
    const double y = norm > 0.0 ? band.major * ratio * ratio * s / norm : 0.0;
    const double along = x + (band.h * c);
    const double across = y + (band.h * s);
    return band.upright ? glintfx::gltfx_vec2_world{across, along}
                        : glintfx::gltfx_vec2_world{along, across};
}

// The distance from (x, y) to the closed polygon `polygon` (its nearest side), in pixels.
[[nodiscard]] double distance_to_polygon(double x, double y,
                                         std::span<const colored_pixel_vertex> polygon) {
    const std::size_t n = polygon.size();
    double best = std::numeric_limits<double>::infinity();
    for (std::size_t k = 0; k < n; ++k) {
        const auto &a = polygon[k].position;
        const auto &b = polygon[k + 1U < n ? k + 1U : 0U].position;
        const double ax = static_cast<double>(a.x);
        const double ay = static_cast<double>(a.y);
        const double dx = static_cast<double>(b.x) - ax;
        const double dy = static_cast<double>(b.y) - ay;
        const double length_squared = (dx * dx) + (dy * dy);
        double along = 0.0;
        if (length_squared > 0.0) {
            along = std::clamp((((x - ax) * dx) + ((y - ay) * dy)) / length_squared, 0.0, 1.0);
        }
        best = std::min(best, std::hypot(x - (ax + (along * dx)), y - (ay + (along * dy))));
    }
    return best;
}

// The largest distance from the true outer edge (40000 normal angles, each carried to the screen by
// `affine`) to `polygon`, the outer polygon of a build.
[[nodiscard]] double outer_error(std::span<const colored_pixel_vertex> polygon,
                                 const true_band &band, const pixel_affine &affine) {
    double worst = 0.0;
    for (int j = 0; j < 40000; ++j) {
        const double phi = 2.0 * k_pi * (static_cast<double>(j) + 0.5) / 40000.0;
        const auto p = glintfx::draw2d::pixel_position_of(true_outer(band, phi), affine);
        worst = std::max(worst, distance_to_polygon(p.x, p.y, polygon));
    }
    return worst;
}

// The largest distance from the true outer edge of a CIRCLE on the screen (centre (cx, cy), radius
// `outer_radius`: the curve's radius plus h, by hand), sampled at 20000 angles of its top, to
// `polygon`, counting only the points ON the surface 800 by 600; `on_surface` is how many were.
[[nodiscard]] double circle_edge_error_on_surface(std::span<const colored_pixel_vertex> polygon,
                                                  double cx, double cy, double outer_radius,
                                                  std::size_t &on_surface) {
    const double half_width = std::min(1.0, 1200.0 / outer_radius);
    double worst = 0.0;
    on_surface = 0;
    for (int j = 0; j < 20000; ++j) {
        const double angle = (k_pi / 2.0) - half_width +
                             (2.0 * half_width * (static_cast<double>(j) + 0.5) / 20000.0);
        const double x = cx + (outer_radius * std::cos(angle));
        const double y = cy - (outer_radius * std::sin(angle));
        if (x < 0.0 || x > 800.0 || y < 0.0 || y > 600.0) {
            continue;
        }
        ++on_surface;
        worst = std::max(worst, distance_to_polygon(x, y, polygon));
    }
    return worst;
}

} // namespace

GLINTFX_TEST(ellipse_pixels_filled_counts_of_the_plan) {
    const auto built = std::make_unique<ellipse_pixels>();
    const pixel_affine identity{};
    const gltfx_ellipse_world circle{{100.0, 100.0}, 10.0, 10.0, gltfx_angle{0.0}};
    GLINTFX_CHECK(built->build_fill(circle, k_red, screen_of(identity)) ==
                  ellipse_pixels_outcome::built);
    GLINTFX_CHECK_EQ(built->vertices().size(), std::size_t{16});
    GLINTFX_CHECK_EQ(built->indices().size(), std::size_t{45});
    GLINTFX_CHECK(distance(built->vertices()[0], 100.0, 100.0) <= 1e-4);
    GLINTFX_CHECK(distance(built->vertices()[1], 110.0, 100.0) <= 1e-4);
    GLINTFX_CHECK(built->vertices()[1].color.red == 1.0F &&
                  built->vertices()[1].color.green == 0.0F);
    const gltfx_ellipse_world small{{10.0, 10.0}, 10.0, 10.0, gltfx_angle{0.0}};
    const pixel_affine tenfold = pixel_affine_from_transform(
        gltfx_transform{.translation = {0.0, 0.0}, .rotation = {0.0}, .scale = {10.0, 10.0}});
    GLINTFX_CHECK(built->build_fill(small, k_red, screen_of(tenfold)) ==
                  ellipse_pixels_outcome::built);
    GLINTFX_CHECK_EQ(built->vertices().size(), std::size_t{46});
    GLINTFX_CHECK_EQ(built->indices().size(), std::size_t{135});
    const gltfx_ellipse_world wide{{100.0, 100.0}, 20.0, 10.0, gltfx_angle{0.0}};
    GLINTFX_CHECK(built->build_fill(wide, k_red, screen_of(identity)) ==
                  ellipse_pixels_outcome::built);
    GLINTFX_CHECK_EQ(built->vertices().size(), std::size_t{21});
    const gltfx_ellipse_world turned{{100.0, 100.0}, 20.0, 10.0, gltfx_angle{k_pi / 2.0}};
    GLINTFX_CHECK(built->build_fill(turned, k_red, screen_of(identity)) ==
                  ellipse_pixels_outcome::built);
    GLINTFX_CHECK(distance(built->vertices()[1], 100.0, 120.0) <= 1e-4);
}

GLINTFX_TEST(ellipse_pixels_outlined_positions) {
    const auto built = std::make_unique<ellipse_pixels>();
    const pixel_affine identity{};
    const gltfx_stroke two_pixels{2.0, gltfx_stroke_unit::pixel};
    // A ring of radius 10, 2 thick: the outer half at radius 11, the inner half at radius 9.
    const gltfx_ellipse_world circle{{100.0, 100.0}, 10.0, 10.0, gltfx_angle{0.0}};
    GLINTFX_CHECK(built->build_stroke(circle, two_pixels, k_red, screen_of(identity)) ==
                  ellipse_pixels_outcome::built);
    const auto ring = built->vertices();
    GLINTFX_CHECK(ring.size() % 2U == 0U && ring.size() >= 16U);
    GLINTFX_CHECK_EQ(built->indices().size(), 3U * ring.size());
    std::size_t off_radius = 0;
    for (std::size_t k = 0; k < ring.size(); ++k) {
        const double want = k < ring.size() / 2U ? 11.0 : 9.0;
        off_radius += std::abs(distance(ring[k], 100.0, 100.0) - want) <= 1e-4 ? 0U : 1U;
    }
    GLINTFX_CHECK_EQ(off_radius, std::size_t{0});
    GLINTFX_CHECK(all_turn_alike(*built));
    // The ellipse 20 by 10, 2 thick: the outer point at t = 0 is (121, 100) and at t = pi/2 it is
    // (100, 111); the inner ones (119, 100) and (100, 109). At both, the normal is the axis.
    const gltfx_ellipse_world wide{{100.0, 100.0}, 20.0, 10.0, gltfx_angle{0.0}};
    GLINTFX_CHECK(built->build_stroke(wide, two_pixels, k_red, screen_of(identity)) ==
                  ellipse_pixels_outcome::built);
    GLINTFX_CHECK(has_vertex(built->vertices(), 121.0, 100.0));
    GLINTFX_CHECK(has_vertex(built->vertices(), 100.0, 111.0));
    GLINTFX_CHECK(has_vertex(built->vertices(), 119.0, 100.0));
    GLINTFX_CHECK(has_vertex(built->vertices(), 100.0, 109.0));
    // h = 6 passes b^2 / a = 5: the hole has the corner X at 13.856406460551021 from the centre.
    const gltfx_stroke twelve{12.0, gltfx_stroke_unit::pixel};
    GLINTFX_CHECK(built->build_stroke(wide, twelve, k_red, screen_of(identity)) ==
                  ellipse_pixels_outcome::built);
    GLINTFX_CHECK(has_vertex(built->vertices(), 113.856406460551021, 100.0));
    GLINTFX_CHECK(has_vertex(built->vertices(), 100.0 - 13.856406460551021, 100.0));
    GLINTFX_CHECK(all_turn_alike(*built));
    // In world unit, under a scale of (3, 1): the world outline (11, 0) and (0, 11) carried to
    // (433, 300) and (400, 311).
    const pixel_affine stretched = pixel_affine_from_transform(
        gltfx_transform{.translation = {400.0, 300.0}, .rotation = {0.0}, .scale = {3.0, 1.0}});
    const gltfx_ellipse_world origin{{0.0, 0.0}, 10.0, 10.0, gltfx_angle{0.0}};
    GLINTFX_CHECK(built->build_stroke(origin, gltfx_stroke{2.0, gltfx_stroke_unit::world}, k_red,
                                      screen_of(stretched)) == ellipse_pixels_outcome::built);
    GLINTFX_CHECK(has_vertex(built->vertices(), 433.0, 300.0));
    GLINTFX_CHECK(has_vertex(built->vertices(), 400.0, 311.0));
    GLINTFX_CHECK(has_vertex(built->vertices(), 427.0, 300.0));
}

// The outlines with no hole and the outlines of no area (D-W8-414, D-W8-412).
GLINTFX_TEST(ellipse_pixels_outlined_degenerate_shapes) {
    const auto built = std::make_unique<ellipse_pixels>();
    const pixel_affine identity{};
    // Radius zero, 4 thick: the disc of radius 2, a fan of 20 rim points (the uniform count for a
    // reach of 2; the four arc parameters of a radius of zero all have the normal angle 0).
    const gltfx_ellipse_world dot{{50.0, 60.0}, 0.0, 0.0, gltfx_angle{0.0}};
    GLINTFX_CHECK(built->build_stroke(dot, gltfx_stroke{4.0, gltfx_stroke_unit::pixel}, k_red,
                                      screen_of(identity)) == ellipse_pixels_outcome::built);
    GLINTFX_CHECK_EQ(built->vertices().size(), std::size_t{21});
    GLINTFX_CHECK_EQ(built->indices().size(), std::size_t{60});
    GLINTFX_CHECK(distance(built->vertices()[0], 50.0, 60.0) <= 1e-4);
    std::size_t off_disc = 0;
    for (std::size_t k = 1; k < built->vertices().size(); ++k) {
        off_disc += std::abs(distance(built->vertices()[k], 50.0, 60.0) - 2.0) <= 1e-4 ? 0U : 1U;
    }
    GLINTFX_CHECK_EQ(off_disc, std::size_t{0});
    // h = 3 is at least b = 2: no hole, a fan from the centre.
    const gltfx_ellipse_world flat{{100.0, 100.0}, 20.0, 2.0, gltfx_angle{0.0}};
    GLINTFX_CHECK(built->build_stroke(flat, gltfx_stroke{6.0, gltfx_stroke_unit::pixel}, k_red,
                                      screen_of(identity)) == ellipse_pixels_outcome::built);
    GLINTFX_CHECK_EQ(built->indices().size(), 3U * (built->vertices().size() - 1U));
    GLINTFX_CHECK(all_turn_alike(*built));
    // Thickness zero: a ring whose two halves are the same points, of no area.
    const gltfx_ellipse_world circle{{100.0, 100.0}, 10.0, 10.0, gltfx_angle{0.0}};
    GLINTFX_CHECK(built->build_stroke(circle, gltfx_stroke{0.0, gltfx_stroke_unit::pixel}, k_red,
                                      screen_of(identity)) == ellipse_pixels_outcome::built);
    const auto zero = built->vertices();
    const std::size_t half = zero.size() / 2U;
    std::size_t apart = 0;
    for (std::size_t k = 0; k < half; ++k) {
        apart += zero[k].position.x == zero[half + k].position.x &&
                         zero[k].position.y == zero[half + k].position.y
                     ? 0U
                     : 1U;
    }
    GLINTFX_CHECK_EQ(apart, std::size_t{0});
}

GLINTFX_TEST(ellipse_pixels_refuses_what_the_vertex_program_cannot_scale) {
    const auto built = std::make_unique<ellipse_pixels>();
    const gltfx_ellipse_world circle{{0.0, 0.0}, 10.0, 10.0, gltfx_angle{0.0}};
    // A translation past the pixel range: every list empty.
    const pixel_affine far = pixel_affine_from_transform(
        gltfx_transform{.translation = {1.0e39, 0.0}, .rotation = {0.0}, .scale = {1.0, 1.0}});
    GLINTFX_CHECK(built->build_fill(circle, k_red, screen_of(far)) ==
                  ellipse_pixels_outcome::pixel_position_out_of_range);
    GLINTFX_CHECK(built->vertices().empty() && built->indices().empty());
    GLINTFX_CHECK(built->build_stroke(circle, gltfx_stroke{2.0, gltfx_stroke_unit::pixel}, k_red,
                                      screen_of(far)) ==
                  ellipse_pixels_outcome::pixel_position_out_of_range);
    GLINTFX_CHECK(built->vertices().empty() && built->indices().empty());
    // A screen ellipse too large to measure (radius 1e300 by a scale of 1e300).
    const gltfx_ellipse_world huge{{0.0, 0.0}, 1.0e300, 1.0e300, gltfx_angle{0.0}};
    const pixel_affine enormous = pixel_affine_from_transform(
        gltfx_transform{.translation = {0.0, 0.0}, .rotation = {0.0}, .scale = {1.0e300, 1.0e300}});
    GLINTFX_CHECK(built->build_fill(huge, k_red, screen_of(enormous)) ==
                  ellipse_pixels_outcome::pixel_position_out_of_range);
    GLINTFX_CHECK(built->vertices().empty());
    // A build after a refusal holds only itself.
    const pixel_affine identity{};
    GLINTFX_CHECK(built->build_fill(circle, k_red, screen_of(identity)) ==
                  ellipse_pixels_outcome::built);
    GLINTFX_CHECK_EQ(built->vertices().size(), std::size_t{16});
}

// A circle of radius 1e7 pixels whose edge crosses the surface: the cut regime of arc_parameters,
// the stroke within the fixed storage, every point in range. (The turn of each triangle is not
// asked here: far from the surface the float positions are 2 pixels apart, and a sliver's turn is
// noise; the turn is proved by the outlines near the surface above and by ellipse_outline_test.)
GLINTFX_TEST(ellipse_pixels_a_huge_outline_fits_the_storage) {
    const auto built = std::make_unique<ellipse_pixels>();
    const pixel_affine identity{};
    const gltfx_ellipse_world huge{{400.0, 1.0e7 + 300.0}, 1.0e7, 1.0e7, gltfx_angle{0.0}};
    GLINTFX_CHECK(built->build_stroke(huge, gltfx_stroke{2.0, gltfx_stroke_unit::pixel}, k_red,
                                      screen_of(identity)) == ellipse_pixels_outcome::built);
    GLINTFX_CHECK(built->vertices().size() <= glintfx::draw2d::k_max_ellipse_vertices);
    GLINTFX_CHECK(has_vertex(built->vertices(), 400.0, 299.0));
}

// The guarantee of the header (D-W8-418), measured on the screen against the true outer edge
// (python, before the code: 0.065 and 0.113 pixel):
//   - a thick outline on a thin ellipse in pixel unit: 300 by 30 at (400, 300), h = 20, past
//     b^2 / a = 3 (a ring, its outer polygon the first half of the vertices);
//   - a segment 100 long, upright, 20 thick in world unit, under a scale of (3, 1): the bar with
//     rounded ends, no hole (a fan, its outer polygon every vertex but the centre). Its ends are
//     stretched three times across, so the count of the uniform normal angles must follow the
//     reach ON THE SCREEN, 30 pixels: taken from the world thickness, 10, the edge passes the error
//     (0.364 pixel in python).
GLINTFX_TEST(ellipse_pixels_the_outer_edge_is_within_the_error) {
    const auto built = std::make_unique<ellipse_pixels>();
    const pixel_affine centred = pixel_affine_from_transform(
        gltfx_transform{.translation = {400.0, 300.0}, .rotation = {0.0}, .scale = {1.0, 1.0}});
    const gltfx_ellipse_world thin{{400.0, 300.0}, 300.0, 30.0, gltfx_angle{0.0}};
    GLINTFX_CHECK(built->build_stroke(thin, gltfx_stroke{40.0, gltfx_stroke_unit::pixel}, k_red,
                                      screen_of(pixel_affine{})) == ellipse_pixels_outcome::built);
    const auto ring = built->vertices();
    GLINTFX_CHECK(outer_error(ring.first(ring.size() / 2U), true_band{300.0, 30.0, 20.0, false},
                              centred) <= 0.25 + 1e-3);
    const pixel_affine stretched = pixel_affine_from_transform(
        gltfx_transform{.translation = {400.0, 300.0}, .rotation = {0.0}, .scale = {3.0, 1.0}});
    const gltfx_ellipse_world bar{{0.0, 0.0}, 0.0, 50.0, gltfx_angle{0.0}};
    GLINTFX_CHECK(built->build_stroke(bar, gltfx_stroke{20.0, gltfx_stroke_unit::world}, k_red,
                                      screen_of(stretched)) == ellipse_pixels_outcome::built);
    const auto fan = built->vertices();
    GLINTFX_CHECK_EQ(built->indices().size(), 3U * (fan.size() - 1U));
    GLINTFX_CHECK(outer_error(fan.subspan(1), true_band{50.0, 0.0, 10.0, true}, stretched) <=
                  0.25 + 1e-3);
}

// The arc's HALF of the error (S5-B-fix1, D-W8-485; the S5-B review, mutant MA). A circle of radius
// 80 pixels, 20 thick (h = 10): its true outer edge is the circle of radius 90, by hand. The radius
// of curvature, 80, is near 4 h, where the curve's own chords decide the step and the thickness
// multiplies their error by (80 + 10) / 80: with the arc at half the error the edge stays within
// it; with the arc at the whole error it passes it (measured before the code: 0.137 and 0.277
// pixel).
GLINTFX_TEST(ellipse_pixels_the_arc_takes_half_the_error) {
    const auto built = std::make_unique<ellipse_pixels>();
    const pixel_affine centred = pixel_affine_from_transform(
        gltfx_transform{.translation = {400.0, 300.0}, .rotation = {0.0}, .scale = {1.0, 1.0}});
    const gltfx_ellipse_world circle{{0.0, 0.0}, 80.0, 80.0, gltfx_angle{0.0}};
    GLINTFX_CHECK(built->build_stroke(circle, gltfx_stroke{20.0, gltfx_stroke_unit::pixel}, k_red,
                                      screen_of(centred)) == ellipse_pixels_outcome::built);
    const auto ring = built->vertices();
    GLINTFX_CHECK_EQ(built->indices().size(), 3U * ring.size());
    const double worst =
        outer_error(ring.first(ring.size() / 2U), true_band{80.0, 80.0, 10.0, false}, centred);
    std::println("ellipse_pixels_test: meio erro no arco pior_erro={}", worst);
    GLINTFX_CHECK(worst <= 0.25 + 1e-3);
}

// The CUT regime of the arc (S5-B-fix1, D-W8-486; the S5-B review, mutant MB). A circle of radius
// 20000 pixels whose curve passes 5 pixels BELOW the surface (centre (400, 20605)), 60 thick (h =
// 30): the curve itself is never on the surface, but the band reaches 25 pixels into it. Its true
// outer edge is the circle of radius 20030, by hand. The cut keeps the pieces whose band touches
// the surface only when it is told the band's reach; told 0, it draws them as long chords and the
// edge on the surface is pixels away (measured before the code: 0.094 with the reach, 13.5
// without).
GLINTFX_TEST(ellipse_pixels_the_cut_keeps_the_band_on_the_surface) {
    const auto built = std::make_unique<ellipse_pixels>();
    const gltfx_ellipse_world huge{{400.0, 20605.0}, 20000.0, 20000.0, gltfx_angle{0.0}};
    GLINTFX_CHECK(built->build_stroke(huge, gltfx_stroke{60.0, gltfx_stroke_unit::pixel}, k_red,
                                      screen_of(pixel_affine{})) == ellipse_pixels_outcome::built);
    const auto ring = built->vertices();
    GLINTFX_CHECK_EQ(built->indices().size(), 3U * ring.size());
    std::size_t on_surface = 0;
    const double worst = circle_edge_error_on_surface(ring.first(ring.size() / 2U), 400.0, 20605.0,
                                                      20030.0, on_surface);
    std::println("ellipse_pixels_test: corte com alcance pontos_na_tela={} pior_erro={}",
                 on_surface, worst);
    GLINTFX_CHECK(on_surface >= 1000U);
    GLINTFX_CHECK(worst <= 0.25 + 1e-3);
}

// THE FILL'S EDGE IN FLOAT ON THE SURFACE (fatia CURVE-FLOAT-NARROW). 400 filled ellipses of a
// fixed generator, under a batch transform turned and scaled unevenly (the larger scale from 1/8 to
// 8, the other up to 64 times smaller), with the larger world radius times the larger stretch drawn
// from 512 to 2^39.5 pixels (both regimes), the smaller one down to an eighth of it, a rotation
// drawn: so the larger radius on the screen lies from 64 to 2^39.5 pixels, and the sweep measures
// it from the true ellipse (the review of CURVE-FLOAT-NARROW, IMPORTANT-5, its twin here),
// and the largest error from 1/16 to 8 pixels; each crosses the 800 x 600 surface at a point drawn
// on it. The smaller screen radius stays above one pixel (512 / 8 / 64): below that, reading a
// parameter back from a float vertex is no longer well conditioned (measured: 22 of 400 ellipses of
// a first generator, down to 1e-4 of the larger radius from 1 pixel, had no rim point read back),
// and a curve that small never leaves the uniform regime, where its ends are near the surface
// anyway. The true screen ellipse is built by hand from the definition of core/ellipse.hpp: the two
// world radii turned by the rotation, carried by the linear part, about the centre carried by the
// transform. For every pair of consecutive rim vertices (the ones after the centre, in the order of
// t, the last one joined to the first) whose hull touches the surface grown by half a pixel: the
// largest distance from the true ellipse between their two parameters (read back from the FLOAT
// vertices) to their chord is within the error, plus a thousandth of it, plus the rounding the
// public text of fill_ellipse() allows (2^-24 of the distance from the origin of the farthest point
// the ends may reach: the surface grown by 1 + 1024 pixels, or 1 + 652 times the error when that is
// more); and both vertices lie within that reach. The floor (GODS_LAWS.md L-40): every ellipse has
// at least one chord on the surface.
namespace {

using glintfx::test_support::lcg;

struct fill_sweep_case {
    gltfx_ellipse_world ellipse{};
    ellipse_screen screen{};
    pixel_ellipse truth{}; // the true ellipse on the screen
};

// A linear part turned by a drawn angle and scaled unevenly, scale first and then rotation, as
// pixel_affine_from_transform builds it; `larger` is its largest stretch.
struct drawn_linear_part {
    pixel_affine affine{};
    double larger = 1.0;
};

drawn_linear_part drawn_linear(lcg &g) {
    const double turn = 2.0 * k_pi * g.next();
    double sx = std::exp2((6.0 * g.next()) - 3.0);
    double sy = sx * std::exp2(-6.0 * g.next());
    if (g.next() < 0.5) {
        std::swap(sx, sy);
    }
    return drawn_linear_part{pixel_affine{sx * std::cos(turn), sx * std::sin(turn),
                                          -sy * std::sin(turn), sy * std::cos(turn), 0.0, 0.0},
                             std::max(sx, sy)};
}

pixel_position_wide carried(const pixel_affine &a, double x, double y) {
    return pixel_position_wide{(a.column0_x * x) + (a.column1_x * y),
                               (a.column0_y * x) + (a.column1_y * y)};
}

fill_sweep_case drawn_fill_case(lcg &g) {
    const drawn_linear_part linear = drawn_linear(g);
    const double error = std::exp2(-4.0 + (7.0 * g.next()));
    const double radius = std::exp2(9.0 + (30.5 * g.next()));
    const double rx = radius / linear.larger;
    const double ry = rx * std::exp2(-3.0 * g.next());
    const double rotation = 2.0 * k_pi * g.next();
    const double t0 = 2.0 * k_pi * g.next();
    const pixel_position_wide target{800.0 * g.next(), 600.0 * g.next()};
    const pixel_position_wide u =
        carried(linear.affine, rx * std::cos(rotation), rx * std::sin(rotation));
    const pixel_position_wide v =
        carried(linear.affine, -ry * std::sin(rotation), ry * std::cos(rotation));
    const pixel_position_wide centre{target.x - ((u.x * std::cos(t0)) + (v.x * std::sin(t0))),
                                     target.y - ((u.y * std::cos(t0)) + (v.y * std::sin(t0)))};
    pixel_affine affine = linear.affine;
    affine.translation_x = centre.x;
    affine.translation_y = centre.y;
    return fill_sweep_case{gltfx_ellipse_world{{0.0, 0.0}, rx, ry, gltfx_angle{rotation}},
                           ellipse_screen{affine, pixel_box{0.0, 0.0, 800.0, 600.0}, error},
                           pixel_ellipse{centre, u, v}};
}

// A rim point read back from a float vertex: the coordinates of p - centre in the basis of the two
// axes are (cos t, sin t); `on` when the point lies on the ellipse within a thousandth of its size
// (the centre vertex does not).
struct rim_point {
    bool on = false;
    long double t = 0.0L;
};

rim_point read_on_rim(const pixel_ellipse &e, const colored_pixel_vertex &vertex) {
    const long double px = static_cast<long double>(vertex.position.x) - e.center.x;
    const long double py = static_cast<long double>(vertex.position.y) - e.center.y;
    const long double det = (static_cast<long double>(e.axis_u.x) * e.axis_v.y) -
                            (static_cast<long double>(e.axis_v.x) * e.axis_u.y);
    const long double c = ((px * e.axis_v.y) - (py * e.axis_v.x)) / det;
    const long double s = ((py * e.axis_u.x) - (px * e.axis_u.y)) / det;
    return rim_point{std::fabs(std::hypot(c, s) - 1.0L) <= 1.0e-3L, std::atan2(s, c)};
}

using glintfx::test_support::allowed_rounding;
using glintfx::test_support::chord_on_curve;
using glintfx::test_support::hull_touches;
using glintfx::test_support::largest_distance_to_chord;
using glintfx::test_support::near_bound;
using glintfx::test_support::offset_wide;
using glintfx::test_support::outside_of;

// The 800 x 600 surface of the sweep, the band of a fill (one pixel: a fill has no reach), and the
// box a chord is measured in: the surface grown by half a pixel, inside the band, so that the
// oracle's own rounding never counts a piece the library drew coarse (harness/curve_oracle.hpp).
constexpr pixel_box k_fill_surface{0.0, 0.0, 800.0, 600.0};
constexpr pixel_box k_fill_band{-1.0, -1.0, 801.0, 601.0};
constexpr pixel_box k_rim_measured_box{-0.5, -0.5, 800.5, 600.5};

// The chord between two float rim vertices, with the parameters read back from them.
struct rim_chord {
    const colored_pixel_vertex *a = nullptr;
    const colored_pixel_vertex *b = nullptr;
    long double ta = 0.0L;
    long double tb = 0.0L;
};

offset_wide rim_offset(const pixel_ellipse &e, const colored_pixel_vertex &vertex) {
    return offset_wide{static_cast<long double>(vertex.position.x) - e.center.x,
                       static_cast<long double>(vertex.position.y) - e.center.y};
}

// The largest distance from the true ellipse between the two ends of `chord` to the line through
// its two float vertices (harness/curve_oracle.hpp).
double rim_chord_error(const pixel_ellipse &e, const rim_chord &chord) {
    return largest_distance_to_chord(
        e, chord_on_curve{chord.ta, chord.tb, rim_offset(e, *chord.a), rim_offset(e, *chord.b)});
}

// How far `vertex` lies outside the band of the fill, in multiples of the reach the ends may have.
double rim_outside_fraction(const colored_pixel_vertex &vertex, double e) {
    const pixel_position_wide p{vertex.position.x, vertex.position.y};
    return outside_of(k_fill_band, p) / near_bound(e);
}

// What one case measured: the chords on the surface, the rim vertices not read back on the ellipse
// (every rim vertex is a point of it, so none may be; the review of CURVE-FLOAT-NARROW, COSMETIC-3:
// they used to be skipped without a count), the largest error past the asked distance, the
// farthest end, and whether every chord held.
struct fill_measure {
    std::size_t chords = 0U;
    std::size_t off_rim = 0U;
    double excess = -1.0e300;
    double farthest = 0.0;
    bool holds = true;
};

void measure_rim_chord(const fill_sweep_case &c, const rim_chord &chord, fill_measure &m) {
    const double e = c.screen.max_error_pixels;
    const double error = rim_chord_error(c.truth, chord);
    const double farthest =
        std::max(rim_outside_fraction(*chord.a, e), rim_outside_fraction(*chord.b, e));
    ++m.chords;
    m.excess = std::max(m.excess, error - e);
    m.farthest = std::max(m.farthest, farthest);
    m.holds = m.holds && error <= (e * (1.0 + 1.0e-3)) + allowed_rounding(k_fill_surface, 0.0, e) &&
              farthest <= 1.0;
}

fill_measure measured_fill(const fill_sweep_case &c, std::span<const colored_pixel_vertex> v) {
    fill_measure m{};
    const std::size_t rim = v.size() - 1U;
    for (std::size_t k = 0U; k < rim; ++k) {
        const colored_pixel_vertex &a = v[1U + k];
        const colored_pixel_vertex &b = v[1U + ((k + 1U) % rim)];
        const rim_point pa = read_on_rim(c.truth, a);
        const rim_point pb = read_on_rim(c.truth, b);
        m.off_rim += pa.on ? 0U : 1U;
        const long double tb = pb.t < pa.t ? pb.t + (2.0L * std::numbers::pi_v<long double>) : pb.t;
        if (pa.on && pb.on &&
            hull_touches(c.truth, static_cast<double>(pa.t), static_cast<double>(tb),
                         k_rim_measured_box)) {
            measure_rim_chord(c, rim_chord{&a, &b, pa.t, tb}, m);
        }
    }
    return m;
}

} // namespace

GLINTFX_TEST(ellipse_pixels_the_fill_edge_in_float_meets_the_error_on_the_surface_swept) {
    const auto built = std::make_unique<ellipse_pixels>();
    lcg g{20261029U};
    int failures = 0;
    fill_measure all{};
    double largest_radius = 0.0;
    for (int i = 0; i < 400; ++i) {
        const fill_sweep_case c = drawn_fill_case(g);
        largest_radius = std::max(largest_radius, glintfx::test_support::larger_radius(c.truth));
        const bool ok =
            built->build_fill(c.ellipse, k_red, c.screen) == ellipse_pixels_outcome::built;
        const fill_measure m = ok ? measured_fill(c, built->vertices()) : fill_measure{};
        all.chords += m.chords;
        all.off_rim += m.off_rim;
        all.excess = std::max(all.excess, m.excess);
        all.farthest = std::max(all.farthest, m.farthest);
        if (!ok || m.chords == 0U || m.off_rim != 0U || !m.holds) {
            ++failures;
            std::println("[FAIL FILL SWEEP] case {}: built={} chords={} excess={:.6g}", i,
                         ok ? 1 : 0, m.chords, m.excess);
        }
    }
    std::println("borda float casos=400 cordas_medidas={} fora_do_aro={} maior_excesso={:.6g} "
                 "maior_fracao_da_ponta={:.6g} maior_raio={:.6g} falhas={}",
                 all.chords, all.off_rim, all.excess, all.farthest, largest_radius, failures);
    GLINTFX_CHECK(failures == 0);
    // The range the public text cites is reached, measured on the true ellipse (fatia S5-C2-fix1,
    // C-2; CURVE-FLOAT-NARROW, IMPORTANT-5).
    GLINTFX_CHECK(largest_radius >= 0x1p39);
}

// THE ROUNDING OF A CORNER (fatia CURVE-FLOAT-NARROW-fix2; the review of CURVE-FLOAT-NARROW, its
// mutant R5). The public text of fill_ellipse() says that rounding moves a corner by at most 2^-24
// of its distance from the origin of the screen, or by 2^-149 pixel, whichever is more. The sweeps
// above only bound the rounding from above, so a bound twice as loose passed them. This case pins
// the constant from both sides on the library's one narrowing (narrowed_to_pixel): corners whose
// two coordinates lie at a tie between two floats, (1 + 2^-24) 2^k, and their neighbours one
// double away, for k from -100 to 100, and the subnormal ties 2^-150 and 3 2^-150. The largest move
// is at most the bound and, at the ties, within a millionth of it (IEEE 754: round to nearest moves
// a tie by half a unit in the last place, the bound itself).
namespace {

double rounding_move(const pixel_position_wide &p) {
    const glintfx::gltfx_vec2_screen q = glintfx::draw2d::narrowed_to_pixel(p);
    return static_cast<double>(
        std::hypot(static_cast<long double>(q.x) - p.x, static_cast<long double>(q.y) - p.y));
}

} // namespace

GLINTFX_TEST(ellipse_pixels_a_corner_rounds_by_at_most_the_bound_and_by_all_of_it_at_a_tie) {
    double largest = 0.0;
    int checked = 0;
    int over = 0;
    for (int k = -100; k <= 100; ++k) {
        const double tie = std::ldexp(1.0 + 0x1p-24, k);
        for (const double x : {std::nextafter(tie, 0.0), tie, std::nextafter(tie, 2.0 * tie)}) {
            const pixel_position_wide p{x, -tie};
            const double distance = std::hypot(p.x, p.y);
            const double ratio = rounding_move(p) / distance;
            ++checked;
            over += ratio > glintfx::test_support::k_single_rounding ? 1 : 0;
            largest = std::max(largest, ratio / glintfx::test_support::k_single_rounding);
        }
    }
    for (const double x : {0x1p-150, 0x3p-150}) {
        ++checked;
        over += rounding_move(pixel_position_wide{x, 0.0}) > 0x1p-149 ? 1 : 0;
    }
    std::println("arredondamento cantos={} acima={} maior_fracao_da_cota={:.9g}", checked, over,
                 largest);
    GLINTFX_CHECK(checked == 605);
    GLINTFX_CHECK(over == 0);
    GLINTFX_CHECK(largest >= 1.0 - 1.0e-6);
}
