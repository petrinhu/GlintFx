// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/arc_parameters.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <span>

#include "draw2d/arc_segments.hpp"

// draw2d/arc_parameters.cpp - R2D-SHAPES, fatia S5-A: the two regimes of arc_parameters.hpp, one
// step per function.
namespace glintfx::draw2d {

namespace {

// One quarter turn: the top-level pieces of the cut, and the unit of every cut parameter.
constexpr double k_quarter_turn = std::numbers::pi / 2.0;
// The levels the cut starts at: 8 for the finest level the error asks for, 7 as the last resort.
constexpr std::uint32_t k_lowest_fitting_level = 7U;
constexpr std::uint32_t k_first_cut_level = 8U;
// The depth of the walk is at most quarters - 1 + level + 1 pending pieces, so 48 is enough for a
// level up to k_max_arc_cut_level (44 needed).
constexpr std::size_t k_piece_stack_size = k_max_arc_cut_level + 8U;

// The piece from quarter * (numerator / 2^level) to quarter * ((numerator + 1) / 2^level).
struct arc_piece {
    std::uint64_t numerator;
    std::uint32_t level;
};

using piece_stack = std::array<arc_piece, k_piece_stack_size>;

// What to cut: the ellipse, the surface grown by the margin and the reach, the near box (that one
// grown again by k_arc_near_surface_pixels), the number of top-level pieces (4 for a whole curve, 2
// for a half), the level the error asks for, and whether a piece on the surface is cut on until
// its ends are in the near box.
struct cut_plan {
    pixel_ellipse ellipse;
    pixel_box grown;
    pixel_box near;
    std::uint32_t quarters{0};
    std::uint32_t level{0};
    bool ends_near{true};
};

bool is_finite_position(const pixel_position_wide &p) noexcept {
    return std::isfinite(p.x) && std::isfinite(p.y);
}

bool is_finite_ellipse(const pixel_ellipse &e) noexcept {
    return is_finite_position(e.center) && is_finite_position(e.axis_u) &&
           is_finite_position(e.axis_v);
}

bool is_finite_surface(const pixel_box &s) noexcept {
    return std::isfinite(s.min_x) && std::isfinite(s.min_y) && std::isfinite(s.max_x) &&
           std::isfinite(s.max_y);
}

// Everything the cut reads is checked before any comparison: a NaN makes every test false, and the
// cut would then go down to the deepest level for nothing.
bool request_is_valid(const arc_request &request, const pixel_box &surface) noexcept {
    return is_finite_ellipse(request.ellipse) && is_finite_surface(surface) &&
           std::isfinite(request.max_error_pixels) && request.max_error_pixels > 0.0 &&
           std::isfinite(request.reach_pixels) && request.reach_pixels >= 0.0;
}

// The largest radius on the screen: the largest singular value of the two axes.
double largest_radius(const pixel_ellipse &e) noexcept {
    return screen_radius(1.0,
                         pixel_affine{e.axis_u.x, e.axis_u.y, e.axis_v.x, e.axis_v.y, 0.0, 0.0});
}

// The uniform regime holds while its count stays within the 512 segments, or while the radius
// does not exceed the error (then the count is the minimum).
bool uniform_is_enough(double radius, double error) noexcept {
    if (!(radius > error)) {
        return true;
    }
    return std::numbers::pi / std::acos(1.0 - (error / radius)) <= 512.0;
}

// The n equal steps of the uniform regime: n = arc_segment_count for the whole curve, the half
// count for the half, and t_k = pi * (halves * k) / n. Writes n + 1 parameters and answers n + 1.
std::size_t write_uniform(arc_extent extent, double radius, double error,
                          std::span<double> out) noexcept {
    const bool whole = extent == arc_extent::whole;
    const std::uint32_t steps =
        whole ? arc_segment_count(radius, error) : half_arc_segment_count(radius, error);
    const std::uint32_t halves = whole ? 2U : 1U;
    for (std::uint32_t k = 0U; k <= steps; ++k) {
        out[k] = std::numbers::pi * static_cast<double>(halves * k) / static_cast<double>(steps);
    }
    return static_cast<std::size_t>(steps) + 1U;
}

// The first level from 8 whose pieces, quarter / 2^level, are no wider than the widest chord that
// stays within the error, 2 acos(1 - e / r). The deepest level if none is that narrow.
std::uint32_t level_for(double radius, double error) noexcept {
    const double widest = 2.0 * std::acos(1.0 - (error / radius));
    std::uint32_t level = k_first_cut_level;
    while (level < k_max_arc_cut_level &&
           std::ldexp(k_quarter_turn, -static_cast<int>(level)) > widest) {
        ++level;
    }
    return level;
}

// The parameter of the grid point numerator / 2^level, in radians. ldexp divides by 2^level
// exactly, so every level shares the same grid and there is one rounding, in the product.
double parameter_of(std::uint64_t numerator, std::uint32_t level) noexcept {
    return k_quarter_turn * std::ldexp(static_cast<double>(numerator), -static_cast<int>(level));
}

// axis_u cos t + axis_v sin t: the offset of the arc from its centre at parameter t.
pixel_position_wide offset_at(const pixel_ellipse &e, double t) noexcept {
    return pixel_position_wide{(e.axis_u.x * std::cos(t)) + (e.axis_v.x * std::sin(t)),
                               (e.axis_u.y * std::cos(t)) + (e.axis_v.y * std::sin(t))};
}

pixel_position_wide point_at(const pixel_ellipse &e, double t) noexcept {
    const pixel_position_wide offset = offset_at(e, t);
    return pixel_position_wide{e.center.x + offset.x, e.center.y + offset.y};
}

// Where the tangents at from and to meet: the offset of the middle, stretched by 1 / cos(half the
// angle). The arc lies inside the triangle of its two ends and this apex.
pixel_position_wide apex_of(const pixel_ellipse &e, double from, double to) noexcept {
    const double middle = 0.5 * (from + to);
    const double stretch = 1.0 / std::cos(0.5 * (to - from));
    const pixel_position_wide offset = offset_at(e, middle);
    return pixel_position_wide{e.center.x + (offset.x * stretch),
                               e.center.y + (offset.y * stretch)};
}

// The two ends of a piece and the apex of its hull.
struct piece_points {
    pixel_position_wide from;
    pixel_position_wide to;
    pixel_position_wide apex;
};

piece_points points_of(const cut_plan &plan, const arc_piece &piece) noexcept {
    const double from = parameter_of(piece.numerator, piece.level);
    const double to = parameter_of(piece.numerator + 1U, piece.level);
    return piece_points{point_at(plan.ellipse, from), point_at(plan.ellipse, to),
                        apex_of(plan.ellipse, from, to)};
}

// True when the box of the hull (the two ends and the apex) lies wholly to one side of the grown
// surface. Then the piece is drawn as its chord, and no pixel of the surface changes.
bool hull_misses(const cut_plan &plan, const piece_points &points) noexcept {
    const auto [low_x, high_x] = std::minmax({points.from.x, points.to.x, points.apex.x});
    const auto [low_y, high_y] = std::minmax({points.from.y, points.to.y, points.apex.y});
    return high_x < plan.grown.min_x || low_x > plan.grown.max_x || high_y < plan.grown.min_y ||
           low_y > plan.grown.max_y;
}

bool is_inside(const pixel_box &box, const pixel_position_wide &p) noexcept {
    return p.x >= box.min_x && p.x <= box.max_x && p.y >= box.min_y && p.y <= box.max_y;
}

// A piece on the surface is fine when it is as fine as e needs and, while the plan keeps the ends
// near, both ends of its chord lie in the near box or the level is the deepest: its ends then
// become single precision close to the surface.
bool is_fine(const cut_plan &plan, const arc_piece &piece, const piece_points &points) noexcept {
    if (piece.level < plan.level) {
        return false;
    }
    return !plan.ends_near || piece.level >= k_max_arc_cut_level ||
           (is_inside(plan.near, points.from) && is_inside(plan.near, points.to));
}

void push_piece(piece_stack &stack, std::size_t &depth, arc_piece piece) noexcept {
    stack[depth] = piece;
    ++depth;
}

// The two halves of a piece, one level finer. The upper half is pushed first, so the lower one is
// on top and is walked first, which keeps the walk in the order of t.
void split_piece(const arc_piece &piece, piece_stack &stack, std::size_t &depth) noexcept {
    const std::uint64_t lower = piece.numerator * 2U;
    push_piece(stack, depth, arc_piece{lower + 1U, piece.level + 1U});
    push_piece(stack, depth, arc_piece{lower, piece.level + 1U});
}

// Walks the cut depth first and writes the end of each piece that is not split. Answers the count
// of parameters the cut needs, including t = 0. The count passes out.size() when it does not fit,
// and the walk stops there, so it never goes past the room.
std::size_t cut_parameters(const cut_plan &plan, std::span<double> out) noexcept {
    piece_stack stack{};
    std::size_t depth = 0U;
    for (std::uint32_t q = plan.quarters; q > 0U; --q) {
        push_piece(stack, depth, arc_piece{q - 1U, 0U});
    }
    if (!out.empty()) {
        out[0] = 0.0;
    }
    std::size_t count = 1U;
    while (depth > 0U && count <= out.size()) {
        const arc_piece piece = stack[--depth];
        const piece_points points = points_of(plan, piece);
        if (hull_misses(plan, points) || is_fine(plan, piece, points)) {
            if (count < out.size()) {
                out[count] = parameter_of(piece.numerator + 1U, piece.level);
            }
            ++count;
        } else {
            split_piece(piece, stack, depth);
        }
    }
    return count;
}

// The surface grown by `grow` pixels on every side: the band the curve must stay off.
pixel_box grown_surface(const pixel_box &surface, double grow) noexcept {
    return pixel_box{surface.min_x - grow, surface.min_y - grow, surface.max_x + grow,
                     surface.max_y + grow};
}

// The cut for a curve the uniform regime cannot meet. First the level the error asks for with the
// ends kept near the surface. When that does not fit the room, the cut of before, with no ends
// kept near: the deepest level, from the one the error asks for down to 8, whose count fits the
// room; level 7 when none does, which always fits, because the room holds at least
// fewest_arc_parameters(extent) and level 7 needs at most that many.
std::size_t write_cut(const arc_request &request, double radius, const pixel_box &surface,
                      std::span<double> out) noexcept {
    const double grow = k_arc_surface_margin_pixels + request.reach_pixels;
    cut_plan plan{request.ellipse,
                  grown_surface(surface, grow),
                  grown_surface(surface, grow + k_arc_near_surface_pixels),
                  request.extent == arc_extent::whole ? 4U : 2U,
                  level_for(radius, request.max_error_pixels),
                  true};
    const std::span<double> room = out.first(std::min(out.size(), k_max_arc_parameters));
    const std::size_t near_count = cut_parameters(plan, room);
    if (near_count <= room.size()) {
        return near_count;
    }
    plan.ends_near = false;
    for (std::uint32_t level = plan.level; level >= k_first_cut_level; --level) {
        plan.level = level;
        const std::size_t count = cut_parameters(plan, room);
        if (count <= room.size()) {
            return count;
        }
    }
    plan.level = k_lowest_fitting_level;
    return cut_parameters(plan, room);
}

} // namespace

std::size_t fewest_arc_parameters(arc_extent extent) noexcept {
    switch (extent) {
    case arc_extent::whole:
        return k_fewest_whole_arc_parameters;
    case arc_extent::half:
        return k_fewest_half_arc_parameters;
    }
    return 0U;
}

std::size_t arc_parameters(const arc_request &request, const pixel_box &surface,
                           std::span<double> out) noexcept {
    const std::size_t fewest = fewest_arc_parameters(request.extent);
    if (fewest == 0U || out.size() < fewest || !request_is_valid(request, surface)) {
        return 0U;
    }
    const double radius = largest_radius(request.ellipse);
    if (!std::isfinite(radius)) {
        return 0U;
    }
    if (uniform_is_enough(radius, request.max_error_pixels)) {
        return write_uniform(request.extent, radius, request.max_error_pixels, out);
    }
    return write_cut(request, radius, surface, out);
}

} // namespace glintfx::draw2d
