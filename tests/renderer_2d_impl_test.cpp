// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <print>
#include <string>
#include <utility>
#include <vector>

#include <glintfx/core/color.hpp>
#include <glintfx/core/err_code.hpp>

#include "draw2d/renderer_2d_impl.hpp"

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
