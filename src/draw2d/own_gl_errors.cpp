// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/own_gl_errors.hpp"

#include "draw2d/prior_gl_errors.hpp"

namespace glintfx::draw2d {

render::GLenum first_own_gl_error(const render::gl_function_table &gl) noexcept {
    render::GLenum first = 0;
    for (int read = 0; read < k_max_gl_error_reads; ++read) {
        const render::GLenum error = gl.glGetError();
        if (error == 0) {
            return first;
        }
        first = first != 0 ? first : error;
    }
    return first;
}

} // namespace glintfx::draw2d
