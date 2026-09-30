// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/gl_state_contract.hpp"

namespace glintfx::draw2d {

namespace {

using render::GLenum;
using render::GLint;
using render::GLsizei;

// The values of the vendored registry (third_party/khronos/gl.xml); the generated loader carries
// functions, not constants.
constexpr GLenum k_gl_blend = 0x0BE2;
constexpr GLenum k_gl_cull_face = 0x0B44;
constexpr GLenum k_gl_depth_test = 0x0B71;
constexpr GLenum k_gl_scissor_test = 0x0C11;
constexpr GLenum k_gl_stencil_test = 0x0B90;
constexpr GLenum k_gl_framebuffer_srgb = 0x8DB9;
constexpr GLenum k_gl_one = 1;
constexpr GLenum k_gl_one_minus_src_alpha = 0x0303;
constexpr GLenum k_gl_texture0 = 0x84C0;
constexpr GLenum k_gl_texture_2d = 0x0DE1;
constexpr GLenum k_gl_array_buffer = 0x8892;
constexpr GLenum k_gl_element_array_buffer = 0x8893;
constexpr GLenum k_gl_unpack_alignment = 0x0CF5;
constexpr GLenum k_gl_func_add = 0x8006;
constexpr GLenum k_gl_front_and_back = 0x0408;
constexpr GLenum k_gl_fill = 0x1B02;
constexpr GLenum k_gl_rasterizer_discard = 0x8C89;
constexpr GLenum k_gl_color_logic_op = 0x0BF2;
constexpr GLenum k_gl_sample_alpha_to_coverage = 0x809E;
constexpr GLenum k_gl_draw_framebuffer = 0x8CA9;

// The state both halves agree on: blending as premultiplied color needs it, every test that could
// discard a fragment off, every channel writable, the viewport over the whole surface, sRGB as the
// context option says.
void set_common_state(const render::gl_function_table &gl, std::uint32_t viewport_width,
                      std::uint32_t viewport_height, bool srgb_framebuffer) noexcept {
    gl.glEnable(k_gl_blend);
    gl.glBlendFuncSeparate(k_gl_one, k_gl_one_minus_src_alpha, k_gl_one, k_gl_one_minus_src_alpha);
    // The rest of what the drawing depends on (D-B3c-1, the emenda E3 of the frozen text): the
    // equation (a consumer's additive, MAX or subtract blending would corrupt premultiplied alpha),
    // the polygon mode (a debug wireframe would draw our quads as lines), the tests that discard or
    // replace a fragment (rasterizer discard, logic op, alpha-to-coverage), and the DRAW
    // framebuffer (a consumer's FBO would receive the drawing instead of the surface).
    gl.glBlendEquationSeparate(k_gl_func_add, k_gl_func_add);
    gl.glPolygonMode(k_gl_front_and_back, k_gl_fill);
    gl.glDisable(k_gl_rasterizer_discard);
    gl.glDisable(k_gl_color_logic_op);
    gl.glDisable(k_gl_sample_alpha_to_coverage);
    gl.glBindFramebuffer(k_gl_draw_framebuffer, 0);
    gl.glDisable(k_gl_depth_test);
    gl.glDisable(k_gl_stencil_test);
    gl.glDisable(k_gl_scissor_test);
    gl.glDisable(k_gl_cull_face);
    gl.glColorMask(1, 1, 1, 1);
    gl.glViewport(0, 0, static_cast<GLsizei>(viewport_width),
                  static_cast<GLsizei>(viewport_height));
    if (srgb_framebuffer) {
        gl.glEnable(k_gl_framebuffer_srgb);
    } else {
        gl.glDisable(k_gl_framebuffer_srgb);
    }
}

} // namespace

void set_gl_state_for_drawing(const render::gl_function_table &gl,
                              const draw_state_request &request) noexcept {
    set_common_state(gl, request.viewport_width, request.viewport_height, request.srgb_framebuffer);
    gl.glPixelStorei(k_gl_unpack_alignment, 1);
    gl.glActiveTexture(k_gl_texture0);
    gl.glUseProgram(request.program);
    gl.glBindVertexArray(request.vertex_array);
    gl.glBindBuffer(k_gl_array_buffer, request.vertex_buffer);
    // The element-array binding belongs to the vertex array bound just above.
    gl.glBindBuffer(k_gl_element_array_buffer, request.index_buffer);
}

void leave_gl_state_after_drawing(const render::gl_function_table &gl, std::uint32_t viewport_width,
                                  std::uint32_t viewport_height, bool srgb_framebuffer) noexcept {
    set_common_state(gl, viewport_width, viewport_height, srgb_framebuffer);
    gl.glActiveTexture(k_gl_texture0);
    gl.glBindTexture(k_gl_texture_2d, 0);
    gl.glUseProgram(0);
    gl.glBindBuffer(k_gl_array_buffer, 0);
    // Zeroed while the vertex array is still bound: the element-array binding is state OF the
    // array.
    gl.glBindBuffer(k_gl_element_array_buffer, 0);
    gl.glBindVertexArray(0);
}

} // namespace glintfx::draw2d
