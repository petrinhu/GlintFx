// SPDX-License-Identifier: AGPL-3.0-or-later
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/draw2d/renderer_2d.hpp>
#include <glintfx/platform/gl/context.hpp>
#include <glintfx/platform/gl/gfx_option.hpp>
#include <glintfx/platform/window/display.hpp>
#include <glintfx/platform/window/window.hpp>

// capture_known_color_smoke.cpp - QA-SCREEN-CAPTURE P3 (D-W8-20, D-W8-32, D-W8-33) and C2b-4
// (D-W8-39): the ONE fixture, the same source on both systems (no #if, only the public API), whose
// frame is captured from OUTSIDE and whose own pixel readback the capture is then compared against.
// Linux: the wire relay captures the frame IN THE PROTOCOL
// (tests/container/run_capture_known_color.sh). Windows: tests/tools/win32_window_capture.cpp
// launches this fixture and reads its window by PrintWindow and by BitBlt of the screen
// (tests/tools/run_capture_known_color_win32.py). Test instrument, not product code: it draws a
// frame of KNOWN colors through the PUBLIC API only (the shape of draw2d_parity_test.cpp), reads
// the WHOLE frame back with glReadPixels BEFORE the swap, writes those bytes untouched, and
// presents the frame (swap_buffers() answering `presented`). The relay saves the last committed
// wl_shm buffer when this connection closes; the driver
// (tests/container/run_capture_known_color.sh) compares the two, byte for byte, alpha included,
// with tests/tools/capture_vs_readback.py, and probes the capture's pixels with
// tests/tools/image_probe.py against tests/fixtures/capture_known_color_probes_<mode>.txt.
//
// USAGE: capture_known_color_smoke <off|on> <readback-directory> [--hold-until-close <ms>]
//            [<sabotage>]
//   off|on: the context option srgb_framebuffer (open_only), one window per process, so ONE run is
//   one connection and one captured surface: the mode of every capture is known by construction.
//   readback-directory: an existing directory INSIDE the container; writes readback_<mode>.raw
//   (RGBA, glReadPixels order, bottom row first, untouched) and readback_<mode>.meta.
//   --hold-until-close <ms>: after the frame is presented, pump events every 10 ms until
//   close_requested() (a window that is read from outside must be alive and answering messages;
//   PrintWindow waits for the owner of the window). Budget exceeded: the fixture FAILS. The Linux
//   driver does not pass it (the relay records at disconnect); the Windows driver always does.
//   sabotage (L-36, the debut proof that the gate bites, never used by the real run): swap_red_blue
//   draws the red square blue; alpha_half draws it half transparent; corrupt_readback flips one
//   byte of the readback AFTER it is read (the equality, not the probes, must catch that one).
//
// THE SCENE (L-43: the criterion is fixed HERE, before the data exists). 320x240, origin top-left,
// the clear is TRANSPARENT black (0,0,0,0), so the destination alpha is real and a half-transparent
// piece leaves alpha 0.5 behind it (over an opaque clear the final alpha would always be 1 and the
// alpha probe could never fail). Asymmetric in both axes:
//   opaque red    (20,20)   60x60  -> RGBA FF0000FF, both modes
//   half white    (120,20)  60x60  -> premultiplied: 808080 80 (off), BCBCBC 80 (on), +-2
//   half red      (20,150)  60x60  -> premultiplied: 800000 80 (off), BC0000 80 (on), +-2
//   opaque blue   (240,160) 60x60  -> RGBA 0000FFFF
//   outside every piece            -> RGBA 00000000
// wl_shm ARGB8888 is PREMULTIPLIED and nothing in the reader un-premultiplies, so the half values
// are the premultiplied ones. With srgb_framebuffer ON the hardware encodes the stored value
// (encode(1.0 * 0.5) = 188); with it OFF the library encodes first and then premultiplies
// (encode(1.0) * 0.5 = 127.5, 127 or 128 by the rasterizer's rounding): the two modes legitimately
// differ, which is why there is one probe file per mode and a sabotage run in each. The alpha is
// not encoded in either mode: 127.5, 127 or 128.
//
// EXIT CODES: 0 ok; 1 the fixture failed; 77 DECLARED ABSENCE (D-W8-39, the pattern of
// draw2d_parity_test.cpp, D-SRGB2-13): the context refused srgb_framebuffer=on and named that very
// option, printed as the line "AUSENCIA DECLARADA srgb_framebuffer=on". The Windows driver COUNTS
// that absence (the runner's Mesa has no sRGB framebuffer); the Linux driver treats 77 as a FAILURE
// (llvmpipe supports it there, so an absence is a regression). Same behavior of the fixture, a
// verdict per system.
//
// THE FIXTURE ITSELF FAILS when: the context cannot open (the srgb refusal above aside), the swap
// never answers `presented` (a frame that is never shown is never captured), the surface is smaller
// than the scene, or the readback cannot be written. What it does NOT decide: whether the capture
// equals the readback and whether the pixels are the expected ones - that is the driver's verdict,
// from the capture the relay writes after this process is gone.

namespace {

using gl_enum = unsigned int;
using gl_int = int;
using gl_sizei = int;

constexpr gl_enum k_gl_viewport = 0x0BA2;
constexpr gl_enum k_gl_rgba = 0x1908;
constexpr gl_enum k_gl_unsigned_byte = 0x1401;
constexpr int k_scene_width = 320;
constexpr int k_scene_height = 240;
constexpr int k_present_attempts = 200;
constexpr std::chrono::milliseconds k_present_pause{10};
constexpr int k_exit_declared_absence = 77;
constexpr int k_max_hold_ms = 600000;
constexpr std::chrono::milliseconds k_hold_pause{10};

struct gl_api {
    void (*get_integerv)(gl_enum, gl_int *) = nullptr;
    void (*finish)() = nullptr;
    void (*read_pixels)(gl_int, gl_int, gl_sizei, gl_sizei, gl_enum, gl_enum, void *) = nullptr;
};

struct run_options {
    std::string mode;
    std::string directory;
    std::string sabotage;
    bool srgb = false;
    int hold_ms = 0; // 0: no hold (the Linux run)
};

struct frame_bytes {
    int width = 0;
    int height = 0;
    std::vector<unsigned char> rgba; // glReadPixels order: bottom row first
};

void say(const char *what, const std::string &detail) {
    std::fprintf(stdout, "capture_known_color_smoke: %s %s\n", what, detail.c_str());
}

int fail(const char *what, const std::string &detail = "") {
    std::fprintf(stderr, "capture_known_color_smoke: FAIL %s %s\n", what, detail.c_str());
    return EXIT_FAILURE;
}

std::string code_name(const glintfx::gltfx_err &error) {
    return std::string(glintfx::gltfx_err_code_name(error.code()));
}

bool is_known_sabotage(std::string_view name) {
    return name.empty() || name == "swap_red_blue" || name == "alpha_half" ||
           name == "corrupt_readback";
}

// "<positive integer>" up to k_max_hold_ms; false for anything else (never a guess).
bool read_hold_ms(const char *text, int &value) {
    char *end = nullptr;
    const long parsed = std::strtol(text, &end, 10);
    if (end == text || *end != '\0' || parsed <= 0 || parsed > k_max_hold_ms) {
        return false;
    }
    value = static_cast<int>(parsed);
    return true;
}

// <off|on> <directory> [--hold-until-close <ms>] [<sabotage>]: the Linux driver's two or three
// arguments stay valid as they were.
std::optional<run_options> parse_options(int argc, char **argv) {
    if (argc < 3) {
        return std::nullopt;
    }
    run_options options;
    options.mode = argv[1];
    options.directory = argv[2];
    options.srgb = options.mode == "on";
    int next = 3;
    if (next < argc && std::string_view(argv[next]) == "--hold-until-close") {
        if (next + 1 >= argc || !read_hold_ms(argv[next + 1], options.hold_ms)) {
            return std::nullopt;
        }
        next += 2;
    }
    if (next < argc) {
        options.sabotage = argv[next];
        ++next;
    }
    if (next != argc || (options.mode != "on" && options.mode != "off") ||
        !is_known_sabotage(options.sabotage)) {
        return std::nullopt;
    }
    return options;
}

template <typename F>
bool resolve(const glintfx::gltfx_gl_context &context, const char *name, F &out) {
    void *address = context.proc_address(name);
    if (address == nullptr) {
        say("FAIL entry point not resolved:", name);
        return false;
    }
    out = reinterpret_cast<F>(address);
    return true;
}

bool load_gl(const glintfx::gltfx_gl_context &context, gl_api &gl) {
    return resolve(context, "glGetIntegerv", gl.get_integerv) &&
           resolve(context, "glFinish", gl.finish) &&
           resolve(context, "glReadPixels", gl.read_pixels);
}

glintfx::gltfx_rect_world square(double x, double y) {
    return glintfx::gltfx_rect_world{{x, y}, {60.0, 60.0}};
}

// The four pieces of the scene. `red` is what the sabotage runs change.
void draw_scene(glintfx::gltfx_renderer_2d &renderer, const std::string &sabotage) {
    glintfx::gltfx_rgba red{1.0F, 0.0F, 0.0F, 1.0F};
    if (sabotage == "swap_red_blue") {
        red = glintfx::gltfx_rgba{0.0F, 0.0F, 1.0F, 1.0F};
    } else if (sabotage == "alpha_half") {
        red = glintfx::gltfx_rgba{1.0F, 0.0F, 0.0F, 0.5F};
    }
    glintfx::gltfx_frame_2d_desc desc;
    desc.clear_color = glintfx::gltfx_rgba{0.0F, 0.0F, 0.0F, 0.0F};
    renderer.begin_frame(desc);
    renderer.fill_rect(square(20, 20), red);
    renderer.fill_rect(square(120, 20), glintfx::gltfx_rgba{1.0F, 1.0F, 1.0F, 0.5F});
    renderer.fill_rect(square(20, 150), glintfx::gltfx_rgba{1.0F, 0.0F, 0.0F, 0.5F});
    renderer.fill_rect(square(240, 160), glintfx::gltfx_rgba{0.0F, 0.0F, 1.0F, 1.0F});
}

frame_bytes read_whole_frame(const gl_api &gl, int width, int height) {
    frame_bytes frame;
    frame.width = width;
    frame.height = height;
    frame.rgba.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U, 0);
    gl.finish();
    gl.read_pixels(0, 0, width, height, k_gl_rgba, k_gl_unsigned_byte, frame.rgba.data());
    return frame;
}

// The pixel at TOP-origin (x, y), as the readback holds it (bottom row first).
std::string describe_pixel(const frame_bytes &frame, int x, int y) {
    const std::size_t at =
        (static_cast<std::size_t>(frame.height - 1 - y) * static_cast<std::size_t>(frame.width) +
         static_cast<std::size_t>(x)) *
        4U;
    char text[64];
    const int written = std::snprintf(
        text, sizeof(text), "(%d,%d)=%02X%02X%02X%02X", x, y, static_cast<unsigned>(frame.rgba[at]),
        static_cast<unsigned>(frame.rgba[at + 1]), static_cast<unsigned>(frame.rgba[at + 2]),
        static_cast<unsigned>(frame.rgba[at + 3]));
    return written > 0 ? std::string(text) : std::string("?");
}

void report_probe_pixels(const frame_bytes &frame) {
    std::string line;
    for (const auto &point : {std::pair{50, 50}, std::pair{150, 50}, std::pair{50, 180},
                              std::pair{270, 190}, std::pair{160, 200}}) {
        line += describe_pixel(frame, point.first, point.second) + " ";
    }
    say("readback RGBA", line);
}

bool write_all(const std::string &path, const void *data, std::size_t size) {
    std::ofstream file(path, std::ios::binary);
    file.write(static_cast<const char *>(data), static_cast<std::streamsize>(size));
    file.flush();
    return static_cast<bool>(file);
}

bool write_readback(const run_options &options, const frame_bytes &frame) {
    const std::string stem = options.directory + "/readback_" + options.mode;
    const std::string meta = "width=" + std::to_string(frame.width) +
                             "\nheight=" + std::to_string(frame.height) +
                             "\norigin=bottom_left\norder=rgba\n";
    return write_all(stem + ".raw", frame.rgba.data(), frame.rgba.size()) &&
           write_all(stem + ".meta", meta.data(), meta.size());
}

// A frame that never reaches the compositor is never captured: `skipped_hidden` is retried (the
// window is not shown yet), a refusal or the budget running out is a failure.
bool present_until_shown(glintfx::gltfx_gl_context &context) {
    for (int attempt = 1; attempt <= k_present_attempts; ++attempt) {
        const glintfx::gltfx_rslt<glintfx::gltfx_present_outcome> swapped = context.swap_buffers();
        if (swapped.has_error()) {
            say("FAIL swap_buffers:", code_name(swapped.err()));
            return false;
        }
        if (swapped.value() == glintfx::gltfx_present_outcome::presented) {
            say("presented at attempt", std::to_string(attempt));
            return true;
        }
        std::this_thread::sleep_for(k_present_pause);
    }
    say("FAIL never presented in attempts:", std::to_string(k_present_attempts));
    return false;
}

struct opened_surface {
    std::optional<glintfx::gltfx_window> window;
    std::optional<glintfx::gltfx_gl_context> context;
};

enum class surface_outcome { opened, absent, failed };

// srgb_framebuffer=on refused BY NAME (code unsupported, rejected_value "srgb_framebuffer"): a
// declared absence (D-SRGB2-13). A refusal that names another option is a defect of the adapter.
bool is_declared_srgb_absence(const glintfx::gltfx_err &error) {
    return error.code() == glintfx::gltfx_err_code::unsupported &&
           error.rejected_value() == "srgb_framebuffer";
}

// Window, then context with srgb_framebuffer set to the mode, made current, vsync off (swap_buffers
// never waits for the compositor's frame callback, so the run is not paced by it). `absent` only
// when the mode is on and the context refused srgb_framebuffer by name.
surface_outcome open_surface(glintfx::gltfx_display &display, bool srgb, opened_surface &out) {
    const glintfx::gltfx_window_desc window_desc{
        .title = "janela da fumaca de cor conhecida",
        .application_id = "org.glintfx.capture_known_color_smoke",
        .logical_size = {.width = k_scene_width, .height = k_scene_height},
    };
    glintfx::gltfx_rslt<glintfx::gltfx_window> window =
        glintfx::gltfx_window::open(display, window_desc);
    if (window.has_error()) {
        say("FAIL window open:", code_name(window.err()));
        return surface_outcome::failed;
    }
    out.window.emplace(std::move(window.value()));
    const glintfx::gltfx_gfx_option_entry options[] = {
        {.id = glintfx::gltfx_gfx_option::srgb_framebuffer, .value = srgb ? 1 : 0}};
    const glintfx::gltfx_gl_context_desc desc{.options = options, .option_count = 1};
    glintfx::gltfx_rslt<glintfx::gltfx_gl_context> context =
        glintfx::gltfx_gl_context::open(*out.window, desc);
    if (context.has_error()) {
        if (srgb && is_declared_srgb_absence(context.err())) {
            say("AUSENCIA DECLARADA srgb_framebuffer=on: the context refused the option",
                code_name(context.err()));
            return surface_outcome::absent;
        }
        say("FAIL context open:", code_name(context.err()) + " rejected_value=" +
                                      std::string(context.err().rejected_value()));
        return surface_outcome::failed;
    }
    out.context.emplace(std::move(context.value()));
    const glintfx::gltfx_rslt<void> current = out.context->make_current();
    const glintfx::gltfx_rslt<void> vsync =
        out.context->set_option({.id = glintfx::gltfx_gfx_option::vsync, .value = 0});
    if (current.has_error() || vsync.has_error()) {
        say("FAIL make_current or vsync=off", "");
        return surface_outcome::failed;
    }
    return surface_outcome::opened;
}

// The viewport the renderer leaves after an empty frame: the real surface size.
bool measure_surface(glintfx::gltfx_renderer_2d &renderer, const gl_api &gl, int &width,
                     int &height) {
    renderer.begin_frame(glintfx::gltfx_frame_2d_desc{});
    if (renderer.finish_frame().has_error()) {
        return false;
    }
    gl_int viewport[4] = {0, 0, 0, 0};
    gl.get_integerv(k_gl_viewport, viewport);
    width = viewport[2];
    height = viewport[3];
    return width >= k_scene_width && height >= k_scene_height;
}

int draw_read_and_present(glintfx::gltfx_gl_context &context, const run_options &options) {
    gl_api gl;
    if (!load_gl(context, gl)) {
        return EXIT_FAILURE;
    }
    glintfx::gltfx_rslt<glintfx::gltfx_renderer_2d> opened =
        glintfx::gltfx_renderer_2d::open(context, glintfx::gltfx_renderer_2d_desc{});
    if (opened.has_error()) {
        return fail("renderer open:", code_name(opened.err()));
    }
    glintfx::gltfx_renderer_2d renderer = std::move(opened.value());
    int width = 0;
    int height = 0;
    if (!measure_surface(renderer, gl, width, height)) {
        return fail("surface smaller than the scene:",
                    std::to_string(width) + "x" + std::to_string(height));
    }
    draw_scene(renderer, options.sabotage);
    if (renderer.finish_frame().has_error()) {
        return fail("finish_frame", "");
    }
    frame_bytes frame = read_whole_frame(gl, width, height);
    if (options.sabotage == "corrupt_readback") {
        frame.rgba[0] ^= 0xFFU;
    }
    report_probe_pixels(frame);
    if (!write_readback(options, frame)) {
        return fail("cannot write the readback into", options.directory);
    }
    return present_until_shown(context) ? EXIT_SUCCESS : EXIT_FAILURE;
}

// After the frame is presented, pump events until the window is asked to close (the tool posts
// WM_CLOSE after its captures). A window read from outside has to stay alive and answer messages.
bool hold_until_close(glintfx::gltfx_display &display, const glintfx::gltfx_window &window,
                      int budget_ms) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(budget_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        if (const glintfx::gltfx_rslt<void> pumped = display.pump_events(); pumped.has_error()) {
            say("FAIL pump_events while holding:", code_name(pumped.err()));
            return false;
        }
        if (window.close_requested()) {
            say("close requested", "");
            return true;
        }
        std::this_thread::sleep_for(k_hold_pause);
    }
    say("FAIL no close request within ms:", std::to_string(budget_ms));
    return false;
}

int run(const run_options &options) {
    glintfx::gltfx_rslt<glintfx::gltfx_display> display_opened = glintfx::gltfx_display::open();
    if (display_opened.has_error()) {
        return fail("display open:", code_name(display_opened.err()));
    }
    glintfx::gltfx_display display = std::move(display_opened.value());
    opened_surface surface;
    const surface_outcome opened = open_surface(display, options.srgb, surface);
    if (opened == surface_outcome::absent) {
        return k_exit_declared_absence;
    }
    if (opened == surface_outcome::failed) {
        return EXIT_FAILURE;
    }
    if (!surface.context.has_value() || !surface.window.has_value()) {
        return fail("context missing after open_surface", "");
    }
    int outcome = draw_read_and_present(*surface.context, options);
    if (outcome == EXIT_SUCCESS && options.hold_ms > 0 &&
        !hold_until_close(display, *surface.window, options.hold_ms)) {
        outcome = EXIT_FAILURE;
    }
    if (outcome == EXIT_SUCCESS) {
        say("ok mode",
            options.mode + (options.sabotage.empty() ? "" : " sabotage=" + options.sabotage));
    }
    return outcome;
}

} // namespace

int main(int argc, char **argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    const std::optional<run_options> options = parse_options(argc, argv);
    if (!options.has_value()) {
        return fail("usage: capture_known_color_smoke <off|on> <readback-directory> "
                    "[--hold-until-close <ms>] [swap_red_blue|alpha_half|corrupt_readback]");
    }
    return run(*options);
}
