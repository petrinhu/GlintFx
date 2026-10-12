// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <span>
#include <string>
#include <utility>

#include <glintfx/core/color.hpp>
#include <glintfx/core/err.hpp>
#include <glintfx/core/vec2.hpp>
#include <glintfx/draw2d/frame_2d_report.hpp>
#include <glintfx/draw2d/renderer_2d.hpp>
#include <glintfx/draw2d/vertex_2d.hpp>
#include <glintfx/platform/gl/context.hpp>
#include <glintfx/platform/window/display.hpp>
#include <glintfx/platform/window/window.hpp>

// draw2d_shapes_parity_test.cpp - R2D-SHAPES, fatia S2b (docs/plano-fechamento-w8-r19.md, §3 S2b):
// the triangle methods of the 2D layer through the PUBLIC API only, READ BACK AS PIXELS from a real
// surface. ONE file, no #if, no harness: the Windows side links it into an add_executable() target
// (tests/CMakeLists.txt, if(WIN32)); the Linux side runs it as a container fixture of this exact
// name, inside a nested compositor (GODS_LAWS.md L-09), never on the live session.
//
// THE CRITERIA ARE FIXED HERE, BEFORE THE DATA EXISTS (GODS_LAWS.md L-43), and asserted on both
// systems:
//   - fill_triangles(): a triangle whose vertices are red, green and blue, read at the pixel
//   (20,20)
//     inside it: the three channels are above 0 and below 255 (the colour is interpolated and the
//     triangle was painted); the pixel (100,100), outside it, is the clear colour, black;
//   - the order of submission: a blue square submitted AFTER the triangle, over (20,20), leaves
//   that
//     pixel exactly blue;
//   - the refusals of one frame are counted per CALL (D-W8-287): five refused calls with five
//     reasons (the six-vertex call with one NaN counts once, not per triangle), one accepted
//     triangle, and two empty calls that are not pieces. The report reads 6 submitted, 1 drawn,
//     5 refused, first_refusal count_not_multiple_of_three, the counter of each reason, and the sum
//     of the seven per-reason counters equals pieces_refused;
//   - a call with no frame open is counted as dropped outside the frame in the NEXT report, and an
//     empty call outside the frame is not counted at all.
// Geometry assumes a scale factor of 1: pixels are the numbers written below (as in
// draw2d_parity_test.cpp). The surface is 320 by 240.

namespace {

using gl_enum = unsigned int;
using gl_int = int;
using gl_sizei = int;

constexpr gl_enum k_gl_viewport = 0x0BA2;
constexpr gl_enum k_gl_rgba = 0x1908;
constexpr gl_enum k_gl_unsigned_byte = 0x1401;

// The GL entry points the cells need, resolved through the PUBLIC gltfx_gl_context::proc_address().
struct gl_api {
    void (*get_integerv)(gl_enum, gl_int *) = nullptr;
    void (*read_pixels)(gl_int, gl_int, gl_sizei, gl_sizei, gl_enum, gl_enum, void *) = nullptr;
};

template <typename F> bool resolve(glintfx::gltfx_gl_context &context, const char *name, F &out) {
    void *address = context.proc_address(name);
    if (address == nullptr) {
        std::fprintf(stderr, "draw2d_shapes_parity_test: FAIL %s nao resolvido pelo contexto\n",
                     name);
        return false;
    }
    out = reinterpret_cast<F>(address);
    return true;
}

bool load_gl(glintfx::gltfx_gl_context &context, gl_api &gl) {
    return resolve(context, "glGetIntegerv", gl.get_integerv) &&
           resolve(context, "glReadPixels", gl.read_pixels);
}

struct pixel {
    int red = 0;
    int green = 0;
    int blue = 0;
};

// Everything a cell needs: the renderer under test, the GL entry points and the surface height
// (glReadPixels counts rows from the BOTTOM, the drawing from the top).
struct session {
    glintfx::gltfx_renderer_2d &renderer;
    const gl_api &gl;
    int height = 0;
};

int g_cells = 0;
int g_failures = 0;

void cell(bool ok, const char *name) {
    ++g_cells;
    if (!ok) {
        ++g_failures;
        std::fprintf(stderr, "draw2d_shapes_parity_test: FAIL %s\n", name);
    }
}

pixel read_at(const session &s, int x, int y) {
    unsigned char rgba[4] = {0, 0, 0, 0};
    s.gl.read_pixels(x, s.height - 1 - y, 1, 1, k_gl_rgba, k_gl_unsigned_byte, rgba);
    return pixel{rgba[0], rgba[1], rgba[2]};
}

bool is_exact(const pixel &got, int red, int green, int blue) {
    return got.red == red && got.green == green && got.blue == blue;
}

// Strictly inside the channel range, all three: the colour was interpolated, not left at zero.
bool is_interpolated(const pixel &got) {
    return got.red > 0 && got.green > 0 && got.blue > 0 && got.red < 255 && got.green < 255 &&
           got.blue < 255;
}

glintfx::gltfx_vertex_2d_world vertex(double x, double y, glintfx::gltfx_rgba color) {
    return glintfx::gltfx_vertex_2d_world{glintfx::gltfx_vec2_world{x, y}, color};
}

// The three corners of the triangle, one colour each, and its pixels of interest.
glintfx::gltfx_vertex_2d_world corner(int which) {
    constexpr glintfx::gltfx_rgba red{1.0F, 0.0F, 0.0F, 1.0F};
    constexpr glintfx::gltfx_rgba green{0.0F, 1.0F, 0.0F, 1.0F};
    constexpr glintfx::gltfx_rgba blue{0.0F, 0.0F, 1.0F, 1.0F};
    if (which == 0) {
        return vertex(10.0, 10.0, red);
    }
    if (which == 1) {
        return vertex(110.0, 10.0, green);
    }
    return vertex(10.0, 110.0, blue);
}

void draw_triangle(glintfx::gltfx_renderer_2d &renderer) {
    const glintfx::gltfx_vertex_2d_world triangle[3] = {corner(0), corner(1), corner(2)};
    renderer.fill_triangles(triangle, glintfx::gltfx_draw_layer{});
}

// C1: the triangle is painted, and a piece submitted AFTER it covers it on the same layer.
void cell_triangle_and_order(const session &s) {
    s.renderer.begin_frame(glintfx::gltfx_frame_2d_desc{});
    draw_triangle(s.renderer);
    s.renderer.fill_rect(glintfx::gltfx_rect_world{{15.0, 15.0}, {10.0, 10.0}},
                         glintfx::gltfx_rgba{0.0F, 0.0F, 1.0F, 1.0F});
    const auto finished = s.renderer.finish_frame();
    cell(finished.has_value(), "C1 finish_frame() com o triangulo e o quadrado tem valor");
    cell(is_exact(read_at(s, 20, 20), 0, 0, 255),
         "C1 o quadrado azul, enviado depois, cobre o triangulo em (20,20)");
    cell(is_exact(read_at(s, 100, 100), 0, 0, 0),
         "C1 o fundo fora do triangulo e a cor de limpeza, preto");
}

// C2: the triangle alone, painted with its colours interpolated, and one piece in the report.
void cell_triangle_alone(const session &s) {
    s.renderer.begin_frame(glintfx::gltfx_frame_2d_desc{});
    draw_triangle(s.renderer);
    const auto finished = s.renderer.finish_frame();
    cell(finished.has_value(), "C2 finish_frame() com so o triangulo tem valor");
    cell(is_interpolated(read_at(s, 20, 20)),
         "C2 o triangulo pinta (20,20) com as tres cores interpoladas (nenhuma a zero)");
    cell(is_exact(read_at(s, 100, 100), 0, 0, 0), "C2 (100,100) continua preto, fora do triangulo");
    if (finished.has_value()) {
        const glintfx::gltfx_frame_2d_report &report = finished.value();
        cell(report.pieces_submitted == 1 && report.pieces_drawn == 1 && report.pieces_refused == 0,
             "C2 o relatorio conta 1 peca enviada, 1 desenhada e nenhuma recusada");
    }
}

// C3: five refusals with five reasons, one accepted triangle, two empty calls. The counters add up.
void cell_refusals(const session &s) {
    const glintfx::gltfx_vertex_2d_world quad[4] = {
        vertex(0.0, 0.0, {1.0F, 1.0F, 1.0F, 1.0F}), vertex(10.0, 0.0, {1.0F, 1.0F, 1.0F, 1.0F}),
        vertex(10.0, 10.0, {1.0F, 1.0F, 1.0F, 1.0F}), vertex(0.0, 10.0, {1.0F, 1.0F, 1.0F, 1.0F})};
    const double not_a_number = std::numeric_limits<double>::quiet_NaN();
    const glintfx::gltfx_vertex_2d_world hexagon[6] = {
        corner(0), corner(1),
        corner(2), corner(0),
        corner(1), vertex(not_a_number, 0.0, glintfx::gltfx_rgba{1.0F, 1.0F, 1.0F, 1.0F})};
    const float infinity = std::numeric_limits<float>::infinity();
    const glintfx::gltfx_vertex_2d_world infinite_colour[3] = {
        corner(0), corner(1), vertex(0.0, 110.0, glintfx::gltfx_rgba{1.0F, infinity, 1.0F, 1.0F})};
    const std::uint32_t out_of_range[3] = {0U, 1U, 4U};
    const std::uint32_t two_indices[2] = {0U, 1U};

    s.renderer.begin_frame(glintfx::gltfx_frame_2d_desc{});
    s.renderer.fill_triangles(quad, glintfx::gltfx_draw_layer{});                       // R1
    s.renderer.fill_indexed_triangles(quad, out_of_range, glintfx::gltfx_draw_layer{}); // R2
    s.renderer.fill_triangles(hexagon, glintfx::gltfx_draw_layer{});                    // R3
    s.renderer.fill_triangles(infinite_colour, glintfx::gltfx_draw_layer{});            // R4
    s.renderer.fill_indexed_triangles(infinite_colour, two_indices,
                                      glintfx::gltfx_draw_layer{}); // R5
    draw_triangle(s.renderer);                                      // accepted
    s.renderer.fill_triangles(std::span<const glintfx::gltfx_vertex_2d_world>{},
                              glintfx::gltfx_draw_layer{}); // empty
    // Empty by its indices: three vertices and no index (the table's cell 11), not a refused call.
    const glintfx::gltfx_vertex_2d_world three[3] = {corner(0), corner(1), corner(2)};
    s.renderer.fill_indexed_triangles(three, std::span<const std::uint32_t>{},
                                      glintfx::gltfx_draw_layer{}); // empty
    const auto finished = s.renderer.finish_frame();
    if (!finished.has_value()) {
        cell(false, "C3 finish_frame() com as recusas tem valor");
        return;
    }
    const glintfx::gltfx_frame_2d_report &report = finished.value();
    cell(report.pieces_submitted == 6 && report.pieces_drawn == 1 && report.pieces_refused == 5,
         "C3 o relatorio conta 6 enviadas, 1 desenhada e 5 recusadas (uma por chamada)");
    cell(report.first_refusal == glintfx::gltfx_draw_2d_refusal::count_not_multiple_of_three,
         "C3 a primeira recusa e count_not_multiple_of_three");
    cell(report.pieces_refused_count_not_multiple_of_three == 2 &&
             report.pieces_refused_index_out_of_range == 1 &&
             report.pieces_refused_not_a_number == 1 && report.pieces_refused_infinite == 1,
         "C3 cada motivo recusado aparece no seu contador");
    const std::uint64_t reasons =
        report.pieces_refused_not_a_number + report.pieces_refused_infinite +
        report.pieces_refused_negative_size + report.pieces_refused_index_out_of_range +
        report.pieces_refused_count_not_multiple_of_three + report.pieces_refused_unknown_option +
        report.pieces_refused_too_many_vertices;
    cell(reasons == report.pieces_refused, "C3 a soma dos sete motivos e igual a pieces_refused");
}

// C4: a call with no frame open is counted in the next report; an empty call is not counted at all.
void cell_outside_frame(const session &s) {
    draw_triangle(s.renderer);
    s.renderer.fill_triangles(std::span<const glintfx::gltfx_vertex_2d_world>{},
                              glintfx::gltfx_draw_layer{});
    s.renderer.begin_frame(glintfx::gltfx_frame_2d_desc{});
    const auto finished = s.renderer.finish_frame();
    cell(finished.has_value() && finished.value().pieces_dropped_outside_frame == 1,
         "C4 a chamada sem quadro aberto conta 1 fora do quadro, e a vazia nao conta");
}

} // namespace

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    glintfx::gltfx_rslt<glintfx::gltfx_display> display_opened = glintfx::gltfx_display::open();
    if (display_opened.has_error()) {
        std::fprintf(stderr, "draw2d_shapes_parity_test: gltfx_display::open() falhou: %s\n",
                     std::string(display_opened.err().rejected_value()).c_str());
        return EXIT_FAILURE;
    }
    glintfx::gltfx_display display = std::move(display_opened.value());
    const glintfx::gltfx_window_desc window_desc{
        .title = "janela de paridade das formas 2D",
        .application_id = "org.glintfx.draw2d_shapes_parity_test",
        .logical_size = {.width = 320, .height = 240},
    };
    glintfx::gltfx_rslt<glintfx::gltfx_window> window_opened =
        glintfx::gltfx_window::open(display, window_desc);
    if (window_opened.has_error()) {
        std::fprintf(stderr, "draw2d_shapes_parity_test: FAIL gltfx_window::open()\n");
        return EXIT_FAILURE;
    }
    glintfx::gltfx_window window = std::move(window_opened.value());
    glintfx::gltfx_rslt<glintfx::gltfx_gl_context> opened =
        glintfx::gltfx_gl_context::open(window, glintfx::gltfx_gl_context_desc{});
    if (opened.has_error()) {
        std::fprintf(stderr, "draw2d_shapes_parity_test: FAIL gltfx_gl_context::open(): %s\n",
                     std::string(opened.err().rejected_value()).c_str());
        return EXIT_FAILURE;
    }
    glintfx::gltfx_gl_context context = std::move(opened.value());
    if (const glintfx::gltfx_rslt<void> current = context.make_current(); current.has_error()) {
        std::fprintf(stderr, "draw2d_shapes_parity_test: FAIL make_current()\n");
        return EXIT_FAILURE;
    }
    gl_api gl;
    if (!load_gl(context, gl)) {
        return EXIT_FAILURE;
    }
    glintfx::gltfx_rslt<glintfx::gltfx_renderer_2d> renderer_opened =
        glintfx::gltfx_renderer_2d::open(context, glintfx::gltfx_renderer_2d_desc{});
    if (renderer_opened.has_error()) {
        std::fprintf(stderr, "draw2d_shapes_parity_test: FAIL gltfx_renderer_2d::open()\n");
        return EXIT_FAILURE;
    }
    glintfx::gltfx_renderer_2d renderer = std::move(renderer_opened.value());

    // The surface size: the viewport the layer leaves covers all of it (one frame, drawn empty).
    renderer.begin_frame(glintfx::gltfx_frame_2d_desc{});
    (void)renderer.finish_frame();
    gl_int viewport[4] = {0, 0, 0, 0};
    gl.get_integerv(k_gl_viewport, viewport);
    if (viewport[2] < 320 || viewport[3] < 240) {
        std::fprintf(stderr, "draw2d_shapes_parity_test: FAIL superficie menor que 320 x 240\n");
        return EXIT_FAILURE;
    }
    const session s{renderer, gl, viewport[3]};
    cell_triangle_and_order(s);
    cell_triangle_alone(s);
    cell_refusals(s);
    cell_outside_frame(s);

    // Piso de varredura nao-vazia (GODS_LAWS.md L-40): a run that checked nothing is not a pass.
    std::fprintf(stdout, "draw2d_shapes_parity_test: %d celula(s) conferida(s), %d reprovada(s)\n",
                 g_cells, g_failures);
    if (g_cells < 10) {
        std::fprintf(stderr, "draw2d_shapes_parity_test: menos celulas do que o piso\n");
        return EXIT_FAILURE;
    }
    return g_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
