// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/export.hpp>

// core/vec2.hpp - CORE-MATH2D (TODO.md W4, GODS_LAWS.md
// L-19/L-20/L-26): the two-component position/measure value types of
// the core layer, and the ONLY two functions that cross between the
// project's two precision families.
//
// Core-layer value types (GODS_LAWS.md L-19's opacity clause is scoped
// to "handle e subsistema com estado" - these are neither; the STABLE
// LAYOUT IS the contract, the same exception glintfx::version,
// glintfx::gltfx_rgba and glintfx::gltfx_duration already use).
//
// ============================================================
// THE DECISION OF THE PROJECT LEADER THIS FILE FREEZES
// 05/09/2026, via AskUserQuestion (axis 1 of four)
// ============================================================
//
//   DOUBLE PRECISION IN THE WORLD, SINGLE PRECISION ON THE SCREEN.
//
//   Positions and measures of the WORLD use double precision - which
//   is what makes a large map's coordinates keep their exactness past
//   the point where single precision stops holding whole numbers
//   (sixteen million; measured, see below). The matrix handed to the
//   graphics card is born in SINGLE precision and is computed ONCE PER
//   FRAME, never once per object.
//
// THE FACT THAT FORCED IT, and it is not a preference: the project's
// graphics target is OpenGL 3.3 core (ESCOPO.md SS6, GODS_LAWS.md
// L-31), which DOES NOT ACCEPT double precision - neither as a vertex
// attribute nor as a matrix uniform. Double-precision attributes and
// uniforms only exist from OpenGL 4.0/4.1 onward, and 4.x was refused
// by the project leader for cutting off older consumer hardware. So
// narrowing to single precision AT THE GRAPHICS BOUNDARY is imposed by
// the target, not chosen here.
//
// THE COST THE LEADER WAS TOLD ABOUT AND ACCEPTED: there are TWO
// FAMILIES OF TYPE on the public surface. The boundary between them
// has to be OBVIOUS - whoever reads a signature must know which side
// they are on WITHOUT THINKING - because an ambiguous boundary turns
// the decision itself into a defect.
//
// HOW THIS FILE MAKES THE BOUNDARY OBVIOUS - four mechanisms, not one
// (implementer inference on the MECHANISM, GODS_LAWS.md L-27; the
// REQUIREMENT that the boundary be obvious is the leader's):
//
//   1. THE NAME CARRIES A WHOLE WORD, never a single letter.
//      `gltfx_vec2_world` and `gltfx_vec2_screen` - not the classic
//      `vec2d`/`vec2f`, whose entire difference is one character that
//      the eye skips. `world` and `screen` cannot be misread for one
//      another in a diff, in a compiler diagnostic, or at 3 a.m.
//   2. THE NAME SAYS THE ROLE, and the role IS the precision.
//      The leader's decision binds the two: the world side IS the
//      double side, the screen side IS the single side. Naming the
//      role therefore names the precision without the reader having to
//      remember a second mapping.
//   3. THE TWO TYPES DO NOT IMPLICITLY CONVERT INTO EACH OTHER.
//      They are distinct struct types; C++ offers no conversion
//      between them. Handing a world vector to something expecting a
//      screen vector does not compile - proved, not asserted, by
//      `the_two_precision_families_never_convert_into_each_other` in
//      tests/math2d_test.cpp (std::is_convertible_v in both
//      directions, checked at compile time).
//   4. CROSSING IS AN EXPLICIT, NAMED CALL. The two functions below
//      are the only supported way across, and each one says both
//      sides in its own name. There is no quiet narrowing anywhere.
//
// WHAT CROSSING COSTS, measured live in a container on 05/09/2026
// (GODS_LAWS.md L-09), never estimated: single precision holds every
// whole number up to 16'777'216 exactly and stops there - 16'777'217
// converts to 16'777'216, silently. Double precision holds
// 16'777'217 exactly. That is the ONE number this whole two-family
// decision exists for, and it is exercised by
// `world_precision_survives_past_the_screen_precision_limit` and
// `crossing_to_screen_precision_is_deterministic_and_can_lose_exactness`
// in tests/math2d_test.cpp.
//
// FIELD NAMES `x`/`y`: short on purpose, and safe. GODS_LAWS.md L-21
// (docs/api-conventions.md R6) asks every public name to be checked
// against system macros and standard-library names; `x` and `y` are
// DATA MEMBERS, scoped by their own struct, which is the category R6's
// own audit already records as safe (the same reasoning that cleared
// gltfx_rslt's `value`/`error`). They are not free names and cannot be
// reached by a function-like macro, which only pattern-matches
// `identifier(`.

namespace glintfx {

// A position or measure in WORLD space, in double precision.
//
// Trivial aggregate, same shape as glintfx::version /
// glintfx::gltfx_rgba / glintfx::gltfx_duration - no user-declared
// special member to default (Rule of Zero, CONTRACT.md SS2.2), and the
// layout below IS the public contract (GODS_LAWS.md L-26: changing it
// raises A).
struct gltfx_vec2_world {
    double x;
    double y;
};

// A position or measure in SCREEN space, in single precision - the
// side that reaches the graphics card.
struct gltfx_vec2_screen {
    float x;
    float y;
};

// Cross from the world side to the screen side.
//
// TOTAL, per the pure-math rule this project already carries
// (core/time.hpp's own header comment, canonical record in
// DECISOES_AUTONOMAS.md entry D8): deterministic, never undefined
// behavior, never fallible, no error channel. Narrowing a double to a
// float is a well-defined conversion in C++ for every finite value:
// in range it rounds to the nearest representable float (which is
// where exactness can be LOST - see the sixteen-million paragraph
// above); out of range, or for an infinity, it saturates to the
// float infinity ON THE SAME SIDE, which is exactly the "keeps the
// input's direction" clause of that rule; a non-number stays a
// non-number.
//
// NOT A PROJECTION. This function changes PRECISION and nothing else -
// it does not apply a camera, a viewport, or any world-to-screen
// mapping. Naming it `world_to_screen` describes which FAMILY OF TYPE
// it crosses (the leader's axis-1 decision binds the two families to
// those two roles), never a coordinate transform. The transform that
// actually maps world onto screen is core/mat3.hpp's
// gltfx_mat3_from_transform().
[[nodiscard]] GLINTFX_API gltfx_vec2_screen gltfx_vec2_world_to_screen(gltfx_vec2_world v) noexcept;

// Cross from the screen side back to the world side.
//
// TOTAL and, unlike its sibling above, EXACT for every input: every
// float value - finite, infinite or not-a-number - is representable
// as a double with no rounding at all, because double's exponent and
// significand both strictly contain float's. So this direction never
// loses anything, and a round trip world -> screen -> world returns
// exactly what the FIRST conversion produced (never necessarily the
// original world value, which may already have been rounded on the
// way out - proved by
// crossing_to_screen_precision_is_deterministic_and_can_lose_exactness
// in tests/math2d_test.cpp).
[[nodiscard]] GLINTFX_API gltfx_vec2_world gltfx_vec2_screen_to_world(gltfx_vec2_screen v) noexcept;

} // namespace glintfx
