// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

#include <glintfx/core/atlas_allocator.hpp>
#include <glintfx/core/err.hpp>

#include "core/atlas_geometry.hpp"
#include "core/atlas_memory.hpp"
#include "core/atlas_slots.hpp"

// core/atlas_allocator_impl.hpp - R2D-TEXTURE. The interior of one gltfx_atlas_allocator
// (core/atlas_allocator.hpp) and the things it does: open, allocate, release, release all and move,
// each checked first (core/atlas_request_check.hpp) and then carried out on the two stores, the
// positions (core/atlas_geometry.hpp) and the receipts (core/atlas_slots.hpp), so that a refused
// call leaves both exactly as they were. All memory, the interior itself included, comes from the
// atlas_memory given to make_atlas_allocator_impl(), which is how atlas_allocator_impl_test arms a
// failure at each request. The public handle (atlas_allocator.cpp) only forwards here. NOT under
// include/glintfx/: internal, not frozen ABI.
namespace glintfx {

struct atlas_allocator_impl {
    atlas_memory memory{};
    atlas_geometry geometry{};
    atlas_slot_table slots{};
    std::uint32_t owner = 0;
};

// The owner number that comes after `last`: one more, and 1 after 4294967295, so 0 is never one.
[[nodiscard]] std::uint32_t atlas_owner_after(std::uint32_t last) noexcept;

// A number for a new allocator: atlas_owner_after() the last one this process handed out, never 0.
// Safe to call from several threads.
[[nodiscard]] std::uint32_t next_atlas_owner() noexcept;

// The interior of open(width, height). Refusals: the size (core/atlas_request_check.hpp), then
// out_of_memory "atlas_open" when the memory refused the interior or the first band.
[[nodiscard]] gltfx_rslt<atlas_allocator_impl *>
make_atlas_allocator_impl(std::uint32_t width, std::uint32_t height, atlas_memory memory) noexcept;

// Gives back everything `impl` holds, and `impl` itself. Harmless on nullptr.
void destroy_atlas_allocator_impl(atlas_allocator_impl *impl) noexcept;

// The move assignment of the public handle: gives back what `target` holds, then `target` takes
// what `source` holds and `source` is left empty (nullptr). When both name the same handle,
// nothing changes.
void move_atlas_allocator_impl(atlas_allocator_impl *&target,
                               atlas_allocator_impl *&source) noexcept;

// allocate(width, height) on `impl` (nullptr for a moved-from allocator).
[[nodiscard]] gltfx_rslt<gltfx_atlas_slot>
allocate_in_atlas(atlas_allocator_impl *impl, std::uint32_t width, std::uint32_t height) noexcept;

// release(slot) on `impl` (nullptr for a moved-from allocator).
[[nodiscard]] gltfx_rslt<void> release_from_atlas(atlas_allocator_impl *impl,
                                                  const gltfx_atlas_slot &slot) noexcept;

// The checks release(slot) makes, in its order and with its names, and nothing freed: ok when
// `slot` is the receipt of a cell in use in `impl` now (nullptr for a moved-from allocator). The
// texture atlas (draw2d/texture_atlas_impl.hpp) reads a receipt through it, so the receipt has one
// definition of "current".
[[nodiscard]] gltfx_rslt<void> check_current_atlas_slot(const atlas_allocator_impl *impl,
                                                        const gltfx_atlas_slot &slot) noexcept;

// release_all() on `impl`; does nothing on nullptr.
void release_everything_in_atlas(atlas_allocator_impl *impl) noexcept;

} // namespace glintfx
