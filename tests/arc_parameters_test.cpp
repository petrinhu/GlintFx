// SPDX-License-Identifier: AGPL-3.0-or-later
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <numbers>
#include <span>

#include "draw2d/arc_parameters.hpp"
#include "draw2d/quad_vertices.hpp"
#include "harness/check.hpp"
#include "harness/sweep_lcg.hpp"
#include "harness/test_registry.hpp"

// arc_parameters_test.cpp - R2D-SHAPES, fatia S5-A (TODO.md ARC-VIEWPORT-TESSELLATION; decisions
// D-W8-269, D-W8-377, D-W8-378, and the fix1 decisions D-W8-398 to D-W8-404; GODS_LAWS.md L-17,
// L-20, L-40).
//
// The expected numbers are FIXED here, written before the implementation existed, from
// /var/tmp/cto-w8/r20/s5a/contas/modelo_arco2.py, an independent model outside the library (never
// from the program's output). Each closed case was chosen so that no hull comparison is closer than
// 0.015 pixel to its edge (the model prints that slack), so the last bit of cos and sin on each
// system cannot change a count (GODS_LAWS.md L-04).
//
// The two sweeps at the end do not fix counts: they check the contract itself on 400 curves each,
// made by fixed integer generators (the same numbers on every system), and print how many they
// checked: the whole curve, and the half curve of a rounded cap (fatia S5-C2-fix1). Their error
// oracle is GEOMETRIC (fix1, I-6): the largest distance from points of the true curve to the chord,
// in long double, never the sagitta formula the implementation uses to choose a level.
//
// Every call runs in a buffer larger than the room it is given, and the slot right after the room
// must stay untouched (fix1, I-3): a write past the room fails here, not only under a sanitizer.

namespace {

using glintfx::draw2d::arc_extent;
using glintfx::draw2d::arc_parameters;
using glintfx::draw2d::arc_request;
using glintfx::draw2d::k_largest_exact_arc_radius_pixels;
using glintfx::draw2d::k_max_arc_parameters;
using glintfx::draw2d::pixel_box;
using glintfx::draw2d::pixel_ellipse;
using glintfx::draw2d::pixel_position_wide;

constexpr double k_pi = std::numbers::pi;
constexpr double k_nan = std::numeric_limits<double>::quiet_NaN();
constexpr double k_inf = std::numeric_limits<double>::infinity();
constexpr pixel_box k_screen{0.0, 0.0, 1920.0, 1080.0};
constexpr pixel_box k_large_screen{0.0, 0.0, 16384.0, 16384.0};
// A value arc_parameters() never writes: a parameter is 0 or more.
constexpr double k_untouched = -1.0;

using parameters = std::array<double, 2048>;

// A circle (u = (r, 0), v = (0, r)) or an ellipse with the two axes given, whole or half.
arc_request curve(pixel_position_wide center, pixel_position_wide u, pixel_position_wide v,
                  arc_extent extent) {
    return arc_request{pixel_ellipse{center, u, v}, extent, 0.25, 0.0};
}

arc_request circle(double cx, double cy, double r) {
    return curve({cx, cy}, {r, 0.0}, {0.0, r}, arc_extent::whole);
}

// Four units in the last place of `scale`.
bool within_four_ulps(double got, double want, double scale) {
    const double ulp = std::nextafter(scale, k_inf) - scale;
    return std::fabs(got - want) <= 4.0 * ulp;
}

bool strictly_increasing(std::span<const double> t) {
    for (std::size_t k = 1U; k < t.size(); ++k) {
        if (!(t[k] > t[k - 1U])) {
            return false;
        }
    }
    return true;
}

// The call, with the room smaller than the buffer by at least one slot: the slot right after the
// room is checked untouched after the call (fix1, I-3).
std::size_t run(const arc_request &request, const pixel_box &surface, parameters &out,
                std::size_t room) {
    out.fill(k_untouched);
    const std::size_t n = arc_parameters(request, surface, std::span<double>(out).first(room));
    GLINTFX_CHECK(room < out.size());
    GLINTFX_CHECK(room >= out.size() || out[room] == k_untouched);
    return n;
}

} // namespace

GLINTFX_TEST(arc_parameters_uniform_circle_of_radius_ten_is_fifteen_equal_steps) {
    parameters t{};
    const std::size_t n = run(circle(960.0, 540.0, 10.0), k_screen, t, 1025U);
    GLINTFX_CHECK(n == 16U);
    GLINTFX_CHECK(t[0] == 0.0);
    GLINTFX_CHECK(within_four_ulps(t[1], 2.0 * k_pi / 15.0, 2.0 * k_pi));
    GLINTFX_CHECK(within_four_ulps(t[15], 2.0 * k_pi, 2.0 * k_pi));
    GLINTFX_CHECK(strictly_increasing(std::span<const double>(t).first(n)));
    GLINTFX_CHECK(t[16] == k_untouched);
}

GLINTFX_TEST(arc_parameters_uniform_regime_does_not_look_at_the_surface) {
    parameters t{};
    GLINTFX_CHECK(run(circle(-1000.0, -1000.0, 10.0), k_screen, t, 1025U) == 16U);
}

GLINTFX_TEST(arc_parameters_uniform_half_circle_is_eight_steps_up_to_pi) {
    parameters t{};
    const arc_request half = curve({960.0, 540.0}, {10.0, 0.0}, {0.0, 10.0}, arc_extent::half);
    const std::size_t n = run(half, k_screen, t, 257U);
    GLINTFX_CHECK(n == 9U);
    GLINTFX_CHECK(t[0] == 0.0);
    GLINTFX_CHECK(within_four_ulps(t[1], k_pi / 8.0, k_pi));
    GLINTFX_CHECK(within_four_ulps(t[8], k_pi, k_pi));
}

// 512 segments meet 0.25 pixel up to a radius of 13280.41 (D-W8-269): 13280 is uniform, 13281 is
// cut, and the whole curve of that circle centred on the screen lies off it.
GLINTFX_TEST(arc_parameters_regime_changes_between_radius_13280_and_13281) {
    parameters t{};
    GLINTFX_CHECK(run(circle(960.0, 540.0, 13280.0), k_screen, t, 1025U) == 513U);
    GLINTFX_CHECK(run(circle(960.0, 540.0, 13281.0), k_screen, t, 1025U) == 9U);
}

namespace {

struct count_case {
    const char *label = "";
    arc_request request{};
    std::size_t room = 0U;
    std::size_t expected = 0U;
};

arc_request with_error(arc_request r, double error) {
    r.max_error_pixels = error;
    return r;
}

arc_request with_reach(arc_request r, double reach) {
    r.reach_pixels = reach;
    return r;
}

} // namespace

// THE CLOSED CASES OF THE CUT REGIME, each count from the model (D-W8-377). The circle of radius
// 2^40 gives 65 since fatia CURVE-FLOAT-NARROW (47 before): its pieces on the screen are cut on
// until their ends lie within 1024 pixels of it (curvefloat-cto/modelo_caso_2e40.py, the same model
// with the rule, run before the count was changed here).
GLINTFX_TEST(arc_parameters_cut_counts_of_the_closed_cases) {
    const double crossing = 540.0 + 20000.0 - 300.0; // the top of the circle at y = 240
    const std::array<count_case, 12> cases{
        count_case{"circle 20000 around the screen", circle(960.0, 540.0, 20000.0), 1025U, 9U},
        count_case{"circle 20000 crossing the screen", circle(960.0, crossing, 20000.0), 1025U,
                   29U},
        count_case{"half of it, away from the screen",
                   curve({960.0, crossing}, {20000.0, 0.0}, {0.0, 20000.0}, arc_extent::half),
                   1025U, 3U},
        count_case{"half of it, turned onto the screen",
                   curve({960.0, crossing}, {0.0, -20000.0}, {20000.0, 0.0}, arc_extent::half),
                   1025U, 15U},
        count_case{"error 1e-9: the deepest level that fits 1025",
                   with_error(circle(960.0, 540.0 + 1.0e6 - 300.0, 1.0e6), 1.0e-9), 1025U, 679U},
        count_case{"error 1e-9: the deepest level that fits 600",
                   with_error(circle(960.0, 540.0 + 1.0e6 - 300.0, 1.0e6), 1.0e-9), 600U, 357U},
        count_case{"error 1e-9: room of 2000 still holds 1025 at most",
                   with_error(circle(960.0, 540.0 + 1.0e6 - 300.0, 1.0e6), 1.0e-9), 2000U, 679U},
        count_case{"curve 30 pixels off the screen, reach 0",
                   circle(960.0, -30.0 - 20000.0, 20000.0), 1025U, 5U},
        count_case{"curve 30 pixels off the screen, reach 50",
                   with_reach(circle(960.0, -30.0 - 20000.0, 20000.0), 50.0), 1025U, 29U},
        count_case{"curve 0.5 pixel off the screen (inside the margin)",
                   circle(960.0, -0.5 - 20000.0, 20000.0), 1025U, 21U},
        count_case{"curve 1.5 pixels off the screen (beyond the margin)",
                   circle(960.0, -1.5 - 20000.0, 20000.0), 1025U, 5U},
        count_case{"radius 2^40 crossing the screen, the ends kept near it",
                   circle(960.0, 540.0 + k_largest_exact_arc_radius_pixels - 300.0,
                          k_largest_exact_arc_radius_pixels),
                   1025U, 65U},
    };
    int failures = 0;
    parameters t{};
    for (const count_case &c : cases) {
        const std::size_t n = run(c.request, k_screen, t, c.room);
        const bool ends =
            n >= 2U && t[0] == 0.0 && within_four_ulps(t[n - 1U], 2.0 * k_pi, 2.0 * k_pi);
        const bool half_ends = n >= 2U && t[0] == 0.0 && within_four_ulps(t[n - 1U], k_pi, k_pi);
        const bool right_end = c.request.extent == arc_extent::whole ? ends : half_ends;
        if (n != c.expected || !right_end || n > k_max_arc_parameters) {
            ++failures;
            std::printf("[FAIL CASE] %s: n=%zu, expected %zu\n", c.label, n, c.expected);
        }
    }
    std::printf("casos=%zu falhas=%d\n", cases.size(), failures);
    GLINTFX_CHECK(failures == 0);
}

// A thin ellipse, 1e6 by 50, whose end of the long axis lies on the screen.
GLINTFX_TEST(arc_parameters_thin_ellipse_with_its_tip_on_the_screen) {
    parameters t{};
    const arc_request thin =
        curve({960.0 - 1.0e6 + 100.0, 540.0}, {1.0e6, 0.0}, {0.0, 50.0}, arc_extent::whole);
    GLINTFX_CHECK(run(thin, k_screen, t, 1025U) == 139U);
}

// Every parameter of the cut regime is (pi/2) * (N / 2^level): for the crossing circle the deepest
// level is 8, so t * 512 / pi is an integer.
GLINTFX_TEST(arc_parameters_cut_parameters_are_exact_steps_of_the_level) {
    parameters t{};
    const std::size_t n = run(circle(960.0, 540.0 + 20000.0 - 300.0, 20000.0), k_screen, t, 1025U);
    GLINTFX_CHECK(n == 29U);
    int off_grid = 0;
    for (std::size_t k = 0U; k < n; ++k) {
        const double steps = t[k] * 512.0 / k_pi;
        if (std::fabs(steps - std::round(steps)) > 1.0e-9) {
            ++off_grid;
        }
    }
    GLINTFX_CHECK(off_grid == 0);
    GLINTFX_CHECK(strictly_increasing(std::span<const double>(t).first(n)));
}

namespace {

struct refusal_cell {
    const char *label = "";
    arc_request request{};
    pixel_box surface{};
    std::size_t room = 0U;
    bool cut = false; // the curve would be in the cut regime if it were accepted
};

// The two curves the finiteness cells break: one in the uniform regime, one in the cut regime.
arc_request curve_of_regime(bool cut) {
    return cut ? circle(960.0, 540.0 + 20000.0 - 300.0, 20000.0) : circle(960.0, 540.0, 10.0);
}

constexpr std::array<const char *, 12> k_value_names{
    "centre x",      "centre y",      "axis_u x",      "axis_u y",      "axis_v x", "axis_v y",
    "surface min_x", "surface min_y", "surface max_x", "surface max_y", "error",    "reach"};

// One of the 12 values the request and the surface hold, made not finite: NaN in the uniform
// regime, +infinity in the cut regime (fix1, I-1).
refusal_cell with_value_not_finite(std::size_t value, bool cut) {
    refusal_cell c{k_value_names.at(value), curve_of_regime(cut), k_screen, 1025U, cut};
    const std::array<double *, 12> slots{&c.request.ellipse.center.x,
                                         &c.request.ellipse.center.y,
                                         &c.request.ellipse.axis_u.x,
                                         &c.request.ellipse.axis_u.y,
                                         &c.request.ellipse.axis_v.x,
                                         &c.request.ellipse.axis_v.y,
                                         &c.surface.min_x,
                                         &c.surface.min_y,
                                         &c.surface.max_x,
                                         &c.surface.max_y,
                                         &c.request.max_error_pixels,
                                         &c.request.reach_pixels};
    *slots.at(value) = cut ? k_inf : k_nan;
    return c;
}

bool cell_holds(const refusal_cell &c, parameters &t) {
    const std::size_t n = run(c.request, c.surface, t, c.room);
    const bool untouched =
        std::all_of(t.begin(), t.end(), [](double v) { return v == k_untouched; });
    if (n != 0U || !untouched) {
        std::printf("[FAIL CELL] %s%s: n=%zu\n", c.label, c.cut ? " (cut regime)" : "", n);
        return false;
    }
    return true;
}

} // namespace

// What the function cannot cut answers 0 and writes nothing: each of the 12 values not finite in
// each regime (24 cells), and the 7 other refusals.
GLINTFX_TEST(arc_parameters_answers_zero_for_what_it_cannot_cut) {
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) reason: the cast is the case
    const auto unknown_extent = static_cast<arc_extent>(7);
    arc_request unknown = circle(960.0, 540.0, 10.0);
    unknown.extent = unknown_extent;
    const arc_request half = curve({960.0, 540.0}, {10.0, 0.0}, {0.0, 10.0}, arc_extent::half);
    // The largest stretch of this axis is about 2.4e308: past the largest double.
    const arc_request endless =
        curve({960.0, 540.0}, {1.7e308, 1.7e308}, {0.0, 0.0}, arc_extent::whole);
    const std::array<refusal_cell, 7> others{
        refusal_cell{"whole curve, room 512", circle(960.0, 540.0, 10.0), k_screen, 512U},
        refusal_cell{"half curve, room 256", half, k_screen, 256U},
        refusal_cell{"error zero", with_error(circle(960.0, 540.0, 10.0), 0.0), k_screen, 1025U},
        refusal_cell{"error negative", with_error(circle(960.0, 540.0, 10.0), -1.0), k_screen,
                     1025U},
        refusal_cell{"reach negative", with_reach(circle(960.0, 540.0, 10.0), -1.0), k_screen,
                     1025U},
        refusal_cell{"extent out of the enumeration", unknown, k_screen, 1025U},
        refusal_cell{"radius not finite", endless, k_screen, 1025U},
    };
    int checked = 0;
    int failures = 0;
    parameters t{};
    for (std::size_t value = 0U; value < k_value_names.size(); ++value) {
        for (const bool cut : {false, true}) {
            ++checked;
            failures += cell_holds(with_value_not_finite(value, cut), t) ? 0 : 1;
        }
    }
    for (const refusal_cell &c : others) {
        ++checked;
        failures += cell_holds(c, t) ? 0 : 1;
    }
    std::printf("celulas=%d falhas=%d\n", checked, failures);
    GLINTFX_CHECK(checked == 31);
    GLINTFX_CHECK(failures == 0);
}

// A radius that does not exceed the error is the uniform minimum, 4 segments, even at radius 0
// (fix1, mutant M10 of the review).
GLINTFX_TEST(arc_parameters_radius_not_above_the_error_is_four_segments) {
    parameters t{};
    for (const double radius : {0.0, 0.1, 0.25}) {
        const std::size_t n = run(circle(960.0, 540.0, radius), k_screen, t, 1025U);
        GLINTFX_CHECK(n == 5U);
        GLINTFX_CHECK(within_four_ulps(t[1], k_pi / 2.0, 2.0 * k_pi));
    }
}

// When the level the error asks for does not fit the room, the deepest level down to 8 that fits;
// when none fits, level 7 (fix1, I-2). A circle of radius 13281 wholly inside a surface of 30000
// asks for level 8: 1025 parameters in a room of 1025, and level 7, 513 parameters, in a room of
// 600 or of 513. Every parameter of level 7 is a step of pi / 256.
GLINTFX_TEST(arc_parameters_falls_back_to_level_seven_when_nothing_finer_fits) {
    constexpr pixel_box k_wide{0.0, 0.0, 30000.0, 30000.0};
    const arc_request inside = circle(15000.0, 15000.0, 13281.0);
    parameters t{};
    GLINTFX_CHECK(run(inside, k_wide, t, 1025U) == 1025U);
    for (const std::size_t room : {std::size_t{600U}, std::size_t{513U}}) {
        const std::size_t n = run(inside, k_wide, t, room);
        GLINTFX_CHECK(n == 513U);
        int off_grid = 0;
        for (std::size_t k = 0U; k < n; ++k) {
            const double steps = t[k] * 256.0 / k_pi;
            off_grid += std::fabs(steps - std::round(steps)) > 1.0e-9 ? 1 : 0;
        }
        GLINTFX_CHECK(off_grid == 0);
        GLINTFX_CHECK(within_four_ulps(t[n - 1U], 2.0 * k_pi, 2.0 * k_pi));
    }
}

// A finite radius is cut, however large (fix1, I-5): the largest stretch no longer overflows on the
// way. The counts are not fixed (above 2^40 the rounding sets the error, D-W8-378); the ends, the
// order and the room are.
GLINTFX_TEST(arc_parameters_cuts_every_finite_radius) {
    const std::array<arc_request, 3> huge{
        curve({960.0, 540.0}, {1.0e78, 0.0}, {0.0, 1.0}, arc_extent::whole),
        circle(960.0, 540.0, 1.0e154),
        circle(960.0, 540.0, 1.0e300),
    };
    parameters t{};
    for (const arc_request &request : huge) {
        const std::size_t n = run(request, k_screen, t, 1025U);
        GLINTFX_CHECK(n >= 2U);
        GLINTFX_CHECK(n <= k_max_arc_parameters);
        GLINTFX_CHECK(t[0] == 0.0);
        GLINTFX_CHECK(n >= 2U && within_four_ulps(t[n - 1U], 2.0 * k_pi, 2.0 * k_pi));
        GLINTFX_CHECK(strictly_increasing(std::span<const double>(t).first(n)));
    }
}

// THE SWEEP. A fixed integer generator (the same numbers on every system) draws 200 curves on each
// of two surfaces, each crossing the surface at a point drawn on it. For every answer: the ends,
// the order, the room, and the contract piece by piece (a piece whose hull touches the grown
// surface is within the error of the curve).
namespace {

using glintfx::test_support::lcg;

pixel_position_wide point_on(const pixel_ellipse &e, double t) {
    return pixel_position_wide{e.center.x + (e.axis_u.x * std::cos(t)) + (e.axis_v.x * std::sin(t)),
                               e.center.y + (e.axis_u.y * std::cos(t)) +
                                   (e.axis_v.y * std::sin(t))};
}

// The largest distance, in long double, from 15 points of the true curve inside the piece (the
// middle among them) to the line of its chord: the geometric error of the piece (fix1, I-6). The
// centre cancels out, so the offsets from it are used.
double largest_distance_to_chord(const pixel_ellipse &e, double from, double to) {
    const auto offset = [&e](long double t) {
        return std::array<long double, 2>{(static_cast<long double>(e.axis_u.x) * std::cos(t)) +
                                              (static_cast<long double>(e.axis_v.x) * std::sin(t)),
                                          (static_cast<long double>(e.axis_u.y) * std::cos(t)) +
                                              (static_cast<long double>(e.axis_v.y) * std::sin(t))};
    };
    const std::array<long double, 2> a = offset(from);
    const std::array<long double, 2> b = offset(to);
    const long double cx = b[0] - a[0];
    const long double cy = b[1] - a[1];
    const long double length = std::hypot(cx, cy);
    long double largest = 0.0L;
    for (int i = 1; i < 16; ++i) {
        const long double t = static_cast<long double>(from) +
                              ((static_cast<long double>(to) - static_cast<long double>(from)) *
                               static_cast<long double>(i) / 16.0L);
        const std::array<long double, 2> p = offset(t);
        const long double px = p[0] - a[0];
        const long double py = p[1] - a[1];
        const long double d =
            length > 0.0L ? std::fabs((cx * py) - (cy * px)) / length : std::hypot(px, py);
        largest = std::max(largest, d);
    }
    return static_cast<double>(largest);
}

// True when the box of the piece's hull (its two ends and the apex where their tangents meet)
// misses the surface grown by the margin and the reach: the rule of arc_parameters.hpp, under which
// the piece is drawn as its chord and the contract does not look at it.
bool piece_misses(const arc_request &request, const pixel_box &surface, double from, double to) {
    const pixel_ellipse &e = request.ellipse;
    const double stretch = 1.0 / std::cos(0.5 * (to - from));
    const pixel_position_wide a = point_on(e, from);
    const pixel_position_wide b = point_on(e, to);
    const pixel_position_wide m = point_on(e, 0.5 * (from + to));
    const pixel_position_wide apex{e.center.x + ((m.x - e.center.x) * stretch),
                                   e.center.y + ((m.y - e.center.y) * stretch)};
    const double grow = 1.0 + request.reach_pixels;
    const auto [low_x, high_x] = std::minmax({a.x, b.x, apex.x});
    const auto [low_y, high_y] = std::minmax({a.y, b.y, apex.y});
    return high_x < surface.min_x - grow || low_x > surface.max_x + grow ||
           high_y < surface.min_y - grow || low_y > surface.max_y + grow;
}

bool piece_holds(const arc_request &request, const pixel_box &surface, double from, double to) {
    // A thousandth of the error covers the rounding of the double evaluation of curves up to
    // 1.3e12 pixels of radius on a system whose long double is a double.
    return piece_misses(request, surface, from, to) ||
           largest_distance_to_chord(request.ellipse, from, to) <=
               request.max_error_pixels * (1.0 + 1.0e-3);
}

// The pieces of an answer whose hull touches the grown surface: the ones the contract is checked
// on. The floor of a sweep (GODS_LAWS.md L-40): none would prove nothing.
std::size_t pieces_on_surface(const arc_request &request, const pixel_box &surface,
                              std::span<const double> t) {
    std::size_t touching = 0U;
    for (std::size_t k = 1U; k < t.size(); ++k) {
        touching += piece_misses(request, surface, t[k - 1U], t[k]) ? 0U : 1U;
    }
    return touching;
}

// The ends, the order, the room and the contract piece by piece; the last parameter is 2 pi for the
// whole curve and pi for the half.
bool answer_holds(const arc_request &request, const pixel_box &surface, std::span<const double> t) {
    const double end = request.extent == arc_extent::whole ? 2.0 * k_pi : k_pi;
    if (t.size() < 2U || t.size() > k_max_arc_parameters || t[0] != 0.0 ||
        !within_four_ulps(t.back(), end, end) || !strictly_increasing(t)) {
        return false;
    }
    for (std::size_t k = 1U; k < t.size(); ++k) {
        if (!piece_holds(request, surface, t[k - 1U], t[k])) {
            return false;
        }
    }
    return true;
}

arc_request drawn_curve(lcg &g, const pixel_box &surface) {
    const double a = 13281.0 * std::pow(10.0, 8.0 * g.next());
    const double b = a * std::pow(10.0, -4.0 * g.next());
    const double turn = k_pi * g.next();
    const pixel_position_wide u{a * std::cos(turn), a * std::sin(turn)};
    const pixel_position_wide v{-b * std::sin(turn), b * std::cos(turn)};
    const double t0 = 2.0 * k_pi * g.next();
    const double px = surface.max_x * g.next();
    const double py = surface.max_y * g.next();
    const pixel_position_wide center{px - ((u.x * std::cos(t0)) + (v.x * std::sin(t0))),
                                     py - ((u.y * std::cos(t0)) + (v.y * std::sin(t0)))};
    return curve(center, u, v, arc_extent::whole);
}

// A half curve of the second generator (fatia S5-C2-fix1): the half ellipse of a rounded cap. Its
// two axes have lengths a and b, a from 1 to 2^39.5 pixels (both regimes) and b from a down to a
// ten-thousandth of it, in two directions 0.1 pi to 0.9 pi apart: a world line under an uneven
// scale gives two axes that are not at right angles. The larger radius stays within 2^40 (it is at
// most a times the square root of 2). The error is drawn from 1/16 to 8 pixels, and the curve
// crosses the surface at a point drawn on it, at a parameter t0 of the half, from 0 to pi.
arc_request drawn_half_curve(lcg &g, const pixel_box &surface) {
    const double a = std::exp2(39.5 * g.next());
    const double b = a * std::pow(10.0, -4.0 * g.next());
    const double turn = 2.0 * k_pi * g.next();
    const double apart = k_pi * (0.1 + (0.8 * g.next()));
    const pixel_position_wide u{a * std::cos(turn), a * std::sin(turn)};
    const pixel_position_wide v{b * std::cos(turn + apart), b * std::sin(turn + apart)};
    const double error = std::exp2(-4.0 + (7.0 * g.next()));
    const double t0 = k_pi * g.next();
    const double px = surface.max_x * g.next();
    const double py = surface.max_y * g.next();
    const pixel_position_wide center{px - ((u.x * std::cos(t0)) + (v.x * std::sin(t0))),
                                     py - ((u.y * std::cos(t0)) + (v.y * std::sin(t0)))};
    return arc_request{pixel_ellipse{center, u, v}, arc_extent::half, error, 0.0};
}

} // namespace

GLINTFX_TEST(arc_parameters_sweep_meets_the_error_on_the_surface) {
    int checked = 0;
    int failures = 0;
    std::size_t largest = 0U;
    parameters t{};
    lcg g{20261014U};
    for (const pixel_box &surface : {k_screen, k_large_screen}) {
        for (int i = 0; i < 200; ++i) {
            const arc_request request = drawn_curve(g, surface);
            const std::size_t n = run(request, surface, t, 1025U);
            ++checked;
            largest = std::max(largest, n);
            if (!answer_holds(request, surface, std::span<const double>(t).first(n))) {
                ++failures;
                std::printf("[FAIL SWEEP] case %d: n=%zu\n", checked, n);
            }
        }
    }
    std::printf("varredura casos=%d maior=%zu falhas=%d\n", checked, largest, failures);
    GLINTFX_CHECK(checked == 400);
    GLINTFX_CHECK(failures == 0);
}

// THE SWEEP OF THE HALF CURVE (fatia S5-C2-fix1; the S5-C2 review, I1). The rounded cap of a line
// is a half curve (stroke_line_pixels.hpp), and the sweep above draws only whole ones. This one
// draws 200 half curves on each of the two surfaces from a generator of its own (drawn_half_curve):
// the same contract piece by piece, with the end at pi, and the floor: every curve crosses the
// surface, so at least one of its pieces touches it, or the sweep measured nothing (GODS_LAWS.md
// L-40).
namespace {

// What the half sweep counted: the curves checked, the ones that failed, the pieces measured on the
// surface and the largest answer.
struct half_sweep_tally {
    int checked = 0;
    int failures = 0;
    std::size_t measured = 0U;
    std::size_t largest = 0U;
    double largest_axis = 0.0;
};

// One half curve of the generator, cut and checked on `surface`, counted into `tally`.
void check_one_half_curve(lcg &g, const pixel_box &surface, half_sweep_tally &tally) {
    parameters t{};
    const arc_request request = drawn_half_curve(g, surface);
    const std::size_t n = run(request, surface, t, 1025U);
    const std::span<const double> answer = std::span<const double>(t).first(n);
    const std::size_t touching = pieces_on_surface(request, surface, answer);
    ++tally.checked;
    tally.largest = std::max(tally.largest, n);
    tally.largest_axis = std::max(tally.largest_axis,
                                  std::hypot(request.ellipse.axis_u.x, request.ellipse.axis_u.y));
    tally.measured += touching;
    if (touching == 0U || !answer_holds(request, surface, answer)) {
        ++tally.failures;
        std::printf("[FAIL HALF SWEEP] case %d: n=%zu touching=%zu\n", tally.checked, n, touching);
    }
}

} // namespace

GLINTFX_TEST(arc_parameters_sweep_of_the_half_curve_meets_the_error_on_the_surface) {
    half_sweep_tally tally{};
    lcg g{20261025U};
    for (const pixel_box &surface : {k_screen, k_large_screen}) {
        for (int i = 0; i < 200; ++i) {
            check_one_half_curve(g, surface, tally);
        }
    }
    std::printf("varredura da meia curva casos=%d pecas_medidas=%zu maior=%zu maior_eixo=%.6g "
                "falhas=%d\n",
                tally.checked, tally.measured, tally.largest, tally.largest_axis, tally.failures);
    GLINTFX_CHECK(tally.checked == 400);
    GLINTFX_CHECK(tally.failures == 0);
    // The range the public text cites is reached, not only allowed by the generator (the review of
    // fatia S5-C2-fix1, C-2).
    GLINTFX_CHECK(tally.largest_axis >= 0x1p39);
}

// THE ENDS NEAR THE SURFACE (fatia CURVE-FLOAT-NARROW). The ends of the segments become single
// precision last (renderer_2d.hpp, PRECISION), and the rounding of an end grows with its distance
// from the origin. So every piece whose hull touches the surface grown by the margin and the reach
// must END near it: in the CUT regime the cut goes on until both ends lie within 1024 pixels of
// that grown surface; in the UNIFORM regime nothing is cut, and a piece is never longer than 652
// times the error (its chord is at most 2 r sin(pi / 512), with r at most e / (1 - cos(pi / 512))).
// The bound checked is the larger of the two, for every piece of 400 curves of a generator of its
// own: whole and half curves, the two regimes (larger axis from 1 to 2^39.5 pixels), axes up to
// 1e4 apart in length and 0.1 pi to 0.9 pi apart in direction, errors from 1/16 to 8 pixels, a
// reach of 0 on half of them and from 1 to 4096 pixels on the other half, each crossing one of the
// two surfaces at a point drawn on it. The floor (GODS_LAWS.md L-40): every curve has at least one
// piece on the surface.
namespace {

// How far the ends of a piece on the surface may lie from the surface grown by the margin and the
// reach: 1024 pixels, or 652 times the error when that is more.
double near_bound(double error) { return std::max(1024.0, 652.0 * error); }

// How far `p` lies outside `surface` grown by `grow`, on the farther axis; 0 inside.
double outside_by(const pixel_box &surface, double grow, const pixel_position_wide &p) {
    const double dx = std::max({surface.min_x - grow - p.x, p.x - surface.max_x - grow, 0.0});
    const double dy = std::max({surface.min_y - grow - p.y, p.y - surface.max_y - grow, 0.0});
    return std::max(dx, dy);
}

// What the sweep of the ends counted: the curves, the failures, the pieces on the surface and the
// farthest end of one of them, in multiples of its bound.
struct near_tally {
    int checked = 0;
    int failures = 0;
    std::size_t measured = 0U;
    double farthest = 0.0;
    double largest_axis = 0.0;
};

arc_request drawn_near_curve(lcg &g, const pixel_box &surface, int index) {
    const double a = std::exp2(39.5 * g.next());
    const double b = a * std::pow(10.0, -4.0 * g.next());
    const double turn = 2.0 * k_pi * g.next();
    const double apart = k_pi * (0.1 + (0.8 * g.next()));
    const pixel_position_wide u{a * std::cos(turn), a * std::sin(turn)};
    const pixel_position_wide v{b * std::cos(turn + apart), b * std::sin(turn + apart)};
    const double error = std::exp2(-4.0 + (7.0 * g.next()));
    const double reach = index % 4 < 2 ? 0.0 : std::exp2(12.0 * g.next());
    const arc_extent extent = index % 2 == 0 ? arc_extent::whole : arc_extent::half;
    const double t0 = (extent == arc_extent::whole ? 2.0 * k_pi : k_pi) * g.next();
    const double px = surface.max_x * g.next();
    const double py = surface.max_y * g.next();
    const pixel_position_wide center{px - ((u.x * std::cos(t0)) + (v.x * std::sin(t0))),
                                     py - ((u.y * std::cos(t0)) + (v.y * std::sin(t0)))};
    return arc_request{pixel_ellipse{center, u, v}, extent, error, reach};
}

// One curve of the generator, cut on `surface`: each piece whose hull touches the grown surface has
// both ends within the bound, counted into `tally`.
void check_one_near_curve(lcg &g, const pixel_box &surface, int index, near_tally &tally) {
    parameters t{};
    const arc_request request = drawn_near_curve(g, surface, index);
    const std::size_t n = run(request, surface, t, 1025U);
    const double grow = 1.0 + request.reach_pixels;
    const double bound = near_bound(request.max_error_pixels);
    std::size_t touching = 0U;
    double farthest = 0.0;
    for (std::size_t k = 1U; k < n; ++k) {
        if (!piece_misses(request, surface, t[k - 1U], t[k])) {
            ++touching;
            const double a = outside_by(surface, grow, point_on(request.ellipse, t[k - 1U]));
            const double b = outside_by(surface, grow, point_on(request.ellipse, t[k]));
            farthest = std::max({farthest, a / bound, b / bound});
        }
    }
    ++tally.checked;
    tally.measured += touching;
    tally.farthest = std::max(tally.farthest, farthest);
    tally.largest_axis = std::max(tally.largest_axis,
                                  std::hypot(request.ellipse.axis_u.x, request.ellipse.axis_u.y));
    if (n < 2U || touching == 0U || farthest > 1.0) {
        ++tally.failures;
        std::printf("[FAIL NEAR SWEEP] case %d: n=%zu touching=%zu farthest=%.6g\n", index, n,
                    touching, farthest);
    }
}

} // namespace

GLINTFX_TEST(arc_parameters_the_ends_of_every_piece_on_the_surface_stay_near_it_swept) {
    near_tally tally{};
    lcg g{20261028U};
    int index = 0;
    for (const pixel_box &surface : {k_screen, k_large_screen}) {
        for (int i = 0; i < 200; ++i) {
            check_one_near_curve(g, surface, index, tally);
            ++index;
        }
    }
    std::printf("pontas perto casos=%d pecas_medidas=%zu maior_fracao=%.6g maior_eixo=%.6g "
                "falhas=%d\n",
                tally.checked, tally.measured, tally.farthest, tally.largest_axis, tally.failures);
    GLINTFX_CHECK(tally.checked == 400);
    GLINTFX_CHECK(tally.failures == 0);
    GLINTFX_CHECK(tally.largest_axis >= 0x1p39); // the cited range is reached (S5-C2-fix1, C-2)
}
