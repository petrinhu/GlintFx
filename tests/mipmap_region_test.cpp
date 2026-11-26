// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <cstdint>
#include <print>
#include <span>
#include <vector>

#include <glintfx/core/color.hpp>

#include "draw2d/mipmap_reduce.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// mipmap_region_test.cpp - R2D-TEXTURE, fatia T3: the reduced texels a change touches
// (draw2d/mipmap_reduce.hpp, targets_touching()) and their recomputation alone
// (reduce_region_to_next_level()). The touched ranges are a CLOSED table computed by brute force
// from the overlap of the intervals (contas_t3b.py, ordem T3, Anexo C), never by the formula of the
// code. The recomputed texels are compared with the WHOLE reduction of the same level
// (reduce_to_next_level(), proved by mipmap_reduce_test): every texel of the region equals it, and
// every texel outside keeps the sentinel.

using glintfx::gltfx_rgba8;
using glintfx::draw2d::covered_range;
using glintfx::draw2d::reduce_region_to_next_level;
using glintfx::draw2d::reduce_to_next_level;
using glintfx::draw2d::reduced_side;
using glintfx::draw2d::source_level;
using glintfx::draw2d::targets_touching;
using glintfx::draw2d::texel_rect;

namespace {

constexpr gltfx_rgba8 k_sentinel{7, 7, 7, 7};

struct touch_row {
    std::uint32_t first = 0U;
    std::uint32_t last = 0U;
    std::uint32_t source_length = 0U;
    std::uint32_t target_length = 0U;
    std::uint32_t touched_first = 0U;
    std::uint32_t touched_last = 0U;
};

// A level of `width` x `height` texels, each a different colour, so a texel read from the wrong
// place shows.
[[nodiscard]] std::vector<gltfx_rgba8> varied(std::uint32_t width, std::uint32_t height) {
    std::vector<gltfx_rgba8> texels;
    texels.reserve(static_cast<std::size_t>(width) * height);
    for (std::uint32_t i = 0; i < width * height; ++i) {
        texels.push_back(gltfx_rgba8{static_cast<std::uint8_t>((i * 37U) % 256U),
                                     static_cast<std::uint8_t>((i * 91U + 13U) % 256U),
                                     static_cast<std::uint8_t>((i * 53U + 101U) % 256U),
                                     static_cast<std::uint8_t>(128U + (i * 17U) % 128U)});
    }
    return texels;
}

[[nodiscard]] bool same(gltfx_rgba8 a, gltfx_rgba8 b) {
    return a.red == b.red && a.green == b.green && a.blue == b.blue && a.alpha == b.alpha;
}

[[nodiscard]] bool inside(const texel_rect &rect, std::uint32_t x, std::uint32_t y) {
    return x >= rect.columns.first && x <= rect.columns.last && y >= rect.rows.first &&
           y <= rect.rows.last;
}

// How many texels of the level below break the rule: inside `rect`, equal to the whole reduction;
// outside, the sentinel.
[[nodiscard]] std::size_t region_mismatches(std::uint32_t width, std::uint32_t height,
                                            const texel_rect &rect) {
    const std::vector<gltfx_rgba8> source = varied(width, height);
    const std::uint32_t below_width = reduced_side(width, 1);
    const std::uint32_t below_height = reduced_side(height, 1);
    const std::size_t below = static_cast<std::size_t>(below_width) * below_height;
    std::vector<gltfx_rgba8> whole(below, k_sentinel);
    std::vector<gltfx_rgba8> region(below, k_sentinel);
    GLINTFX_CHECK(reduce_to_next_level(source, width, height, whole));
    GLINTFX_CHECK(reduce_region_to_next_level(source_level{source, width, height}, region, rect));
    std::size_t wrong = 0;
    for (std::uint32_t y = 0; y < below_height; ++y) {
        for (std::uint32_t x = 0; x < below_width; ++x) {
            const std::size_t at = static_cast<std::size_t>(y) * below_width + x;
            const gltfx_rgba8 want = inside(rect, x, y) ? whole[at] : k_sentinel;
            wrong += same(region[at], want) ? 0U : 1U;
        }
    }
    return wrong;
}

} // namespace

GLINTFX_TEST(mipmap_region_closed_table_of_the_touched_texels) {
    const std::array<touch_row, 14> rows{{
        {0U, 0U, 8U, 4U, 0U, 0U},
        {1U, 1U, 8U, 4U, 0U, 0U},
        {2U, 4U, 8U, 4U, 1U, 2U},
        {7U, 7U, 8U, 4U, 3U, 3U},
        {0U, 0U, 7U, 3U, 0U, 0U},
        {2U, 2U, 7U, 3U, 0U, 1U},
        {3U, 3U, 7U, 3U, 1U, 1U},
        {4U, 4U, 7U, 3U, 1U, 2U},
        {6U, 6U, 7U, 3U, 2U, 2U},
        {1U, 1U, 4U, 2U, 0U, 0U},
        {0U, 4U, 5U, 2U, 0U, 1U},
        {0U, 0U, 1U, 1U, 0U, 0U},
        {2U, 3U, 5U, 2U, 0U, 1U},
        {0U, 0U, 3U, 1U, 0U, 0U},
    }};
    std::size_t failed = 0;
    for (const touch_row &row : rows) {
        const covered_range touched = targets_touching(covered_range{row.first, row.last},
                                                       row.source_length, row.target_length);
        if (touched.first != row.touched_first || touched.last != row.touched_last) {
            ++failed;
            std::println("mipmap_region_test: FALHOU [{}, {}] de {} em {}: [{}, {}]", row.first,
                         row.last, row.source_length, row.target_length, touched.first,
                         touched.last);
        }
    }
    std::println("mipmap_region_test: casos={} falhas={}", rows.size(), failed);
    GLINTFX_CHECK_EQ(failed, std::size_t{0});
}

// An even level (8 x 8 to 4 x 4) and an odd one (7 x 5 to 3 x 2): the region equals the whole
// reduction, and nothing outside it is written.
GLINTFX_TEST(mipmap_region_recomputes_its_texels_as_the_whole_reduction_and_nothing_else) {
    GLINTFX_CHECK_EQ(region_mismatches(8U, 8U, texel_rect{{1U, 2U}, {1U, 2U}}), std::size_t{0});
    GLINTFX_CHECK_EQ(region_mismatches(7U, 5U, texel_rect{{1U, 2U}, {0U, 0U}}), std::size_t{0});
    GLINTFX_CHECK_EQ(region_mismatches(7U, 5U, texel_rect{{0U, 2U}, {0U, 1U}}), std::size_t{0});
}

// A region that reaches past the level below, and storage of the wrong size: false, nothing
// written.
GLINTFX_TEST(mipmap_region_past_the_level_or_of_the_wrong_size_writes_nothing) {
    const std::vector<gltfx_rgba8> source = varied(4U, 4U);
    std::vector<gltfx_rgba8> below(4U, k_sentinel);
    GLINTFX_CHECK(!reduce_region_to_next_level(source_level{source, 4U, 4U}, below,
                                               texel_rect{{0U, 2U}, {0U, 0U}}));
    GLINTFX_CHECK(!reduce_region_to_next_level(source_level{source, 4U, 4U}, below,
                                               texel_rect{{1U, 0U}, {0U, 0U}}));
    std::vector<gltfx_rgba8> wrong_size(3U, k_sentinel);
    GLINTFX_CHECK(!reduce_region_to_next_level(source_level{source, 4U, 4U}, wrong_size,
                                               texel_rect{{0U, 0U}, {0U, 0U}}));
    std::size_t written = 0;
    for (const gltfx_rgba8 texel : below) {
        written += same(texel, k_sentinel) ? 0U : 1U;
    }
    GLINTFX_CHECK_EQ(written, std::size_t{0});
}
