// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <print>

#include <glintfx/core/color.hpp>
#include <glintfx/core/transform.hpp>
#include <glintfx/core/vec2.hpp>
#include <glintfx/draw2d/vertex_2d.hpp>

#include "draw2d/quad_vertices.hpp"
#include "draw2d/triangle_pixel_vertices.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// triangle_pixel_vertices_test.cpp - R2D-SHAPES, S2b-fix1 (N1 review of 00cdbb4e, I-8 and M10):
// the pixel vertices of one triangle call, taken out of renderer_2d_impl. Proved here with no
// window and no GL (the third question of GODS_LAWS.md L-17: the atom builds alone):
//   - the vertices are turned by the batch transform, each colour kept as given;
//   - only the last call's vertices are held;
//   - a buffer that cannot grow answers out_of_memory and holds nothing (M10 of the review);
//   - a triangle vertex and a quad corner at the same world position land on the same pixel,
//     compared bit for bit (C-1 of the review), at a position a float cannot hold.

using glintfx::gltfx_rgba;
using glintfx::gltfx_transform;
using glintfx::gltfx_vec2_world;
using glintfx::gltfx_vertex_2d_world;
using glintfx::draw2d::batch_allocator;
using glintfx::draw2d::pixel_affine;
using glintfx::draw2d::pixel_affine_from_transform;
using glintfx::draw2d::quad_corners_world;
using glintfx::draw2d::quad_vertices;
using glintfx::draw2d::triangle_pixel_vertices;
using glintfx::draw2d::triangle_pixels_outcome;

namespace {

bool deny = false;
void *maybe_reallocate(void *block, std::size_t bytes) noexcept {
    return deny ? nullptr : std::realloc(block, bytes);
}
void plain_release(void *block) noexcept { std::free(block); }
constexpr batch_allocator k_test_allocator{&maybe_reallocate, &plain_release};

constexpr gltfx_rgba k_red{1.0F, 0.0F, 0.0F, 1.0F};
constexpr gltfx_rgba k_half_green{0.0F, 1.0F, 0.0F, 0.5F};

[[nodiscard]] pixel_affine scale_two_then_move(double x, double y) {
    return pixel_affine_from_transform(
        gltfx_transform{.translation = {x, y}, .rotation = {0.0}, .scale = {2.0, 2.0}});
}

} // namespace

GLINTFX_TEST(triangle_pixel_vertices_turns_each_vertex_by_the_transform_and_keeps_its_colour) {
    deny = false;
    triangle_pixel_vertices pixels(k_test_allocator);
    const std::array<gltfx_vertex_2d_world, 3> call{gltfx_vertex_2d_world{{1.0, 2.0}, k_red},
                                                    gltfx_vertex_2d_world{{3.0, 4.0}, k_half_green},
                                                    gltfx_vertex_2d_world{{5.0, 6.0}, k_red}};
    GLINTFX_CHECK(pixels.convert(call, scale_two_then_move(10.0, 20.0)) ==
                  triangle_pixels_outcome::converted);
    const auto got = pixels.converted();
    GLINTFX_CHECK_EQ(got.size(), std::size_t{3});
    if (got.size() == 3) {
        GLINTFX_CHECK_EQ(got[0].position.x, 12.0F);
        GLINTFX_CHECK_EQ(got[0].position.y, 24.0F);
        GLINTFX_CHECK_EQ(got[1].position.x, 16.0F);
        GLINTFX_CHECK_EQ(got[2].position.y, 32.0F);
        GLINTFX_CHECK_EQ(got[1].color.green, 1.0F);
        GLINTFX_CHECK_EQ(got[1].color.alpha, 0.5F); // straight, as given: the batch premultiplies
    }
}

GLINTFX_TEST(triangle_pixel_vertices_holds_only_the_last_call) {
    deny = false;
    triangle_pixel_vertices pixels(k_test_allocator);
    const std::array<gltfx_vertex_2d_world, 6> first{
        gltfx_vertex_2d_world{{0.0, 0.0}, k_red}, gltfx_vertex_2d_world{{1.0, 0.0}, k_red},
        gltfx_vertex_2d_world{{0.0, 1.0}, k_red}, gltfx_vertex_2d_world{{2.0, 0.0}, k_red},
        gltfx_vertex_2d_world{{3.0, 0.0}, k_red}, gltfx_vertex_2d_world{{2.0, 1.0}, k_red}};
    const std::array<gltfx_vertex_2d_world, 3> second{gltfx_vertex_2d_world{{7.0, 0.0}, k_red},
                                                      gltfx_vertex_2d_world{{8.0, 0.0}, k_red},
                                                      gltfx_vertex_2d_world{{7.0, 1.0}, k_red}};
    GLINTFX_CHECK(pixels.convert(first, pixel_affine{}) == triangle_pixels_outcome::converted);
    GLINTFX_CHECK(pixels.convert(second, pixel_affine{}) == triangle_pixels_outcome::converted);
    GLINTFX_CHECK_EQ(pixels.converted().size(), std::size_t{3});
    if (!pixels.converted().empty()) {
        GLINTFX_CHECK_EQ(pixels.converted()[0].position.x, 7.0F);
    }
}

// M10 of the N1 review: the buffer cannot grow, the call answers out_of_memory and holds nothing;
// with the memory back, the next call is converted.
GLINTFX_TEST(triangle_pixel_vertices_without_memory_answers_out_of_memory_and_holds_nothing) {
    deny = true;
    triangle_pixel_vertices pixels(k_test_allocator);
    const std::array<gltfx_vertex_2d_world, 3> call{gltfx_vertex_2d_world{{0.0, 0.0}, k_red},
                                                    gltfx_vertex_2d_world{{1.0, 0.0}, k_red},
                                                    gltfx_vertex_2d_world{{0.0, 1.0}, k_red}};
    GLINTFX_CHECK(pixels.convert(call, pixel_affine{}) == triangle_pixels_outcome::out_of_memory);
    GLINTFX_CHECK(pixels.converted().empty());
    deny = false;
    GLINTFX_CHECK(pixels.convert(call, pixel_affine{}) == triangle_pixels_outcome::converted);
    GLINTFX_CHECK_EQ(pixels.converted().size(), std::size_t{3});
}

// C-1 of the N1 review: one arithmetic for both kinds of piece. A camera at x = 16 777 217 (which
// no float holds exactly) looking at a vertex there: the quad corner and the triangle vertex at the
// same world position land on the same float pixel, bit for bit.
GLINTFX_TEST(triangle_pixel_vertices_lands_on_the_same_pixel_as_a_quad_corner) {
    deny = false;
    const pixel_affine camera = pixel_affine_from_transform(
        gltfx_transform{.translation = {-16777217.0, 0.5}, .rotation = {0.3}, .scale = {1.5, 1.5}});
    const gltfx_vec2_world where{16777217.25, 3.75};
    triangle_pixel_vertices pixels(k_test_allocator);
    const std::array<gltfx_vertex_2d_world, 3> call{gltfx_vertex_2d_world{where, k_red},
                                                    gltfx_vertex_2d_world{where, k_red},
                                                    gltfx_vertex_2d_world{where, k_red}};
    GLINTFX_CHECK(pixels.convert(call, camera) == triangle_pixels_outcome::converted);
    const auto corners = quad_vertices(quad_corners_world{where, where, where, where}, camera);
    if (!pixels.converted().empty()) {
        GLINTFX_CHECK_EQ(pixels.converted()[0].position.x, corners[0].x);
        GLINTFX_CHECK_EQ(pixels.converted()[0].position.y, corners[0].y);
    }
}

// S2b-fix1 (I-7 of the N1 review): a vertex whose pixel position is out of the range the vertex
// program can scale is not converted: finite in the world, out of range after the transform. Half
// the largest float is out too (the narrowing alone would allow it). Asked BEFORE any memory, so
// with no memory at all the answer is still the refusal, never out_of_memory.
GLINTFX_TEST(triangle_pixel_vertices_a_pixel_position_out_of_range_is_not_converted) {
    deny = false;
    triangle_pixel_vertices pixels(k_test_allocator);
    const std::array<gltfx_vertex_2d_world, 3> far{gltfx_vertex_2d_world{{0.0, 0.0}, k_red},
                                                   gltfx_vertex_2d_world{{1.0e39, 0.0}, k_red},
                                                   gltfx_vertex_2d_world{{0.0, 1.0}, k_red}};
    GLINTFX_CHECK(pixels.convert(far, pixel_affine{}) ==
                  triangle_pixels_outcome::pixel_position_out_of_range);
    GLINTFX_CHECK(pixels.converted().empty());
    const std::array<gltfx_vertex_2d_world, 3> carried{gltfx_vertex_2d_world{{0.0, 0.0}, k_red},
                                                       gltfx_vertex_2d_world{{1.0, 0.0}, k_red},
                                                       gltfx_vertex_2d_world{{0.0, 1.0e38}, k_red}};
    GLINTFX_CHECK(pixels.convert(carried, scale_two_then_move(0.0, 0.0)) ==
                  triangle_pixels_outcome::pixel_position_out_of_range);
    const std::array<gltfx_vertex_2d_world, 3> half_largest{
        gltfx_vertex_2d_world{{0.0, 0.0}, k_red}, gltfx_vertex_2d_world{{1.0, 0.0}, k_red},
        gltfx_vertex_2d_world{{0.0, 0.5 * std::numeric_limits<float>::max()}, k_red}};
    GLINTFX_CHECK(pixels.convert(half_largest, pixel_affine{}) ==
                  triangle_pixels_outcome::pixel_position_out_of_range);
    GLINTFX_CHECK(pixels.convert(carried, pixel_affine{}) == triangle_pixels_outcome::converted);
    deny = true;
    GLINTFX_CHECK(pixels.convert(far, pixel_affine{}) ==
                  triangle_pixels_outcome::pixel_position_out_of_range);
    deny = false;
}
