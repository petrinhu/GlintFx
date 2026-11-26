// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <string_view>

#include "gl_abi.hpp"
#include "gl_functions.hpp"

// draw2d/surface_stencil.hpp - R2D-SPRITE-SHADE H3b-2 (D-W8-1328,
// docs/auditoria-api-sprite-shade.md sec. 9): whether the surface of the context has stencil bits,
// asked of the graphics card, never read from what the context asked for. Without bits every
// stencil test passes and nothing is marked (GL 3.3 core, sec. 4.1.4), so two shadows would darken
// a pixel twice with no GL error. Over an already loaded gl_function_table: no context, no
// operating system.
//
// THE THREE ANSWERS OF THE CARD, in the order they are asked:
//   - the attachment type of GL_STENCIL is GL_NONE: no bits, and the size is NOT asked (Mesa
//   answers
//     that question with GL_INVALID_OPERATION; the GL 3.3 core text, sec. 6.1.12, says all other
//     queries do);
//   - a size: bits when it is above zero;
//   - a GL error on either question (Intel HD 3000, SDL issue 12070: the stencil was there): the
//     reading says nothing about the bits. The answer is "present", because both systems ask for
//     at least 8 and refusing shadows on a surface that has them is the worse mistake. INFERENCE,
//     stated in the public text of fill_shadow(). It is never silent: the event below goes to the
//     log sink, and the fixture of the shadows reproves it on a driver that reads well, so a wrong
//     constant here cannot hide behind "present" (D-W8-1546).
//
// GL_STENCIL names an attachment of the DEFAULT framebuffer only: with a framebuffer of the
// consumer bound it gives GL_INVALID_ENUM (measured on llvmpipe), which would read as "present" on
// a surface with no bits. So the default framebuffer is bound to GL_DRAW_FRAMEBUFFER for the two
// questions and the consumer's binding is put back. No GL error is left in the queue.
namespace glintfx::draw2d {

// The event of the reading that failed, and its one field, the GL error (docs/api-conventions.md
// R9).
inline constexpr std::string_view k_stencil_bits_unread_event = "draw2d_stencil_bits_unread";
inline constexpr std::string_view k_gl_error_field_of_stencil = "gl_error";

[[nodiscard]] bool surface_has_stencil(const render::gl_function_table &gl) noexcept;

} // namespace glintfx::draw2d
