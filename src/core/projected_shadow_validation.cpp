// SPDX-License-Identifier: AGPL-3.0-or-later
#include "core/projected_shadow_validation.hpp"

#include <cmath>
#include <string_view>

#include <glintfx/core/err_code.hpp>

namespace glintfx {
namespace {

using check_result = gltfx_rslt<void>;

// A refusal names the field that was refused, in rejected_value(), with invalid_argument.
check_result refused(std::string_view field) noexcept {
    return check_result::err(
        gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value(field));
}

bool is_finite_point(gltfx_vec2_world point) noexcept {
    return std::isfinite(point.x) && std::isfinite(point.y);
}

check_result check_light_height(double light_height, double piece_height) noexcept {
    const bool above = std::isfinite(light_height) && light_height > piece_height;
    return above ? check_result::ok() : refused("light_height");
}

// A piece: finite base, finite non-negative height. A zero-length base and a zero height are
// accepted; they degenerate the shadow but refuse nothing.
check_result check_piece(const gltfx_standing_piece_2d &piece) noexcept {
    if (!is_finite_point(piece.base_start) || !is_finite_point(piece.base_end)) {
        return refused("position");
    }
    const bool height_ok = std::isfinite(piece.height) && piece.height >= 0.0;
    return height_ok ? check_result::ok() : refused("piece_height");
}

check_result check_elevation(gltfx_angle elevation) noexcept {
    const bool in_range =
        elevation.radians > 0.0 && elevation.radians <= shadow_elevation_upper_bound().radians;
    return in_range ? check_result::ok() : refused("elevation");
}

check_result check_azimuth(gltfx_angle azimuth) noexcept {
    return std::isfinite(azimuth.radians) ? check_result::ok() : refused("azimuth");
}

} // namespace

// From the same conversion a caller uses, never from a pi/2 literal, so 90 degrees compares equal
// to itself.
gltfx_angle shadow_elevation_upper_bound() noexcept { return gltfx_angle_from_degrees(90.0); }

gltfx_rslt<void> validate_directional_shadow_input(gltfx_directional_light_2d light,
                                                   const gltfx_standing_piece_2d &piece) noexcept {
    if (auto piece_ok = check_piece(piece); piece_ok.has_error()) {
        return piece_ok;
    }
    if (auto elevation_ok = check_elevation(light.elevation); elevation_ok.has_error()) {
        return elevation_ok;
    }
    return check_azimuth(light.azimuth);
}

gltfx_rslt<void> validate_point_shadow_input(const gltfx_point_light_2d &light,
                                             const gltfx_standing_piece_2d &piece) noexcept {
    if (auto piece_ok = check_piece(piece); piece_ok.has_error()) {
        return piece_ok;
    }
    if (!is_finite_point(light.position)) {
        return refused("position");
    }
    return check_light_height(light.height, piece.height);
}

// Only the two tops can overflow: the projection computed them. The two bottom corners are the
// piece's own base, already refused above when not finite.
gltfx_rslt<void> validate_shadow_extent(const gltfx_quad_world &shadow) noexcept {
    const bool finite = is_finite_point(shadow.top_left) && is_finite_point(shadow.top_right);
    return finite ? check_result::ok() : refused("shadow_extent");
}

} // namespace glintfx
