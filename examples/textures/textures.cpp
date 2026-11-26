// SPDX-License-Identifier: AGPL-3.0-or-later
// textures.cpp - the GlintFx example of drawing with a texture (R2D-TEXTURE): images made from
// bytes in memory, drawn by the four textured calls of the 2D renderer (draw_texture,
// draw_texture_quad, fill_textured_triangles and fill_indexed_textured_triangles), with the two
// filters, reduced levels, transparency, a tint and an atlas, side by side, in one fixed scene
// drawn in every frame. A living proof, not a product: it uses only the public API, and nothing
// here becomes a rule of the library. Read examples/first_window/ first (the window, the context
// and the loop) and examples/shapes/ (the shapes of one colour).
//
// THE SCENE, in pixels of the buffer (origin at the top-left corner, y grows DOWN; a rectangle is
// its corner and its size). No piece covers another. The background is the colour the renderer
// clears the frame with, (0.2, 0.2, 0.2). The images are bytes in memory: no file is read.
//
//   Row 1, one 4 x 4 image (red at its top left, green at its bottom right, a blue square in the
//   middle, white around), drawn 32 times larger, 128 by 128:
//     A  draw_texture, NEAREST filter, at (20, 20): every texel is a sharp block of one colour, the
//        filter for pixel art. The first texel of the first row of the bytes is at the top left.
//     B  the same image with the LINEAR filter, at (168.5, 20.5): neighbouring texels blend. The
//        blend is done in linear light, so red and white meet in a light pink, not a dark one.
//     C  NEAREST, mirrored left to right (flip_left_right), at (316, 20): the red texel is now at
//        the top right. A mirror is something you ask for, never an accident of the axes.
//     D  NEAREST, tinted white with alpha 0.5, at (464, 20): the whole image half transparent.
//   Row 2, an 8 x 8 checker (white and blue), NEAREST, on the same trapezoid three times: top edge
//   from (45, 0) to (135, 0), bottom edge from (0, 128) to (180, 128), placed at x 20, 220, 420 and
//   y 168:
//     E  draw_texture_quad: read PROJECTIVELY, the lines of the checker stay straight lines.
//     F  fill_textured_triangles, two triangles of three vertices each: each triangle is read
//        AFFINELY, so the checker bends where the two triangles meet, along the diagonal.
//     G  fill_indexed_textured_triangles: the same two triangles from four vertices and six
//        indices; the same picture as F.
//   Row 3, y 316, each piece 64 by 64:
//     H1 at (20.1, 316.1): a 256 x 256 checker of one-texel squares drawn 4 times smaller, NEAREST,
//        WITHOUT reduced levels: each pixel takes one texel, and here they all fall on white. A
//        fine pattern drawn small without reduced levels can come out a wrong colour.
//     H2 at (104, 316): the same checker WITH every reduced level: a smaller level is chosen, and
//        the square comes out the grey the checker averages to.
//     I  at (188.5, 316.5): a 4 x 4 image, LINEAR, opaque red in its 2 x 2 centre and TRANSPARENT
//        around, with white written in the transparent texels. The library multiplies each colour
//        by its alpha, so the white never shows: the red fades into the background, no pale fringe.
//     J1 at (272, 316): the white image of a plain 12 x 4 texture that holds a red, a white and a
//        blue image side by side, LINEAR, drawn 16 times larger: its edge pixels blend in the red
//        and the blue next to it.
//     J2 at (356, 316): the same white image from a gltfx_texture_atlas that holds the same three
//        images: each image gets a margin that copies its own edge, so its edge pixels stay white.
//     K1 at (440, 316) and K2 at (524, 316): one grey given as bytes in two formats, 188 as sRGB
//        bytes and 128 as linear-light bytes. The format says how the bytes are read: the same
//        grey.
//
// The CI reads pixels of this scene: tests/fixtures/textures_probes.txt says the expected colour of
// each pixel, why, and what the probes prove and what they do not. SCALE 1 IS ASSUMED AND
// DECLARED: the window is 640x480 logical pixels and the scene is in physical pixels
// (renderer_2d.hpp), so a scaled desktop would move it. The capture drivers refuse a capture that
// is not 640x480.
//
// USAGE: textures [--frames N]
//   no argument: runs until the system asks to close the window, and exits 0;
//   --frames N (1 to 1000000): stops by itself after N DRAWN frames, through the normal path of the
//   loop. The scene is the same in every frame.
// EXIT: 0 done; 1 a step of the library failed (the line says which) or a frame lost pieces; 2 bad
// usage.
// The command line, the ready line, the line of a failed step and the tally of the frames are the
// ones every example shares to run without a person (examples/common/example_harness.hpp); this
// file is the lesson.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <glintfx/core/atlas_allocator.hpp>
#include <glintfx/core/color.hpp>
#include <glintfx/core/err.hpp>
#include <glintfx/core/image_view.hpp>
#include <glintfx/core/vec2.hpp>
#include <glintfx/draw2d/frame_2d_desc.hpp>
#include <glintfx/draw2d/renderer_2d.hpp>
#include <glintfx/draw2d/renderer_2d_desc.hpp>
#include <glintfx/draw2d/texture_2d.hpp>
#include <glintfx/draw2d/texture_2d_desc.hpp>
#include <glintfx/draw2d/texture_atlas.hpp>
#include <glintfx/draw2d/texture_region.hpp>
#include <glintfx/platform/gl/context.hpp>
#include <glintfx/platform/loop/loop.hpp>
#include <glintfx/platform/loop/loop_bind.hpp>
#include <glintfx/platform/window/display.hpp>
#include <glintfx/platform/window/window.hpp>

#include "../common/example_harness.hpp"

namespace {

using glintfx::gltfx_texture_filter;
using glintfx_examples::frame_tally;

constexpr std::string_view k_example = "textures";

constexpr glintfx::gltfx_window_desc k_window_desc{
    .title = "GlintFx example: textures",
    .application_id = "org.glintfx.examples.textures",
    .logical_size = {.width = 640, .height = 480},
};

constexpr glintfx::gltfx_rgba k_background{.red = 0.2F, .green = 0.2F, .blue = 0.2F, .alpha = 1.0F};

// ---- The images: bytes in memory, four per pixel (red, green, blue, alpha) ----

constexpr glintfx::gltfx_rgba8 k_red{.red = 255, .green = 0, .blue = 0, .alpha = 255};
constexpr glintfx::gltfx_rgba8 k_white{.red = 255, .green = 255, .blue = 255, .alpha = 255};
constexpr glintfx::gltfx_rgba8 k_blue{.red = 0, .green = 0, .blue = 255, .alpha = 255};
constexpr glintfx::gltfx_rgba8 k_green{.red = 0, .green = 255, .blue = 0, .alpha = 255};
constexpr glintfx::gltfx_rgba8 k_black{.red = 0, .green = 0, .blue = 0, .alpha = 255};
// Transparent, with white written in it: what a paint program often leaves around a sprite.
constexpr glintfx::gltfx_rgba8 k_clear_white{.red = 255, .green = 255, .blue = 255, .alpha = 0};

// An image the example owns. view() lends it to a call of the library, which reads the bytes only
// during that call (core/image_view.hpp).
struct image {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    glintfx::gltfx_pixel_format format = glintfx::gltfx_pixel_format::rgba8_srgb_straight;
    std::vector<std::byte> bytes;

    void set(std::uint32_t x, std::uint32_t y, glintfx::gltfx_rgba8 colour) {
        const std::size_t at = (static_cast<std::size_t>(y) * width + x) * 4;
        bytes[at] = std::byte{colour.red};
        bytes[at + 1] = std::byte{colour.green};
        bytes[at + 2] = std::byte{colour.blue};
        bytes[at + 3] = std::byte{colour.alpha};
    }

    [[nodiscard]] glintfx::gltfx_image_view view() const {
        return {.pixels = bytes,
                .width = width,
                .height = height,
                .bytes_per_row = static_cast<std::size_t>(width) * 4,
                .pixel_format = format};
    }
};

// An image of one colour, every row first, then the pattern on top of it.
image blank(std::uint32_t width, std::uint32_t height, glintfx::gltfx_rgba8 colour) {
    image made{.width = width,
               .height = height,
               .bytes = std::vector<std::byte>(static_cast<std::size_t>(width) * height * 4)};
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            made.set(x, y, colour);
        }
    }
    return made;
}

// A, B, C, D: red at the top left, green at the bottom right, a blue 2 x 2 square in the middle.
image pixel_art() {
    image made = blank(4, 4, k_white);
    made.set(0, 0, k_red);
    made.set(3, 3, k_green);
    for (std::uint32_t y = 1; y < 3; ++y) {
        for (std::uint32_t x = 1; x < 3; ++x) {
            made.set(x, y, k_blue);
        }
    }
    return made;
}

// E, F, G and H: `even` where column + row is even, `odd` where it is odd.
image checker(std::uint32_t side, glintfx::gltfx_rgba8 even, glintfx::gltfx_rgba8 odd) {
    image made = blank(side, side, even);
    for (std::uint32_t y = 0; y < side; ++y) {
        for (std::uint32_t x = 0; x < side; ++x) {
            made.set(x, y, (x + y) % 2 == 0 ? even : odd);
        }
    }
    return made;
}

// I: opaque red in the 2 x 2 centre, transparent with white written in it around.
image red_on_clear() {
    image made = blank(4, 4, k_clear_white);
    for (std::uint32_t y = 1; y < 3; ++y) {
        for (std::uint32_t x = 1; x < 3; ++x) {
            made.set(x, y, k_red);
        }
    }
    return made;
}

// J1: three 4 x 4 images, red, white and blue, side by side in one plain image, touching.
image sheet_of_three() {
    image made = blank(12, 4, k_white);
    for (std::uint32_t y = 0; y < 4; ++y) {
        for (std::uint32_t x = 0; x < 4; ++x) {
            made.set(x, y, k_red);
            made.set(x + 8, y, k_blue);
        }
    }
    return made;
}

// K1 and K2: one grey, written in the bytes of `format`.
image grey(std::uint8_t level, glintfx::gltfx_pixel_format format) {
    image made = blank(2, 2, {.red = level, .green = level, .blue = level, .alpha = 255});
    made.format = format;
    return made;
}

// ---- The textures: the filter and the reduced levels are chosen when a texture is made ----

using texture = glintfx::gltfx_texture_2d;

// One texture from `img`; on a refusal, the line of the failed step, and nothing.
std::optional<texture> make_texture(glintfx::gltfx_renderer_2d &renderer, const image &img,
                                    glintfx::gltfx_texture_2d_desc desc, std::string_view step) {
    auto made = renderer.create_texture(img.view(), desc);
    if (made.has_error()) {
        (void)glintfx_examples::fail(k_example, step, made.err());
        return std::nullopt;
    }
    return std::move(made.value());
}

// Every texture of the scene, and the place of the white image in the atlas (J2).
struct scene_textures {
    texture nearest;
    texture linear;
    texture checker;
    texture full_size_only;
    texture reduced;
    texture red_on_clear;
    texture sheet;
    texture grey_srgb;
    texture grey_linear;
    glintfx::gltfx_texture_atlas atlas;
    glintfx::gltfx_rect_texel white_in_atlas;
};

// The atlas of J2: the same three images as the sheet of J1, each written into a cell of its own,
// with a margin of one texel that copies the image's edge. region_of() says where the white one is.
std::optional<std::pair<glintfx::gltfx_texture_atlas, glintfx::gltfx_rect_texel>>
make_atlas(glintfx::gltfx_renderer_2d &renderer) {
    auto atlas = glintfx::gltfx_texture_atlas::open(
        renderer,
        {.width = 32, .height = 32, .margin_pixels = 1, .filter = gltfx_texture_filter::linear});
    if (atlas.has_error()) {
        (void)glintfx_examples::fail(k_example, "atlas", atlas.err());
        return std::nullopt;
    }
    const std::array<image, 3> images{blank(4, 4, k_red), blank(4, 4, k_white),
                                      blank(4, 4, k_blue)};
    std::array<glintfx::gltfx_atlas_slot, 3> slots{};
    for (std::size_t i = 0; i < images.size(); ++i) {
        auto slot = atlas.value().add_image(images[i].view());
        if (slot.has_error()) {
            (void)glintfx_examples::fail(k_example, "atlas_add_image", slot.err());
            return std::nullopt;
        }
        slots[i] = slot.value();
    }
    auto white = atlas.value().region_of(slots[1]);
    if (white.has_error()) {
        (void)glintfx_examples::fail(k_example, "atlas_region_of", white.err());
        return std::nullopt;
    }
    return std::pair{std::move(atlas.value()), white.value()};
}

// All of them, in the order of the scene. Each refusal prints its own line, and any refusal ends
// the run.
std::optional<scene_textures> make_textures(glintfx::gltfx_renderer_2d &renderer) {
    const image art = pixel_art();
    auto nearest =
        make_texture(renderer, art, {.filter = gltfx_texture_filter::nearest}, "texture_nearest");
    auto linear =
        make_texture(renderer, art, {.filter = gltfx_texture_filter::linear}, "texture_linear");
    auto board = make_texture(renderer, checker(8, k_white, k_blue),
                              {.filter = gltfx_texture_filter::nearest}, "texture_checker");
    const image fine = checker(256, k_white, k_black);
    auto full = make_texture(renderer, fine, {.filter = gltfx_texture_filter::nearest},
                             "texture_full_size");
    auto reduced = make_texture(renderer, fine,
                                {.filter = gltfx_texture_filter::nearest,
                                 .mipmap_levels = glintfx::gltfx_all_mipmap_levels},
                                "texture_reduced");
    auto clear = make_texture(renderer, red_on_clear(), {}, "texture_red_on_clear");
    auto sheet = make_texture(renderer, sheet_of_three(), {}, "texture_sheet");
    auto grey_srgb =
        make_texture(renderer, grey(188, glintfx::gltfx_pixel_format::rgba8_srgb_straight),
                     {.filter = gltfx_texture_filter::nearest}, "texture_grey_srgb");
    auto grey_linear =
        make_texture(renderer, grey(128, glintfx::gltfx_pixel_format::rgba8_linear_straight),
                     {.filter = gltfx_texture_filter::nearest}, "texture_grey_linear");
    auto atlas = make_atlas(renderer);
    if (!nearest || !linear || !board || !full || !reduced || !clear || !sheet || !grey_srgb ||
        !grey_linear || !atlas) {
        return std::nullopt;
    }
    return scene_textures{std::move(*nearest),     std::move(*linear),    std::move(*board),
                          std::move(*full),        std::move(*reduced),   std::move(*clear),
                          std::move(*sheet),       std::move(*grey_srgb), std::move(*grey_linear),
                          std::move(atlas->first), atlas->second};
}

// ---- The lesson: the textured calls, one group per function ----

constexpr glintfx::gltfx_rect_texel k_whole_art{{0.0, 0.0}, {4.0, 4.0}};

// Row 1. The same image four times: two filters, a mirror and a tint. The region is the whole
// image; the paint you do not choose is no mirror and an opaque white tint, which leaves the
// texture as it is.
void draw_magnified(glintfx::gltfx_renderer_2d &renderer, const scene_textures &textures) {
    renderer.draw_texture(textures.nearest, {.region = {.source = k_whole_art}},
                          {{20.0, 20.0}, {128.0, 128.0}});
    renderer.draw_texture(textures.linear, {.region = {.source = k_whole_art}},
                          {{168.5, 20.5}, {128.0, 128.0}});
    renderer.draw_texture(textures.nearest,
                          {.region = {.source = k_whole_art,
                                      .mirror = glintfx::gltfx_texture_mirror::flip_left_right}},
                          {{316.0, 20.0}, {128.0, 128.0}});
    renderer.draw_texture(textures.nearest,
                          {.region = {.source = k_whole_art},
                           .tint = {.red = 1.0F, .green = 1.0F, .blue = 1.0F, .alpha = 0.5F}},
                          {{464.0, 20.0}, {128.0, 128.0}});
}

// A corner of the trapezoid of row 2, `x` to the right of where the trapezoid starts, with the
// texel it shows: the checker's top-left corner (0, 0) to its bottom-right corner (8, 8).
glintfx::gltfx_textured_vertex_2d corner(double x, glintfx::gltfx_vec2_world at,
                                         glintfx::gltfx_vec2_texel texel) {
    return {.position = {x + at.x, 168.0 + at.y}, .texel = texel};
}

// Row 2. E maps the whole checker onto the four corners at once; F and G cut the same trapezoid
// into two triangles, and each triangle maps its own three corners.
void draw_trapezoids(glintfx::gltfx_renderer_2d &renderer, const texture &board) {
    renderer.draw_texture_quad(board, {.region = {.source = {{0.0, 0.0}, {8.0, 8.0}}}},
                               {{65.0, 168.0}, {155.0, 168.0}, {200.0, 296.0}, {20.0, 296.0}});
    const std::array<glintfx::gltfx_textured_vertex_2d, 4> f{
        corner(220.0, {45.0, 0.0}, {0.0, 0.0}), corner(220.0, {135.0, 0.0}, {8.0, 0.0}),
        corner(220.0, {180.0, 128.0}, {8.0, 8.0}), corner(220.0, {0.0, 128.0}, {0.0, 8.0})};
    const std::array<glintfx::gltfx_textured_vertex_2d, 6> two_triangles{f[0], f[1], f[2],
                                                                         f[0], f[2], f[3]};
    renderer.fill_textured_triangles(board, two_triangles);
    const std::array<glintfx::gltfx_textured_vertex_2d, 4> g{
        corner(420.0, {45.0, 0.0}, {0.0, 0.0}), corner(420.0, {135.0, 0.0}, {8.0, 0.0}),
        corner(420.0, {180.0, 128.0}, {8.0, 8.0}), corner(420.0, {0.0, 128.0}, {0.0, 8.0})};
    constexpr std::array<std::uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
    renderer.fill_indexed_textured_triangles(board, g, indices);
}

// Row 3, H1 and H2: the fine checker with its full-size level only, then with its reduced
// levels. Every region in this row is in texels of the texture drawn.
void draw_reduced_levels(glintfx::gltfx_renderer_2d &renderer, const scene_textures &textures) {
    constexpr glintfx::gltfx_rect_texel whole_fine{{0.0, 0.0}, {256.0, 256.0}};
    renderer.draw_texture(textures.full_size_only, {.region = {.source = whole_fine}},
                          {{20.1, 316.1}, {64.0, 64.0}});
    renderer.draw_texture(textures.reduced, {.region = {.source = whole_fine}},
                          {{104.0, 316.0}, {64.0, 64.0}});
}

// Row 3, I: the red image on clear texels, whose own written colour never shows.
void draw_transparency(glintfx::gltfx_renderer_2d &renderer, const scene_textures &textures) {
    renderer.draw_texture(textures.red_on_clear, {.region = {.source = k_whole_art}},
                          {{188.5, 316.5}, {64.0, 64.0}});
}

// Row 3, J: J1 takes the white image from the plain sheet, columns 4 to 8; J2 from the atlas,
// where region_of() put it.
void draw_atlas_comparison(glintfx::gltfx_renderer_2d &renderer, const scene_textures &textures) {
    renderer.draw_texture(textures.sheet, {.region = {.source = {{4.0, 0.0}, {4.0, 4.0}}}},
                          {{272.0, 316.0}, {64.0, 64.0}});
    renderer.draw_texture(textures.atlas.texture(), {.region = {.source = textures.white_in_atlas}},
                          {{356.0, 316.0}, {64.0, 64.0}});
}

// Row 3, K: the same grey in two pixel formats.
void draw_grey_formats(glintfx::gltfx_renderer_2d &renderer, const scene_textures &textures) {
    constexpr glintfx::gltfx_rect_texel whole_grey{{0.0, 0.0}, {2.0, 2.0}};
    renderer.draw_texture(textures.grey_srgb, {.region = {.source = whole_grey}},
                          {{440.0, 316.0}, {64.0, 64.0}});
    renderer.draw_texture(textures.grey_linear, {.region = {.source = whole_grey}},
                          {{524.0, 316.0}, {64.0, 64.0}});
}

void draw_scene(glintfx::gltfx_renderer_2d &renderer, const scene_textures &textures) {
    renderer.begin_frame(glintfx::gltfx_frame_2d_desc{.clear_color = k_background});
    draw_magnified(renderer, textures);
    draw_trapezoids(renderer, textures.checker);
    draw_reduced_levels(renderer, textures);
    draw_transparency(renderer, textures);
    draw_atlas_comparison(renderer, textures);
    draw_grey_formats(renderer, textures);
}

// Everything one run of the loop needs, and the two callbacks the loop calls on it.
struct example_state {
    glintfx::gltfx_renderer_2d &renderer;
    const scene_textures &textures;
    frame_tally tally;

    bool on_frame(const glintfx::gltfx_frame_tick &tick) noexcept;
    void on_render(const glintfx::gltfx_frame_tick & /*tick*/) noexcept;
};

// Runs every tick: prints the ready line once, and ends the loop after the last frame.
bool example_state::on_frame(const glintfx::gltfx_frame_tick &tick) noexcept {
    tally.announce_first_frame(tick);
    return !tally.all_frames_drawn();
}

// Runs only when the loop will present: draws the scene and keeps what the frame reported.
void example_state::on_render(const glintfx::gltfx_frame_tick & /*tick*/) noexcept {
    ++tally.frames_drawn;
    draw_scene(renderer, textures);
    tally.count_frame(renderer.finish_frame());
}

// ---- Opening and running ----

// The renderer, the textures (made once, before the loop, and destroyed before the renderer:
// a texture does not outlive its renderer, texture_2d.hpp), then the loop.
int run_with_context(glintfx::gltfx_display &display, glintfx::gltfx_window &window,
                     glintfx::gltfx_gl_context &context,
                     std::optional<std::uint64_t> frames_to_draw) {
    auto renderer = glintfx::gltfx_renderer_2d::open(context, glintfx::gltfx_renderer_2d_desc{});
    if (renderer.has_error()) {
        return glintfx_examples::fail(k_example, "renderer", renderer.err());
    }
    const std::optional<scene_textures> textures = make_textures(renderer.value());
    if (!textures) {
        return glintfx_examples::k_exit_failed;
    }
    auto loop = glintfx::gltfx_loop::open(display, window, context);
    if (loop.has_error()) {
        return glintfx_examples::fail(k_example, "loop", loop.err());
    }
    example_state state{.renderer = renderer.value(),
                        .textures = *textures,
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
