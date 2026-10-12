// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/core/angle.hpp>
#include <glintfx/core/err.hpp>
#include <glintfx/core/light_2d.hpp>
#include <glintfx/core/projected_shadow.hpp>
#include <glintfx/core/quad.hpp>

// core/projected_shadow_validation.hpp - R2D-SPRITE-SHADE, H1 (docs/auditoria-api-sprite-shade.md,
// section 2; decision D-W8-324). Which input of a shadow projection is refused, and by which name.
// One subject, taken out of projected_shadow.cpp, which only computes the corners (GODS_LAWS.md
// L-17: a sentence with no "and"). NOT under include/glintfx/: internal, not frozen ABI. Pure: no
// state, no allocation, no operating system.
//
// Every refusal is a gltfx_err with code invalid_argument and the refused field's name in
// rejected_value(), the names listed in core/projected_shadow.hpp's own REFUSALS block.

namespace glintfx {

// The upper bound of the elevation range, (0, 90] degrees. The geometry uses the same value as
// its exact-90 marker, so the bound and the marker can never drift apart.
[[nodiscard]] gltfx_angle shadow_elevation_upper_bound() noexcept;

// The piece, then the elevation, then the azimuth: the first refused field is the one named.
[[nodiscard]] gltfx_rslt<void>
validate_directional_shadow_input(gltfx_directional_light_2d light,
                                  const gltfx_standing_piece_2d &piece) noexcept;

// The piece, then the light's position, then its height against the piece's top.
[[nodiscard]] gltfx_rslt<void>
validate_point_shadow_input(const gltfx_point_light_2d &light,
                            const gltfx_standing_piece_2d &piece) noexcept;

// The result side: every input was accepted, but a corner that is not a finite number (a grazing
// elevation, or values near the largest double) is refused as "shadow_extent". An accepted shadow
// never holds an infinite or NaN corner.
[[nodiscard]] gltfx_rslt<void> validate_shadow_extent(const gltfx_quad_world &shadow) noexcept;

} // namespace glintfx
