// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

#include "gl_abi.hpp"
#include "gl_functions.hpp"

// draw2d/gl_state_contract.hpp - R2D-BATCH, fatia B3c (docs/plano-w7d.md sec. 4.3, R-B3, D-W7D-11,
// docs/auditoria-api-draw2d.md B0-I5): the CONTRACT of what the drawing layer does to the state of
// GL, as two functions over an already loaded gl_function_table (no context, no operating system).
//
// (b) of D-W7D-11: the layer trusts NO state of GL when it starts to draw - it SETS everything it
// depends on (set_gl_state_for_drawing) - and it declares, as a closed list, the state it leaves
// behind (leave_gl_state_after_drawing). It never saves and restores the consumer's state (a
// glGet per item is a synchronous round trip to the driver).
//
// THE CLOSED LIST of what the layer leaves (the one flush() and finish_frame() promise in
// include/glintfx/draw2d/renderer_2d.hpp, and nothing else is promised):
//   - no program, no vertex array, no buffer and no texture bound (all zero), texture unit 0
//   active;
//   - blending on, as (ONE, ONE_MINUS_SRC_ALPHA) for color and alpha, with the equation ADD for
//   both;
//   - the polygon mode FILL for both faces;
//   - depth test, stencil test, scissor test and face culling off; color mask all on;
//   - rasterizer discard, color logic op and sample-alpha-to-coverage off;
//   - the DRAW framebuffer 0 (the surface itself);
//   - the viewport covering the whole surface;
//   - GL_FRAMEBUFFER_SRGB on exactly when the context option `srgb_framebuffer` is on.
// (The element-array binding is state OF the vertex array, so zeroing it while the array is still
// bound is part of unbinding the array; set_gl_state_for_drawing rebinds it every time.)
namespace glintfx::draw2d {

// What set_gl_state_for_drawing() needs to know.
struct draw_state_request {
    std::uint32_t program = 0;
    std::uint32_t vertex_array = 0;
    std::uint32_t vertex_buffer = 0;
    std::uint32_t index_buffer = 0;
    std::uint32_t viewport_width = 0; // physical pixels of the drawing surface
    std::uint32_t viewport_height = 0;
    bool srgb_framebuffer = false; // the context option `srgb_framebuffer`
};

// Sets every piece of state the drawing depends on, whatever it was: the program, the vertex array
// with its buffers, blending as (ONE, ONE_MINUS_SRC_ALPHA) for color and alpha, depth test, stencil
// test, scissor test and face culling off, color mask all on, the viewport (0, 0, width, height),
// GL_FRAMEBUFFER_SRGB as asked, unpack alignment 1, texture unit 0 active.
void set_gl_state_for_drawing(const render::gl_function_table &gl,
                              const draw_state_request &request) noexcept;

// Leaves GL as the closed list above says.
void leave_gl_state_after_drawing(const render::gl_function_table &gl, std::uint32_t viewport_width,
                                  std::uint32_t viewport_height, bool srgb_framebuffer) noexcept;

} // namespace glintfx::draw2d
