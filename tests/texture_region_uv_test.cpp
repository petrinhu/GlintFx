// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <optional>
#include <print>

#include <glintfx/draw2d/texture_region.hpp>

#include "draw2d/texture_region_uv.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// texture_region_uv_test.cpp - R2D-TEXTURE, fatia T4b-A (D-W8-297, D-W8-456): where each corner of
// a drawn piece reads the texture (draw2d/texture_region_uv.hpp). The source of the closed table is
// x 16 to 48 and y 8 to 24 of a texture 64 by 32, so its edges are 0.25 and 0.75 in both
// directions, exact in binary; written by hand from the rule of the header, never read from the
// code. No GL, no window.

using glintfx::gltfx_rect_texel;
using glintfx::gltfx_texture_mirror;
using glintfx::gltfx_texture_region;
using glintfx::draw2d::region_corner_coordinates;
using glintfx::draw2d::texture_coordinate;
using glintfx::draw2d::texture_extent;

namespace {

constexpr gltfx_rect_texel k_source{{16.0, 8.0}, {32.0, 16.0}};
constexpr texture_extent k_texture{64U, 32U};

struct corners_row {
    const char *name = nullptr;
    gltfx_texture_mirror mirror = gltfx_texture_mirror::none;
    // top_left, top_right, bottom_right, bottom_left
    std::array<texture_coordinate, 4> want{};
};

[[nodiscard]] bool same(const std::array<texture_coordinate, 4> &got,
                        const std::array<texture_coordinate, 4> &want) {
    for (std::size_t k = 0; k < 4U; ++k) {
        if (got[k].u != want[k].u || got[k].v != want[k].v) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] gltfx_texture_mirror unknown_mirror() {
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) reason: the cast is the case
    return static_cast<gltfx_texture_mirror>(9);
}

} // namespace

// The source spans u 0.25 to 0.75 and v 0.25 to 0.75 (y 8 to 24 of 32).
GLINTFX_TEST(texture_region_uv_each_mirror_moves_the_corners) {
    const std::array<corners_row, 4> rows{{
        {"sem espelho",
         gltfx_texture_mirror::none,
         {{{0.25, 0.25}, {0.75, 0.25}, {0.75, 0.75}, {0.25, 0.75}}}},
        {"esquerda e direita",
         gltfx_texture_mirror::flip_left_right,
         {{{0.75, 0.25}, {0.25, 0.25}, {0.25, 0.75}, {0.75, 0.75}}}},
        {"cima e baixo",
         gltfx_texture_mirror::flip_top_bottom,
         {{{0.25, 0.75}, {0.75, 0.75}, {0.75, 0.25}, {0.25, 0.25}}}},
        {"os dois",
         gltfx_texture_mirror::flip_left_right_and_top_bottom,
         {{{0.75, 0.75}, {0.25, 0.75}, {0.25, 0.25}, {0.75, 0.25}}}},
    }};
    std::size_t counted = 0;
    std::size_t failed = 0;
    for (const corners_row &row : rows) {
        ++counted;
        const auto got =
            region_corner_coordinates(gltfx_texture_region{k_source, row.mirror}, k_texture);
        if (!got.has_value() || !same(*got, row.want)) {
            ++failed;
            std::println("texture_region_uv_test: FALHOU {}", row.name);
        }
    }
    std::println("texture_region_uv_test: espelhos={} falhas={}", counted, failed);
    GLINTFX_CHECK_EQ(counted, std::size_t{4});
    GLINTFX_CHECK_EQ(failed, std::size_t{0});
}

GLINTFX_TEST(texture_region_uv_the_whole_texture_is_zero_to_one) {
    const auto whole =
        region_corner_coordinates(gltfx_texture_region{gltfx_rect_texel{{0.0, 0.0}, {64.0, 32.0}},
                                                       gltfx_texture_mirror::none},
                                  k_texture);
    GLINTFX_CHECK(whole.has_value());
    if (whole.has_value()) {
        GLINTFX_CHECK((*whole)[0].u == 0.0 && (*whole)[0].v == 0.0);
        GLINTFX_CHECK((*whole)[2].u == 1.0 && (*whole)[2].v == 1.0);
    }
    // A source that starts between two texels keeps its fraction: x 0.5 of 64 is 1/128.
    const auto half_texel = region_corner_coordinates(
        gltfx_texture_region{gltfx_rect_texel{{0.5, 0.0}, {1.0, 1.0}}, gltfx_texture_mirror::none},
        k_texture);
    GLINTFX_CHECK(half_texel.has_value() && (*half_texel)[0].u == 0.0078125);
}

GLINTFX_TEST(texture_region_uv_answers_nothing_it_cannot_say) {
    GLINTFX_CHECK(
        !region_corner_coordinates(gltfx_texture_region{k_source, unknown_mirror()}, k_texture)
             .has_value());
    GLINTFX_CHECK(!region_corner_coordinates(
                       gltfx_texture_region{k_source, gltfx_texture_mirror::none}, {0U, 32U})
                       .has_value());
    GLINTFX_CHECK(!region_corner_coordinates(
                       gltfx_texture_region{k_source, gltfx_texture_mirror::none}, {64U, 0U})
                       .has_value());
}
