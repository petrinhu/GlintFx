// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <print>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <glintfx/core/color.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/core/log/event.hpp>
#include <glintfx/core/log/sink.hpp>
#include <glintfx/core/segment.hpp>
#include <glintfx/draw2d/stroke.hpp>

#include "draw2d/renderer_2d_impl.hpp"
#include "draw2d/texture_2d_impl.hpp"
#include "draw2d/texture_release.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// renderer_2d_impl_test.cpp - R2D-BATCH, fatia B4. Proves the frame machine behind the public
// handle (renderer_2d_impl.hpp): what begin_frame/fill_*/flush/finish_frame do, in what order, and
// what the report and the error say - against a FAKE host and a table of FAKE GL functions that
// keep the bytes sent to the vertex buffer, so the paint order and the transform can be read back.
//
// A DECLARED DOWNGRADE (GODS_LAWS.md L-09): no window, no driver. The real open() (which builds the
// host from a gltfx_gl_context) and what a real card does with the frame are the live cell of the
// parity test (B5). The literal GL enumerators are the registry's, never the constants of the code.

// S2b-fix2 (F-1 of the N1 re-review): the destructor is private, so only
// renderer_2d_impl::destroy() deletes the interior; destroy() makes the context current and deletes
// the GL objects first.
static_assert(!std::is_destructible_v<glintfx::renderer_2d_impl>,
              "renderer_2d_impl is deleted only by renderer_2d_impl::destroy()");

using glintfx::gltfx_err_code;
using glintfx::gltfx_frame_2d_desc;
using glintfx::gltfx_rgba;
using glintfx::renderer_2d_host;
using glintfx::renderer_2d_impl;
using glintfx::renderer_2d_options;
using glintfx::render::gl_function_table;
using glintfx::render::GLbitfield;
using glintfx::render::GLboolean;
using glintfx::render::GLchar;
using glintfx::render::GLenum;
using glintfx::render::GLfloat;
using glintfx::render::GLint;
using glintfx::render::GLintptr;
using glintfx::render::GLsizei;
using glintfx::render::GLsizeiptr;
using glintfx::render::GLuint;

namespace {

constexpr GLenum k_array_buffer = 0x8892;
constexpr GLenum k_out_of_memory = 0x0505;
constexpr GLenum k_invalid_operation = 0x0502;
constexpr GLint k_viewport_location = 11;
constexpr GLint k_encode_location = 12;

struct fake_card {
    GLuint next_id = 1;
    GLuint bound_array_buffer = 0;
    std::vector<GLenum> error_queue;
    GLenum error_from_draw = 0;
    bool fail_compile = false;
    int objects_created = 0;
    int objects_deleted = 0;
    std::vector<std::string> calls; // "clear", "upload", "draw", "use_program" in order
    std::array<float, 4> clear_color{};
    std::vector<unsigned char> vertex_bytes; // the last sub-data sent to the array buffer
    std::vector<int> draw_counts;
    std::pair<float, float> viewport_uniform{-1.0F, -1.0F};
    int encode_uniform = -1;
    int gl_calls = 0;
    GLuint current_program = 0;
    GLuint current_vao = 0;
    GLuint program_at_draw = 0;
    std::array<int, 65536> cap_state{}; // 0 unknown, 1 enabled, 2 disabled, by enumerator
    std::array<GLint, 4> viewport{-1, -1, -1, -1};
};
fake_card card;

struct fake_context {
    bool fail_current = false;
    int current_calls = 0;
    std::uint32_t width = 800;
    std::uint32_t height = 600;
    int calls_seen_at_failure = -1;
};
fake_context context;

void reset() {
    card = fake_card{};
    context = fake_context{};
}

glintfx::gltfx_rslt<void> fake_make_current(void *) noexcept {
    ++context.current_calls;
    if (context.fail_current) {
        context.calls_seen_at_failure = card.gl_calls;
        return glintfx::gltfx_rslt<void>::err(
            glintfx::gltfx_err(gltfx_err_code::platform_failure).with_rejected_value("context"));
    }
    return glintfx::gltfx_rslt<void>::ok();
}
std::pair<std::uint32_t, std::uint32_t> fake_surface_size(void *) noexcept {
    return {context.width, context.height};
}
renderer_2d_host fake_host() {
    return renderer_2d_host{&context, &fake_make_current, &fake_surface_size};
}

// Every table function counts itself; only those the frame machine reads back do more.
#define GLINTFX_COUNT() ++card.gl_calls
GLuint GLINTFX_GL_APIENTRY f_create(GLenum) {
    GLINTFX_COUNT();
    ++card.objects_created;
    return card.next_id++;
}
GLuint GLINTFX_GL_APIENTRY f_create_program() {
    GLINTFX_COUNT();
    ++card.objects_created;
    return card.next_id++;
}
void GLINTFX_GL_APIENTRY f_shader_source(GLuint, GLsizei, const GLchar *const *, const GLint *) {
    GLINTFX_COUNT();
}
void GLINTFX_GL_APIENTRY f_one(GLuint) { GLINTFX_COUNT(); }
void GLINTFX_GL_APIENTRY f_delete(GLuint) {
    GLINTFX_COUNT();
    ++card.objects_deleted;
}
void GLINTFX_GL_APIENTRY f_attach(GLuint, GLuint) { GLINTFX_COUNT(); }
void GLINTFX_GL_APIENTRY f_shaderiv(GLuint, GLenum pname, GLint *params) {
    GLINTFX_COUNT();
    *params = pname == 0x8B81 ? (card.fail_compile ? 0 : 1) : 1;
}
void GLINTFX_GL_APIENTRY f_get_log(GLuint, GLsizei, GLsizei *length, GLchar *) {
    GLINTFX_COUNT();
    *length = 0;
}
void GLINTFX_GL_APIENTRY f_programiv(GLuint, GLenum, GLint *params) {
    GLINTFX_COUNT();
    *params = 1;
}
GLint GLINTFX_GL_APIENTRY f_uniform_location(GLuint, const GLchar *name) {
    GLINTFX_COUNT();
    return std::strcmp(name, "u_encode_srgb") == 0 ? k_encode_location : k_viewport_location;
}
void GLINTFX_GL_APIENTRY f_gen(GLsizei n, GLuint *ids) {
    GLINTFX_COUNT();
    for (GLsizei i = 0; i < n; ++i) {
        ids[i] = card.next_id++;
        ++card.objects_created;
    }
}
void GLINTFX_GL_APIENTRY f_delete_many(GLsizei n, const GLuint *) {
    GLINTFX_COUNT();
    card.objects_deleted += n;
}
void GLINTFX_GL_APIENTRY f_bind_buffer(GLenum target, GLuint id) {
    GLINTFX_COUNT();
    if (target == k_array_buffer) {
        card.bound_array_buffer = id;
    }
}
void GLINTFX_GL_APIENTRY f_bind_array(GLuint id) {
    GLINTFX_COUNT();
    card.current_vao = id;
}
void GLINTFX_GL_APIENTRY f_use_program(GLuint id) {
    GLINTFX_COUNT();
    card.current_program = id;
}
void GLINTFX_GL_APIENTRY f_buffer_data(GLenum, GLsizeiptr, const void *, GLenum) {
    GLINTFX_COUNT();
}
void GLINTFX_GL_APIENTRY f_sub_data(GLenum target, GLintptr, GLsizeiptr size, const void *data) {
    GLINTFX_COUNT();
    if (target == k_array_buffer) {
        card.calls.emplace_back("upload");
        card.vertex_bytes.assign(static_cast<const unsigned char *>(data),
                                 static_cast<const unsigned char *>(data) + size);
    }
}
void GLINTFX_GL_APIENTRY f_enable_attrib(GLuint) { GLINTFX_COUNT(); }
void GLINTFX_GL_APIENTRY f_attrib_pointer(GLuint, GLint, GLenum, GLboolean, GLsizei, const void *) {
    GLINTFX_COUNT();
}
void GLINTFX_GL_APIENTRY f_draw(GLenum, GLsizei count, GLenum, const void *) {
    GLINTFX_COUNT();
    card.calls.emplace_back("draw");
    card.program_at_draw = card.current_program;
    card.draw_counts.push_back(count);
    if (card.error_from_draw != 0) {
        card.error_queue.push_back(card.error_from_draw);
    }
}
GLenum GLINTFX_GL_APIENTRY f_get_error() {
    GLINTFX_COUNT();
    if (card.error_queue.empty()) {
        return 0;
    }
    const GLenum first = card.error_queue.front();
    card.error_queue.erase(card.error_queue.begin());
    return first;
}
void GLINTFX_GL_APIENTRY f_clear_color(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
    GLINTFX_COUNT();
    card.clear_color = {r, g, b, a};
}
void GLINTFX_GL_APIENTRY f_clear(GLbitfield) {
    GLINTFX_COUNT();
    card.calls.emplace_back("clear");
}
void GLINTFX_GL_APIENTRY f_uniform2f(GLint location, GLfloat x, GLfloat y) {
    GLINTFX_COUNT();
    if (location == k_viewport_location) {
        card.viewport_uniform = {x, y};
    }
}
void GLINTFX_GL_APIENTRY f_uniform1i(GLint location, GLint value) {
    GLINTFX_COUNT();
    if (location == k_encode_location) {
        card.encode_uniform = value;
    }
}
void GLINTFX_GL_APIENTRY f_enum(GLenum) { GLINTFX_COUNT(); }
void GLINTFX_GL_APIENTRY f_enable(GLenum cap) {
    GLINTFX_COUNT();
    card.cap_state.at(cap) = 1;
}
void GLINTFX_GL_APIENTRY f_disable(GLenum cap) {
    GLINTFX_COUNT();
    card.cap_state.at(cap) = 2;
}
void GLINTFX_GL_APIENTRY f_enum_uint(GLenum, GLuint) { GLINTFX_COUNT(); }
void GLINTFX_GL_APIENTRY f_enum_enum(GLenum, GLenum) { GLINTFX_COUNT(); }
void GLINTFX_GL_APIENTRY f_blend4(GLenum, GLenum, GLenum, GLenum) { GLINTFX_COUNT(); }
void GLINTFX_GL_APIENTRY f_color_mask(GLboolean, GLboolean, GLboolean, GLboolean) {
    GLINTFX_COUNT();
}
void GLINTFX_GL_APIENTRY f_pixel_store(GLenum, GLint) { GLINTFX_COUNT(); }
void GLINTFX_GL_APIENTRY f_viewport(GLint x, GLint y, GLsizei w, GLsizei h) {
    GLINTFX_COUNT();
    card.viewport = {x, y, w, h};
}

gl_function_table fake_table() {
    gl_function_table t;
    t.glCreateShader = &f_create;
    t.glCreateProgram = &f_create_program;
    t.glShaderSource = &f_shader_source;
    t.glCompileShader = &f_one;
    t.glGetShaderiv = &f_shaderiv;
    t.glGetShaderInfoLog = &f_get_log;
    t.glDeleteShader = &f_delete;
    t.glAttachShader = &f_attach;
    t.glDetachShader = &f_attach;
    t.glLinkProgram = &f_one;
    t.glGetProgramiv = &f_programiv;
    t.glGetProgramInfoLog = &f_get_log;
    t.glDeleteProgram = &f_delete;
    t.glGetUniformLocation = &f_uniform_location;
    t.glGenVertexArrays = &f_gen;
    t.glGenBuffers = &f_gen;
    t.glDeleteVertexArrays = &f_delete_many;
    t.glDeleteBuffers = &f_delete_many;
    t.glBindVertexArray = &f_bind_array;
    t.glBindBuffer = &f_bind_buffer;
    t.glBufferData = &f_buffer_data;
    t.glBufferSubData = &f_sub_data;
    t.glEnableVertexAttribArray = &f_enable_attrib;
    t.glVertexAttribPointer = &f_attrib_pointer;
    t.glDrawElements = &f_draw;
    t.glGetError = &f_get_error;
    t.glClearColor = &f_clear_color;
    t.glClear = &f_clear;
    t.glUniform2f = &f_uniform2f;
    t.glUniform1i = &f_uniform1i;
    t.glUseProgram = &f_use_program;
    t.glActiveTexture = &f_enum;
    t.glBindFramebuffer = &f_enum_uint;
    t.glBindTexture = &f_enum_uint;
    t.glBlendEquationSeparate = &f_enum_enum;
    t.glBlendFuncSeparate = &f_blend4;
    t.glColorMask = &f_color_mask;
    t.glDisable = &f_disable;
    t.glEnable = &f_enable;
    t.glPixelStorei = &f_pixel_store;
    t.glPolygonMode = &f_enum_enum;
    t.glViewport = &f_viewport;
    return t;
}

// The renderer keeps its own copy of the table, so the local one may go away after create().
renderer_2d_impl *make(bool srgb, std::size_t reserve = 16) {
    const gl_function_table table = fake_table();
    auto created = renderer_2d_impl::create(fake_host(), table, renderer_2d_options{srgb, reserve});
    GLINTFX_CHECK(created.has_value());
    return created.has_value() ? created.value() : nullptr;
}

// A check must FAIL, never crash, when nothing was drawn: the first draw count, or -1.
int first_draw_count() { return card.draw_counts.empty() ? -1 : card.draw_counts.front(); }
std::string first_call() { return card.calls.empty() ? std::string() : card.calls.front(); }

// The public rule of the report: every submitted piece ends in exactly one of four places.
bool ends_add_up(const glintfx::gltfx_frame_2d_report &r) {
    return r.pieces_submitted == r.pieces_drawn + r.pieces_refused +
                                     r.pieces_dropped_out_of_memory +
                                     r.pieces_dropped_graphics_failure;
}

glintfx::gltfx_rect_world rect(double x, double y, double w, double h) {
    return glintfx::gltfx_rect_world{{x, y}, {w, h}};
}

constexpr gltfx_rgba k_red{1.0F, 0.0F, 0.0F, 1.0F};
constexpr gltfx_rgba k_blue{0.0F, 0.0F, 1.0F, 1.0F};
constexpr glintfx::gltfx_draw_layer k_layer_0{0};

// The first vertex of the k-th piece drawn: {x, y, u, v, r, g, b, a} as floats, 4 vertices a piece.
float sent(std::size_t piece, std::size_t field) {
    float value = -12345.0F;
    const std::size_t at = (piece * 4 * 8 + field) * sizeof(float);
    if (at + sizeof(float) <= card.vertex_bytes.size()) {
        std::memcpy(&value, card.vertex_bytes.data() + at, sizeof(float));
    }
    return value;
}

bool deny_growth = false;
void *denying_reallocate(void *block, std::size_t bytes) noexcept {
    return deny_growth ? nullptr : std::realloc(block, bytes);
}
void plain_release(void *block) noexcept { std::free(block); }

// Counts every growth the renderer asks for (S2b-fix1, M9): a call that must store nothing is
// proved by this count staying where it was.
std::size_t reallocations = 0;
void *counting_reallocate(void *block, std::size_t bytes) noexcept {
    ++reallocations;
    return std::realloc(block, bytes);
}

// The k-th vertex sent, field f of {x, y, u, v, r, g, b, a}: a triangle piece has no fixed four.
float sent_vertex(std::size_t vertex, std::size_t field) { return sent(0, vertex * 8 + field); }

glintfx::gltfx_vertex_2d_world corner_at(double x, double y) {
    return glintfx::gltfx_vertex_2d_world{{x, y}, k_red};
}

} // namespace

GLINTFX_TEST(renderer_2d_impl_one_rect_is_one_draw_call_and_one_piece_in_the_report) {
    reset();
    renderer_2d_impl *renderer = make(true);
    renderer->begin_frame(gltfx_frame_2d_desc{});
    renderer->fill_rect(rect(10, 20, 30, 40), k_red, k_layer_0);
    const auto report = renderer->finish_frame();
    GLINTFX_CHECK(report.has_value());
    GLINTFX_CHECK_EQ(report.value().pieces_submitted, std::uint64_t{1});
    GLINTFX_CHECK_EQ(report.value().pieces_drawn, std::uint64_t{1});
    GLINTFX_CHECK_EQ(report.value().draw_calls, std::uint64_t{1});
    GLINTFX_CHECK_EQ(card.draw_counts.size(), std::size_t{1});
    GLINTFX_CHECK_EQ(first_draw_count(), 6);
    GLINTFX_CHECK_EQ(sent(0, 0), 10.0F); // top-left x
    GLINTFX_CHECK_EQ(sent(0, 1), 20.0F); // top-left y
    renderer_2d_impl::destroy(renderer);
}

GLINTFX_TEST(renderer_2d_impl_the_paint_order_is_layer_then_submission) {
    reset();
    renderer_2d_impl *renderer = make(true);
    renderer->begin_frame(gltfx_frame_2d_desc{});
    renderer->fill_rect(rect(0, 0, 1, 1), k_red, glintfx::gltfx_draw_layer{5});
    renderer->fill_rect(rect(0, 0, 1, 1), k_blue, glintfx::gltfx_draw_layer{1});
    GLINTFX_CHECK(renderer->finish_frame().has_value());
    GLINTFX_CHECK_EQ(sent(0, 6), 1.0F); // the first piece painted is blue: its b
    GLINTFX_CHECK_EQ(sent(0, 4), 0.0F);
    GLINTFX_CHECK_EQ(sent(1, 4), 1.0F); // the second is red
    renderer_2d_impl::destroy(renderer);
}

GLINTFX_TEST(renderer_2d_impl_the_order_never_crosses_a_flush) {
    reset();
    renderer_2d_impl *renderer = make(true);
    renderer->begin_frame(gltfx_frame_2d_desc{});
    renderer->fill_rect(rect(0, 0, 1, 1), k_red, glintfx::gltfx_draw_layer{5});
    renderer->flush();
    renderer->fill_rect(rect(0, 0, 1, 1), k_blue, glintfx::gltfx_draw_layer{1});
    const auto report = renderer->finish_frame();
    GLINTFX_CHECK(report.has_value());
    GLINTFX_CHECK_EQ(report.value().flushes, std::uint64_t{1});
    GLINTFX_CHECK_EQ(report.value().draw_calls, std::uint64_t{2});
    GLINTFX_CHECK_EQ(sent(0, 6), 1.0F); // the LAST upload (after the flush) is the blue piece
    renderer_2d_impl::destroy(renderer);
}

GLINTFX_TEST(renderer_2d_impl_the_transform_is_applied_when_the_piece_is_submitted) {
    reset();
    renderer_2d_impl *renderer = make(true);
    renderer->begin_frame(gltfx_frame_2d_desc{});
    renderer->begin_batch(glintfx::gltfx_transform{
        .translation = {.x = 100.0, .y = 0.0},
        .rotation = {.radians = 0.0},
        .scale = {.x = 2.0, .y = 2.0},
    });
    renderer->fill_rect(rect(1, 1, 1, 1), k_red, k_layer_0);
    renderer->begin_batch(); // changing batch afterwards moves nothing already submitted
    const auto report = renderer->finish_frame();
    GLINTFX_CHECK(report.has_value());
    GLINTFX_CHECK_EQ(report.value().batches,
                     std::uint64_t{3}); // the pixel-direct one and the two begun
    GLINTFX_CHECK_EQ(sent(0, 0), 102.0F);
    GLINTFX_CHECK_EQ(sent(0, 1), 2.0F);
    renderer_2d_impl::destroy(renderer);
}

GLINTFX_TEST(renderer_2d_impl_every_frame_starts_in_the_pixel_direct_batch) {
    reset();
    renderer_2d_impl *renderer = make(true);
    renderer->begin_frame(gltfx_frame_2d_desc{});
    renderer->begin_batch(glintfx::gltfx_transform{
        .translation = {.x = 100.0, .y = 0.0},
        .rotation = {.radians = 0.0},
        .scale = {.x = 1.0, .y = 1.0},
    });
    GLINTFX_CHECK(renderer->finish_frame().has_value());
    renderer->begin_frame(gltfx_frame_2d_desc{});
    renderer->fill_rect(rect(1, 1, 1, 1), k_red, k_layer_0);
    GLINTFX_CHECK(renderer->finish_frame().has_value());
    GLINTFX_CHECK_EQ(sent(0, 0), 1.0F);
    renderer_2d_impl::destroy(renderer);
}

GLINTFX_TEST(renderer_2d_impl_a_refused_piece_is_counted_and_the_frame_is_still_ok) {
    reset();
    renderer_2d_impl *renderer = make(true);
    renderer->begin_frame(gltfx_frame_2d_desc{});
    renderer->fill_rect(rect(std::numeric_limits<double>::quiet_NaN(), 0, 1, 1), k_red, k_layer_0);
    renderer->fill_rect(rect(0, 0, -1, 1), k_red, k_layer_0);
    const auto report = renderer->finish_frame();
    GLINTFX_CHECK(report.has_value());
    GLINTFX_CHECK_EQ(report.value().pieces_refused, std::uint64_t{2});
    GLINTFX_CHECK(report.value().first_refusal == glintfx::gltfx_draw_2d_refusal::not_a_number);
    GLINTFX_CHECK_EQ(report.value().pieces_drawn, std::uint64_t{0});
    GLINTFX_CHECK(card.draw_counts.empty());
    renderer_2d_impl::destroy(renderer);
}

GLINTFX_TEST(renderer_2d_impl_a_quad_is_two_triangles_like_a_rect) {
    reset();
    renderer_2d_impl *renderer = make(true);
    renderer->begin_frame(gltfx_frame_2d_desc{});
    renderer->fill_quad(glintfx::gltfx_quad_world{{0, 0}, {4, 1}, {5, 5}, {0, 4}}, k_red,
                        k_layer_0);
    GLINTFX_CHECK(renderer->finish_frame().has_value());
    GLINTFX_CHECK_EQ(first_draw_count(), 6);
    GLINTFX_CHECK_EQ(sent(0, 8), 4.0F); // vertex 1 x: the top-right corner, in the order given
    GLINTFX_CHECK_EQ(sent(0, 9), 1.0F);
    renderer_2d_impl::destroy(renderer);
}

GLINTFX_TEST(renderer_2d_impl_finish_frame_with_no_frame_open_is_invalid_argument_frame) {
    reset();
    renderer_2d_impl *renderer = make(true);
    const auto report = renderer->finish_frame();
    GLINTFX_CHECK(report.has_error());
    if (report.has_error()) {
        GLINTFX_CHECK(report.err().code() == gltfx_err_code::invalid_argument);
        GLINTFX_CHECK_EQ(std::string(report.err().rejected_value()), std::string("frame"));
    }
    renderer_2d_impl::destroy(renderer);
}

GLINTFX_TEST(renderer_2d_impl_a_piece_with_no_frame_open_is_counted_outside_the_frame) {
    reset();
    renderer_2d_impl *renderer = make(true);
    renderer->fill_rect(rect(0, 0, 1, 1), k_red, k_layer_0);
    renderer->begin_frame(gltfx_frame_2d_desc{});
    const auto report = renderer->finish_frame();
    GLINTFX_CHECK(report.has_value());
    GLINTFX_CHECK_EQ(report.value().pieces_dropped_outside_frame, std::uint64_t{1});
    GLINTFX_CHECK_EQ(report.value().pieces_submitted, std::uint64_t{0});
    renderer_2d_impl::destroy(renderer);
}

GLINTFX_TEST(renderer_2d_impl_a_frame_begun_over_an_open_one_abandons_it_undrawn) {
    reset();
    renderer_2d_impl *renderer = make(true);
    renderer->begin_frame(gltfx_frame_2d_desc{});
    renderer->fill_rect(rect(0, 0, 1, 1), k_red, k_layer_0);
    renderer->begin_frame(gltfx_frame_2d_desc{});
    const auto report = renderer->finish_frame();
    GLINTFX_CHECK(report.has_value());
    GLINTFX_CHECK_EQ(report.value().frames_abandoned, std::uint64_t{1});
    GLINTFX_CHECK(card.draw_counts.empty());
    renderer_2d_impl::destroy(renderer);
}

GLINTFX_TEST(renderer_2d_impl_the_clear_color_is_written_as_is_with_the_surface_encoding) {
    reset();
    renderer_2d_impl *renderer = make(true);
    renderer->begin_frame(gltfx_frame_2d_desc{gltfx_rgba{0.25F, 0.5F, 0.75F, 1.0F}});
    GLINTFX_CHECK_EQ(card.clear_color[0], 0.25F);
    GLINTFX_CHECK_EQ(card.clear_color[1], 0.5F);
    GLINTFX_CHECK_EQ(card.clear_color[2], 0.75F);
    GLINTFX_CHECK_EQ(first_call(), std::string("clear"));
    GLINTFX_CHECK(renderer->finish_frame().has_value());
    renderer_2d_impl::destroy(renderer);
}

GLINTFX_TEST(renderer_2d_impl_the_clear_color_is_encoded_on_the_cpu_without_the_surface_encoding) {
    reset();
    renderer_2d_impl *renderer = make(false);
    renderer->begin_frame(gltfx_frame_2d_desc{gltfx_rgba{0.5F, 0.0F, 1.0F, 0.5F}});
    // sRGB of linear 0.5 is 0.735357...; 0 and 1 stay; the alpha is never encoded.
    GLINTFX_CHECK(std::fabs(card.clear_color[0] - 0.735357F) < 1.0e-4F);
    GLINTFX_CHECK_EQ(card.clear_color[1], 0.0F);
    GLINTFX_CHECK(std::fabs(card.clear_color[2] - 1.0F) < 1.0e-6F);
    GLINTFX_CHECK_EQ(card.clear_color[3], 0.5F);
    GLINTFX_CHECK(renderer->finish_frame().has_value());
    renderer_2d_impl::destroy(renderer);
}

GLINTFX_TEST(renderer_2d_impl_no_clear_color_means_no_clear) {
    reset();
    renderer_2d_impl *renderer = make(true);
    renderer->begin_frame(gltfx_frame_2d_desc{std::nullopt});
    renderer->fill_rect(rect(0, 0, 1, 1), k_red, k_layer_0);
    GLINTFX_CHECK(renderer->finish_frame().has_value());
    GLINTFX_CHECK_EQ(first_draw_count(), 6); // the frame WAS drawn, only not cleared
    for (const std::string &call : card.calls) {
        GLINTFX_CHECK(call != "clear");
    }
    renderer_2d_impl::destroy(renderer);
}

GLINTFX_TEST(renderer_2d_impl_the_program_gets_the_surface_size_and_the_encoding_switch) {
    reset();
    renderer_2d_impl *renderer = make(false);
    context.width = 1280;
    context.height = 720;
    renderer->begin_frame(gltfx_frame_2d_desc{});
    renderer->fill_rect(rect(0, 0, 1, 1), k_red, k_layer_0);
    GLINTFX_CHECK(renderer->finish_frame().has_value());
    GLINTFX_CHECK_EQ(card.viewport_uniform.first, 1280.0F);
    GLINTFX_CHECK_EQ(card.viewport_uniform.second, 720.0F);
    GLINTFX_CHECK_EQ(card.encode_uniform, 1); // the option is OFF: the shader encodes
    renderer_2d_impl::destroy(renderer);

    reset();
    renderer = make(true);
    renderer->begin_frame(gltfx_frame_2d_desc{});
    renderer->fill_rect(rect(0, 0, 1, 1), k_red, k_layer_0);
    GLINTFX_CHECK(renderer->finish_frame().has_value());
    GLINTFX_CHECK_EQ(card.encode_uniform, 0);
    renderer_2d_impl::destroy(renderer);
}

GLINTFX_TEST(renderer_2d_impl_a_card_out_of_memory_in_the_draw_is_out_of_memory_with_the_token) {
    reset();
    renderer_2d_impl *renderer = make(true);
    renderer->begin_frame(gltfx_frame_2d_desc{});
    renderer->fill_rect(rect(0, 0, 1, 1), k_red, k_layer_0);
    card.error_from_draw = k_out_of_memory;
    const auto report = renderer->finish_frame();
    GLINTFX_CHECK(report.has_error());
    if (report.has_error()) {
        GLINTFX_CHECK(report.err().code() == gltfx_err_code::out_of_memory);
        GLINTFX_CHECK_EQ(std::string(report.err().rejected_value()), std::string("draw"));
    }
    // "When there is an error, the report is still there": the piece was accepted and did NOT reach
    // the card, so it is not counted as drawn (D-B4-1).
    GLINTFX_CHECK_EQ(renderer->last_frame_report().pieces_submitted, std::uint64_t{1});
    GLINTFX_CHECK_EQ(renderer->last_frame_report().pieces_drawn, std::uint64_t{0});
    GLINTFX_CHECK_EQ(renderer->last_frame_report().pieces_dropped_graphics_failure,
                     std::uint64_t{1});
    GLINTFX_CHECK(ends_add_up(renderer->last_frame_report()));
    renderer_2d_impl::destroy(renderer);
}

// The card's own OUT_OF_MEMORY counts in pieces_dropped_graphics_failure;
// pieces_dropped_out_of_memory is the memory of the library itself (errata sec. 19, addendum to
// D-B4-1).
GLINTFX_TEST(renderer_2d_impl_the_cards_out_of_memory_is_counted_as_a_graphics_failure) {
    reset();
    renderer_2d_impl *renderer = make(true);
    renderer->begin_frame(gltfx_frame_2d_desc{});
    renderer->fill_rect(rect(0, 0, 1, 1), k_red, k_layer_0);
    card.error_from_draw = k_out_of_memory;
    const auto report = renderer->finish_frame();
    GLINTFX_CHECK(report.has_error());
    if (report.has_error()) {
        // What the E4 promises: the error says out_of_memory, with the card's own code (0x0505).
        GLINTFX_CHECK(report.err().code() == gltfx_err_code::out_of_memory);
        GLINTFX_CHECK_EQ(report.err().os_error_code(), std::int64_t{k_out_of_memory});
    }
    const auto last = renderer->last_frame_report();
    GLINTFX_CHECK_EQ(last.pieces_dropped_graphics_failure, std::uint64_t{1});
    GLINTFX_CHECK_EQ(last.pieces_dropped_out_of_memory, std::uint64_t{0});
    renderer_2d_impl::destroy(renderer);
}

GLINTFX_TEST(renderer_2d_impl_any_other_card_error_in_the_draw_is_platform_failure_with_its_code) {
    reset();
    renderer_2d_impl *renderer = make(true);
    renderer->begin_frame(gltfx_frame_2d_desc{});
    renderer->fill_rect(rect(0, 0, 1, 1), k_red, k_layer_0);
    card.error_from_draw = k_invalid_operation;
    const auto report = renderer->finish_frame();
    GLINTFX_CHECK(report.has_error());
    if (report.has_error()) {
        GLINTFX_CHECK(report.err().code() == gltfx_err_code::platform_failure);
        GLINTFX_CHECK_EQ(report.err().os_error_code(), std::int64_t{k_invalid_operation});
    }
    renderer_2d_impl::destroy(renderer);
}

GLINTFX_TEST(renderer_2d_impl_a_context_that_cannot_be_made_current_is_the_error_of_finish_frame) {
    reset();
    renderer_2d_impl *renderer = make(true);
    const int calls_before = card.gl_calls;
    context.fail_current = true;
    renderer->begin_frame(gltfx_frame_2d_desc{});
    renderer->fill_rect(rect(0, 0, 1, 1), k_red, k_layer_0);
    const auto report = renderer->finish_frame();
    GLINTFX_CHECK(report.has_error());
    if (report.has_error()) {
        GLINTFX_CHECK(report.err().code() == gltfx_err_code::platform_failure);
        GLINTFX_CHECK_EQ(std::string(report.err().rejected_value()), std::string("context"));
    }
    // With no current context NO GL function is called: none of the frame touched the card.
    GLINTFX_CHECK_EQ(card.gl_calls, calls_before);
    GLINTFX_CHECK(card.draw_counts.empty());
    // The piece was accepted and never drawn: submitted, and no other end.
    GLINTFX_CHECK_EQ(renderer->last_frame_report().pieces_submitted, std::uint64_t{1});
    GLINTFX_CHECK_EQ(renderer->last_frame_report().pieces_drawn, std::uint64_t{0});
    GLINTFX_CHECK_EQ(renderer->last_frame_report().pieces_dropped_graphics_failure,
                     std::uint64_t{1});
    GLINTFX_CHECK(ends_add_up(renderer->last_frame_report()));
    context.fail_current = false;
    renderer_2d_impl::destroy(renderer);
}

GLINTFX_TEST(renderer_2d_impl_the_gl_objects_created_are_all_deleted_by_destroy) {
    reset();
    renderer_2d_impl *renderer = make(true);
    GLINTFX_CHECK(card.objects_created > 0);
    context.current_calls = 0;
    renderer_2d_impl::destroy(renderer);
    GLINTFX_CHECK_EQ(context.current_calls, 1); // the renderer's own context is made current first
    GLINTFX_CHECK_EQ(card.objects_deleted, card.objects_created);
}

GLINTFX_TEST(renderer_2d_impl_destroy_of_null_is_harmless) {
    reset();
    renderer_2d_impl::destroy(nullptr);
    GLINTFX_CHECK_EQ(context.current_calls, 0);
}

GLINTFX_TEST(renderer_2d_impl_a_failed_create_leaves_no_gl_object_alive) {
    reset();
    card.fail_compile = true;
    const gl_function_table table = fake_table();
    const auto created =
        renderer_2d_impl::create(fake_host(), table, renderer_2d_options{true, 16});
    GLINTFX_CHECK(created.has_error());
    GLINTFX_CHECK_EQ(card.objects_deleted, card.objects_created);
}

GLINTFX_TEST(renderer_2d_impl_more_pieces_than_reserved_are_all_drawn) {
    reset();
    renderer_2d_impl *renderer = make(true, 2);
    renderer->begin_frame(gltfx_frame_2d_desc{});
    for (int i = 0; i < 100; ++i) {
        renderer->fill_rect(rect(i, 0, 1, 1), k_red, k_layer_0);
    }
    const auto report = renderer->finish_frame();
    GLINTFX_CHECK(report.has_value());
    GLINTFX_CHECK_EQ(report.value().pieces_drawn, std::uint64_t{100});
    GLINTFX_CHECK_EQ(first_draw_count(), 600);
    renderer_2d_impl::destroy(renderer);
}

GLINTFX_TEST(renderer_2d_impl_the_last_report_is_the_one_finish_frame_returned) {
    reset();
    renderer_2d_impl *renderer = make(true);
    renderer->begin_frame(gltfx_frame_2d_desc{});
    renderer->fill_rect(rect(0, 0, 1, 1), k_red, k_layer_0);
    const auto report = renderer->finish_frame();
    GLINTFX_CHECK(report.has_value());
    GLINTFX_CHECK_EQ(report.value().pieces_drawn, std::uint64_t{1});
    GLINTFX_CHECK_EQ(renderer->last_frame_report().pieces_drawn, report.value().pieces_drawn);
    GLINTFX_CHECK_EQ(renderer->last_frame_report().draw_calls, report.value().draw_calls);
    renderer_2d_impl::destroy(renderer);
}

GLINTFX_TEST(renderer_2d_impl_a_piece_dropped_for_lack_of_memory_is_counted_and_is_the_error) {
    reset();
    deny_growth = false;
    const gl_function_table table = fake_table();
    auto created = renderer_2d_impl::create(
        fake_host(), table,
        renderer_2d_options{true, 1,
                            glintfx::draw2d::batch_allocator{&denying_reallocate, &plain_release}});
    GLINTFX_CHECK(created.has_value());
    renderer_2d_impl *renderer = created.value();
    renderer->begin_frame(gltfx_frame_2d_desc{});
    deny_growth =
        true; // from here the storage cannot grow: what fits is drawn, the rest is counted
    for (int i = 0; i < 100; ++i) {
        renderer->fill_rect(rect(i, 0, 1, 1), k_red, k_layer_0);
    }
    const auto report = renderer->finish_frame();
    deny_growth = false;
    GLINTFX_CHECK(report.has_error());
    if (report.has_error()) {
        GLINTFX_CHECK(report.err().code() == gltfx_err_code::out_of_memory);
    }
    const auto last = renderer->last_frame_report();
    GLINTFX_CHECK_EQ(last.pieces_submitted, std::uint64_t{100});
    GLINTFX_CHECK(last.pieces_dropped_out_of_memory > 0);
    GLINTFX_CHECK(last.pieces_drawn > 0);
    GLINTFX_CHECK_EQ(last.pieces_drawn + last.pieces_dropped_out_of_memory, std::uint64_t{100});
    GLINTFX_CHECK(ends_add_up(last));
    renderer_2d_impl::destroy(renderer);
}

GLINTFX_TEST(renderer_2d_impl_the_first_error_of_the_frame_is_the_one_returned) {
    reset();
    renderer_2d_impl *renderer = make(true);
    renderer->begin_frame(gltfx_frame_2d_desc{});
    renderer->fill_rect(rect(0, 0, 1, 1), k_red, k_layer_0);
    card.error_from_draw = k_invalid_operation;
    renderer->flush(); // the FIRST error: a card that refuses the draw
    renderer->fill_rect(rect(0, 0, 1, 1), k_red, k_layer_0);
    card.error_from_draw = k_out_of_memory;
    const auto report = renderer->finish_frame(); // the SECOND error
    GLINTFX_CHECK(report.has_error());
    if (report.has_error()) {
        GLINTFX_CHECK(report.err().code() == gltfx_err_code::platform_failure);
        GLINTFX_CHECK_EQ(report.err().os_error_code(), std::int64_t{k_invalid_operation});
    }
    GLINTFX_CHECK(ends_add_up(renderer->last_frame_report()));
    GLINTFX_CHECK_EQ(renderer->last_frame_report().pieces_dropped_graphics_failure,
                     std::uint64_t{2});
    renderer_2d_impl::destroy(renderer);
}

GLINTFX_TEST(renderer_2d_impl_begin_batch_without_a_transform_goes_back_to_pixels_mid_frame) {
    reset();
    renderer_2d_impl *renderer = make(true);
    renderer->begin_frame(gltfx_frame_2d_desc{});
    renderer->begin_batch(glintfx::gltfx_transform{
        .translation = {.x = 100.0, .y = 0.0},
        .rotation = {.radians = 0.0},
        .scale = {.x = 1.0, .y = 1.0},
    });
    renderer->fill_rect(rect(1, 1, 1, 1), k_red, k_layer_0);
    renderer->begin_batch(); // pixel-direct again, in the MIDDLE of the frame
    renderer->fill_rect(rect(1, 1, 1, 1), k_blue, k_layer_0);
    GLINTFX_CHECK(renderer->finish_frame().has_value());
    GLINTFX_CHECK_EQ(sent(0, 0), 101.0F);
    GLINTFX_CHECK_EQ(sent(1, 0), 1.0F);
    renderer_2d_impl::destroy(renderer);
}

// The closed list of the state left behind (B0-I5): after every flush() and at the end of a frame,
// even an empty one.
GLINTFX_TEST(
    renderer_2d_impl_the_gl_state_left_after_a_flush_and_after_the_frame_is_the_closed_list) {
    constexpr GLenum k_blend = 0x0BE2;
    constexpr GLenum k_depth_test = 0x0B71;
    constexpr GLenum k_framebuffer_srgb = 0x8DB9;
    reset();
    renderer_2d_impl *renderer = make(false);
    renderer->begin_frame(gltfx_frame_2d_desc{});
    renderer->fill_rect(rect(0, 0, 1, 1), k_red, k_layer_0);
    renderer->flush();
    GLINTFX_CHECK(card.program_at_draw != 0); // while drawing, the renderer's program is in use
    GLINTFX_CHECK_EQ(card.current_program, GLuint{0});
    GLINTFX_CHECK_EQ(card.current_vao, GLuint{0});
    GLINTFX_CHECK_EQ(card.bound_array_buffer, GLuint{0});
    GLINTFX_CHECK_EQ(card.cap_state.at(k_blend), 1);
    GLINTFX_CHECK_EQ(card.cap_state.at(k_depth_test), 2);
    GLINTFX_CHECK_EQ(card.cap_state.at(k_framebuffer_srgb), 2); // the option is off
    GLINTFX_CHECK_EQ(card.viewport[2], 800);
    GLINTFX_CHECK_EQ(card.viewport[3], 600);
    card.current_program = 99; // dirty it: the end of an EMPTY frame must leave it clean again
    card.current_vao = 99;
    GLINTFX_CHECK(renderer->finish_frame().has_value());
    GLINTFX_CHECK_EQ(card.current_program, GLuint{0});
    GLINTFX_CHECK_EQ(card.current_vao, GLuint{0});
    renderer_2d_impl::destroy(renderer);
}

GLINTFX_TEST(renderer_2d_impl_the_error_of_the_card_comes_before_out_of_memory) {
    reset();
    deny_growth = false;
    const gl_function_table table = fake_table();
    auto created = renderer_2d_impl::create(
        fake_host(), table,
        renderer_2d_options{true, 1,
                            glintfx::draw2d::batch_allocator{&denying_reallocate, &plain_release}});
    GLINTFX_CHECK(created.has_value());
    renderer_2d_impl *renderer = created.value();
    renderer->begin_frame(gltfx_frame_2d_desc{});
    deny_growth = true;
    for (int i = 0; i < 100; ++i) {
        renderer->fill_rect(rect(i, 0, 1, 1), k_red, k_layer_0);
    }
    card.error_from_draw =
        k_invalid_operation; // a card error AND pieces dropped for lack of memory
    const auto report = renderer->finish_frame();
    deny_growth = false;
    GLINTFX_CHECK(report.has_error());
    if (report.has_error()) {
        GLINTFX_CHECK(report.err().code() == gltfx_err_code::platform_failure);
    }
    const auto last = renderer->last_frame_report();
    GLINTFX_CHECK(last.pieces_dropped_out_of_memory > 0);
    GLINTFX_CHECK(last.pieces_dropped_graphics_failure > 0);
    GLINTFX_CHECK(ends_add_up(last));
    renderer_2d_impl::destroy(renderer);
}

GLINTFX_TEST(renderer_2d_impl_the_default_reserve_holds_a_thousand_pieces_without_growing) {
    reset();
    deny_growth = false;
    const gl_function_table table = fake_table();
    auto created = renderer_2d_impl::create(
        fake_host(), table,
        renderer_2d_options{true, 0, // 0 lets the renderer choose
                            glintfx::draw2d::batch_allocator{&denying_reallocate, &plain_release}});
    GLINTFX_CHECK(created.has_value());
    renderer_2d_impl *renderer = created.value();
    renderer->begin_frame(gltfx_frame_2d_desc{});
    deny_growth = true; // nothing may grow from here: the first frames "do not grow storage"
    for (int i = 0; i < 1024; ++i) {
        renderer->fill_rect(rect(i, 0, 1, 1), k_red, k_layer_0);
    }
    const auto report = renderer->finish_frame();
    deny_growth = false;
    GLINTFX_CHECK(report.has_value());
    GLINTFX_CHECK_EQ(renderer->last_frame_report().pieces_drawn, std::uint64_t{1024});
    renderer_2d_impl::destroy(renderer);
}

// Two windows, two renderers: every method that touches GL makes ITS context current first.
GLINTFX_TEST(renderer_2d_impl_flush_makes_the_context_current_again) {
    reset();
    renderer_2d_impl *renderer = make(true);
    renderer->begin_frame(gltfx_frame_2d_desc{});
    const int after_begin = context.current_calls;
    renderer->fill_rect(rect(0, 0, 1, 1), k_red, k_layer_0);
    renderer->flush();
    GLINTFX_CHECK_EQ(context.current_calls, after_begin + 1);
    GLINTFX_CHECK(renderer->finish_frame().has_value());
    GLINTFX_CHECK_EQ(context.current_calls, after_begin + 2);
    renderer_2d_impl::destroy(renderer);
}

// S2b-fix1: the triangle path, against the fake card (the N1 review found it covered only by the
// container cell). The vertices are turned by the batch transform when the call is made, and the
// card gets one piece of three indices.
GLINTFX_TEST(renderer_2d_impl_a_triangle_call_reaches_the_card_at_its_pixel_positions) {
    reset();
    renderer_2d_impl *renderer = make(true);
    renderer->begin_frame(gltfx_frame_2d_desc{});
    renderer->begin_batch(glintfx::gltfx_transform{
        .translation = {.x = 10.0, .y = 20.0},
        .rotation = {.radians = 0.0},
        .scale = {.x = 2.0, .y = 2.0},
    });
    const std::array<glintfx::gltfx_vertex_2d_world, 3> triangle{
        corner_at(1.0, 2.0), corner_at(3.0, 4.0), corner_at(5.0, 6.0)};
    renderer->fill_triangles(triangle, k_layer_0);
    const auto report = renderer->finish_frame();
    GLINTFX_CHECK(report.has_value());
    if (report.has_value()) {
        GLINTFX_CHECK_EQ(report.value().pieces_submitted, std::uint64_t{1});
        GLINTFX_CHECK_EQ(report.value().pieces_drawn, std::uint64_t{1});
    }
    GLINTFX_CHECK_EQ(first_draw_count(), 3);
    GLINTFX_CHECK_EQ(sent_vertex(0, 0), 12.0F);
    GLINTFX_CHECK_EQ(sent_vertex(0, 1), 24.0F);
    GLINTFX_CHECK_EQ(sent_vertex(1, 0), 16.0F);
    GLINTFX_CHECK_EQ(sent_vertex(1, 1), 28.0F);
    GLINTFX_CHECK_EQ(sent_vertex(2, 0), 20.0F);
    GLINTFX_CHECK_EQ(sent_vertex(2, 1), 32.0F);
    renderer_2d_impl::destroy(renderer);
}

GLINTFX_TEST(renderer_2d_impl_an_indexed_triangle_call_is_one_piece_of_all_its_indices) {
    reset();
    renderer_2d_impl *renderer = make(true);
    renderer->begin_frame(gltfx_frame_2d_desc{});
    const std::array<glintfx::gltfx_vertex_2d_world, 4> quad{
        corner_at(0.0, 0.0), corner_at(8.0, 0.0), corner_at(8.0, 8.0), corner_at(0.0, 8.0)};
    const std::array<std::uint32_t, 6> indices{0U, 1U, 2U, 0U, 2U, 3U};
    renderer->fill_indexed_triangles(quad, indices, k_layer_0);
    const auto report = renderer->finish_frame();
    GLINTFX_CHECK(report.has_value());
    if (report.has_value()) {
        GLINTFX_CHECK_EQ(report.value().pieces_drawn, std::uint64_t{1});
    }
    GLINTFX_CHECK_EQ(first_draw_count(), 6);
    GLINTFX_CHECK_EQ(sent_vertex(2, 0), 8.0F);
    GLINTFX_CHECK_EQ(sent_vertex(3, 1), 8.0F);
    renderer_2d_impl::destroy(renderer);
}

// M9 of the N1 review. A call with no frame open is dropped and counted BEFORE anything is
// converted or kept: it never asks for memory. (What it would store is thrown away by the next
// begin_frame() anyway - renderer_2d_impl.cpp, begin_frame() clears the pieces - so the memory it
// asks for is the one thing that tells the two apart.)
GLINTFX_TEST(renderer_2d_impl_a_triangle_call_with_no_frame_open_asks_for_no_memory) {
    reset();
    const gl_function_table table = fake_table();
    auto created = renderer_2d_impl::create(
        fake_host(), table,
        renderer_2d_options{
            true, 16, glintfx::draw2d::batch_allocator{&counting_reallocate, &plain_release}});
    GLINTFX_CHECK(created.has_value());
    renderer_2d_impl *renderer = created.value();
    reallocations = 0;
    const std::array<glintfx::gltfx_vertex_2d_world, 3> triangle{
        corner_at(0.0, 0.0), corner_at(4.0, 0.0), corner_at(0.0, 4.0)};
    renderer->fill_triangles(triangle, k_layer_0);
    GLINTFX_CHECK_EQ(reallocations, std::size_t{0});
    renderer->begin_frame(gltfx_frame_2d_desc{});
    const auto report = renderer->finish_frame();
    GLINTFX_CHECK(report.has_value());
    if (report.has_value()) {
        GLINTFX_CHECK_EQ(report.value().pieces_dropped_outside_frame, std::uint64_t{1});
        GLINTFX_CHECK_EQ(report.value().pieces_submitted, std::uint64_t{0});
    }
    renderer_2d_impl::destroy(renderer);
}

// M10 of the N1 review. The pixel vertices of a triangle call need memory of their own; when it
// cannot grow, the call is counted as dropped for lack of memory, and that is the frame's error.
GLINTFX_TEST(renderer_2d_impl_a_triangle_call_that_finds_no_memory_is_counted_and_is_the_error) {
    reset();
    deny_growth = false;
    const gl_function_table table = fake_table();
    auto created = renderer_2d_impl::create(
        fake_host(), table,
        renderer_2d_options{true, 16,
                            glintfx::draw2d::batch_allocator{&denying_reallocate, &plain_release}});
    GLINTFX_CHECK(created.has_value());
    renderer_2d_impl *renderer = created.value();
    renderer->begin_frame(gltfx_frame_2d_desc{});
    deny_growth = true;
    const std::array<glintfx::gltfx_vertex_2d_world, 3> triangle{
        corner_at(0.0, 0.0), corner_at(4.0, 0.0), corner_at(0.0, 4.0)};
    renderer->fill_triangles(triangle, k_layer_0);
    const auto report = renderer->finish_frame();
    deny_growth = false;
    GLINTFX_CHECK(report.has_error());
    if (report.has_error()) {
        GLINTFX_CHECK(report.err().code() == gltfx_err_code::out_of_memory);
    }
    const auto last = renderer->last_frame_report();
    GLINTFX_CHECK_EQ(last.pieces_submitted, std::uint64_t{1});
    GLINTFX_CHECK_EQ(last.pieces_dropped_out_of_memory, std::uint64_t{1});
    GLINTFX_CHECK_EQ(last.pieces_drawn, std::uint64_t{0});
    GLINTFX_CHECK(ends_add_up(last));
    renderer_2d_impl::destroy(renderer);
}

// S2b-fix1 (I-7 of the N1 review): a piece whose pixel position is out of the range the vertex
// program can scale is REFUSED and counted as pixel_position_out_of_range, never sent to the card
// to become an infinity there. The triangle path
// and the quad path both; a refusal is not an error of the frame.
GLINTFX_TEST(renderer_2d_impl_a_pixel_position_out_of_range_is_refused_on_both_paths) {
    reset();
    renderer_2d_impl *renderer = make(true);
    renderer->begin_frame(gltfx_frame_2d_desc{});
    const std::array<glintfx::gltfx_vertex_2d_world, 3> far{
        corner_at(0.0, 0.0), corner_at(1.0e39, 0.0), corner_at(0.0, 4.0)};
    renderer->fill_triangles(far, k_layer_0);
    renderer->fill_rect(rect(1.0e39, 0, 1, 1), k_red, k_layer_0);
    renderer->begin_batch(glintfx::gltfx_transform{
        .translation = {.x = 0.0, .y = 0.0},
        .rotation = {.radians = 0.0},
        .scale = {.x = 1.0e300, .y = 1.0e300},
    });
    renderer->fill_quad(glintfx::gltfx_quad_world{{0, 0}, {1.0e10, 0}, {1, 1}, {0, 1}}, k_red,
                        k_layer_0);
    const auto report = renderer->finish_frame();
    GLINTFX_CHECK(report.has_value());
    if (report.has_value()) {
        GLINTFX_CHECK_EQ(report.value().pieces_submitted, std::uint64_t{3});
        GLINTFX_CHECK_EQ(report.value().pieces_refused, std::uint64_t{3});
        GLINTFX_CHECK_EQ(report.value().pieces_refused_pixel_position_out_of_range,
                         std::uint64_t{3});
        GLINTFX_CHECK(report.value().first_refusal ==
                      glintfx::gltfx_draw_2d_refusal::pixel_position_out_of_range);
        GLINTFX_CHECK(ends_add_up(report.value()));
    }
    GLINTFX_CHECK(card.draw_counts.empty());
    renderer_2d_impl::destroy(renderer);
}

// The value refusals come first: a NaN corner is not_a_number even where the transform would also
// carry it past the float range.
GLINTFX_TEST(renderer_2d_impl_a_value_refusal_comes_before_the_pixel_position) {
    reset();
    renderer_2d_impl *renderer = make(true);
    renderer->begin_frame(gltfx_frame_2d_desc{});
    const std::array<glintfx::gltfx_vertex_2d_world, 3> both{
        corner_at(std::numeric_limits<double>::quiet_NaN(), 0.0), corner_at(1.0e39, 0.0),
        corner_at(0.0, 4.0)};
    renderer->fill_triangles(both, k_layer_0);
    const auto report = renderer->finish_frame();
    GLINTFX_CHECK(report.has_value());
    if (report.has_value()) {
        GLINTFX_CHECK(report.value().first_refusal == glintfx::gltfx_draw_2d_refusal::not_a_number);
        GLINTFX_CHECK_EQ(report.value().pieces_refused_pixel_position_out_of_range,
                         std::uint64_t{0});
    }
    renderer_2d_impl::destroy(renderer);
}

// S2b-fix1 (I-7): the edge of the pixel range, on a surface 1 pixel wide and 1 high (the worst case
// of the derivation in quad_vertices.cpp). A vertex at the edge is drawn, and the two lines of the
// vertex program, run on the vertex the card receives with the viewport it receives, stay finite;
// one double past the edge, the call is refused by name. The two lines are checked word for word,
// so a change to them breaks this test before it silently breaks the bound.
GLINTFX_TEST(renderer_2d_impl_the_edge_of_the_pixel_range_keeps_the_vertex_program_finite) {
    const std::string_view program = glintfx::draw2d::embedded_vertex_shader_source();
    GLINTFX_CHECK(program.find("a_position.x / u_viewport_pixels.x * 2.0 - 1.0") !=
                  std::string_view::npos);
    GLINTFX_CHECK(program.find("1.0 - a_position.y / u_viewport_pixels.y * 2.0") !=
                  std::string_view::npos);
    reset();
    context.width = 1;
    context.height = 1;
    renderer_2d_impl *renderer = make(true);
    renderer->begin_frame(gltfx_frame_2d_desc{});
    // Written from the derivation, not the code: 3 ULP of 2^103 below FLT_MAX / 2 (S2b-fix2, F-3).
    const double edge =
        0.5 * static_cast<double>(std::numeric_limits<float>::max()) - 3.0 * std::ldexp(1.0, 103);
    const double past = std::nextafter(edge, std::numeric_limits<double>::infinity());
    const std::array<glintfx::gltfx_vertex_2d_world, 3> at_the_edge{
        corner_at(0.0, 0.0), corner_at(edge, 0.0), corner_at(0.0, edge)};
    const std::array<glintfx::gltfx_vertex_2d_world, 3> past_the_edge{
        corner_at(0.0, 0.0), corner_at(past, 0.0), corner_at(0.0, 1.0)};
    renderer->fill_triangles(at_the_edge, k_layer_0);
    renderer->fill_triangles(past_the_edge, k_layer_0);
    const auto report = renderer->finish_frame();
    GLINTFX_CHECK(report.has_value());
    if (report.has_value()) {
        GLINTFX_CHECK_EQ(report.value().pieces_drawn, std::uint64_t{1});
        GLINTFX_CHECK_EQ(report.value().pieces_refused_pixel_position_out_of_range,
                         std::uint64_t{1});
        GLINTFX_CHECK(report.value().first_refusal ==
                      glintfx::gltfx_draw_2d_refusal::pixel_position_out_of_range);
    }
    const float width = card.viewport_uniform.first;
    const float height = card.viewport_uniform.second;
    GLINTFX_CHECK_EQ(width, 1.0F);
    GLINTFX_CHECK_EQ(height, 1.0F);
    const float x = sent_vertex(1, 0);
    const float y = sent_vertex(2, 1);
    GLINTFX_CHECK(std::isfinite(x) && x > 1.0e38F);
    GLINTFX_CHECK(std::isfinite(y) && y > 1.0e38F);
    GLINTFX_CHECK(std::isfinite(x / width * 2.0F - 1.0F));
    GLINTFX_CHECK(std::isfinite(1.0F - y / height * 2.0F));
    renderer_2d_impl::destroy(renderer);
}

// R2D-TEXTURE, fatia T3 (D-W8-288, texture_2d.hpp LIFETIME): a texture destroyed while its renderer
// has a frame open is released when the frame ends, AFTER its last draw, and its destruction sends
// nothing (it is not a barrier: a piece of a lower layer submitted after it is still painted under
// the piece submitted before it). An abandoned frame releases at the next begin_frame(), a
// destroyed renderer before it goes; outside a frame the release is at once, after the context. A
// release whose context fails deletes nothing and says so (texture_release_failed). The records are
// built by hand: the release does not depend on how a texture was made (texture_upload_test).
namespace {

std::vector<GLuint> deleted_textures;

void GLINTFX_GL_APIENTRY f_delete_textures(GLsizei n, const GLuint *names) {
    GLINTFX_COUNT();
    for (GLsizei i = 0; i < n; ++i) {
        card.calls.emplace_back("delete_texture");
        deleted_textures.push_back(names[i]);
    }
}

renderer_2d_impl *make_with_textures() {
    gl_function_table table = fake_table();
    table.glDeleteTextures = &f_delete_textures;
    auto created = renderer_2d_impl::create(fake_host(), table, renderer_2d_options{true, 16});
    GLINTFX_CHECK(created.has_value());
    return created.has_value() ? created.value() : nullptr;
}

// A texture of `renderer`, GL name `name`, as the making leaves it (texture_upload.hpp).
[[nodiscard]] glintfx::texture_2d_impl *texture_of(renderer_2d_impl *renderer, GLuint name) {
    return new glintfx::texture_2d_impl{
        .access = renderer->texture_access(),
        .gl_name = name,
        .width = 2U,
        .height = 2U,
        .filter = glintfx::gltfx_texture_filter::linear,
        .reduced_levels = 0U,
    };
}

// Where `name` sits in the order of the calls the card saw; -1 when it is not there.
[[nodiscard]] int position_of(const std::string &name, bool last) {
    int found = -1;
    for (std::size_t i = 0; i < card.calls.size(); ++i) {
        if (card.calls[i] == name) {
            found = static_cast<int>(i);
            if (!last) {
                return found;
            }
        }
    }
    return found;
}

std::vector<std::string> release_events;

void capture_release_event(void *, const glintfx::gltfx_log_event &event) noexcept {
    release_events.emplace_back(event.name());
}

// Arms the capture for one case and disarms it at the end, even when a check fails.
struct armed_release_log {
    armed_release_log() {
        release_events.clear();
        glintfx::gltfx_log_set_sink(glintfx::gltfx_log_sink{&capture_release_event, nullptr,
                                                            glintfx::gltfx_log_severity::trace});
    }
    ~armed_release_log() { glintfx::gltfx_log_set_sink(glintfx::gltfx_log_sink{}); }
    armed_release_log(const armed_release_log &) = delete;
    armed_release_log &operator=(const armed_release_log &) = delete;
};

constexpr glintfx::gltfx_draw_layer k_layer_1{1};

} // namespace

GLINTFX_TEST(renderer_2d_impl_a_texture_destroyed_in_a_frame_is_released_after_the_last_draw) {
    reset();
    deleted_textures.clear();
    renderer_2d_impl *renderer = make_with_textures();
    renderer->begin_frame(gltfx_frame_2d_desc{});
    renderer->fill_rect(rect(10.0, 10.0, 10.0, 10.0), k_red, k_layer_1);
    glintfx::draw2d::release_texture_object(texture_of(renderer, 77U));
    GLINTFX_CHECK(card.calls.empty() || position_of("draw", false) < 0);
    GLINTFX_CHECK(deleted_textures.empty());
    // Submitted AFTER the destruction, in a LOWER layer: with no barrier, it is painted first.
    renderer->fill_rect(rect(12.0, 12.0, 10.0, 10.0), k_blue, k_layer_0);
    const auto report = renderer->finish_frame();
    GLINTFX_CHECK(report.has_value());
    GLINTFX_CHECK_EQ(card.draw_counts.size(), std::size_t{1});
    GLINTFX_CHECK_EQ(sent(0, 6), 1.0F); // the blue piece (layer 0) first
    GLINTFX_CHECK_EQ(sent(1, 4), 1.0F); // the red piece (layer 1) over it
    GLINTFX_CHECK_EQ(deleted_textures.size(), std::size_t{1});
    GLINTFX_CHECK(!deleted_textures.empty() && deleted_textures.front() == 77U);
    GLINTFX_CHECK(position_of("delete_texture", false) > position_of("draw", true));
    renderer_2d_impl::destroy(renderer);
}

GLINTFX_TEST(renderer_2d_impl_a_texture_held_by_an_abandoned_frame_is_released_at_the_next_one) {
    reset();
    deleted_textures.clear();
    renderer_2d_impl *renderer = make_with_textures();
    renderer->begin_frame(gltfx_frame_2d_desc{});
    glintfx::draw2d::release_texture_object(texture_of(renderer, 78U));
    GLINTFX_CHECK(deleted_textures.empty());
    const int current_before = context.current_calls;
    renderer->begin_frame(gltfx_frame_2d_desc{});
    GLINTFX_CHECK_EQ(deleted_textures.size(), std::size_t{1});
    GLINTFX_CHECK(context.current_calls > current_before);
    (void)renderer->finish_frame();
    GLINTFX_CHECK_EQ(deleted_textures.size(), std::size_t{1}); // released once, not twice
    renderer_2d_impl::destroy(renderer);
}

GLINTFX_TEST(renderer_2d_impl_destroying_the_renderer_releases_the_textures_it_holds) {
    reset();
    deleted_textures.clear();
    renderer_2d_impl *renderer = make_with_textures();
    renderer->begin_frame(gltfx_frame_2d_desc{});
    glintfx::draw2d::release_texture_object(texture_of(renderer, 79U));
    GLINTFX_CHECK(deleted_textures.empty());
    renderer_2d_impl::destroy(renderer);
    GLINTFX_CHECK_EQ(deleted_textures.size(), std::size_t{1});
    GLINTFX_CHECK(!deleted_textures.empty() && deleted_textures.front() == 79U);
}

GLINTFX_TEST(renderer_2d_impl_a_texture_destroyed_outside_a_frame_is_released_at_once) {
    reset();
    deleted_textures.clear();
    renderer_2d_impl *renderer = make_with_textures();
    const int current_before = context.current_calls;
    glintfx::draw2d::release_texture_object(texture_of(renderer, 80U));
    GLINTFX_CHECK_EQ(deleted_textures.size(), std::size_t{1});
    GLINTFX_CHECK(context.current_calls > current_before);
    renderer_2d_impl::destroy(renderer);
}

GLINTFX_TEST(renderer_2d_impl_a_release_whose_context_fails_deletes_nothing_and_says_so) {
    reset();
    deleted_textures.clear();
    const armed_release_log log;
    renderer_2d_impl *renderer = make_with_textures();
    // Outside a frame: at once, after the context, which fails.
    context.fail_current = true;
    glintfx::draw2d::release_texture_object(texture_of(renderer, 81U));
    GLINTFX_CHECK(deleted_textures.empty());
    GLINTFX_CHECK_EQ(release_events.size(), std::size_t{1});
    // In a frame: held, and released at the end of the frame, whose context fails as well.
    context.fail_current = false;
    renderer->begin_frame(gltfx_frame_2d_desc{});
    glintfx::draw2d::release_texture_object(texture_of(renderer, 82U));
    context.fail_current = true;
    (void)renderer->finish_frame();
    GLINTFX_CHECK(deleted_textures.empty());
    GLINTFX_CHECK_EQ(release_events.size(), std::size_t{2});
    GLINTFX_CHECK(!release_events.empty() && release_events.back() == "texture_release_failed");
    context.fail_current = false;
    renderer_2d_impl::destroy(renderer);
}

// R2D-SHAPES, fatia S3: stroke_line() in the frame machine. One call is one piece: its outline, a
// fan of triangles, reaches the card at its pixel positions; a refusal is counted once with its
// reason, in the order of the vocabulary; with no frame open, it is counted outside the frame; a
// line of zero thickness is a piece of zero area, drawn. Expected values by hand (axis-aligned).
namespace {

[[nodiscard]] glintfx::gltfx_line_style line_style(double thickness, glintfx::gltfx_line_cap cap) {
    glintfx::gltfx_line_style style;
    style.stroke.thickness = thickness;
    style.cap = cap;
    return style;
}

constexpr glintfx::gltfx_segment_world k_line{{10.0, 10.0}, {50.0, 10.0}};

[[nodiscard]] glintfx::gltfx_line_cap unknown_line_cap() {
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) reason: the cast is the case
    return static_cast<glintfx::gltfx_line_cap>(9);
}

} // namespace

GLINTFX_TEST(renderer_2d_impl_a_line_reaches_the_card_as_its_outline) {
    reset();
    renderer_2d_impl *renderer = make(true);
    renderer->begin_frame(gltfx_frame_2d_desc{});
    renderer->stroke_line(k_line, line_style(4.0, glintfx::gltfx_line_cap::flat), k_red, k_layer_0);
    const auto report = renderer->finish_frame();
    GLINTFX_CHECK(report.has_value());
    if (report.has_value()) {
        GLINTFX_CHECK_EQ(report.value().pieces_submitted, std::uint64_t{1});
        GLINTFX_CHECK_EQ(report.value().pieces_drawn, std::uint64_t{1});
        GLINTFX_CHECK(ends_add_up(report.value()));
    }
    GLINTFX_CHECK_EQ(first_draw_count(), 6);
    GLINTFX_CHECK_EQ(sent_vertex(0, 0), 10.0F);
    GLINTFX_CHECK_EQ(sent_vertex(0, 1), 12.0F);
    GLINTFX_CHECK_EQ(sent_vertex(2, 0), 50.0F);
    GLINTFX_CHECK_EQ(sent_vertex(2, 1), 8.0F);
    renderer_2d_impl::destroy(renderer);
}

GLINTFX_TEST(renderer_2d_impl_a_line_refusal_is_counted_once_with_its_reason) {
    reset();
    renderer_2d_impl *renderer = make(true);
    renderer->begin_frame(gltfx_frame_2d_desc{});
    const double not_a_number = std::numeric_limits<double>::quiet_NaN();
    renderer->stroke_line(k_line, line_style(not_a_number, glintfx::gltfx_line_cap::flat), k_red,
                          k_layer_0);
    renderer->stroke_line(k_line, line_style(4.0, unknown_line_cap()), k_red, k_layer_0);
    renderer->stroke_line(k_line, line_style(-1.0, glintfx::gltfx_line_cap::flat), k_red,
                          k_layer_0);
    const glintfx::gltfx_segment_world far{{1.0e39, 0.0}, {2.0e39, 0.0}};
    renderer->stroke_line(far, line_style(4.0, glintfx::gltfx_line_cap::rounded), k_red, k_layer_0);
    const auto report = renderer->finish_frame();
    GLINTFX_CHECK(report.has_value());
    if (report.has_value()) {
        const glintfx::gltfx_frame_2d_report &r = report.value();
        GLINTFX_CHECK_EQ(r.pieces_submitted, std::uint64_t{4});
        GLINTFX_CHECK_EQ(r.pieces_refused, std::uint64_t{4});
        GLINTFX_CHECK_EQ(r.pieces_refused_not_a_number, std::uint64_t{1});
        GLINTFX_CHECK_EQ(r.pieces_refused_unknown_option, std::uint64_t{1});
        GLINTFX_CHECK_EQ(r.pieces_refused_negative_size, std::uint64_t{1});
        GLINTFX_CHECK_EQ(r.pieces_refused_pixel_position_out_of_range, std::uint64_t{1});
        GLINTFX_CHECK(r.first_refusal == glintfx::gltfx_draw_2d_refusal::not_a_number);
        GLINTFX_CHECK(ends_add_up(r));
    }
    GLINTFX_CHECK(card.draw_counts.empty());
    renderer_2d_impl::destroy(renderer);
}

GLINTFX_TEST(renderer_2d_impl_a_line_with_no_frame_open_is_counted_outside_the_frame) {
    reset();
    renderer_2d_impl *renderer = make(true);
    renderer->stroke_line(k_line, line_style(4.0, glintfx::gltfx_line_cap::flat), k_red, k_layer_0);
    renderer->begin_frame(gltfx_frame_2d_desc{});
    const auto report = renderer->finish_frame();
    GLINTFX_CHECK(report.has_value());
    if (report.has_value()) {
        GLINTFX_CHECK_EQ(report.value().pieces_dropped_outside_frame, std::uint64_t{1});
        GLINTFX_CHECK_EQ(report.value().pieces_submitted, std::uint64_t{0});
    }
    renderer_2d_impl::destroy(renderer);
}

// S3-fix1 (I-2 of the N1 review): the line is placed by the batch transform in force. Expected
// values by the geometry, not read from the code: the world line runs at y = 10 from x = 10 to
// x = 50; the batch scales by 2, so its ends land at (20, 20) and (100, 20); in pixel unit the
// thickness 4 stays 4 on the screen, half of it (2 px) on each side of y = 20, so the flat outline
// starts at (20, 22) and its third point is (100, 18).
GLINTFX_TEST(renderer_2d_impl_a_line_is_placed_by_the_batch_transform_in_force) {
    reset();
    renderer_2d_impl *renderer = make(true);
    renderer->begin_frame(gltfx_frame_2d_desc{});
    renderer->begin_batch(glintfx::gltfx_transform{
        .translation = {.x = 0.0, .y = 0.0},
        .rotation = {.radians = 0.0},
        .scale = {.x = 2.0, .y = 2.0},
    });
    glintfx::gltfx_line_style style = line_style(4.0, glintfx::gltfx_line_cap::flat);
    style.stroke.unit = glintfx::gltfx_stroke_unit::pixel;
    renderer->stroke_line(k_line, style, k_red, k_layer_0);
    const auto report = renderer->finish_frame();
    GLINTFX_CHECK(report.has_value());
    GLINTFX_CHECK_EQ(first_draw_count(), 6);
    GLINTFX_CHECK_EQ(sent_vertex(0, 0), 20.0F);
    GLINTFX_CHECK_EQ(sent_vertex(0, 1), 22.0F);
    GLINTFX_CHECK_EQ(sent_vertex(2, 0), 100.0F);
    GLINTFX_CHECK_EQ(sent_vertex(2, 1), 18.0F);
    renderer_2d_impl::destroy(renderer);
}

// R2D-SHAPES S4: stroke_rect() through the frame machine. One call is one piece of the triangles of
// its outline (24 indices with a hole, 6 without), refused whole and counted once with its reason,
// in the order of the vocabulary; with no frame open, it is counted outside the frame. Expected
// values by hand (the outline of stroke_rect_geometry_test: x = 10, y = 10, width 40, height 20,
// thickness 4, centered: outer (8,8) to (52,32), inner (12,12) to (48,28)).
namespace {

[[nodiscard]] glintfx::gltfx_rect_outline_style outline_style(double thickness,
                                                              glintfx::gltfx_stroke_align align) {
    return glintfx::gltfx_rect_outline_style{
        glintfx::gltfx_stroke{thickness, glintfx::gltfx_stroke_unit::world}, align};
}

[[nodiscard]] glintfx::gltfx_stroke_align unknown_stroke_align() {
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) reason: the cast is the case
    return static_cast<glintfx::gltfx_stroke_align>(9);
}

} // namespace

GLINTFX_TEST(renderer_2d_impl_a_rect_outline_reaches_the_card_as_four_trapezoids) {
    reset();
    renderer_2d_impl *renderer = make(true);
    renderer->begin_frame(gltfx_frame_2d_desc{});
    renderer->stroke_rect(rect(10.0, 10.0, 40.0, 20.0),
                          outline_style(4.0, glintfx::gltfx_stroke_align::centered), k_red,
                          k_layer_0);
    const auto report = renderer->finish_frame();
    GLINTFX_CHECK(report.has_value());
    if (report.has_value()) {
        GLINTFX_CHECK_EQ(report.value().pieces_submitted, std::uint64_t{1});
        GLINTFX_CHECK_EQ(report.value().pieces_drawn, std::uint64_t{1});
        GLINTFX_CHECK(ends_add_up(report.value()));
    }
    GLINTFX_CHECK_EQ(first_draw_count(), 24);
    GLINTFX_CHECK_EQ(sent_vertex(0, 0), 8.0F);
    GLINTFX_CHECK_EQ(sent_vertex(0, 1), 8.0F);
    GLINTFX_CHECK_EQ(sent_vertex(2, 0), 52.0F);
    GLINTFX_CHECK_EQ(sent_vertex(2, 1), 32.0F);
    GLINTFX_CHECK_EQ(sent_vertex(4, 0), 12.0F);
    GLINTFX_CHECK_EQ(sent_vertex(6, 1), 28.0F);
    renderer_2d_impl::destroy(renderer);
}

// The batch transform in force carries the outline (world unit): zoom 2, moved by (100, 50), so the
// outer top-left corner (8, 8) lands on (116, 66) and the inner one (12, 12) on (124, 74).
GLINTFX_TEST(renderer_2d_impl_a_rect_outline_is_carried_by_the_batch_transform) {
    reset();
    renderer_2d_impl *renderer = make(true);
    renderer->begin_frame(gltfx_frame_2d_desc{});
    renderer->begin_batch(glintfx::gltfx_transform{
        .translation = {.x = 100.0, .y = 50.0},
        .rotation = {.radians = 0.0},
        .scale = {.x = 2.0, .y = 2.0},
    });
    renderer->stroke_rect(rect(10.0, 10.0, 40.0, 20.0),
                          outline_style(4.0, glintfx::gltfx_stroke_align::centered), k_red,
                          k_layer_0);
    const auto report = renderer->finish_frame();
    GLINTFX_CHECK(report.has_value());
    GLINTFX_CHECK_EQ(first_draw_count(), 24);
    GLINTFX_CHECK_EQ(sent_vertex(0, 0), 116.0F);
    GLINTFX_CHECK_EQ(sent_vertex(0, 1), 66.0F);
    GLINTFX_CHECK_EQ(sent_vertex(4, 0), 124.0F);
    GLINTFX_CHECK_EQ(sent_vertex(4, 1), 74.0F);
    renderer_2d_impl::destroy(renderer);
}

GLINTFX_TEST(renderer_2d_impl_a_rect_outline_without_a_hole_is_one_quad) {
    reset();
    renderer_2d_impl *renderer = make(true);
    renderer->begin_frame(gltfx_frame_2d_desc{});
    renderer->stroke_rect(rect(10.0, 10.0, 6.0, 20.0),
                          outline_style(4.0, glintfx::gltfx_stroke_align::inside), k_red,
                          k_layer_0);
    const auto report = renderer->finish_frame();
    GLINTFX_CHECK(report.has_value());
    GLINTFX_CHECK_EQ(first_draw_count(), 6);
    GLINTFX_CHECK_EQ(sent_vertex(2, 0), 16.0F);
    GLINTFX_CHECK_EQ(sent_vertex(2, 1), 30.0F);
    renderer_2d_impl::destroy(renderer);
}

GLINTFX_TEST(renderer_2d_impl_a_rect_outline_refusal_is_counted_once_with_its_reason) {
    reset();
    renderer_2d_impl *renderer = make(true);
    renderer->begin_frame(gltfx_frame_2d_desc{});
    const double not_a_number = std::numeric_limits<double>::quiet_NaN();
    renderer->stroke_rect(rect(10.0, 10.0, 40.0, 20.0),
                          outline_style(not_a_number, glintfx::gltfx_stroke_align::centered), k_red,
                          k_layer_0);
    renderer->stroke_rect(rect(10.0, 10.0, 40.0, 20.0), outline_style(4.0, unknown_stroke_align()),
                          k_red, k_layer_0);
    renderer->stroke_rect(rect(10.0, 10.0, -1.0, 20.0),
                          outline_style(4.0, glintfx::gltfx_stroke_align::centered), k_red,
                          k_layer_0);
    renderer->stroke_rect(rect(1.0e39, 0.0, 40.0, 20.0),
                          outline_style(4.0, glintfx::gltfx_stroke_align::centered), k_red,
                          k_layer_0);
    const auto report = renderer->finish_frame();
    GLINTFX_CHECK(report.has_value());
    if (report.has_value()) {
        const glintfx::gltfx_frame_2d_report &r = report.value();
        GLINTFX_CHECK_EQ(r.pieces_submitted, std::uint64_t{4});
        GLINTFX_CHECK_EQ(r.pieces_refused, std::uint64_t{4});
        GLINTFX_CHECK_EQ(r.pieces_refused_not_a_number, std::uint64_t{1});
        GLINTFX_CHECK_EQ(r.pieces_refused_unknown_option, std::uint64_t{1});
        GLINTFX_CHECK_EQ(r.pieces_refused_negative_size, std::uint64_t{1});
        GLINTFX_CHECK_EQ(r.pieces_refused_pixel_position_out_of_range, std::uint64_t{1});
        GLINTFX_CHECK(r.first_refusal == glintfx::gltfx_draw_2d_refusal::not_a_number);
        GLINTFX_CHECK(ends_add_up(r));
    }
    GLINTFX_CHECK(card.draw_counts.empty());
    renderer_2d_impl::destroy(renderer);
}

// S4-fix1 (revisao N1 da S4, IMPORTANTE-3): the outline is painted in ITS layer. A red outline in
// layer 1, then a blue square in layer 0 submitted after it: the square is painted first (its
// first vertex is the first sent), and the outline's first vertex comes after its four.
GLINTFX_TEST(renderer_2d_impl_a_rect_outline_is_painted_in_its_layer) {
    reset();
    renderer_2d_impl *renderer = make(true);
    renderer->begin_frame(gltfx_frame_2d_desc{});
    renderer->stroke_rect(rect(10.0, 10.0, 40.0, 20.0),
                          outline_style(4.0, glintfx::gltfx_stroke_align::centered), k_red,
                          glintfx::gltfx_draw_layer{1});
    renderer->fill_rect(rect(12.0, 12.0, 10.0, 10.0), k_blue, k_layer_0);
    GLINTFX_CHECK(renderer->finish_frame().has_value());
    GLINTFX_CHECK_EQ(sent_vertex(0, 6), 1.0F); // the blue square (layer 0) first
    GLINTFX_CHECK_EQ(sent_vertex(4, 4), 1.0F); // then the red outline (layer 1), over it
    GLINTFX_CHECK_EQ(sent_vertex(4, 6), 0.0F);
    renderer_2d_impl::destroy(renderer);
}

GLINTFX_TEST(renderer_2d_impl_a_rect_outline_with_no_frame_open_is_counted_outside_the_frame) {
    reset();
    renderer_2d_impl *renderer = make(true);
    renderer->stroke_rect(rect(10.0, 10.0, 40.0, 20.0),
                          outline_style(4.0, glintfx::gltfx_stroke_align::centered), k_red,
                          k_layer_0);
    renderer->begin_frame(gltfx_frame_2d_desc{});
    const auto report = renderer->finish_frame();
    GLINTFX_CHECK(report.has_value());
    if (report.has_value()) {
        GLINTFX_CHECK_EQ(report.value().pieces_dropped_outside_frame, std::uint64_t{1});
        GLINTFX_CHECK_EQ(report.value().pieces_submitted, std::uint64_t{0});
    }
    renderer_2d_impl::destroy(renderer);
}

GLINTFX_TEST(renderer_2d_impl_a_line_of_zero_thickness_is_a_drawn_piece_of_zero_area) {
    reset();
    renderer_2d_impl *renderer = make(true);
    renderer->begin_frame(gltfx_frame_2d_desc{});
    renderer->stroke_line(k_line, line_style(0.0, glintfx::gltfx_line_cap::flat), k_red, k_layer_0);
    const auto report = renderer->finish_frame();
    GLINTFX_CHECK(report.has_value());
    if (report.has_value()) {
        GLINTFX_CHECK_EQ(report.value().pieces_drawn, std::uint64_t{1});
        GLINTFX_CHECK_EQ(report.value().pieces_refused, std::uint64_t{0});
    }
    GLINTFX_CHECK_EQ(first_draw_count(), 6);
    GLINTFX_CHECK_EQ(sent_vertex(0, 1), 10.0F);
    GLINTFX_CHECK_EQ(sent_vertex(2, 1), 10.0F);
    renderer_2d_impl::destroy(renderer);
}
