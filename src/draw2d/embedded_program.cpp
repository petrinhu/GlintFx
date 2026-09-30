// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/embedded_program.hpp"

#include <array>
#include <span>

#include <glintfx/core/err_code.hpp>
#include <glintfx/core/log/field.hpp>
#include <glintfx/core/log/severity.hpp>

#include "core/log/emit.hpp"

namespace glintfx::draw2d {

namespace {

using render::GLchar;
using render::GLenum;
using render::GLint;
using render::GLsizei;
using render::GLuint;

// The values of the vendored registry (third_party/khronos/gl.xml), declared here because the
// generated loader carries functions, not constants.
constexpr GLenum k_gl_fragment_shader = 0x8B30;
constexpr GLenum k_gl_vertex_shader = 0x8B31;
constexpr GLenum k_gl_compile_status = 0x8B81;
constexpr GLenum k_gl_link_status = 0x8B82;
constexpr GLenum k_gl_info_log_length = 0x8B84;

// Position in PIXELS to clip space: top-left origin, y down, as the corner roles of core/quad.hpp.
constexpr char k_vertex_source[] =
    "#version 330 core\n"
    "layout(location = 0) in vec2 a_position;\n"
    "layout(location = 1) in vec2 a_texcoord;\n"
    "layout(location = 2) in vec4 a_color;\n"
    "uniform vec2 u_viewport_pixels;\n"
    "out vec2 v_texcoord;\n"
    "out vec4 v_color;\n"
    "void main() {\n"
    "    vec2 clip = vec2(a_position.x / u_viewport_pixels.x * 2.0 - 1.0,\n"
    "                     1.0 - a_position.y / u_viewport_pixels.y * 2.0);\n"
    "    gl_Position = vec4(clip, 0.0, 1.0);\n"
    "    v_texcoord = a_texcoord;\n"
    "    v_color = a_color;\n"
    "}\n";

// The color is already premultiplied (triangle_batch.hpp). With the context option
// `srgb_framebuffer` ON the surface encodes for us and the color is written as it is; with it OFF
// (`u_encode_srgb` = 1) the program encodes to sRGB before the blend does its work, which is what
// most 2D libraries do (D-W7D-12): un-premultiply, encode the straight color, premultiply again.
constexpr char k_fragment_source[] =
    "#version 330 core\n"
    "in vec2 v_texcoord;\n"
    "in vec4 v_color;\n"
    "uniform int u_encode_srgb;\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    "    vec4 color = v_color;\n"
    "    if (u_encode_srgb != 0 && color.a > 0.0) {\n"
    "        vec3 straight = color.rgb / color.a;\n"
    "        vec3 low = straight * 12.92;\n"
    "        vec3 high = 1.055 * pow(straight, vec3(1.0 / 2.4)) - 0.055;\n"
    "        color.rgb = mix(low, high, step(vec3(0.0031308), straight)) * color.a;\n"
    "    }\n"
    "    frag_color = color;\n"
    "}\n";

// Sends the refusal to the log sink as the event draw2d_program_rejected: the driver's message
// (already read into `text`, empty when the refusal was not the driver's own), the boolean that
// says whether it was cut, and the token that says why. The message never enters the error.
void report_rejection(std::string_view reason, std::string_view text, bool truncated) noexcept {
    const std::array<gltfx_log_field, 3> fields = {
        gltfx_log_field{k_driver_log_field, gltfx_log_value::make_text(text)},
        gltfx_log_field{k_driver_log_truncated_field, gltfx_log_value::make_boolean(truncated)},
        gltfx_log_field{k_reason_field, gltfx_log_value::make_text(reason)},
    };
    log_emit_fields(gltfx_log_severity::err, "draw2d", k_program_rejected_event, fields);
}

[[nodiscard]] gltfx_err rejection(std::string_view token) noexcept {
    return gltfx_err(gltfx_err_code::platform_failure).with_rejected_value(token);
}

struct driver_log_reading {
    std::string_view text;
    bool truncated = false;
};

// The driver's message for a shader (`is_program` false) or a program, cut to the stack buffer. It
// is truncated when the driver's own GL_INFO_LOG_LENGTH (terminator included) is larger than the
// buffer.
[[nodiscard]] driver_log_reading
read_driver_log(const render::gl_function_table &gl, GLuint object, bool is_program,
                std::array<char, k_driver_log_capacity> &buffer) noexcept {
    GLint full_length = 0;
    GLsizei length = 0;
    const auto capacity = static_cast<GLsizei>(buffer.size());
    if (is_program) {
        gl.glGetProgramiv(object, k_gl_info_log_length, &full_length);
        gl.glGetProgramInfoLog(object, capacity, &length, buffer.data());
    } else {
        gl.glGetShaderiv(object, k_gl_info_log_length, &full_length);
        gl.glGetShaderInfoLog(object, capacity, &length, buffer.data());
    }
    if (length < 0) {
        length = 0;
    }
    if (static_cast<std::size_t>(length) >= buffer.size()) {
        length = static_cast<GLsizei>(buffer.size() - 1);
    }
    return {std::string_view(buffer.data(), static_cast<std::size_t>(length)),
            full_length > capacity};
}

// Compiles one shader. On failure the shader is deleted, the driver's message is logged and the
// error carries `token`.
[[nodiscard]] gltfx_rslt<GLuint> compile_shader(const render::gl_function_table &gl, GLenum type,
                                                const char *source,
                                                std::string_view token) noexcept {
    const GLuint shader = gl.glCreateShader(type);
    if (shader == 0) {
        report_rejection(k_reason_create_shader_returned_zero, "", false);
        return gltfx_rslt<GLuint>::err(rejection(token));
    }
    gl.glShaderSource(shader, 1, &source, nullptr);
    gl.glCompileShader(shader);
    GLint status = 0;
    gl.glGetShaderiv(shader, k_gl_compile_status, &status);
    if (status != 1) {
        std::array<char, k_driver_log_capacity> buffer{};
        const driver_log_reading reading = read_driver_log(gl, shader, false, buffer);
        report_rejection(k_reason_shader_compile_failed, reading.text, reading.truncated);
        gl.glDeleteShader(shader);
        return gltfx_rslt<GLuint>::err(rejection(token));
    }
    return gltfx_rslt<GLuint>::ok(shader);
}

} // namespace

std::string_view embedded_vertex_shader_source() noexcept {
    return std::string_view(k_vertex_source, sizeof(k_vertex_source) - 1);
}

std::string_view embedded_fragment_shader_source() noexcept {
    return std::string_view(k_fragment_source, sizeof(k_fragment_source) - 1);
}

gltfx_rslt<embedded_program> create_embedded_program(const render::gl_function_table &gl) noexcept {
    const gltfx_rslt<GLuint> vertex =
        compile_shader(gl, k_gl_vertex_shader, k_vertex_source, k_reject_vertex_shader);
    if (vertex.has_error()) {
        return gltfx_rslt<embedded_program>::err(vertex.err());
    }
    const gltfx_rslt<GLuint> fragment =
        compile_shader(gl, k_gl_fragment_shader, k_fragment_source, k_reject_fragment_shader);
    if (fragment.has_error()) {
        gl.glDeleteShader(vertex.value());
        return gltfx_rslt<embedded_program>::err(fragment.err());
    }

    const GLuint program = gl.glCreateProgram();
    if (program == 0) {
        // Nothing to attach to: do not call attach/link/get on program 0 (each would leave
        // GL_INVALID_VALUE in the consumer's error queue, R-B3). Free the shaders and refuse.
        report_rejection(k_reason_create_program_returned_zero, "", false);
        gl.glDeleteShader(vertex.value());
        gl.glDeleteShader(fragment.value());
        return gltfx_rslt<embedded_program>::err(rejection(k_reject_program_link));
    }
    gl.glAttachShader(program, vertex.value());
    gl.glAttachShader(program, fragment.value());
    gl.glLinkProgram(program);
    GLint status = 0;
    gl.glGetProgramiv(program, k_gl_link_status, &status);
    if (status != 1) {
        std::array<char, k_driver_log_capacity> buffer{};
        const driver_log_reading reading = read_driver_log(gl, program, true, buffer);
        report_rejection(k_reason_program_link_failed, reading.text, reading.truncated);
        gl.glDeleteProgram(program);
        gl.glDeleteShader(vertex.value());
        gl.glDeleteShader(fragment.value());
        return gltfx_rslt<embedded_program>::err(rejection(k_reject_program_link));
    }

    // Linked: the shaders are no longer needed (a deleted shader stays alive while attached, so
    // detach first).
    gl.glDetachShader(program, vertex.value());
    gl.glDetachShader(program, fragment.value());
    gl.glDeleteShader(vertex.value());
    gl.glDeleteShader(fragment.value());

    embedded_program result;
    result.program = program;
    result.viewport_location = gl.glGetUniformLocation(program, "u_viewport_pixels");
    result.encode_srgb_location = gl.glGetUniformLocation(program, "u_encode_srgb");
    return gltfx_rslt<embedded_program>::ok(result);
}

void destroy_embedded_program(const render::gl_function_table &gl,
                              embedded_program &program) noexcept {
    if (program.program != 0) {
        gl.glDeleteProgram(program.program);
    }
    program = embedded_program{};
}

} // namespace glintfx::draw2d
