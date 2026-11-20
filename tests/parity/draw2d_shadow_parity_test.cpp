// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <glintfx/core/angle.hpp>
#include <glintfx/core/color.hpp>
#include <glintfx/core/err.hpp>
#include <glintfx/core/image_view.hpp>
#include <glintfx/core/light_2d.hpp>
#include <glintfx/core/log/event.hpp>
#include <glintfx/core/log/sink.hpp>
#include <glintfx/core/projected_shadow.hpp>
#include <glintfx/core/quad.hpp>
#include <glintfx/core/rect.hpp>
#include <glintfx/draw2d/draw_layer.hpp>
#include <glintfx/draw2d/frame_2d_report.hpp>
#include <glintfx/draw2d/renderer_2d.hpp>
#include <glintfx/draw2d/texture_2d.hpp>
#include <glintfx/draw2d/texture_2d_desc.hpp>
#include <glintfx/draw2d/texture_region.hpp>
#include <glintfx/platform/gl/context.hpp>
#include <glintfx/platform/window/display.hpp>
#include <glintfx/platform/window/window.hpp>

// draw2d_shadow_parity_test.cpp - R2D-SPRITE-SHADE H3b-2 (docs/auditoria-api-sprite-shade.md, sec.
// 2 and 4.2; the audit's CRI-H01, IMP-H09 and IMP-H10; the v1 of the plan): fill_shadow() through
// the PUBLIC API only, READ BACK AS PIXELS from a real surface. ONE file, no #if, no harness: the
// Windows side links it into an add_executable() target (tests/CMakeLists.txt, if(WIN32)); the
// Linux side runs it as a container fixture of this exact name, inside a nested compositor
// (GODS_LAWS.md L-09), never on the live session. Every cell runs in both values of the context
// option srgb_framebuffer (the absence of "on" is declared and counted, as in the sister fixtures).
//
// THE CRITERIA ARE FIXED HERE, BEFORE THE DATA EXISTS (GODS_LAWS.md L-43). The surface starts white
// and the shadow is black with alpha 0.5, so a pixel darkened once and one darkened twice are far
// apart in either mode (off: 127.5 and 63.75; on: 188 and 137, the linear 0.5 and 0.25 encoded):
//   - the cells of the v1 and of the audit, each {without, with} a layer key. WITHOUT: the steps
//   are
//     submitted in paint order, in one layer. WITH: the same steps submitted in REVERSE, each in
//     the layer of its place, so only the layer key can give the same paint order:
//       H1 two overlapping shadows: the overlap is darkened once;
//       H2 a shadow over an opaque piece drawn after another shadow (CRI-H01): the piece is
//       darkened
//          by the second shadow, and the first shadow's ground stays darkened once;
//       H3 a transparent pixel of a piece does not release: a texture half transparent, half opaque
//          between two shadows; and the same with a rectangle of alpha 0;
//       H4 a half-transparent piece does not release (IMP-H09);
//       H5 a shadow painted before its own opaque piece does not darken it;
//   - H6 the barrier (IMP-H10): a shadow after flush() darkens again, in a lower layer too; after
//     begin_batch() it does not;
//   - H7 the tint: red with alpha 0.5 mixes towards red; black with alpha 1 leaves black; a shadow
//     whose alpha rounds to nothing darkens nothing and marks nothing, one of alpha 0.01 marks;
//   - H8 the count: three rectangles are 1 draw call and no shadow segment; a rectangle, a shadow
//     and a rectangle are 4 and 1; four runs with two shadows are 5 and 1; two segments count 2;
//   - H9 the refusals: a NaN corner, an infinite corner and a NaN tint, one piece each with its
//   token;
//     a corner beyond the pixel range; a shadow of zero area is drawn and covers no pixel; none of
//     them by stencil_unavailable on this surface; with no frame open, counted in the next report;
//   - H10 the stencil a barrier leaves, read back with glGetIntegerv (the closed list of flush()):
//     after a flush() with a shadow, after one with none and after finish_frame(), from hostile
//     values the consumer left;
//   - H11 the stencil buffer of the consumer: a frame with no shadow and with the clear colour
//   keeps
//     what the consumer wrote; a frame with a shadow clears it, and what was written does not
//     change what the shadow darkens;
//   - H12 the reading of the bits: the surface has at least 8 bits, the renderer did not refuse, a
//     renderer opened with a framebuffer of the consumer bound puts the binding back and leaves no
//     GL error, and neither open reads with an error: the event draw2d_stencil_bits_unread does
//     not come, on a driver that follows the specification;
//   - the silhouette (H3b-3), draw_shadow_texture(), on a texture of two texels, the left one
//     transparent and the right one opaque green:
//       H13 the transparent texel does not darken, the opaque one does by the alpha of the tint,
//       the
//           colour of the texel does not enter (a red tint stays red), a mirror swaps the two;
//       H14 {without, with} a layer key: what a silhouette does not darken it does not mark, so the
//           next shadow darkens there; what it darkens it marks, so the next one does not;
//       H15 {without, with} a layer key: two overlapping silhouettes darken once; an opaque piece
//           between them releases, and the second darkens the piece;
//       H16 a texel of alpha 0.5 under a black tint of alpha 1 darkens by 0.5, once, and marks;
//       H17 a checkerboard in a trapezoid: the silhouette covers exactly the pixels
//           draw_texture_quad() paints with the same corners, a grid of probes;
//       H18 the shadow of a piece under a point light (the trapezoid core/projected_shadow.hpp
//       gives):
//           the texel at base_start lands on the base_start side, with no swap of corners; under a
//           sun overhead the four corners lie on one line: drawn, not refused, no pixel;
//       H19 one call is one piece, and the reasons of draw_texture_quad() with their tokens: a
//       corner
//           that is not a number, a texture that is not open, a texture of another renderer, a
//           region outside it, three corners on a line; a rectangle, a silhouette and a rectangle
//           are 4 draw calls, 2 texture switches and 1 segment.
// Geometry assumes a scale factor of 1 (as the sister fixtures). The surface is 320 by 240.

namespace {

using gl_enum = unsigned int;
using gl_uint = unsigned int;
using gl_int = int;
using gl_sizei = int;

constexpr gl_enum k_gl_viewport = 0x0BA2;
constexpr gl_enum k_gl_rgba = 0x1908;
constexpr gl_enum k_gl_unsigned_byte = 0x1401;
constexpr gl_enum k_gl_stencil_index = 0x1901;
constexpr gl_enum k_gl_stencil_test = 0x0B90;
constexpr gl_enum k_gl_stencil_buffer_bit = 0x00000400;
constexpr gl_enum k_gl_never = 0x0200;
constexpr gl_enum k_gl_invert = 0x150A;
constexpr gl_enum k_gl_draw_framebuffer = 0x8CA9;
constexpr gl_enum k_gl_draw_framebuffer_binding = 0x8CA6;
constexpr gl_enum k_gl_stencil = 0x1802;
constexpr gl_enum k_gl_attachment_stencil_size = 0x8217;

// The GL entry points the cells need, resolved through the PUBLIC gltfx_gl_context::proc_address().
struct gl_api {
    void (*get_integerv)(gl_enum, gl_int *) = nullptr;
    void (*read_pixels)(gl_int, gl_int, gl_sizei, gl_sizei, gl_enum, gl_enum, void *) = nullptr;
    void (*enable)(gl_enum) = nullptr;
    void (*disable)(gl_enum) = nullptr;
    void (*stencil_func)(gl_enum, gl_int, gl_uint) = nullptr;
    void (*stencil_op)(gl_enum, gl_enum, gl_enum) = nullptr;
    void (*stencil_mask)(gl_uint) = nullptr;
    void (*clear_stencil)(gl_int) = nullptr;
    void (*clear)(gl_enum) = nullptr;
    void (*gen_framebuffers)(gl_sizei, gl_uint *) = nullptr;
    void (*delete_framebuffers)(gl_sizei, const gl_uint *) = nullptr;
    void (*bind_framebuffer)(gl_enum, gl_uint) = nullptr;
    void (*get_attachment)(gl_enum, gl_enum, gl_enum, gl_int *) = nullptr;
    gl_enum (*get_error)() = nullptr;
};

template <typename F> bool resolve(glintfx::gltfx_gl_context &context, const char *name, F &out) {
    void *address = context.proc_address(name);
    if (address == nullptr) {
        std::fprintf(stderr, "draw2d_shadow_parity_test: FAIL %s nao resolvido pelo contexto\n",
                     name);
        return false;
    }
    out = reinterpret_cast<F>(address);
    return true;
}

bool load_stencil_gl(glintfx::gltfx_gl_context &context, gl_api &gl) {
    return resolve(context, "glStencilFunc", gl.stencil_func) &&
           resolve(context, "glStencilOp", gl.stencil_op) &&
           resolve(context, "glStencilMask", gl.stencil_mask) &&
           resolve(context, "glClearStencil", gl.clear_stencil) &&
           resolve(context, "glClear", gl.clear) && resolve(context, "glEnable", gl.enable) &&
           resolve(context, "glDisable", gl.disable);
}

bool load_gl(glintfx::gltfx_gl_context &context, gl_api &gl) {
    return load_stencil_gl(context, gl) && resolve(context, "glGetIntegerv", gl.get_integerv) &&
           resolve(context, "glReadPixels", gl.read_pixels) &&
           resolve(context, "glGenFramebuffers", gl.gen_framebuffers) &&
           resolve(context, "glDeleteFramebuffers", gl.delete_framebuffers) &&
           resolve(context, "glBindFramebuffer", gl.bind_framebuffer) &&
           resolve(context, "glGetFramebufferAttachmentParameteriv", gl.get_attachment) &&
           resolve(context, "glGetError", gl.get_error);
}

struct pixel {
    int red = 0;
    int green = 0;
    int blue = 0;
};

// Everything one cell needs: the renderer under test, the context, the GL entry points, the mode
// of the surface and its height (glReadPixels counts rows from the BOTTOM, the drawing from the
// TOP).
struct cells_context {
    glintfx::gltfx_renderer_2d &renderer;
    glintfx::gltfx_gl_context &context;
    const gl_api &gl;
    bool srgb = false;
    int height = 0;
    const char *mode = ""; // "off" or "on", for the messages
};

int g_cells = 0;
int g_failures = 0;

void report(const cells_context &c, const std::string &cell, bool ok) {
    ++g_cells;
    if (!ok) {
        ++g_failures;
        std::fprintf(stderr, "draw2d_shadow_parity_test: FAIL [srgb=%s] %s\n", c.mode,
                     cell.c_str());
    }
}

pixel read_at(const cells_context &c, int x, int y) {
    unsigned char rgba[4] = {0, 0, 0, 0};
    c.gl.read_pixels(x, c.height - 1 - y, 1, 1, k_gl_rgba, k_gl_unsigned_byte, rgba);
    return pixel{rgba[0], rgba[1], rgba[2]};
}

int stencil_at(const cells_context &c, int x, int y) {
    unsigned char value = 0;
    c.gl.read_pixels(x, c.height - 1 - y, 1, 1, k_gl_stencil_index, k_gl_unsigned_byte, &value);
    return value;
}

// One channel, at linear `light` over a surface that encodes it (on) or was given encoded values to
// blend (off), as the COLOR section of renderer_2d.hpp says. The two values of `light` a cell asks
// for come from blending, so `on` encodes the result and `off` is the result, in levels.
double level_of(const cells_context &c, double light) {
    if (!c.srgb) {
        return 255.0 * light;
    }
    const double encoded =
        light <= 0.0031308 ? 12.92 * light : 1.055 * std::pow(light, 1.0 / 2.4) - 0.055;
    return 255.0 * encoded;
}

// Within two levels per channel: one of rounding, one of the driver's.
bool near_pixel(const pixel &got, double red, double green, double blue) {
    return std::abs(got.red - red) <= 2.0 && std::abs(got.green - green) <= 2.0 &&
           std::abs(got.blue - blue) <= 2.0;
}

bool gray_at(const cells_context &c, int x, int y, double light) {
    const double level = level_of(c, light);
    return near_pixel(read_at(c, x, y), level, level, level);
}

const glintfx::gltfx_rgba k_shade{0.0F, 0.0F, 0.0F, 0.5F};
const glintfx::gltfx_rgba k_opaque_red{1.0F, 0.0F, 0.0F, 1.0F};
const glintfx::gltfx_rgba k_white{1.0F, 1.0F, 1.0F, 1.0F};

glintfx::gltfx_quad_world box(double x0, double y0, double x1, double y1) {
    return glintfx::gltfx_quad_world{{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
}

void begin_white(const cells_context &c) {
    glintfx::gltfx_frame_2d_desc desc;
    desc.clear_color = k_white;
    c.renderer.begin_frame(desc);
}

// One step of a paint order. `shadow` picks fill_shadow(); `texture`, when not null, picks
// draw_texture() with the whole of it, and with `shadow` too, draw_shadow_texture().
struct step {
    bool shadow = false;
    glintfx::gltfx_quad_world corners{};
    glintfx::gltfx_rgba color{};
    const glintfx::gltfx_texture_2d *texture = nullptr;
};

step shadow_step(double x0, double x1, glintfx::gltfx_rgba tint = k_shade) {
    return step{true, box(x0, 20, x1, 60), tint, nullptr};
}

step piece_step(double x0, double x1, glintfx::gltfx_rgba color) {
    return step{false, box(x0, 20, x1, 60), color, nullptr};
}

glintfx::gltfx_texture_paint whole_paint(glintfx::gltfx_rgba tint) {
    return glintfx::gltfx_texture_paint{
        glintfx::gltfx_texture_region{glintfx::gltfx_rect_texel{{0.0, 0.0}, {2.0, 1.0}}}, tint};
}

void draw_step(const cells_context &c, const step &s, glintfx::gltfx_draw_layer layer) {
    if (s.shadow && s.texture != nullptr) {
        c.renderer.draw_shadow_texture(*s.texture, whole_paint(s.color), s.corners, layer);
        return;
    }
    if (s.shadow) {
        c.renderer.fill_shadow(s.corners, s.color, layer);
        return;
    }
    if (s.texture == nullptr) {
        c.renderer.fill_quad(s.corners, s.color, layer);
        return;
    }
    const glintfx::gltfx_texture_paint paint = whole_paint(k_white);
    const glintfx::gltfx_rect_world where{s.corners.top_left,
                                          {s.corners.bottom_right.x - s.corners.top_left.x,
                                           s.corners.bottom_right.y - s.corners.top_left.y}};
    c.renderer.draw_texture(*s.texture, paint, where, layer);
}

// The steps in paint order on a white surface. Without the key: in order, in one layer. With it:
// submitted in REVERSE, each in the layer of its place (only the key can give the paint order).
bool draw_order(const cells_context &c, std::span<const step> steps, bool keyed) {
    begin_white(c);
    for (std::size_t i = 0; i < steps.size(); ++i) {
        const std::size_t k = keyed ? steps.size() - 1 - i : i;
        draw_step(c, steps[k], glintfx::gltfx_draw_layer{keyed ? static_cast<std::int32_t>(k) : 0});
    }
    const auto finished = c.renderer.finish_frame();
    return finished.has_value() && finished.value().pieces_refused == 0;
}

std::string named(const char *cell, bool keyed, const char *what) {
    return std::string(cell) + (keyed ? " com chave: " : " sem chave: ") + what;
}

void cell_two_overlapping_shadows(const cells_context &c, bool keyed) {
    const step steps[] = {shadow_step(20, 60), shadow_step(40, 80)};
    report(c, named("H1", keyed, "o quadro termina"), draw_order(c, steps, keyed));
    report(c, named("H1", keyed, "so a primeira: uma vez"), gray_at(c, 30, 40, 0.5));
    report(c, named("H1", keyed, "a sobreposicao: uma vez, nunca duas"), gray_at(c, 50, 40, 0.5));
    report(c, named("H1", keyed, "so a segunda: uma vez"), gray_at(c, 70, 40, 0.5));
    report(c, named("H1", keyed, "fora: branco"), gray_at(c, 100, 40, 1.0));
}

void cell_shadow_over_a_piece_after_a_shadow(const cells_context &c, bool keyed) {
    const step steps[] = {shadow_step(20, 80), piece_step(40, 60, k_opaque_red),
                          shadow_step(20, 80)};
    report(c, named("H2", keyed, "o quadro termina"), draw_order(c, steps, keyed));
    report(c, named("H2", keyed, "o chao fora da peca: uma vez"), gray_at(c, 30, 40, 0.5));
    report(c, named("H2", keyed, "a peca opaca, escurecida pela segunda sombra"),
           near_pixel(read_at(c, 50, 40), level_of(c, 0.5), 0.0, 0.0));
    report(c, named("H2", keyed, "o chao do outro lado: uma vez"), gray_at(c, 70, 40, 0.5));
}

// Two texels: transparent on the left, opaque green on the right, with no filter between them.
std::optional<glintfx::gltfx_texture_2d> half_transparent_texture(const cells_context &c) {
    static const std::array<std::byte, 8> bytes{std::byte{0}, std::byte{0},  std::byte{0},
                                                std::byte{0}, std::byte{0},  std::byte{255},
                                                std::byte{0}, std::byte{255}};
    const glintfx::gltfx_image_view image{.pixels = bytes,
                                          .width = 2,
                                          .height = 1,
                                          .bytes_per_row = 8,
                                          .pixel_format =
                                              glintfx::gltfx_pixel_format::rgba8_srgb_straight};
    glintfx::gltfx_texture_2d_desc desc;
    desc.filter = glintfx::gltfx_texture_filter::nearest;
    glintfx::gltfx_rslt<glintfx::gltfx_texture_2d> made = c.renderer.create_texture(image, desc);
    if (made.has_error()) {
        report(c, "H3 create_texture", false);
        return std::nullopt;
    }
    return std::move(made.value());
}

void cell_transparent_pixel_does_not_release(const cells_context &c, bool keyed) {
    const auto texture = half_transparent_texture(c);
    if (!texture.has_value()) {
        return;
    }
    const step steps[] = {shadow_step(20, 80), step{false, box(40, 20, 60, 60), {}, &*texture},
                          shadow_step(20, 80)};
    report(c, named("H3", keyed, "a textura: o quadro termina"), draw_order(c, steps, keyed));
    report(c, named("H3", keyed, "o texel transparente nao libera: uma vez"),
           gray_at(c, 45, 40, 0.5));
    report(c, named("H3", keyed, "o texel opaco libera: o verde escurecido"),
           near_pixel(read_at(c, 55, 40), 0.0, level_of(c, 0.5), 0.0));
    const step alpha_zero[] = {shadow_step(20, 80),
                               piece_step(40, 60, glintfx::gltfx_rgba{1.0F, 0.0F, 0.0F, 0.0F}),
                               shadow_step(20, 80)};
    report(c, named("H3", keyed, "alfa 0: o quadro termina"), draw_order(c, alpha_zero, keyed));
    report(c, named("H3", keyed, "o retangulo de alfa 0 nao libera: uma vez"),
           gray_at(c, 50, 40, 0.5));
}

// White, darkened by 0.5, mixed with red of alpha 0.5, and the second shadow held back by the mark.
void cell_half_transparent_does_not_release(const cells_context &c, bool keyed) {
    const step steps[] = {shadow_step(20, 80),
                          piece_step(40, 60, glintfx::gltfx_rgba{1.0F, 0.0F, 0.0F, 0.5F}),
                          shadow_step(20, 80)};
    report(c, named("H4", keyed, "o quadro termina"), draw_order(c, steps, keyed));
    const double red = c.srgb ? level_of(c, 0.75) : (255.0 * 0.5 + 127.5 * 0.5);
    const double rest = c.srgb ? level_of(c, 0.25) : (127.5 * 0.5);
    report(c, named("H4", keyed, "a semitransparente nao libera: a segunda nao escurece"),
           near_pixel(read_at(c, 50, 40), red, rest, rest));
}

void cell_shadow_before_its_own_piece(const cells_context &c, bool keyed) {
    const step steps[] = {shadow_step(20, 80), piece_step(40, 60, k_opaque_red)};
    report(c, named("H5", keyed, "o quadro termina"), draw_order(c, steps, keyed));
    report(c, named("H5", keyed, "a peca por cima da propria sombra: intacta"),
           near_pixel(read_at(c, 50, 40), 255.0, 0.0, 0.0));
    report(c, named("H5", keyed, "a sombra ao lado da peca: uma vez"), gray_at(c, 30, 40, 0.5));
}

void cell_the_barrier(const cells_context &c) {
    begin_white(c);
    c.renderer.fill_shadow(box(20, 20, 60, 60), k_shade, glintfx::gltfx_draw_layer{3});
    c.renderer.flush();
    c.renderer.fill_shadow(box(20, 20, 60, 60), k_shade, glintfx::gltfx_draw_layer{0});
    const auto twice = c.renderer.finish_frame();
    report(c, "H6 flush: o quadro conta dois segmentos",
           twice.has_value() && twice.value().shadow_segments == 2U);
    report(c, "H6 flush: a sombra depois da barreira escurece de novo", gray_at(c, 40, 40, 0.25));
    begin_white(c);
    c.renderer.fill_shadow(box(20, 20, 60, 60), k_shade);
    c.renderer.begin_batch();
    c.renderer.fill_shadow(box(20, 20, 60, 60), k_shade);
    const auto once = c.renderer.finish_frame();
    report(c, "H6 begin_batch: um segmento so",
           once.has_value() && once.value().shadow_segments == 1U);
    report(c, "H6 begin_batch: nao encerra o segmento, uma vez", gray_at(c, 40, 40, 0.5));
}

void cell_the_tint(const cells_context &c) {
    const step red[] = {shadow_step(20, 60, glintfx::gltfx_rgba{1.0F, 0.0F, 0.0F, 0.5F})};
    report(c, "H7 vermelho: o quadro termina", draw_order(c, red, false));
    report(c, "H7 vermelho de alfa 0,5 mistura para o vermelho",
           near_pixel(read_at(c, 40, 40), 255.0, level_of(c, 0.5), level_of(c, 0.5)));
    const step black[] = {shadow_step(20, 60, glintfx::gltfx_rgba{0.0F, 0.0F, 0.0F, 1.0F})};
    report(c, "H7 preto: o quadro termina", draw_order(c, black, false));
    report(c, "H7 preto de alfa 1 deixa preto", gray_at(c, 40, 40, 0.0));
}

// The limit of the mark, half a level of alpha (embedded_program.hpp): what darkens nothing marks
// nothing, so the next shadow darkens; what darkens marks, so the next one does not.
void cell_the_mark_follows_the_darkening(const cells_context &c) {
    const step nothing[] = {shadow_step(20, 60, glintfx::gltfx_rgba{0.0F, 0.0F, 0.0F, 0.0F}),
                            shadow_step(20, 60)};
    report(c, "H7 alfa 0: o quadro termina", draw_order(c, nothing, false));
    report(c, "H7 a sombra de alfa 0 nao marca: a seguinte escurece", gray_at(c, 40, 40, 0.5));
    const step tiny[] = {shadow_step(20, 60, glintfx::gltfx_rgba{0.0F, 0.0F, 0.0F, 0.001F}),
                         shadow_step(20, 60)};
    report(c, "H7 alfa 0,001: o quadro termina", draw_order(c, tiny, false));
    report(c, "H7 abaixo de meio nivel de alfa nao marca", gray_at(c, 40, 40, 0.5));
    const step faint[] = {shadow_step(20, 60, glintfx::gltfx_rgba{0.0F, 0.0F, 0.0F, 0.1F}),
                          shadow_step(20, 60)};
    report(c, "H7 alfa 0,1: o quadro termina", draw_order(c, faint, false));
    report(c, "H7 a sombra de alfa 0,1 marca: a seguinte nao escurece", gray_at(c, 40, 40, 0.9));
}

struct counts {
    std::uint64_t calls = 0;
    std::uint64_t segments = 0;
};

// The two numbers of the report of a frame of quads along a row, in paint order.
counts counts_of(const cells_context &c, std::span<const step> steps) {
    begin_white(c);
    for (const step &s : steps) {
        draw_step(c, s, glintfx::gltfx_draw_layer{});
    }
    const auto finished = c.renderer.finish_frame();
    return finished.has_value()
               ? counts{finished.value().draw_calls, finished.value().shadow_segments}
               : counts{};
}

void cell_the_count(const cells_context &c) {
    const step plain = piece_step(0, 10, k_opaque_red);
    const step shade = shadow_step(10, 20);
    const step rectangles[] = {plain, plain, plain};
    const counts none = counts_of(c, rectangles);
    report(c, "H8 tres retangulos: 1 chamada, nenhum segmento",
           none.calls == 1U && none.segments == 0U);
    const step sandwich[] = {plain, shade, plain};
    const counts four = counts_of(c, sandwich);
    report(c, "H8 retangulo, sombra e retangulo: 4 chamadas, 1 segmento",
           four.calls == 4U && four.segments == 1U);
    const step alternating[] = {plain, shade, plain, shade};
    const counts five = counts_of(c, alternating);
    report(c, "H8 quatro runs com duas sombras: 5 chamadas, 1 segmento",
           five.calls == 5U && five.segments == 1U);
    const step shadows[] = {shade, shade};
    const counts one = counts_of(c, shadows);
    report(c, "H8 duas sombras seguidas: 1 chamada, 1 segmento",
           one.calls == 1U && one.segments == 1U);
}

void submit_refused_shadows(const cells_context &c) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    const float nan_float = std::numeric_limits<float>::quiet_NaN();
    c.renderer.fill_shadow(glintfx::gltfx_quad_world{{nan, 0}, {1, 0}, {1, 1}, {0, 1}}, k_shade);
    c.renderer.fill_shadow(glintfx::gltfx_quad_world{{0, 0}, {1, inf}, {1, 1}, {0, 1}}, k_shade);
    c.renderer.fill_shadow(box(0, 0, 1, 1), glintfx::gltfx_rgba{nan_float, 0.0F, 0.0F, 0.5F});
    c.renderer.fill_shadow(box(1e300, 0, 2e300, 1), k_shade);
}

void cell_the_refusals(const cells_context &c) {
    c.renderer.fill_shadow(box(0, 0, 1, 1), k_shade); // no frame open: the next report
    begin_white(c);
    submit_refused_shadows(c);
    c.renderer.fill_shadow(box(40, 40, 40, 60), k_shade); // zero area: drawn, covers no pixel
    const auto finished = c.renderer.finish_frame();
    report(c, "H9 o quadro termina com valor", finished.has_value());
    if (!finished.has_value()) {
        return;
    }
    const glintfx::gltfx_frame_2d_report r = finished.value();
    report(c, "H9 cinco pecas, uma desenhada, quatro recusadas",
           r.pieces_submitted == 5U && r.pieces_drawn == 1U && r.pieces_refused == 4U);
    report(c, "H9 o primeiro motivo",
           r.first_refusal == glintfx::gltfx_draw_2d_refusal::not_a_number);
    report(c, "H9 o contador de cada motivo",
           r.pieces_refused_not_a_number == 2U && r.pieces_refused_infinite == 1U &&
               r.pieces_refused_pixel_position_out_of_range == 1U);
    report(c, "H9 nada recusado por stencil nesta superficie",
           r.pieces_refused_stencil_unavailable == 0U);
    report(c, "H9 sem quadro aberto: no relatorio seguinte", r.pieces_dropped_outside_frame == 1U);
    report(c, "H9 a sombra de area zero nao cobre pixel", gray_at(c, 40, 50, 1.0));
    report(c, "H9 o token",
           glintfx::gltfx_draw_2d_refusal_name(
               glintfx::gltfx_draw_2d_refusal::stencil_unavailable) == "stencil_unavailable");
}

gl_int read_int(const cells_context &c, gl_enum name) {
    gl_int value = -1;
    c.gl.get_integerv(name, &value);
    return value;
}

// A write or value mask that holds every bit of the surface's 8. Only those are compared: the
// driver clamps what it answers (Mesa 26.1.8 reads 0x7FFFFFFF for 0xFFFFFFFF, measured by the
// H3a-fix2).
bool every_bit(const cells_context &c, gl_enum name) {
    return (static_cast<unsigned int>(read_int(c, name)) & 0xFFU) == 0xFFU;
}

// The function, the reference and the three operations of one face, then its two masks.
struct stencil_face {
    gl_enum func, ref, fail, depth_fail, depth_pass, value_mask, write_mask;
};

bool face_at_gl_defaults(const cells_context &c, const stencil_face &f) {
    constexpr gl_int k_always = 0x0207;
    constexpr gl_int k_keep = 0x1E00;
    return read_int(c, f.func) == k_always && read_int(c, f.ref) == 0 &&
           read_int(c, f.fail) == k_keep && read_int(c, f.depth_fail) == k_keep &&
           read_int(c, f.depth_pass) == k_keep && every_bit(c, f.value_mask) &&
           every_bit(c, f.write_mask);
}

// The values GL starts with, both faces (GL 3.3 core, sec. 4.1.4), as the closed list of flush()
// says.
bool stencil_at_gl_defaults(const cells_context &c) {
    const stencil_face front{0x0B92, 0x0B97, 0x0B94, 0x0B95, 0x0B96, 0x0B93, 0x0B98};
    const stencil_face back{0x8800, 0x8CA3, 0x8801, 0x8802, 0x8803, 0x8CA4, 0x8CA5};
    return face_at_gl_defaults(c, front) && face_at_gl_defaults(c, back) &&
           read_int(c, k_gl_stencil_test) == 0;
}

// What a consumer might leave: the test on, a function that never passes, inverting, no bit
// writable.
void plant_hostile_stencil(const cells_context &c) {
    c.gl.enable(k_gl_stencil_test);
    c.gl.stencil_func(k_gl_never, 7, 0x0FU);
    c.gl.stencil_op(k_gl_invert, k_gl_invert, k_gl_invert);
    c.gl.stencil_mask(0U);
}

void cell_the_stencil_a_barrier_leaves(const cells_context &c) {
    plant_hostile_stencil(c);
    begin_white(c);
    c.renderer.fill_shadow(box(20, 20, 60, 60), k_shade);
    c.renderer.fill_quad(box(0, 0, 4, 4), k_opaque_red); // the release pass is the last one set
    c.renderer.flush();
    report(c, "H10 o estado hostil nao muda o que a sombra escurece", gray_at(c, 40, 40, 0.5));
    report(c, "H10 depois de um flush com sombra", stencil_at_gl_defaults(c));
    plant_hostile_stencil(c);
    c.renderer.fill_quad(box(0, 0, 4, 4), k_opaque_red);
    c.renderer.flush();
    report(c, "H10 depois de um flush sem sombra", stencil_at_gl_defaults(c));
    plant_hostile_stencil(c);
    const auto finished = c.renderer.finish_frame();
    report(c, "H10 depois de finish_frame", finished.has_value() && stencil_at_gl_defaults(c));
}

// The clear value goes back to 0 afterwards: a stray clear of the stencil by the library would
// write that value, so if it stayed at `value` the cell could not see the clear (the mutant CLEARC
// survived the first version of this function).
void write_consumer_stencil(const cells_context &c, gl_int value) {
    c.gl.disable(k_gl_stencil_test);
    c.gl.stencil_mask(0xFFFFFFFFU);
    c.gl.clear_stencil(value);
    c.gl.clear(k_gl_stencil_buffer_bit);
    c.gl.clear_stencil(0);
}

void cell_the_stencil_buffer_of_the_consumer(const cells_context &c) {
    constexpr gl_int k_written = 0x5A;
    write_consumer_stencil(c, k_written);
    begin_white(c); // with the clear colour
    c.renderer.fill_quad(box(20, 20, 60, 60), k_opaque_red);
    (void)c.renderer.finish_frame();
    report(c, "H11 um quadro sem sombra guarda o que o consumidor escreveu",
           stencil_at(c, 40, 40) == k_written && stencil_at(c, 100, 100) == k_written);
    write_consumer_stencil(c, k_written);
    begin_white(c);
    c.renderer.fill_shadow(box(20, 20, 60, 60), k_shade);
    (void)c.renderer.finish_frame();
    report(c, "H11 o que o consumidor escreveu nao muda o que a sombra escurece",
           gray_at(c, 40, 40, 0.5));
    report(c, "H11 um quadro com sombra limpa o stencil fora dela", stencil_at(c, 100, 100) == 0);
    report(c, "H11 e deixa as marcas dentro dela", stencil_at(c, 40, 40) != k_written);
    write_consumer_stencil(c, 0);
}

void cell_the_bits_of_the_surface(const cells_context &c) {
    gl_int bits = -1;
    c.gl.get_attachment(k_gl_draw_framebuffer, k_gl_stencil, k_gl_attachment_stencil_size, &bits);
    std::fprintf(stdout, "MEASURED draw2d_shadow_parity_test.stencil_bits_%s=%d\n", c.mode, bits);
    report(c, "H12 a superficie tem pelo menos 8 bits", bits >= 8);
    begin_white(c);
    c.renderer.fill_shadow(box(20, 20, 60, 60), k_shade);
    const auto finished = c.renderer.finish_frame();
    const std::uint64_t refused =
        finished.has_value() ? finished.value().pieces_refused_stencil_unavailable : 99U;
    std::fprintf(stdout, "MEASURED draw2d_shadow_parity_test.stencil_unavailable_%s=%llu\n", c.mode,
                 static_cast<unsigned long long>(refused));
    report(c, "H12 o desenhador nao recusou a sombra", refused == 0U);
}

int g_unread_events = 0;

void count_unread_events(void *, const glintfx::gltfx_log_event &event) noexcept {
    g_unread_events += event.name() == "draw2d_stencil_bits_unread" ? 1 : 0;
}

// The texture of H13 to H19: `bytes` holds `width` by `height` straight texels, with no filter.
std::optional<glintfx::gltfx_texture_2d> texture_of(const cells_context &c,
                                                    std::span<const std::byte> bytes,
                                                    std::uint32_t width, std::uint32_t height) {
    const glintfx::gltfx_image_view image{.pixels = bytes,
                                          .width = width,
                                          .height = height,
                                          .bytes_per_row = static_cast<std::size_t>(width) * 4,
                                          .pixel_format =
                                              glintfx::gltfx_pixel_format::rgba8_srgb_straight};
    glintfx::gltfx_texture_2d_desc desc;
    desc.filter = glintfx::gltfx_texture_filter::nearest;
    glintfx::gltfx_rslt<glintfx::gltfx_texture_2d> made = c.renderer.create_texture(image, desc);
    if (made.has_error()) {
        report(c, "H13 create_texture", false);
        return std::nullopt;
    }
    return std::move(made.value());
}

step silhouette_step(const glintfx::gltfx_texture_2d &texture, glintfx::gltfx_rgba tint) {
    return step{true, box(20, 20, 60, 60), tint, &texture};
}

void cell_silhouette_shape(const cells_context &c, const glintfx::gltfx_texture_2d &half) {
    const step black[] = {silhouette_step(half, k_shade)};
    report(c, "H13 preto: o quadro termina", draw_order(c, black, false));
    report(c, "H13 o texel transparente nao escurece", gray_at(c, 30, 40, 1.0));
    report(c, "H13 o texel opaco escurece pelo alfa da tinta", gray_at(c, 50, 40, 0.5));
    const step red[] = {silhouette_step(half, glintfx::gltfx_rgba{1.0F, 0.0F, 0.0F, 0.5F})};
    report(c, "H13 vermelho: o quadro termina", draw_order(c, red, false));
    report(c, "H13 a cor do texel nao entra: a tinta vermelha mistura para o vermelho",
           near_pixel(read_at(c, 50, 40), 255.0, level_of(c, 0.5), level_of(c, 0.5)));
    begin_white(c);
    glintfx::gltfx_texture_paint mirrored = whole_paint(k_shade);
    mirrored.region.mirror = glintfx::gltfx_texture_mirror::flip_left_right;
    c.renderer.draw_shadow_texture(half, mirrored, box(20, 20, 60, 60));
    (void)c.renderer.finish_frame();
    report(c, "H13 o espelho troca os dois lados",
           gray_at(c, 30, 40, 0.5) && gray_at(c, 50, 40, 1.0));
}

void cell_silhouette_marks_where_it_darkens(const cells_context &c, bool keyed,
                                            const glintfx::gltfx_texture_2d &half) {
    const step steps[] = {silhouette_step(half, k_shade), shadow_step(20, 60)};
    report(c, named("H14", keyed, "o quadro termina"), draw_order(c, steps, keyed));
    report(c, named("H14", keyed, "o texel transparente nao marca: a seguinte escurece"),
           gray_at(c, 30, 40, 0.5));
    report(c, named("H14", keyed, "o texel opaco marca: a seguinte nao escurece, uma vez"),
           gray_at(c, 50, 40, 0.5));
}

void cell_silhouettes_overlap_and_release(const cells_context &c, bool keyed,
                                          const glintfx::gltfx_texture_2d &half) {
    const step overlap[] = {silhouette_step(half, k_shade), silhouette_step(half, k_shade)};
    report(c, named("H15", keyed, "duas: o quadro termina"), draw_order(c, overlap, keyed));
    report(c, named("H15", keyed, "duas silhuetas sobrepostas: uma vez"), gray_at(c, 50, 40, 0.5));
    const step between[] = {silhouette_step(half, k_shade), piece_step(45, 55, k_opaque_red),
                            silhouette_step(half, k_shade)};
    report(c, named("H15", keyed, "com a peca: o quadro termina"), draw_order(c, between, keyed));
    report(c, named("H15", keyed, "a peca opaca libera: a segunda a escurece"),
           near_pixel(read_at(c, 50, 40), level_of(c, 0.5), 0.0, 0.0));
}

void cell_silhouette_half_alpha(const cells_context &c) {
    const std::array<std::byte, 8> half_alpha{std::byte{9},   std::byte{9},  std::byte{9},
                                              std::byte{128}, std::byte{9},  std::byte{9},
                                              std::byte{9},   std::byte{128}};
    const auto texture = texture_of(c, half_alpha, 2, 1);
    if (!texture.has_value()) {
        return;
    }
    const glintfx::gltfx_rgba solid{0.0F, 0.0F, 0.0F, 1.0F};
    const step steps[] = {silhouette_step(*texture, solid), shadow_step(20, 60)};
    report(c, "H16 o quadro termina", draw_order(c, steps, false));
    report(c, "H16 alfa 0,5 sob tinta de alfa 1: escurece 0,5, e marca", gray_at(c, 40, 40, 0.5));
}

// A checkerboard of two by two, opaque black and transparent, in a trapezoid.
void draw_checker(const cells_context &c, const glintfx::gltfx_texture_2d &checker,
                  bool silhouette) {
    const glintfx::gltfx_quad_world trapezoid{{40, 20}, {160, 20}, {220, 200}, {10, 200}};
    const glintfx::gltfx_texture_paint paint{
        glintfx::gltfx_texture_region{glintfx::gltfx_rect_texel{{0.0, 0.0}, {2.0, 2.0}}},
        glintfx::gltfx_rgba{silhouette ? 0.0F : 1.0F, silhouette ? 0.0F : 1.0F,
                            silhouette ? 0.0F : 1.0F, 1.0F}};
    begin_white(c);
    if (silhouette) {
        c.renderer.draw_shadow_texture(checker, paint, trapezoid);
    } else {
        c.renderer.draw_texture_quad(checker, paint, trapezoid);
    }
    (void)c.renderer.finish_frame();
}

bool is_dark(const cells_context &c, int x, int y) { return read_at(c, x, y).red < 64; }

void cell_silhouette_in_a_trapezoid(const cells_context &c) {
    const std::array<std::byte, 16> texels{
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{255}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},   std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{255}};
    const auto checker = texture_of(c, texels, 2, 2);
    if (!checker.has_value()) {
        return;
    }
    std::vector<bool> painted;
    draw_checker(c, *checker, false);
    for (int y = 24; y < 200; y += 6) {
        for (int x = 14; x < 216; x += 6) {
            painted.push_back(is_dark(c, x, y));
        }
    }
    draw_checker(c, *checker, true);
    std::size_t at = 0;
    int different = 0;
    int dark = 0;
    for (int y = 24; y < 200; y += 6) {
        for (int x = 14; x < 216; x += 6) {
            const bool shadowed = is_dark(c, x, y);
            different += shadowed != painted[at] ? 1 : 0;
            dark += shadowed ? 1 : 0;
            ++at;
        }
    }
    std::fprintf(stdout,
                 "draw2d_shadow_parity_test: [srgb=%s] H17 sondas=%zu diferentes=%d escuras=%d\n",
                 c.mode, at, different, dark);
    report(c, "H17 a silhueta cobre os pixels que draw_texture_quad pinta", different == 0);
    report(c, "H17 a sonda viu os dois lados", dark > 100 && dark + 100 < static_cast<int>(at));
}

void cell_silhouette_of_a_projected_piece(const cells_context &c,
                                          const glintfx::gltfx_texture_2d &half) {
    const glintfx::gltfx_point_light_2d lamp{{120.0, 0.0}, 80.0};
    const glintfx::gltfx_standing_piece_2d piece{{100.0, 60.0}, {140.0, 60.0}, 40.0};
    const auto shadow = glintfx::gltfx_project_point_shadow(lamp, piece);
    report(c, "H18 a projecao tem valor", shadow.has_value());
    if (!shadow.has_value()) {
        return;
    }
    begin_white(c);
    c.renderer.draw_shadow_texture(half, whole_paint(k_shade), shadow.value());
    (void)c.renderer.finish_frame();
    report(c, "H18 o texel de base_start cai do lado de base_start", gray_at(c, 105, 90, 1.0));
    report(c, "H18 o texel de base_end cai do lado de base_end", gray_at(c, 135, 90, 0.5));
    const glintfx::gltfx_directional_light_2d sun{glintfx::gltfx_angle_from_degrees(0.0),
                                                  glintfx::gltfx_angle_from_degrees(90.0)};
    const auto overhead = glintfx::gltfx_project_directional_shadow(sun, piece);
    report(c, "H18 o sol a pino: a projecao tem valor", overhead.has_value());
    if (!overhead.has_value()) {
        return;
    }
    begin_white(c);
    c.renderer.draw_shadow_texture(half, whole_paint(k_shade), overhead.value());
    const auto finished = c.renderer.finish_frame();
    report(c, "H18 o sol a pino: desenhada, nao recusada, um trecho",
           finished.has_value() && finished.value().pieces_refused == 0U &&
               finished.value().pieces_drawn == 1U && finished.value().shadow_segments == 1U);
    report(c, "H18 o sol a pino: nenhum pixel", gray_at(c, 120, 60, 1.0));
}

void cell_silhouette_refusals_and_count(const cells_context &c,
                                        const glintfx::gltfx_texture_2d &half) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    glintfx::gltfx_texture_paint outside = whole_paint(k_shade);
    outside.region.source.size = {9.0, 9.0};
    const std::array<std::byte, 8> texels{std::byte{0}};
    auto closed = texture_of(c, texels, 2, 1);
    if (!closed.has_value()) {
        return;
    }
    const glintfx::gltfx_texture_2d moved = std::move(*closed); // the one left behind is not open
    begin_white(c);
    c.renderer.draw_shadow_texture(half, whole_paint(k_shade),
                                   glintfx::gltfx_quad_world{{nan, 0}, {1, 0}, {1, 1}, {0, 1}});
    c.renderer.draw_shadow_texture(*closed, whole_paint(k_shade), box(0, 0, 1, 1));
    c.renderer.draw_shadow_texture(half, outside, box(0, 0, 1, 1));
    c.renderer.draw_shadow_texture(half, whole_paint(k_shade),
                                   glintfx::gltfx_quad_world{{0, 0}, {2, 0}, {4, 0}, {0, 4}});
    const auto refused = c.renderer.finish_frame();
    report(c, "H19 o quadro termina", refused.has_value());
    if (refused.has_value()) {
        const glintfx::gltfx_frame_2d_report r = refused.value();
        report(c, "H19 cada recusa e uma peca, com o seu motivo",
               r.pieces_submitted == 4U && r.pieces_refused == 4U &&
                   r.pieces_refused_not_a_number == 1U && r.pieces_refused_texture_not_open == 1U &&
                   r.pieces_refused_region_out_of_range == 1U &&
                   r.pieces_refused_degenerate_quad == 1U && r.shadow_segments == 0U);
    }
    begin_white(c);
    c.renderer.fill_rect(glintfx::gltfx_rect_world{{0, 0}, {4, 4}}, k_opaque_red);
    c.renderer.draw_shadow_texture(half, whole_paint(k_shade), box(20, 20, 60, 60));
    c.renderer.fill_rect(glintfx::gltfx_rect_world{{4, 0}, {4, 4}}, k_opaque_red);
    const auto counted = c.renderer.finish_frame();
    report(c, "H19 retangulo, silhueta e retangulo: 4 chamadas, 2 trocas, 1 trecho",
           counted.has_value() && counted.value().draw_calls == 4U &&
               counted.value().texture_switches == 2U && counted.value().shadow_segments == 1U);
}

void run_silhouette_cells(const cells_context &c) {
    const std::array<std::byte, 8> transparent_and_green{
        std::byte{0}, std::byte{0},   std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{255}, std::byte{0}, std::byte{255}};
    const auto half = texture_of(c, transparent_and_green, 2, 1);
    if (!half.has_value()) {
        return;
    }
    cell_silhouette_shape(c, *half);
    for (const bool keyed : {false, true}) {
        cell_silhouette_marks_where_it_darkens(c, keyed, *half);
        cell_silhouettes_overlap_and_release(c, keyed, *half);
    }
    cell_silhouette_half_alpha(c);
    cell_silhouette_in_a_trapezoid(c);
    cell_silhouette_of_a_projected_piece(c, *half);
    cell_silhouette_refusals_and_count(c, *half);
}

void cell_open_with_a_framebuffer_of_the_consumer(const cells_context &c) {
    for (int read = 0; read < 16 && c.gl.get_error() != 0U; ++read) {
    }
    g_unread_events = 0;
    const glintfx::gltfx_log_sink previous = glintfx::gltfx_log_set_sink(
        glintfx::gltfx_log_sink{&count_unread_events, nullptr, glintfx::gltfx_log_severity::trace});
    {
        const auto plain =
            glintfx::gltfx_renderer_2d::open(c.context, glintfx::gltfx_renderer_2d_desc{});
        report(c, "H12 um open com a ligacao do consumidor limpa abre", plain.has_value());
    }
    glintfx::gltfx_log_set_sink(previous);
    std::fprintf(stdout, "MEASURED draw2d_shadow_parity_test.stencil_bits_unread_%s=%d\n", c.mode,
                 g_unread_events);
    report(c, "H12 a leitura dos bits nao da erro neste driver", g_unread_events == 0);
    gl_uint framebuffer = 0;
    c.gl.gen_framebuffers(1, &framebuffer);
    c.gl.bind_framebuffer(k_gl_draw_framebuffer, framebuffer);
    const auto opened =
        glintfx::gltfx_renderer_2d::open(c.context, glintfx::gltfx_renderer_2d_desc{});
    report(c, "H12 o open com um framebuffer do consumidor ligado abre", opened.has_value());
    report(c, "H12 a ligacao volta como estava",
           read_int(c, k_gl_draw_framebuffer_binding) == static_cast<gl_int>(framebuffer));
    report(c, "H12 nenhum erro de GL sobra para o consumidor", c.gl.get_error() == 0U);
    c.gl.bind_framebuffer(k_gl_draw_framebuffer, 0U);
    c.gl.delete_framebuffers(1, &framebuffer);
}

void run_cells(const cells_context &c) {
    for (const bool keyed : {false, true}) {
        cell_two_overlapping_shadows(c, keyed);
        cell_shadow_over_a_piece_after_a_shadow(c, keyed);
        cell_transparent_pixel_does_not_release(c, keyed);
        cell_half_transparent_does_not_release(c, keyed);
        cell_shadow_before_its_own_piece(c, keyed);
    }
    cell_the_barrier(c);
    cell_the_tint(c);
    cell_the_mark_follows_the_darkening(c);
    cell_the_count(c);
    cell_the_refusals(c);
    cell_the_stencil_a_barrier_leaves(c);
    cell_the_stencil_buffer_of_the_consumer(c);
    cell_the_bits_of_the_surface(c);
    cell_open_with_a_framebuffer_of_the_consumer(c);
    run_silhouette_cells(c);
}

enum class mode_result : std::uint8_t { ran, absent, fatal };

std::optional<glintfx::gltfx_gl_context> open_context(glintfx::gltfx_window &window, bool srgb,
                                                      mode_result &result) {
    const glintfx::gltfx_gfx_option_entry options[] = {
        {.id = glintfx::gltfx_gfx_option::srgb_framebuffer, .value = srgb ? 1 : 0}};
    const glintfx::gltfx_gl_context_desc desc{.options = options, .option_count = 1};
    glintfx::gltfx_rslt<glintfx::gltfx_gl_context> opened =
        glintfx::gltfx_gl_context::open(window, desc);
    if (opened.has_value()) {
        return std::move(opened.value());
    }
    if (srgb && opened.err().code() == glintfx::gltfx_err_code::unsupported &&
        opened.err().rejected_value() == "srgb_framebuffer") {
        std::fprintf(stdout, "draw2d_shadow_parity_test: AUSENCIA DECLARADA: "
                             "srgb_framebuffer=on nao e suportado aqui\n");
        result = mode_result::absent;
        return std::nullopt;
    }
    std::fprintf(stderr, "draw2d_shadow_parity_test: FAIL gltfx_gl_context::open(srgb=%d): %s\n",
                 srgb ? 1 : 0, std::string(opened.err().rejected_value()).c_str());
    result = mode_result::fatal;
    return std::nullopt;
}

// The renderer over a current context, the surface size checked, then every cell.
mode_result run_with_context(glintfx::gltfx_gl_context &context, bool srgb) {
    gl_api gl;
    if (context.make_current().has_error() || !load_gl(context, gl)) {
        return mode_result::fatal;
    }
    glintfx::gltfx_rslt<glintfx::gltfx_renderer_2d> opened =
        glintfx::gltfx_renderer_2d::open(context, glintfx::gltfx_renderer_2d_desc{});
    if (opened.has_error()) {
        std::fprintf(stderr, "draw2d_shadow_parity_test: FAIL renderer_2d::open(srgb=%d)\n",
                     srgb ? 1 : 0);
        return mode_result::fatal;
    }
    glintfx::gltfx_renderer_2d renderer = std::move(opened.value());
    renderer.begin_frame(glintfx::gltfx_frame_2d_desc{});
    (void)renderer.finish_frame();
    gl_int viewport[4] = {0, 0, 0, 0};
    gl.get_integerv(k_gl_viewport, viewport);
    if (viewport[2] < 320 || viewport[3] < 240) {
        std::fprintf(stderr, "draw2d_shadow_parity_test: FAIL superficie menor que a geometria\n");
        return mode_result::fatal;
    }
    run_cells(cells_context{renderer, context, gl, srgb, viewport[3], srgb ? "on" : "off"});
    return mode_result::ran;
}

mode_result run_mode(glintfx::gltfx_display &display, bool srgb) {
    const glintfx::gltfx_window_desc window_desc{
        .title = "janela de paridade das sombras",
        .application_id = "org.glintfx.draw2d_shadow_parity_test",
        .logical_size = {.width = 320, .height = 240},
    };
    glintfx::gltfx_rslt<glintfx::gltfx_window> window_opened =
        glintfx::gltfx_window::open(display, window_desc);
    if (window_opened.has_error()) {
        std::fprintf(stderr, "draw2d_shadow_parity_test: FAIL gltfx_window::open()\n");
        return mode_result::fatal;
    }
    glintfx::gltfx_window window = std::move(window_opened.value());
    mode_result result = mode_result::ran;
    std::optional<glintfx::gltfx_gl_context> context = open_context(window, srgb, result);
    if (!context.has_value()) {
        return result;
    }
    return run_with_context(*context, srgb);
}

int fail(const char *why) {
    std::fprintf(stderr, "draw2d_shadow_parity_test: FAIL %s\n", why);
    return EXIT_FAILURE;
}

} // namespace

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    glintfx::gltfx_rslt<glintfx::gltfx_display> display_opened = glintfx::gltfx_display::open();
    if (display_opened.has_error()) {
        return fail("gltfx_display::open()");
    }
    glintfx::gltfx_display display = std::move(display_opened.value());
    if (run_mode(display, false) != mode_result::ran) {
        return fail("o modo srgb_framebuffer=off (o padrao) tem de rodar em todo driver");
    }
    const mode_result on = run_mode(display, true);
    if (on == mode_result::fatal) {
        return fail(
            "o modo srgb_framebuffer=on falhou por outro motivo que nao a falta de suporte");
    }
    std::fprintf(stdout, "MEASURED draw2d_shadow_parity_test.srgb_on_cells_absent=%d\n",
                 on == mode_result::absent ? 1 : 0);
    std::fprintf(stdout, "draw2d_shadow_parity_test: %d celula(s) conferida(s), %d reprovada(s)\n",
                 g_cells, g_failures);
    // Piso de varredura nao-vazia (GODS_LAWS.md L-40): a run that checked nothing is not a pass.
    if (g_cells < 112) {
        return fail("menos celulas do que o piso (varredura quebrada)");
    }
    return g_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
