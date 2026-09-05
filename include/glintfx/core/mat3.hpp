// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>

#include <glintfx/core/transform.hpp>
#include <glintfx/export.hpp>

// core/mat3.hpp - CORE-MATH2D (TODO.md W4, GODS_LAWS.md
// L-19/L-20/L-26): the 3x3 affine matrix that reaches the graphics
// card, and the one function that builds it.
//
// ============================================================
// READ THIS BEFORE YOU "FIX" ANYTHING IN THIS FILE
// The decision of the project leader, 05/09/2026, AskUserQuestion,
// axis 4 of four
// ============================================================
//
//   THE MATRIX IS STORED IN THE ORDER THE GRAPHICS CARD EXPECTS, with
//   NO CONVERSION PER FRAME.
//
//   CONSEQUENCE HE WAS TOLD ABOUT AND ACCEPTED: READ AS CODE, THIS
//   MATRIX LOOKS TRANSPOSED COMPARED TO THE NOTATION IN THE TEXTBOOKS.
//   IT IS NOT WRONG. A textbook writes a matrix row by row; this
//   array holds it COLUMN BY COLUMN. THE NEXT READER WHO "CORRECTS"
//   THIS IS BREAKING WORKING CODE.
//
// THE ORDER, spelled out so nobody has to infer it: the element at row
// `r`, column `c` lives at index `c * 3 + r`. Indices 0,1,2 are the
// FIRST COLUMN; 3,4,5 the SECOND; 6,7,8 the THIRD. The translation
// therefore sits at indices 6 and 7 - contiguous, at the end - which
// is exactly where a reader used to row-major order does NOT expect it.
//
// THE SOURCE FOR THE CONVENTION, checked and not remembered
// (GODS_LAWS.md L-43): the OpenGL reference for glUniformMatrix*fv
// states that when the `transpose` argument is GL_FALSE, "each matrix
// is assumed to be supplied in column major order"; GL_TRUE means row
// major. Storing column-major is therefore what lets the library hand
// the array straight to the driver with transpose = GL_FALSE and no
// rearranging step - which is the "no conversion per frame" half of
// the decision. (Khronos OpenGL reference pages, glUniform /
// glUniformMatrix3fv:
// https://registry.khronos.org/OpenGL-Refpages/gl2.1/xhtml/glUniform.xml
// and
// https://manpages.debian.org/jessie/opengl-4-man-doc/glUniformMatrix3fv.3G.en.html)
//
// WHY SINGLE PRECISION AND NOTHING ELSE: OpenGL 3.3 core, the
// project's graphics target (ESCOPO.md SS6, GODS_LAWS.md L-31), has no
// double-precision matrix uniform at all - that only arrives with
// OpenGL 4.0/4.1, which the leader refused. This type is the SCREEN
// side of the two precision families core/vec2.hpp describes, and the
// narrowing from world precision happens in exactly one place: the
// function below.
//
// WHAT THIS TYPE DELIBERATELY CANNOT DO: multiply. There is no
// matrix-times-matrix, and that is the same correctness argument
// core/transform.hpp's own comment makes about composing transforms -
// combining belongs to the slice that actually needs a scene graph,
// with a type that can hold every result. A public multiply frozen
// here today (GODS_LAWS.md L-26) would also invite composing IN SINGLE
// PRECISION, chaining exactly the rounding the world/screen split
// exists to keep out of the world side.
//
// NO IDENTITY CONSTANT EITHER: `gltfx_mat3_from_transform({.translation
// = {0, 0}, .rotation = {.radians = 0}, .scale = {1, 1}})` already
// spells it, through the one entry point this type has - see
// `the_neutral_transform_produces_the_identity_matrix` in
// tests/math2d_test.cpp, which proves that exact call yields the
// identity. A second spelling of the same value is public surface
// frozen forever for no new capability.

namespace glintfx {

// A 3x3 affine matrix in single precision, stored COLUMN BY COLUMN -
// read the "READ THIS BEFORE YOU FIX ANYTHING" block above before
// touching the order.
//
// Trivial aggregate; the layout IS the contract (GODS_LAWS.md L-26).
// std::array, not a raw array, so the type stays assignable and
// copyable by the Rule of Zero with no special member declared; it
// allocates nothing, so nothing crosses the library boundary that
// docs/api-conventions.md R5 would object to - the storage is nine
// floats and nothing else.
//
// The field is named `column_major` rather than `elements` on purpose:
// the reader who indexes it has the convention in front of them at the
// call site, not only in this comment.
struct gltfx_mat3 {
    std::array<float, 9> column_major;
};

// Build the frame's matrix from a world-space transform.
//
// THIS IS THE ONE PLACE the two precision families meet on the matrix
// path, and it is meant to be called ONCE PER FRAME, not once per
// object (the leader's axis-1 decision). Everything inside is computed
// in DOUBLE precision - the trigonometry, the scaling, the placement
// of the translation - and only the nine finished numbers are narrowed
// to float on the way out. Narrowing earlier would round the world's
// coordinates before they had been reduced to screen-sized values,
// which is the whole defect the two families exist to avoid.
//
// ORDER OF APPLICATION, frozen: scale, then rotation, then translation
// (see core/transform.hpp's own comment for why that order and no
// other).
//
// TOTAL, per the pure-math rule this project already carries
// (core/time.hpp's own header comment, canonical record in
// DECISOES_AUTONOMAS.md entry D8): deterministic, never undefined
// behavior, never fallible, no error channel. A transform holding an
// infinity or a non-number produces a matrix holding those same
// values, propagated - never a crash, and never a fabricated finite
// number pretending the input was valid.
[[nodiscard]] GLINTFX_API gltfx_mat3 gltfx_mat3_from_transform(gltfx_transform t) noexcept;

} // namespace glintfx
