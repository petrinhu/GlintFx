// SPDX-License-Identifier: AGPL-3.0-or-later
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <glintfx/core/color.hpp>

#include "draw2d/mipmap_chain.hpp"
#include "draw2d/mipmap_reduce.hpp"
#include "draw2d/texture_update_kept.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// texture_update_kept_test.cpp - R2D-TEXTURE, T3-fix1: the kept texels of an updatable texture
// with reduced levels taking an update and giving it back (draw2d/texture_update_kept.hpp), with no
// GL and no texture record: a chain is made here, level by level, by reduce_to_next_level()
// (mipmap_reduce_test proves it).
//
// THE ORACLES ARE NOT THIS CODE: the touched rectangles are the brute-force values of
// contas_t3fix1.py (ordem T3-fix1, Anexo C), from the definition of the overlap of two texels; the
// chain after a swap must be the chain MADE from the new level 0; and the chain after the same swap
// twice must be, byte for byte, the chain before.

using glintfx::gltfx_rgba8;
using glintfx::draw2d::chain_texel_count;
using glintfx::draw2d::k_max_chain_levels;
using glintfx::draw2d::kept_chain;
using glintfx::draw2d::level_offset;
using glintfx::draw2d::reduce_to_next_level;
using glintfx::draw2d::reduced_side;
using glintfx::draw2d::swap_update;
using glintfx::draw2d::texel_rect;
using glintfx::draw2d::touched_levels;

namespace {

// `width` x `height` texels, each a different colour (alpha too), so a texel from the wrong place
// shows.
[[nodiscard]] std::vector<gltfx_rgba8> varied(std::uint32_t width, std::uint32_t height,
                                              std::uint32_t seed) {
    std::vector<gltfx_rgba8> texels;
    for (std::uint32_t i = 0; i < width * height; ++i) {
        const std::uint32_t v = (i + seed) * 2654435761U;
        texels.push_back(gltfx_rgba8{
            static_cast<std::uint8_t>(v >> 24U), static_cast<std::uint8_t>(v >> 16U),
            static_cast<std::uint8_t>(v >> 8U), static_cast<std::uint8_t>(64U + (v % 192U))});
    }
    return texels;
}

// The whole chain of `level0` (width x height), levels 0 to `levels`, in the layout of
// mipmap_chain.hpp, each level reduced from the one above.
[[nodiscard]] std::vector<gltfx_rgba8> chain_made_from(const std::vector<gltfx_rgba8> &level0,
                                                       std::uint32_t width, std::uint32_t height,
                                                       std::uint32_t levels) {
    std::vector<gltfx_rgba8> chain(chain_texel_count(width, height, levels));
    std::copy(level0.begin(), level0.end(), chain.begin());
    for (std::uint32_t k = 1U; k <= levels; ++k) {
        const std::size_t above = level_offset(width, height, k - 1U);
        const std::size_t here = level_offset(width, height, k);
        const std::span<const gltfx_rgba8> source =
            std::span<const gltfx_rgba8>(chain).subspan(above, here - above);
        const std::size_t size =
            static_cast<std::size_t>(reduced_side(width, k)) * reduced_side(height, k);
        GLINTFX_CHECK(reduce_to_next_level(source, reduced_side(width, k - 1U),
                                           reduced_side(height, k - 1U),
                                           std::span<gltfx_rgba8>(chain).subspan(here, size)));
    }
    return chain;
}

// `base` with the texels of `patch` pasted at `at`.
[[nodiscard]] std::vector<gltfx_rgba8> pasted(std::vector<gltfx_rgba8> base, std::uint32_t width,
                                              const std::vector<gltfx_rgba8> &patch,
                                              const texel_rect &at) {
    const std::uint32_t patch_width = at.columns.last - at.columns.first + 1U;
    for (std::uint32_t row = at.rows.first; row <= at.rows.last; ++row) {
        for (std::uint32_t column = at.columns.first; column <= at.columns.last; ++column) {
            base[row * width + column] =
                patch[(row - at.rows.first) * patch_width + (column - at.columns.first)];
        }
    }
    return base;
}

[[nodiscard]] bool same_texels(std::span<const gltfx_rgba8> a, std::span<const gltfx_rgba8> b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].red != b[i].red || a[i].green != b[i].green || a[i].blue != b[i].blue ||
            a[i].alpha != b[i].alpha) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool same_rect(const texel_rect &rect, std::uint32_t x, std::uint32_t y,
                             std::uint32_t width, std::uint32_t height) {
    return rect.columns.first == x && rect.rows.first == y &&
           rect.columns.last - rect.columns.first + 1U == width &&
           rect.rows.last - rect.rows.first + 1U == height;
}

// One texture of the cases: its size, its reduced levels, and the rectangle of the update.
struct swap_case {
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    std::uint32_t levels = 0U;
    texel_rect at{};
};

[[nodiscard]] std::uint32_t patch_width(const swap_case &c) {
    return c.at.columns.last - c.at.columns.first + 1U;
}

[[nodiscard]] std::uint32_t patch_height(const swap_case &c) {
    return c.at.rows.last - c.at.rows.first + 1U;
}

// The touched rectangles of one swap, on a chain made from varied texels.
[[nodiscard]] touched_levels touched_by(const swap_case &c) {
    std::vector<gltfx_rgba8> chain =
        chain_made_from(varied(c.width, c.height, 1U), c.width, c.height, c.levels);
    std::vector<gltfx_rgba8> stored = varied(patch_width(c), patch_height(c), 9U);
    touched_levels touched{};
    GLINTFX_CHECK(
        swap_update(kept_chain{chain, c.width, c.height, c.levels}, c.at, stored, touched));
    return touched;
}

} // namespace

// contas_t3fix1.py, section 1: a rectangle taller than wide on a square texture (the reduced
// rectangles are not square, so columns and rows taken for each other show), then a texture wider
// than tall and one taller than wide (each axis reduced by its own side).
GLINTFX_TEST(texture_update_kept_each_level_touches_the_brute_force_rectangle) {
    const touched_levels tall = touched_by(swap_case{8U, 8U, 3U, texel_rect{{2U, 2U}, {0U, 3U}}});
    GLINTFX_CHECK_EQ(tall.count, 4U);
    GLINTFX_CHECK(same_rect(tall.rects[0], 2U, 0U, 1U, 4U));
    GLINTFX_CHECK(same_rect(tall.rects[1], 1U, 0U, 1U, 2U));
    GLINTFX_CHECK(same_rect(tall.rects[2], 0U, 0U, 1U, 1U));
    GLINTFX_CHECK(same_rect(tall.rects[3], 0U, 0U, 1U, 1U));
    const touched_levels wide = touched_by(swap_case{12U, 6U, 2U, texel_rect{{3U, 7U}, {1U, 2U}}});
    GLINTFX_CHECK_EQ(wide.count, 3U);
    GLINTFX_CHECK(same_rect(wide.rects[1], 1U, 0U, 3U, 2U));
    GLINTFX_CHECK(same_rect(wide.rects[2], 0U, 0U, 2U, 1U));
    const touched_levels high = touched_by(swap_case{6U, 12U, 2U, texel_rect{{1U, 2U}, {3U, 7U}}});
    GLINTFX_CHECK_EQ(high.count, 3U);
    GLINTFX_CHECK(same_rect(high.rects[1], 0U, 1U, 2U, 3U));
    GLINTFX_CHECK(same_rect(high.rects[2], 0U, 0U, 1U, 2U));
}

// After the swap the chain is the chain MADE from the new level 0, and the update's buffer holds
// what level 0 had in the rectangle (rows 1 and 2, columns 3 to 7, of a level 12 texels wide).
GLINTFX_TEST(texture_update_kept_the_chain_becomes_the_chain_of_the_new_level_0) {
    const swap_case c{12U, 6U, 2U, texel_rect{{3U, 7U}, {1U, 2U}}};
    const std::vector<gltfx_rgba8> before = varied(c.width, c.height, 1U);
    const std::vector<gltfx_rgba8> patch = varied(patch_width(c), patch_height(c), 9U);
    std::vector<gltfx_rgba8> chain = chain_made_from(before, c.width, c.height, c.levels);
    std::vector<gltfx_rgba8> stored = patch;
    touched_levels touched{};
    GLINTFX_CHECK(
        swap_update(kept_chain{chain, c.width, c.height, c.levels}, c.at, stored, touched));
    GLINTFX_CHECK(same_texels(
        chain, chain_made_from(pasted(before, c.width, patch, c.at), c.width, c.height, c.levels)));
    const std::vector<gltfx_rgba8> old_rect = {before[15], before[16], before[17], before[18],
                                               before[19], before[27], before[28], before[29],
                                               before[30], before[31]};
    GLINTFX_CHECK(same_texels(stored, old_rect));
}

// The same swap twice gives the chain back byte for byte, and the buffer its update: this is how a
// refused writing leaves no refused texel in the library's memory (IMPORTANTE-2 of the N1 review).
GLINTFX_TEST(texture_update_kept_the_same_swap_twice_gives_everything_back) {
    const swap_case c{8U, 8U, 3U, texel_rect{{2U, 4U}, {3U, 4U}}};
    const std::vector<gltfx_rgba8> original =
        chain_made_from(varied(c.width, c.height, 1U), c.width, c.height, c.levels);
    const std::vector<gltfx_rgba8> patch = varied(patch_width(c), patch_height(c), 9U);
    std::vector<gltfx_rgba8> chain = original;
    std::vector<gltfx_rgba8> stored = patch;
    touched_levels touched{};
    const kept_chain kept{chain, c.width, c.height, c.levels};
    GLINTFX_CHECK(swap_update(kept, c.at, stored, touched));
    GLINTFX_CHECK(!same_texels(chain, original));
    GLINTFX_CHECK(swap_update(kept, c.at, stored, touched));
    GLINTFX_CHECK(same_texels(chain, original));
    GLINTFX_CHECK(same_texels(stored, patch));
}

// Every refusal is decided before anything changes: a chain one texel short, k_max_chain_levels
// reduced levels (a chain of that very size), a rectangle past level 0, an empty one, and a buffer
// of the wrong size.
GLINTFX_TEST(texture_update_kept_a_refused_swap_changes_nothing) {
    const std::vector<gltfx_rgba8> original = chain_made_from(varied(8U, 8U, 1U), 8U, 8U, 3U);
    std::vector<gltfx_rgba8> chain = original;
    std::vector<gltfx_rgba8> too_long =
        chain_made_from(varied(8U, 8U, 1U), 8U, 8U, k_max_chain_levels);
    const std::vector<gltfx_rgba8> patch = varied(3U, 2U, 9U);
    std::vector<gltfx_rgba8> stored = patch;
    touched_levels touched{};
    const texel_rect fits{{2U, 4U}, {3U, 4U}};
    const std::span<gltfx_rgba8> all(chain);
    GLINTFX_CHECK(
        !swap_update(kept_chain{all.first(all.size() - 1U), 8U, 8U, 3U}, fits, stored, touched));
    GLINTFX_CHECK(
        !swap_update(kept_chain{too_long, 8U, 8U, k_max_chain_levels}, fits, stored, touched));
    GLINTFX_CHECK(
        !swap_update(kept_chain{all, 8U, 8U, 3U}, texel_rect{{6U, 8U}, {3U, 4U}}, stored, touched));
    GLINTFX_CHECK(
        !swap_update(kept_chain{all, 8U, 8U, 3U}, texel_rect{{4U, 2U}, {3U, 4U}}, stored, touched));
    GLINTFX_CHECK(
        !swap_update(kept_chain{all, 8U, 8U, 3U}, texel_rect{{2U, 4U}, {3U, 5U}}, stored, touched));
    GLINTFX_CHECK(same_texels(chain, original));
    GLINTFX_CHECK(same_texels(stored, patch));
}
