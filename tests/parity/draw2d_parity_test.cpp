// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/draw2d/renderer_2d.hpp>
#include <glintfx/platform/gl/context.hpp>
#include <glintfx/platform/gl/gfx_option.hpp>
#include <glintfx/platform/window/display.hpp>
#include <glintfx/platform/window/window.hpp>

// draw2d_parity_test.cpp - R2D-BATCH, fatias B4 e B5 (docs/plano-w7d.md sec. 4.3, errata sec. 19 e
// 21): the 2D drawing layer through the PUBLIC API only, READ BACK AS PIXELS from a real surface,
// on both systems. ONE file, no #if, no harness, the shape of gl_context_parity_test.cpp: the
// Windows side links it into an add_executable() target (tests/CMakeLists.txt, if(WIN32)); the
// Linux side runs it as a container fixture of this exact name, inside a nested compositor
// (GODS_LAWS.md L-09), never on the live session.
//
// THE CRITERION IS FIXED HERE, BEFORE THE DATA EXISTS (GODS_LAWS.md L-43):
// ASSERTED, on both systems (a genuine FAIL and EXIT_FAILURE the moment one disagrees):
//   - open() refuses a context that is not open, by name; finish_frame() with no frame open is
//   refused by
//     name;
//   - the ORDER: the eight cells {no layer, equal, lower, higher} x {A then B, B then A} over two
//     overlapping squares, the color of the overlap read at the pixel (exact, opaque pieces);
//   - the PREMULTIPLIED EDGE: a half-transparent piece over black gives 128 (+-2) in a surface that
//   does not
//     encode (the option srgb_framebuffer OFF: the color is encoded before blending) and 188 (+-2)
//     in one that does (ON: blending in linear light, the surface encodes); a half-transparent red
//     keeps green and blue at EXACTLY zero (no dark fringe, which straight-alpha blending of
//     premultiplied data would give);
//   - the HOSTILE STATE planted before the frame (depth, stencil, a one-pixel scissor, front faces
//   culled,
//     color mask off, blending swapped and the equation changed, wireframe polygon mode, rasterizer
//     discard, logic op, alpha-to-coverage, a draw framebuffer, a bound texture, a program in use):
//     the clear and the piece still land as exact pixels, and after flush() AND after
//     finish_frame() the closed list of the state left behind reads back exactly
//     (docs/auditoria-api-draw2d.md B0-I5);
//   - an EMPTY frame reports all zeros and shows the clear color; begin_frame(nullopt) does not
//   clear;
//   - TWO RECTANGLES sharing an edge, half-transparent, read along the row: every covered pixel
//   equal (a gap
//     would show zero, an overlap a brighter value), the pixels outside untouched;
//   - ONE draw call for N pieces of equal state (and 0 for none).
// The surface encoding is read in BOTH modes: two contexts opened in sequence, because
// srgb_framebuffer is open_only. A driver that does not support the option is a DECLARED and
// COUNTED absence (printed, and a MEASURED key, listed per platform in
// tests/measured_exceptions.txt), never a silent skip. MEASURED, PRINTED, NEVER ASSERTED EQUAL:
// what the driver leaves after make_current, the raw channel values of the half-transparent cells,
// the number of draw calls of the N-piece frame.
//
// Geometry assumes a scale factor of 1 (both fixtures run at 1): pixels are the numbers written
// below.

namespace {

using gl_enum = unsigned int;
using gl_uint = unsigned int;
using gl_int = int;
using gl_sizei = int;
using gl_bool = unsigned char;

constexpr gl_enum k_gl_blend = 0x0BE2;
constexpr gl_enum k_gl_cull_face = 0x0B44;
constexpr gl_enum k_gl_depth_test = 0x0B71;
constexpr gl_enum k_gl_stencil_test = 0x0B90;
constexpr gl_enum k_gl_scissor_test = 0x0C11;
constexpr gl_enum k_gl_framebuffer_srgb = 0x8DB9;
constexpr gl_enum k_gl_rasterizer_discard = 0x8C89;
constexpr gl_enum k_gl_color_logic_op = 0x0BF2;
constexpr gl_enum k_gl_sample_alpha_to_coverage = 0x809E;
constexpr gl_enum k_gl_front = 0x0404;
constexpr gl_enum k_gl_front_and_back = 0x0408;
constexpr gl_enum k_gl_line = 0x1B01;
constexpr gl_enum k_gl_fill = 0x1B02;
constexpr gl_enum k_gl_zero = 0x0000;
constexpr gl_enum k_gl_one = 0x0001;
constexpr gl_enum k_gl_one_minus_src_alpha = 0x0303;
constexpr gl_enum k_gl_func_add = 0x8006;
constexpr gl_enum k_gl_func_subtract = 0x800A;
constexpr gl_enum k_gl_viewport = 0x0BA2;
constexpr gl_enum k_gl_polygon_mode = 0x0B40;
constexpr gl_enum k_gl_color_writemask = 0x0C23;
constexpr gl_enum k_gl_blend_dst_rgb = 0x80C8;
constexpr gl_enum k_gl_blend_src_rgb = 0x80C9;
constexpr gl_enum k_gl_blend_dst_alpha = 0x80CA;
constexpr gl_enum k_gl_blend_src_alpha = 0x80CB;
constexpr gl_enum k_gl_blend_equation_rgb = 0x8009;
constexpr gl_enum k_gl_blend_equation_alpha = 0x883D;
constexpr gl_enum k_gl_current_program = 0x8B8D;
constexpr gl_enum k_gl_vertex_array_binding = 0x85B5;
constexpr gl_enum k_gl_array_buffer_binding = 0x8894;
constexpr gl_enum k_gl_draw_framebuffer_binding = 0x8CA6;
constexpr gl_enum k_gl_texture_binding_2d = 0x8069;
constexpr gl_enum k_gl_active_texture = 0x84E0;
constexpr gl_enum k_gl_draw_framebuffer = 0x8CA9;
constexpr gl_enum k_gl_texture_2d = 0x0DE1;
constexpr gl_enum k_gl_texture0 = 0x84C0;
constexpr gl_enum k_gl_texture3 = 0x84C3;
constexpr gl_enum k_gl_vertex_shader = 0x8B31;
constexpr gl_enum k_gl_fragment_shader = 0x8B30;
constexpr gl_enum k_gl_rgba = 0x1908;
constexpr gl_enum k_gl_unsigned_byte = 0x1401;

// The GL entry points the test needs, resolved through the PUBLIC gltfx_gl_context::proc_address().
struct gl_api {
    void (*get_integerv)(gl_enum, gl_int *) = nullptr;
    void (*get_booleanv)(gl_enum, gl_bool *) = nullptr;
    gl_bool (*is_enabled)(gl_enum) = nullptr;
    void (*enable)(gl_enum) = nullptr;
    void (*disable)(gl_enum) = nullptr;
    void (*scissor)(gl_int, gl_int, gl_sizei, gl_sizei) = nullptr;
    void (*cull_face)(gl_enum) = nullptr;
    void (*color_mask)(gl_bool, gl_bool, gl_bool, gl_bool) = nullptr;
    void (*blend_func)(gl_enum, gl_enum) = nullptr;
    void (*blend_equation)(gl_enum) = nullptr;
    void (*polygon_mode)(gl_enum, gl_enum) = nullptr;
    void (*gen_framebuffers)(gl_sizei, gl_uint *) = nullptr;
    void (*bind_framebuffer)(gl_enum, gl_uint) = nullptr;
    void (*delete_framebuffers)(gl_sizei, const gl_uint *) = nullptr;
    void (*gen_textures)(gl_sizei, gl_uint *) = nullptr;
    void (*bind_texture)(gl_enum, gl_uint) = nullptr;
    void (*delete_textures)(gl_sizei, const gl_uint *) = nullptr;
    void (*active_texture)(gl_enum) = nullptr;
    gl_uint (*create_shader)(gl_enum) = nullptr;
    void (*shader_source)(gl_uint, gl_sizei, const char *const *, const gl_int *) = nullptr;
    void (*compile_shader)(gl_uint) = nullptr;
    gl_uint (*create_program)() = nullptr;
    void (*attach_shader)(gl_uint, gl_uint) = nullptr;
    void (*link_program)(gl_uint) = nullptr;
    void (*use_program)(gl_uint) = nullptr;
    void (*delete_shader)(gl_uint) = nullptr;
    void (*delete_program)(gl_uint) = nullptr;
    void (*finish)() = nullptr;
    void (*read_pixels)(gl_int, gl_int, gl_sizei, gl_sizei, gl_enum, gl_enum, void *) = nullptr;
};

template <typename F> bool resolve(glintfx::gltfx_gl_context &context, const char *name, F &out) {
    void *address = context.proc_address(name);
    if (address == nullptr) {
        std::fprintf(stderr, "draw2d_parity_test: FAIL %s nao resolvido pelo contexto\n", name);
        return false;
    }
    out = reinterpret_cast<F>(address);
    return true;
}

bool load_gl(glintfx::gltfx_gl_context &context, gl_api &gl) {
    return resolve(context, "glGetIntegerv", gl.get_integerv) &&
           resolve(context, "glGetBooleanv", gl.get_booleanv) &&
           resolve(context, "glIsEnabled", gl.is_enabled) &&
           resolve(context, "glEnable", gl.enable) && resolve(context, "glDisable", gl.disable) &&
           resolve(context, "glScissor", gl.scissor) &&
           resolve(context, "glCullFace", gl.cull_face) &&
           resolve(context, "glColorMask", gl.color_mask) &&
           resolve(context, "glBlendFunc", gl.blend_func) &&
           resolve(context, "glBlendEquation", gl.blend_equation) &&
           resolve(context, "glPolygonMode", gl.polygon_mode) &&
           resolve(context, "glGenFramebuffers", gl.gen_framebuffers) &&
           resolve(context, "glBindFramebuffer", gl.bind_framebuffer) &&
           resolve(context, "glDeleteFramebuffers", gl.delete_framebuffers) &&
           resolve(context, "glGenTextures", gl.gen_textures) &&
           resolve(context, "glBindTexture", gl.bind_texture) &&
           resolve(context, "glDeleteTextures", gl.delete_textures) &&
           resolve(context, "glActiveTexture", gl.active_texture) &&
           resolve(context, "glCreateShader", gl.create_shader) &&
           resolve(context, "glShaderSource", gl.shader_source) &&
           resolve(context, "glCompileShader", gl.compile_shader) &&
           resolve(context, "glCreateProgram", gl.create_program) &&
           resolve(context, "glAttachShader", gl.attach_shader) &&
           resolve(context, "glLinkProgram", gl.link_program) &&
           resolve(context, "glUseProgram", gl.use_program) &&
           resolve(context, "glDeleteShader", gl.delete_shader) &&
           resolve(context, "glDeleteProgram", gl.delete_program) &&
           resolve(context, "glFinish", gl.finish) &&
           resolve(context, "glReadPixels", gl.read_pixels);
}

struct pixel {
    int red = 0;
    int green = 0;
    int blue = 0;
};

// Everything one cell needs: the renderer under test, the GL entry points, the mode of the surface
// and the height of the surface (glReadPixels counts rows from the BOTTOM, the drawing from the
// TOP).
struct cells_context {
    glintfx::gltfx_renderer_2d &renderer;
    const gl_api &gl;
    bool srgb;
    int height;
    const char *mode; // "off" or "on", for the messages
};

int g_cells = 0;
int g_failures = 0;

void report(const cells_context &c, const char *cell, bool ok, const char *detail = "") {
    ++g_cells;
    if (!ok) {
        ++g_failures;
        std::fprintf(stderr, "draw2d_parity_test: FAIL [srgb=%s] %s %s\n", c.mode, cell, detail);
    }
}

pixel read_at(const cells_context &c, int x, int y) {
    unsigned char rgba[4] = {0, 0, 0, 0};
    c.gl.finish();
    c.gl.read_pixels(x, c.height - 1 - y, 1, 1, k_gl_rgba, k_gl_unsigned_byte, rgba);
    return pixel{rgba[0], rgba[1], rgba[2]};
}

bool is_exact(pixel p, int red, int green, int blue) {
    return p.red == red && p.green == green && p.blue == blue;
}

bool within(int got, int want, int tolerance) {
    return got >= want - tolerance && got <= want + tolerance;
}

const glintfx::gltfx_rgba k_red{1.0F, 0.0F, 0.0F, 1.0F};
const glintfx::gltfx_rgba k_blue{0.0F, 0.0F, 1.0F, 1.0F};
const glintfx::gltfx_rgba k_half_white{1.0F, 1.0F, 1.0F, 0.5F};
const glintfx::gltfx_rgba k_half_red{1.0F, 0.0F, 0.0F, 0.5F};

glintfx::gltfx_rect_world rect(double x, double y, double width, double height) {
    return glintfx::gltfx_rect_world{{x, y}, {width, height}};
}

// The value a half-transparent white over black reads as: 128 when the surface does not encode (the
// color is encoded before blending), 188 when it does (blended in linear light, then encoded).
int half_value(const cells_context &c) { return c.srgb ? 188 : 128; }

// ---------------------------------------------------------------------------------------------------------
// C1: the eight cells of ORDER.
// ---------------------------------------------------------------------------------------------------------
enum class layer_case { none, equal, lower, higher };

void cell_order(const cells_context &c) {
    // A red at (40,40) 80x80, B blue at (80,80) 80x80: the overlap is x 80..119, y 80..119.
    const glintfx::gltfx_rect_world a = rect(40, 40, 80, 80);
    const glintfx::gltfx_rect_world b = rect(80, 80, 80, 80);
    const layer_case cases[] = {layer_case::none, layer_case::equal, layer_case::lower,
                                layer_case::higher};
    const char *names[] = {"sem camada", "camada igual", "A abaixo de B", "A acima de B"};
    int analyzed = 0;
    for (std::size_t k = 0; k < 4; ++k) {
        for (int a_first = 1; a_first >= 0; --a_first) {
            glintfx::gltfx_draw_layer layer_a{};
            glintfx::gltfx_draw_layer layer_b{};
            if (cases[k] == layer_case::equal) {
                layer_a = {5};
                layer_b = {5};
            } else if (cases[k] == layer_case::lower) {
                layer_a = {1};
                layer_b = {2};
            } else if (cases[k] == layer_case::higher) {
                layer_a = {2};
                layer_b = {1};
            }
            c.renderer.begin_frame(glintfx::gltfx_frame_2d_desc{});
            for (int step = 0; step < 2; ++step) {
                const bool draw_a = (step == 0) == (a_first == 1);
                if (cases[k] == layer_case::none) {
                    c.renderer.fill_rect(draw_a ? a : b, draw_a ? k_red : k_blue);
                } else {
                    c.renderer.fill_rect(draw_a ? a : b, draw_a ? k_red : k_blue,
                                         draw_a ? layer_a : layer_b);
                }
            }
            const auto finished = c.renderer.finish_frame();
            // the top piece at the overlap: the last submitted when the layers do not decide, else
            // the higher
            bool a_on_top = a_first == 0;
            if (cases[k] == layer_case::lower) {
                a_on_top = false;
            } else if (cases[k] == layer_case::higher) {
                a_on_top = true;
            }
            const pixel overlap = read_at(c, 100, 100);
            const bool ok =
                finished.has_value() &&
                (a_on_top ? is_exact(overlap, 255, 0, 0) : is_exact(overlap, 0, 0, 255)) &&
                is_exact(read_at(c, 50, 50), 255, 0, 0) &&
                is_exact(read_at(c, 150, 150), 0, 0, 255) && is_exact(read_at(c, 10, 10), 0, 0, 0);
            std::string detail = std::string("celula de ordem \"") + names[k] +
                                 (a_first == 1 ? "\" A depois B" : "\" B depois A");
            report(c, detail.c_str(), ok);
            ++analyzed;
        }
    }
    std::fprintf(stdout, "draw2d_parity_test: ordem, %d celula(s) lida(s) no pixel [srgb=%s]\n",
                 analyzed, c.mode);
    report(c, "as 8 celulas de ordem foram todas analisadas", analyzed == 8);
}

// ---------------------------------------------------------------------------------------------------------
// C2: the premultiplied edge, and the two surface encodings.
// ---------------------------------------------------------------------------------------------------------
void cell_premultiplied_edge(const cells_context &c) {
    c.renderer.begin_frame(glintfx::gltfx_frame_2d_desc{});
    c.renderer.fill_rect(rect(20, 20, 60, 60), k_half_white);
    c.renderer.fill_rect(rect(120, 20, 60, 60), k_half_red);
    const auto finished = c.renderer.finish_frame();
    const pixel white = read_at(c, 50, 50);
    const pixel red = read_at(c, 150, 50);
    const int want = half_value(c);
    std::fprintf(stdout, "MEASURED draw2d_parity_test.half_white_srgb_%s=%d\n", c.mode, white.red);
    std::fprintf(stdout, "MEASURED draw2d_parity_test.half_red_srgb_%s=%d\n", c.mode, red.red);
    report(c, "a borda meio transparente: branco sobre preto",
           finished.has_value() && within(white.red, want, 2) && within(white.green, want, 2) &&
               within(white.blue, want, 2));
    report(c, "a borda pre-multiplicada: vermelho meio transparente, verde e azul EXATAMENTE zero",
           within(red.red, want, 2) && red.green == 0 && red.blue == 0);
}

// ---------------------------------------------------------------------------------------------------------
// C4: the hostile state, and the closed list read back after flush() and after finish_frame().
// ---------------------------------------------------------------------------------------------------------
struct hostile_objects {
    gl_uint framebuffer = 0;
    gl_uint texture = 0;
    gl_uint program = 0;
};

const char *const k_minimal_vertex =
    "#version 330 core\nvoid main() { gl_Position = vec4(0.0); }\n";
const char *const k_minimal_fragment =
    "#version 330 core\nout vec4 c;\nvoid main() { c = vec4(1.0); }\n";

hostile_objects plant_hostile_state(const gl_api &gl) {
    hostile_objects objects;
    gl.enable(k_gl_depth_test);
    gl.enable(k_gl_stencil_test);
    gl.enable(k_gl_scissor_test);
    gl.scissor(0, 0, 1, 1); // a one-pixel scissor
    gl.enable(k_gl_cull_face);
    gl.cull_face(k_gl_front); // front faces culled
    gl.color_mask(0, 0, 0, 0);
    gl.enable(k_gl_blend);
    gl.blend_func(k_gl_zero, k_gl_zero); // blending swapped
    gl.blend_equation(k_gl_func_subtract);
    gl.polygon_mode(k_gl_front_and_back, k_gl_line);
    gl.enable(k_gl_rasterizer_discard);
    gl.enable(k_gl_color_logic_op);
    gl.enable(k_gl_sample_alpha_to_coverage);
    gl.gen_framebuffers(1, &objects.framebuffer);
    gl.bind_framebuffer(k_gl_draw_framebuffer, objects.framebuffer); // a consumer's FBO
    gl.gen_textures(1, &objects.texture);
    gl.active_texture(k_gl_texture0);
    gl.bind_texture(k_gl_texture_2d, objects.texture);
    const gl_uint vertex = gl.create_shader(k_gl_vertex_shader);
    const gl_uint fragment = gl.create_shader(k_gl_fragment_shader);
    gl.shader_source(vertex, 1, &k_minimal_vertex, nullptr);
    gl.shader_source(fragment, 1, &k_minimal_fragment, nullptr);
    gl.compile_shader(vertex);
    gl.compile_shader(fragment);
    objects.program = gl.create_program();
    gl.attach_shader(objects.program, vertex);
    gl.attach_shader(objects.program, fragment);
    gl.link_program(objects.program);
    gl.delete_shader(vertex);
    gl.delete_shader(fragment);
    gl.use_program(objects.program);  // a consumer's program in use
    gl.active_texture(k_gl_texture3); // and another texture unit active
    return objects;
}

void discard_hostile_objects(const gl_api &gl, hostile_objects &objects) {
    gl.use_program(0);
    gl.active_texture(k_gl_texture0);
    gl.bind_texture(k_gl_texture_2d, 0);
    gl.bind_framebuffer(k_gl_draw_framebuffer, 0);
    gl.delete_program(objects.program);
    gl.delete_textures(1, &objects.texture);
    gl.delete_framebuffers(1, &objects.framebuffer);
}

// Reads every item of the closed list; returns how many items were wrong and prints each one that
// is.
int check_closed_list(const cells_context &c, const char *when, int surface_width) {
    const gl_api &gl = c.gl;
    int wrong = 0;
    const auto expect = [&](const char *item, bool ok) {
        if (!ok) {
            ++wrong;
            std::fprintf(stderr, "draw2d_parity_test: FAIL [srgb=%s] lista fechada %s: %s\n",
                         c.mode, when, item);
        }
    };
    const auto integer = [&](gl_enum name) {
        gl_int value = -1;
        gl.get_integerv(name, &value);
        return value;
    };
    expect("blend ligado", gl.is_enabled(k_gl_blend) != 0);
    expect("blend_src_rgb ONE", integer(k_gl_blend_src_rgb) == static_cast<gl_int>(k_gl_one));
    expect("blend_dst_rgb ONE_MINUS_SRC_ALPHA",
           integer(k_gl_blend_dst_rgb) == static_cast<gl_int>(k_gl_one_minus_src_alpha));
    expect("blend_src_alpha ONE", integer(k_gl_blend_src_alpha) == static_cast<gl_int>(k_gl_one));
    expect("blend_dst_alpha ONE_MINUS_SRC_ALPHA",
           integer(k_gl_blend_dst_alpha) == static_cast<gl_int>(k_gl_one_minus_src_alpha));
    expect("equacao RGB ADD",
           integer(k_gl_blend_equation_rgb) == static_cast<gl_int>(k_gl_func_add));
    expect("equacao alfa ADD",
           integer(k_gl_blend_equation_alpha) == static_cast<gl_int>(k_gl_func_add));
    expect("depth desligado", gl.is_enabled(k_gl_depth_test) == 0);
    expect("stencil desligado", gl.is_enabled(k_gl_stencil_test) == 0);
    expect("recorte desligado", gl.is_enabled(k_gl_scissor_test) == 0);
    expect("cull desligado", gl.is_enabled(k_gl_cull_face) == 0);
    expect("descarte do rasterizador desligado", gl.is_enabled(k_gl_rasterizer_discard) == 0);
    expect("logic op desligado", gl.is_enabled(k_gl_color_logic_op) == 0);
    expect("alpha-to-coverage desligado", gl.is_enabled(k_gl_sample_alpha_to_coverage) == 0);
    gl_bool mask[4] = {0, 0, 0, 0};
    gl.get_booleanv(k_gl_color_writemask, mask);
    expect("mascara de cor toda ligada",
           mask[0] != 0 && mask[1] != 0 && mask[2] != 0 && mask[3] != 0);
    gl_int polygon[2] = {0, 0};
    gl.get_integerv(k_gl_polygon_mode, polygon);
    expect("modo de poligono FILL nas duas faces",
           polygon[0] == static_cast<gl_int>(k_gl_fill) &&
               polygon[1] == static_cast<gl_int>(k_gl_fill));
    expect("framebuffer de desenho 0", integer(k_gl_draw_framebuffer_binding) == 0);
    gl_int viewport[4] = {-1, -1, -1, -1};
    gl.get_integerv(k_gl_viewport, viewport);
    expect("viewport sobre a superficie inteira", viewport[0] == 0 && viewport[1] == 0 &&
                                                      viewport[2] == surface_width &&
                                                      viewport[3] == c.height);
    expect("FRAMEBUFFER_SRGB segue a opcao", (gl.is_enabled(k_gl_framebuffer_srgb) != 0) == c.srgb);
    expect("programa 0", integer(k_gl_current_program) == 0);
    expect("vertex array 0", integer(k_gl_vertex_array_binding) == 0);
    expect("array buffer 0", integer(k_gl_array_buffer_binding) == 0);
    expect("unidade de textura ativa 0",
           integer(k_gl_active_texture) == static_cast<gl_int>(k_gl_texture0));
    expect("textura 2D da unidade 0 desligada", integer(k_gl_texture_binding_2d) == 0);
    return wrong;
}

void cell_hostile_state(const cells_context &c, int surface_width) {
    hostile_objects objects = plant_hostile_state(c.gl);
    // the clear color: exact channels in both surface modes
    c.renderer.begin_frame(
        glintfx::gltfx_frame_2d_desc{glintfx::gltfx_rgba{0.0F, 1.0F, 0.0F, 1.0F}});
    c.renderer.fill_rect(rect(60, 60, 60, 60), k_red);
    c.renderer.flush();
    const pixel piece_after_flush = read_at(c, 90, 90);
    const pixel clear_after_flush = read_at(c, 10, 10);
    const int wrong_after_flush = check_closed_list(c, "depois do flush()", surface_width);
    const auto finished = c.renderer.finish_frame();
    const int wrong_after_finish = check_closed_list(c, "depois do finish_frame()", surface_width);
    report(c, "estado hostil: o clear sai verde exato", is_exact(clear_after_flush, 0, 255, 0));
    report(c, "estado hostil: a peca sai vermelha exata", is_exact(piece_after_flush, 255, 0, 0));
    report(c, "estado hostil: o quadro termina sem erro", finished.has_value());
    report(c, "lista fechada do estado depois do flush()", wrong_after_flush == 0);
    report(c, "lista fechada do estado depois do fim do quadro", wrong_after_finish == 0);
    discard_hostile_objects(c.gl, objects);
}

// ---------------------------------------------------------------------------------------------------------
// C5: the empty frame, and a frame that does not clear.
// ---------------------------------------------------------------------------------------------------------
void cell_empty_frame(const cells_context &c) {
    c.renderer.begin_frame(
        glintfx::gltfx_frame_2d_desc{glintfx::gltfx_rgba{0.0F, 1.0F, 0.0F, 1.0F}});
    const auto finished = c.renderer.finish_frame();
    bool zeros = false;
    if (finished.has_value()) {
        const glintfx::gltfx_frame_2d_report r = finished.value();
        zeros = r.pieces_submitted == 0 && r.pieces_drawn == 0 && r.pieces_refused == 0 &&
                r.pieces_dropped_out_of_memory == 0 && r.pieces_dropped_graphics_failure == 0 &&
                r.pieces_dropped_outside_frame == 0 && r.draw_calls == 0;
    }
    report(c, "quadro vazio: o relatorio e todo zero", zeros);
    report(c, "quadro vazio: mostra a cor do clear", is_exact(read_at(c, 160, 120), 0, 255, 0));
    // A frame with no clear leaves the surface as it was (nothing was swapped): the green stays
    // around a new piece.
    c.renderer.begin_frame(glintfx::gltfx_frame_2d_desc{std::nullopt});
    c.renderer.fill_rect(rect(10, 10, 20, 20), k_red);
    const auto second = c.renderer.finish_frame();
    report(c, "quadro sem clear: nao limpa (o verde continua ao redor da peca)",
           second.has_value() && is_exact(read_at(c, 20, 20), 255, 0, 0) &&
               is_exact(read_at(c, 160, 120), 0, 255, 0));
}

// ---------------------------------------------------------------------------------------------------------
// C6: two rectangles sharing an edge.
// ---------------------------------------------------------------------------------------------------------
void cell_shared_edge(const cells_context &c) {
    c.renderer.begin_frame(glintfx::gltfx_frame_2d_desc{});
    c.renderer.fill_rect(rect(40, 100, 40, 10), k_half_white); // x 40..79
    c.renderer.fill_rect(rect(80, 100, 40, 10), k_half_white); // x 80..119
    const auto finished = c.renderer.finish_frame();
    std::vector<unsigned char> row(88 * 4, 0);
    c.gl.finish();
    c.gl.read_pixels(36, c.height - 1 - 105, 88, 1, k_gl_rgba, k_gl_unsigned_byte, row.data());
    const int want = half_value(c);
    int gaps_or_overlaps = 0;
    int outside_touched = 0;
    for (int i = 0; i < 88; ++i) {
        const int x = 36 + i;
        const int value = row[static_cast<std::size_t>(i) * 4];
        if (x >= 40 && x < 120) {
            if (!within(value, want, 2)) {
                ++gaps_or_overlaps;
            }
        } else if (value != 0) {
            ++outside_touched;
        }
    }
    report(c, "dois retangulos lado a lado: nem fresta nem sobreposicao em nenhum dos 80 pixels",
           finished.has_value() && gaps_or_overlaps == 0);
    report(c, "dois retangulos lado a lado: nada fora deles", outside_touched == 0);
}

// ---------------------------------------------------------------------------------------------------------
// C7: one draw call for N pieces of equal state.
// ---------------------------------------------------------------------------------------------------------
void cell_draw_calls(const cells_context &c) {
    c.renderer.begin_frame(glintfx::gltfx_frame_2d_desc{});
    for (int i = 0; i < 100; ++i) {
        c.renderer.fill_rect(rect(3.0 * i, 200, 2, 2), k_red);
    }
    const auto many = c.renderer.finish_frame();
    c.renderer.begin_frame(glintfx::gltfx_frame_2d_desc{});
    c.renderer.fill_rect(rect(0, 200, 2, 2), k_red);
    const auto one = c.renderer.finish_frame();
    c.renderer.begin_frame(glintfx::gltfx_frame_2d_desc{});
    const auto none = c.renderer.finish_frame();
    if (many.has_value()) {
        std::fprintf(stdout, "MEASURED draw2d_parity_test.draw_calls_100_pieces_srgb_%s=%llu\n",
                     c.mode, static_cast<unsigned long long>(many.value().draw_calls));
    }
    report(c, "100 pecas de estado igual: UMA chamada de desenho e as 100 desenhadas",
           many.has_value() && many.value().draw_calls == 1 && many.value().pieces_drawn == 100);
    report(c, "1 peca: uma chamada de desenho", one.has_value() && one.value().draw_calls == 1);
    report(c, "nenhuma peca: zero chamadas de desenho",
           none.has_value() && none.value().draw_calls == 0);
}

int fail(const char *what) {
    std::fprintf(stderr, "draw2d_parity_test: FAIL %s\n", what);
    return EXIT_FAILURE;
}

std::string code_name(const glintfx::gltfx_err &error) {
    return std::string(glintfx::gltfx_err_code_name(error.code()));
}

// Opens a context with the surface encoding `srgb`, runs every cell over it, and destroys it.
// Returns false when the driver does not support the mode (a DECLARED absence, counted), true
// otherwise; a failed cell is counted in g_failures, and a failure to open anything at all ends the
// run with -1 through `fatal`.
enum class mode_result { ran, absent, fatal };

mode_result run_mode(glintfx::gltfx_display &display, bool srgb) {
    const char *mode = srgb ? "on" : "off";
    // A NEW window per mode: srgb_framebuffer is open_only, and the first context opened over a
    // window FIXES its open_only values (a second open with another value is refused as
    // invalid_argument - the fixation cases of gl_context_parity_test). The window is destroyed
    // after its context, which is after its renderer.
    const glintfx::gltfx_window_desc window_desc{
        .title = "janela de paridade do desenho 2D",
        .application_id = "org.glintfx.draw2d_parity_test",
        .logical_size = {.width = 320, .height = 240},
    };
    glintfx::gltfx_rslt<glintfx::gltfx_window> window_opened =
        glintfx::gltfx_window::open(display, window_desc);
    if (window_opened.has_error()) {
        std::fprintf(stderr, "draw2d_parity_test: FAIL gltfx_window::open(srgb=%s): %s\n", mode,
                     code_name(window_opened.err()).c_str());
        return mode_result::fatal;
    }
    glintfx::gltfx_window window = std::move(window_opened.value());
    const glintfx::gltfx_gfx_option_entry options[] = {
        {.id = glintfx::gltfx_gfx_option::srgb_framebuffer, .value = srgb ? 1 : 0}};
    const glintfx::gltfx_gl_context_desc desc{.options = options, .option_count = 1};
    glintfx::gltfx_rslt<glintfx::gltfx_gl_context> opened =
        glintfx::gltfx_gl_context::open(window, desc);
    if (opened.has_error()) {
        if (srgb && opened.err().code() == glintfx::gltfx_err_code::unsupported) {
            std::fprintf(stdout,
                         "draw2d_parity_test: AUSENCIA DECLARADA: srgb_framebuffer=on nao e "
                         "suportado aqui (%s)\n",
                         std::string(opened.err().rejected_value()).c_str());
            return mode_result::absent;
        }
        std::fprintf(
            stderr,
            "draw2d_parity_test: FAIL gltfx_gl_context::open(srgb=%s): %s (rejected_value=%s)\n",
            mode, code_name(opened.err()).c_str(),
            std::string(opened.err().rejected_value()).c_str());
        return mode_result::fatal;
    }
    glintfx::gltfx_gl_context context = std::move(opened.value());
    if (const glintfx::gltfx_rslt<void> current = context.make_current(); current.has_error()) {
        std::fprintf(stderr, "draw2d_parity_test: FAIL make_current(srgb=%s): %s\n", mode,
                     code_name(current.err()).c_str());
        return mode_result::fatal;
    }
    gl_api gl;
    if (!load_gl(context, gl)) {
        return mode_result::fatal;
    }
    if (!srgb) {
        // MEASURED: what the driver leaves after make_current, the state the layer's contract does
        // NOT assume.
        const struct {
            const char *key;
            gl_enum cap;
        } items[] = {{"blend", k_gl_blend},
                     {"depth_test", k_gl_depth_test},
                     {"stencil_test", k_gl_stencil_test},
                     {"scissor_test", k_gl_scissor_test},
                     {"cull_face", k_gl_cull_face},
                     {"framebuffer_srgb", k_gl_framebuffer_srgb}};
        for (const auto &item : items) {
            std::fprintf(stdout, "MEASURED draw2d_parity_test.state_after_make_current_%s=%d\n",
                         item.key, static_cast<int>(gl.is_enabled(item.cap)));
        }
    }

    // Cell: a context that is not open is refused by name (a moved-from handle is not open).
    if (!srgb) {
        glintfx::gltfx_gl_context spare = std::move(context);
        glintfx::gltfx_rslt<glintfx::gltfx_renderer_2d> refused =
            glintfx::gltfx_renderer_2d::open(context, glintfx::gltfx_renderer_2d_desc{});
        const bool ok = refused.has_error() &&
                        refused.err().code() == glintfx::gltfx_err_code::invalid_argument &&
                        refused.err().rejected_value() == "context";
        ++g_cells;
        if (!ok) {
            ++g_failures;
            std::fprintf(stderr, "draw2d_parity_test: FAIL open() over a closed context must be "
                                 "invalid_argument \"context\"\n");
        }
        context = std::move(spare);
        if (const glintfx::gltfx_rslt<void> current = context.make_current(); current.has_error()) {
            return mode_result::fatal;
        }
    }

    glintfx::gltfx_rslt<glintfx::gltfx_renderer_2d> renderer_opened =
        glintfx::gltfx_renderer_2d::open(context, glintfx::gltfx_renderer_2d_desc{});
    if (renderer_opened.has_error()) {
        std::fprintf(
            stderr,
            "draw2d_parity_test: FAIL gltfx_renderer_2d::open(srgb=%s): %s (rejected_value=%s)\n",
            mode, code_name(renderer_opened.err()).c_str(),
            std::string(renderer_opened.err().rejected_value()).c_str());
        return mode_result::fatal;
    }
    glintfx::gltfx_renderer_2d renderer = std::move(renderer_opened.value());

    // finish_frame() with no frame open.
    {
        glintfx::gltfx_rslt<glintfx::gltfx_frame_2d_report> none = renderer.finish_frame();
        ++g_cells;
        if (none.has_value() || none.err().code() != glintfx::gltfx_err_code::invalid_argument ||
            none.err().rejected_value() != "frame") {
            ++g_failures;
            std::fprintf(stderr, "draw2d_parity_test: FAIL finish_frame() with no frame must be "
                                 "invalid_argument \"frame\"\n");
        }
    }

    // The size of the surface: the viewport the layer leaves covers all of it.
    renderer.begin_frame(glintfx::gltfx_frame_2d_desc{});
    (void)renderer.finish_frame();
    gl_int viewport[4] = {0, 0, 0, 0};
    gl.get_integerv(k_gl_viewport, viewport);
    if (viewport[2] < 320 || viewport[3] < 240) {
        std::fprintf(
            stderr,
            "draw2d_parity_test: FAIL superficie menor que a geometria da prova (%d x %d)\n",
            viewport[2], viewport[3]);
        return mode_result::fatal;
    }
    const cells_context cells{renderer, gl, srgb, viewport[3], mode};
    cell_order(cells);
    cell_premultiplied_edge(cells);
    cell_hostile_state(cells, viewport[2]);
    cell_empty_frame(cells);
    cell_shared_edge(cells);
    cell_draw_calls(cells);
    return mode_result::ran;
}

} // namespace

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    glintfx::gltfx_rslt<glintfx::gltfx_display> display_opened = glintfx::gltfx_display::open();
    if (display_opened.has_error()) {
        std::fprintf(stderr, "draw2d_parity_test: gltfx_display::open() failed: %s\n",
                     code_name(display_opened.err()).c_str());
        return EXIT_FAILURE;
    }
    glintfx::gltfx_display display = std::move(display_opened.value());
    // The surface that does not encode (the option off), then the one that does (on). Off must run:
    // it is the default, and every driver has it.
    const mode_result off = run_mode(display, false);
    if (off != mode_result::ran) {
        return fail("o modo srgb_framebuffer=off (o padrao) tem de rodar em todo driver");
    }
    const mode_result on = run_mode(display, true);
    if (on == mode_result::fatal) {
        return fail("o modo srgb_framebuffer=on falhou ao abrir por outro motivo que nao a falta "
                    "de suporte");
    }
    std::fprintf(stdout, "MEASURED draw2d_parity_test.srgb_on_cells_absent=%d\n",
                 on == mode_result::absent ? 1 : 0);
    // Piso de varredura nao-vazia (GODS_LAWS.md L-40): a run that checked nothing is not a pass.
    std::fprintf(stdout, "draw2d_parity_test: %d celula(s) conferida(s), %d reprovada(s)\n",
                 g_cells, g_failures);
    if (g_cells < 20) {
        return fail("menos celulas do que o piso (varredura quebrada)");
    }
    return g_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
