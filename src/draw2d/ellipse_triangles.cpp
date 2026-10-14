// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/ellipse_triangles.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

// draw2d/ellipse_triangles.cpp - R2D-SHAPES, fatia S5-B: the indices of ellipse_triangles.hpp.
namespace glintfx::draw2d {

namespace {

constexpr std::size_t k_largest_vertex_count =
    static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max());

// Twice the signed area of the triangle (a, b, c): positive when it turns counter-clockwise in a
// plane whose y goes up. Measured from a, so a position far from the origin keeps its digits.
double turn_of(const pixel_position_wide &a, const pixel_position_wide &b,
               const pixel_position_wide &c) noexcept {
    return ((b.x - a.x) * (c.y - a.y)) - ((b.y - a.y) * (c.x - a.x));
}

// Twice the signed area of the outer polygon, measured from its first point.
double turn_of_polygon(std::span<const pixel_position_wide> polygon) noexcept {
    double sum = 0.0;
    for (std::size_t k = 1U; k + 1U < polygon.size(); ++k) {
        sum += turn_of(polygon[0], polygon[k], polygon[k + 1U]);
    }
    return sum;
}

// True when `turn` does not go against `sense` (a turn of no area goes with every sense).
bool goes_with(double turn, double sense) noexcept {
    return sense >= 0.0 ? turn >= 0.0 : turn <= 0.0;
}

void write_triangle(std::span<std::uint32_t> out, std::size_t at, std::size_t a, std::size_t b,
                    std::size_t c) noexcept {
    out[at] = static_cast<std::uint32_t>(a);
    out[at + 1U] = static_cast<std::uint32_t>(b);
    out[at + 2U] = static_cast<std::uint32_t>(c);
}

} // namespace

std::size_t write_fan_indices(std::size_t rim_count, std::span<std::uint32_t> out) noexcept {
    if (rim_count == 0U || rim_count >= k_largest_vertex_count || out.size() / 3U < rim_count) {
        return 0U;
    }
    for (std::size_t k = 0U; k < rim_count; ++k) {
        const std::size_t next = k + 1U == rim_count ? 1U : k + 2U;
        write_triangle(out, 3U * k, 0U, k + 1U, next);
    }
    return 3U * rim_count;
}

std::size_t write_ring_indices(std::span<const pixel_position_wide> points,
                               std::span<std::uint32_t> out) noexcept {
    const std::size_t total = points.size();
    if (total % 2U != 0U || total < 6U || total > k_largest_vertex_count ||
        out.size() / 3U < total) {
        return 0U;
    }
    const std::size_t n = total / 2U;
    const double sense = turn_of_polygon(points.first(n));
    for (std::size_t k = 0U; k < n; ++k) {
        const std::size_t outer = k;
        const std::size_t outer_next = k + 1U == n ? 0U : k + 1U;
        const std::size_t inner = n + outer;
        const std::size_t inner_next = n + outer_next;
        const bool first_diagonal =
            goes_with(turn_of(points[outer], points[outer_next], points[inner_next]), sense) &&
            goes_with(turn_of(points[outer], points[inner_next], points[inner]), sense);
        if (first_diagonal) {
            write_triangle(out, 6U * k, outer, outer_next, inner_next);
            write_triangle(out, (6U * k) + 3U, outer, inner_next, inner);
        } else {
            write_triangle(out, 6U * k, outer, outer_next, inner);
            write_triangle(out, (6U * k) + 3U, outer_next, inner_next, inner);
        }
    }
    return 6U * n;
}

} // namespace glintfx::draw2d
