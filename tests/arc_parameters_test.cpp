// SPDX-License-Identifier: AGPL-3.0-or-later
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <numbers>
#include <span>

#include "draw2d/arc_parameters.hpp"
#include "draw2d/quad_vertices.hpp"
#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// arc_parameters_test.cpp - R2D-SHAPES, fatia S5-A (TODO.md ARC-VIEWPORT-TESSELLATION; decisions
// D-W8-269, D-W8-377 and D-W8-378; GODS_LAWS.md L-17, L-20, L-40).
//
// The expected numbers are FIXED here, written before the implementation existed, from
// /var/tmp/cto-w8/r20/s5a/contas/modelo_arco2.py, an independent model outside the library (never
// from the program's output). Each closed case was chosen so that no hull comparison is closer than
// 0.015 pixel to its edge (the model prints that slack), so the last bit of cos and sin on each
// system cannot change a count (GODS_LAWS.md L-04).
//
// The sweep at the end does not fix counts: it checks the contract itself on 400 curves made by a
// fixed integer generator (the same numbers on every system), and prints how many it checked.

namespace {

using glintfx::draw2d::arc_extent;
using glintfx::draw2d::arc_parameters;
using glintfx::draw2d::arc_request;
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

std::size_t run(const arc_request &request, const pixel_box &surface, parameters &out,
                std::size_t room) {
    out.fill(k_untouched);
    return arc_parameters(request, surface, std::span<double>(out).first(room));
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

// THE CLOSED CASES OF THE CUT REGIME, each count from the model (D-W8-377).
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
        count_case{"error 1e-9: room of 2048 still holds 1025 at most",
                   with_error(circle(960.0, 540.0 + 1.0e6 - 300.0, 1.0e6), 1.0e-9), 2048U, 679U},
        count_case{"curve 30 pixels off the screen, reach 0",
                   circle(960.0, -30.0 - 20000.0, 20000.0), 1025U, 5U},
        count_case{"curve 30 pixels off the screen, reach 50",
                   with_reach(circle(960.0, -30.0 - 20000.0, 20000.0), 50.0), 1025U, 29U},
        count_case{"curve 0.5 pixel off the screen (inside the margin)",
                   circle(960.0, -0.5 - 20000.0, 20000.0), 1025U, 21U},
        count_case{"curve 1.5 pixels off the screen (beyond the margin)",
                   circle(960.0, -1.5 - 20000.0, 20000.0), 1025U, 5U},
        count_case{"radius 2^40 crossing the screen",
                   circle(960.0, 540.0 + 1099511627776.0 - 300.0, 1099511627776.0), 1025U, 47U},
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
};

arc_request with_center_x(double x) {
    arc_request r = circle(960.0, 540.0, 20000.0);
    r.ellipse.center.x = x;
    return r;
}

} // namespace

// What the function cannot cut answers 0 and writes nothing.
GLINTFX_TEST(arc_parameters_answers_zero_for_what_it_cannot_cut) {
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) reason: the cast is the case
    const auto unknown_extent = static_cast<arc_extent>(7);
    arc_request unknown = circle(960.0, 540.0, 10.0);
    unknown.extent = unknown_extent;
    arc_request infinite_axis = circle(960.0, 540.0, 10.0);
    infinite_axis.ellipse.axis_u.x = k_inf;
    const arc_request half = curve({960.0, 540.0}, {10.0, 0.0}, {0.0, 10.0}, arc_extent::half);
    const std::array<refusal_cell, 13> cells{
        refusal_cell{"whole curve, room 512", circle(960.0, 540.0, 10.0), k_screen, 512U},
        refusal_cell{"half curve, room 256", half, k_screen, 256U},
        refusal_cell{"centre x NaN", with_center_x(k_nan), k_screen, 1025U},
        refusal_cell{"axis infinite", infinite_axis, k_screen, 1025U},
        refusal_cell{"error zero", with_error(circle(960.0, 540.0, 10.0), 0.0), k_screen, 1025U},
        refusal_cell{"error negative", with_error(circle(960.0, 540.0, 10.0), -1.0), k_screen,
                     1025U},
        refusal_cell{"error NaN", with_error(circle(960.0, 540.0, 10.0), k_nan), k_screen, 1025U},
        refusal_cell{"error infinite", with_error(circle(960.0, 540.0, 10.0), k_inf), k_screen,
                     1025U},
        refusal_cell{"reach negative", with_reach(circle(960.0, 540.0, 10.0), -1.0), k_screen,
                     1025U},
        refusal_cell{"reach NaN", with_reach(circle(960.0, 540.0, 10.0), k_nan), k_screen, 1025U},
        refusal_cell{"surface not finite", circle(960.0, 540.0, 10.0),
                     pixel_box{0.0, 0.0, k_inf, 1080.0}, 1025U},
        refusal_cell{"extent out of the enumeration", unknown, k_screen, 1025U},
        refusal_cell{"radius too large for a double", circle(960.0, 540.0, 1.0e200), k_screen,
                     1025U},
    };
    int failures = 0;
    parameters t{};
    for (const refusal_cell &c : cells) {
        const std::size_t n = run(c.request, c.surface, t, c.room);
        const bool untouched =
            std::all_of(t.begin(), t.end(), [](double v) { return v == k_untouched; });
        if (n != 0U || !untouched) {
            ++failures;
            std::printf("[FAIL CELL] %s: n=%zu\n", c.label, n);
        }
    }
    std::printf("celulas=%zu falhas=%d\n", cells.size(), failures);
    GLINTFX_CHECK(failures == 0);
}

// THE SWEEP. A fixed integer generator (the same numbers on every system) draws 200 curves on each
// of two surfaces, each crossing the surface at a point drawn on it. For every answer: the ends,
// the order, the room, and the contract piece by piece (a piece whose hull touches the grown
// surface is within the error of the curve).
namespace {

struct lcg {
    std::uint64_t state = 20261014U;
    double next() {
        state = (state * 6364136223846793005ULL) + 1442695040888963407ULL;
        return static_cast<double>(state >> 11U) * 0x1.0p-53;
    }
};

pixel_position_wide point_on(const pixel_ellipse &e, double t) {
    return pixel_position_wide{e.center.x + (e.axis_u.x * std::cos(t)) + (e.axis_v.x * std::sin(t)),
                               e.center.y + (e.axis_u.y * std::cos(t)) +
                                   (e.axis_v.y * std::sin(t))};
}

// The largest singular value of the two axes, written here from the 2x2 formula.
double largest_radius_of(const pixel_ellipse &e) {
    const double a = e.axis_u.x;
    const double b = e.axis_v.x;
    const double c = e.axis_u.y;
    const double d = e.axis_v.y;
    const double squares = (a * a) + (b * b) + (c * c) + (d * d);
    const double det = (a * d) - (b * c);
    const double spread = std::sqrt(std::max(0.0, (squares * squares) - (4.0 * det * det)));
    return std::sqrt((squares + spread) / 2.0);
}

bool piece_holds(const arc_request &request, const pixel_box &surface, double from, double to) {
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
    const bool misses = high_x < surface.min_x - grow || low_x > surface.max_x + grow ||
                        high_y < surface.min_y - grow || low_y > surface.max_y + grow;
    const double sagitta = largest_radius_of(e) * (1.0 - std::cos(0.5 * (to - from)));
    return misses || sagitta <= request.max_error_pixels + 1.0e-9;
}

bool answer_holds(const arc_request &request, const pixel_box &surface, std::span<const double> t) {
    if (t.size() < 2U || t.size() > k_max_arc_parameters || t[0] != 0.0 ||
        !within_four_ulps(t.back(), 2.0 * k_pi, 2.0 * k_pi) || !strictly_increasing(t)) {
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

} // namespace

GLINTFX_TEST(arc_parameters_sweep_meets_the_error_on_the_surface) {
    int checked = 0;
    int failures = 0;
    std::size_t largest = 0U;
    parameters t{};
    lcg g;
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
