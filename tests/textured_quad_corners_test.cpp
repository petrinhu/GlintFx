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
//
// THE WEIGHTS THE VERTEX CARRIES (T4b-A-FLOAT, D-W8-716 to D-W8-735): the kite (0,0) (a,-1) (2,0)
// (a,1) has its diagonals crossing at (a, 0), the fraction a/2 of the first and 1/2 of the second,
// so its exact weights are 1/(2 - a), 1, 1/a and 1. With a = 3 times a power of two, the largest
// weight 1/a is a third of a power of two, far from the exponents the rule compares, so the
// expected values below are decided by hand: the largest weight, 2^E/3, below 2^32 stays as it is;
// from 2^32 up to below 2^63 the four are multiplied by the one power of two that brings the
// largest to 2^33/3; from 2^63 on the quad has no corners.

using glintfx::gltfx_quad_world;
using glintfx::gltfx_rect_texel;
using glintfx::gltfx_texture_mirror;
using glintfx::gltfx_texture_region;
using glintfx::draw2d::quad_texture_corner;
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

// The kite whose diagonals cross at the fraction a/2 of the first and 1/2 of the second.
[[nodiscard]] gltfx_quad_world kite_crossing_at(double a) {
    return gltfx_quad_world{{0.0, 0.0}, {a, -1.0}, {2.0, 0.0}, {a, 1.0}};
}

// A kite and the weights its corners must carry: the exact ones times 2^shift.
struct carried_kite {
    double a = 0.0;
    int shift = 0;
};

// True when each corner of `got` reads the region of k_source by role: 0.25 to 0.75 in both
// directions.
[[nodiscard]] bool reads_by_role(const quad_texture_corners &got) {
    const std::array<std::array<float, 2>, 4> read{
        {{0.25F, 0.25F}, {0.75F, 0.25F}, {0.75F, 0.75F}, {0.25F, 0.75F}}};
    int right = 0;
    for (std::size_t i = 0; i < 4U; ++i) {
        right += got[i].u == read[i][0] && got[i].v == read[i][1] ? 1 : 0;
    }
    return right == 4;
}

// True when each corner of `got` weighs the kite's exact weight times 2^shift, within 2^-23.
[[nodiscard]] bool weighs_as_carried(const quad_texture_corners &got, carried_kite kite) {
    const std::array<double, 4> exact{1.0 / (2.0 - kite.a), 1.0, 1.0 / kite.a, 1.0};
    int right = 0;
    for (std::size_t i = 0; i < 4U; ++i) {
        const double want = std::ldexp(exact[i], kite.shift);
        right += std::fabs(got[i].q - want) <= std::ldexp(want, -23) ? 1 : 0;
    }
    return right == 4;
}

// True when every product of a texture coordinate by its weight that the vertex will store
// (u q, v q; triangle_batch.cpp) is a normal float.
[[nodiscard]] bool stores_normal_floats(const quad_texture_corners &got) {
    int right = 0;
    for (const quad_texture_corner &corner : got) {
        right += std::isnormal(corner.u * corner.q) && std::isnormal(corner.v * corner.q) ? 1 : 0;
    }
    return right == 4;
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

// T4b-A-FLOAT: the largest weight below 2^32 reaches the vertex as it is; from 2^32 up to below
// 2^63 the four weights are multiplied by one power of two, the largest landing at 2^33/3.
GLINTFX_TEST(textured_quad_corners_a_large_weight_is_carried_with_the_others) {
    const gltfx_texture_region region{k_source, gltfx_texture_mirror::none};
    // largest weight 2^33/3 (exponent 31), 2^41/3 (39) and 2^64/3 (62)
    constexpr std::array<carried_kite, 3> kites{
        {{0x1.8p-32, 0}, {0x1.8p-40, -8}, {0x1.8p-63, -31}}};
    std::size_t right = 0;
    for (const carried_kite &kite : kites) {
        const std::optional<quad_texture_corners> got =
            textured_quad_corners(region, k_texture, kite_crossing_at(kite.a), true);
        const bool carried = got.has_value() && reads_by_role(*got) &&
                             weighs_as_carried(*got, kite) && stores_normal_floats(*got);
        right += carried ? 1U : 0U;
    }
    std::println("textured_quad_corners_test: pesos levados pipas={} certas={}", kites.size(),
                 right);
    GLINTFX_CHECK_EQ(right, kites.size());
}

// T4b-A-FLOAT: a largest weight of 2^63 or more gives no corners (degenerate_quad for the caller):
// 2^65/3, 2^201/3 (past the largest float, the inf the review of T4b-A-PHRASE found) and 2^1030
// (past the largest double, which projective_quad.hpp already refuses).
GLINTFX_TEST(textured_quad_corners_a_weight_of_two_to_the_63_or_more_has_no_corners) {
    const gltfx_texture_region region{k_source, gltfx_texture_mirror::none};
    constexpr std::array<double, 3> crossings{0x1.8p-64, 0x1.8p-200, 0x1p-1030};
    std::size_t refused = 0;
    for (const double a : crossings) {
        refused += textured_quad_corners(region, k_texture, kite_crossing_at(a), true).has_value()
                       ? 0U
                       : 1U;
    }
    std::println("textured_quad_corners_test: pesos recusados pipas={} recusadas={}",
                 crossings.size(), refused);
    GLINTFX_CHECK_EQ(refused, crossings.size());
}
