// SPDX-License-Identifier: AGPL-3.0-or-later
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <optional>
#include <print>
#include <span>
#include <utility>
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

namespace {

// The sweep of the band, in atoms (S5-B-fix3, D-W8-559; S5-B-fix4, D-W8-621): each band is drawn,
// built, written into triangles, checked and counted, one atom per step, so a check that changes
// changes one atom; the draws and the samples keep the order the counts depend on.

// One band of the sweep: its place in the draw, the two half-axes and the half thickness.
struct sweep_case {
    std::size_t index = 0;
    double major = 0.0;
    double minor = 0.0;
    double h = 0.0;
};

// What the sweep measured, over every band so far.
struct sweep_tally {
    std::size_t counted = 0;
    std::size_t wrong = 0;
    double worst_error = 0.0;
    double worst_inner = 0.0;
    std::size_t inner_samples = 0;
};

// The angles and the two edges of one band, with the buffers the checks reuse. The three counts
// are what the code under test answered, kept for band_holds to check.
struct sweep_band {
    outline_band band{};
    std::size_t parameter_count = 0;
    std::size_t n = 0;
    std::size_t index_count = 0;
    std::vector<double> parameters;
    std::vector<double> angles;
    std::vector<plane_point> outer;
    std::vector<plane_point> inner;
    std::vector<pixel_position_wide> ring;
    std::vector<std::uint32_t> indices;
};

// The band number `index` of the draw: the major half-axis from 0.1 to 300, the ratio of the two
// from the table, h from 0.01 to 300, and every fortieth ellipse of radius zero.
[[nodiscard]] sweep_case draw_sweep_case(ellipse_draw &draw, std::size_t index) {
    const std::array<double, 9> ratios{0.0, 1e-6, 1e-3, 0.01, 0.1, 0.3, 0.5, 0.9, 1.0};
    sweep_case c{index, 0.0, 0.0, 0.0};
    c.major = std::pow(10.0, -1.0 + (3.477 * draw.next_unit()));
    c.minor = c.major * ratios[index % ratios.size()];
    c.h = std::pow(10.0, -2.0 + (4.477 * draw.next_unit()));
    if (index % 40U == 39U) {
        c.major = 0.0;
        c.minor = 0.0;
    }
    return c;
}

// The ellipse of `c`, centred at the origin with its major axis along x.
[[nodiscard]] principal_ellipse principal_of(const sweep_case &c) {
    return principal_ellipse{{0.0, 0.0}, {1.0, 0.0}, c.major, c.minor};
}

// Fills `out` with the angles of `c` (the arc's parameters for half the error and the reach h,
// merged with the uniform normal angles) and the outer and inner point at each of them.
void build_sweep_band(const sweep_case &c, sweep_band &out) {
    const principal_ellipse e = principal_of(c);
    const conjugate_ellipse plane{{0.0, 0.0}, {c.major, 0.0}, {0.0, c.minor}};
    out.parameters.assign(k_max_arc_parameters, 0.0);
    const arc_request request{pixel_ellipse{{0.0, 0.0}, {c.major, 0.0}, {0.0, c.minor}},
                              arc_extent::whole, 0.5 * k_error, c.h};
    out.parameter_count =
        arc_parameters(request, pixel_box{-1000.0, -1000.0, 1000.0, 1000.0}, out.parameters);
    const std::span<const double> from_arc(
        out.parameters.data(), out.parameter_count > 0U ? out.parameter_count - 1U : 0U);
    out.angles.assign(k_max_arc_parameters + 1024U, 0.0);
    out.n = merge_outline_angles(
        plane, e, outline_angle_sources{from_arc, uniform_normal_angle_count(c.h, k_error)},
        out.angles);
    out.band = outline_band{c.h, outline_shape_of(e, c.h)};
    out.outer.assign(out.n, plane_point{});
    out.inner.assign(out.n, plane_point{});
    for (std::size_t k = 0; k < out.n; ++k) {
        const outline_pair pair = outline_pair_at(e, out.band, out.angles[k]);
        out.outer[k] = pair.outer;
        out.inner[k] = pair.inner;
    }
}

// Writes the triangles of `b` into its buffers: the ring between the outer and the inner polygon
// when the band has a hole, the fan of the outer polygon when it has none. The count the writer
// answers goes to `b.index_count`. Every band is written, the failed ones too, so that the checks
// only read (S5-B-fix4, D-W8-622).
void write_band_triangles(sweep_band &b) {
    const std::size_t n = b.n;
    if (b.band.shape.hole == outline_hole::none) {
        b.indices.assign(3U * n, 0U);
        b.index_count = write_fan_indices(n, b.indices);
        return;
    }
    b.ring.assign(2U * n, pixel_position_wide{});
    for (std::size_t k = 0; k < n; ++k) {
        b.ring[k] = {b.outer[k].x, b.outer[k].y};
        b.ring[n + k] = {b.inner[k].x, b.inner[k].y};
    }
    b.indices.assign(6U * n, 0U);
    b.index_count = write_ring_indices(b.ring, b.indices);
}

// Whether the true outer edge, between every two angles, is within the error of the chord
// between them. Stops after the first side that fails; every sample it reads is in `tally`.
[[nodiscard]] bool outer_edge_holds(const sweep_case &c, const sweep_band &b, sweep_tally &tally) {
    const principal_ellipse e = principal_of(c);
    const std::size_t n = b.n;
    bool good = true;
    for (std::size_t k = 0; good && k < n; ++k) {
        const double from = b.angles[k];
        const double to = k + 1U < n ? b.angles[k + 1U] : b.angles[0] + (2.0 * k_pi);
        const plane_point &a = b.outer[k];
        const plane_point &next = b.outer[k + 1U < n ? k + 1U : 0U];
        for (int s = 1; s < 8; ++s) {
            const double phi = from + ((to - from) * static_cast<double>(s) / 8.0);
            const plane_point on_curve = local_point_at_normal_angle(e, phi);
            const plane_point edge{on_curve.x + (c.h * std::cos(phi)),
                                   on_curve.y + (c.h * std::sin(phi))};
            const double error = distance_to_segment(edge, a, next);
            tally.worst_error = std::max(tally.worst_error, error);
            good = good && error <= k_error * (1.0 + 1e-9);
        }
    }
    return good;
}

// The distance from the true inner edge at the normal angle `phi`, between angles k and k + 1, to
// the three inner sides around it; none in a cut hole where the sample has folded onto the
// corner X (the inner point crossed the major axis, or sits ON it at the rounding of the size).
[[nodiscard]] std::optional<double> inner_sample_error(const sweep_case &c, const sweep_band &b,
                                                       std::size_t k, double phi) {
    const std::size_t n = b.n;
    const plane_point on_curve = curve_point_of_normal(c.major, c.minor, phi);
    const plane_point edge{on_curve.x - (c.h * std::cos(phi)), on_curve.y - (c.h * std::sin(phi))};
    const bool on_axis = std::abs(edge.y) <= 1e-9 * (c.major + c.h);
    if (b.band.shape.hole == outline_hole::cut && (on_axis || !(edge.y * std::sin(phi) > 0.0))) {
        return std::nullopt;
    }
    const plane_point &before = b.inner[k == 0U ? n - 1U : k - 1U];
    const plane_point &here = b.inner[k];
    const plane_point &after = b.inner[k + 1U < n ? k + 1U : 0U];
    const plane_point &later = b.inner[k + 2U < n ? k + 2U : (k + 2U) - n];
    return std::min({distance_to_segment(edge, before, here),
                     distance_to_segment(edge, here, after),
                     distance_to_segment(edge, after, later)});
}

// Whether the true inner edge between angles k and k + 1 is within the error of the inner sides
// around it, at its seven samples. Every sample counts (S5-B-fix2, D-W8-533): `good &&` keeps the
// failure of an earlier sample of the side.
[[nodiscard]] bool inner_side_holds(const sweep_case &c, const sweep_band &b, std::size_t k,
                                    sweep_tally &tally) {
    const double from = b.angles[k];
    const double to = k + 1U < b.n ? b.angles[k + 1U] : b.angles[0] + (2.0 * k_pi);
    bool good = true;
    for (int s = 1; s < 8; ++s) {
        const double phi = from + ((to - from) * static_cast<double>(s) / 8.0);
        const std::optional<double> error = inner_sample_error(c, b, k, phi);
        if (error.has_value()) {
            tally.worst_inner = std::max(tally.worst_inner, *error);
            ++tally.inner_samples;
            good = good && *error <= k_error * (1.0 + 1e-9);
        }
    }
    return good;
}

// Whether the true inner edge, between every two angles, is within the error of the inner sides
// around it (always, when the band has no hole). Stops after the first side that fails.
[[nodiscard]] bool inner_edge_holds(const sweep_case &c, const sweep_band &b, sweep_tally &tally) {
    if (b.band.shape.hole == outline_hole::none) {
        return true;
    }
    bool good = true;
    for (std::size_t k = 0; good && k < b.n; ++k) {
        good = inner_side_holds(c, b, k, tally);
    }
    return good;
}

// Whether every triangle of the ring of `b` turns with the outer polygon (a triangle of no area,
// at the rounding of the size, passes).
[[nodiscard]] bool ring_turns_with_the_outline(const sweep_case &c, const sweep_band &b) {
    const double scale = (c.major + c.h) * (c.major + c.h);
    bool good = true;
    for (std::size_t t = 0; good && t < b.indices.size(); t += 3U) {
        const pixel_position_wide &p = b.ring[b.indices[t]];
        const pixel_position_wide &q = b.ring[b.indices[t + 1U]];
        const pixel_position_wide &r = b.ring[b.indices[t + 2U]];
        const double turn = ((q.x - p.x) * (r.y - p.y)) - ((q.y - p.y) * (r.x - p.x));
        good = turn >= -1e-9 * scale;
    }
    return good;
}

// Whether the hole of `b` is convex: at every inner point, the inner polygon turns the way the
// outer one does (a straight run, at the rounding of the size, passes).
[[nodiscard]] bool hole_is_convex(const sweep_case &c, const sweep_band &b) {
    const std::size_t n = b.n;
    const double scale = (c.major + c.h) * (c.major + c.h);
    bool good = true;
    for (std::size_t k = 0; good && k < n; ++k) {
        const plane_point &before = b.inner[k == 0U ? n - 1U : k - 1U];
        const plane_point &here = b.inner[k];
        const plane_point &after = b.inner[k + 1U < n ? k + 1U : 0U];
        const double turn = ((here.x - before.x) * (after.y - before.y)) -
                            ((here.y - before.y) * (after.x - before.x));
        good = turn >= -1e-9 * scale;
    }
    return good;
}

// Whether every sixteenth inner point of `b` lies in the erosion of the ellipse by h, the points
// inside the ellipse at least h from its curve.
[[nodiscard]] bool hole_is_inside_the_erosion(const sweep_case &c, const sweep_band &b) {
    bool good = true;
    for (std::size_t k = 0; good && k < b.n; k += 16U) {
        const plane_point &here = b.inner[k];
        const double inside =
            ((here.x / c.major) * (here.x / c.major)) + ((here.y / c.minor) * (here.y / c.minor));
        good = inside <= 1.0 + 1e-9 &&
               distance_to_curve(here, c.major, c.minor) >= c.h - (1e-7 * (c.major + c.h));
    }
    return good;
}

// Whether band `b` of `c` holds every check, in the order of the sweep: enough angles, the outer
// edge, the inner edge, every index of its triangles written, then, around a hole, the ring and
// the hole itself. Every sample the edges read is in `tally`.
[[nodiscard]] bool band_holds(const sweep_case &c, const sweep_band &b, sweep_tally &tally) {
    const bool has_hole = b.band.shape.hole != outline_hole::none;
    bool good = b.parameter_count >= 2U && b.n >= 8U;
    good = good && outer_edge_holds(c, b, tally);
    good = good && inner_edge_holds(c, b, tally);
    good = good && b.index_count == (has_hole ? 6U : 3U) * b.n;
    if (good && has_hole) {
        good = ring_turns_with_the_outline(c, b) && hole_is_convex(c, b) &&
               hole_is_inside_the_erosion(c, b);
    }
    return good;
}

// Counts one band in `tally`: every band in `counted`, a band that failed also in `wrong`.
void count_band(sweep_tally &tally, bool good) {
    ++tally.counted;
    if (good) {
        return;
    }
    ++tally.wrong;
}

// Prints band `c` when it failed and is among the first four that did: a red sweep names bands
// to reproduce, without a line for every one of them. Reads the count of `tally` already updated.
void report_failed_band(const sweep_tally &tally, const sweep_case &c, std::size_t n, bool good) {
    if (good || tally.wrong > 4U) {
        return;
    }
    std::println("ellipse_outline_test: FALHOU faixa i={} a={} b={} h={} n={}", c.index, c.major,
                 c.minor, c.h, n);
}

} // namespace

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
    sweep_tally tally;
    sweep_band band;
    for (std::size_t i = 0; i < 240U; ++i) {
        const sweep_case c = draw_sweep_case(draw, i);
        build_sweep_band(c, band);
        write_band_triangles(band);
        const bool good = band_holds(c, band, tally);
        count_band(tally, good);
        report_failed_band(tally, c, band.n, good);
    }
    std::println("ellipse_outline_test: varredura faixas={} erradas={} pior_erro={} "
                 "amostras_internas={} pior_erro_interno={}",
                 tally.counted, tally.wrong, tally.worst_error, tally.inner_samples,
                 tally.worst_inner);
    GLINTFX_CHECK_EQ(tally.counted, std::size_t{240});
    // The worst inner sample is asserted BEFORE the count of wrong bands (S5-B-fix3, D-W8-562):
    // it does not read the bookkeeping of `good`, so a check that loses a failed inner sample
    // still fails here.
    GLINTFX_CHECK(tally.worst_inner <= k_error * (1.0 + 1e-9));
    GLINTFX_CHECK_EQ(tally.wrong, std::size_t{0});
    GLINTFX_CHECK(tally.inner_samples >= 10000U);
}

// The ring check of the sweep fails on a ring of its own (S5-B-fix4, D-W8-623): with a correct
// library no band of the sweep fails it, so without this case nothing shows that it can.
// A ring the writer built passes; the same ring with its first triangle turned back does not.
GLINTFX_TEST(ellipse_outline_the_ring_check_refuses_a_triangle_that_turns_back) {
    const sweep_case c{0U, 20.0, 10.0, 2.0};
    sweep_band b;
    build_sweep_band(c, b);
    write_band_triangles(b);
    GLINTFX_CHECK(b.band.shape.hole != outline_hole::none);
    GLINTFX_CHECK(b.n >= 8U);
    GLINTFX_CHECK_EQ(b.index_count, 6U * b.n);
    GLINTFX_CHECK(ring_turns_with_the_outline(c, b));
    std::swap(b.indices[1], b.indices[2]);
    GLINTFX_CHECK(!ring_turns_with_the_outline(c, b));
}
