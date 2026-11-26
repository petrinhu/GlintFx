// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>

#include <glintfx/core/atlas_allocator.hpp>

#include "core/atlas_memory.hpp"

// core/atlas_geometry.hpp - R2D-TEXTURE, the atlas allocator (core/atlas_allocator.hpp). WHERE a
// rectangle goes: the shelves and free runs of one atlas, the four steps of the placement and the
// joining of what is freed (the public header's "HOW A RECTANGLE IS PLACED" and "RELEASE GIVES THE
// SPACE BACK"). It knows nothing of slots, numbers or refusals by name: the caller checks the
// request first (core/atlas_request_check.hpp) and keeps the receipts (core/atlas_slots.hpp). NOT
// under include/glintfx/: internal, not frozen ABI.
//
// THE LAYOUT. The atlas is a list of runs, sorted by (shelf_y, x). The runs of one shelf share
// shelf_y and shelf_height and cover the atlas width from 0 to `width`, side by side; the shelves
// cover the height from 0 to `height`, one above the other. A FREE BAND is a shelf of exactly one
// run, free and as wide as the atlas; two free bands are never neighbours (they are joined).
namespace glintfx {

struct atlas_run {
    std::uint32_t shelf_y = 0;
    std::uint32_t shelf_height = 0;
    std::uint32_t x = 0;
    std::uint32_t width = 0;
    bool taken = false;
};

struct atlas_geometry {
    atlas_memory memory{};
    atlas_array<atlas_run> runs{};
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

// Where a rectangle can go: the run it lands on, and whether a new shelf is opened at the top of
// that run (a free band) or the rectangle goes on the shelf the run belongs to.
struct atlas_spot {
    std::size_t run = 0;
    bool opens_shelf = false;
};

enum class atlas_place_outcome : std::uint8_t { placed, no_room, no_memory };

struct atlas_placement {
    atlas_place_outcome outcome = atlas_place_outcome::no_room;
    std::uint32_t x = 0;
    std::uint32_t y = 0;
};

// An atlas of `width` by `height` (both nonzero, checked by the caller), one free band. False when
// the memory refused; `geometry` is then left empty.
[[nodiscard]] bool open_atlas_geometry(atlas_geometry &geometry, atlas_memory memory,
                                       std::uint32_t width, std::uint32_t height) noexcept;

// Gives the geometry's memory back.
void close_atlas_geometry(atlas_geometry &geometry) noexcept;

// The spot the four steps choose for `width` by `height`, or found == false when there is none.
// Reads only.
struct atlas_spot_search {
    bool found = false;
    atlas_spot spot{};
};
[[nodiscard]] atlas_spot_search find_atlas_spot(const atlas_geometry &geometry, std::uint32_t width,
                                                std::uint32_t height) noexcept;

// Places `width` by `height` (each at most the atlas size, checked by the caller). no_room and
// no_memory leave the geometry exactly as it was.
[[nodiscard]] atlas_placement place_in_atlas(atlas_geometry &geometry, std::uint32_t width,
                                             std::uint32_t height) noexcept;

// Frees `rect`, which place_in_atlas() returned and which is still taken (checked by the caller,
// core/atlas_slots.hpp), and joins it with its free neighbours. Asks for no memory. A rect that
// does not start a taken run is ignored, and nothing is touched.
void free_in_atlas(atlas_geometry &geometry, const gltfx_atlas_rect &rect) noexcept;

// Makes the whole atlas one free band again. Asks for no memory.
void clear_atlas_geometry(atlas_geometry &geometry) noexcept;

} // namespace glintfx
