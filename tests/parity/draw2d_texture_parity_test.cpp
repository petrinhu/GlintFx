// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <glintfx/core/color.hpp>
#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/core/image_view.hpp>
#include <glintfx/core/quad.hpp>
#include <glintfx/core/rect.hpp>
#include <glintfx/draw2d/frame_2d_report.hpp>
#include <glintfx/draw2d/renderer_2d.hpp>
#include <glintfx/draw2d/texture_2d.hpp>
#include <glintfx/draw2d/texture_2d_desc.hpp>
#include <glintfx/draw2d/texture_region.hpp>
#include <glintfx/platform/gl/context.hpp>
#include <glintfx/platform/gl/gfx_option.hpp>
#include <glintfx/platform/window/display.hpp>
#include <glintfx/platform/window/window.hpp>

// draw2d_texture_parity_test.cpp - R2D-TEXTURE, fatia T4b-B2 (D-W8-475 a 480): the textured drawing
// of the 2D layer through the PUBLIC API only, READ BACK AS PIXELS from a real surface. ONE file,
// no #if, no harness, the shape of draw2d_parity_test.cpp: the Windows side links it into an
// add_executable() target (tests/CMakeLists.txt, if(WIN32)); the Linux side runs it as a container
// fixture of this exact name, inside a nested compositor (GODS_LAWS.md L-09), never on the live
// session.
//
// THE CRITERIA ARE FIXED HERE, BEFORE THE DATA EXISTS (GODS_LAWS.md L-43), asserted on both
// systems and in both values of the context option srgb_framebuffer (a declared, counted absence
// where a driver refuses ON, as in draw2d_parity_test.cpp). Every expected value comes from a
// formula written below, never from a reading of this code's own output:
//   - THE FILTER (D-W8-305): a 2 x 1 texture drawn into 200 x 4 pixels starting at x0. The pixel
//     x0 + k samples the texture at t = (k + 0.5) / 100 texels, and the right texel weighs
//     w = t - 0.5 there (0.495 at k = 99, 0.505 at k = 100).
//       * filter_nearest_edge: red | blue, nearest: k = 99 exactly red, k = 100 exactly blue;
//       * filter_linear_srgb_order: black | white, linear: 255 * srgb(w) +- 2 (186.7 at k = 99,
//         188.3 at k = 100), the same in both modes. A driver that converts sRGB AFTER the blend
//         reads about 127 and fails with its GL_RENDERER named;
//       * filter_linear_no_dark_halo: opaque red | transparent, linear, over black, at k = 100:
//         red 255 * (1 - w) = 126.2 +- 2 when the surface does not encode (the program encodes the
//         straight colour and blends encoded values) and 255 * srgb(1 - w) = 186.7 +- 2 when it
//         does; green and blue EXACTLY 0 in both (a straight-alpha texel would leave a dark halo).
//   - THE PROJECTIVE QUADRILATERAL (D-W8-463, D-W8-476): an 8 x 8 checkerboard, nearest, drawn into
//     the trapezoid (20,20) (300,20) (200,220) (120,220). The oracle is the square-to-quadrilateral
//     homography of Heckbert (1989), written here from its equations: for every pixel of an 8-pixel
//     grid whose centre maps, by the INVERSE homography, at least a quarter texel inside a square,
//     the pixel has that square's colour (white or black, exact). Among those pixels, the ones
//     whose AFFINE reading (each of the two triangles interpolated linearly, as a program that
//     ignores q would draw) falls, also a quarter texel inside, in a square of the OTHER colour are
//     counted: at least 10 of them, or the cell does not discriminate and fails.
//   - THE ATTRIBUTE OF q (D-W8-463, D-W8-477): among the program objects alive in the context
//     (names 1 to 4096, glIsProgram), the one with the uniform u_viewport_pixels is the library's;
//     its attribute a_texcoord_q has location 3. A driver may drop an attribute that never reaches
//     the output (location -1), so this is an AUXILIARY lock where the driver does that (llvmpipe
//     does); the trapezoid is the proof on every driver.
//   - THE ORDER (D-W8-13): textures A, B, A on the same layer, overlapping, are painted in that
//     order (the overlap of A and B is B, the overlap of B and the second A is A), and the report
//     says texture_switches 3; a solid piece, a textured one and a solid one say texture_switches 2
//     (D-W8-292: a change to or from no texture counts).
//   - THE MIRRORS (D-W8-297): a 2 x 2 texture red | green over blue | white, nearest, drawn into
//     40 x 40 with each of the four mirrors; the centre of each quarter reads the texel the mirror
//     puts there, exactly.
//   - THE TINT: an opaque white texel tinted white with alpha 0.5, over black: 128 +- 2 when the
//     surface does not encode, 188 +- 2 when it does (the premultiplied edge of
//     draw2d_parity_test).
//   - THE TEXTURED TRIANGLES (R2D-TEXTURE T5, D-W8-678): two indexed triangles of opposite
//     orientations over a 40 x 40 square, reading the whole of the 2 x 2 texture of the mirrors,
//     put each texel in its quarter, exactly, texel (0, 0) at the top left (TT1); one triangle
//     whose three vertices are white with alpha 0.5, over an opaque white texel and black, reads
//     the value of the tint cell, 128 or 188 +- 2 (TT2); a rectangle and a triangle of the same
//     texture, one after the other, read the texture (red, exact) and the report says draw_calls 1
//     and texture_switches 1 (TT3).
// MEASURED, PRINTED, NEVER ASSERTED EQUAL: the value read at k = 100 of filter_linear_srgb_order in
// each mode, and whether the ON mode was absent.
//
// Geometry assumes a scale factor of 1 (both fixtures run at 1). The surface is 320 by 240.

namespace {

using gl_enum = unsigned int;
using gl_uint = unsigned int;
using gl_int = int;
using gl_sizei = int;
using gl_bool = unsigned char;

constexpr gl_enum k_gl_viewport = 0x0BA2;
constexpr gl_enum k_gl_rgba = 0x1908;
constexpr gl_enum k_gl_unsigned_byte = 0x1401;
constexpr gl_enum k_gl_renderer = 0x1F01;

// The GL entry points the cells need, resolved through the PUBLIC gltfx_gl_context::proc_address().
struct gl_api {
    void (*get_integerv)(gl_enum, gl_int *) = nullptr;
    void (*read_pixels)(gl_int, gl_int, gl_sizei, gl_sizei, gl_enum, gl_enum, void *) = nullptr;
    void (*finish)() = nullptr;
    const unsigned char *(*get_string)(gl_enum) = nullptr;
    gl_bool (*is_program)(gl_uint) = nullptr;
    gl_int (*get_attrib_location)(gl_uint, const char *) = nullptr;
    gl_int (*get_uniform_location)(gl_uint, const char *) = nullptr;
};

template <typename F> bool resolve(glintfx::gltfx_gl_context &context, const char *name, F &out) {
    void *address = context.proc_address(name);
    if (address == nullptr) {
        std::fprintf(stderr, "draw2d_texture_parity_test: FAIL %s nao resolvido pelo contexto\n",
                     name);
        return false;
    }
    out = reinterpret_cast<F>(address);
    return true;
}

bool load_gl(glintfx::gltfx_gl_context &context, gl_api &gl) {
    return resolve(context, "glGetIntegerv", gl.get_integerv) &&
           resolve(context, "glReadPixels", gl.read_pixels) &&
           resolve(context, "glFinish", gl.finish) &&
           resolve(context, "glGetString", gl.get_string) &&
           resolve(context, "glIsProgram", gl.is_program) &&
           resolve(context, "glGetAttribLocation", gl.get_attrib_location) &&
           resolve(context, "glGetUniformLocation", gl.get_uniform_location);
}

struct pixel {
    int red = 0;
    int green = 0;
    int blue = 0;
};

// Everything one cell needs: the renderer under test, the GL entry points, the mode of the surface
// and its height (glReadPixels counts rows from the BOTTOM, the drawing from the TOP).
struct cells_context {
    glintfx::gltfx_renderer_2d &renderer;
    const gl_api &gl;
    bool srgb = false;
    int height = 0;
    const char *mode = ""; // "off" or "on", for the messages
};

int g_cells = 0;
int g_failures = 0;

void report(const cells_context &c, const char *cell, bool ok, const std::string &detail = {}) {
    ++g_cells;
    if (!ok) {
        ++g_failures;
        std::fprintf(stderr, "draw2d_texture_parity_test: FAIL [srgb=%s] %s %s\n", c.mode, cell,
                     detail.c_str());
    }
}

pixel read_at(const cells_context &c, int x, int y) {
    unsigned char rgba[4] = {0, 0, 0, 0};
    c.gl.read_pixels(x, c.height - 1 - y, 1, 1, k_gl_rgba, k_gl_unsigned_byte, rgba);
    return pixel{rgba[0], rgba[1], rgba[2]};
}

std::string text_of(pixel p) {
    return "lido (" + std::to_string(p.red) + "," + std::to_string(p.green) + "," +
           std::to_string(p.blue) + ")";
}

bool is_exact(pixel p, int red, int green, int blue) {
    return p.red == red && p.green == green && p.blue == blue;
}

bool within(int got, double want, double tolerance) {
    return static_cast<double>(got) >= want - tolerance &&
           static_cast<double>(got) <= want + tolerance;
}

std::string code_name(const glintfx::gltfx_err &error) {
    return std::string(glintfx::gltfx_err_code_name(error.code()));
}

int fail(const char *what) {
    std::fprintf(stderr, "draw2d_texture_parity_test: FAIL %s\n", what);
    return EXIT_FAILURE;
}

// The sRGB encoding of IEC 61966-2-1, as a fraction (0 to 1): the expected values are computed with
// it here, never read from the library.
double srgb_encode(double linear) {
    return linear <= 0.0031308 ? 12.92 * linear : 1.055 * std::pow(linear, 1.0 / 2.4) - 0.055;
}

glintfx::gltfx_rect_world rect(double x, double y, double width, double height) {
    return glintfx::gltfx_rect_world{{x, y}, {width, height}};
}

// ---------------------------------------------------------------------------------------------------------
// Textures, made through the public create_texture() from straight sRGB bytes.
// ---------------------------------------------------------------------------------------------------------
using rgba = std::array<std::uint8_t, 4>;

constexpr rgba k_px_red{255, 0, 0, 255};
constexpr rgba k_px_green{0, 255, 0, 255};
constexpr rgba k_px_blue{0, 0, 255, 255};
constexpr rgba k_px_white{255, 255, 255, 255};
constexpr rgba k_px_black{0, 0, 0, 255};
constexpr rgba k_px_clear{0, 0, 0, 0};

std::optional<glintfx::gltfx_texture_2d> make_texture(const cells_context &c, std::uint32_t width,
                                                      std::uint32_t height,
                                                      const std::vector<rgba> &texels,
                                                      glintfx::gltfx_texture_filter filter) {
    std::vector<std::byte> bytes;
    bytes.reserve(texels.size() * 4);
    for (const rgba &texel : texels) {
        for (const std::uint8_t channel : texel) {
            bytes.push_back(static_cast<std::byte>(channel));
        }
    }
    const glintfx::gltfx_image_view image{.pixels = bytes,
                                          .width = width,
                                          .height = height,
                                          .bytes_per_row = static_cast<std::size_t>(width) * 4,
                                          .pixel_format =
                                              glintfx::gltfx_pixel_format::rgba8_srgb_straight};
    glintfx::gltfx_texture_2d_desc desc;
    desc.filter = filter;
    glintfx::gltfx_rslt<glintfx::gltfx_texture_2d> made = c.renderer.create_texture(image, desc);
    if (made.has_error()) {
        report(c, "create_texture da celula", false, code_name(made.err()));
        return std::nullopt;
    }
    return std::move(made.value());
}

glintfx::gltfx_texture_paint
whole(std::uint32_t width, std::uint32_t height,
      glintfx::gltfx_texture_mirror mirror = glintfx::gltfx_texture_mirror::none,
      glintfx::gltfx_rgba tint = {1.0F, 1.0F, 1.0F, 1.0F}) {
    return glintfx::gltfx_texture_paint{
        glintfx::gltfx_texture_region{
            glintfx::gltfx_rect_texel{{0.0, 0.0},
                                      {static_cast<double>(width), static_cast<double>(height)}},
            mirror},
        tint};
}

// One textured rectangle alone in a frame cleared to black; the frame's report.
std::optional<glintfx::gltfx_frame_2d_report> draw_alone(const cells_context &c,
                                                         const glintfx::gltfx_texture_2d &texture,
                                                         const glintfx::gltfx_texture_paint &paint,
                                                         const glintfx::gltfx_rect_world &where) {
    c.renderer.begin_frame(
        glintfx::gltfx_frame_2d_desc{.clear_color = glintfx::gltfx_rgba{0.0F, 0.0F, 0.0F, 1.0F}});
    c.renderer.draw_texture(texture, paint, where);
    const auto finished = c.renderer.finish_frame();
    c.gl.finish();
    if (finished.has_error()) {
        return std::nullopt;
    }
    return finished.value();
}

// ---------------------------------------------------------------------------------------------------------
// D-W8-305: the three filter cells.
// ---------------------------------------------------------------------------------------------------------
constexpr int k_filter_x0 = 60;
constexpr int k_filter_y0 = 40;

// The weight of the RIGHT texel at the pixel x0 + k (see the header).
double right_weight(int k) { return (static_cast<double>(k) + 0.5) / 100.0 - 0.5; }

void cell_filter_nearest_edge(const cells_context &c) {
    const auto texture =
        make_texture(c, 2, 1, {k_px_red, k_px_blue}, glintfx::gltfx_texture_filter::nearest);
    if (!texture.has_value()) {
        return;
    }
    const auto drawn = draw_alone(c, *texture, whole(2, 1), rect(k_filter_x0, k_filter_y0, 200, 4));
    report(c, "F1 filter_nearest_edge: o quadro termina com valor", drawn.has_value());
    const pixel left = read_at(c, k_filter_x0 + 99, k_filter_y0 + 1);
    const pixel right = read_at(c, k_filter_x0 + 100, k_filter_y0 + 1);
    report(c, "F1 filter_nearest_edge: k = 99 vermelho exato", is_exact(left, 255, 0, 0),
           text_of(left));
    report(c, "F1 filter_nearest_edge: k = 100 azul exato", is_exact(right, 0, 0, 255),
           text_of(right));
}

void cell_filter_linear_srgb_order(const cells_context &c) {
    const auto texture =
        make_texture(c, 2, 1, {k_px_black, k_px_white}, glintfx::gltfx_texture_filter::linear);
    if (!texture.has_value()) {
        return;
    }
    const auto drawn = draw_alone(c, *texture, whole(2, 1), rect(k_filter_x0, k_filter_y0, 200, 4));
    report(c, "F2 filter_linear_srgb_order: o quadro termina com valor", drawn.has_value());
    const char *driver = reinterpret_cast<const char *>(c.gl.get_string(k_gl_renderer));
    const std::string named =
        std::string("(o driver converte sRGB depois do filtro? GL_RENDERER: ") +
        (driver != nullptr ? driver : "?") + ")";
    for (const int k : {99, 100}) {
        const pixel got = read_at(c, k_filter_x0 + k, k_filter_y0 + 1);
        const double want = 255.0 * srgb_encode(right_weight(k));
        const bool ok = within(got.red, want, 2.0) && within(got.green, want, 2.0) &&
                        within(got.blue, want, 2.0);
        const std::string what = "F2 filter_linear_srgb_order em k = " + std::to_string(k);
        report(c, what.c_str(), ok,
               text_of(got) + " esperado " + std::to_string(want) + " +-2 " + named);
        if (k == 100) {
            std::fprintf(stdout,
                         "MEASURED draw2d_texture_parity_test.srgb_filter_midpoint_srgb_%s=%d\n",
                         c.mode, got.red);
        }
    }
}

void cell_filter_linear_no_dark_halo(const cells_context &c) {
    const auto texture =
        make_texture(c, 2, 1, {k_px_red, k_px_clear}, glintfx::gltfx_texture_filter::linear);
    if (!texture.has_value()) {
        return;
    }
    const auto drawn = draw_alone(c, *texture, whole(2, 1), rect(k_filter_x0, k_filter_y0, 200, 4));
    report(c, "F3 filter_linear_no_dark_halo: o quadro termina com valor", drawn.has_value());
    const double red_weight = 1.0 - right_weight(100);
    const double want = c.srgb ? 255.0 * srgb_encode(red_weight) : 255.0 * red_weight;
    const pixel got = read_at(c, k_filter_x0 + 100, k_filter_y0 + 1);
    report(c, "F3 filter_linear_no_dark_halo: vermelho em k = 100", within(got.red, want, 2.0),
           text_of(got) + " esperado " + std::to_string(want) + " +-2");
    report(c, "F3 filter_linear_no_dark_halo: verde e azul exatamente 0",
           got.green == 0 && got.blue == 0, text_of(got));
}

// ---------------------------------------------------------------------------------------------------------
// D-W8-463, D-W8-476: the checkered trapezoid, against the homography of Heckbert (1989).
// ---------------------------------------------------------------------------------------------------------
struct point {
    double x = 0.0;
    double y = 0.0;
};

constexpr std::array<point, 4> k_trapezoid{
    {{20.0, 20.0}, {300.0, 20.0}, {200.0, 220.0}, {120.0, 220.0}}};
constexpr int k_checker = 8;
constexpr double k_margin = 0.25;    // texels from the edge of a square
constexpr int k_grid = 8;            // pixels between two probes
constexpr int k_discriminating = 10; // the floor of probes the affine drawing would get wrong

// The square-to-quadrilateral homography, corner 0 at (0,0), 1 at (1,0), 2 at (1,1), 3 at (0,1),
// as the 3 x 3 matrix [a b c; d e f; g h 1]; and its inverse, by the adjugate.
using matrix3 = std::array<std::array<double, 3>, 3>;

matrix3 square_to_quad(const std::array<point, 4> &p) {
    const double dx1 = p[1].x - p[2].x;
    const double dx2 = p[3].x - p[2].x;
    const double dx3 = p[0].x - p[1].x + p[2].x - p[3].x;
    const double dy1 = p[1].y - p[2].y;
    const double dy2 = p[3].y - p[2].y;
    const double dy3 = p[0].y - p[1].y + p[2].y - p[3].y;
    const double den = dx1 * dy2 - dx2 * dy1;
    const double g = (dx3 * dy2 - dx2 * dy3) / den;
    const double h = (dx1 * dy3 - dx3 * dy1) / den;
    return matrix3{{{p[1].x - p[0].x + g * p[1].x, p[3].x - p[0].x + h * p[3].x, p[0].x},
                    {p[1].y - p[0].y + g * p[1].y, p[3].y - p[0].y + h * p[3].y, p[0].y},
                    {g, h, 1.0}}};
}

matrix3 adjugate(const matrix3 &m) {
    return matrix3{{{m[1][1] * m[2][2] - m[1][2] * m[2][1], m[0][2] * m[2][1] - m[0][1] * m[2][2],
                     m[0][1] * m[1][2] - m[0][2] * m[1][1]},
                    {m[1][2] * m[2][0] - m[1][0] * m[2][2], m[0][0] * m[2][2] - m[0][2] * m[2][0],
                     m[0][2] * m[1][0] - m[0][0] * m[1][2]},
                    {m[1][0] * m[2][1] - m[1][1] * m[2][0], m[0][1] * m[2][0] - m[0][0] * m[2][1],
                     m[0][0] * m[1][1] - m[0][1] * m[1][0]}}};
}

point apply(const matrix3 &m, point p) {
    const double w = m[2][0] * p.x + m[2][1] * p.y + m[2][2];
    return point{(m[0][0] * p.x + m[0][1] * p.y + m[0][2]) / w,
                 (m[1][0] * p.x + m[1][1] * p.y + m[1][2]) / w};
}

// The texture coordinate of `p` as a program that ignores q draws it: linear over the triangle
// (0,1,2) or (0,2,3), the frozen diagonal. No value outside both.
std::optional<point> affine_reading(const std::array<point, 4> &q, point p) {
    constexpr std::array<point, 4> roles{{{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}}};
    constexpr std::array<std::array<int, 3>, 2> triangles{{{0, 1, 2}, {0, 2, 3}}};
    for (const auto &t : triangles) {
        const point a = q[static_cast<std::size_t>(t[0])];
        const point b = q[static_cast<std::size_t>(t[1])];
        const point d = q[static_cast<std::size_t>(t[2])];
        const double det = (b.y - d.y) * (a.x - d.x) + (d.x - b.x) * (a.y - d.y);
        const double l0 = ((b.y - d.y) * (p.x - d.x) + (d.x - b.x) * (p.y - d.y)) / det;
        const double l1 = ((d.y - a.y) * (p.x - d.x) + (a.x - d.x) * (p.y - d.y)) / det;
        const double l2 = 1.0 - l0 - l1;
        if (l0 >= 0.0 && l1 >= 0.0 && l2 >= 0.0) {
            const point ra = roles[static_cast<std::size_t>(t[0])];
            const point rb = roles[static_cast<std::size_t>(t[1])];
            const point rd = roles[static_cast<std::size_t>(t[2])];
            return point{l0 * ra.x + l1 * rb.x + l2 * rd.x, l0 * ra.y + l1 * rb.y + l2 * rd.y};
        }
    }
    return std::nullopt;
}

// The square a texture coordinate falls in, when it is at least k_margin texels inside it.
struct square {
    int column = 0;
    int row = 0;
};

std::optional<square> square_inside(point uv) {
    const double tu = uv.x * k_checker;
    const double tv = uv.y * k_checker;
    if (!(tu > 0.0 && tu < k_checker && tv > 0.0 && tv < k_checker)) {
        return std::nullopt;
    }
    const double column = std::floor(tu);
    const double row = std::floor(tv);
    const double edge =
        std::fmin(std::fmin(tu - column, column + 1.0 - tu), std::fmin(tv - row, row + 1.0 - tv));
    if (edge < k_margin) {
        return std::nullopt;
    }
    return square{static_cast<int>(column), static_cast<int>(row)};
}

bool is_white(square s) { return (s.column + s.row) % 2 == 0; }

std::vector<rgba> checkerboard() {
    std::vector<rgba> texels;
    for (int row = 0; row < k_checker; ++row) {
        for (int column = 0; column < k_checker; ++column) {
            texels.push_back(is_white(square{column, row}) ? k_px_white : k_px_black);
        }
    }
    return texels;
}

void cell_projective_trapezoid(const cells_context &c) {
    const auto texture = make_texture(c, k_checker, k_checker, checkerboard(),
                                      glintfx::gltfx_texture_filter::nearest);
    if (!texture.has_value()) {
        return;
    }
    c.renderer.begin_frame(
        glintfx::gltfx_frame_2d_desc{.clear_color = glintfx::gltfx_rgba{1.0F, 0.0F, 0.0F, 1.0F}});
    c.renderer.draw_texture_quad(*texture, whole(k_checker, k_checker),
                                 glintfx::gltfx_quad_world{{k_trapezoid[0].x, k_trapezoid[0].y},
                                                           {k_trapezoid[1].x, k_trapezoid[1].y},
                                                           {k_trapezoid[2].x, k_trapezoid[2].y},
                                                           {k_trapezoid[3].x, k_trapezoid[3].y}});
    const auto finished = c.renderer.finish_frame();
    c.gl.finish();
    report(c, "K1 trapezio: o quadro termina com valor", finished.has_value());
    const matrix3 inverse = adjugate(square_to_quad(k_trapezoid));
    int checked = 0;
    int wrong = 0;
    int discriminating = 0;
    for (int y = 0; y < 240; y += k_grid) {
        for (int x = 0; x < 320; x += k_grid) {
            const point centre{x + 0.5, y + 0.5};
            const std::optional<point> affine = affine_reading(k_trapezoid, centre);
            const std::optional<square> projective = square_inside(apply(inverse, centre));
            if (!affine.has_value() || !projective.has_value()) {
                continue;
            }
            ++checked;
            const int want = is_white(*projective) ? 255 : 0;
            if (!is_exact(read_at(c, x, y), want, want, want)) {
                ++wrong;
                std::fprintf(stderr,
                             "draw2d_texture_parity_test: [srgb=%s] K1 pixel (%d,%d) %s, "
                             "esperado %d\n",
                             c.mode, x, y, text_of(read_at(c, x, y)).c_str(), want);
            }
            const std::optional<square> affine_square = square_inside(*affine);
            if (affine_square.has_value() && is_white(*affine_square) != is_white(*projective)) {
                ++discriminating;
            }
        }
    }
    std::fprintf(stdout,
                 "draw2d_texture_parity_test: [srgb=%s] K1 trapezio sondas=%d erradas=%d "
                 "que_o_afim_erra=%d\n",
                 c.mode, checked, wrong, discriminating);
    report(c, "K1 trapezio: toda sonda tem a cor da homografia", checked > 0 && wrong == 0);
    report(c, "K1 trapezio: sondas que o desenho afim erraria >= 10",
           discriminating >= k_discriminating, std::to_string(discriminating));
}

// D-W8-463 (2), D-W8-477: the library's program, found among the live program objects by its
// uniform u_viewport_pixels, keeps the attribute a_texcoord_q at location 3.
void cell_attribute_of_q(const cells_context &c) {
    int programs = 0;
    int library = 0;
    gl_int location = -1;
    for (gl_uint name = 1; name <= 4096; ++name) {
        if (c.gl.is_program(name) == 0) {
            continue;
        }
        ++programs;
        if (c.gl.get_uniform_location(name, "u_viewport_pixels") < 0) {
            continue;
        }
        ++library;
        location = c.gl.get_attrib_location(name, "a_texcoord_q");
    }
    std::fprintf(stdout,
                 "draw2d_texture_parity_test: [srgb=%s] K2 programas=%d da_biblioteca=%d "
                 "a_texcoord_q=%d\n",
                 c.mode, programs, library, static_cast<int>(location));
    report(c, "K2 um programa da biblioteca vivo no contexto", library == 1,
           std::to_string(library));
    report(c, "K2 a_texcoord_q chega ao fragmento: localizacao 3", location == 3,
           std::to_string(static_cast<int>(location)));
}

// ---------------------------------------------------------------------------------------------------------
// D-W8-13 and D-W8-292: the order of textures A, B, A, and the switches to and from no texture.
// ---------------------------------------------------------------------------------------------------------
void cell_order_a_b_a(const cells_context &c) {
    const auto red = make_texture(c, 1, 1, {k_px_red}, glintfx::gltfx_texture_filter::nearest);
    const auto blue = make_texture(c, 1, 1, {k_px_blue}, glintfx::gltfx_texture_filter::nearest);
    if (!red.has_value() || !blue.has_value()) {
        return;
    }
    c.renderer.begin_frame(
        glintfx::gltfx_frame_2d_desc{.clear_color = glintfx::gltfx_rgba{0.0F, 0.0F, 0.0F, 1.0F}});
    c.renderer.draw_texture(*red, whole(1, 1), rect(20, 150, 40, 40));
    c.renderer.draw_texture(*blue, whole(1, 1), rect(40, 160, 40, 40));
    c.renderer.draw_texture(*red, whole(1, 1), rect(60, 170, 40, 40));
    const auto finished = c.renderer.finish_frame();
    c.gl.finish();
    report(c, "O1 A, B, A: o quadro termina com valor", finished.has_value());
    const pixel first = read_at(c, 50, 165);
    const pixel second = read_at(c, 70, 180);
    report(c, "O1 a sobreposicao de A e B e B (azul)", is_exact(first, 0, 0, 255), text_of(first));
    report(c, "O1 a sobreposicao de B e do segundo A e A (vermelho)", is_exact(second, 255, 0, 0),
           text_of(second));
    if (finished.has_value()) {
        report(c, "O1 texture_switches = 3", finished.value().texture_switches == 3,
               std::to_string(finished.value().texture_switches));
    }
}

void cell_switches_to_and_from_none(const cells_context &c) {
    const auto red = make_texture(c, 1, 1, {k_px_red}, glintfx::gltfx_texture_filter::nearest);
    if (!red.has_value()) {
        return;
    }
    c.renderer.begin_frame(
        glintfx::gltfx_frame_2d_desc{.clear_color = glintfx::gltfx_rgba{0.0F, 0.0F, 0.0F, 1.0F}});
    c.renderer.fill_rect(rect(20, 150, 20, 20), glintfx::gltfx_rgba{0.0F, 1.0F, 0.0F, 1.0F});
    c.renderer.draw_texture(*red, whole(1, 1), rect(50, 150, 20, 20));
    c.renderer.fill_rect(rect(80, 150, 20, 20), glintfx::gltfx_rgba{0.0F, 0.0F, 1.0F, 1.0F});
    const auto finished = c.renderer.finish_frame();
    c.gl.finish();
    const pixel solid_after = read_at(c, 90, 160);
    report(c, "O2 a peca solida depois da texturizada sai solida (azul)",
           is_exact(solid_after, 0, 0, 255), text_of(solid_after));
    report(c, "O2 a peca texturizada entre as solidas sai vermelha",
           is_exact(read_at(c, 60, 160), 255, 0, 0));
    if (finished.has_value()) {
        report(c, "O2 texture_switches = 2", finished.value().texture_switches == 2,
               std::to_string(finished.value().texture_switches));
    } else {
        report(c, "O2 o quadro termina com valor", false);
    }
}

// ---------------------------------------------------------------------------------------------------------
// D-W8-297: each mirror puts each texel of a 2 x 2 texture in its quarter.
// ---------------------------------------------------------------------------------------------------------
void cell_mirrors(const cells_context &c) {
    const auto texture = make_texture(c, 2, 2, {k_px_red, k_px_green, k_px_blue, k_px_white},
                                      glintfx::gltfx_texture_filter::nearest);
    if (!texture.has_value()) {
        return;
    }
    struct mirror_case {
        glintfx::gltfx_texture_mirror mirror = glintfx::gltfx_texture_mirror::none;
        const char *name = "";
        // top-left, top-right, bottom-left, bottom-right quarters
        std::array<rgba, 4> quarters{};
    };
    const std::array<mirror_case, 4> cases{{
        {glintfx::gltfx_texture_mirror::none,
         "none",
         {k_px_red, k_px_green, k_px_blue, k_px_white}},
        {glintfx::gltfx_texture_mirror::flip_left_right,
         "flip_left_right",
         {k_px_green, k_px_red, k_px_white, k_px_blue}},
        {glintfx::gltfx_texture_mirror::flip_top_bottom,
         "flip_top_bottom",
         {k_px_blue, k_px_white, k_px_red, k_px_green}},
        {glintfx::gltfx_texture_mirror::flip_left_right_and_top_bottom,
         "flip_left_right_and_top_bottom",
         {k_px_white, k_px_blue, k_px_green, k_px_red}},
    }};
    constexpr std::array<std::array<int, 2>, 4> offsets{{{10, 10}, {30, 10}, {10, 30}, {30, 30}}};
    for (const mirror_case &m : cases) {
        const auto drawn = draw_alone(c, *texture, whole(2, 2, m.mirror), rect(100, 100, 40, 40));
        int right = 0;
        for (std::size_t q = 0; q < 4; ++q) {
            const pixel got = read_at(c, 100 + offsets[q][0], 100 + offsets[q][1]);
            right += is_exact(got, m.quarters[q][0], m.quarters[q][1], m.quarters[q][2]) ? 1 : 0;
        }
        const std::string what = std::string("M espelho ") + m.name + ": os quatro quartos";
        report(c, what.c_str(), drawn.has_value() && right == 4, std::to_string(right) + "/4");
    }
}

// The tint: an opaque white texel tinted half transparent white, over black.
void cell_tint(const cells_context &c) {
    const auto texture =
        make_texture(c, 1, 1, {k_px_white}, glintfx::gltfx_texture_filter::nearest);
    if (!texture.has_value()) {
        return;
    }
    const auto drawn = draw_alone(
        c, *texture, whole(1, 1, glintfx::gltfx_texture_mirror::none, {1.0F, 1.0F, 1.0F, 0.5F}),
        rect(200, 100, 20, 20));
    const pixel got = read_at(c, 210, 110);
    const double want = c.srgb ? 188.0 : 128.0;
    report(c, "TN tinta branca com alfa 0,5 sobre preto",
           drawn.has_value() && within(got.red, want, 2.0) && within(got.green, want, 2.0) &&
               within(got.blue, want, 2.0),
           text_of(got));
}

// ---------------------------------------------------------------------------------------------------------
// R2D-TEXTURE T5 (D-W8-678): textured triangles on a real surface.
// ---------------------------------------------------------------------------------------------------------
constexpr glintfx::gltfx_rgba k_opaque_white{1.0F, 1.0F, 1.0F, 1.0F};

// A vertex at (x, y) reading the texel (u, v), with the opaque white tint.
glintfx::gltfx_textured_vertex_2d textured_vertex(double x, double y, double u, double v) {
    return glintfx::gltfx_textured_vertex_2d{{x, y}, k_opaque_white, {u, v}};
}

// TT1: two indexed triangles covering a 40 x 40 square and reading the whole of a 2 x 2 texture put
// each texel in its quarter, as the rectangle without mirror does (cell M, "none"): texel (0, 0),
// the first texel of the first row, at the top left. The two triangles have OPPOSITE orientations
// on the screen (0, 1, 2 one way, 0, 3, 2 the other): neither is dropped for its orientation.
void cell_textured_triangles_quarters(const cells_context &c) {
    const auto texture = make_texture(c, 2, 2, {k_px_red, k_px_green, k_px_blue, k_px_white},
                                      glintfx::gltfx_texture_filter::nearest);
    if (!texture.has_value()) {
        return;
    }
    const std::array<glintfx::gltfx_textured_vertex_2d, 4> corners{
        textured_vertex(100, 100, 0, 0), textured_vertex(140, 100, 2, 0),
        textured_vertex(140, 140, 2, 2), textured_vertex(100, 140, 0, 2)};
    const std::array<std::uint32_t, 6> indices{0U, 1U, 2U, 0U, 3U, 2U};
    c.renderer.begin_frame(
        glintfx::gltfx_frame_2d_desc{.clear_color = glintfx::gltfx_rgba{0.0F, 0.0F, 0.0F, 1.0F}});
    c.renderer.fill_indexed_textured_triangles(*texture, corners, indices);
    const auto finished = c.renderer.finish_frame();
    c.gl.finish();
    constexpr std::array<std::array<int, 2>, 4> offsets{{{10, 10}, {30, 10}, {10, 30}, {30, 30}}};
    const std::array<rgba, 4> quarters{k_px_red, k_px_green, k_px_blue, k_px_white};
    int right = 0;
    for (std::size_t q = 0; q < 4; ++q) {
        const pixel got = read_at(c, 100 + offsets[q][0], 100 + offsets[q][1]);
        right += is_exact(got, quarters[q][0], quarters[q][1], quarters[q][2]) ? 1 : 0;
    }
    report(c, "TT1 triangulos indexados: cada texel no seu quarto",
           finished.has_value() && right == 4, std::to_string(right) + "/4");
}

// TT2: the tint of each vertex multiplies the texel: an opaque white texel under one triangle
// whose three vertices are white with alpha 0.5, over black, reads what the tint cell (TN) reads.
void cell_textured_triangles_tint(const cells_context &c) {
    const auto texture =
        make_texture(c, 1, 1, {k_px_white}, glintfx::gltfx_texture_filter::nearest);
    if (!texture.has_value()) {
        return;
    }
    std::array<glintfx::gltfx_textured_vertex_2d, 3> triangle{textured_vertex(200, 100, 0, 0),
                                                              textured_vertex(260, 100, 1, 0),
                                                              textured_vertex(200, 160, 0, 1)};
    for (glintfx::gltfx_textured_vertex_2d &vertex : triangle) {
        vertex.tint = glintfx::gltfx_rgba{1.0F, 1.0F, 1.0F, 0.5F};
    }
    c.renderer.begin_frame(
        glintfx::gltfx_frame_2d_desc{.clear_color = glintfx::gltfx_rgba{0.0F, 0.0F, 0.0F, 1.0F}});
    c.renderer.fill_textured_triangles(*texture, triangle);
    const auto finished = c.renderer.finish_frame();
    c.gl.finish();
    const pixel got = read_at(c, 210, 110);
    const double want = c.srgb ? 188.0 : 128.0;
    report(c, "TT2 tinta por vertice branca com alfa 0,5 sobre preto",
           finished.has_value() && within(got.red, want, 2.0) && within(got.green, want, 2.0) &&
               within(got.blue, want, 2.0),
           text_of(got));
}

// TT3: a sprite and a mesh of the same texture, one after the other, are ONE draw call and one
// switch; the mesh reads the texture (red), never the white of its vertices.
void cell_sprite_and_mesh_share_a_draw_call(const cells_context &c) {
    const auto red = make_texture(c, 1, 1, {k_px_red}, glintfx::gltfx_texture_filter::nearest);
    if (!red.has_value()) {
        return;
    }
    const std::array<glintfx::gltfx_textured_vertex_2d, 3> mesh{textured_vertex(60, 150, 0, 0),
                                                                textured_vertex(100, 150, 1, 0),
                                                                textured_vertex(60, 190, 0, 1)};
    c.renderer.begin_frame(
        glintfx::gltfx_frame_2d_desc{.clear_color = glintfx::gltfx_rgba{0.0F, 0.0F, 0.0F, 1.0F}});
    c.renderer.draw_texture(*red, whole(1, 1), rect(20, 150, 30, 30));
    c.renderer.fill_textured_triangles(*red, mesh);
    const auto finished = c.renderer.finish_frame();
    c.gl.finish();
    const pixel got = read_at(c, 65, 155);
    report(c, "TT3 a malha le a textura (vermelho)", is_exact(got, 255, 0, 0), text_of(got));
    const bool counted = finished.has_value() && finished.value().draw_calls == 1 &&
                         finished.value().texture_switches == 1;
    report(c, "TT3 sprite e malha da mesma textura: draw_calls = 1 e texture_switches = 1", counted,
           finished.has_value() ? std::to_string(finished.value().draw_calls) + " e " +
                                      std::to_string(finished.value().texture_switches)
                                : std::string("sem valor"));
}

// ---------------------------------------------------------------------------------------------------------
// One mode: a window, a context with srgb_framebuffer as asked, a renderer, and every cell.
// ---------------------------------------------------------------------------------------------------------
enum class mode_result { ran, absent, fatal };

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
        std::fprintf(stdout, "draw2d_texture_parity_test: AUSENCIA DECLARADA: "
                             "srgb_framebuffer=on nao e suportado aqui\n");
        result = mode_result::absent;
        return std::nullopt;
    }
    std::fprintf(stderr,
                 "draw2d_texture_parity_test: FAIL gltfx_gl_context::open(srgb=%d): %s "
                 "(rejected_value=%s)\n",
                 srgb ? 1 : 0, code_name(opened.err()).c_str(),
                 std::string(opened.err().rejected_value()).c_str());
    result = mode_result::fatal;
    return std::nullopt;
}

void run_cells(const cells_context &cells) {
    cell_filter_nearest_edge(cells);
    cell_filter_linear_srgb_order(cells);
    cell_filter_linear_no_dark_halo(cells);
    cell_projective_trapezoid(cells);
    cell_attribute_of_q(cells);
    cell_order_a_b_a(cells);
    cell_switches_to_and_from_none(cells);
    cell_mirrors(cells);
    cell_tint(cells);
    cell_textured_triangles_quarters(cells);
    cell_textured_triangles_tint(cells);
    cell_sprite_and_mesh_share_a_draw_call(cells);
}

mode_result run_mode(glintfx::gltfx_display &display, bool srgb) {
    const char *mode = srgb ? "on" : "off";
    const glintfx::gltfx_window_desc window_desc{
        .title = "janela de paridade do desenho com textura",
        .application_id = "org.glintfx.draw2d_texture_parity_test",
        .logical_size = {.width = 320, .height = 240},
    };
    glintfx::gltfx_rslt<glintfx::gltfx_window> window_opened =
        glintfx::gltfx_window::open(display, window_desc);
    if (window_opened.has_error()) {
        std::fprintf(stderr, "draw2d_texture_parity_test: FAIL gltfx_window::open(srgb=%s): %s\n",
                     mode, code_name(window_opened.err()).c_str());
        return mode_result::fatal;
    }
    glintfx::gltfx_window window = std::move(window_opened.value());
    mode_result result = mode_result::ran;
    std::optional<glintfx::gltfx_gl_context> context = open_context(window, srgb, result);
    if (!context.has_value()) {
        return result;
    }
    gl_api gl;
    if (context->make_current().has_error() || !load_gl(*context, gl)) {
        return mode_result::fatal;
    }
    glintfx::gltfx_rslt<glintfx::gltfx_renderer_2d> renderer_opened =
        glintfx::gltfx_renderer_2d::open(*context, glintfx::gltfx_renderer_2d_desc{});
    if (renderer_opened.has_error()) {
        std::fprintf(stderr, "draw2d_texture_parity_test: FAIL renderer_2d::open(srgb=%s): %s\n",
                     mode, code_name(renderer_opened.err()).c_str());
        return mode_result::fatal;
    }
    glintfx::gltfx_renderer_2d renderer = std::move(renderer_opened.value());
    renderer.begin_frame(glintfx::gltfx_frame_2d_desc{});
    (void)renderer.finish_frame();
    gl_int viewport[4] = {0, 0, 0, 0};
    gl.get_integerv(k_gl_viewport, viewport);
    if (viewport[2] < 320 || viewport[3] < 240) {
        std::fprintf(
            stderr, "draw2d_texture_parity_test: FAIL superficie menor que a geometria (%d x %d)\n",
            viewport[2], viewport[3]);
        return mode_result::fatal;
    }
    run_cells(cells_context{renderer, gl, srgb, viewport[3], mode});
    return mode_result::ran;
}

} // namespace

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    glintfx::gltfx_rslt<glintfx::gltfx_display> display_opened = glintfx::gltfx_display::open();
    if (display_opened.has_error()) {
        std::fprintf(stderr, "draw2d_texture_parity_test: gltfx_display::open() failed: %s\n",
                     code_name(display_opened.err()).c_str());
        return EXIT_FAILURE;
    }
    glintfx::gltfx_display display = std::move(display_opened.value());
    // The surface that does not encode (the option off, the default every driver has), then the
    // one that does (on), whose refusal by name is a declared absence.
    if (run_mode(display, false) != mode_result::ran) {
        return fail("o modo srgb_framebuffer=off (o padrao) tem de rodar em todo driver");
    }
    const mode_result on = run_mode(display, true);
    if (on == mode_result::fatal) {
        return fail(
            "o modo srgb_framebuffer=on falhou por outro motivo que nao a falta de suporte");
    }
    std::fprintf(stdout, "MEASURED draw2d_texture_parity_test.srgb_on_cells_absent=%d\n",
                 on == mode_result::absent ? 1 : 0);
    // Piso de varredura nao-vazia (GODS_LAWS.md L-40): a run that checked nothing is not a pass.
    std::fprintf(stdout, "draw2d_texture_parity_test: %d celula(s) conferida(s), %d reprovada(s)\n",
                 g_cells, g_failures);
    if (g_cells < 24) {
        return fail("menos celulas do que o piso (varredura quebrada)");
    }
    return g_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
