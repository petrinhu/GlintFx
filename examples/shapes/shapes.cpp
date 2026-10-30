// SPDX-License-Identifier: AGPL-3.0-or-later
// shapes.cpp - the GlintFx example of the 2D shapes (R2D-SHAPES): every shape the 2D renderer
// fills or outlines with a plain colour, side by side, in one fixed scene drawn in every frame.
// Drawing with a texture (draw_texture, draw_texture_quad, fill_textured_triangles and
// fill_indexed_textured_triangles) is not in this example: it needs a texture made first, with
// create_texture(). A living proof, not a product: it uses only the public API, and nothing here
// becomes a rule of the library. Read examples/first_window/ first: it explains the window, the
// context and the loop, which this example repeats with less comment.
//
// THE SCENE, in pixels of the buffer (origin at the top-left corner, y grows DOWN; a rectangle is
// its corner and its size). The shapes are on layer 0 and none covers another; the black path
// markers (M) are on layer 1, over them. The background is the colour the first renderer clears
// the frame with, (0.2, 0.2, 0.2).
// Colours are linear light, alpha 1 unless said.
//
//   Row 1, filled shapes:
//     A  fill_rect: red, (20, 20, 120, 120).
//     B  fill_quad: green, a diamond (240, 20), (300, 80), (240, 140), (180, 80).
//     C  fill_triangles: one triangle, a colour per corner, red (340, 140), green (400, 20) and
//        blue (460, 140); the colours blend across it.
//     D  fill_indexed_triangles: a yellow hexagon, centre (560, 80), corners 60 away; 7 vertices
//        and 18 indices, the centre shared by the six triangles.
//   Row 2, outlines 10 thick of the same 100 by 100 square, three ways:
//     E  stroke_rect, centered: white at alpha 0.5, square at (30, 190); half the thickness on
//        each side of the edge, and no corner darker than a side.
//     F  stroke_rect, inside: magenta, square at (190, 190); the whole thickness inside.
//     G  stroke_rect, outside: cyan, square at (350, 190); the whole thickness outside.
//     H  stroke_ellipse: blue, centre (560, 240), radii 55 and 30, turned by 30 degrees. A
//        positive angle turns from +x toward +y; y grows down, so the long axis goes down to the
//        right: clockwise on the screen.
//   Row 3:
//     I  stroke_line, 28 thick: white, from x 30 to x 110, one line with each cap. Flat at y 350
//        (it ends where the line ends), square at y 400 (half the thickness past each end),
//        rounded at y 450 (a half disc on each end).
//     J  the unit of a thickness: yellow, in a batch zoomed 2 times. A thickness of 4 in WORLD
//        unit is 8 pixels (the line at y 370); 4 in PIXEL unit stays 4 (the line at y 420).
//     K  fill_ellipse: a green circle, centre (400, 400), radius 55, at the default curve
//        distance (0.25 pixel); it looks round.
//     L  the curve distance: the same circle at (560, 400), drawn by a SECOND renderer opened with
//        a curve distance of 12 pixels; it comes out with five straight sides, a pentagon.
//   Over rows 2 and 3:
//     M  the path you asked for: black, 2 pixels wide in pixel unit, on layer 1, over E, F and G
//        (the square each outline was given) and over I (the line from x 30 to x 110). It shows
//        where each outline grows and how far each cap goes past the end.
//
// The CI reads pixels of this scene: tests/fixtures/shapes_probes.txt says the expected colour of
// each pixel, why, and what the probes prove and what they do not. SCALE 1 IS ASSUMED AND
// DECLARED: the window is 640x480 logical pixels and the scene is in physical pixels
// (renderer_2d.hpp), so a scaled desktop would move it. The capture drivers refuse a capture that
// is not 640x480.
//
// USAGE: shapes [--frames N]
//   no argument: runs until the system asks to close the window, and exits 0;
//   --frames N (1 to 1000000): stops by itself after N DRAWN frames, through the normal path of the
//   loop. The scene is the same in every frame.
// EXIT: 0 done; 1 a step of the library failed (the line says which) or a frame lost pieces; 2 bad
// usage.

#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <numbers>
#include <optional>
#include <string_view>
#include <system_error>

#include <glintfx/core/angle.hpp>
#include <glintfx/core/color.hpp>
#include <glintfx/core/ellipse.hpp>
#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/core/quad.hpp>
#include <glintfx/core/rect.hpp>
#include <glintfx/core/segment.hpp>
#include <glintfx/core/transform.hpp>
#include <glintfx/draw2d/draw_layer.hpp>
#include <glintfx/draw2d/frame_2d_desc.hpp>
#include <glintfx/draw2d/frame_2d_report.hpp>
#include <glintfx/draw2d/renderer_2d.hpp>
#include <glintfx/draw2d/renderer_2d_desc.hpp>
#include <glintfx/draw2d/stroke.hpp>
#include <glintfx/draw2d/vertex_2d.hpp>
#include <glintfx/platform/gl/context.hpp>
#include <glintfx/platform/loop/loop.hpp>
#include <glintfx/platform/loop/loop_bind.hpp>
#include <glintfx/platform/window/display.hpp>
#include <glintfx/platform/window/window.hpp>

namespace {

constexpr int k_exit_ok = 0;
constexpr int k_exit_failed = 1;
constexpr int k_exit_usage = 2;

constexpr glintfx::gltfx_window_desc k_window_desc{
    .title = "GlintFx example: shapes",
    .application_id = "org.glintfx.examples.shapes",
    .logical_size = {.width = 640, .height = 480},
};

constexpr glintfx::gltfx_rgba k_background{.red = 0.2F, .green = 0.2F, .blue = 0.2F, .alpha = 1.0F};
constexpr glintfx::gltfx_rgba k_red{.red = 1.0F, .green = 0.0F, .blue = 0.0F, .alpha = 1.0F};
constexpr glintfx::gltfx_rgba k_green{.red = 0.0F, .green = 1.0F, .blue = 0.0F, .alpha = 1.0F};
constexpr glintfx::gltfx_rgba k_blue{.red = 0.0F, .green = 0.0F, .blue = 1.0F, .alpha = 1.0F};
constexpr glintfx::gltfx_rgba k_yellow{.red = 1.0F, .green = 1.0F, .blue = 0.0F, .alpha = 1.0F};
constexpr glintfx::gltfx_rgba k_magenta{.red = 1.0F, .green = 0.0F, .blue = 1.0F, .alpha = 1.0F};
constexpr glintfx::gltfx_rgba k_cyan{.red = 0.0F, .green = 1.0F, .blue = 1.0F, .alpha = 1.0F};
constexpr glintfx::gltfx_rgba k_white{.red = 1.0F, .green = 1.0F, .blue = 1.0F, .alpha = 1.0F};
constexpr glintfx::gltfx_rgba k_half_white{.red = 1.0F, .green = 1.0F, .blue = 1.0F, .alpha = 0.5F};
constexpr glintfx::gltfx_rgba k_black{.red = 0.0F, .green = 0.0F, .blue = 0.0F, .alpha = 1.0F};

// The curve distance of the second renderer (L). For a circle of radius 55 it gives
// ceil(pi / acos(1 - 12 / 55)) = 5 straight sides.
constexpr double k_coarse_curve_pixels = 12.0;

// ---- The lesson: the shapes, one group per function ----

// Row 1. C: a list of vertices read three at a time. D: the six triangles of the hexagon share the
// centre (vertex 0), so the indices name each vertex once instead of repeating it.
void draw_filled_shapes(glintfx::gltfx_renderer_2d &renderer) {
    renderer.fill_rect({{20.0, 20.0}, {120.0, 120.0}}, k_red);
    renderer.fill_quad({{240.0, 20.0}, {300.0, 80.0}, {240.0, 140.0}, {180.0, 80.0}}, k_green);
    const std::array<glintfx::gltfx_vertex_2d_world, 3> triangle{{
        {{340.0, 140.0}, k_red},
        {{400.0, 20.0}, k_green},
        {{460.0, 140.0}, k_blue},
    }};
    renderer.fill_triangles(triangle);
    std::array<glintfx::gltfx_vertex_2d_world, 7> hexagon{};
    hexagon[0] = {{560.0, 80.0}, k_yellow};
    for (std::size_t corner = 0; corner < 6; ++corner) {
        const double angle = std::numbers::pi / 3.0 * static_cast<double>(corner);
        hexagon[corner + 1] = {{560.0 + 60.0 * std::cos(angle), 80.0 + 60.0 * std::sin(angle)},
                               k_yellow};
    }
    constexpr std::array<std::uint32_t, 18> indices{0, 1, 2, 0, 2, 3, 0, 3, 4,
                                                    0, 4, 5, 0, 5, 6, 0, 6, 1};
    renderer.fill_indexed_triangles(hexagon, indices);
}

// Row 2. The same square and the same thickness; only the alignment changes. E is half transparent
// to show that the four sides meet at the corners without covering a pixel twice.
void draw_outlines(glintfx::gltfx_renderer_2d &renderer) {
    const glintfx::gltfx_stroke ten{.thickness = 10.0, .unit = glintfx::gltfx_stroke_unit::world};
    renderer.stroke_rect({{30.0, 190.0}, {100.0, 100.0}},
                         {.stroke = ten, .align = glintfx::gltfx_stroke_align::centered},
                         k_half_white);
    renderer.stroke_rect({{190.0, 190.0}, {100.0, 100.0}},
                         {.stroke = ten, .align = glintfx::gltfx_stroke_align::inside}, k_magenta);
    renderer.stroke_rect({{350.0, 190.0}, {100.0, 100.0}},
                         {.stroke = ten, .align = glintfx::gltfx_stroke_align::outside}, k_cyan);
    const glintfx::gltfx_ellipse_world turned{.center = {560.0, 240.0},
                                              .radius_x = 55.0,
                                              .radius_y = 30.0,
                                              .rotation = glintfx::gltfx_angle_from_degrees(30.0)};
    renderer.stroke_ellipse(turned, ten, k_blue);
}

// Row 3, I: one line, three caps. The cap decides what happens past each end of the line.
void draw_caps(glintfx::gltfx_renderer_2d &renderer) {
    const glintfx::gltfx_stroke thick{.thickness = 28.0, .unit = glintfx::gltfx_stroke_unit::world};
    renderer.stroke_line({{30.0, 350.0}, {110.0, 350.0}},
                         {.stroke = thick, .cap = glintfx::gltfx_line_cap::flat}, k_white);
    renderer.stroke_line({{30.0, 400.0}, {110.0, 400.0}},
                         {.stroke = thick, .cap = glintfx::gltfx_line_cap::square}, k_white);
    renderer.stroke_line({{30.0, 450.0}, {110.0, 450.0}},
                         {.stroke = thick, .cap = glintfx::gltfx_line_cap::rounded}, k_white);
}

// M: the path each outline and each line was given, 2 pixels wide, on layer 1, so it is painted
// over the shapes whatever the order of sending. Without it the alignment and the caps are hard to
// see.
void draw_path_markers(glintfx::gltfx_renderer_2d &renderer) {
    constexpr glintfx::gltfx_draw_layer on_top{1};
    const glintfx::gltfx_stroke two{.thickness = 2.0, .unit = glintfx::gltfx_stroke_unit::pixel};
    for (const double x : {30.0, 190.0, 350.0}) {
        renderer.stroke_rect({{x, 190.0}, {100.0, 100.0}}, {.stroke = two}, k_black, on_top);
    }
    for (const double y : {350.0, 400.0, 450.0}) {
        renderer.stroke_line({{30.0, y}, {110.0, y}}, {.stroke = two}, k_black, on_top);
    }
}

// Row 3, J: a batch whose transform zooms 2 times. Positions are in the world from here on: (0, 20)
// lands on pixel (190, 370). A thickness in world unit zooms with the batch; in pixel unit it does
// not. begin_batch() with no transform puts positions back in pixels.
void draw_stroke_units(glintfx::gltfx_renderer_2d &renderer) {
    renderer.begin_batch(glintfx::gltfx_transform{
        .translation = {190.0, 330.0}, .rotation = {0.0}, .scale = {2.0, 2.0}});
    renderer.stroke_line({{0.0, 20.0}, {50.0, 20.0}},
                         {.stroke = {.thickness = 4.0, .unit = glintfx::gltfx_stroke_unit::world}},
                         k_yellow);
    renderer.stroke_line({{0.0, 45.0}, {50.0, 45.0}},
                         {.stroke = {.thickness = 4.0, .unit = glintfx::gltfx_stroke_unit::pixel}},
                         k_yellow);
    renderer.begin_batch();
}

// The whole scene of the first renderer, which clears the frame. The curve distance it was opened
// with is the default, 0.25 pixel: the circle K looks round.
void draw_scene(glintfx::gltfx_renderer_2d &renderer) {
    renderer.begin_frame(glintfx::gltfx_frame_2d_desc{.clear_color = k_background});
    draw_filled_shapes(renderer);
    draw_outlines(renderer);
    draw_caps(renderer);
    draw_path_markers(renderer);
    draw_stroke_units(renderer);
    renderer.fill_ellipse({.center = {400.0, 400.0}, .radius_x = 55.0, .radius_y = 55.0}, k_green);
}

// L: the second renderer draws over what the first one left, so its frame does not clear. It was
// opened with a curve distance of 12 pixels, so the same circle comes out with five sides.
void draw_coarse_circle(glintfx::gltfx_renderer_2d &coarse) {
    coarse.begin_frame(glintfx::gltfx_frame_2d_desc{.clear_color = std::nullopt});
    coarse.fill_ellipse({.center = {560.0, 400.0}, .radius_x = 55.0, .radius_y = 55.0}, k_green);
}

// Everything one run of the loop needs, and the two callbacks the loop calls on it.
struct example_state {
    glintfx::gltfx_renderer_2d &renderer;
    glintfx::gltfx_renderer_2d &coarse;
    std::optional<std::uint64_t> frames_to_draw;
    std::uint64_t frames_drawn = 0;
    bool ready_line_printed = false;
    std::uint64_t pieces_lost = 0;
    bool frame_failed = false;

    bool on_frame(const glintfx::gltfx_frame_tick &tick) noexcept;
    void on_render(const glintfx::gltfx_frame_tick & /*tick*/) noexcept;
    void count_frame(const glintfx::gltfx_rslt<glintfx::gltfx_frame_2d_report> &finished) noexcept;
    void announce_first_frame(const glintfx::gltfx_frame_tick &tick) noexcept;
    [[nodiscard]] bool all_frames_drawn() const noexcept;
};

// Runs every tick: prints the ready line once, and ends the loop after the last frame.
bool example_state::on_frame(const glintfx::gltfx_frame_tick &tick) noexcept {
    announce_first_frame(tick);
    return !all_frames_drawn();
}

// Runs only when the loop will present: both renderers draw, one after the other, and each frame
// report is kept. The loop presents after this returns.
void example_state::on_render(const glintfx::gltfx_frame_tick & /*tick*/) noexcept {
    ++frames_drawn;
    draw_scene(renderer);
    count_frame(renderer.finish_frame());
    draw_coarse_circle(coarse);
    count_frame(coarse.finish_frame());
}

// A piece refused, dropped or abandoned is a frame that did not show the whole scene: the run
// fails.
void example_state::count_frame(
    const glintfx::gltfx_rslt<glintfx::gltfx_frame_2d_report> &finished) noexcept {
    if (finished.has_error()) {
        frame_failed = true;
        return;
    }
    const glintfx::gltfx_frame_2d_report &report = finished.value();
    pieces_lost += report.pieces_refused + report.pieces_dropped_out_of_memory +
                   report.pieces_dropped_graphics_failure + report.pieces_dropped_outside_frame +
                   report.frames_abandoned;
}

// ---- Running without a person (CI, replay): --frames and the ready line ----

constexpr std::uint64_t k_max_frames = 1000000;

// The line a capture tool waits for before it reads a pixel, printed once, at the first tick after
// a frame was really presented. fflush, because a pipe makes stdout block-buffered and the tool
// would never see it.
void example_state::announce_first_frame(const glintfx::gltfx_frame_tick &tick) noexcept {
    if (ready_line_printed || frames_drawn < 1 ||
        tick.last_present != glintfx::gltfx_present_outcome::presented) {
        return;
    }
    std::puts("shapes: first frame presented");
    (void)std::fflush(stdout);
    ready_line_printed = true;
}

bool example_state::all_frames_drawn() const noexcept {
    return frames_to_draw.has_value() && frames_drawn >= *frames_to_draw;
}

// The command line: nothing, or --frames N with N in [1, 1000000], digits only (std::from_chars).
struct command_line {
    bool valid = false;
    std::optional<std::uint64_t> frames_to_draw;
};

command_line read_command_line(int argc, char **argv) {
    if (argc == 1) {
        return {.valid = true, .frames_to_draw = std::nullopt};
    }
    if (argc != 3 || std::string_view(argv[1]) != "--frames") {
        return {};
    }
    const std::string_view text(argv[2]);
    std::uint64_t frames = 0;
    const std::from_chars_result result =
        std::from_chars(text.data(), text.data() + text.size(), frames);
    const bool whole_number = result.ec == std::errc{} && result.ptr == text.data() + text.size();
    if (!whole_number || frames < 1 || frames > k_max_frames) {
        return {};
    }
    return {.valid = true, .frames_to_draw = frames};
}

// ---- Opening, running and reporting ----

// One failed step of the library, in the form the capture drivers read; the example then exits 1.
int fail(std::string_view step, const glintfx::gltfx_err &error) {
    const std::string_view code = glintfx::gltfx_err_code_name(error.code());
    const std::string_view rejected = error.rejected_value();
    (void)std::fprintf(stderr, "shapes: %.*s failed: %.*s rejected_value=%.*s\n",
                       static_cast<int>(step.size()), step.data(), static_cast<int>(code.size()),
                       code.data(), static_cast<int>(rejected.size()), rejected.data());
    return k_exit_failed;
}

// The frame report of the whole run: any piece lost, or a frame that failed to finish, fails the
// run.
int report_frames(const example_state &state) {
    std::printf("shapes: frames_drawn=%llu pieces_lost=%llu frame_failed=%d\n",
                static_cast<unsigned long long>(state.frames_drawn),
                static_cast<unsigned long long>(state.pieces_lost), state.frame_failed ? 1 : 0);
    return state.pieces_lost > 0 || state.frame_failed ? k_exit_failed : k_exit_ok;
}

// The two renderers over the same context, then the loop. Each renderer keeps the curve distance it
// was opened with; the second one is opened with a large one on purpose (L).
int run_with_context(glintfx::gltfx_display &display, glintfx::gltfx_window &window,
                     glintfx::gltfx_gl_context &context,
                     std::optional<std::uint64_t> frames_to_draw) {
    auto renderer = glintfx::gltfx_renderer_2d::open(context, glintfx::gltfx_renderer_2d_desc{});
    if (renderer.has_error()) {
        return fail("renderer", renderer.err());
    }
    auto coarse = glintfx::gltfx_renderer_2d::open(
        context, glintfx::gltfx_renderer_2d_desc{.curve_max_error_pixels = k_coarse_curve_pixels});
    if (coarse.has_error()) {
        return fail("coarse_renderer", coarse.err());
    }
    auto loop = glintfx::gltfx_loop::open(display, window, context);
    if (loop.has_error()) {
        return fail("loop", loop.err());
    }
    example_state state{
        .renderer = renderer.value(), .coarse = coarse.value(), .frames_to_draw = frames_to_draw};
    const glintfx::gltfx_rslt<void> ran = loop.value().run(
        glintfx::gltfx_bind_loop_callbacks<&example_state::on_frame, &example_state::on_render>(
            state));
    if (ran.has_error()) {
        return fail("run", ran.err());
    }
    return report_frames(state);
}

int run_example(std::optional<std::uint64_t> frames_to_draw) {
    auto display = glintfx::gltfx_display::open();
    if (display.has_error()) {
        return fail("display", display.err());
    }
    auto window = glintfx::gltfx_window::open(display.value(), k_window_desc);
    if (window.has_error()) {
        return fail("window", window.err());
    }
    auto context =
        glintfx::gltfx_gl_context::open(window.value(), glintfx::gltfx_gl_context_desc{});
    if (context.has_error()) {
        return fail("context", context.err());
    }
    return run_with_context(display.value(), window.value(), context.value(), frames_to_draw);
}

} // namespace

int main(int argc, char **argv) {
    const command_line args = read_command_line(argc, argv);
    if (!args.valid) {
        (void)std::fputs("usage: shapes [--frames N]   (N from 1 to 1000000)\n", stderr);
        return k_exit_usage;
    }
    return run_example(args.frames_to_draw);
}
