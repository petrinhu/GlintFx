// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string_view>

#include <glintfx/core/atlas_allocator.hpp>
#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/draw2d/texture_2d_desc.hpp>
#include <glintfx/draw2d/texture_atlas.hpp>

#include "draw2d/atlas_cell.hpp"
#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// atlas_cell_test.cpp - R2D-TEXTURE, fatia T6-B1 (draw2d/atlas_cell.hpp; GODS_LAWS.md L-17, L-20,
// L-43). The arithmetic of the texture atlas, alone: atlas_cell.cpp is compiled a second time into
// this test (tests/CMakeLists.txt). Every expected value is a literal worked out by hand from
// texture_atlas.hpp, or the formula written below, never a reading of the program's output.
//
// THE RULE OF THE MARGIN, AGAINST THE FILTER (texture_atlas.hpp, MARGIN AND REDUCED LEVELS). The
// oracle is the linear filter of OpenGL 3.3 core, section 3.8.11: on one axis, a sample at u texels
// of level j reads the texels floor(u - 1/2) and floor(u - 1/2) + 1, and texel i of level j covers
// the texels [i * 2^j, (i + 1) * 2^j) of level 0 (the levels are the library's area averages, on
// an atlas whose sides are multiples of 2^k). On one row of the atlas, a neighbour's cell, then the
// cell of an image, placed as the atlas places them (atlas_cell_side()): every sample of the image,
// one each sixteenth of a texel from its first edge to its last, at every level from 0 to k, is
// moved toward each neighbour by just under half a texel of level k (2^(k-1) - 1/64), and must
// still read only texels of its own cell. The criterion is fixed here, before any run (L-43
// global). The two-dimensional filter is this rule on each axis, so one row is the whole question.
// The space is small and closed (GODS_LAWS.md L-17 global): every k from 0 to 4, every neighbour
// width from 1 to 2^(k+1) and every image width from 1 to 2^(k+1) + 1.

namespace {

using glintfx::gltfx_all_mipmap_levels;
using glintfx::gltfx_atlas_rect;
using glintfx::gltfx_err_code;
using glintfx::gltfx_rect_texel;
using glintfx::gltfx_rslt;
using glintfx::gltfx_texture_atlas_desc;
using glintfx::gltfx_texture_filter;
using glintfx::draw2d::atlas_cell_side;
using glintfx::draw2d::atlas_image_region;
using glintfx::draw2d::atlas_image_size;
using glintfx::draw2d::atlas_layout;
using glintfx::draw2d::check_atlas_desc;

[[nodiscard]] gltfx_texture_atlas_desc desc_of(std::uint32_t width, std::uint32_t height,
                                               std::uint32_t margin, std::uint32_t levels) {
    return gltfx_texture_atlas_desc{.width = width,
                                    .height = height,
                                    .margin_pixels = margin,
                                    .filter = gltfx_texture_filter::linear,
                                    .mipmap_levels = levels};
}

[[nodiscard]] bool refused_as(const gltfx_rslt<atlas_layout> &got, std::string_view name) {
    return got.has_error() && got.err().code() == gltfx_err_code::invalid_argument &&
           got.err().rejected_value() == name;
}

// One row of the atlas: the cell [cell_start, cell_end) holds the image from image_first to
// image_last, and the texture has `levels` reduced levels.
struct atlas_row {
    double cell_start = 0.0;
    double cell_end = 0.0;
    double image_first = 0.0;
    double image_last = 0.0;
    std::uint32_t levels = 0;
};

// True when the two texels the linear filter reads at level `level` for a sample at `u` (texels of
// level 0) lie inside the cell (OpenGL 3.3 core, section 3.8.11).
[[nodiscard]] bool reads_inside(double u, std::uint32_t level, const atlas_row &row) {
    const double side = std::ldexp(1.0, static_cast<int>(level));
    const double first = std::floor(u / side - 0.5);
    return first * side >= row.cell_start && (first + 2.0) * side <= row.cell_end;
}

// How many samples of the image, moved `shift` texels toward either neighbour, read a texel outside
// the cell at some level from 0 to row.levels.
[[nodiscard]] int samples_leaving(const atlas_row &row, double shift) {
    const int steps = static_cast<int>((row.image_last - row.image_first) * 16.0);
    int leaving = 0;
    for (int step = 0; step <= steps; ++step) {
        const double u = row.image_first + step / 16.0;
        for (std::uint32_t level = 0; level <= row.levels; ++level) {
            const bool inside =
                reads_inside(u - shift, level, row) && reads_inside(u + shift, level, row);
            leaving += inside ? 0 : 1;
        }
    }
    return leaving;
}

// A sweep: one k, one margin, the cells rounded as the atlas rounds them or not, and the shift.
struct sweep {
    std::uint32_t levels = 0;
    std::uint32_t margin = 0;
    bool rounded = true;
    double shift = 0.0;
};

// The row of a neighbour `neighbour_width` wide and an image `image_width` wide, placed side by
// side: with the atlas's rounding (atlas_cell_side()), or with none (each cell exactly image plus
// two margins), which a cell that did not start on a multiple of 2^k would be.
[[nodiscard]] atlas_row row_of(const sweep &s, std::uint32_t neighbour_width,
                               std::uint32_t image_width) {
    const atlas_layout layout{s.margin, s.levels, 1U << s.levels};
    const double start = s.rounded ? static_cast<double>(atlas_cell_side(neighbour_width, layout))
                                   : neighbour_width + 2.0 * s.margin;
    const double side = s.rounded ? static_cast<double>(atlas_cell_side(image_width, layout))
                                  : image_width + 2.0 * s.margin;
    const double first = start + s.margin;
    return atlas_row{start, start + side, first, first + image_width, s.levels};
}

// How many (neighbour, image) pairs of the sweep have at least one sample leaving the cell.
[[nodiscard]] int pairs_leaving(const sweep &s, int &pairs) {
    const std::uint32_t widest = 2U << s.levels;
    int leaving = 0;
    for (std::uint32_t neighbour = 1; neighbour <= widest; ++neighbour) {
        for (std::uint32_t image = 1; image <= widest + 1U; ++image) {
            leaving += samples_leaving(row_of(s, neighbour, image), s.shift) > 0 ? 1 : 0;
            ++pairs;
        }
    }
    return leaving;
}

// Just under half a texel of level k: 2^(k-1) - 1/64 texels of level 0.
[[nodiscard]] double half_texel_of(std::uint32_t levels) {
    return std::ldexp(1.0, static_cast<int>(levels)) / 2.0 - 1.0 / 64.0;
}

} // namespace

GLINTFX_TEST(atlas_cell_open_refuses_each_field_by_name_in_this_order) {
    gltfx_texture_atlas_desc unknown_filter = desc_of(8, 8, 1, 0);
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) reason: the cast is the case
    unknown_filter.filter = static_cast<gltfx_texture_filter>(7);
    gltfx_texture_atlas_desc filter_before_levels = unknown_filter;
    filter_before_levels.mipmap_levels = 9;
    GLINTFX_CHECK(refused_as(check_atlas_desc(desc_of(0, 0, 1, 0)), "width"));
    GLINTFX_CHECK(refused_as(check_atlas_desc(desc_of(8, 0, 1, 0)), "height"));
    GLINTFX_CHECK(refused_as(check_atlas_desc(unknown_filter), "filter"));
    GLINTFX_CHECK(refused_as(check_atlas_desc(filter_before_levels), "filter"));
    GLINTFX_CHECK(refused_as(check_atlas_desc(desc_of(8, 8, 8, 4)), "mipmap_levels"));
    GLINTFX_CHECK(
        refused_as(check_atlas_desc(desc_of(8, 8, 8, gltfx_all_mipmap_levels)), "mipmap_levels"));
    GLINTFX_CHECK(refused_as(check_atlas_desc(desc_of(8, 8, 1, 1)), "margin_pixels"));
    GLINTFX_CHECK(refused_as(check_atlas_desc(desc_of(8, 8, 4, 0)), "margin_pixels"));
    GLINTFX_CHECK(refused_as(check_atlas_desc(desc_of(18, 16, 4, 2)), "width"));
    GLINTFX_CHECK(refused_as(check_atlas_desc(desc_of(16, 18, 4, 2)), "height"));
    const gltfx_rslt<atlas_layout> accepted = check_atlas_desc(desc_of(16, 16, 4, 2));
    GLINTFX_CHECK(accepted.has_value());
    GLINTFX_CHECK(accepted.value().margin == 4U && accepted.value().reduced_levels == 2U &&
                  accepted.value().alignment == 4U);
}

GLINTFX_TEST(atlas_cell_margin_two_to_the_k_is_accepted_and_one_less_refused_for_every_k) {
    int accepted = 0;
    int refused = 0;
    for (std::uint32_t k = 0; k <= 12U; ++k) {
        const std::uint32_t rule = 1U << k;
        // 16384 texels a side: room for the largest margin (2 * 4096 + 1) and a multiple of 2^12.
        accepted += check_atlas_desc(desc_of(16384, 16384, rule, k)).has_value() ? 1 : 0;
        refused +=
            refused_as(check_atlas_desc(desc_of(16384, 16384, rule - 1U, k)), "margin_pixels") ? 1
                                                                                               : 0;
    }
    std::printf("atlas_cell_test: margem 2^k aceita em %d de 13, 2^k - 1 recusada em %d de 13\n",
                accepted, refused);
    GLINTFX_CHECK(accepted == 13);
    GLINTFX_CHECK(refused == 13);
}

GLINTFX_TEST(atlas_cell_margin_that_leaves_no_room_for_one_texel_is_refused_at_its_border) {
    GLINTFX_CHECK(check_atlas_desc(desc_of(9, 9, 4, 0)).has_value());
    GLINTFX_CHECK(refused_as(check_atlas_desc(desc_of(9, 9, 5, 0)), "margin_pixels"));
    GLINTFX_CHECK(refused_as(check_atlas_desc(desc_of(9, 8, 4, 0)), "margin_pixels"));
}

GLINTFX_TEST(atlas_cell_sides_must_be_a_multiple_of_two_to_the_k_at_the_border) {
    GLINTFX_CHECK(check_atlas_desc(desc_of(808, 800, 8, 3)).has_value());
    GLINTFX_CHECK(refused_as(check_atlas_desc(desc_of(804, 800, 8, 3)), "width"));
    GLINTFX_CHECK(refused_as(check_atlas_desc(desc_of(800, 804, 8, 3)), "height"));
    // With no reduced levels every side is a multiple of 2^0.
    GLINTFX_CHECK(check_atlas_desc(desc_of(801, 799, 1, 0)).has_value());
}

GLINTFX_TEST(atlas_cell_side_is_image_and_margins_rounded_up_and_the_image_sits_margin_in) {
    const atlas_layout two_levels{4U, 2U, 4U};
    GLINTFX_CHECK(atlas_cell_side(5U, two_levels) == 16U);
    GLINTFX_CHECK(atlas_cell_side(1U, two_levels) == 12U);
    GLINTFX_CHECK(atlas_cell_side(8U, two_levels) == 16U);
    GLINTFX_CHECK(atlas_cell_side(3U, atlas_layout{1U, 0U, 1U}) == 5U);
    // The largest side and margin in 64 bits: 2^32 - 1 + 2 * (2^31 - 1) = 2^33 - 3, rounded to a
    // multiple of 2^31: 2^33.
    GLINTFX_CHECK(atlas_cell_side(0xFFFFFFFFU, atlas_layout{0x7FFFFFFFU, 31U, 0x80000000U}) ==
                  0x200000000ULL);
    const gltfx_rect_texel region = atlas_image_region(gltfx_atlas_rect{12U, 0U, 16U, 16U},
                                                       atlas_image_size{8U, 8U}, two_levels);
    GLINTFX_CHECK(region.corner.x == 16.0 && region.corner.y == 4.0);
    GLINTFX_CHECK(region.size.x == 8.0 && region.size.y == 8.0);
}

GLINTFX_TEST(atlas_cell_every_sample_keeps_half_a_texel_of_level_k_with_margin_two_to_the_k) {
    int pairs = 0;
    int leaving = 0;
    for (std::uint32_t k = 0; k <= 4U; ++k) {
        leaving += pairs_leaving(sweep{k, 1U << k, true, half_texel_of(k)}, pairs);
    }
    std::printf("atlas_cell_test: margem 2^k, %d pares conferidos, %d com amostra fora da celula\n",
                pairs, leaving);
    GLINTFX_CHECK(pairs == 2 * 3 + 4 * 5 + 8 * 9 + 16 * 17 + 32 * 33);
    GLINTFX_CHECK(leaving == 0);
}

GLINTFX_TEST(atlas_cell_one_texel_less_of_margin_loses_the_half_texel_for_every_k) {
    std::array<int, 5> leaving{};
    for (std::uint32_t k = 0; k <= 4U; ++k) {
        int pairs = 0;
        leaving[k] = pairs_leaving(sweep{k, (1U << k) - 1U, true, half_texel_of(k)}, pairs);
    }
    std::printf(
        "atlas_cell_test: margem 2^k - 1, pares com amostra fora por k = 0..4: %d %d %d %d %d\n",
        leaving[0], leaving[1], leaving[2], leaving[3], leaving[4]);
    GLINTFX_CHECK(leaving[0] > 0 && leaving[1] > 0 && leaving[2] > 0);
    GLINTFX_CHECK(leaving[3] > 0 && leaving[4] > 0);
}

GLINTFX_TEST(atlas_cell_a_cell_not_on_a_multiple_of_two_to_the_k_reaches_the_neighbour) {
    // No shift at all: the sample itself, on a cell that is not rounded, with margin 2^k.
    std::array<int, 5> leaving{};
    for (std::uint32_t k = 1; k <= 4U; ++k) {
        int pairs = 0;
        leaving[k] = pairs_leaving(sweep{k, 1U << k, false, 0.0}, pairs);
    }
    std::printf("atlas_cell_test: sem arredondamento, pares que alcancam a vizinha por k = 1..4: "
                "%d %d %d %d\n",
                leaving[1], leaving[2], leaving[3], leaving[4]);
    GLINTFX_CHECK(leaving[2] > 0 && leaving[3] > 0 && leaving[4] > 0);
}
