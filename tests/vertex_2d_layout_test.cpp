// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstddef>
#include <type_traits>

#include <glintfx/draw2d/vertex_2d.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// vertex_2d_layout_test.cpp - R2D-SHAPES, S1 (docs/auditoria-api-shapes.md section 2;
// GODS_LAWS.md L-19, L-20, L-26). Proves the frozen layout of the PUBLIC vertex of
// fill_triangles(): gltfx_vertex_2d_world = position (gltfx_vec2_world, two doubles, world
// space) followed by color (gltfx_rgba, four floats, linear light, straight alpha).
//
// THE PUBLIC VERTEX IS NOT THE INTERNAL ONE. The renderer uploads a different vertex,
// batch_vertex (src/draw2d/triangle_batch.hpp), made of floats only (nine since R2D-TEXTURE T4a,
// 36 bytes). This test cannot include anything under src/ (the include list below is the proof of
// that), so it cannot compare the two types by name. It tells them apart by the field types
// instead: the public position is double precision, and batch_vertex has no double at all. The
// sizes once matched (32 bytes each) and may match again, so size alone would NOT tell them apart;
// that is why the checks below read the field types, not sizeof.

static_assert(std::is_aggregate_v<glintfx::gltfx_vertex_2d_world>,
              "gltfx_vertex_2d_world must stay an aggregate: the layout IS the contract");
static_assert(std::is_trivially_copyable_v<glintfx::gltfx_vertex_2d_world>,
              "gltfx_vertex_2d_world must stay trivially copyable: a value, no owned resource");
static_assert(std::is_standard_layout_v<glintfx::gltfx_vertex_2d_world>,
              "gltfx_vertex_2d_world must stay standard layout (fixed field offsets)");

// Field types: the position is the world vector in double precision, and the colour is the
// library's gltfx_rgba. batch_vertex has float fields only, so a double x here is the
// discriminator from the internal vertex.
static_assert(
    std::is_same_v<decltype(glintfx::gltfx_vertex_2d_world::position), glintfx::gltfx_vec2_world>,
    "the public position must be gltfx_vec2_world (world space, double)");
static_assert(std::is_same_v<decltype(glintfx::gltfx_vec2_world::x), double>,
              "the public position must be double precision: batch_vertex is all float, and "
              "this is the difference that keeps the two vertices apart");
static_assert(std::is_same_v<decltype(glintfx::gltfx_vertex_2d_world::color), glintfx::gltfx_rgba>,
              "the public colour must be gltfx_rgba (linear light, straight alpha)");

// Field order and offsets: position first, colour second, and no padding between them.
static_assert(offsetof(glintfx::gltfx_vertex_2d_world, position) == 0,
              "position must stay the first field");
static_assert(offsetof(glintfx::gltfx_vertex_2d_world, color) == 16,
              "color must start right after position (two doubles = 16 bytes)");
static_assert(sizeof(glintfx::gltfx_vertex_2d_world::position) == 2 * sizeof(double),
              "position must stay two doubles: x then y");
static_assert(sizeof(glintfx::gltfx_vertex_2d_world) == 32,
              "the public vertex is 32 bytes: 2 doubles + 4 floats");
static_assert(alignof(glintfx::gltfx_vertex_2d_world) == alignof(double),
              "alignment follows the double position");

GLINTFX_TEST(vertex_2d_world_layout_matches_the_frozen_offsets_at_run_time) {
    // The same facts as the static_asserts above, checked again in the running binary, so a
    // failing layout shows up in the test report and not only as a build error.
    GLINTFX_CHECK_EQ(offsetof(glintfx::gltfx_vertex_2d_world, position), std::size_t{0});
    GLINTFX_CHECK_EQ(offsetof(glintfx::gltfx_vertex_2d_world, color), std::size_t{16});
    GLINTFX_CHECK_EQ(sizeof(glintfx::gltfx_vertex_2d_world), std::size_t{32});
}

GLINTFX_TEST(vertex_2d_world_holds_position_and_colour_by_value) {
    // Values are written by hand, so the test does not copy them from the code under test
    // (memory: "teste que copia o valor da implementação").
    constexpr glintfx::gltfx_vertex_2d_world corner{
        .position = {.x = 12.5, .y = -3.25},
        .color = {.red = 0.25F, .green = 0.5F, .blue = 0.75F, .alpha = 1.0F}};

    const glintfx::gltfx_vertex_2d_world copy = corner;

    GLINTFX_CHECK_EQ(copy.position.x, 12.5);
    GLINTFX_CHECK_EQ(copy.position.y, -3.25);
    GLINTFX_CHECK_EQ(copy.color.red, 0.25F);
    GLINTFX_CHECK_EQ(copy.color.green, 0.5F);
    GLINTFX_CHECK_EQ(copy.color.blue, 0.75F);
    GLINTFX_CHECK_EQ(copy.color.alpha, 1.0F);
}
