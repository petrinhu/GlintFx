// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/core/vec2.hpp>

// core/rect.hpp - CORE-MATH2D (TODO.md W4, GODS_LAWS.md
// L-19/L-20/L-26): the core layer's rectangle value types, one per
// precision family (see core/vec2.hpp for why there are two).
//
// ============================================================
// THE DECISION OF THE PROJECT LEADER THIS FILE FREEZES
// 05/09/2026, via AskUserQuestion (axis 3 of four)
// ============================================================
//
//   A RECTANGLE IS A CORNER PLUS A SIZE. Not two opposite corners.
//
// THE COST HE WAS TOLD ABOUT: a size component that is negative by
// mistake describes an invalid area, and the corner-plus-size shape
// cannot make that unrepresentable the way two-corners cannot make
// an inverted rectangle unrepresentable either.
//
// ============================================================
// WHAT THIS SLICE DOES ABOUT A NEGATIVE SIZE, AND WHY
// implementer decision, GODS_LAWS.md L-27 - NOT the leader's, and
// reported to the orchestrator as such
// ============================================================
//
// THE DECISION: a negative size component is LET THROUGH by the type,
// and its MEANING is frozen here as EMPTY - an area of nothing. It is
// never an "inverted" rectangle that secretly describes the region on
// the other side of the corner.
//
// THE THREE OPTIONS AND WHY THIS ONE:
//
//   (a) REJECT AT CONSTRUCTION. Impossible without giving up the one
//       thing this slice exists to freeze: GODS_LAWS.md L-19 makes the
//       VISIBLE, STABLE LAYOUT the contract of a core value type, which
//       means an aggregate with public members and no constructor to
//       hook. Rejecting would require a factory function returning
//       gltfx_rslt<>, which changes how every call site is written, for
//       a check that costs a branch in code that runs per object.
//   (b) NORMALIZE (flip the corner, take the absolute size). Rejected
//       because it SILENTLY CHANGES the consumer's data: a rectangle
//       built from a drag gesture that went right-to-left would come
//       back describing a different corner than the one the consumer
//       wrote, and nothing would say so. This project's standing
//       preference is a loud refusal over a quiet correction.
//   (c) LET IT THROUGH WITH A FROZEN MEANING - taken.
//
// WHY (c) IS NOT A ONE-WAY DOOR, which is the question that decides
// whether this had to go to the leader first: NOTHING IN THIS SLICE
// CONSUMES A RECTANGLE. There is no intersection test, no containment
// test, no clipping - so no function here can produce a wrong answer
// from a negative size today. What is frozen is the LAYOUT (which
// option (c) leaves untouched) plus the MEANING future slices must
// honor. A checked factory can still be ADDED later without moving a
// single byte or breaking a single consumer, which raises B, not A
// (GODS_LAWS.md L-26). Had this instead frozen "negative means
// inverted", THAT would have been the one-way door, because consumers
// would build data on it.
//
// THE OBLIGATION THIS PUTS ON THE NEXT SLICE, written here because it
// is the first place anyone implementing MAP-COLLIDE-GRID, R2D-BATCH
// or LAYOUT-BOX-MODEL will look: the FIRST public operation that
// consumes a rectangle must treat any negative size component as an
// empty area - it contains nothing, it intersects nothing, it clips
// everything away - and must prove that with its own test.
//
// THE ARITHMETIC CONSEQUENCE, exercised by
// `a_negative_rect_size_places_the_far_corner_before_the_near_one` in
// tests/math2d_test.cpp: the far corner of a rectangle is, by this
// shape's definition, `corner + size` component by component. With a
// negative size component the far corner lands BEFORE the near corner
// on that axis, which is precisely the shape that has no area - the
// reason "empty" is the honest reading and "inverted" is not.
//
// WHY THERE IS NO far_corner() FUNCTION HERE: it would be a public
// function frozen forever (GODS_LAWS.md L-26) doing one addition the
// consumer can write inline, on a type whose fields are public
// precisely so it can. GODS_LAWS.md L-17's warning about a surface
// that grows by "it is only one more method" cuts against adding it.

namespace glintfx {

// A rectangle in WORLD space (double precision): the corner it starts
// at, plus its size. Trivial aggregate; the layout IS the contract.
struct gltfx_rect_world {
    gltfx_vec2_world corner;
    gltfx_vec2_world size;
};

// A rectangle in SCREEN space (single precision) - same shape, the
// other precision family.
struct gltfx_rect_screen {
    gltfx_vec2_screen corner;
    gltfx_vec2_screen size;
};

} // namespace glintfx
