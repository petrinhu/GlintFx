// SPDX-License-Identifier: AGPL-3.0-or-later
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <print>
#include <span>
#include <vector>

#include "draw2d/arc_parameters.hpp"
#include "draw2d/ellipse_frame.hpp"
#include "draw2d/ellipse_outline.hpp"
#include "draw2d/ellipse_triangles.hpp"
#include "draw2d/quad_vertices.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// ellipse_outline_test.cpp - R2D-SHAPES, fatia S5-B (D-W8-412, D-W8-416): the band of a thick
// ellipse (draw2d/ellipse_outline.hpp). The closed values come from python, before the code: the
// uniform counts from the sagitta, and the corner X of 20 by 10 with h = 6, x* =
// 13.856406460551021, whose distance to the ellipse is 6 (the edge of the hole). The sweep is
// GEOMETRIC, never the code's own formula: every triangle turns with the outer polygon, the hole
// is convex, every inner point is inside the ellipse and at least h from its curve (inside the
// erosion), and the true outer edge, the curve pushed out by h, sampled between every two angles,
// is at most the error from the outer polygon. No GL, no window.

using glintfx::draw2d::arc_extent;
using glintfx::draw2d::arc_parameters;
using glintfx::draw2d::arc_request;
using glintfx::draw2d::conjugate_ellipse;
using glintfx::draw2d::k_max_arc_parameters;
using glintfx::draw2d::local_point_at_normal_angle;
using glintfx::draw2d::merge_outline_angles;
using glintfx::draw2d::outline_angle_sources;
using glintfx::draw2d::outline_band;
using glintfx::draw2d::outline_hole;
using glintfx::draw2d::outline_pair;
using glintfx::draw2d::outline_pair_at;
using glintfx::draw2d::outline_shape;
using glintfx::draw2d::outline_shape_of;
using glintfx::draw2d::pixel_box;
using glintfx::draw2d::pixel_ellipse;
using glintfx::draw2d::pixel_position_wide;
using glintfx::draw2d::plane_point;
using glintfx::draw2d::principal_ellipse;
using glintfx::draw2d::uniform_normal_angle_count;
using glintfx::draw2d::write_fan_indices;
using glintfx::draw2d::write_ring_indices;

namespace {

constexpr double k_pi = std::numbers::pi;
constexpr double k_error = 0.25;

[[nodiscard]] bool near(double got, double want, double tolerance) {
    return std::abs(got - want) <= tolerance;
}

// A linear congruential generator with a fixed seed, so every system draws the same ellipses.
struct ellipse_draw {
    std::uint64_t state = 20261014U;
    [[nodiscard]] double next_unit() {
        state = (state * 6364136223846793005ULL) + 1442695040888963407ULL;
        return static_cast<double>(state >> 11U) * 0x1.0p-53;
    }
};

[[nodiscard]] double distance_to_segment(const plane_point &p, const plane_point &a,
                                         const plane_point &b) {
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    const double length_squared = (dx * dx) + (dy * dy);
    double along = 0.0;
    if (length_squared > 0.0) {
        along = std::clamp((((p.x - a.x) * dx) + ((p.y - a.y) * dy)) / length_squared, 0.0, 1.0);
    }
    return std::hypot(p.x - (a.x + (along * dx)), p.y - (a.y + (along * dy)));
}

// The distance from p to the curve of the axis-aligned ellipse major by minor at the origin, by
// a dense walk of the parameter and a refinement: an oracle of its own, not the code's formula.
[[nodiscard]] double distance_to_curve(const plane_point &p, double major, double minor) {
    double best_t = 0.0;
    double best = 1e300;
    constexpr int k_walk = 4096;
    for (int k = 0; k < k_walk; ++k) {
        const double t = 2.0 * k_pi * static_cast<double>(k) / k_walk;
        const double d = std::hypot(p.x - (major * std::cos(t)), p.y - (minor * std::sin(t)));
        if (d < best) {
            best = d;
            best_t = t;
        }
    }
    double step = 2.0 * k_pi / k_walk;
    for (int round = 0; round < 60; ++round) {
        for (const double t : {best_t - step, best_t + step}) {
            const double d = std::hypot(p.x - (major * std::cos(t)), p.y - (minor * std::sin(t)));
            if (d < best) {
                best = d;
                best_t = t;
            }
        }
        step *= 0.5;
    }
    return best;
}

// The point of the curve of the axis-aligned ellipse major by minor (minor > 0) whose outward
// normal has the angle phi, written here from the geometry and not read from the code: the normal
// of (major cos t, minor sin t) is along (cos t / major, sin t / minor), so tan phi = (major /
// minor) tan t.
[[nodiscard]] plane_point curve_point_of_normal(double major, double minor, double phi) {
    const double t = std::atan2(minor * std::sin(phi), major * std::cos(phi));
    return plane_point{major * std::cos(t), minor * std::sin(t)};
}

} // namespace

GLINTFX_TEST(ellipse_outline_uniform_counts_closed_table) {
    // python: arc_segment_count(h, e / 2), doubled and rounded up to a multiple of 4.
    GLINTFX_CHECK_EQ(uniform_normal_angle_count(0.0, k_error), std::size_t{8});
    GLINTFX_CHECK_EQ(uniform_normal_angle_count(1.0, k_error), std::size_t{16});
    GLINTFX_CHECK_EQ(uniform_normal_angle_count(2.0, k_error), std::size_t{20});
    GLINTFX_CHECK_EQ(uniform_normal_angle_count(10.0, k_error), std::size_t{40});
    GLINTFX_CHECK_EQ(uniform_normal_angle_count(100.0, k_error), std::size_t{128});
    GLINTFX_CHECK_EQ(uniform_normal_angle_count(1.0e6, k_error), std::size_t{1024});
}

GLINTFX_TEST(ellipse_outline_shape_of_each_region) {
    const principal_ellipse circle{{0.0, 0.0}, {1.0, 0.0}, 10.0, 10.0};
    GLINTFX_CHECK(outline_shape_of(circle, 2.0).hole == outline_hole::plain);
    GLINTFX_CHECK(outline_shape_of(circle, 10.0).hole == outline_hole::none);
    const principal_ellipse e{{0.0, 0.0}, {1.0, 0.0}, 20.0, 10.0}; // b^2 / a = 5
    GLINTFX_CHECK(outline_shape_of(e, 0.0).hole == outline_hole::plain);
    GLINTFX_CHECK(outline_shape_of(e, 5.0).hole == outline_hole::plain);
    const outline_shape cut = outline_shape_of(e, 6.0);
    GLINTFX_CHECK(cut.hole == outline_hole::cut);
    GLINTFX_CHECK(near(cut.cut_x, 13.856406460551021, 1e-12));
    // X is on the edge of the hole: exactly h from the curve.
    GLINTFX_CHECK(near(distance_to_curve(plane_point{cut.cut_x, 0.0}, 20.0, 10.0), 6.0, 1e-9));
    GLINTFX_CHECK(outline_shape_of(e, 10.0).hole == outline_hole::none);
    GLINTFX_CHECK(outline_shape_of(e, 50.0).hole == outline_hole::none);
    const principal_ellipse segment{{0.0, 0.0}, {1.0, 0.0}, 20.0, 0.0};
    GLINTFX_CHECK(outline_shape_of(segment, 0.0).hole == outline_hole::none);
    const principal_ellipse point{{0.0, 0.0}, {1.0, 0.0}, 0.0, 0.0};
    GLINTFX_CHECK(outline_shape_of(point, 1.0).hole == outline_hole::none);
}

GLINTFX_TEST(ellipse_outline_pairs_closed_cases) {
    const principal_ellipse circle{{100.0, 50.0}, {1.0, 0.0}, 10.0, 10.0};
    const outline_band thin{2.0, outline_shape_of(circle, 2.0)};
    const outline_pair right = outline_pair_at(circle, thin, 0.0);
    GLINTFX_CHECK(near(right.outer.x, 112.0, 1e-12) && near(right.outer.y, 50.0, 1e-12));
    GLINTFX_CHECK(near(right.inner.x, 108.0, 1e-12) && near(right.inner.y, 50.0, 1e-12));
    const principal_ellipse e{{0.0, 0.0}, {1.0, 0.0}, 20.0, 10.0};
    const outline_band one{1.0, outline_shape_of(e, 1.0)};
    const outline_pair top = outline_pair_at(e, one, k_pi / 2.0);
    GLINTFX_CHECK(near(top.outer.x, 0.0, 1e-12) && near(top.outer.y, 11.0, 1e-12));
    GLINTFX_CHECK(near(top.inner.x, 0.0, 1e-12) && near(top.inner.y, 9.0, 1e-12));
    // h = 6 cuts: at phi = 0 the inner point pushed in would be (14, 0), past X; it is X.
    const outline_band six{6.0, outline_shape_of(e, 6.0)};
    const outline_pair tip = outline_pair_at(e, six, 0.0);
    GLINTFX_CHECK(near(tip.outer.x, 26.0, 1e-12) && near(tip.inner.x, 13.856406460551021, 1e-12));
    const outline_pair other_tip = outline_pair_at(e, six, k_pi);
    GLINTFX_CHECK(near(other_tip.inner.x, -13.856406460551021, 1e-12));
    // A disc: both radii zero, the outer point is h along the normal.
    const principal_ellipse point{{5.0, 5.0}, {1.0, 0.0}, 0.0, 0.0};
    const outline_band disc{2.0, outline_shape_of(point, 2.0)};
    const outline_pair down = outline_pair_at(point, disc, 1.5 * k_pi);
    GLINTFX_CHECK(near(down.outer.x, 5.0, 1e-12) && near(down.outer.y, 3.0, 1e-12));
}

GLINTFX_TEST(ellipse_outline_angles_are_sorted_once_each) {
    // A circle: the normal angle of the point at t is t. t = 0 is already the first uniform angle
    // (counted once), and t = 0.1 falls between the first two: 9 angles, in order.
    const conjugate_ellipse plane{{0.0, 0.0}, {10.0, 0.0}, {0.0, 10.0}};
    const principal_ellipse circle{{0.0, 0.0}, {1.0, 0.0}, 10.0, 10.0};
    const std::array<double, 2> parameters{0.1, 0.0};
    std::array<double, 10> out{};
    const std::size_t n =
        merge_outline_angles(plane, circle, outline_angle_sources{parameters, 8U}, out);
    GLINTFX_CHECK_EQ(n, std::size_t{9});
    GLINTFX_CHECK(out[0] == 0.0);
    GLINTFX_CHECK(near(out[1], 0.1, 1e-15));
    for (std::size_t k = 2; k < n; ++k) {
        GLINTFX_CHECK(near(out[k], (k_pi / 4.0) * static_cast<double>(k - 1U), 1e-15));
    }
    std::array<double, 9> short_out{};
    GLINTFX_CHECK_EQ(
        merge_outline_angles(plane, circle, outline_angle_sources{parameters, 8U}, short_out),
        std::size_t{0});
}

// 240 bands: the major half-axis from 0.1 to 300, the ratio of the two from 0 (a segment) to 1, h
// from 0.01 to 300 across the three regions, and some ellipses of radius zero. The angles are the
// two sets of the header, as ellipse_pixels takes them (S5-B-fix1, D-W8-488; the S5-B review,
// IMPORTANTE-4): the parameters of arc_parameters() for half the error and the reach h, and the
// uniform normal angles. Both edges are measured against the geometry: the true outer edge, and,
// when the band has a hole, the true inner edge, the curve pushed in by h where that is the edge
// of the erosion (every angle of a plain hole; in a cut hole, the angles whose inner point has not
// crossed the major axis; the corner X is the closed case above).
GLINTFX_TEST(ellipse_outline_sweep_of_the_band) {
    ellipse_draw draw;
    std::size_t counted = 0;
    std::size_t wrong = 0;
    double worst_error = 0.0;
    double worst_inner = 0.0;
    std::size_t inner_samples = 0;
    std::vector<double> parameters;
    std::vector<double> angles;
    std::vector<plane_point> outer;
    std::vector<plane_point> inner;
    std::vector<pixel_position_wide> ring;
    std::vector<std::uint32_t> indices;
    for (std::size_t i = 0; i < 240U; ++i) {
        const std::array<double, 9> ratios{0.0, 1e-6, 1e-3, 0.01, 0.1, 0.3, 0.5, 0.9, 1.0};
        double major = std::pow(10.0, -1.0 + (3.477 * draw.next_unit()));
        double minor = major * ratios[i % ratios.size()];
        const double h = std::pow(10.0, -2.0 + (4.477 * draw.next_unit()));
        if (i % 40U == 39U) {
            major = 0.0;
            minor = 0.0;
        }
        const principal_ellipse e{{0.0, 0.0}, {1.0, 0.0}, major, minor};
        const conjugate_ellipse plane{{0.0, 0.0}, {major, 0.0}, {0.0, minor}};
        parameters.assign(k_max_arc_parameters, 0.0);
        const arc_request request{pixel_ellipse{{0.0, 0.0}, {major, 0.0}, {0.0, minor}},
                                  arc_extent::whole, 0.5 * k_error, h};
        const std::size_t count =
            arc_parameters(request, pixel_box{-1000.0, -1000.0, 1000.0, 1000.0}, parameters);
        const std::span<const double> from_arc(parameters.data(), count > 0U ? count - 1U : 0U);
        angles.assign(k_max_arc_parameters + 1024U, 0.0);
        const std::size_t n = merge_outline_angles(
            plane, e, outline_angle_sources{from_arc, uniform_normal_angle_count(h, k_error)},
            angles);
        const outline_band band{h, outline_shape_of(e, h)};
        outer.assign(n, plane_point{});
        inner.assign(n, plane_point{});
        for (std::size_t k = 0; k < n; ++k) {
            const outline_pair pair = outline_pair_at(e, band, angles[k]);
            outer[k] = pair.outer;
            inner[k] = pair.inner;
        }
        bool good = count >= 2U && n >= 8U;
        // The true outer edge between every two angles, against the chord between them.
        for (std::size_t k = 0; good && k < n; ++k) {
            const double from = angles[k];
            const double to = k + 1U < n ? angles[k + 1U] : angles[0] + (2.0 * k_pi);
            const plane_point &a = outer[k];
            const plane_point &b = outer[k + 1U < n ? k + 1U : 0U];
            for (int s = 1; s < 8; ++s) {
                const double phi = from + ((to - from) * static_cast<double>(s) / 8.0);
                const plane_point on_curve = local_point_at_normal_angle(e, phi);
                const plane_point edge{on_curve.x + (h * std::cos(phi)),
                                       on_curve.y + (h * std::sin(phi))};
                const double error = distance_to_segment(edge, a, b);
                worst_error = std::max(worst_error, error);
                good = good && error <= k_error * (1.0 + 1e-9);
            }
        }
        // The true inner edge between every two angles, against the inner sides around it.
        for (std::size_t k = 0; good && band.shape.hole != outline_hole::none && k < n; ++k) {
            const double from = angles[k];
            const double to = k + 1U < n ? angles[k + 1U] : angles[0] + (2.0 * k_pi);
            const plane_point &before = inner[k == 0U ? n - 1U : k - 1U];
            const plane_point &here = inner[k];
            const plane_point &after = inner[k + 1U < n ? k + 1U : 0U];
            const plane_point &later = inner[k + 2U < n ? k + 2U : (k + 2U) - n];
            for (int s = 1; s < 8; ++s) {
                const double phi = from + ((to - from) * static_cast<double>(s) / 8.0);
                const plane_point on_curve = curve_point_of_normal(major, minor, phi);
                const plane_point edge{on_curve.x - (h * std::cos(phi)),
                                       on_curve.y - (h * std::sin(phi))};
                // In a cut hole, the inner point that crossed the major axis is inside the
                // fold, and so is one ON the axis (its y at the rounding of the size): there the
                // edge is the corner X, the closed case above.
                const bool on_axis = std::abs(edge.y) <= 1e-9 * (major + h);
                if (band.shape.hole == outline_hole::cut &&
                    (on_axis || !(edge.y * std::sin(phi) > 0.0))) {
                    continue;
                }
                const double error = std::min({distance_to_segment(edge, before, here),
                                               distance_to_segment(edge, here, after),
                                               distance_to_segment(edge, after, later)});
                worst_inner = std::max(worst_inner, error);
                ++inner_samples;
                // Every sample counts (S5-B-fix2, D-W8-533): `good &&` keeps the failure of an
                // earlier sample of the band, which a plain assignment erased.
                good = good && error <= k_error * (1.0 + 1e-9);
            }
        }
        if (good && band.shape.hole != outline_hole::none) {
            ring.assign(2U * n, pixel_position_wide{});
            for (std::size_t k = 0; k < n; ++k) {
                ring[k] = {outer[k].x, outer[k].y};
                ring[n + k] = {inner[k].x, inner[k].y};
            }
            indices.assign(6U * n, 0U);
            good = write_ring_indices(ring, indices) == 6U * n;
            const double scale = (major + h) * (major + h);
            for (std::size_t t = 0; good && t < indices.size(); t += 3U) {
                const pixel_position_wide &p = ring[indices[t]];
                const pixel_position_wide &q = ring[indices[t + 1U]];
                const pixel_position_wide &r = ring[indices[t + 2U]];
                const double turn = ((q.x - p.x) * (r.y - p.y)) - ((q.y - p.y) * (r.x - p.x));
                good = turn >= -1e-9 * scale;
            }
            // The hole is convex, inside the ellipse, and at least h from its curve.
            for (std::size_t k = 0; good && k < n; ++k) {
                const plane_point &before = inner[k == 0U ? n - 1U : k - 1U];
                const plane_point &here = inner[k];
                const plane_point &after = inner[k + 1U < n ? k + 1U : 0U];
                const double turn = ((here.x - before.x) * (after.y - before.y)) -
                                    ((here.y - before.y) * (after.x - before.x));
                good = turn >= -1e-9 * scale;
                if (good && k % 16U == 0U) {
                    const double inside = ((here.x / major) * (here.x / major)) +
                                          ((here.y / minor) * (here.y / minor));
                    good = inside <= 1.0 + 1e-9 &&
                           distance_to_curve(here, major, minor) >= h - (1e-7 * (major + h));
                }
            }
        }
        if (good && band.shape.hole == outline_hole::none) {
            indices.assign(3U * n, 0U);
            good = write_fan_indices(n, indices) == 3U * n;
        }
        ++counted;
        if (!good) {
            ++wrong;
            if (wrong <= 4U) {
                std::println("ellipse_outline_test: FALHOU faixa i={} a={} b={} h={} n={}", i,
                             major, minor, h, n);
            }
        }
    }
    std::println("ellipse_outline_test: varredura faixas={} erradas={} pior_erro={} "
                 "amostras_internas={} pior_erro_interno={}",
                 counted, wrong, worst_error, inner_samples, worst_inner);
    GLINTFX_CHECK_EQ(counted, std::size_t{240});
    GLINTFX_CHECK_EQ(wrong, std::size_t{0});
    GLINTFX_CHECK(inner_samples >= 10000U);
    // The worst inner sample, printed above, is also asserted (S5-B-fix2, D-W8-533).
    GLINTFX_CHECK(worst_inner <= k_error * (1.0 + 1e-9));
}
