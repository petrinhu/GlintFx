// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/texture_update_kept.hpp"

#include <algorithm>
#include <cstddef>

#include "draw2d/mipmap_chain.hpp"

// draw2d/texture_update_kept.cpp - R2D-TEXTURE, T3-fix1: the swap of texture_update_kept.hpp. The
// checks come first and cover every refusal, so a refused swap changes nothing; then level 0 is
// exchanged row by row, then each reduced level is recomputed where the level above changed.
namespace glintfx::draw2d {

namespace {

using glintfx::gltfx_rgba8;

// Where one level lies in the block, and its size.
struct level_plane {
    std::size_t offset = 0U;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
};

[[nodiscard]] std::uint32_t side_of(const covered_range &range) noexcept {
    return range.last - range.first + 1U;
}

[[nodiscard]] std::size_t texels_of(const level_plane &plane) noexcept {
    return static_cast<std::size_t>(plane.width) * plane.height;
}

[[nodiscard]] level_plane plane_of(const kept_chain &chain, std::uint32_t level) noexcept {
    return level_plane{
        .offset = level_offset(chain.width, chain.height, level),
        .width = reduced_side(chain.width, level),
        .height = reduced_side(chain.height, level),
    };
}

// True when the block holds exactly the texels of every level, and the levels fit in
// touched_levels.
[[nodiscard]] bool holds_every_level(const kept_chain &chain) noexcept {
    return chain.reduced_levels < k_max_chain_levels &&
           chain.texels.size() ==
               chain_texel_count(chain.width, chain.height, chain.reduced_levels);
}

// True when `range` is not empty and lies inside a side of `side` texels.
[[nodiscard]] bool lies_within(const covered_range &range, std::uint32_t side) noexcept {
    return range.first <= range.last && range.last < side;
}

// True when `rect` lies inside level 0 and `stored` holds exactly its texels.
[[nodiscard]] bool rect_fits(const kept_chain &chain, const texel_rect &rect,
                             std::span<const gltfx_rgba8> stored) noexcept {
    if (!lies_within(rect.columns, chain.width) || !lies_within(rect.rows, chain.height)) {
        return false;
    }
    return static_cast<std::size_t>(side_of(rect.columns)) * side_of(rect.rows) == stored.size();
}

// The texels of `rect` in level 0 and those of `stored` change places, row by row.
void exchange_level_zero(const kept_chain &chain, const texel_rect &rect,
                         std::span<gltfx_rgba8> stored) noexcept {
    const std::size_t width = side_of(rect.columns);
    for (std::uint32_t row = rect.rows.first; row <= rect.rows.last; ++row) {
        const std::size_t from = static_cast<std::size_t>(row - rect.rows.first) * width;
        const std::size_t to = static_cast<std::size_t>(row) * chain.width + rect.columns.first;
        std::swap_ranges(stored.begin() + static_cast<std::ptrdiff_t>(from),
                         stored.begin() + static_cast<std::ptrdiff_t>(from + width),
                         chain.texels.begin() + static_cast<std::ptrdiff_t>(to));
    }
}

// The rectangle of level `level` that the rectangle of the level above touches turns `rect` into
// that rectangle (targets_touching(), both axes), and the texels in it are recomputed in the block
// from the level above (reduce_region_to_next_level()). False when the block does not fit.
[[nodiscard]] bool reduce_touched(const kept_chain &chain, std::uint32_t level,
                                  texel_rect &rect) noexcept {
    const level_plane above = plane_of(chain, level - 1U);
    const level_plane here = plane_of(chain, level);
    rect = texel_rect{
        .columns = targets_touching(rect.columns, above.width, here.width),
        .rows = targets_touching(rect.rows, above.height, here.height),
    };
    const source_level source{
        .texels =
            std::span<const gltfx_rgba8>(chain.texels).subspan(above.offset, texels_of(above)),
        .width = above.width,
        .height = above.height,
    };
    return reduce_region_to_next_level(source, chain.texels.subspan(here.offset, texels_of(here)),
                                       rect);
}

// Every reduced level, from the first to the last, recomputed where the level above changed; the
// rectangle of each level goes into `touched`.
[[nodiscard]] bool reduce_touched_levels(const kept_chain &chain, texel_rect rect,
                                         touched_levels &touched) noexcept {
    touched.count = chain.reduced_levels + 1U;
    touched.rects[0] = rect;
    for (std::uint32_t level = 1U; level <= chain.reduced_levels; ++level) {
        if (!reduce_touched(chain, level, rect)) {
            return false;
        }
        touched.rects[level] = rect;
    }
    return true;
}

} // namespace

bool swap_update(const kept_chain &chain, const texel_rect &rect, std::span<gltfx_rgba8> stored,
                 touched_levels &touched) noexcept {
    if (!holds_every_level(chain) || !rect_fits(chain, rect, stored)) {
        return false;
    }
    exchange_level_zero(chain, rect, stored);
    return reduce_touched_levels(chain, rect, touched);
}

} // namespace glintfx::draw2d
