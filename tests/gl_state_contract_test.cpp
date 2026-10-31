// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstdint>
#include <print>

#include "draw2d/gl_state_contract.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// gl_state_contract_test.cpp - R2D-BATCH, fatia B3c (docs/plano-w7d.md sec. 4.3, R-B3, D-W7D-11,
// B0-I5). Proves the contract of gl_state_contract.hpp against a gl_function_table of FAKES that
// keep a MODEL of the GL state: the state is planted HOSTILE (depth test on, blending swapped, a
// one-pixel scissor, front faces culled, color mask off, a stray program, vertex array and buffers,
// a 1x1 viewport), the function runs, and EVERY item of the closed list is read back.
//
// A DECLARED DOWNGRADE (GODS_LAWS.md L-09): this proves that the layer asks GL for each item, in a
// model. It does not prove what a real driver does with the calls; that is the pixel read of the
// live cells (tests/parity/draw2d_parity_test.cpp, with the renderer of B4). The literal
// enumerators below are the ones of the vendored registry, never the constants the code uses
// (D-P1-5's rule).

using glintfx::draw2d::clear_stencil;
using glintfx::draw2d::draw_state_request;
using glintfx::draw2d::leave_gl_state_after_drawing;
using glintfx::draw2d::set_gl_state_for_drawing;
using glintfx::draw2d::set_stencil_pass;
using glintfx::draw2d::stencil_pass;
using glintfx::render::gl_function_table;
using glintfx::render::GLbitfield;
using glintfx::render::GLboolean;
using glintfx::render::GLenum;
using glintfx::render::GLint;
using glintfx::render::GLsizei;
using glintfx::render::GLuint;

namespace {

constexpr GLenum k_blend = 0x0BE2;
constexpr GLenum k_cull_face = 0x0B44;
constexpr GLenum k_depth_test = 0x0B71;
constexpr GLenum k_scissor_test = 0x0C11;
constexpr GLenum k_stencil_test = 0x0B90;
constexpr GLenum k_framebuffer_srgb = 0x8DB9;
constexpr GLenum k_one = 1;
constexpr GLenum k_one_minus_src_alpha = 0x0303;
constexpr GLenum k_zero = 0;
constexpr GLenum k_src_alpha = 0x0302;
constexpr GLenum k_texture0 = 0x84C0;
constexpr GLenum k_array_buffer = 0x8892;
constexpr GLenum k_element_array_buffer = 0x8893;
constexpr GLenum k_texture_2d = 0x0DE1;
constexpr GLenum k_unpack_alignment = 0x0CF5;
constexpr GLenum k_func_add = 0x8006;
constexpr GLenum k_func_max = 0x8008;
constexpr GLenum k_front_and_back = 0x0408;
constexpr GLenum k_fill = 0x1B02;
constexpr GLenum k_line = 0x1B01;
constexpr GLenum k_rasterizer_discard = 0x8C89;
constexpr GLenum k_color_logic_op = 0x0BF2;
constexpr GLenum k_sample_alpha_to_coverage = 0x809E;
constexpr GLenum k_draw_framebuffer = 0x8CA9;
// R2D-SPRITE-SHADE H3a: the stencil (the registry's values).
constexpr GLenum k_never = 0x0200;
constexpr GLenum k_equal = 0x0202;
constexpr GLenum k_always = 0x0207;
constexpr GLenum k_keep = 0x1E00;
constexpr GLenum k_incr = 0x1E02;
constexpr GLenum k_invert = 0x150A;
constexpr GLbitfield k_stencil_buffer_bit = 0x00000400;
constexpr GLuint k_every_bit = 0xFFFFFFFFU;

// The model of the GL state the fakes keep.
struct gl_model {
    bool blend = false;
    bool cull_face = false;
    bool depth_test = false;
    bool scissor_test = false;
    bool stencil_test = false;
    bool framebuffer_srgb = false;
    bool rasterizer_discard = false;
    bool color_logic_op = false;
    bool sample_alpha_to_coverage = false;
    std::array<GLenum, 2> blend_equation{}; // rgb, alpha
    std::array<GLenum, 2> polygon_mode{};   // front, back
    GLuint draw_framebuffer = 0;
    std::array<GLenum, 4> blend_func{}; // src rgb, dst rgb, src alpha, dst alpha
    std::array<bool, 4> color_mask{};
    std::array<GLint, 4> viewport{};
    GLuint program = 0;
    GLuint vertex_array = 0;
    GLuint array_buffer = 0;
    GLuint element_buffer = 0;
    GLenum active_texture = 0;
    GLuint texture_2d = 0;
    GLint unpack_alignment = 4;
    // R2D-SPRITE-SHADE H3a: the stencil function, operation and write mask, and what a clear saw.
    GLenum stencil_function = k_always;
    GLint stencil_reference = 0;
    GLuint stencil_read_mask = k_every_bit;
    std::array<GLenum, 3> stencil_op{k_keep, k_keep, k_keep}; // fail, depth fail, pass
    GLuint stencil_write_mask = k_every_bit;
    GLint stencil_clear_value = 0;
    int stencil_clears = 0;
    GLuint write_mask_at_clear = 0;
    GLint value_at_clear = -1;
};

gl_model model;

void plant_hostile_state() {
    model = gl_model{};
    model.blend = false;                                      // off
    model.blend_func = {k_zero, k_zero, k_src_alpha, k_zero}; // swapped and mixed
    model.depth_test = true;
    model.stencil_test = true;
    model.scissor_test = true;
    model.cull_face = true;
    model.framebuffer_srgb = true;
    model.color_mask = {false, false, false, false};
    model.viewport = {5, 5, 1, 1};
    model.program = 99;
    model.vertex_array = 98;
    model.array_buffer = 97;
    model.element_buffer = 96;
    model.active_texture = k_texture0 + 3;
    model.texture_2d = 95;
    model.unpack_alignment = 8;
    model.rasterizer_discard = true;
    model.color_logic_op = true;
    model.sample_alpha_to_coverage = true;
    model.blend_equation = {k_func_max, k_func_max}; // MAX: a consumer's blending
    model.polygon_mode = {k_line, k_line};           // a debug wireframe
    model.draw_framebuffer = 77;                     // the consumer's own FBO
}

// A consumer's stencil, every item away from the GL defaults (R2D-SPRITE-SHADE H3a).
void plant_hostile_stencil() {
    model.stencil_function = k_never;
    model.stencil_reference = 7;
    model.stencil_read_mask = 0x0FU;
    model.stencil_op = {k_invert, k_invert, k_invert};
    model.stencil_write_mask = 0;
    model.stencil_clear_value = 9;
}

void set_cap(GLenum cap, bool value) {
    switch (cap) {
    case k_blend:
        model.blend = value;
        break;
    case k_cull_face:
        model.cull_face = value;
        break;
    case k_depth_test:
        model.depth_test = value;
        break;
    case k_scissor_test:
        model.scissor_test = value;
        break;
    case k_stencil_test:
        model.stencil_test = value;
        break;
    case k_framebuffer_srgb:
        model.framebuffer_srgb = value;
        break;
    case k_rasterizer_discard:
        model.rasterizer_discard = value;
        break;
    case k_color_logic_op:
        model.color_logic_op = value;
        break;
    case k_sample_alpha_to_coverage:
        model.sample_alpha_to_coverage = value;
        break;
    default:
        break;
    }
}

void GLINTFX_GL_APIENTRY fake_enable(GLenum cap) { set_cap(cap, true); }
void GLINTFX_GL_APIENTRY fake_disable(GLenum cap) { set_cap(cap, false); }
void GLINTFX_GL_APIENTRY fake_blend_func_separate(GLenum a, GLenum b, GLenum c, GLenum d) {
    model.blend_func = {a, b, c, d};
}
void GLINTFX_GL_APIENTRY fake_blend_equation_separate(GLenum rgb, GLenum alpha) {
    model.blend_equation = {rgb, alpha};
}
void GLINTFX_GL_APIENTRY fake_polygon_mode(GLenum face, GLenum mode) {
    if (face == k_front_and_back) {
        model.polygon_mode = {mode, mode};
    }
}
void GLINTFX_GL_APIENTRY fake_bind_framebuffer(GLenum target, GLuint framebuffer) {
    if (target == k_draw_framebuffer) {
        model.draw_framebuffer = framebuffer;
    }
}
void GLINTFX_GL_APIENTRY fake_color_mask(GLboolean r, GLboolean g, GLboolean b, GLboolean a) {
    model.color_mask = {r != 0, g != 0, b != 0, a != 0};
}
void GLINTFX_GL_APIENTRY fake_viewport(GLint x, GLint y, GLsizei w, GLsizei h) {
    model.viewport = {x, y, w, h};
}
void GLINTFX_GL_APIENTRY fake_use_program(GLuint program) { model.program = program; }
void GLINTFX_GL_APIENTRY fake_bind_vertex_array(GLuint array) { model.vertex_array = array; }
void GLINTFX_GL_APIENTRY fake_bind_buffer(GLenum target, GLuint buffer) {
    if (target == k_array_buffer) {
        model.array_buffer = buffer;
    } else if (target == k_element_array_buffer) {
        model.element_buffer = buffer;
    }
}
void GLINTFX_GL_APIENTRY fake_active_texture(GLenum texture) { model.active_texture = texture; }
void GLINTFX_GL_APIENTRY fake_bind_texture(GLenum target, GLuint texture) {
    if (target == k_texture_2d) {
        model.texture_2d = texture;
    }
}
void GLINTFX_GL_APIENTRY fake_pixel_storei(GLenum pname, GLint param) {
    if (pname == k_unpack_alignment) {
        model.unpack_alignment = param;
    }
}

void GLINTFX_GL_APIENTRY fake_stencil_func(GLenum function, GLint reference, GLuint mask) {
    model.stencil_function = function;
    model.stencil_reference = reference;
    model.stencil_read_mask = mask;
}
void GLINTFX_GL_APIENTRY fake_stencil_op(GLenum fail, GLenum depth_fail, GLenum pass) {
    model.stencil_op = {fail, depth_fail, pass};
}
void GLINTFX_GL_APIENTRY fake_stencil_mask(GLuint mask) { model.stencil_write_mask = mask; }
void GLINTFX_GL_APIENTRY fake_clear_stencil(GLint value) { model.stencil_clear_value = value; }
// A clear of the stencil writes the clear value through the write mask of that moment: the fake
// records both, so a mask set after the clear does not count.
void GLINTFX_GL_APIENTRY fake_clear(GLbitfield mask) {
    if ((mask & k_stencil_buffer_bit) != 0) {
        ++model.stencil_clears;
        model.write_mask_at_clear = model.stencil_write_mask;
        model.value_at_clear = model.stencil_clear_value;
    }
}

gl_function_table fake_table() {
    gl_function_table table;
    table.glEnable = &fake_enable;
    table.glDisable = &fake_disable;
    table.glBlendFuncSeparate = &fake_blend_func_separate;
    table.glColorMask = &fake_color_mask;
    table.glBlendEquationSeparate = &fake_blend_equation_separate;
    table.glPolygonMode = &fake_polygon_mode;
    table.glBindFramebuffer = &fake_bind_framebuffer;
    table.glViewport = &fake_viewport;
    table.glUseProgram = &fake_use_program;
    table.glBindVertexArray = &fake_bind_vertex_array;
    table.glBindBuffer = &fake_bind_buffer;
    table.glActiveTexture = &fake_active_texture;
    table.glBindTexture = &fake_bind_texture;
    table.glPixelStorei = &fake_pixel_storei;
    table.glStencilFunc = &fake_stencil_func;
    table.glStencilOp = &fake_stencil_op;
    table.glStencilMask = &fake_stencil_mask;
    table.glClearStencil = &fake_clear_stencil;
    table.glClear = &fake_clear;
    return table;
}

constexpr draw_state_request k_request{.program = 7,
                                       .vertex_array = 8,
                                       .vertex_buffer = 9,
                                       .index_buffer = 10,
                                       .viewport_width = 320,
                                       .viewport_height = 240,
                                       .srgb_framebuffer = false};

// The stencil at the values GL starts with: function ALWAYS, reference 0, every bit; KEEP on every
// outcome; every bit writable (R2D-SPRITE-SHADE H3a).
[[nodiscard]] bool stencil_at_gl_defaults() {
    return model.stencil_function == k_always && model.stencil_reference == 0 &&
           model.stencil_read_mask == k_every_bit &&
           model.stencil_op == std::array<GLenum, 3>{k_keep, k_keep, k_keep} &&
           model.stencil_write_mask == k_every_bit;
}

} // namespace

GLINTFX_TEST(gl_state_contract_setting_state_overrides_every_hostile_item) {
    const gl_function_table table = fake_table();
    for (const bool srgb : {false, true}) {
        plant_hostile_state();
        draw_state_request request = k_request;
        request.srgb_framebuffer = srgb;
        set_gl_state_for_drawing(table, request);
        GLINTFX_CHECK(model.blend);
        GLINTFX_CHECK((model.blend_func == std::array<GLenum, 4>{k_one, k_one_minus_src_alpha,
                                                                 k_one, k_one_minus_src_alpha}));
        GLINTFX_CHECK(!model.depth_test);
        GLINTFX_CHECK(!model.stencil_test);
        GLINTFX_CHECK(!model.scissor_test);
        GLINTFX_CHECK(!model.cull_face);
        GLINTFX_CHECK((model.color_mask == std::array<bool, 4>{true, true, true, true}));
        GLINTFX_CHECK((model.viewport == std::array<GLint, 4>{0, 0, 320, 240}));
        GLINTFX_CHECK_EQ(model.framebuffer_srgb, srgb);
        GLINTFX_CHECK((model.blend_equation == std::array<GLenum, 2>{k_func_add, k_func_add}));
        GLINTFX_CHECK((model.polygon_mode == std::array<GLenum, 2>{k_fill, k_fill}));
        GLINTFX_CHECK(!model.rasterizer_discard);
        GLINTFX_CHECK(!model.color_logic_op);
        GLINTFX_CHECK(!model.sample_alpha_to_coverage);
        GLINTFX_CHECK_EQ(model.draw_framebuffer, GLuint{0});
        GLINTFX_CHECK_EQ(model.program, GLuint{7});
        GLINTFX_CHECK_EQ(model.vertex_array, GLuint{8});
        GLINTFX_CHECK_EQ(model.array_buffer, GLuint{9});
        GLINTFX_CHECK_EQ(model.element_buffer, GLuint{10});
        GLINTFX_CHECK_EQ(model.active_texture, k_texture0);
        GLINTFX_CHECK_EQ(model.unpack_alignment, 1);
    }
}

// The viewport is the SIZE it is given, not a constant and not the previous one.
GLINTFX_TEST(gl_state_contract_the_viewport_takes_the_size_it_is_given) {
    const gl_function_table table = fake_table();
    plant_hostile_state();
    draw_state_request request = k_request;
    request.viewport_width = 1024;
    request.viewport_height = 7;
    set_gl_state_for_drawing(table, request);
    GLINTFX_CHECK((model.viewport == std::array<GLint, 4>{0, 0, 1024, 7}));
    leave_gl_state_after_drawing(table, 55, 66, false);
    GLINTFX_CHECK((model.viewport == std::array<GLint, 4>{0, 0, 55, 66}));
}

// The closed list of what is LEFT, every item read back, from a hostile start.
GLINTFX_TEST(gl_state_contract_leaving_state_reads_back_the_closed_list) {
    const gl_function_table table = fake_table();
    for (const bool srgb : {false, true}) {
        plant_hostile_state();
        leave_gl_state_after_drawing(table, 800, 600, srgb);
        GLINTFX_CHECK_EQ(model.program, GLuint{0});
        GLINTFX_CHECK_EQ(model.vertex_array, GLuint{0});
        GLINTFX_CHECK_EQ(model.array_buffer, GLuint{0});
        GLINTFX_CHECK_EQ(model.element_buffer, GLuint{0});
        GLINTFX_CHECK_EQ(model.texture_2d, GLuint{0});
        GLINTFX_CHECK_EQ(model.active_texture, k_texture0);
        GLINTFX_CHECK(model.blend);
        GLINTFX_CHECK((model.blend_func == std::array<GLenum, 4>{k_one, k_one_minus_src_alpha,
                                                                 k_one, k_one_minus_src_alpha}));
        GLINTFX_CHECK(!model.depth_test);
        GLINTFX_CHECK(!model.stencil_test);
        GLINTFX_CHECK(!model.scissor_test);
        GLINTFX_CHECK(!model.cull_face);
        GLINTFX_CHECK((model.color_mask == std::array<bool, 4>{true, true, true, true}));
        GLINTFX_CHECK((model.viewport == std::array<GLint, 4>{0, 0, 800, 600}));
        GLINTFX_CHECK_EQ(model.framebuffer_srgb, srgb);
        GLINTFX_CHECK((model.blend_equation == std::array<GLenum, 2>{k_func_add, k_func_add}));
        GLINTFX_CHECK((model.polygon_mode == std::array<GLenum, 2>{k_fill, k_fill}));
        GLINTFX_CHECK(!model.rasterizer_discard);
        GLINTFX_CHECK(!model.color_logic_op);
        GLINTFX_CHECK(!model.sample_alpha_to_coverage);
        GLINTFX_CHECK_EQ(model.draw_framebuffer, GLuint{0});
    }
    std::println(
        "gl_state_contract_test: lista fechada conferida item a item (14 itens, 2 modos de sRGB)");
}

// Set and then leave: the two halves compose (the state a frame ends in is the closed list, not
// what the drawing set).
GLINTFX_TEST(gl_state_contract_a_frame_ends_in_the_closed_list_not_in_the_drawing_state) {
    const gl_function_table table = fake_table();
    plant_hostile_state();
    set_gl_state_for_drawing(table, k_request);
    GLINTFX_CHECK_EQ(model.program, GLuint{7});
    leave_gl_state_after_drawing(table, 320, 240, false);
    GLINTFX_CHECK_EQ(model.program, GLuint{0});
    GLINTFX_CHECK_EQ(model.vertex_array, GLuint{0});
    GLINTFX_CHECK_EQ(model.array_buffer, GLuint{0});
    GLINTFX_CHECK_EQ(model.element_buffer, GLuint{0});
}

// R2D-SPRITE-SHADE H3a: each pass of shadow_group.hpp, from a hostile start (stencil test on, color
// mask off, a consumer's stencil), sets everything it needs. plain: test off, color on.
// mark_shadow: EQUAL 0 with every bit, KEEP KEEP INCR, every bit writable, color on. release_marks:
// ALWAYS 0 with every bit, KEEP KEEP ZERO, every bit writable, color OFF.
GLINTFX_TEST(gl_state_contract_each_stencil_pass_sets_all_it_needs) {
    const gl_function_table table = fake_table();
    const std::array<bool, 4> color_on{true, true, true, true};
    std::size_t analyzed = 0;
    plant_hostile_state();
    plant_hostile_stencil();
    set_stencil_pass(table, stencil_pass::plain);
    GLINTFX_CHECK(!model.stencil_test && model.color_mask == color_on);
    ++analyzed;
    plant_hostile_state();
    plant_hostile_stencil();
    set_stencil_pass(table, stencil_pass::mark_shadow);
    GLINTFX_CHECK(model.stencil_test && model.color_mask == color_on);
    GLINTFX_CHECK(model.stencil_function == k_equal && model.stencil_reference == 0 &&
                  model.stencil_read_mask == k_every_bit);
    GLINTFX_CHECK((model.stencil_op == std::array<GLenum, 3>{k_keep, k_keep, k_incr}));
    GLINTFX_CHECK_EQ(model.stencil_write_mask, k_every_bit);
    ++analyzed;
    plant_hostile_state();
    plant_hostile_stencil();
    set_stencil_pass(table, stencil_pass::release_marks);
    GLINTFX_CHECK((model.stencil_test && model.color_mask == std::array<bool, 4>{}));
    GLINTFX_CHECK(model.stencil_function == k_always && model.stencil_reference == 0 &&
                  model.stencil_read_mask == k_every_bit);
    GLINTFX_CHECK((model.stencil_op == std::array<GLenum, 3>{k_keep, k_keep, k_zero}));
    GLINTFX_CHECK_EQ(model.stencil_write_mask, k_every_bit);
    ++analyzed;
    std::println("gl_state_contract_test: passadas de stencil encontradas=3 analisadas={}",
                 analyzed);
}

// R2D-SPRITE-SHADE H3a: the clear writes 0 with every bit writable, whatever mask the consumer left
// (a clear obeys the write mask: the fake records the mask of the moment of the clear).
GLINTFX_TEST(gl_state_contract_clearing_the_stencil_makes_every_bit_writable_first) {
    const gl_function_table table = fake_table();
    plant_hostile_state();
    plant_hostile_stencil();
    clear_stencil(table);
    GLINTFX_CHECK_EQ(model.stencil_clears, 1);
    GLINTFX_CHECK_EQ(model.write_mask_at_clear, k_every_bit);
    GLINTFX_CHECK_EQ(model.value_at_clear, 0);
}

// R2D-SPRITE-SHADE H3a: after a pass that turned the stencil on and the color off, leaving puts the
// stencil test off, the color on and the stencil at the GL defaults; setting the state for drawing
// puts the same stencil defaults (the two halves agree).
GLINTFX_TEST(gl_state_contract_both_halves_leave_the_stencil_at_the_gl_defaults) {
    const gl_function_table table = fake_table();
    plant_hostile_state();
    plant_hostile_stencil();
    set_stencil_pass(table, stencil_pass::release_marks);
    leave_gl_state_after_drawing(table, 320, 240, false);
    GLINTFX_CHECK(!model.stencil_test);
    GLINTFX_CHECK((model.color_mask == std::array<bool, 4>{true, true, true, true}));
    GLINTFX_CHECK(stencil_at_gl_defaults());
    plant_hostile_state();
    plant_hostile_stencil();
    set_gl_state_for_drawing(table, k_request);
    GLINTFX_CHECK(!model.stencil_test);
    GLINTFX_CHECK(stencil_at_gl_defaults());
}
