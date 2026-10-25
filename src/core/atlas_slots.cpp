// SPDX-License-Identifier: AGPL-3.0-or-later
#include "core/atlas_slots.hpp"

#include <cstddef>
#include <cstdint>

// core/atlas_slots.cpp - R2D-TEXTURE. The numbers of the cells and their generations;
// core/atlas_slots.hpp says the rules.
namespace glintfx {

namespace {

bool same_rect(const gltfx_atlas_rect &left, const gltfx_atlas_rect &right) noexcept {
    return left.x == right.x && left.y == right.y && left.width == right.width &&
           left.height == right.height;
}

} // namespace

bool reserve_atlas_number(atlas_slot_table &table) noexcept {
    if (table.free_head != k_atlas_no_number) {
        return true;
    }
    if (table.entries.size >= std::size_t{k_atlas_no_number}) {
        return false;
    }
    return reserve_atlas_room(table.memory, table.entries, 1);
}

gltfx_atlas_slot hand_out_atlas_number(atlas_slot_table &table, const gltfx_atlas_rect &rect,
                                       std::uint32_t owner) noexcept {
    std::uint32_t id = table.free_head;
    if (id != k_atlas_no_number) {
        atlas_slot_entry &reused = table.entries.data[id];
        table.free_head = reused.next_free;
        reused.generation += 1U;
    } else {
        id = static_cast<std::uint32_t>(table.entries.size);
        table.entries.data[id] = atlas_slot_entry{.generation = 1};
        ++table.entries.size;
    }
    atlas_slot_entry &entry = table.entries.data[id];
    entry.rect = rect;
    entry.next_free = k_atlas_no_number;
    entry.in_use = true;
    return gltfx_atlas_slot{.rect = rect, .id = id, .generation = entry.generation, .owner = owner};
}

bool is_current_atlas_slot(const atlas_slot_table &table, const gltfx_atlas_slot &slot) noexcept {
    if (std::size_t{slot.id} >= table.entries.size) {
        return false;
    }
    const atlas_slot_entry &entry = table.entries.data[slot.id];
    return entry.in_use && entry.generation == slot.generation && same_rect(entry.rect, slot.rect);
}

void take_back_atlas_number(atlas_slot_table &table, std::uint32_t id) noexcept {
    atlas_slot_entry &entry = table.entries.data[id];
    entry.in_use = false;
    if (entry.generation == k_atlas_last_generation) {
        entry.next_free = k_atlas_no_number; // retired: never handed out again
        return;
    }
    entry.next_free = table.free_head;
    table.free_head = id;
}

void take_back_every_atlas_number(atlas_slot_table &table) noexcept {
    for (std::size_t index = 0; index < table.entries.size; ++index) {
        if (table.entries.data[index].in_use) {
            take_back_atlas_number(table, static_cast<std::uint32_t>(index));
        }
    }
}

void close_atlas_slot_table(atlas_slot_table &table) noexcept {
    if (table.entries.data != nullptr) {
        free_atlas_array(table.memory, table.entries);
    }
    table.free_head = k_atlas_no_number;
}

} // namespace glintfx
