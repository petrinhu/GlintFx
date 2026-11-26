// SPDX-License-Identifier: AGPL-3.0-or-later
// first_window.cpp - the first GlintFx consumer example (DEMO-1): open a window, create its GL
// context, open the 2D renderer and run the library's own loop, drawing one fixed scene in every
// frame. A living proof, not a product: it uses only the public API, and nothing here becomes a
// rule of the library.
//
// THE SCENE, in pixels of the buffer (origin at the top-left corner; a rectangle is its corner and
// its size; a lower layer is painted first, whatever the order of sending):
//   background  the whole window     (0.2, 0.2, 0.2, 1)  cleared by the CPU
//   A, red      (40, 40, 200, 150)   (1, 0, 0, 1)        layer 0, sent 2nd
//   B, blue     (160, 120, 200, 150) (0, 0, 1, 1)        layer 1, sent 1st: on top of A
//   C, white    (300, 200, 200, 150) (1, 1, 1, 0.5)      layer 2, sent 3rd: over background and B
//   D, green    (x, 400, 32, 32)     (0, 1, 0, 1)        layer 3, sent 4th: the square that moves
// SCALE 1 IS ASSUMED AND DECLARED: the window is 640x480 logical pixels and the scene is in
// physical pixels (renderer_2d.hpp), so a scaled desktop would move it. The capture drivers refuse
// a capture that is not 640x480.
//
// USAGE: first_window [--frames N]
//   no argument: runs until the system asks to close the window, and exits 0;
//   --frames N (1 to 1000000): stops by itself after N DRAWN frames, through the normal path of the
//   loop.
// EXIT: 0 done; 1 a step of the library failed (the line says which) or a frame lost pieces; 2 bad
// usage.
// The command line, the ready line, the line of a failed step and the tally of the frames are the
// ones every example shares to run without a person (examples/common/example_harness.hpp); this
// file is the lesson.

#include <cmath>
#include <cstdint>
#include <optional>
#include <string_view>

#include <glintfx/core/color.hpp>
#include <glintfx/core/err.hpp>
#include <glintfx/core/rect.hpp>
#include <glintfx/draw2d/frame_2d_desc.hpp>
#include <glintfx/draw2d/frame_2d_report.hpp>
#include <glintfx/draw2d/renderer_2d.hpp>
#include <glintfx/draw2d/renderer_2d_desc.hpp>
#include <glintfx/platform/gl/context.hpp>
#include <glintfx/platform/loop/loop.hpp>
#include <glintfx/platform/loop/loop_bind.hpp>
#include <glintfx/platform/window/display.hpp>
#include <glintfx/platform/window/window.hpp>

#include "../common/example_harness.hpp"

namespace {

using glintfx_examples::frame_tally;

constexpr std::string_view k_example = "first_window";

constexpr glintfx::gltfx_window_desc k_window_desc{
    .title = "GlintFx example: first window",
    .application_id = "org.glintfx.examples.first_window",
    .logical_size = {.width = 640, .height = 480},
};

constexpr glintfx::gltfx_rgba k_background{.red = 0.2F, .green = 0.2F, .blue = 0.2F, .alpha = 1.0F};

// The green square moves at 120 pixels per second and starts over at x = 20 once it passes x = 588.
constexpr double k_square_start_x = 20.0;
constexpr double k_square_last_x = 588.0;
constexpr double k_square_pixels_per_second = 120.0;

// ---- The lesson: one scene, drawn in every frame ----

// Clears the frame and sends the four pieces. B goes before A on purpose: the layer decides who is
// on top.
void draw_scene(glintfx::gltfx_renderer_2d &renderer, double square_x) {
    renderer.begin_frame(glintfx::gltfx_frame_2d_desc{.clear_color = k_background});
    renderer.fill_rect({{160.0, 120.0}, {200.0, 150.0}}, {0.0F, 0.0F, 1.0F, 1.0F}, {1});
    renderer.fill_rect({{40.0, 40.0}, {200.0, 150.0}}, {1.0F, 0.0F, 0.0F, 1.0F}, {0});
    renderer.fill_rect({{300.0, 200.0}, {200.0, 150.0}}, {1.0F, 1.0F, 1.0F, 0.5F}, {2});
    renderer.fill_rect({{square_x, 400.0}, {32.0, 32.0}}, {0.0F, 1.0F, 0.0F, 1.0F}, {3});
}

// Where the square is after `seconds` of real time; starts the time over when the square leaves the
// window.
double timed_square_x(double &seconds) {
    double x = k_square_start_x + std::floor(k_square_pixels_per_second * seconds);
    if (x > k_square_last_x) {
        seconds = 0.0;
        x = k_square_start_x;
    }
    return x;
}

// Everything one run of the loop needs, and the two callbacks the loop calls on it.
struct example_state {
    glintfx::gltfx_renderer_2d &renderer;
    frame_tally tally;
    double seconds = 0.0;

    bool on_frame(const glintfx::gltfx_frame_tick &tick) noexcept;
    void on_render(const glintfx::gltfx_frame_tick & /*tick*/) noexcept;
    [[nodiscard]] double square_x() noexcept;
};

// Runs every tick: keeps the time, prints the ready line once, and ends the loop after the last
// frame.
bool example_state::on_frame(const glintfx::gltfx_frame_tick &tick) noexcept {
    seconds += static_cast<double>(tick.elapsed.nanoseconds) / 1e9;
    tally.announce_first_frame(tick);
    return !tally.all_frames_drawn();
}

// Runs only when the loop will present: draws the scene and keeps what the frame reported.
void example_state::on_render(const glintfx::gltfx_frame_tick & /*tick*/) noexcept {
    ++tally.frames_drawn;
    draw_scene(renderer, square_x());
    tally.count_frame(renderer.finish_frame());
}

// With --frames, the square moves two whole pixels per DRAWN frame (x = 20 + 2 * (d - 1)), so the
// last frame's edges are exact pixels a capture can probe; without it, it follows real time.
double example_state::square_x() noexcept {
    if (!tally.frames_to_draw.has_value()) {
        return timed_square_x(seconds);
    }
    return k_square_start_x + 2.0 * static_cast<double>(tally.frames_drawn - 1);
}

// ---- Opening and running ----

// The window, its context, the renderer and the loop, in that order; the first step that fails ends
// the run.
int run_example(std::optional<std::uint64_t> frames_to_draw) {
    auto display = glintfx::gltfx_display::open();
    if (display.has_error()) {
        return glintfx_examples::fail(k_example, "display", display.err());
    }
    auto window = glintfx::gltfx_window::open(display.value(), k_window_desc);
    if (window.has_error()) {
        return glintfx_examples::fail(k_example, "window", window.err());
    }
    auto context =
        glintfx::gltfx_gl_context::open(window.value(), glintfx::gltfx_gl_context_desc{});
    if (context.has_error()) {
        return glintfx_examples::fail(k_example, "context", context.err());
    }
    auto renderer =
        glintfx::gltfx_renderer_2d::open(context.value(), glintfx::gltfx_renderer_2d_desc{});
    if (renderer.has_error()) {
        return glintfx_examples::fail(k_example, "renderer", renderer.err());
    }
    auto loop = glintfx::gltfx_loop::open(display.value(), window.value(), context.value());
    if (loop.has_error()) {
        return glintfx_examples::fail(k_example, "loop", loop.err());
    }
    example_state state{.renderer = renderer.value(),
                        .tally = {.example = k_example, .frames_to_draw = frames_to_draw}};
    const glintfx::gltfx_rslt<void> ran = loop.value().run(
        glintfx::gltfx_bind_loop_callbacks<&example_state::on_frame, &example_state::on_render>(
            state));
    if (ran.has_error()) {
        return glintfx_examples::fail(k_example, "run", ran.err());
    }
    return state.tally.report();
}

} // namespace

int main(int argc, char **argv) {
    const glintfx_examples::command_line args = glintfx_examples::read_command_line(argc, argv);
    if (!args.valid) {
        return glintfx_examples::print_usage(k_example);
    }
    return run_example(args.frames_to_draw);
}
