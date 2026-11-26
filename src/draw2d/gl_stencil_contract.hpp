// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "draw2d/shadow_group.hpp"
#include "gl_abi.hpp"
#include "gl_functions.hpp"

// draw2d/gl_stencil_contract.hpp - R2D-SPRITE-SHADE H3a-fix1 (the N1 review of H3a, I-2;
// GODS_LAWS.md L-17): what the drawing layer sets of the STENCIL in GL, over an already loaded
// gl_function_table (no context, no operating system): the state of each pass of shadow_group.hpp,
// the clear before the first shadow of a segment, and the values GL starts with. Taken out of
// gl_state_contract.hpp, which keeps the closed list of what a barrier leaves; that list calls
// leave_stencil_at_gl_defaults() below, so the stencil part of it lives here, once.
namespace glintfx::draw2d {

// Leaves the stencil at the values GL starts with: the function ALWAYS with reference 0 and every
// bit, the operation KEEP for every outcome, every bit writable. What a consumer that turns the
// test on after us finds (gl_state_contract.hpp calls it in both halves).
void leave_stencil_at_gl_defaults(const render::gl_function_table &gl) noexcept;

// Sets what one pass of shadow_group.hpp needs of the stencil and of the color mask, in full every
// time, never trusting what the pass before left:
//   - plain: stencil test off, every color channel written;
//   - mark_shadow: stencil test on, passing where the stencil is 0 (EQUAL, reference 0, every bit),
//     adding 1 where it passes (KEEP, KEEP, INCR), every bit writable, every color channel written;
//   - release_marks: stencil test on, always passing (ALWAYS, reference 0, every bit), writing 0
//     where it passes (KEEP, KEEP, ZERO), every bit writable, NO color channel written.
// Which fragments a pass discards is embedded_program.hpp's (u_stencil_discard).
void set_stencil_pass(const render::gl_function_table &gl, stencil_pass pass) noexcept;

// Clears the stencil to 0. A clear obeys the stencil write mask, so every bit is made writable
// FIRST. A clear also obeys the scissor test: the caller, the drawing of the shadows (H3b), is to
// call it after set_gl_state_for_drawing() has turned the scissor off.
void clear_stencil(const render::gl_function_table &gl) noexcept;

} // namespace glintfx::draw2d
