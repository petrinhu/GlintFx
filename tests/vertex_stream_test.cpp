// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstdint>
#include <cstring>
#include <print>
#include <string>
#include <vector>

#include <glintfx/core/color.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/core/log/event.hpp>
#include <glintfx/core/log/sink.hpp>

#include "draw2d/quad_vertices.hpp"
#include "draw2d/triangle_batch.hpp"
#include "draw2d/vertex_stream.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// vertex_stream_test.cpp - R2D-BATCH, fatia B3c (docs/plano-w7d.md sec. 4.3, R-B6). Proves the
// creation, the two ways of sending a batch and the draw of vertex_stream.hpp against a
// gl_function_table of FAKES that keep real storage for the buffers: what the fake buffer holds
// after the upload is compared BYTE FOR BYTE with the batch, so a wrong offset, size or order
// shows.
//
// A DECLARED DOWNGRADE (GODS_LAWS.md L-09): no real driver is asked, so this proves the sequencing
// and the bytes, never the cost of either technique nor what a real driver does with a corrupted
// map; those are the live cells (tests/parity/draw2d_parity_test.cpp, printed as MEASURED). The
// literal GL enumerators are the vendored registry's, never the constants the code uses (D-P1-5's
// rule).

using glintfx::gltfx_err_code;
using glintfx::gltfx_log_event;
using glintfx::gltfx_log_set_sink;
using glintfx::gltfx_log_severity;
using glintfx::gltfx_log_sink;
using glintfx::gltfx_rgba;
using glintfx::gltfx_vec2_screen;
using glintfx::draw2d::batch_state;
using glintfx::draw2d::batch_vertex;
using glintfx::draw2d::create_vertex_stream;
using glintfx::draw2d::destroy_vertex_stream;
using glintfx::draw2d::draw_batch;
using glintfx::draw2d::quad_corners_pixel;
using glintfx::draw2d::triangle_batch;
using glintfx::draw2d::upload_batch;
using glintfx::draw2d::vertex_stream;
using glintfx::draw2d::vertex_upload_technique;
using glintfx::render::gl_function_table;
using glintfx::render::GLbitfield;
using glintfx::render::GLboolean;
using glintfx::render::GLenum;
using glintfx::render::GLint;
using glintfx::render::GLintptr;
using glintfx::render::GLsizei;
using glintfx::render::GLsizeiptr;
using glintfx::render::GLuint;

namespace {

constexpr GLenum k_array_buffer = 0x8892;
constexpr GLenum k_float = 0x1406;
constexpr GLenum k_unsigned_int = 0x1405;
constexpr GLenum k_triangles = 0x0004;
constexpr GLbitfield k_map_write_bit = 0x0002;
constexpr GLbitfield k_map_invalidate_buffer_bit = 0x0008;

struct attrib_call {
    GLuint index = 0;
    GLint size = 0;
    GLenum type = 0;
    GLboolean normalized = 0;
    GLsizei stride = 0;
    std::uintptr_t offset = 0;
};
struct draw_call {
    GLenum mode = 0;
    GLsizei count = 0;
    GLenum type = 0;
    std::uintptr_t offset = 0;
};

struct fake_gl {
    GLuint next_id = 1;
    bool gen_fails = false;        // glGen* leaves the ids at zero
    bool gen_buffers_fail = false; // only glGenBuffers does (the array WAS created)
    std::vector<GLenum>
        error_queue; // the GL error queue: glGetError pops the FRONT (what the consumer left,
                     // planted before the call, and what the fake raises below)
    GLenum endless_error = 0; // a GL that never empties its queue (glGetError always answers it)
    GLenum error_from_layout = 0;            // raised by the first glVertexAttribPointer
    GLenum error_from_first_buffer_data = 0; // raised by the FIRST glBufferData (the vertex buffer)
    GLenum error_from_second_buffer_data =
        0; // raised by the SECOND glBufferData (the index buffer)
    bool also_out_of_memory_from_first_buffer_data =
        false; // a SECOND error, out-of-memory, behind the first
    int buffer_data_calls = 0;
    GLenum error_from_draw = 0; // raised by glDrawElements
    int ignored_buffer_data_call =
        0; // CTO: this glBufferData raises OUT_OF_MEMORY and keeps the OLD store
    int get_error_calls = 0;
    bool map_returns_null = false;
    bool unmap_reports_corruption = false;
    std::array<std::vector<unsigned char>, 32> storage{};
    std::array<int, 32> orphan_calls{}; // glBufferData with a null pointer, per buffer id
    std::array<GLsizeiptr, 32> last_buffer_data_size{};
    std::array<int, 32> map_calls{};
    std::array<int, 32> unmap_calls{};
    std::array<GLbitfield, 32> last_map_access{};
    GLuint bound_array_buffer = 0;
    GLuint bound_element_buffer = 0;
    GLuint bound_vertex_array = 0;
    std::vector<attrib_call> attribs;
    std::vector<GLuint> enabled_attribs;
    std::vector<draw_call> draws;
    int vertex_arrays_created = 0;
    int vertex_arrays_deleted = 0;
    int buffers_created = 0;
    int buffers_deleted = 0;
    int gl_calls = 0; // every call of the table, to prove an empty batch calls none
};

fake_gl gl_state;

void reset() { gl_state = fake_gl{}; }

GLuint &bound(GLenum target) {
    return target == k_array_buffer ? gl_state.bound_array_buffer : gl_state.bound_element_buffer;
}

void GLINTFX_GL_APIENTRY fake_gen_vertex_arrays(GLsizei n, GLuint *arrays) {
    ++gl_state.gl_calls;
    for (GLsizei i = 0; i < n; ++i) {
        arrays[i] = gl_state.gen_fails ? 0 : gl_state.next_id++;
        gl_state.vertex_arrays_created += arrays[i] != 0 ? 1 : 0;
    }
}
void GLINTFX_GL_APIENTRY fake_gen_buffers(GLsizei n, GLuint *buffers) {
    ++gl_state.gl_calls;
    for (GLsizei i = 0; i < n; ++i) {
        buffers[i] = (gl_state.gen_fails || gl_state.gen_buffers_fail) ? 0 : gl_state.next_id++;
        gl_state.buffers_created += buffers[i] != 0 ? 1 : 0;
    }
}
void GLINTFX_GL_APIENTRY fake_delete_vertex_arrays(GLsizei n, const GLuint *arrays) {
    ++gl_state.gl_calls;
    for (GLsizei i = 0; i < n; ++i) {
        gl_state.vertex_arrays_deleted += arrays[i] != 0 ? 1 : 0;
    }
}
void GLINTFX_GL_APIENTRY fake_delete_buffers(GLsizei n, const GLuint *buffers) {
    ++gl_state.gl_calls;
    for (GLsizei i = 0; i < n; ++i) {
        gl_state.buffers_deleted += buffers[i] != 0 ? 1 : 0;
    }
}
void GLINTFX_GL_APIENTRY fake_bind_vertex_array(GLuint array) {
    ++gl_state.gl_calls;
    gl_state.bound_vertex_array = array;
}
void GLINTFX_GL_APIENTRY fake_bind_buffer(GLenum target, GLuint buffer) {
    ++gl_state.gl_calls;
    bound(target) = buffer;
}
void GLINTFX_GL_APIENTRY fake_buffer_data(GLenum target, GLsizeiptr size, const void *data,
                                          GLenum) {
    ++gl_state.gl_calls;
    ++gl_state.buffer_data_calls;
    if (gl_state.buffer_data_calls == 1 && gl_state.error_from_first_buffer_data != 0) {
        gl_state.error_queue.push_back(gl_state.error_from_first_buffer_data);
        if (gl_state.also_out_of_memory_from_first_buffer_data) {
            gl_state.error_queue.push_back(0x0505);
        }
    }
    if (gl_state.buffer_data_calls == 2 && gl_state.error_from_second_buffer_data != 0) {
        gl_state.error_queue.push_back(gl_state.error_from_second_buffer_data);
    }
    if (gl_state.ignored_buffer_data_call != 0 &&
        gl_state.buffer_data_calls == gl_state.ignored_buffer_data_call) {
        gl_state.error_queue.push_back(0x0505);
        return;
    }
    const GLuint id = bound(target);
    gl_state.storage[id].assign(static_cast<std::size_t>(size), 0);
    gl_state.last_buffer_data_size[id] = size;
    if (data == nullptr) {
        ++gl_state.orphan_calls[id];
    } else {
        std::memcpy(gl_state.storage[id].data(), data, static_cast<std::size_t>(size));
    }
}
void GLINTFX_GL_APIENTRY fake_buffer_sub_data(GLenum target, GLintptr offset, GLsizeiptr size,
                                              const void *data) {
    ++gl_state.gl_calls;
    auto &store = gl_state.storage[bound(target)];
    if (static_cast<std::size_t>(offset + size) <= store.size()) {
        std::memcpy(store.data() + offset, data, static_cast<std::size_t>(size));
    }
}
void *GLINTFX_GL_APIENTRY fake_map_buffer_range(GLenum target, GLintptr offset, GLsizeiptr length,
                                                GLbitfield access) {
    ++gl_state.gl_calls;
    const GLuint id = bound(target);
    ++gl_state.map_calls[id];
    gl_state.last_map_access[id] = access;
    if (gl_state.map_returns_null ||
        static_cast<std::size_t>(offset + length) > gl_state.storage[id].size()) {
        return nullptr;
    }
    return gl_state.storage[id].data() + offset;
}
GLboolean GLINTFX_GL_APIENTRY fake_unmap_buffer(GLenum target) {
    ++gl_state.gl_calls;
    ++gl_state.unmap_calls[bound(target)];
    return gl_state.unmap_reports_corruption ? 0 : 1;
}
void GLINTFX_GL_APIENTRY fake_enable_vertex_attrib_array(GLuint index) {
    ++gl_state.gl_calls;
    gl_state.enabled_attribs.push_back(index);
}
void GLINTFX_GL_APIENTRY fake_vertex_attrib_pointer(GLuint index, GLint size, GLenum type,
                                                    GLboolean normalized, GLsizei stride,
                                                    const void *pointer) {
    ++gl_state.gl_calls;
    if (gl_state.attribs.empty() && gl_state.error_from_layout != 0) {
        gl_state.error_queue.push_back(gl_state.error_from_layout);
    }
    gl_state.attribs.push_back(
        {index, size, type, normalized, stride, reinterpret_cast<std::uintptr_t>(pointer)});
}
void GLINTFX_GL_APIENTRY fake_draw_elements(GLenum mode, GLsizei count, GLenum type,
                                            const void *indices) {
    ++gl_state.gl_calls;
    if (gl_state.error_from_draw != 0) {
        gl_state.error_queue.push_back(gl_state.error_from_draw);
    }
    gl_state.draws.push_back({mode, count, type, reinterpret_cast<std::uintptr_t>(indices)});
}
GLenum GLINTFX_GL_APIENTRY fake_get_error() {
    ++gl_state.gl_calls;
    ++gl_state.get_error_calls;
    if (gl_state.endless_error != 0) {
        return gl_state.endless_error;
    }
    if (gl_state.error_queue.empty()) {
        return 0;
    }
    const GLenum error = gl_state.error_queue.front();
    gl_state.error_queue.erase(gl_state.error_queue.begin());
    return error;
}

gl_function_table fake_table() {
    gl_function_table table;
    table.glGenVertexArrays = &fake_gen_vertex_arrays;
    table.glGenBuffers = &fake_gen_buffers;
    table.glDeleteVertexArrays = &fake_delete_vertex_arrays;
    table.glDeleteBuffers = &fake_delete_buffers;
    table.glBindVertexArray = &fake_bind_vertex_array;
    table.glBindBuffer = &fake_bind_buffer;
    table.glBufferData = &fake_buffer_data;
    table.glBufferSubData = &fake_buffer_sub_data;
    table.glMapBufferRange = &fake_map_buffer_range;
    table.glUnmapBuffer = &fake_unmap_buffer;
    table.glEnableVertexAttribArray = &fake_enable_vertex_attrib_array;
    table.glVertexAttribPointer = &fake_vertex_attrib_pointer;
    table.glDrawElements = &fake_draw_elements;
    table.glGetError = &fake_get_error;
    return table;
}

constexpr gltfx_rgba k_red{1.0F, 0.0F, 0.0F, 1.0F};
constexpr batch_state k_state_a{1, 2, 3};
constexpr batch_state k_state_b{1, 9, 3};

[[nodiscard]] quad_corners_pixel square_at(float x, float y, float size) {
    return quad_corners_pixel{gltfx_vec2_screen{x, y}, gltfx_vec2_screen{x + size, y},
                              gltfx_vec2_screen{x + size, y + size},
                              gltfx_vec2_screen{x, y + size}};
}

void fill(triangle_batch &batch, int pieces, bool alternate_states) {
    for (int i = 0; i < pieces; ++i) {
        const batch_state state = (alternate_states && (i % 2 == 1)) ? k_state_b : k_state_a;
        GLINTFX_CHECK(batch.add_quad(square_at(static_cast<float>(i), 1.0F, 2.0F), k_red, state));
    }
}

[[nodiscard]] bool bytes_equal(const std::vector<unsigned char> &stored, const void *expected,
                               std::size_t size) {
    return stored.size() >= size && std::memcmp(stored.data(), expected, size) == 0;
}

// The log sink of the test: every event, its text copied out (an event is valid only during the
// call).
struct captured_event {
    std::string name;
    std::string category;
    gltfx_log_severity severity = gltfx_log_severity::unknown;
    std::string field_name;
    std::uint64_t field_value = 0;
};
std::vector<captured_event> captured;

void capture_sink(void *, const gltfx_log_event &event) noexcept {
    captured_event entry;
    entry.name = std::string(event.name());
    entry.category = std::string(event.category());
    entry.severity = event.severity();
    if (!event.fields().empty()) {
        entry.field_name = std::string(event.fields()[0].name);
        entry.field_value = event.fields()[0].value.unsigned_integer();
    }
    captured.push_back(entry);
}

void arm_log() {
    captured.clear();
    gltfx_log_set_sink(gltfx_log_sink{&capture_sink, nullptr, gltfx_log_severity::trace});
}
void disarm_log() { gltfx_log_set_sink(gltfx_log_sink{}); }

} // namespace

GLINTFX_TEST(vertex_stream_creation_describes_the_layout_and_leaves_everything_unbound) {
    reset();
    const gl_function_table table = fake_table();
    auto created = create_vertex_stream(table);
    GLINTFX_CHECK(created.has_value());
    const vertex_stream stream = created.value();
    GLINTFX_CHECK(stream.vertex_array != 0 && stream.vertex_buffer != 0 &&
                  stream.index_buffer != 0);
    GLINTFX_CHECK_EQ(gl_state.vertex_arrays_created, 1);
    GLINTFX_CHECK_EQ(gl_state.buffers_created, 2);
    GLINTFX_CHECK_EQ(gl_state.attribs.size(), std::size_t{3});
    // position float2 @0, texture coordinate float2 @8, color float4 @16, stride 32, not normalized
    GLINTFX_CHECK_EQ(gl_state.attribs[0].index, GLuint{0});
    GLINTFX_CHECK_EQ(gl_state.attribs[0].size, 2);
    GLINTFX_CHECK_EQ(gl_state.attribs[0].type, k_float);
    GLINTFX_CHECK_EQ(gl_state.attribs[0].stride, 32);
    GLINTFX_CHECK_EQ(gl_state.attribs[0].offset, std::uintptr_t{0});
    GLINTFX_CHECK_EQ(gl_state.attribs[1].index, GLuint{1});
    GLINTFX_CHECK_EQ(gl_state.attribs[1].size, 2);
    GLINTFX_CHECK_EQ(gl_state.attribs[1].stride, 32);
    GLINTFX_CHECK_EQ(gl_state.attribs[1].offset, std::uintptr_t{8});
    GLINTFX_CHECK_EQ(gl_state.attribs[2].index, GLuint{2});
    GLINTFX_CHECK_EQ(gl_state.attribs[2].size, 4);
    GLINTFX_CHECK_EQ(gl_state.attribs[2].stride, 32);
    GLINTFX_CHECK_EQ(gl_state.attribs[2].offset, std::uintptr_t{16});
    for (const auto &attrib : gl_state.attribs) {
        GLINTFX_CHECK_EQ(attrib.type, k_float);
        GLINTFX_CHECK_EQ(attrib.normalized, GLboolean{0});
    }
    GLINTFX_CHECK_EQ(gl_state.enabled_attribs.size(), std::size_t{3});
    GLINTFX_CHECK_EQ(gl_state.bound_vertex_array, GLuint{0});
    GLINTFX_CHECK_EQ(gl_state.bound_array_buffer, GLuint{0});
    // (the element-array binding is state OF the vertex array, which is unbound: not asserted here)
}

GLINTFX_TEST(vertex_stream_creation_failure_is_out_of_memory_and_leaves_no_object) {
    reset();
    gl_state.gen_fails = true;
    const gl_function_table table = fake_table();
    auto failed = create_vertex_stream(table);
    GLINTFX_CHECK(failed.has_error());
    // glGen* answered 0 with NOTHING in the queue (a lost or not-current context): not
    // out_of_memory, since glGen* allocates no storage - platform_failure with the token of the
    // step and no GL code.
    GLINTFX_CHECK(failed.err().code() == gltfx_err_code::platform_failure);
    GLINTFX_CHECK(failed.err().rejected_value() == "vertex_array_create");
    GLINTFX_CHECK_EQ(failed.err().os_error_code(), std::int64_t{0});
    GLINTFX_CHECK_EQ(gl_state.vertex_arrays_created, gl_state.vertex_arrays_deleted);
    GLINTFX_CHECK_EQ(gl_state.buffers_created, gl_state.buffers_deleted);

    // The array was created but the buffers were not: the array must not be leaked.
    reset();
    gl_state.gen_buffers_fail = true;
    const gl_function_table table3 = fake_table();
    auto half = create_vertex_stream(table3);
    GLINTFX_CHECK(half.has_error());
    GLINTFX_CHECK(half.err().code() == gltfx_err_code::platform_failure);
    GLINTFX_CHECK_EQ(gl_state.vertex_arrays_created, 1);
    GLINTFX_CHECK_EQ(gl_state.vertex_arrays_deleted, 1);

    // A GL error while describing the layout is the same failure, with nothing left alive.
    reset();
    const gl_function_table table2 = fake_table();
    gl_state.error_from_layout = 0x0505; // GL_OUT_OF_MEMORY, raised while describing the layout
    auto errored = create_vertex_stream(table2);
    GLINTFX_CHECK(errored.has_error());
    GLINTFX_CHECK(errored.err().code() == gltfx_err_code::out_of_memory);
    GLINTFX_CHECK(errored.err().rejected_value() == "vertex_array_create");
    GLINTFX_CHECK_EQ(errored.err().os_error_code(), std::int64_t{0x0505});
    GLINTFX_CHECK_EQ(gl_state.vertex_arrays_created, gl_state.vertex_arrays_deleted);
    GLINTFX_CHECK_EQ(gl_state.buffers_created, gl_state.buffers_deleted);
}

GLINTFX_TEST(vertex_stream_both_techniques_put_the_exact_bytes_in_both_buffers) {
    for (const vertex_upload_technique technique :
         {vertex_upload_technique::orphan_and_sub_data,
          vertex_upload_technique::map_range_invalidate}) {
        reset();
        const gl_function_table table = fake_table();
        vertex_stream stream = create_vertex_stream(table).value();
        triangle_batch batch;
        fill(batch, 3, false);
        auto sent = upload_batch(table, stream, technique, batch);
        GLINTFX_CHECK(sent.has_value());
        const std::size_t vertex_bytes = batch.vertices().size() * sizeof(batch_vertex);
        const std::size_t index_bytes = batch.indices().size() * sizeof(std::uint32_t);
        GLINTFX_CHECK_EQ(vertex_bytes, std::size_t{384});
        GLINTFX_CHECK_EQ(index_bytes, std::size_t{72});
        GLINTFX_CHECK(bytes_equal(gl_state.storage[stream.vertex_buffer], batch.vertices().data(),
                                  vertex_bytes));
        GLINTFX_CHECK(bytes_equal(gl_state.storage[stream.index_buffer], batch.indices().data(),
                                  index_bytes));
        GLINTFX_CHECK(stream.vertex_capacity_bytes >= vertex_bytes);
        GLINTFX_CHECK(stream.index_capacity_bytes >= index_bytes);
        if (technique == vertex_upload_technique::orphan_and_sub_data) {
            GLINTFX_CHECK_EQ(gl_state.orphan_calls[stream.vertex_buffer], 1);
            GLINTFX_CHECK_EQ(gl_state.orphan_calls[stream.index_buffer], 1);
            GLINTFX_CHECK_EQ(gl_state.map_calls[stream.vertex_buffer], 0);
        } else {
            GLINTFX_CHECK_EQ(gl_state.map_calls[stream.vertex_buffer], 1);
            GLINTFX_CHECK_EQ(gl_state.map_calls[stream.index_buffer], 1);
            GLINTFX_CHECK_EQ(gl_state.unmap_calls[stream.vertex_buffer], 1);
            GLINTFX_CHECK_EQ(gl_state.unmap_calls[stream.index_buffer], 1);
            // the mapping asks for WRITE and INVALIDATE_BUFFER, and nothing that reads
            GLINTFX_CHECK_EQ(gl_state.last_map_access[stream.vertex_buffer],
                             k_map_write_bit | k_map_invalidate_buffer_bit);
        }
    }
}

// The orphan technique orphans on EVERY upload (that is the point of it); the storage grows when
// the batch no longer fits and never shrinks.
GLINTFX_TEST(vertex_stream_growth_doubles_and_never_shrinks) {
    reset();
    const gl_function_table table = fake_table();
    vertex_stream stream = create_vertex_stream(table).value();
    triangle_batch small;
    fill(small, 1, false);
    GLINTFX_CHECK(upload_batch(table, stream, vertex_upload_technique::orphan_and_sub_data, small)
                      .has_value());
    const std::size_t small_capacity = stream.vertex_capacity_bytes;
    GLINTFX_CHECK(small_capacity >= std::size_t{128});
    GLINTFX_CHECK(upload_batch(table, stream, vertex_upload_technique::orphan_and_sub_data, small)
                      .has_value());
    GLINTFX_CHECK_EQ(gl_state.orphan_calls[stream.vertex_buffer], 2);
    GLINTFX_CHECK_EQ(stream.vertex_capacity_bytes, small_capacity);

    triangle_batch big;
    fill(big, 200, false);
    GLINTFX_CHECK(upload_batch(table, stream, vertex_upload_technique::map_range_invalidate, big)
                      .has_value());
    GLINTFX_CHECK(stream.vertex_capacity_bytes >= std::size_t{25600});
    GLINTFX_CHECK(bytes_equal(gl_state.storage[stream.vertex_buffer], big.vertices().data(),
                              big.vertices().size() * sizeof(batch_vertex)));
    const std::size_t big_capacity = stream.vertex_capacity_bytes;
    GLINTFX_CHECK(upload_batch(table, stream, vertex_upload_technique::map_range_invalidate, small)
                      .has_value());
    GLINTFX_CHECK_EQ(stream.vertex_capacity_bytes, big_capacity);

    // Growth DOUBLES: from a capacity of 2 quads (256 bytes), a batch of 3 quads (384 bytes) asks
    // for 512, not for the exact 384 (an exact fit would reallocate on every piece added).
    reset();
    const gl_function_table table2 = fake_table();
    vertex_stream doubling = create_vertex_stream(table2).value();
    triangle_batch two;
    fill(two, 2, false);
    GLINTFX_CHECK(upload_batch(table2, doubling, vertex_upload_technique::orphan_and_sub_data, two)
                      .has_value());
    GLINTFX_CHECK_EQ(doubling.vertex_capacity_bytes, std::size_t{256});
    triangle_batch three;
    fill(three, 3, false);
    GLINTFX_CHECK(
        upload_batch(table2, doubling, vertex_upload_technique::orphan_and_sub_data, three)
            .has_value());
    GLINTFX_CHECK_EQ(doubling.vertex_capacity_bytes, std::size_t{512});
}

GLINTFX_TEST(vertex_stream_an_empty_batch_sends_nothing_and_calls_no_gl_function) {
    reset();
    const gl_function_table table = fake_table();
    vertex_stream stream = create_vertex_stream(table).value();
    const int calls_before = gl_state.gl_calls;
    triangle_batch empty;
    for (const vertex_upload_technique technique :
         {vertex_upload_technique::orphan_and_sub_data,
          vertex_upload_technique::map_range_invalidate}) {
        GLINTFX_CHECK(upload_batch(table, stream, technique, empty).has_value());
    }
    GLINTFX_CHECK_EQ(gl_state.gl_calls, calls_before);
    GLINTFX_CHECK_EQ(draw_batch(table, empty).value(), std::uint64_t{0});
    GLINTFX_CHECK_EQ(gl_state.draws.size(), std::size_t{0});
}

GLINTFX_TEST(vertex_stream_upload_failures_carry_the_token_of_the_step) {
    // (a) a GL error raised while the vertex buffer was sent: out_of_memory maps to out_of_memory
    // and any
    //     other code to platform_failure; the token is vertex_upload and the GL code rides in
    //     os_error_code
    for (const vertex_upload_technique technique :
         {vertex_upload_technique::orphan_and_sub_data,
          vertex_upload_technique::map_range_invalidate}) {
        for (const GLenum raised : {GLenum{0x0505}, GLenum{0x0502}}) {
            reset();
            const gl_function_table table = fake_table();
            vertex_stream stream = create_vertex_stream(table).value();
            triangle_batch batch;
            fill(batch, 2, false);
            gl_state.buffer_data_calls = 0;
            gl_state.error_from_first_buffer_data = raised;
            auto failed = upload_batch(table, stream, technique, batch);
            GLINTFX_CHECK(failed.has_error());
            GLINTFX_CHECK(failed.err().code() == (raised == 0x0505
                                                      ? gltfx_err_code::out_of_memory
                                                      : gltfx_err_code::platform_failure));
            GLINTFX_CHECK(failed.err().rejected_value() == "vertex_upload");
            GLINTFX_CHECK_EQ(failed.err().os_error_code(), static_cast<std::int64_t>(raised));
        }
    }
    // (a2) the same for the INDEX buffer: its own token, index_upload
    {
        reset();
        const gl_function_table table = fake_table();
        vertex_stream stream = create_vertex_stream(table).value();
        triangle_batch batch;
        fill(batch, 2, false);
        gl_state.buffer_data_calls = 0;
        gl_state.error_from_second_buffer_data = 0x0502; // GL_INVALID_OPERATION
        auto failed =
            upload_batch(table, stream, vertex_upload_technique::orphan_and_sub_data, batch);
        GLINTFX_CHECK(failed.has_error());
        GLINTFX_CHECK(failed.err().code() == gltfx_err_code::platform_failure);
        GLINTFX_CHECK(failed.err().rejected_value() == "index_upload");
        GLINTFX_CHECK_EQ(failed.err().os_error_code(), std::int64_t{0x0502});
    }
    // (a3) a queue holding BOTH an ordinary error and out-of-memory is out_of_memory (the worse one
    // wins),
    //      whichever comes first
    {
        reset();
        const gl_function_table table = fake_table();
        vertex_stream stream = create_vertex_stream(table).value();
        triangle_batch batch;
        fill(batch, 2, false);
        gl_state.buffer_data_calls = 0;
        gl_state.error_from_first_buffer_data = 0x0502;
        gl_state.also_out_of_memory_from_first_buffer_data = true;
        auto failed =
            upload_batch(table, stream, vertex_upload_technique::orphan_and_sub_data, batch);
        GLINTFX_CHECK(failed.has_error());
        GLINTFX_CHECK(failed.err().code() == gltfx_err_code::out_of_memory);
        GLINTFX_CHECK_EQ(failed.err().os_error_code(), std::int64_t{0x0505});
    }
    // (b) a mapping that came back null
    reset();
    const gl_function_table table = fake_table();
    vertex_stream stream = create_vertex_stream(table).value();
    triangle_batch batch;
    fill(batch, 2, false);
    gl_state.map_returns_null = true;
    auto null_map =
        upload_batch(table, stream, vertex_upload_technique::map_range_invalidate, batch);
    GLINTFX_CHECK(null_map.has_error());
    GLINTFX_CHECK(null_map.err().rejected_value() == "vertex_upload");
    GLINTFX_CHECK_EQ(null_map.err().os_error_code(), std::int64_t{0}); // no GL error: no GL code
    GLINTFX_CHECK_EQ(gl_state.unmap_calls[stream.vertex_buffer], 0);   // nothing to unmap
    // (c) a storage the driver says was corrupted (glUnmapBuffer answers false)
    reset();
    const gl_function_table table2 = fake_table();
    vertex_stream stream2 = create_vertex_stream(table2).value();
    gl_state.unmap_reports_corruption = true;
    auto corrupted =
        upload_batch(table2, stream2, vertex_upload_technique::map_range_invalidate, batch);
    GLINTFX_CHECK(corrupted.has_error());
    GLINTFX_CHECK(corrupted.err().code() == gltfx_err_code::platform_failure);
    GLINTFX_CHECK(corrupted.err().rejected_value() == "vertex_upload");
}

// Draw: one call per RUN, in order, with the offset of the run's first index in bytes.
GLINTFX_TEST(vertex_stream_draws_one_call_per_run_with_the_run_offsets) {
    reset();
    const gl_function_table table = fake_table();
    triangle_batch batch;
    fill(batch, 5, true); // states alternate a b a b a: five runs of one piece
    GLINTFX_CHECK_EQ(batch.runs().size(), std::size_t{5});
    GLINTFX_CHECK_EQ(draw_batch(table, batch).value(), std::uint64_t{5});
    GLINTFX_CHECK_EQ(gl_state.draws.size(), std::size_t{5});
    for (std::size_t i = 0; i < 5; ++i) {
        GLINTFX_CHECK_EQ(gl_state.draws[i].mode, k_triangles);
        GLINTFX_CHECK_EQ(gl_state.draws[i].type, k_unsigned_int);
        GLINTFX_CHECK_EQ(gl_state.draws[i].count, 6);
        GLINTFX_CHECK_EQ(gl_state.draws[i].offset, std::uintptr_t{i * 6 * 4});
    }

    reset();
    const gl_function_table table2 = fake_table();
    triangle_batch merged;
    fill(merged, 4, false); // one state: ONE run of 24 indices
    GLINTFX_CHECK_EQ(draw_batch(table2, merged).value(), std::uint64_t{1});
    GLINTFX_CHECK_EQ(gl_state.draws[0].count, 24);
    GLINTFX_CHECK_EQ(gl_state.draws[0].offset, std::uintptr_t{0});
}

GLINTFX_TEST(vertex_stream_destroy_deletes_the_three_objects_and_is_harmless_on_an_empty_one) {
    reset();
    const gl_function_table table = fake_table();
    vertex_stream stream = create_vertex_stream(table).value();
    destroy_vertex_stream(table, stream);
    GLINTFX_CHECK_EQ(gl_state.vertex_arrays_deleted, 1);
    GLINTFX_CHECK_EQ(gl_state.buffers_deleted, 2);
    GLINTFX_CHECK_EQ(stream.vertex_array, GLuint{0});
    GLINTFX_CHECK_EQ(stream.vertex_capacity_bytes, std::size_t{0});
    const int calls = gl_state.gl_calls;
    destroy_vertex_stream(table, stream);
    GLINTFX_CHECK_EQ(gl_state.gl_calls, calls);
    std::println(
        "vertex_stream_test: layout, duas tecnicas, falhas e chamadas de desenho conferidos");
}

// D-B3c-2: what the CONSUMER left in the GL error queue is drained on entry, SAID in the log (the
// event draw2d_prior_gl_error, severity warn, the unsigned field gl_error), and never blamed on us.
GLINTFX_TEST(vertex_stream_the_consumers_prior_errors_are_drained_said_and_not_ours) {
    for (const bool at_upload : {false, true}) {
        reset();
        arm_log();
        const gl_function_table table = fake_table();
        vertex_stream stream;
        if (at_upload) {
            stream = create_vertex_stream(table).value();
            captured.clear();
        }
        gl_state.error_queue = {0x0500,
                                0x0502}; // GL_INVALID_ENUM, GL_INVALID_OPERATION: the consumer's
        if (at_upload) {
            triangle_batch batch;
            fill(batch, 1, false);
            auto sent =
                upload_batch(table, stream, vertex_upload_technique::orphan_and_sub_data, batch);
            GLINTFX_CHECK(sent.has_value()); // their errors are not our failure
        } else {
            auto created = create_vertex_stream(table);
            GLINTFX_CHECK(created.has_value());
        }
        GLINTFX_CHECK_EQ(captured.size(), std::size_t{2});
        if (captured.size() == 2) {
            GLINTFX_CHECK(captured[0].name == "draw2d_prior_gl_error");
            GLINTFX_CHECK(captured[0].category == "draw2d");
            GLINTFX_CHECK(captured[0].severity == gltfx_log_severity::warn);
            GLINTFX_CHECK(captured[0].field_name == "gl_error");
            GLINTFX_CHECK_EQ(captured[0].field_value, std::uint64_t{0x0500});
            GLINTFX_CHECK_EQ(captured[1].field_value, std::uint64_t{0x0502});
        }
        GLINTFX_CHECK(gl_state.error_queue.empty()); // and the queue is empty for whoever asks next
        disarm_log();
    }
}

// The drain stops at the limit when the GL never empties its queue (a broken GL): 16 reads, 16
// events, and the operation then fails as ours (the queue is still not empty afterwards).
GLINTFX_TEST(vertex_stream_the_drain_stops_at_the_limit_when_the_queue_never_empties) {
    reset();
    arm_log();
    gl_state.endless_error = 0x0500;
    const gl_function_table table = fake_table();
    auto created = create_vertex_stream(table);
    GLINTFX_CHECK(created.has_error());
    GLINTFX_CHECK_EQ(captured.size(), std::size_t{16});
    // 16 reads to drain + at most 16 to check the operation: bounded, never a loop that does not
    // end
    GLINTFX_CHECK(gl_state.get_error_calls <= 32);
    disarm_log();
}

// An empty queue says nothing: no event on the ordinary path.
GLINTFX_TEST(vertex_stream_a_clean_queue_emits_no_event) {
    reset();
    arm_log();
    const gl_function_table table = fake_table();
    vertex_stream stream = create_vertex_stream(table).value();
    triangle_batch batch;
    fill(batch, 2, false);
    GLINTFX_CHECK(upload_batch(table, stream, vertex_upload_technique::orphan_and_sub_data, batch)
                      .has_value());
    GLINTFX_CHECK_EQ(captured.size(), std::size_t{0});
    disarm_log();
}

// CTO (revisao da B3c): a glBufferData that fails with OUT_OF_MEMORY must not leave a capacity the
// buffer does not have. After the failure, a batch that fits the RECORDED capacity but not the real
// store has to upload (map_range), not fail forever.
GLINTFX_TEST(vertex_stream_capacity_is_not_trusted_after_a_failed_growth) {
    reset();
    const gl_function_table table = fake_table();
    vertex_stream stream = create_vertex_stream(table).value();
    triangle_batch small;
    fill(small, 1, false);
    GLINTFX_CHECK(upload_batch(table, stream, vertex_upload_technique::map_range_invalidate, small)
                      .has_value());
    triangle_batch big;
    fill(big, 200, false);
    gl_state.ignored_buffer_data_call = gl_state.buffer_data_calls + 1;
    auto failed = upload_batch(table, stream, vertex_upload_technique::map_range_invalidate, big);
    GLINTFX_CHECK(failed.has_error());
    GLINTFX_CHECK(failed.has_error() && failed.err().code() == gltfx_err_code::out_of_memory);
    triangle_batch medium;
    fill(medium, 20, false);
    auto after = upload_batch(table, stream, vertex_upload_technique::map_range_invalidate, medium);
    GLINTFX_CHECK(after.has_value());
    GLINTFX_CHECK(bytes_equal(gl_state.storage[stream.vertex_buffer], medium.vertices().data(),
                              medium.vertices().size() * sizeof(batch_vertex)));
}

// D-B3c-2 / E4: the GL errors the draw calls left are read and mapped, with the token "draw" and
// the GL code in os_error_code(); an empty batch reads nothing.
GLINTFX_TEST(vertex_stream_draw_reads_the_gl_errors_and_maps_them_with_the_token_draw) {
    for (const GLenum raised : {GLenum{0x0505}, GLenum{0x0502}}) {
        reset();
        const gl_function_table table = fake_table();
        triangle_batch batch;
        fill(batch, 3, false);
        gl_state.error_from_draw = raised;
        auto failed = draw_batch(table, batch);
        GLINTFX_CHECK(failed.has_error());
        GLINTFX_CHECK(failed.err().code() == (raised == 0x0505 ? gltfx_err_code::out_of_memory
                                                               : gltfx_err_code::platform_failure));
        GLINTFX_CHECK(failed.err().rejected_value() == "draw");
        GLINTFX_CHECK_EQ(failed.err().os_error_code(), static_cast<std::int64_t>(raised));
        GLINTFX_CHECK_EQ(gl_state.draws.size(), std::size_t{1}); // the call WAS made
    }
    reset();
    const gl_function_table table = fake_table();
    triangle_batch empty;
    gl_state.error_from_draw = 0x0502; // would be raised by a draw: there is none
    auto nothing = draw_batch(table, empty);
    GLINTFX_CHECK(nothing.has_value());
    GLINTFX_CHECK_EQ(gl_state.get_error_calls, 0); // and nothing was read
}

// The twin for the INDEX buffer (L-17): its growth fails with OUT_OF_MEMORY while the vertex
// buffer's succeeded; the next batch that fits the recorded index capacity must still upload.
GLINTFX_TEST(vertex_stream_index_capacity_is_not_trusted_after_a_failed_growth) {
    reset();
    const gl_function_table table = fake_table();
    vertex_stream stream = create_vertex_stream(table).value();
    triangle_batch small;
    fill(small, 1, false);
    GLINTFX_CHECK(upload_batch(table, stream, vertex_upload_technique::map_range_invalidate, small)
                      .has_value());
    triangle_batch big;
    fill(big, 200, false);
    gl_state.ignored_buffer_data_call = gl_state.buffer_data_calls + 2; // the index glBufferData
    auto failed = upload_batch(table, stream, vertex_upload_technique::map_range_invalidate, big);
    GLINTFX_CHECK(failed.has_error() && failed.err().code() == gltfx_err_code::out_of_memory &&
                  failed.err().rejected_value() == "index_upload");
    triangle_batch medium;
    fill(medium, 20, false);
    auto after = upload_batch(table, stream, vertex_upload_technique::map_range_invalidate, medium);
    GLINTFX_CHECK(after.has_value());
    GLINTFX_CHECK(bytes_equal(gl_state.storage[stream.index_buffer], medium.indices().data(),
                              medium.indices().size() * sizeof(std::uint32_t)));
}
