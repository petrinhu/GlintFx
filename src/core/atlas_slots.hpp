// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>

#include <glintfx/core/atlas_allocator.hpp>

#include "core/atlas_memory.hpp"

// core/atlas_slots.hpp - R2D-TEXTURE, the atlas allocator (core/atlas_allocator.hpp). WHICH receipt
// is current: the numbers of the cells, the generation of each and the numbers free to be handed
// out again (the public header's "A SLOT IS A RECEIPT"). It knows nothing of positions: the rect
// of each cell is kept only to compare it with the rect of a slot. NOT under include/glintfx/:
// internal, not frozen ABI.
//
// A number is the index of its entry. A new number starts at generation 1, so the default slot
// (generation 0) is never current. A released number goes on the free list (the last released is
// handed out first), and when it is handed out again its generation grows by one; a number
// released at generation k_atlas_last_generation is RETIRED: it never goes back on the free list.
namespace glintfx {

inline constexpr std::uint32_t k_atlas_last_generation = 0xFFFFFFFFU;
inline constexpr std::uint32_t k_atlas_no_number = 0xFFFFFFFFU;

struct atlas_slot_entry {
    gltfx_atlas_rect rect{};
    std::uint32_t generation = 0;
    std::uint32_t next_free = k_atlas_no_number;
    bool in_use = false;
};

struct atlas_slot_table {
    atlas_memory memory{};
    atlas_array<atlas_slot_entry> entries{};
    std::uint32_t free_head = k_atlas_no_number;
};

// Makes sure one number can be handed out without asking for memory: a free number, or room for a
// new entry. False when the memory refused or every number below k_atlas_no_number is taken; the
// table is then untouched.
[[nodiscard]] bool reserve_atlas_number(atlas_slot_table &table) noexcept;

// Hands out a number for `rect` and returns its slot with `owner` written in it. Precondition:
// reserve_atlas_number() returned true since the last hand-out.
[[nodiscard]] gltfx_atlas_slot hand_out_atlas_number(atlas_slot_table &table,
                                                     const gltfx_atlas_rect &rect,
                                                     std::uint32_t owner) noexcept;

// True when `slot` names a number in use now, with its generation and its rect. The owner is not
// read here (core/atlas_request_check.hpp reads it).
[[nodiscard]] bool is_current_atlas_slot(const atlas_slot_table &table,
                                         const gltfx_atlas_slot &slot) noexcept;

// Takes the number `id`, which is in use, back: on the free list, or retired at the last
// generation. Asks for no memory.
void take_back_atlas_number(atlas_slot_table &table, std::uint32_t id) noexcept;

// Takes every number in use back, as take_back_atlas_number() does one by one. Asks for no memory.
void take_back_every_atlas_number(atlas_slot_table &table) noexcept;

// Gives the table's memory back.
void close_atlas_slot_table(atlas_slot_table &table) noexcept;

} // namespace glintfx
