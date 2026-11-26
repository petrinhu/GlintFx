// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/gl_stencil_contract.hpp"

namespace glintfx::draw2d {

namespace {

using render::GLenum;

// The values of the vendored registry (third_party/khronos/gl.xml); the generated loader carries
// functions, not constants.
constexpr GLenum k_gl_stencil_test = 0x0B90;
constexpr GLenum k_gl_always = 0x0207;
constexpr GLenum k_gl_equal = 0x0202;
constexpr GLenum k_gl_keep = 0x1E00;
constexpr GLenum k_gl_incr = 0x1E02;
constexpr GLenum k_gl_zero = 0;
constexpr render::GLbitfield k_gl_stencil_buffer_bit = 0x00000400;
constexpr render::GLuint k_every_stencil_bit = 0xFFFFFFFFU;

// What one pass of shadow_group.hpp asks of GL; plain leaves the stencil test off.
struct stencil_pass_state {
    bool test = false;
    GLenum function = k_gl_always;
    GLenum on_pass = k_gl_keep;
    render::GLboolean color = 1;
};

[[nodiscard]] stencil_pass_state state_of(stencil_pass pass) noexcept {
    if (pass == stencil_pass::mark_shadow) {
        return stencil_pass_state{true, k_gl_equal, k_gl_incr, 1};
    }
    if (pass == stencil_pass::release_marks) {
        return stencil_pass_state{true, k_gl_always, k_gl_zero, 0};
    }
    return stencil_pass_state{};
}

} // namespace

void leave_stencil_at_gl_defaults(const render::gl_function_table &gl) noexcept {
    gl.glStencilFunc(k_gl_always, 0, k_every_stencil_bit);
    gl.glStencilOp(k_gl_keep, k_gl_keep, k_gl_keep);
    gl.glStencilMask(k_every_stencil_bit);
}

void set_stencil_pass(const render::gl_function_table &gl, stencil_pass pass) noexcept {
    const stencil_pass_state state = state_of(pass);
    gl.glColorMask(state.color, state.color, state.color, state.color);
    if (!state.test) {
        gl.glDisable(k_gl_stencil_test);
        return;
    }
    gl.glEnable(k_gl_stencil_test);
    gl.glStencilMask(k_every_stencil_bit);
    gl.glStencilFunc(state.function, 0, k_every_stencil_bit);
    gl.glStencilOp(k_gl_keep, k_gl_keep, state.on_pass);
}

void clear_stencil(const render::gl_function_table &gl) noexcept {
    gl.glStencilMask(k_every_stencil_bit); // first: the clear obeys the write mask
    gl.glClearStencil(0);
    gl.glClear(k_gl_stencil_buffer_bit);
}

} // namespace glintfx::draw2d
