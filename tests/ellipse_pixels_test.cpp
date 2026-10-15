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
