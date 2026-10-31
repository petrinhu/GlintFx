// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/core/image_view.hpp>
#include <glintfx/draw2d/image_resolver.hpp>
#include <glintfx/draw2d/renderer_2d.hpp>
#include <glintfx/draw2d/texture_2d.hpp>
#include <glintfx/draw2d/texture_2d_desc.hpp>
#include <glintfx/draw2d/texture_region.hpp>
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
//   - a TEXTURE destroyed while the frame is open is not a barrier (T3, D-W8-288): a red piece in
//     layer 1 submitted before the destruction stays over a blue piece in layer 0 submitted after
//     it, at the pixel (210,160); the frame ends with a value, no GL error and no texture bound.
//   - update_texture() (T3) over a hostile unpack state, on an updatable 8 x 8 texture with every
//     reduced level: the call has a value and leaves no unpack buffer, alignment 1, row length and
//     skips 0, no texture bound, unit 0 active and no GL error; an empty update has a value and
//     leaves no GL error; a rectangle past the texture is refused as "rect", a texture made without
//     updatable as "updatable" and a moved-from handle as "texture".
//   - set_image_resolver() (R2D-IMAGE-RESOLVER R2b-fix2) through the PUBLIC handle, on the two
//     renderers of the two-window cell, alive at once: the first install in each hands back none,
//     the second never gets the first one's resolver, emptying each hands back the one installed
//     in it (function AND context), and emptying again hands back none.
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
constexpr gl_enum k_gl_back_left = 0x0402;
constexpr gl_enum k_gl_framebuffer_attachment_color_encoding = 0x8210;
constexpr gl_enum k_gl_texture_2d = 0x0DE1;
constexpr gl_enum k_gl_texture0 = 0x84C0;
constexpr gl_enum k_gl_texture3 = 0x84C3;
constexpr gl_enum k_gl_vertex_shader = 0x8B31;
constexpr gl_enum k_gl_fragment_shader = 0x8B30;
constexpr gl_enum k_gl_rgba = 0x1908;
constexpr gl_enum k_gl_unsigned_byte = 0x1401;
// R2D-TEXTURE T2a-fix1, the unpack state create_texture sets and leaves (gl.xml).
constexpr gl_enum k_gl_unpack_row_length = 0x0CF2;
constexpr gl_enum k_gl_unpack_skip_rows = 0x0CF3;
constexpr gl_enum k_gl_unpack_skip_pixels = 0x0CF4;
constexpr gl_enum k_gl_unpack_alignment = 0x0CF5;
constexpr gl_enum k_gl_pixel_unpack_buffer = 0x88EC;
constexpr gl_enum k_gl_pixel_unpack_buffer_binding = 0x88EF;

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
    void (*pixel_storei)(gl_enum, gl_int) = nullptr;
    void (*gen_buffers)(gl_sizei, gl_uint *) = nullptr;
    void (*bind_buffer)(gl_enum, gl_uint) = nullptr;
    void (*delete_buffers)(gl_sizei, const gl_uint *) = nullptr;
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
    gl_enum (*get_error)() = nullptr;
    void (*get_framebuffer_attachment_parameteriv)(gl_enum, gl_enum, gl_enum, gl_int *) = nullptr;
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

// Bounded: a lost context may answer an error forever.
void drain_gl_errors(const gl_api &gl) {
    for (int i = 0; i < 16 && gl.get_error() != 0; ++i) {
    }
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
           resolve(context, "glPixelStorei", gl.pixel_storei) &&
           resolve(context, "glGenBuffers", gl.gen_buffers) &&
           resolve(context, "glBindBuffer", gl.bind_buffer) &&
           resolve(context, "glDeleteBuffers", gl.delete_buffers) &&
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
           resolve(context, "glGetError", gl.get_error) &&
           resolve(context, "glGetFramebufferAttachmentParameteriv",
                   gl.get_framebuffer_attachment_parameteriv) &&
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
    bool srgb = false;
    int height = 0;
    const char *mode = ""; // "off" or "on", for the messages
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

gl_int integer_of(const gl_api &gl, gl_enum name) {
    gl_int value = -1;
    gl.get_integerv(name, &value);
    return value;
}

// Leaves what a careless consumer could have left before create_texture(): a pixel-unpack buffer
// bound (with no data store: an upload from it is a GL error), odd unpack state, unit 3 active.
gl_uint plant_hostile_unpack_state(const gl_api &gl) {
    gl_uint buffer = 0;
    gl.gen_buffers(1, &buffer);
    gl.bind_buffer(k_gl_pixel_unpack_buffer, buffer);
    gl.pixel_storei(k_gl_unpack_alignment, 8);
    gl.pixel_storei(k_gl_unpack_row_length, 3);
    gl.pixel_storei(k_gl_unpack_skip_rows, 2);
    gl.pixel_storei(k_gl_unpack_skip_pixels, 1);
    gl.active_texture(k_gl_texture3);
    return buffer;
}

// The state renderer_2d.hpp says create_texture() leaves, read back from the driver: no unpack
// buffer, alignment 1, row length and skips 0, no texture bound, unit 0 active, no GL error.
void check_state_left_by_create_texture(const cells_context &c) {
    const gl_api &gl = c.gl;
    report(c, "T2 nenhum buffer de desempacotamento ligado",
           integer_of(gl, k_gl_pixel_unpack_buffer_binding) == 0);
    report(c, "T3 alinhamento de desempacotamento 1", integer_of(gl, k_gl_unpack_alignment) == 1);
    report(c, "T4 comprimento de linha e saltos 0",
           integer_of(gl, k_gl_unpack_row_length) == 0 &&
               integer_of(gl, k_gl_unpack_skip_rows) == 0 &&
               integer_of(gl, k_gl_unpack_skip_pixels) == 0);
    report(c, "T5 nenhuma textura ligada, unidade 0 ativa",
           integer_of(gl, k_gl_texture_binding_2d) == 0 &&
               integer_of(gl, k_gl_active_texture) == static_cast<gl_int>(k_gl_texture0));
    report(c, "T6 nenhum erro GL depois de create_texture", gl.get_error() == 0);
}

// The handle: its size, a move that carries the texture, and a destruction that leaves no GL error.
void check_texture_handle(const cells_context &c, glintfx::gltfx_texture_2d texture) {
    report(c, "T7 a alca aberta diz o tamanho",
           texture.is_open() && texture.width() == 3 && texture.height() == 2);
    glintfx::gltfx_texture_2d moved = std::move(texture);
    // renderer_2d.hpp, texture_2d.hpp: is_open() is the one call allowed on a moved-from handle.
    report(c, "T8 mover leva a textura",
           !texture.is_open() && moved.is_open() && moved.width() == 3);
    {
        const glintfx::gltfx_texture_2d gone = std::move(moved);
    }
    report(c, "T9 destruir a alca nao deixa erro GL", c.gl.get_error() == 0);
}

// Cell T (R2D-TEXTURE T2a-fix1): create_texture() through the PUBLIC handle on a real context, on
// both systems, over hostile unpack state; then a zero width refused by name through the same call.
void cell_texture_creation(const cells_context &c) {
    drain_gl_errors(c.gl);
    const gl_uint buffer = plant_hostile_unpack_state(c.gl);
    const std::vector<std::byte> pixels(static_cast<std::size_t>(3) * 2 * 4, std::byte{0x80});
    const glintfx::gltfx_image_view image{.pixels = pixels,
                                          .width = 3,
                                          .height = 2,
                                          .bytes_per_row = 12,
                                          .pixel_format =
                                              glintfx::gltfx_pixel_format::rgba8_srgb_straight};
    glintfx::gltfx_texture_2d_desc desc;
    desc.mipmap_levels = glintfx::gltfx_all_mipmap_levels;
    glintfx::gltfx_rslt<glintfx::gltfx_texture_2d> made = c.renderer.create_texture(image, desc);
    report(c, "T1 create_texture abre a textura", made.has_value(),
           made.has_error() ? code_name(made.err()).c_str() : "");
    check_state_left_by_create_texture(c);
    c.gl.delete_buffers(1, &buffer);
    if (made.has_value()) {
        check_texture_handle(c, std::move(made.value()));
    }
    glintfx::gltfx_image_view no_width = image;
    no_width.width = 0;
    const glintfx::gltfx_rslt<glintfx::gltfx_texture_2d> refused =
        c.renderer.create_texture(no_width, {});
    report(c, "T10 largura zero recusada pelo nome",
           refused.has_error() &&
               refused.err().code() == glintfx::gltfx_err_code::invalid_argument &&
               refused.err().rejected_value() == "width");
}

// Cell TD (R2D-TEXTURE T3, D-W8-288): a texture destroyed while the frame is open, between a red
// piece of layer 1 and a blue piece of layer 0 over the same pixel. Destruction is not a barrier:
// one batch, painted in layer order, so the pixel stays red; the deferred release leaves no GL
// error and no texture bound.
void cell_texture_destruction_in_frame(const cells_context &c) {
    drain_gl_errors(c.gl);
    const std::vector<std::byte> pixels(4, std::byte{0xFF});
    const glintfx::gltfx_image_view image{.pixels = pixels,
                                          .width = 1,
                                          .height = 1,
                                          .bytes_per_row = 4,
                                          .pixel_format =
                                              glintfx::gltfx_pixel_format::rgba8_srgb_straight};
    c.renderer.begin_frame(glintfx::gltfx_frame_2d_desc{});
    c.renderer.fill_rect(rect(200.0, 150.0, 20.0, 20.0), k_red, glintfx::gltfx_draw_layer{1});
    {
        const glintfx::gltfx_rslt<glintfx::gltfx_texture_2d> made =
            c.renderer.create_texture(image, {});
        report(c, "TD1 create_texture com o quadro aberto abre a textura", made.has_value());
    } // the texture is destroyed here, with the frame open
    c.renderer.fill_rect(rect(205.0, 155.0, 20.0, 20.0), k_blue, glintfx::gltfx_draw_layer{0});
    const auto finished = c.renderer.finish_frame();
    report(c, "TD2 o quadro termina com valor e sem erro GL depois da liberacao adiada",
           finished.has_value() && c.gl.get_error() == 0);
    report(c, "TD3 destruir nao enviou o lote: o vermelho da camada 1 fica sobre o azul",
           is_exact(read_at(c, 210, 160), 255, 0, 0));
    report(c, "TD4 nenhuma textura ligada depois do quadro",
           integer_of(c.gl, k_gl_texture_binding_2d) == 0);
}

// The state update_texture() leaves, read back from the driver: the same as create_texture()'s (no
// unpack buffer, alignment 1, row length and skips 0, no texture bound, unit 0 active, no GL
// error).
void check_state_left_by_update_texture(const cells_context &c) {
    const gl_api &gl = c.gl;
    report(c, "TU3 sem buffer de desempacotamento, alinhamento 1, linha e saltos 0",
           integer_of(gl, k_gl_pixel_unpack_buffer_binding) == 0 &&
               integer_of(gl, k_gl_unpack_alignment) == 1 &&
               integer_of(gl, k_gl_unpack_row_length) == 0 &&
               integer_of(gl, k_gl_unpack_skip_rows) == 0 &&
               integer_of(gl, k_gl_unpack_skip_pixels) == 0);
    report(c, "TU4 nenhuma textura ligada, unidade 0 ativa, nenhum erro GL",
           integer_of(gl, k_gl_texture_binding_2d) == 0 &&
               integer_of(gl, k_gl_active_texture) == static_cast<gl_int>(k_gl_texture0) &&
               gl.get_error() == 0);
}

// Cell TU (R2D-TEXTURE T3): update_texture() through the PUBLIC handle on a real context, over a
// hostile unpack state, on an updatable texture with every reduced level: the call has a value and
// leaves the state create_texture() leaves; an empty update has a value and leaves no GL error; a
// rectangle past the texture, a texture made without updatable and a moved-from handle are refused
// by name.
void cell_texture_update(const cells_context &c) {
    drain_gl_errors(c.gl);
    const std::vector<std::byte> pixels(static_cast<std::size_t>(8) * 8 * 4, std::byte{0x80});
    const std::vector<std::byte> patch(static_cast<std::size_t>(3) * 2 * 4, std::byte{0xFF});
    const auto view = [](const std::vector<std::byte> &bytes, std::uint32_t width,
                         std::uint32_t height) {
        return glintfx::gltfx_image_view{.pixels = bytes,
                                         .width = width,
                                         .height = height,
                                         .bytes_per_row = static_cast<std::size_t>(width) * 4,
                                         .pixel_format =
                                             glintfx::gltfx_pixel_format::rgba8_srgb_straight};
    };
    glintfx::gltfx_texture_2d_desc desc;
    desc.mipmap_levels = glintfx::gltfx_all_mipmap_levels;
    desc.updatable = true;
    glintfx::gltfx_rslt<glintfx::gltfx_texture_2d> made =
        c.renderer.create_texture(view(pixels, 8, 8), desc);
    report(c, "TU1 a textura 8 x 8 com todos os niveis reduzidos abre", made.has_value());
    if (!made.has_value()) {
        return;
    }
    const gl_uint buffer = plant_hostile_unpack_state(c.gl);
    const glintfx::gltfx_rslt<void> updated =
        c.renderer.update_texture(made.value(), 2, 3, view(patch, 3, 2));
    report(c, "TU2 update_texture escreve o retangulo 3 x 2 em (2, 3)", updated.has_value(),
           updated.has_error() ? code_name(updated.err()).c_str() : "");
    check_state_left_by_update_texture(c);
    c.gl.delete_buffers(1, &buffer);
    const glintfx::gltfx_rslt<void> past =
        c.renderer.update_texture(made.value(), 6, 7, view(patch, 3, 2));
    report(c, "TU5 retangulo alem da textura recusado pelo nome",
           past.has_error() && past.err().code() == glintfx::gltfx_err_code::invalid_argument &&
               past.err().rejected_value() == "rect");
    const glintfx::gltfx_rslt<void> empty =
        c.renderer.update_texture(made.value(), 8, 0, view(patch, 0, 2));
    report(c, "TU7 atualizacao vazia na borda tem valor e nenhum erro GL",
           empty.has_value() && c.gl.get_error() == 0);
    desc.updatable = false;
    glintfx::gltfx_rslt<glintfx::gltfx_texture_2d> fixed =
        c.renderer.create_texture(view(pixels, 8, 8), desc);
    const glintfx::gltfx_rslt<void> refused =
        fixed.has_value() ? c.renderer.update_texture(fixed.value(), 0, 0, view(patch, 3, 2))
                          : glintfx::gltfx_rslt<void>::ok();
    report(c, "TU8 textura feita sem updatable recusada pelo nome",
           fixed.has_value() && refused.has_error() &&
               refused.err().code() == glintfx::gltfx_err_code::invalid_argument &&
               refused.err().rejected_value() == "updatable");
    const glintfx::gltfx_texture_2d kept = std::move(made.value());
    const glintfx::gltfx_rslt<void> gone =
        c.renderer.update_texture(made.value(), 0, 0, view(patch, 3, 2));
    report(c, "TU6 alca movida recusada pelo nome",
           kept.is_open() && gone.has_error() &&
               gone.err().code() == glintfx::gltfx_err_code::invalid_argument &&
               gone.err().rejected_value() == "texture");
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
            // D-SRGB2-13: the declared absence names THE OPTION that was refused; a refusal that
            // names another option (msaa_samples, say) is a defect of the adapter, not an absence.
            if (opened.err().rejected_value() != "srgb_framebuffer") {
                std::fprintf(stderr,
                             "draw2d_parity_test: FAIL a recusa de srgb_framebuffer=on nomeou "
                             "\"%s\", esperado \"srgb_framebuffer\"\n",
                             std::string(opened.err().rejected_value()).c_str());
                return mode_result::fatal;
            }
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

    if (srgb) {
        // D-SRGB2-13: a context that opened with the option ON must ANSWER supported for it.
        const glintfx::gltfx_gfx_option_support support =
            context.option_support(glintfx::gltfx_gfx_option::srgb_framebuffer);
        ++g_cells;
        if (support != glintfx::gltfx_gfx_option_support::supported) {
            ++g_failures;
            std::fprintf(stderr, "draw2d_parity_test: FAIL srgb_framebuffer=on abriu mas "
                                 "option_support != supported\n");
        }
    }
    {
        // MEASURED, NEVER ASSERTED (D-SRGB2-11, D-A62): the color encoding of the default
        // framebuffer's back buffer, 35904 (GL_SRGB) or 9729 (GL_LINEAR); -1 when the GL errors.
        // glGetError is drained BEFORE and AFTER, so the read neither inherits an error nor leaves
        // one for the renderer of this mode (the GL state contract, errata sec. 12).
        drain_gl_errors(gl);
        gl_int encoding = -1;
        gl.get_framebuffer_attachment_parameteriv(k_gl_draw_framebuffer, k_gl_back_left,
                                                  k_gl_framebuffer_attachment_color_encoding,
                                                  &encoding);
        if (gl.get_error() != 0) {
            encoding = -1;
        }
        drain_gl_errors(gl);
        std::fprintf(stdout, "MEASURED draw2d_parity_test.back_buffer_color_encoding_srgb_%s=%d\n",
                     mode, static_cast<int>(encoding));
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
    cell_texture_creation(cells);
    cell_texture_destruction_in_frame(cells);
    cell_texture_update(cells);
    return mode_result::ran;
}

// ---------------------------------------------------------------------------------------------------------
// DW-a: two windows, each with its own context and renderer, in ONE process (D-API-07). The test
// itself makes the OTHER window's context current right before every entry point of the window it
// draws on; without that, the cell passes even when the product never makes anything current.
// Colors are opaque primaries (channels 0 or 255, no encoding): the prediction, fixed before the
// data exists (GODS_LAWS.md L-43), is  A: FD red, P1 green, P2 green, P3 red  and  B: FD blue, P1
// blue, P2 blue, P3 yellow, tolerance +-2.
// ---------------------------------------------------------------------------------------------------------
const glintfx::gltfx_rgba k_green{0.0F, 1.0F, 0.0F, 1.0F};
const glintfx::gltfx_rgba k_yellow{1.0F, 1.0F, 0.0F, 1.0F};

// Built IN PLACE and never moved once the renderer is open (the renderer points at its context).
struct drawing_window {
    std::optional<glintfx::gltfx_window> window;
    std::optional<glintfx::gltfx_gl_context> context;
    std::optional<glintfx::gltfx_renderer_2d> renderer;
    gl_api gl;
    int height = 0;
};

bool open_window_and_context(glintfx::gltfx_display &display, drawing_window &w,
                             const char *title) {
    const glintfx::gltfx_window_desc window_desc{
        .title = title,
        .application_id = "org.glintfx.draw2d_parity_test",
        .logical_size = {.width = 320, .height = 240},
    };
    auto window_opened = glintfx::gltfx_window::open(display, window_desc);
    if (window_opened.has_error()) {
        std::fprintf(stderr, "draw2d_parity_test: FAIL [duas_janelas] window::open(%s): %s\n",
                     title, code_name(window_opened.err()).c_str());
        return false;
    }
    w.window.emplace(std::move(window_opened.value()));
    auto context_opened =
        glintfx::gltfx_gl_context::open(*w.window, glintfx::gltfx_gl_context_desc{});
    if (context_opened.has_error()) {
        std::fprintf(stderr, "draw2d_parity_test: FAIL [duas_janelas] gl_context::open(%s): %s\n",
                     title, code_name(context_opened.err()).c_str());
        return false;
    }
    w.context.emplace(std::move(context_opened.value()));
    if (const auto current = w.context->make_current(); current.has_error()) {
        std::fprintf(stderr, "draw2d_parity_test: FAIL [duas_janelas] make_current(%s): %s\n",
                     title, code_name(current.err()).c_str());
        return false;
    }
    // proc_address is per context on WGL: each window loads ITS table, right after ITS
    // make_current.
    return load_gl(*w.context, w.gl);
}

bool open_renderer_and_measure(drawing_window &w, const char *title) {
    auto renderer_opened =
        glintfx::gltfx_renderer_2d::open(*w.context, glintfx::gltfx_renderer_2d_desc{});
    if (renderer_opened.has_error()) {
        std::fprintf(stderr, "draw2d_parity_test: FAIL [duas_janelas] renderer_2d::open(%s): %s\n",
                     title, code_name(renderer_opened.err()).c_str());
        return false;
    }
    w.renderer.emplace(std::move(renderer_opened.value()));
    w.renderer->begin_frame(glintfx::gltfx_frame_2d_desc{});
    (void)w.renderer->finish_frame();
    gl_int viewport[4] = {0, 0, 0, 0};
    w.gl.get_integerv(k_gl_viewport, viewport);
    w.height = viewport[3];
    if (viewport[2] < 320 || viewport[3] < 240) {
        std::fprintf(stderr, "draw2d_parity_test: FAIL [duas_janelas] superficie menor (%d x %d)\n",
                     viewport[2], viewport[3]);
        return false;
    }
    return true;
}

cells_context cells_of(drawing_window &w) {
    return cells_context{*w.renderer, w.gl, false, w.height, "duas_janelas"};
}

// Makes `w`'s context current through the PUBLIC API; a failure is a failed cell with its own
// message.
bool switch_to(drawing_window &w, const char *name) {
    const auto current = w.context->make_current();
    const std::string what = std::string("make_current do teste na janela ") + name;
    if (current.has_error()) {
        report(cells_of(w), what.c_str(), false); // only a FAILURE is a cell, never a success
    }
    return current.has_value();
}

struct two_window_outcome {
    bool a_finished = false;
    bool b_finished = false;
};

glintfx::gltfx_rect_world square(double x, double y) { return rect(x, y, 10, 10); }

// The sequence of the parecer sec. 3.2, steps 1 to 5. Every entry point of A runs with B current.
bool paint_with_the_other_current(drawing_window &a, drawing_window &b, two_window_outcome &out) {
    if (!switch_to(b, "B")) {
        return false;
    }
    b.renderer->begin_frame(glintfx::gltfx_frame_2d_desc{.clear_color = k_blue});
    a.renderer->begin_frame(glintfx::gltfx_frame_2d_desc{.clear_color = k_red});
    a.renderer->fill_rect(square(20, 20), k_green);
    if (!switch_to(b, "B")) {
        return false;
    }
    a.renderer->flush();
    a.renderer->fill_rect(square(60, 20), k_green);
    if (!switch_to(b, "B")) {
        return false;
    }
    out.a_finished = a.renderer->finish_frame().has_value();
    b.renderer->fill_rect(square(100, 20), k_yellow);
    out.b_finished = b.renderer->finish_frame().has_value();
    return true;
}

// FD, P1, P2, P3 of one window, each a cell named by window and point.
void check_window_pixels(drawing_window &w, const char *name, const pixel (&want)[4]) {
    const cells_context c = cells_of(w);
    const char *points[4] = {"FD", "P1", "P2", "P3"};
    const int xs[4] = {200, 25, 65, 105};
    const int ys[4] = {150, 25, 25, 25};
    for (int i = 0; i < 4; ++i) {
        const pixel got = read_at(c, xs[i], ys[i]);
        const bool ok = within(got.red, want[i].red, 2) && within(got.green, want[i].green, 2) &&
                        within(got.blue, want[i].blue, 2);
        const std::string what = std::string(name) + " em " + points[i];
        const std::string detail = "lido (" + std::to_string(got.red) + "," +
                                   std::to_string(got.green) + "," + std::to_string(got.blue) + ")";
        report(c, what.c_str(), ok, detail.c_str());
    }
}

// The two resolvers of cell RS (R2D-IMAGE-RESOLVER R2b-fix2). The library never calls them in this
// version (renderer_2d.hpp). Their bodies differ on purpose: "/OPT:ICF can cause the same address
// to be assigned to different functions" (MSVC, /OPT documentation), and two empty bodies could
// share one. The cell compares the context too, which no linker folds.
glintfx::gltfx_image_resolution
resolver_of_window_a(void *, const glintfx::gltfx_image_reference *) noexcept {
    return {};
}
glintfx::gltfx_image_resolution
resolver_of_window_b(void *, const glintfx::gltfx_image_reference *) noexcept {
    return {nullptr, {{}, glintfx::gltfx_texture_mirror::flip_left_right}};
}

bool resolver_is(glintfx::gltfx_image_resolver got, glintfx::gltfx_image_resolver_fn want_function,
                 void *want_context) {
    return got.function == want_function && got.context == want_context;
}

// Cell RS: set_image_resolver() through the PUBLIC handle, on the two renderers of this cell, alive
// at once. renderer_2d_impl_test proves the interior; this cell proves the handle forwards to it
// and hands back what the interior hands back, renderer by renderer.
void cell_image_resolver_per_renderer(drawing_window &a, drawing_window &b) {
    int context_of_a = 0;
    int context_of_b = 0;
    const cells_context ca = cells_of(a);
    const cells_context cb = cells_of(b);
    report(ca, "RS1 A instala e recebe nenhum",
           resolver_is(a.renderer->set_image_resolver({resolver_of_window_a, &context_of_a}),
                       nullptr, nullptr));
    report(cb, "RS2 B instala e recebe nenhum, nunca o de A",
           resolver_is(b.renderer->set_image_resolver({resolver_of_window_b, &context_of_b}),
                       nullptr, nullptr));
    report(ca, "RS3 A esvaziado devolve o de A",
           resolver_is(a.renderer->set_image_resolver({}), resolver_of_window_a, &context_of_a));
    report(cb, "RS4 B esvaziado devolve o de B",
           resolver_is(b.renderer->set_image_resolver({}), resolver_of_window_b, &context_of_b));
    report(ca, "RS5 A esvaziado de novo devolve nenhum",
           resolver_is(a.renderer->set_image_resolver({}), nullptr, nullptr));
}

// Both renderers die before either context, and both contexts before either window.
void close_in_order(drawing_window &a, drawing_window &b) {
    a.renderer.reset();
    b.renderer.reset();
    a.context.reset();
    b.context.reset();
    a.window.reset();
    b.window.reset();
}

// Returns false when a window could not be opened: duas janelas have to work on both systems, so
// there is no declared absence here.
bool cell_two_windows(glintfx::gltfx_display &display) {
    drawing_window a;
    drawing_window b;
    if (!open_window_and_context(display, a, "janela A") ||
        !open_renderer_and_measure(a, "janela A") ||
        !open_window_and_context(display, b, "janela B") ||
        !open_renderer_and_measure(b, "janela B")) {
        return false;
    }
    two_window_outcome out;
    if (paint_with_the_other_current(a, b, out)) {
        report(cells_of(a), "A finish_frame() tem valor", out.a_finished);
        report(cells_of(b), "B finish_frame() tem valor", out.b_finished);
        const pixel red{255, 0, 0};
        const pixel green{0, 255, 0};
        const pixel blue{0, 0, 255};
        const pixel yellow{255, 255, 0};
        const pixel want_a[4] = {red, green, green, red};
        const pixel want_b[4] = {blue, blue, blue, yellow};
        if (switch_to(a, "A")) {
            check_window_pixels(a, "A", want_a);
        }
        if (switch_to(b, "B")) {
            check_window_pixels(b, "B", want_b);
        }
    }
    cell_image_resolver_per_renderer(a, b);
    close_in_order(a, b);
    return true;
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
    if (!cell_two_windows(display)) {
        return fail("a celula de duas janelas nao conseguiu abrir as duas janelas");
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
