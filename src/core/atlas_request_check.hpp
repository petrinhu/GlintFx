// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

#include <glintfx/core/atlas_allocator.hpp>
#include <glintfx/core/err.hpp>

// core/atlas_request_check.hpp - R2D-TEXTURE, the atlas allocator (core/atlas_allocator.hpp).
// WHICH call is refused, and by which name, before anything is touched: the refusals of the public
// header's "THE REFUSALS" that need no store (the stale slot is the slot table's,
// core/atlas_slots.hpp, and no room and no memory are found while placing). Each is a gltfx_err
// with code invalid_argument and the name of the public header in rejected_value(). Pure: no
// state, no allocation. NOT under include/glintfx/: internal, not frozen ABI.
namespace glintfx {

// What the checks read of one allocator: whether it is open (not moved from), its size and its
// owner number.
struct atlas_identity {
    bool open = false;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t owner = 0;
};

// open(width, height): a zero width ("width"), then a zero height ("height").
[[nodiscard]] gltfx_rslt<void> check_atlas_size(std::uint32_t width, std::uint32_t height) noexcept;

// allocate(width, height): a moved-from allocator ("allocator"), a zero width or height, then a
// width wider or a height taller than the atlas ("width" or "height").
[[nodiscard]] gltfx_rslt<void> check_atlas_request(const atlas_identity &atlas, std::uint32_t width,
                                                   std::uint32_t height) noexcept;

// release(slot): a moved-from allocator ("allocator"), then a slot whose owner is not this
// allocator's: another allocator's ("slot_other_allocator"), or 0, the owner of a slot never handed
// out ("stale_slot"). The other stale slots are the slot table's (core/atlas_slots.hpp).
[[nodiscard]] gltfx_rslt<void> check_atlas_release(const atlas_identity &atlas,
                                                   const gltfx_atlas_slot &slot) noexcept;

} // namespace glintfx
