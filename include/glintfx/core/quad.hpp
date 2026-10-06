// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/core/vec2.hpp>

// core/quad.hpp - R2D-BATCH (docs/auditoria-api-draw2d.md, B0): four
// corners in WORLD space, in double precision, with no rule that the
// sides be parallel to anything. It is the general shape a rectangle
// becomes once it is turned, slanted or tapered, and it lives here, in
// the core layer next to gltfx_rect_world, because it is pure geometry:
// it knows nothing about drawing, GL or the operating system (GODS_
// LAWS.md L-19, "value type of the core").
//
// THE CORNERS ARE NAMED BY ROLE, NOT BY POSITION ON SCREEN. `top_left`
// is the corner where the top-left of an image lands when an image is
// drawn onto this quad (R2D-TEXTURE); after a rotation of half a turn
// it sits at the bottom right of the screen and is still `top_left`.
// Walking top_left -> top_right -> bottom_right -> bottom_left goes
// once around the quad. Either direction of travel is accepted.
//
// Trivial aggregate; the layout IS the contract (GODS_LAWS.md L-19,
// L-26). Nothing here is validated: a quad whose corners are not finite
// numbers is a value like any other, and whoever consumes it decides
// what to do with it.

namespace glintfx {

struct gltfx_quad_world {
    gltfx_vec2_world top_left;
    gltfx_vec2_world top_right;
    gltfx_vec2_world bottom_right;
    gltfx_vec2_world bottom_left;
};

} // namespace glintfx
