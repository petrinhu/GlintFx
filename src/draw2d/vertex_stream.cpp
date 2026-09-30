// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/vertex_stream.hpp"

#include <algorithm>
#include <cstring>

#include <array>

#include <glintfx/core/err_code.hpp>
#include <glintfx/core/log/field.hpp>
#include <glintfx/core/log/severity.hpp>

#include "core/log/emit.hpp"

namespace glintfx::draw2d {

namespace {

using render::GLbitfield;
using render::GLenum;
using render::GLsizei;
using render::GLsizeiptr;
using render::GLuint;

// The values of the vendored registry (third_party/khronos/gl.xml); the generated loader carries
// functions, not constants.
constexpr GLenum k_gl_array_buffer = 0x8892;
constexpr GLenum k_gl_element_array_buffer = 0x8893;
constexpr GLenum k_gl_stream_draw = 0x88E0;
constexpr GLenum k_gl_float = 0x1406;
constexpr GLenum k_gl_unsigned_int = 0x1405;
constexpr GLenum k_gl_triangles = 0x0004;
constexpr GLenum k_gl_out_of_memory = 0x0505;
constexpr GLbitfield k_gl_map_write_bit = 0x0002;
constexpr GLbitfield k_gl_map_invalidate_buffer_bit = 0x0008;

// The layout of batch_vertex (triangle_batch.hpp): position, texture coordinate, color.
constexpr GLsizei k_stride = static_cast<GLsizei>(sizeof(batch_vertex));
constexpr std::uintptr_t k_texcoord_offset = 2 * sizeof(float);
constexpr std::uintptr_t k_color_offset = 4 * sizeof(float);

void delete_objects(const render::gl_function_table &gl, GLuint vertex_array,
                    const GLuint (&buffers)[2]) noexcept {
    if (vertex_array != 0) {
        gl.glDeleteVertexArrays(1, &vertex_array);
    }
    if (buffers[0] != 0 || buffers[1] != 0) {
        gl.glDeleteBuffers(2, buffers);
    }
}

// GL's own convention: with a buffer bound, the "pointer" of glVertexAttribPointer and of
// glDrawElements is a BYTE OFFSET into that buffer, passed as a pointer. The one place the integer
// becomes one.
[[nodiscard]] const void *buffer_offset(std::uintptr_t offset) noexcept {
    // NOLINTNEXTLINE(performance-no-int-to-ptr) reason: the GL offset-as-pointer idiom
    return reinterpret_cast<const void *>(offset);
}

void describe_attribute(const render::gl_function_table &gl, GLuint index, int components,
                        std::uintptr_t offset) noexcept {
    gl.glEnableVertexAttribArray(index);
    gl.glVertexAttribPointer(index, components, k_gl_float, 0, k_stride, buffer_offset(offset));
}

// Sends `bytes` bytes to the buffer bound to `target`, growing its storage (doubling, never
// shrinking) when they no longer fit. False when a mapping came back null or corrupted.
[[nodiscard]] bool send_to_buffer(const render::gl_function_table &gl, GLenum target, GLuint buffer,
                                  std::size_t &capacity, const void *data, std::size_t bytes,
                                  vertex_upload_technique technique) noexcept {
    gl.glBindBuffer(target, buffer);
    const bool must_grow = bytes > capacity;
    if (must_grow) {
        capacity = std::max(bytes, capacity * 2);
    }
    if (technique == vertex_upload_technique::orphan_and_sub_data) {
        // Orphaning on EVERY upload is the technique: the driver hands out fresh storage instead of
        // waiting for the card to finish with the old one.
        gl.glBufferData(target, static_cast<GLsizeiptr>(capacity), nullptr, k_gl_stream_draw);
        gl.glBufferSubData(target, 0, static_cast<GLsizeiptr>(bytes), data);
        return true;
    }
    if (must_grow) {
        gl.glBufferData(target, static_cast<GLsizeiptr>(capacity), nullptr, k_gl_stream_draw);
    }
    void *mapped = gl.glMapBufferRange(target, 0, static_cast<GLsizeiptr>(bytes),
                                       k_gl_map_write_bit | k_gl_map_invalidate_buffer_bit);
    if (mapped == nullptr) {
        return false;
    }
    std::memcpy(mapped, data, bytes);
    return gl.glUnmapBuffer(target) != 0;
}

// Reads the queue the CONSUMER left (D-B3c-2) and says each error in the log: never swallowed,
// never ours.
void drain_prior_gl_errors(const render::gl_function_table &gl) noexcept {
    for (int read = 0; read < k_max_gl_error_reads; ++read) {
        const GLenum error = gl.glGetError();
        if (error == 0) {
            return;
        }
        const std::array<gltfx_log_field, 1> fields = {
            gltfx_log_field{k_gl_error_field, gltfx_log_value::make_unsigned_integer(error)},
        };
        log_emit_fields(gltfx_log_severity::warn, "draw2d", k_prior_gl_error_event, fields);
    }
}

// What the operation left in the queue: the first error, and whether any of them was out-of-memory.
struct gl_errors_of_operation {
    GLenum first = 0;
    bool out_of_memory = false;
};

[[nodiscard]] gl_errors_of_operation read_gl_errors(const render::gl_function_table &gl) noexcept {
    gl_errors_of_operation result;
    for (int read = 0; read < k_max_gl_error_reads; ++read) {
        const GLenum error = gl.glGetError();
        if (error == 0) {
            break;
        }
        if (result.first == 0) {
            result.first = error;
        }
        result.out_of_memory = result.out_of_memory || error == k_gl_out_of_memory;
    }
    return result;
}

// The error of a step whose GL queue held `errors`: GL_OUT_OF_MEMORY is out_of_memory, anything
// else is platform_failure; the token names the step and the GL code rides in os_error_code.
[[nodiscard]] gltfx_err error_of_step(const gl_errors_of_operation &errors,
                                      std::string_view token) noexcept {
    const GLenum code = errors.out_of_memory ? k_gl_out_of_memory : errors.first;
    gltfx_err error(errors.out_of_memory ? gltfx_err_code::out_of_memory
                                         : gltfx_err_code::platform_failure);
    error.with_rejected_value(token).with_os_error_code(static_cast<std::int64_t>(code));
    return error;
}

// A step that failed WITHOUT a GL error (a null or corrupted mapping, a glGen* that gave 0 with
// nothing in the queue): platform_failure with the token and no GL code.
[[nodiscard]] gltfx_err error_without_gl_code(std::string_view token) noexcept {
    return gltfx_err(gltfx_err_code::platform_failure).with_rejected_value(token);
}

} // namespace

gltfx_rslt<vertex_stream> create_vertex_stream(const render::gl_function_table &gl) noexcept {
    drain_prior_gl_errors(gl);
    GLuint vertex_array = 0;
    GLuint buffers[2] = {0, 0};
    gl.glGenVertexArrays(1, &vertex_array);
    gl.glGenBuffers(2, buffers);
    if (vertex_array == 0 || buffers[0] == 0 || buffers[1] == 0) {
        // Ids of zero: with an error in the queue it is that error; with none it is a lost or
        // not-current context, and glGen* allocates no storage, so it is never out_of_memory.
        const gl_errors_of_operation errors = read_gl_errors(gl);
        delete_objects(gl, vertex_array, buffers);
        return gltfx_rslt<vertex_stream>::err(
            errors.first != 0 ? error_of_step(errors, k_reject_vertex_array_create)
                              : error_without_gl_code(k_reject_vertex_array_create));
    }

    gl.glBindVertexArray(vertex_array);
    gl.glBindBuffer(k_gl_array_buffer, buffers[0]);
    describe_attribute(gl, 0, 2, 0);
    describe_attribute(gl, 1, 2, k_texcoord_offset);
    describe_attribute(gl, 2, 4, k_color_offset);
    // The element-array binding is state OF the vertex array: binding it here ties the index buffer
    // to this array.
    gl.glBindBuffer(k_gl_element_array_buffer, buffers[1]);
    gl.glBindVertexArray(0);
    gl.glBindBuffer(k_gl_array_buffer, 0);

    const gl_errors_of_operation errors = read_gl_errors(gl);
    if (errors.first != 0) {
        delete_objects(gl, vertex_array, buffers);
        return gltfx_rslt<vertex_stream>::err(error_of_step(errors, k_reject_vertex_array_create));
    }
    vertex_stream stream;
    stream.vertex_array = vertex_array;
    stream.vertex_buffer = buffers[0];
    stream.index_buffer = buffers[1];
    return gltfx_rslt<vertex_stream>::ok(stream);
}

void destroy_vertex_stream(const render::gl_function_table &gl, vertex_stream &stream) noexcept {
    const GLuint buffers[2] = {stream.vertex_buffer, stream.index_buffer};
    delete_objects(gl, stream.vertex_array, buffers);
    stream = vertex_stream{};
}

gltfx_rslt<void> upload_batch(const render::gl_function_table &gl, vertex_stream &stream,
                              vertex_upload_technique technique,
                              const triangle_batch &batch) noexcept {
    const auto vertices = batch.vertices();
    const auto indices = batch.indices();
    if (vertices.empty() || indices.empty()) {
        return gltfx_rslt<void>::ok();
    }
    drain_prior_gl_errors(gl);
    // The array first, so the element-array binding below lands in ITS state.
    gl.glBindVertexArray(stream.vertex_array);
    const bool vertices_sent =
        send_to_buffer(gl, k_gl_array_buffer, stream.vertex_buffer, stream.vertex_capacity_bytes,
                       vertices.data(), vertices.size() * sizeof(batch_vertex), technique);
    // The errors of each buffer are read BEFORE the next buffer is touched, so each carries its own
    // token.
    const gl_errors_of_operation vertex_errors = read_gl_errors(gl);
    if (vertex_errors.first != 0) {
        return gltfx_rslt<void>::err(error_of_step(vertex_errors, k_reject_vertex_upload));
    }
    if (!vertices_sent) {
        return gltfx_rslt<void>::err(error_without_gl_code(k_reject_vertex_upload));
    }
    const bool indices_sent = send_to_buffer(gl, k_gl_element_array_buffer, stream.index_buffer,
                                             stream.index_capacity_bytes, indices.data(),
                                             indices.size() * sizeof(std::uint32_t), technique);
    const gl_errors_of_operation index_errors = read_gl_errors(gl);
    if (index_errors.first != 0) {
        return gltfx_rslt<void>::err(error_of_step(index_errors, k_reject_index_upload));
    }
    if (!indices_sent) {
        return gltfx_rslt<void>::err(error_without_gl_code(k_reject_index_upload));
    }
    return gltfx_rslt<void>::ok();
}

std::uint64_t draw_batch(const render::gl_function_table &gl,
                         const triangle_batch &batch) noexcept {
    std::uint64_t calls = 0;
    for (const draw_run &run : batch.runs()) {
        gl.glDrawElements(k_gl_triangles, static_cast<GLsizei>(run.index_count), k_gl_unsigned_int,
                          buffer_offset(run.first_index * sizeof(std::uint32_t)));
        ++calls;
    }
    return calls;
}

} // namespace glintfx::draw2d
