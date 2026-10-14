// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/ellipse_outline.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <span>

#include "draw2d/arc_segments.hpp"

// draw2d/ellipse_outline.cpp - R2D-SHAPES, fatia S5-B: the band of ellipse_outline.hpp.
namespace glintfx::draw2d {

std::size_t uniform_normal_angle_count(double reach_pixels, double max_error_pixels) noexcept {
    // arc_segment_count(r, e) is the fewest n with r (1 - cos(pi / n)) <= e: twice as many steps
    // keep r (1 - cos(2 pi / (2 n))) within e for the FULL step, rounded up to a multiple of 4.
    const std::size_t half = arc_segment_count(reach_pixels, 0.5 * max_error_pixels);
    return 4U * ((half + 1U) / 2U);
}

std::size_t write_outline_angles(const conjugate_ellipse &plane, const principal_ellipse &principal,
                                 const outline_angle_sources &sources,
                                 std::span<double> out) noexcept {
    const std::size_t from_arc = sources.parameters.size();
    if (out.size() < from_arc || out.size() - from_arc < sources.uniform_count) {
        return 0U;
    }
    for (std::size_t k = 0U; k < from_arc; ++k) {
        out[k] =
            normal_angle_of_offset(principal, offset_at_parameter(plane, sources.parameters[k]));
    }
    const double step = 2.0 * std::numbers::pi / static_cast<double>(sources.uniform_count);
    for (std::size_t j = 0U; j < sources.uniform_count; ++j) {
        out[from_arc + j] = step * static_cast<double>(j);
    }
    const auto written = out.first(from_arc + sources.uniform_count);
    std::sort(written.begin(), written.end());
    const auto last = std::unique(written.begin(), written.end());
    return static_cast<std::size_t>(last - written.begin());
}

outline_shape outline_shape_of(const principal_ellipse &principal, double half_thickness) noexcept {
    const double a = principal.major;
    const double b = principal.minor;
    if (!(half_thickness < b)) {
        return outline_shape{};
    }
    // b > h >= 0, so a >= b > 0. In units of a: ratio = b / a, reach = h / a < ratio <= 1.
    const double ratio = b / a;
    const double reach = half_thickness / a;
    if (!(reach > ratio * ratio)) {
        return outline_shape{.hole = outline_hole::plain};
    }
    const double reach_over_ratio = reach / ratio;
    const double sine_squared = std::clamp(
        ((reach_over_ratio * reach_over_ratio) - (ratio * ratio)) / (1.0 - (ratio * ratio)), 0.0,
        1.0);
    return outline_shape{.hole = outline_hole::cut,
                         .cut_x = a * std::sqrt(1.0 - sine_squared) * (1.0 - (ratio * ratio))};
}

outline_pair outline_pair_at(const principal_ellipse &principal, const outline_band &band,
                             double phi) noexcept {
    const plane_point on_curve = local_point_at_normal_angle(principal, phi);
    const double nx = std::cos(phi);
    const double ny = std::sin(phi);
    const double h = band.half_thickness;
    const plane_point outer{on_curve.x + (h * nx), on_curve.y + (h * ny)};
    plane_point inner{on_curve.x - (h * nx), on_curve.y - (h * ny)};
    if (band.shape.hole == outline_hole::cut && !(inner.y * ny > 0.0)) {
        inner = plane_point{nx >= 0.0 ? band.shape.cut_x : -band.shape.cut_x, 0.0};
    }
    return outline_pair{plane_point_of_local(principal, outer),
                        plane_point_of_local(principal, inner)};
}

} // namespace glintfx::draw2d
