// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/draw2d/renderer_2d.hpp>
#include <glintfx/platform/gl/context.hpp>
#include <glintfx/platform/window/display.hpp>
#include <glintfx/platform/window/window.hpp>

// draw2d_parity_test.cpp - R2D-BATCH, fatia B4 (docs/plano-w7d.md sec. 4.3, errata sec. 19 point
// 5): the 2D drawing layer through the PUBLIC API only, on both systems. ONE file, no #if, no
// harness, the shape of gl_context_parity_test.cpp: the Windows side links it into an
// add_executable() target (tests/CMakeLists.txt, if(WIN32)); the Linux side runs it as a container
// fixture of this exact name, inside a nested compositor (GODS_LAWS.md L-09), never on the live
// session.
//
// WRITTEN BEFORE THE FACADE EXISTED (GODS_LAWS.md L-20): the link of this file was seen RED without
// renderer_2d_facade.cpp (undefined references to gltfx_renderer_2d), and green with it. What it
// proves on a real context is B5's reading of the verde, in the container and on Windows; B4 only
// delivers it written, compiled and registered.
//
// ASSERTED EQUAL on both systems (a genuine FAIL and EXIT_FAILURE the moment one disagrees):
//   - open() refuses a context that is not open, as invalid_argument naming "context";
//   - one frame of three overlapping rectangles is READ BACK AS PIXELS: the piece on the higher
//   layer
//     is on top although it was submitted FIRST, the lower layer shows where the higher one is
//     absent, and the cleared surface shows outside every piece;
//   - the report of that frame counts what happened: 3 submitted, 3 drawn, none refused, none
//     dropped, none not drawn;
//   - finish_frame() with no frame open is invalid_argument naming "frame".
// MEASURED, PRINTED, NEVER ASSERTED EQUAL: the number of draw calls of that frame (printed for you
// to measure, never promised - frame_2d_report.hpp).

namespace {

using gl_enum = unsigned int;
using gl_int = int;
using gl_sizei = int;
using gl_get_integerv_fn = void (*)(gl_enum, gl_int *);
using gl_read_pixels_fn = void (*)(gl_int, gl_int, gl_sizei, gl_sizei, gl_enum, gl_enum, void *);

constexpr gl_enum k_gl_viewport = 0x0BA2;
constexpr gl_enum k_gl_rgba = 0x1908;
constexpr gl_enum k_gl_unsigned_byte = 0x1401;

struct pixel {
    unsigned char red = 0;
    unsigned char green = 0;
    unsigned char blue = 0;
};

int fail(const char *what) {
    std::fprintf(stderr, "draw2d_parity_test: FAIL %s\n", what);
    return EXIT_FAILURE;
}

std::string code_name(const glintfx::gltfx_err &error) {
    return std::string(glintfx::gltfx_err_code_name(error.code()));
}

// The pixel that a piece at (x, y) in the top-left-origin frame covers, read from the
// bottom-left-origin buffer: the surface is `height` pixels tall.
pixel read_at(gl_read_pixels_fn read_pixels, int x, int y, int height) {
    unsigned char rgba[4] = {0, 0, 0, 0};
    read_pixels(x, height - 1 - y, 1, 1, k_gl_rgba, k_gl_unsigned_byte, rgba);
    return pixel{rgba[0], rgba[1], rgba[2]};
}

bool is(pixel got, unsigned char red, unsigned char green, unsigned char blue) {
    return got.red == red && got.green == green && got.blue == blue;
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

    const glintfx::gltfx_window_desc window_desc{
        .title = "janela de paridade do desenho 2D",
        .application_id = "org.glintfx.draw2d_parity_test",
        .logical_size = {.width = 320, .height = 240},
    };
    glintfx::gltfx_rslt<glintfx::gltfx_window> window_opened =
        glintfx::gltfx_window::open(display, window_desc);
    if (window_opened.has_error()) {
        std::fprintf(stderr, "draw2d_parity_test: gltfx_window::open() failed: %s\n",
                     code_name(window_opened.err()).c_str());
        return EXIT_FAILURE;
    }
    glintfx::gltfx_window window = std::move(window_opened.value());

    const glintfx::gltfx_gl_context_desc context_desc{};
    glintfx::gltfx_rslt<glintfx::gltfx_gl_context> context_opened =
        glintfx::gltfx_gl_context::open(window, context_desc);
    if (context_opened.has_error()) {
        std::fprintf(stderr, "draw2d_parity_test: gltfx_gl_context::open() failed: %s\n",
                     code_name(context_opened.err()).c_str());
        return EXIT_FAILURE;
    }
    glintfx::gltfx_gl_context context = std::move(context_opened.value());
    if (const glintfx::gltfx_rslt<void> current = context.make_current(); current.has_error()) {
        std::fprintf(stderr, "draw2d_parity_test: make_current() failed: %s\n",
                     code_name(current.err()).c_str());
        return EXIT_FAILURE;
    }

    // Cell 1: a context that is not open is refused by name. A moved-from handle is not open.
    {
        glintfx::gltfx_gl_context spare = std::move(context);
        const glintfx::gltfx_renderer_2d_desc desc{};
        glintfx::gltfx_rslt<glintfx::gltfx_renderer_2d> refused =
            glintfx::gltfx_renderer_2d::open(context, desc);
        if (refused.has_value() ||
            refused.err().code() != glintfx::gltfx_err_code::invalid_argument ||
            refused.err().rejected_value() != "context") {
            return fail("open() over a closed context must be invalid_argument \"context\"");
        }
        std::fprintf(stdout, "draw2d_parity_test: contexto fechado recusado por nome\n");
        context = std::move(spare);
    }

    glintfx::gltfx_rslt<glintfx::gltfx_renderer_2d> renderer_opened =
        glintfx::gltfx_renderer_2d::open(context, glintfx::gltfx_renderer_2d_desc{});
    if (renderer_opened.has_error()) {
        std::fprintf(stderr,
                     "draw2d_parity_test: FAIL gltfx_renderer_2d::open(): %s (rejected_value=%s)\n",
                     code_name(renderer_opened.err()).c_str(),
                     std::string(renderer_opened.err().rejected_value()).c_str());
        return EXIT_FAILURE;
    }
    glintfx::gltfx_renderer_2d renderer = std::move(renderer_opened.value());

    // Cell 2: finish_frame() with no frame open.
    {
        glintfx::gltfx_rslt<glintfx::gltfx_frame_2d_report> none = renderer.finish_frame();
        if (none.has_value() || none.err().code() != glintfx::gltfx_err_code::invalid_argument ||
            none.err().rejected_value() != "frame") {
            return fail("finish_frame() with no frame must be invalid_argument \"frame\"");
        }
    }

    // Cell 3: one frame, read back as pixels.
    //   blue  layer 1, submitted FIRST : x 60..160, y 40..120
    //   red   layer 0, submitted second: x 20..120, y 20..100  (overlaps the blue on x 60..120,
    //   y 40..100) green layer 0, submitted third : x 200..260, y 150..210 (alone)
    const glintfx::gltfx_rgba blue{0.0F, 0.0F, 1.0F, 1.0F};
    const glintfx::gltfx_rgba red{1.0F, 0.0F, 0.0F, 1.0F};
    const glintfx::gltfx_rgba green{0.0F, 1.0F, 0.0F, 1.0F};
    renderer.begin_frame(glintfx::gltfx_frame_2d_desc{});
    renderer.fill_rect({{60.0, 40.0}, {100.0, 80.0}}, blue, glintfx::gltfx_draw_layer{1});
    renderer.fill_rect({{20.0, 20.0}, {100.0, 80.0}}, red, glintfx::gltfx_draw_layer{0});
    renderer.fill_rect({{200.0, 150.0}, {60.0, 60.0}}, green, glintfx::gltfx_draw_layer{0});
    glintfx::gltfx_rslt<glintfx::gltfx_frame_2d_report> finished = renderer.finish_frame();
    if (finished.has_error()) {
        std::fprintf(stderr, "draw2d_parity_test: FAIL finish_frame(): %s (rejected_value=%s)\n",
                     code_name(finished.err()).c_str(),
                     std::string(finished.err().rejected_value()).c_str());
        return EXIT_FAILURE;
    }
    const glintfx::gltfx_frame_2d_report report = finished.value();
    if (report.pieces_submitted != 3 || report.pieces_drawn != 3 || report.pieces_refused != 0 ||
        report.pieces_dropped_out_of_memory != 0 || report.pieces_dropped_graphics_failure != 0) {
        std::fprintf(stderr,
                     "draw2d_parity_test: FAIL o relatorio do quadro (submitted=%llu drawn=%llu "
                     "refused=%llu "
                     "dropped=%llu dropped_graphics_failure=%llu)\n",
                     static_cast<unsigned long long>(report.pieces_submitted),
                     static_cast<unsigned long long>(report.pieces_drawn),
                     static_cast<unsigned long long>(report.pieces_refused),
                     static_cast<unsigned long long>(report.pieces_dropped_out_of_memory),
                     static_cast<unsigned long long>(report.pieces_dropped_graphics_failure));
        return EXIT_FAILURE;
    }

    void *get_integerv_address = context.proc_address("glGetIntegerv");
    void *read_pixels_address = context.proc_address("glReadPixels");
    if (get_integerv_address == nullptr || read_pixels_address == nullptr) {
        std::fprintf(stderr,
                     "draw2d_parity_test: FAIL glGetIntegerv/glReadPixels nao resolvidos\n");
        return EXIT_FAILURE;
    }
    const auto get_integerv = reinterpret_cast<gl_get_integerv_fn>(get_integerv_address);
    const auto read_pixels = reinterpret_cast<gl_read_pixels_fn>(read_pixels_address);
    gl_int viewport[4] = {0, 0, 0, 0};
    get_integerv(k_gl_viewport, viewport); // left at the whole surface by the state contract
    const int height = viewport[3];
    if (viewport[2] < 260 || height < 210) {
        std::fprintf(stderr, "draw2d_parity_test: FAIL superficie menor que o desenho (%d x %d)\n",
                     viewport[2], height);
        return EXIT_FAILURE;
    }
    // The surface may be larger than the logical size when a scale factor applies; the drawing is
    // in physical pixels, so the numbers above hold at scale 1 (both fixtures run at scale 1).
    const bool blue_on_top = is(read_at(read_pixels, 90, 70, height), 0, 0, 255);
    const bool red_alone = is(read_at(read_pixels, 30, 30, height), 255, 0, 0);
    const bool blue_alone = is(read_at(read_pixels, 140, 110, height), 0, 0, 255);
    const bool green_alone = is(read_at(read_pixels, 230, 180, height), 0, 255, 0);
    const bool cleared = is(read_at(read_pixels, 300, 20, height), 0, 0, 0);
    if (!blue_on_top || !red_alone || !blue_alone || !green_alone || !cleared) {
        std::fprintf(
            stderr,
            "draw2d_parity_test: FAIL pixels (azul sobre o vermelho=%d vermelho so=%d azul so=%d "
            "verde=%d fundo limpo=%d)\n",
            blue_on_top, red_alone, blue_alone, green_alone, cleared);
        return EXIT_FAILURE;
    }
    std::fprintf(stdout,
                 "draw2d_parity_test: cinco pixels conferidos (a camada mais alta por cima, mesmo "
                 "submetida primeiro)\n");
    std::fprintf(stdout, "MEASURED draw2d_parity_test.draw_calls=%llu\n",
                 static_cast<unsigned long long>(report.draw_calls));
    return EXIT_SUCCESS;
}
