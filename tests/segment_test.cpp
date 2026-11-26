// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstddef>
#include <type_traits>

#include <glintfx/core/segment.hpp>
#include <glintfx/core/vec2.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// segment_test.cpp - R2D-SHAPES, fatia S3 (docs/auditoria-api-shapes.md section 2, D-W8-354;
// GODS_LAWS.md L-19, L-26). Proves the frozen layout of gltfx_segment_world, a value type of the
// core whose layout IS the contract: the start, then the end, both gltfx_vec2_world.

static_assert(std::is_aggregate_v<glintfx::gltfx_segment_world>,
              "gltfx_segment_world must stay an aggregate: the layout IS the contract");
static_assert(std::is_trivially_copyable_v<glintfx::gltfx_segment_world>,
              "gltfx_segment_world must stay trivially copyable: a value, no owned resource");
static_assert(std::is_standard_layout_v<glintfx::gltfx_segment_world>,
              "gltfx_segment_world must stay standard layout (fixed field offsets)");
static_assert(
    std::is_same_v<decltype(glintfx::gltfx_segment_world::start), glintfx::gltfx_vec2_world>,
    "the start must be gltfx_vec2_world (world space)");
static_assert(
    std::is_same_v<decltype(glintfx::gltfx_segment_world::end), glintfx::gltfx_vec2_world>,
    "the end must be gltfx_vec2_world (world space)");
static_assert(offsetof(glintfx::gltfx_segment_world, start) == 0,
              "start must stay the first field");
static_assert(offsetof(glintfx::gltfx_segment_world, end) == 16,
              "end must follow the start (two doubles = 16 bytes)");
static_assert(sizeof(glintfx::gltfx_segment_world) == 32,
              "the segment is 32 bytes: start 16 + end 16");

GLINTFX_TEST(segment_keeps_its_two_ends_in_the_order_given) {
    const glintfx::gltfx_segment_world segment{{1.5, -2.0}, {3.0, 4.25}};
    GLINTFX_CHECK_EQ(segment.start.x, 1.5);
    GLINTFX_CHECK_EQ(segment.start.y, -2.0);
    GLINTFX_CHECK_EQ(segment.end.x, 3.0);
    GLINTFX_CHECK_EQ(segment.end.y, 4.25);
}
