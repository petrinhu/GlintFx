// SPDX-License-Identifier: AGPL-3.0-or-later
// sprite_shadows.cpp - the GlintFx example of the shadows of standing pieces (R2D-SPRITE-SHADE):
// one sun, the shadow of a piece projected on the ground (core/projected_shadow.hpp), a shadow
// that darkens the ground once even where two shadows cover each other (fill_shadow), and the
// shadow of an image that takes the shape of the image (draw_shadow_texture), side by side, in
// one fixed scene drawn in every frame. A living proof, not a product: it uses only the public
// API, and nothing here becomes a rule of the library. Read examples/first_window/ first (the
// window, the context and the loop), examples/shapes/ (the shapes of one colour) and
// examples/textures/ (the images).
//
// THE SCENE, in pixels of the buffer (origin at the top-left corner, y grows DOWN; a rectangle
// is its corner and its size). The world IS the buffer: no batch transform is set, so a world
// unit is one pixel. The background is white, (1, 1, 1). The image is bytes in memory: no file
// is read.
//
//   THE SUN: one directional light, from azimuth 0 degrees (the light COMES FROM +x, the right
//   of the screen) at an elevation of 45 degrees. Every shadow falls to the LEFT of its piece,
//   and is as long as the piece is tall.
//   THE SHADOW COLOUR: black with alpha 0.5, the same for every shadow. Over white it gives the
//   grey 128 (127.5 before rounding); a pixel darkened twice would be 64.
//   A  a standing piece whose base runs from (300, 100) to (300, 160), 120 tall: its shadow
//      covers x 180 to 300, y 100 to 160. The piece itself is drawn as a blue box, x 300 to 340.
//   B  the same piece moved: base from (360, 130) to (360, 190), 120 tall, its shadow x 240 to
//      360, y 130 to 190, and a red box x 360 to 400. THE TWO SHADOWS COVER EACH OTHER at x 240
//      to 300, y 130 to 160, and that part is the same grey as the rest: a pixel is darkened
//      once. The blue box lies in the shadow of B too (x 300 to 340, y 130 to 160), but it is
//      drawn AFTER the shadows, so it stays pure blue: what is painted later is on top.
//   C  a 2 x 1 image: its left texel opaque green, its right texel fully transparent. Its piece
//      has the base from (100, 300) to (100, 360), 80 tall. draw_shadow_texture lays the shape
//      of the image on the shadow of that piece, x 20 to 100: the shadow is grey only where the
//      image is opaque. The top edge of the image runs along the base, from (20, 300) to
//      (20, 360), so the opaque texel darkens y 300 to 330 and the transparent one leaves y 330
//      to 360 white. The image itself is drawn next to its shadow, x 100 to 160, y 300 to 360:
//      green on the left half, nothing on the right half.
//
// The order of painting, all in one layer: the shadow of A, the shadow of B, the shadow of C,
// then the box of A, the box of B and the image of C.
//
// The CI reads pixels of this scene: tests/fixtures/sprite_shadows_probes.txt says the expected
// colour of each pixel, why, and what the probes prove and what they do not. SCALE 1 IS ASSUMED
// AND DECLARED: the window is 640x480 logical pixels and the scene is in physical pixels
// (renderer_2d.hpp), so a scaled desktop would move it. The capture drivers refuse a capture
// that is not 640x480.
//
// USAGE: sprite_shadows [--frames N]
//   no argument: runs until the system asks to close the window, and exits 0;
//   --frames N (1 to 1000000): stops by itself after N DRAWN frames, through the normal path of
//   the loop. The scene is the same in every frame.
// EXIT: 0 done; 1 a step of the library failed (the line says which) or a frame lost pieces; 2
// bad usage.
// The command line, the ready line, the line of a failed step and the tally of the frames are
// the ones every example shares to run without a person (examples/common/example_harness.hpp);
// this file is the lesson.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

#include <glintfx/core/angle.hpp>
#include <glintfx/core/color.hpp>
#include <glintfx/core/err.hpp>
#include <glintfx/core/image_view.hpp>
#include <glintfx/core/light_2d.hpp>
#include <glintfx/core/projected_shadow.hpp>
#include <glintfx/core/quad.hpp>
#include <glintfx/core/rect.hpp>
#include <glintfx/draw2d/frame_2d_desc.hpp>
#include <glintfx/draw2d/renderer_2d.hpp>
#include <glintfx/draw2d/renderer_2d_desc.hpp>
#include <glintfx/draw2d/texture_2d.hpp>
#include <glintfx/draw2d/texture_2d_desc.hpp>
#include <glintfx/draw2d/texture_region.hpp>
#include <glintfx/platform/gl/context.hpp>
#include <glintfx/platform/loop/loop.hpp>
#include <glintfx/platform/loop/loop_bind.hpp>
#include <glintfx/platform/window/display.hpp>
#include <glintfx/platform/window/window.hpp>

#include "../common/example_harness.hpp"

namespace {

using glintfx_examples::frame_tally;

constexpr std::string_view k_example = "sprite_shadows";

constexpr glintfx::gltfx_window_desc k_window_desc{
    .title = "GlintFx example: sprite shadows",
    .application_id = "org.glintfx.examples.sprite_shadows",
    .logical_size = {.width = 640, .height = 480},
};

constexpr glintfx::gltfx_rgba k_background{.red = 1.0F, .green = 1.0F, .blue = 1.0F, .alpha = 1.0F};
// The colour of every shadow: black, half transparent. The tint is a parameter you must give: the
// look of a shadow belongs to the program that draws it, never to the library.
constexpr glintfx::gltfx_rgba k_shadow_tint{
    .red = 0.0F, .green = 0.0F, .blue = 0.0F, .alpha = 0.5F};
constexpr glintfx::gltfx_rgba k_blue{.red = 0.0F, .green = 0.0F, .blue = 1.0F, .alpha = 1.0F};
constexpr glintfx::gltfx_rgba k_red{.red = 1.0F, .green = 0.0F, .blue = 0.0F, .alpha = 1.0F};

// ---- The pieces: where each one stands on the ground, and how tall it is ----

constexpr glintfx::gltfx_standing_piece_2d k_piece_a{
    .base_start = {300.0, 100.0}, .base_end = {300.0, 160.0}, .height = 120.0};
constexpr glintfx::gltfx_standing_piece_2d k_piece_b{
    .base_start = {360.0, 130.0}, .base_end = {360.0, 190.0}, .height = 120.0};
constexpr glintfx::gltfx_standing_piece_2d k_piece_c{
    .base_start = {100.0, 300.0}, .base_end = {100.0, 360.0}, .height = 80.0};

// The one sun of the scene. The azimuth says where the light comes FROM (core/light_2d.hpp).
glintfx::gltfx_directional_light_2d sun() {
    return {.azimuth = glintfx::gltfx_angle_from_degrees(0.0),
            .elevation = glintfx::gltfx_angle_from_degrees(45.0)};
}

// The shadow of `piece` under the sun, as four corners on the ground. The projection is pure
// geometry: it can refuse a piece (a negative height, say), and the example checks it out loud.
std::optional<glintfx::gltfx_quad_world> shadow_of(const glintfx::gltfx_standing_piece_2d &piece,
                                                   std::string_view step) {
    auto shadow = glintfx::gltfx_project_directional_shadow(sun(), piece);
    if (shadow.has_error()) {
        (void)glintfx_examples::fail(k_example, step, shadow.err());
        return std::nullopt;
    }
    return shadow.value();
}

// ---- The image of C: bytes in memory, four per pixel (red, green, blue, alpha) ----

// The left texel opaque green, the right one fully transparent. Only the alpha makes the shape of a
// shadow: the colour of a texel never reaches it.
constexpr std::array<std::byte, 8> k_sprite_bytes{std::byte{0},   std::byte{255}, std::byte{0},
                                                  std::byte{255}, std::byte{0},   std::byte{0},
                                                  std::byte{0},   std::byte{0}};
constexpr glintfx::gltfx_rect_texel k_whole_sprite{{0.0, 0.0}, {2.0, 1.0}};

using texture = glintfx::gltfx_texture_2d;

// The texture of C, with the NEAREST filter: each texel stays a sharp block, so the two halves do
// not blend into each other.
std::optional<texture> make_sprite(glintfx::gltfx_renderer_2d &renderer) {
    const glintfx::gltfx_image_view image{.pixels = k_sprite_bytes,
                                          .width = 2,
                                          .height = 1,
                                          .bytes_per_row = 8,
                                          .pixel_format =
                                              glintfx::gltfx_pixel_format::rgba8_srgb_straight};
    auto made = renderer.create_texture(image, {.filter = glintfx::gltfx_texture_filter::nearest});
    if (made.has_error()) {
        (void)glintfx_examples::fail(k_example, "texture_sprite", made.err());
        return std::nullopt;
    }
    return std::move(made.value());
}

// Everything the scene needs, made once before the loop: the image and the three shadows.
struct scene_parts {
    texture sprite;
    glintfx::gltfx_quad_world shadow_a{};
    glintfx::gltfx_quad_world shadow_b{};
    glintfx::gltfx_quad_world shadow_c{};
};

// All of them. Each refusal prints its own line, and any refusal ends the run.
std::optional<scene_parts> make_scene(glintfx::gltfx_renderer_2d &renderer) {
    auto sprite = make_sprite(renderer);
    const auto shadow_a = shadow_of(k_piece_a, "shadow_a");
    const auto shadow_b = shadow_of(k_piece_b, "shadow_b");
    const auto shadow_c = shadow_of(k_piece_c, "shadow_c");
    if (!sprite || !shadow_a || !shadow_b || !shadow_c) {
        return std::nullopt;
    }
    return scene_parts{std::move(*sprite), *shadow_a, *shadow_b, *shadow_c};
}

// ---- The lesson: the shadows first, then the pieces on top of them ----

// The shadows of A and B, which cover each other and still darken once; then the shadow of C, in
// the shape of its image. The tint is the same for the three.
void draw_shadows(glintfx::gltfx_renderer_2d &renderer, const scene_parts &parts) {
    renderer.fill_shadow(parts.shadow_a, k_shadow_tint);
    renderer.fill_shadow(parts.shadow_b, k_shadow_tint);
    renderer.draw_shadow_texture(parts.sprite,
                                 {.region = {.source = k_whole_sprite}, .tint = k_shadow_tint},
                                 parts.shadow_c);
}

// The pieces themselves, painted after the shadows: the blue box of A, the red box of B and the
// image of C, each standing on its base.
void draw_pieces(glintfx::gltfx_renderer_2d &renderer, const texture &sprite) {
    renderer.fill_rect({{300.0, 100.0}, {40.0, 60.0}}, k_blue);
    renderer.fill_rect({{360.0, 130.0}, {40.0, 60.0}}, k_red);
    renderer.draw_texture(sprite, {.region = {.source = k_whole_sprite}},
                          {{100.0, 300.0}, {60.0, 60.0}});
}

void draw_scene(glintfx::gltfx_renderer_2d &renderer, const scene_parts &parts) {
    renderer.begin_frame(glintfx::gltfx_frame_2d_desc{.clear_color = k_background});
    draw_shadows(renderer, parts);
    draw_pieces(renderer, parts.sprite);
}

// Everything one run of the loop needs, and the two callbacks the loop calls on it.
struct example_state {
    glintfx::gltfx_renderer_2d &renderer;
    const scene_parts &parts;
    frame_tally tally;

    bool on_frame(const glintfx::gltfx_frame_tick &tick) noexcept;
    void on_render(const glintfx::gltfx_frame_tick & /*tick*/) noexcept;
};

// Runs every tick: prints the ready line once, and ends the loop after the last frame.
bool example_state::on_frame(const glintfx::gltfx_frame_tick &tick) noexcept {
    tally.announce_first_frame(tick);
    return !tally.all_frames_drawn();
}

// Runs only when the loop will present: draws the scene and keeps what the frame reported. A shadow
// the renderer refuses is a piece lost, and a piece lost fails the run (example_harness.hpp).
void example_state::on_render(const glintfx::gltfx_frame_tick & /*tick*/) noexcept {
    ++tally.frames_drawn;
    draw_scene(renderer, parts);
    tally.count_frame(renderer.finish_frame());
}

// ---- Opening and running ----

// The renderer, the scene (made once, before the loop; the texture is destroyed before the
// renderer: a texture does not outlive its renderer, texture_2d.hpp), then the loop.
int run_with_context(glintfx::gltfx_display &display, glintfx::gltfx_window &window,
                     glintfx::gltfx_gl_context &context,
                     std::optional<std::uint64_t> frames_to_draw) {
    auto renderer = glintfx::gltfx_renderer_2d::open(context, glintfx::gltfx_renderer_2d_desc{});
    if (renderer.has_error()) {
        return glintfx_examples::fail(k_example, "renderer", renderer.err());
    }
    const std::optional<scene_parts> parts = make_scene(renderer.value());
    if (!parts) {
        return glintfx_examples::k_exit_failed;
    }
    auto loop = glintfx::gltfx_loop::open(display, window, context);
    if (loop.has_error()) {
        return glintfx_examples::fail(k_example, "loop", loop.err());
    }
    example_state state{.renderer = renderer.value(),
                        .parts = *parts,
                        .tally = {.example = k_example, .frames_to_draw = frames_to_draw}};
    const glintfx::gltfx_rslt<void> ran = loop.value().run(
        glintfx::gltfx_bind_loop_callbacks<&example_state::on_frame, &example_state::on_render>(
            state));
    if (ran.has_error()) {
        return glintfx_examples::fail(k_example, "run", ran.err());
    }
    return state.tally.report();
}

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
    return run_with_context(display.value(), window.value(), context.value(), frames_to_draw);
}

} // namespace

int main(int argc, char **argv) {
    const glintfx_examples::command_line args = glintfx_examples::read_command_line(argc, argv);
    if (!args.valid) {
        return glintfx_examples::print_usage(k_example);
    }
    return run_example(args.frames_to_draw);
}
