// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cmath>

#include <glintfx/core/angle.hpp>
#include <glintfx/core/err.hpp>
#include <glintfx/core/light_2d.hpp>
#include <glintfx/core/projected_shadow.hpp>

#include "core/projected_shadow_validation.hpp"

// projected_shadow.cpp - R2D-SPRITE-SHADE, H1 (docs/auditoria-api-sprite-shade.md, sections 2 and
// 4.1; decisions D-W8-319 to D-W8-326 and D-W8-370). Pure geometry: no state, no allocation, no
// operating system. The refusals by name live in projected_shadow_validation.cpp, their own atom
// (GODS_LAWS.md L-17); this file only computes the corners of an accepted shadow, one step per
// function.

namespace glintfx {
namespace {

using quad_result = gltfx_rslt<gltfx_quad_world>;

// The ground run of a shadow per unit of height: 1 / tan(elevation). The elevation of exactly 90
// degrees is an explicit case that answers zero; the cosine of 90 degrees is not zero in double
// precision, so the code never relies on it.
double ground_run_per_unit_height(gltfx_angle elevation) noexcept {
    if (elevation.radians == shadow_elevation_upper_bound().radians) {
        return 0.0;
    }
    return 1.0 / std::tan(elevation.radians);
}

// The unit direction the light COMES FROM on the ground: azimuth 0 is +x, azimuth 90 is +y.
// The shadow of a point lies the other way, so the shadow subtracts this direction.
gltfx_vec2_world direction_light_comes_from(gltfx_angle azimuth) noexcept {
    return {.x = std::cos(azimuth.radians), .y = std::sin(azimuth.radians)};
}

// The ground point under the top of a base point, for a directional light: the base point moves
// away from the light by height * run, along the reversed azimuth.
gltfx_vec2_world directional_shadow_of(gltfx_vec2_world ground, double height,
                                       gltfx_vec2_world from, double run) noexcept {
    const double reach = height * run;
    return {.x = ground.x - from.x * reach, .y = ground.y - from.y * reach};
}

// The ray from the lamp through the top point (ground, height) meets the ground at
// L + (P - L) * H / (H - h). The factor is 1 at the ground and grows as the top rises.
gltfx_vec2_world point_shadow_of(const gltfx_point_light_2d &light, gltfx_vec2_world ground,
                                 double height) noexcept {
    const double factor = light.height / (light.height - height);
    return {.x = light.position.x + (ground.x - light.position.x) * factor,
            .y = light.position.y + (ground.y - light.position.y) * factor};
}

// The corner roles of gltfx_quad_world: the tops of the base ends are the far side of the shadow;
// the base ends themselves are the ground side (the parecer, IMP-H05).
gltfx_quad_world assemble_quad(gltfx_vec2_world top_start, gltfx_vec2_world top_end,
                               const gltfx_standing_piece_2d &piece) noexcept {
    return {.top_left = top_start,
            .top_right = top_end,
            .bottom_right = piece.base_end,
            .bottom_left = piece.base_start};
}

// An accepted input still has to give four finite corners; the extent check names the refusal.
quad_result finite_or_refused(const gltfx_quad_world &shadow) noexcept {
    if (auto finite = validate_shadow_extent(shadow); finite.has_error()) {
        return quad_result::err(finite.err());
    }
    return quad_result::ok(shadow);
}

} // namespace

// The two public signatures below take their lights and the piece BY VALUE: they are VALUE types,
// and value types go by value in the public API (docs/auditoria-api-draw2d.md B0-I10; the parecer
// of docs/auditoria-api-sprite-shade.md freezes these signatures). The copy is a few doubles, and a
// const reference would make the caller's lifetime part of the contract. cppcheck's passedByValue
// asks for a const reference above two pointers' size, so it is suppressed for these two
// definitions only, between the begin and end markers.
// cppcheck-suppress-begin passedByValue
gltfx_rslt<gltfx_quad_world>
gltfx_project_directional_shadow(gltfx_directional_light_2d light,
                                 gltfx_standing_piece_2d piece) noexcept {
    if (auto valid = validate_directional_shadow_input(light, piece); valid.has_error()) {
        return quad_result::err(valid.err());
    }
    const double run = ground_run_per_unit_height(light.elevation);
    const gltfx_vec2_world from = direction_light_comes_from(light.azimuth);
    return finite_or_refused(
        assemble_quad(directional_shadow_of(piece.base_start, piece.height, from, run),
                      directional_shadow_of(piece.base_end, piece.height, from, run), piece));
}

gltfx_rslt<gltfx_quad_world> gltfx_project_point_shadow(gltfx_point_light_2d light,
                                                        gltfx_standing_piece_2d piece) noexcept {
    if (auto valid = validate_point_shadow_input(light, piece); valid.has_error()) {
        return quad_result::err(valid.err());
    }
    return finite_or_refused(assemble_quad(point_shadow_of(light, piece.base_start, piece.height),
                                           point_shadow_of(light, piece.base_end, piece.height),
                                           piece));
}

// cppcheck-suppress-end passedByValue

} // namespace glintfx
