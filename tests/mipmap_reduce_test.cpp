// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <glintfx/core/color.hpp>

#include "draw2d/mipmap_reduce.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// mipmap_reduce_test.cpp - R2D-TEXTURE, fatia T2a: one reduced level made on the CPU, as an area
// average in linear light of premultiplied colour (draw2d/mipmap_reduce.hpp). Every expected byte
// is a literal from the formula of IEC 61966-2-1 in double precision, computed by an independent
// script before it was typed in (the T2a order, item 3), never read back from the code under test.
// Each case says what the wrong reductions would give, so a reader sees what it tells apart.

using glintfx::gltfx_rgba8;
using glintfx::draw2d::reduce_to_next_level;
using glintfx::draw2d::reduced_side;

namespace {

constexpr gltfx_rgba8 k_white{255, 255, 255, 255};
constexpr gltfx_rgba8 k_black{0, 0, 0, 255};
constexpr gltfx_rgba8 k_clear{0, 0, 0, 0};
constexpr gltfx_rgba8 k_sentinel{7, 7, 7, 7};

[[nodiscard]] bool same(gltfx_rgba8 a, gltfx_rgba8 b) {
    return a.red == b.red && a.green == b.green && a.blue == b.blue && a.alpha == b.alpha;
}

[[nodiscard]] gltfx_rgba8 grey(std::uint8_t level) { return gltfx_rgba8{level, level, level, 255}; }

// Reduces `source` and returns the destination, filled with the sentinel first so a texel that
// is never written shows.
[[nodiscard]] std::vector<gltfx_rgba8> reduced(const std::vector<gltfx_rgba8> &source,
                                               std::uint32_t width, std::uint32_t height) {
    std::vector<gltfx_rgba8> destination(
        static_cast<std::size_t>(reduced_side(width, 1)) * reduced_side(height, 1), k_sentinel);
    GLINTFX_CHECK(reduce_to_next_level(source, width, height, destination));
    return destination;
}

} // namespace

// The GL rule for the size of a level: max(1, side >> level).
GLINTFX_TEST(reduced_side_halves_rounding_down_and_never_below_one) {
    GLINTFX_CHECK_EQ(reduced_side(1, 1), 1U);
    GLINTFX_CHECK_EQ(reduced_side(2, 1), 1U);
    GLINTFX_CHECK_EQ(reduced_side(5, 1), 2U);
    GLINTFX_CHECK_EQ(reduced_side(5, 2), 1U);
    GLINTFX_CHECK_EQ(reduced_side(7, 1), 3U);
    GLINTFX_CHECK_EQ(reduced_side(16, 2), 4U);
    GLINTFX_CHECK_EQ(reduced_side(8, 3), 1U);
    GLINTFX_CHECK_EQ(reduced_side(256, 8), 1U);
    GLINTFX_CHECK_EQ(reduced_side(256, 9), 1U);
}

// Grey 188 (linear 0.50289) next to black: the linear average 0.25144 stores as 137.32. Averaging
// the encoded bytes gives 94; averaging them and encoding again gives 163.
GLINTFX_TEST(the_average_is_taken_in_linear_light) {
    const std::vector<gltfx_rgba8> level = reduced({grey(188), k_black}, 2, 1);
    GLINTFX_CHECK(same(level.at(0), grey(137)));
}

// Three opaque white texels and one fully transparent: the premultiplied linear average is 0.75,
// stored as 224.61, and the alpha 191.25 rounds to 191. The encoded average would give 191 for the
// colour too.
GLINTFX_TEST(a_transparent_texel_counts_by_its_coverage) {
    const std::vector<gltfx_rgba8> level = reduced({k_white, k_white, k_white, k_clear}, 2, 2);
    GLINTFX_CHECK(same(level.at(0), gltfx_rgba8{225, 225, 225, 191}));
}

// A side of length 3 becomes 1, and the one texel covers all three: linear 1/3 stores as 156.19.
// A two-texel average that drops the last one gives 188.
GLINTFX_TEST(an_odd_row_averages_every_texel_it_covers) {
    const std::vector<gltfx_rgba8> level = reduced({k_white, k_black, k_black}, 3, 1);
    GLINTFX_CHECK(same(level.at(0), grey(156)));
}

// A side of length 5 becomes 2, and each new texel covers 2.5 old ones: the first takes texels 0
// and 1 whole and half of texel 2. White only at texel 0: 1 / 2.5 = 0.4 stores as 169.62, and the
// second texel stays black.
GLINTFX_TEST(an_odd_row_weighs_a_texel_by_the_part_it_covers) {
    const std::vector<gltfx_rgba8> level =
        reduced({k_white, k_black, k_black, k_black, k_black}, 5, 1);
    GLINTFX_CHECK(same(level.at(0), grey(170)));
    GLINTFX_CHECK(same(level.at(1), k_black));
}

// The same rule down a column: the vertical axis is not a copy of the horizontal one by accident.
GLINTFX_TEST(an_odd_column_averages_every_texel_it_covers) {
    const std::vector<gltfx_rgba8> level = reduced({k_white, k_black, k_black}, 1, 3);
    GLINTFX_CHECK(same(level.at(0), grey(156)));
}

// 4 x 2 becomes 2 x 1: each new texel is its own 2 x 2 block, read from the right rows and columns.
// Left block all grey 188 stays 188; right block three white and one black (linear 0.75) is 225.
GLINTFX_TEST(each_texel_reads_its_own_block) {
    const std::vector<gltfx_rgba8> source = {grey(188), grey(188), k_white, k_black,
                                             grey(188), grey(188), k_white, k_white};
    const std::vector<gltfx_rgba8> level = reduced(source, 4, 2);
    GLINTFX_CHECK(same(level.at(0), grey(188)));
    GLINTFX_CHECK(same(level.at(1), grey(225)));
}

// A destination of the wrong size, or a source that does not hold width x height texels: nothing
// is written.
GLINTFX_TEST(a_storage_of_the_wrong_size_is_not_written) {
    const std::vector<gltfx_rgba8> source = {k_white, k_black};
    std::vector<gltfx_rgba8> too_big(2, k_sentinel);
    GLINTFX_CHECK(!reduce_to_next_level(source, 2, 1, too_big));
    GLINTFX_CHECK(same(too_big.at(0), k_sentinel));
    std::vector<gltfx_rgba8> right(1, k_sentinel);
    GLINTFX_CHECK(!reduce_to_next_level(source, 3, 1, right));
    GLINTFX_CHECK(same(right.at(0), k_sentinel));
}
