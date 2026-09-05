// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/core/angle.hpp>
#include <glintfx/core/vec2.hpp>

// core/transform.hpp - CORE-MATH2D (TODO.md W4, GODS_LAWS.md
// L-19/L-20/L-26): the core layer's description of where something
// sits, how it is turned and how big it is - authored in WORLD space,
// and therefore in double precision.
//
// WHY THIS TYPE IS SEPARATE FROM THE MATRIX (core/mat3.hpp), and it is
// the leader's axis-1 decision made structural rather than an
// implementer's taste: the matrix that reaches the graphics card is
// SINGLE precision and is built ONCE PER FRAME. A transform is what a
// consumer AUTHORS and stores - one per object, in world precision,
// where a large map's coordinates stay exact. The narrowing happens in
// exactly one place, gltfx_mat3_from_transform(), at the moment the
// frame's matrix is built. Keeping the two types apart is what makes
// "once per frame, never once per object" readable in the type system
// instead of being a rule someone has to remember.
//
// THE ORDER THE THREE PARTS APPLY IN, frozen here and proved by
// `mat3_from_transform_applies_scale_then_rotation_then_translation`
// in tests/math2d_test.cpp: SCALE first, then ROTATION, then
// TRANSLATION. This is the conventional order for 2D scene transforms
// and the only one in which the three fields mean what their names
// suggest - the object is sized, then turned about its own origin,
// then moved. Any other order makes `translation` mean "a move that
// gets scaled and rotated afterwards", which is not what the word says.
//
// WHAT IS DELIBERATELY ABSENT: A WAY TO COMBINE TWO TRANSFORMS.
// Implementer decision (GODS_LAWS.md L-27), and it is a correctness
// argument, not a scope argument: this shape (translation, rotation,
// scale) IS NOT CLOSED UNDER COMPOSITION. Composing a rotation with a
// NON-UNIFORM scale produces a shear, and a shear cannot be written
// down in these three fields at all. A `gltfx_transform_compose()`
// frozen into the public surface today would therefore have to either
// silently discard the shear (a wrong answer with no complaint - the
// exact class of defect this project refuses) or return an error from
// a pure math function (which the house rule forbids). Scene-graph
// composition belongs to whichever later slice needs it, in a type
// that can actually hold the result - a full matrix - and that slice
// is free to add it without touching a byte of this layout.

namespace glintfx {

// Where something sits in the world, how it is turned, and how big it
// is. Authored and stored in world (double) precision.
//
// Trivial aggregate; the layout IS the contract (GODS_LAWS.md L-26).
// A default-looking transform is `{.translation = {0, 0},
// .rotation = {.radians = 0}, .scale = {1, 1}}` - note that a
// value-initialized gltfx_transform has a scale of ZERO, not one,
// which is the usual trap of aggregates and is why no "identity"
// spelling is implied by the layout alone.
struct gltfx_transform {
    gltfx_vec2_world translation;
    gltfx_angle rotation;
    gltfx_vec2_world scale;
};

} // namespace glintfx
