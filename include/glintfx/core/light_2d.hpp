// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/core/angle.hpp>
#include <glintfx/core/vec2.hpp>

// core/light_2d.hpp - R2D-SPRITE-SHADE (docs/auditoria-api-sprite-shade.md,
// H0; decision D-W8-16 of the W8 plan). The two kinds of light that cast a
// shadow on the ground plane.
//
// TWO TYPES, NOT ONE WITH A KIND FIELD: a field that one kind ignores
// would describe the other kind wrongly (GODS_LAWS.md L-28), and two
// distinct aggregates keep the two projection functions apart at the call
// site.
//
// THE GROUND AND THE HEIGHT: positions are WORLD positions on the ground
// plane, in double precision (core/vec2.hpp). A height is the distance
// above that plane, in the same world units. Screen y grows DOWN, so a
// point higher on the screen has a smaller y.
//
// Proved by: projected_shadow_test (the directional and point cases, and
// the refusal of each field by name).

namespace glintfx {

// Parallel rays, all coming from one direction (a sun far away).
struct gltfx_directional_light_2d {
    // The direction the light COMES FROM, on the ground plane: 0 degrees is
    // from +x, 90 degrees is from +y (the bottom of the screen). The shadow
    // of a piece points the other way.
    gltfx_angle azimuth{};
    // The angle between the rays and the ground. Accepted: more than 0
    // degrees and at most 90 degrees. Zero would make the shadow infinitely
    // long; above 90 the rays would come from the other side. Anything else
    // is refused by name: invalid_argument, rejected_value() "elevation".
    gltfx_angle elevation{};
};

// Rays that spread out from one lamp, at a point.
struct gltfx_point_light_2d {
    // The lamp's position on the ground plane.
    gltfx_vec2_world position{};
    // The lamp's height above the ground. It must be finite and strictly greater than
    // the height of the piece it lights: a lamp at or below the top of a
    // piece casts no shadow of that piece. Refused by name: invalid_argument,
    // rejected_value() "light_height".
    double height = 0.0;
};

} // namespace glintfx
