// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstddef>
#include <type_traits>

#include <glintfx/core/angle.hpp>
#include <glintfx/core/ellipse.hpp>
#include <glintfx/core/vec2.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// ellipse_test.cpp - R2D-SHAPES, S1 (docs/auditoria-api-shapes.md section 2; decision D-W8-07;
// GODS_LAWS.md L-17, L-19, L-20, L-26). Proves the frozen layout and the default field values
// of gltfx_ellipse_world: centre (gltfx_vec2_world), radius_x, radius_y (double, named so the
// reader sees which one is horizontal), and rotation (gltfx_angle, never a bare double, so
// degrees cannot pass for radians).
//
// The default values are the ones the parecer fixes: centre at the origin, both radii 0.0 (a
// zero radius draws nothing), rotation 0 radians.

static_assert(std::is_aggregate_v<glintfx::gltfx_ellipse_world>,
              "gltfx_ellipse_world must stay an aggregate: the layout IS the contract");
static_assert(std::is_trivially_copyable_v<glintfx::gltfx_ellipse_world>,
              "gltfx_ellipse_world must stay trivially copyable: a value, no owned resource");
static_assert(std::is_standard_layout_v<glintfx::gltfx_ellipse_world>,
              "gltfx_ellipse_world must stay standard layout (fixed field offsets)");

static_assert(
    std::is_same_v<decltype(glintfx::gltfx_ellipse_world::center), glintfx::gltfx_vec2_world>,
    "the centre must be gltfx_vec2_world (world space)");
static_assert(std::is_same_v<decltype(glintfx::gltfx_ellipse_world::radius_x), double>,
              "radius_x must be double, world units");
static_assert(std::is_same_v<decltype(glintfx::gltfx_ellipse_world::radius_y), double>,
              "radius_y must be double, world units");
static_assert(
    std::is_same_v<decltype(glintfx::gltfx_ellipse_world::rotation), glintfx::gltfx_angle>,
    "the rotation must be gltfx_angle, never a bare double (L-19)");

static_assert(offsetof(glintfx::gltfx_ellipse_world, center) == 0,
              "center must stay the first field");
static_assert(offsetof(glintfx::gltfx_ellipse_world, radius_x) == 16,
              "radius_x must follow the centre (two doubles = 16 bytes)");
static_assert(offsetof(glintfx::gltfx_ellipse_world, radius_y) == 24,
              "radius_y must follow radius_x");
static_assert(offsetof(glintfx::gltfx_ellipse_world, rotation) == 32,
              "rotation must follow radius_y");
static_assert(sizeof(glintfx::gltfx_ellipse_world) == 40,
              "the ellipse is 40 bytes: centre 16 + radius_x 8 + radius_y 8 + rotation 8");

GLINTFX_TEST(ellipse_world_layout_matches_the_frozen_offsets_at_run_time) {
    // The static_asserts above, checked again in the running binary (the report shows it).
    GLINTFX_CHECK_EQ(offsetof(glintfx::gltfx_ellipse_world, center), std::size_t{0});
    GLINTFX_CHECK_EQ(offsetof(glintfx::gltfx_ellipse_world, radius_x), std::size_t{16});
    GLINTFX_CHECK_EQ(offsetof(glintfx::gltfx_ellipse_world, radius_y), std::size_t{24});
    GLINTFX_CHECK_EQ(offsetof(glintfx::gltfx_ellipse_world, rotation), std::size_t{32});
    GLINTFX_CHECK_EQ(sizeof(glintfx::gltfx_ellipse_world), std::size_t{40});
}

GLINTFX_TEST(ellipse_world_defaults_are_origin_zero_radii_and_zero_rotation) {
    // Value-initialised with {}: the centre has no initialiser of its own, so it is zeroed by
    // the braces and not read uninitialised (reading a default-initialised centre is UB).
    const glintfx::gltfx_ellipse_world ellipse{};

    GLINTFX_CHECK_EQ(ellipse.center.x, 0.0);
    GLINTFX_CHECK_EQ(ellipse.center.y, 0.0);
    GLINTFX_CHECK_EQ(ellipse.radius_x, 0.0);
    GLINTFX_CHECK_EQ(ellipse.radius_y, 0.0);
    GLINTFX_CHECK_EQ(ellipse.rotation.radians, 0.0);
}

GLINTFX_TEST(ellipse_world_keeps_named_radii_and_rotation_by_value) {
    // Values written by hand (memory: "teste que copia o valor da implementação"). A rotation of
    // pi/2 radians is written as the literal 1.5707963267948966, not computed by the code.
    constexpr glintfx::gltfx_ellipse_world wide{.center = {.x = 100.0, .y = -40.0},
                                                .radius_x = 30.0,
                                                .radius_y = 12.0,
                                                .rotation =
                                                    glintfx::gltfx_angle{1.5707963267948966}};

    const glintfx::gltfx_ellipse_world copy = wide;

    GLINTFX_CHECK_EQ(copy.center.x, 100.0);
    GLINTFX_CHECK_EQ(copy.center.y, -40.0);
    GLINTFX_CHECK_EQ(copy.radius_x, 30.0);
    GLINTFX_CHECK_EQ(copy.radius_y, 12.0);
    GLINTFX_CHECK_EQ(copy.rotation.radians, 1.5707963267948966);
}
