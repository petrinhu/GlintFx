// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/surface_stencil.hpp"

#include <array>

#include <glintfx/core/log/field.hpp>
#include <glintfx/core/log/severity.hpp>

#include "core/log/emit.hpp"

namespace glintfx::draw2d {

namespace {

using render::GLenum;
using render::GLint;

// The values of the vendored registry (third_party/khronos/gl.xml); the generated loader carries
// functions, not constants.
constexpr GLenum k_gl_draw_framebuffer = 0x8CA9;
constexpr GLenum k_gl_draw_framebuffer_binding = 0x8CA6;
constexpr GLenum k_gl_stencil = 0x1802;
constexpr GLenum k_gl_attachment_object_type = 0x8CD0;
constexpr GLenum k_gl_attachment_stencil_size = 0x8217;
constexpr GLint k_gl_none = 0;

// More than any driver keeps: a lost context can answer the same error for ever.
constexpr int k_max_gl_error_reads = 16;

// Empties the queue, and says the first error that was in it (0 for none).
[[nodiscard]] GLenum first_gl_error(const render::gl_function_table &gl) noexcept {
    GLenum first = 0;
    for (int read = 0; read < k_max_gl_error_reads; ++read) {
        const GLenum error = gl.glGetError();
        if (error == 0) {
            return first;
        }
        first = first != 0 ? first : error;
    }
    return first;
}

// The card answered a question about its bits with an error: the library goes on trusting what it
// asked for, and whoever reads the log sees it.
[[nodiscard]] bool present_because_the_reading_failed(GLenum error) noexcept {
    const std::array<gltfx_log_field, 1> fields = {
        gltfx_log_field{k_gl_error_field_of_stencil, gltfx_log_value::make_unsigned_integer(error)},
    };
    log_emit_fields(gltfx_log_severity::warn, "draw2d", k_stencil_bits_unread_event, fields);
    return true;
}

// The two questions, on the framebuffer already bound; a GL error says nothing about the bits.
[[nodiscard]] bool default_framebuffer_has_stencil(const render::gl_function_table &gl) noexcept {
    GLint type = k_gl_none;
    gl.glGetFramebufferAttachmentParameteriv(k_gl_draw_framebuffer, k_gl_stencil,
                                             k_gl_attachment_object_type, &type);
    if (const GLenum error = first_gl_error(gl); error != 0) {
        return present_because_the_reading_failed(error);
    }
    if (type == k_gl_none) {
        return false;
    }
    GLint bits = 0;
    gl.glGetFramebufferAttachmentParameteriv(k_gl_draw_framebuffer, k_gl_stencil,
                                             k_gl_attachment_stencil_size, &bits);
    if (const GLenum error = first_gl_error(gl); error != 0) {
        return present_because_the_reading_failed(error);
    }
    return bits > 0;
}

} // namespace

bool surface_has_stencil(const render::gl_function_table &gl) noexcept {
    (void)first_gl_error(gl); // what is in the queue is not an answer to these questions
    GLint consumer_framebuffer = 0;
    gl.glGetIntegerv(k_gl_draw_framebuffer_binding, &consumer_framebuffer);
    gl.glBindFramebuffer(k_gl_draw_framebuffer, 0);
    const bool present = default_framebuffer_has_stencil(gl);
    gl.glBindFramebuffer(k_gl_draw_framebuffer, static_cast<render::GLuint>(consumer_framebuffer));
    return present;
}

} // namespace glintfx::draw2d
