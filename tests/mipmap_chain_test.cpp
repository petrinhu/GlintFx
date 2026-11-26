// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <cstdint>
#include <print>

#include "draw2d/mipmap_chain.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// mipmap_chain_test.cpp - R2D-TEXTURE, fatia T3: where each level lies in the one block that keeps
// every level of a texture (draw2d/mipmap_chain.hpp), as a CLOSED table. The expected offsets are
// sums of max(1, w >> k) * max(1, h >> k) computed by contas_t3b.py (ordem T3, Anexo C) before the
// code, on even and odd sides, a side of one texel, and the largest chain of a 4096 x 4096 texture.

using glintfx::draw2d::chain_texel_count;
using glintfx::draw2d::level_offset;

namespace {

struct chain_row {
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    std::uint32_t level = 0U;
    std::size_t offset = 0U; // level_offset(width, height, level)
    std::size_t total = 0U;  // chain_texel_count(width, height, level)
};

} // namespace

GLINTFX_TEST(mipmap_chain_closed_table_of_offsets_and_sizes) {
    const std::array<chain_row, 9> rows{{
        {5U, 3U, 0U, 0U, 15U},
        {5U, 3U, 1U, 15U, 17U},
        {5U, 3U, 2U, 17U, 18U},
        {7U, 5U, 2U, 41U, 42U},
        {256U, 1U, 1U, 256U, 384U},
        {256U, 1U, 8U, 510U, 511U},
        {1U, 1U, 0U, 0U, 1U},
        {3U, 2U, 1U, 6U, 7U},
        {4096U, 4096U, 12U, 22369620U, 22369621U},
    }};
    std::size_t failed = 0;
    for (const chain_row &row : rows) {
        const std::size_t offset = level_offset(row.width, row.height, row.level);
        const std::size_t total = chain_texel_count(row.width, row.height, row.level);
        if (offset != row.offset || total != row.total) {
            ++failed;
            std::println("mipmap_chain_test: FALHOU {}x{} nivel {}: deslocamento {}, total {}",
                         row.width, row.height, row.level, offset, total);
        }
    }
    std::println("mipmap_chain_test: casos={} falhas={}", rows.size(), failed);
    GLINTFX_CHECK_EQ(failed, std::size_t{0});
}
