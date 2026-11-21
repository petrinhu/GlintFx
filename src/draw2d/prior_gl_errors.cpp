// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/prior_gl_errors.hpp"

#include <array>

#include <glintfx/core/log/field.hpp>
#include <glintfx/core/log/severity.hpp>

#include "core/log/emit.hpp"

namespace glintfx::draw2d {

void drain_prior_gl_errors(const render::gl_function_table &gl) noexcept {
    for (int read = 0; read < k_max_gl_error_reads; ++read) {
        const render::GLenum error = gl.glGetError();
        if (error == 0) {
            return;
        }
        const std::array<gltfx_log_field, 1> fields = {
            gltfx_log_field{k_gl_error_field, gltfx_log_value::make_unsigned_integer(error)},
        };
        log_emit_fields(gltfx_log_severity::warn, "draw2d", k_prior_gl_error_event, fields);
    }
}

} // namespace glintfx::draw2d
