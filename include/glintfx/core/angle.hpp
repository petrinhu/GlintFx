// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/export.hpp>

// core/angle.hpp - CORE-MATH2D (TODO.md W4, GODS_LAWS.md
// L-19/L-20/L-26): the core layer's angle value type, the one that
// carries its own unit.
//
// ============================================================
// THE DECISION OF THE PROJECT LEADER THIS FILE FREEZES
// 05/09/2026, via AskUserQuestion (axis 2 of four)
// ============================================================
//
//   AN ANGLE IS ITS OWN TYPE, AND IT CARRIES THE UNIT. Passing a bare
//   number where an angle is expected MUST NOT COMPILE. The type must
//   not cost more memory than the number it wraps.
//
// THE REASON HE WAS GIVEN, and it is the whole point: mixing degrees
// with radians is the most common defect of this class of library, and
// it is SILENT - nothing complains, no test necessarily fails, the
// picture just comes out crooked. A distinct type turns that silent
// wrong answer into a compiler error.
//
// HOW THE TWO HALVES OF THE DECISION ARE MET:
//
//   NO IMPLICIT CONVERSION FROM A BARE NUMBER. gltfx_angle is a
//   distinct struct type; C++ defines no conversion from double to it,
//   so `takes_an_angle(1.5)` does not compile. Proved at compile time
//   by `an_angle_never_accepts_a_bare_number` in tests/math2d_test.cpp
//   (std::is_convertible_v in both directions).
//
//   NO MEMORY COST. Exactly one double, nothing else - no virtual, no
//   padding, no tag. Proved by the sizeof/alignof/layout assertions in
//   `every_public_math2d_type_has_the_frozen_layout`.
//
// THE ONE ROUTE THAT DOES BYPASS THE UNIT-NAMING FUNCTION, stated
// honestly instead of being left for someone to discover: because
// gltfx_angle is an aggregate with a public member (which it MUST be -
// GODS_LAWS.md L-19 makes the stable layout of a core value type the
// contract itself), a consumer can still write
// `gltfx_angle{.radians = 1.5}` or `gltfx_angle{1.5}`. That is NOT the
// defect the decision targets: both spellings name the unit at the
// call site - `.radians` literally, and the braces marking a
// deliberate construction of THIS type rather than an accidental
// argument slip. What the decision forbids is a bare `1.5` flowing
// into an angle parameter with nothing written down, and that does not
// compile.
//
// RADIANS IS THE CANONICAL STORAGE, degrees the convenience - and the
// SHAPE of this file follows core/time.hpp's own precedent exactly:
// there, the exact canonical unit (nanoseconds) is read straight off
// the public field and only the convenience unit (seconds) got named
// conversion functions. Here, radians is read straight off `.radians`
// and only DEGREES gets the pair below. Adding
// gltfx_angle_from_radians()/gltfx_angle_to_radians() would be two
// public functions that do nothing a field access does not already do,
// frozen forever (GODS_LAWS.md L-26) - the "e so mais um metodo"
// shape L-17 names as the sound of a monolith growing.
//
// WHY RADIANS AND NOT DEGREES AS THE CANONICAL FORM (implementer
// inference, GODS_LAWS.md L-27 - the leader decided that the type
// carries A unit, not WHICH one is stored): every trigonometric
// function in the C++ standard library takes radians. Storing degrees
// would mean converting on every single use inside the library,
// including the once-per-frame matrix build in core/mat3.hpp.

namespace glintfx {

// An angle, stored as radians.
//
// Trivial aggregate (Rule of Zero, CONTRACT.md SS2.2). The layout below
// IS the public contract (GODS_LAWS.md L-26): exactly one double, so
// the type costs the same as the number it wraps.
struct gltfx_angle {
    double radians;
};

// Build an angle from a measure written in degrees.
//
// TOTAL, per the pure-math rule this project already carries
// (core/time.hpp's own header comment, canonical record in
// DECISOES_AUTONOMAS.md entry D8): deterministic, never undefined
// behavior, never fallible, no error channel. There is no range to
// reject - a negative angle, an angle past a full turn, and zero are
// all legitimate and are all exercised as boundary cases by
// `angle_round_trips_through_degrees_including_zero_negative_and_full_turn`
// in tests/math2d_test.cpp. NO NORMALIZATION into [0, 360) happens
// here, deliberately: 720 degrees and 0 degrees mean different things
// to an animation that spins twice, and silently collapsing them would
// destroy information the consumer wrote down on purpose.
[[nodiscard]] GLINTFX_API gltfx_angle gltfx_angle_from_degrees(double degrees) noexcept;

// Read an angle back as degrees. Exact inverse of the function above
// for every finite input within the round-trip tolerance the test
// above states and checks.
[[nodiscard]] GLINTFX_API double gltfx_angle_to_degrees(gltfx_angle a) noexcept;

} // namespace glintfx
