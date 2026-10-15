// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <print>

#include <glintfx/core/quad.hpp>
#include <glintfx/draw2d/texture_region.hpp>

#include "draw2d/textured_quad_corners.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// textured_quad_corners_test.cpp - R2D-TEXTURE, fatia T4b-B1-fix1 (D-W8-506): where each corner of
// a textured quad reads its texture and with what weight (draw2d/textured_quad_corners.hpp). The
// expected values are by hand: the region x 16 to 48, y 8 to 24 of a 64 by 32 texture reads 0.25 to
// 0.75 in both directions; the trapezoid (0,0) (4,0) (3,2) (1,2) has q = 1.5, 1.5, 0.75, 0.75 (its
// diagonals cross at two thirds of each). No GL, no window.

using glintfx::gltfx_quad_world;
using glintfx::gltfx_rect_texel;
using glintfx::gltfx_texture_mirror;
using glintfx::gltfx_texture_region;
using glintfx::draw2d::quad_texture_corners;
using glintfx::draw2d::texture_extent;
using glintfx::draw2d::textured_quad_corners;

namespace {

constexpr texture_extent k_texture{64U, 32U};
constexpr gltfx_rect_texel k_source{{16.0, 8.0}, {32.0, 16.0}};
constexpr gltfx_quad_world k_trapezoid{{0, 0}, {4, 0}, {3, 2}, {1, 2}};
constexpr gltfx_quad_world k_collinear{{0, 0}, {1, 0}, {2, 0}, {0, 1}};

// The four corners (u, v, q) by role, compared within a millionth.
[[nodiscard]] bool corners_are(const std::optional<quad_texture_corners> &got,
                               const std::array<std::array<float, 3>, 4> &want) {
    if (!got.has_value()) {
        return false;
    }
    int same = 0;
    for (std::size_t i = 0; i < 4U; ++i) {
        const bool close = std::fabs((*got)[i].u - want[i][0]) <= 1e-6F &&
                           std::fabs((*got)[i].v - want[i][1]) <= 1e-6F &&
                           std::fabs((*got)[i].q - want[i][2]) <= 1e-6F;
        same += close ? 1 : 0;
    }
    return same == 4;
}

} // namespace

// A rectangle (projective false): the region by role, q = 1 at every corner, whatever the shape.
GLINTFX_TEST(textured_quad_corners_the_rectangle_reads_by_role_at_q_one) {
    const gltfx_texture_region region{k_source, gltfx_texture_mirror::none};
    GLINTFX_CHECK(corners_are(textured_quad_corners(region, k_texture, k_trapezoid, false),
                              {{{0.25F, 0.25F, 1.0F},
                                {0.75F, 0.25F, 1.0F},
                                {0.75F, 0.75F, 1.0F},
                                {0.25F, 0.75F, 1.0F}}}));
    // the rectangle path asks nothing of the shape: even three corners on a line read the region
    GLINTFX_CHECK(textured_quad_corners(region, k_texture, k_collinear, false).has_value());
}

// The quadrilateral (projective true): the weights of the trapezoid, and the mirror on the region.
GLINTFX_TEST(textured_quad_corners_the_quadrilateral_carries_its_weights_and_mirror) {
    const gltfx_texture_region mirrored{k_source, gltfx_texture_mirror::flip_left_right};
    GLINTFX_CHECK(corners_are(textured_quad_corners(mirrored, k_texture, k_trapezoid, true),
                              {{{0.75F, 0.25F, 1.5F},
                                {0.25F, 0.25F, 1.5F},
                                {0.25F, 0.75F, 0.75F},
                                {0.75F, 0.75F, 0.75F}}}));
    std::println("textured_quad_corners_test: trapezio espelhado conferido");
}

// No corners for a quadrilateral with no projective mapping, nor for a region with no coordinates.
GLINTFX_TEST(textured_quad_corners_answers_nothing_it_cannot_say) {
    const gltfx_texture_region region{k_source, gltfx_texture_mirror::none};
    GLINTFX_CHECK(!textured_quad_corners(region, k_texture, k_collinear, true).has_value());
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) reason: the cast is the case
    const gltfx_texture_region unknown{k_source, static_cast<gltfx_texture_mirror>(9)};
    GLINTFX_CHECK(!textured_quad_corners(unknown, k_texture, k_trapezoid, false).has_value());
    GLINTFX_CHECK(
        !textured_quad_corners(region, texture_extent{0U, 32U}, k_trapezoid, true).has_value());
}
