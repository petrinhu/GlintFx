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

#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string_view>
#include <system_error>

#include <glintfx/core/color.hpp>
#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
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

namespace {

constexpr int k_exit_ok = 0;
constexpr int k_exit_failed = 1;
constexpr int k_exit_usage = 2;

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

// D6-red: only the clear; the four pieces arrive in D6-green.
void draw_scene(glintfx::gltfx_renderer_2d &renderer, [[maybe_unused]] double square_x) {
    renderer.begin_frame(glintfx::gltfx_frame_2d_desc{.clear_color = k_background});
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
    std::optional<std::uint64_t> frames_to_draw;
    std::uint64_t frames_drawn = 0;
    double seconds = 0.0;
    bool ready_line_printed = false;
    std::uint64_t pieces_lost = 0;
    bool frame_failed = false;

    bool on_frame(const glintfx::gltfx_frame_tick &tick) noexcept;
    void on_render(const glintfx::gltfx_frame_tick & /*tick*/) noexcept;
    void count_frame(const glintfx::gltfx_rslt<glintfx::gltfx_frame_2d_report> &finished) noexcept;
    [[nodiscard]] double square_x() noexcept;
    void announce_first_frame(const glintfx::gltfx_frame_tick &tick) noexcept;
    [[nodiscard]] bool all_frames_drawn() const noexcept;
};

// Runs every tick: keeps the time, prints the ready line once, and ends the loop after the last
// frame.
bool example_state::on_frame(const glintfx::gltfx_frame_tick &tick) noexcept {
    seconds += static_cast<double>(tick.elapsed.nanoseconds) / 1e9;
    announce_first_frame(tick);
    return !all_frames_drawn();
}

// Runs only when the loop will present: draws the scene and keeps what the frame reported.
void example_state::on_render(const glintfx::gltfx_frame_tick & /*tick*/) noexcept {
    ++frames_drawn;
    draw_scene(renderer, square_x());
    count_frame(renderer.finish_frame());
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

// With --frames, the square moves two whole pixels per DRAWN frame (x = 20 + 2 * (d - 1)), so the
// last frame's edges are exact pixels a capture can probe; without it, it follows real time.
double example_state::square_x() noexcept {
    if (!frames_to_draw.has_value()) {
        return timed_square_x(seconds);
    }
    return k_square_start_x + 2.0 * static_cast<double>(frames_drawn - 1);
}

// The line a capture tool waits for before it reads a pixel, printed once, at the first tick after
// a frame was really presented. fflush, because a pipe makes stdout block-buffered and the tool
// would never see it.
void example_state::announce_first_frame(const glintfx::gltfx_frame_tick &tick) noexcept {
    if (ready_line_printed || frames_drawn < 1 ||
        tick.last_present != glintfx::gltfx_present_outcome::presented) {
        return;
    }
    std::puts("first_window: first frame presented");
    std::fflush(stdout);
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
    std::fprintf(stderr, "first_window: %.*s failed: %.*s rejected_value=%.*s\n",
                 static_cast<int>(step.size()), step.data(), static_cast<int>(code.size()),
                 code.data(), static_cast<int>(rejected.size()), rejected.data());
    return k_exit_failed;
}

// The frame report of the whole run: any piece lost, or a frame that failed to finish, fails the
// run.
int report_frames(const example_state &state) {
    std::printf("first_window: frames_drawn=%llu pieces_lost=%llu frame_failed=%d\n",
                static_cast<unsigned long long>(state.frames_drawn),
                static_cast<unsigned long long>(state.pieces_lost), state.frame_failed ? 1 : 0);
    return state.pieces_lost > 0 || state.frame_failed ? k_exit_failed : k_exit_ok;
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
    auto renderer =
        glintfx::gltfx_renderer_2d::open(context.value(), glintfx::gltfx_renderer_2d_desc{});
    if (renderer.has_error()) {
        return fail("renderer", renderer.err());
    }
    auto loop = glintfx::gltfx_loop::open(display.value(), window.value(), context.value());
    if (loop.has_error()) {
        return fail("loop", loop.err());
    }
    example_state state{.renderer = renderer.value(), .frames_to_draw = frames_to_draw};
    const glintfx::gltfx_rslt<void> ran = loop.value().run(
        glintfx::gltfx_bind_loop_callbacks<&example_state::on_frame, &example_state::on_render>(
            state));
    if (ran.has_error()) {
        return fail("run", ran.err());
    }
    return report_frames(state);
}

} // namespace

int main(int argc, char **argv) {
    const command_line args = read_command_line(argc, argv);
    if (!args.valid) {
        std::fputs("usage: first_window [--frames N]   (N from 1 to 1000000)\n", stderr);
        return k_exit_usage;
    }
    return run_example(args.frames_to_draw);
}
