// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <cstdint>

#include <glintfx/core/atlas_allocator.hpp>

#include "core/atlas_memory.hpp"
#include "core/atlas_slots.hpp"
#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// atlas_slots_test.cpp - R2D-TEXTURE (core/atlas_slots.hpp; GODS_LAWS.md L-17, L-20, L-22). The
// numbers of the cells, alone: a new number at generation 1, the last released handed out first
// with the next generation, the number retired at the last generation, the current-slot check
// (each field of the rect, the number and the generation, one at a time) and the memory refused.
// atlas_slots.cpp is compiled a second time into this test (tests/CMakeLists.txt): it has no
// GLINTFX_API symbol. The generation at the edge is reached by writing it into the entry, which
// only an internal test can do: handing a number out 4294967295 times would take minutes for one
// case.

namespace {

using glintfx::atlas_memory;
using glintfx::atlas_slot_table;
using glintfx::default_atlas_memory;
using glintfx::gltfx_atlas_rect;
using glintfx::gltfx_atlas_slot;
using glintfx::k_atlas_last_generation;
using glintfx::k_atlas_no_number;

struct armed_state {
    bool refuse = false;
};
thread_local armed_state g_armed;

[[nodiscard]] void *armed_reallocate(void *block, std::size_t bytes) noexcept {
    if (g_armed.refuse) {
        return nullptr; // like realloc: the old block is untouched
    }
    return default_atlas_memory().reallocate(block, bytes);
}
void armed_release(void *block) noexcept { default_atlas_memory().release(block); }

// A table with the armed memory, closed at the end of the case.
struct open_table {
    atlas_slot_table table{};
    open_table() {
        g_armed = armed_state{};
        table.memory = atlas_memory{armed_reallocate, armed_release};
    }
    open_table(const open_table &) = delete;
    open_table &operator=(const open_table &) = delete;
    open_table(open_table &&) = delete;
    open_table &operator=(open_table &&) = delete;
    ~open_table() { glintfx::close_atlas_slot_table(table); }
};

constexpr gltfx_atlas_rect k_rect{2, 3, 4, 5};
constexpr std::uint32_t k_owner = 7;

// Hands out a number for k_rect; the reservation must succeed.
[[nodiscard]] gltfx_atlas_slot handed_out(atlas_slot_table &table) {
    GLINTFX_CHECK(glintfx::reserve_atlas_number(table));
    return glintfx::hand_out_atlas_number(table, k_rect, k_owner);
}

[[nodiscard]] bool is_slot(const gltfx_atlas_slot &slot, std::uint32_t id,
                           std::uint32_t generation) {
    return slot.id == id && slot.generation == generation && slot.owner == k_owner &&
           slot.rect.x == k_rect.x && slot.rect.y == k_rect.y && slot.rect.width == k_rect.width &&
           slot.rect.height == k_rect.height;
}

} // namespace

GLINTFX_TEST(atlas_slots_new_numbers_count_up_from_zero_at_generation_one) {
    open_table open;
    GLINTFX_CHECK(is_slot(handed_out(open.table), 0, 1));
    GLINTFX_CHECK(is_slot(handed_out(open.table), 1, 1));
    GLINTFX_CHECK(is_slot(handed_out(open.table), 2, 1));
}

GLINTFX_TEST(atlas_slots_last_released_number_is_handed_out_first_at_the_next_generation) {
    open_table open;
    const gltfx_atlas_slot first = handed_out(open.table);
    const gltfx_atlas_slot second = handed_out(open.table);
    glintfx::take_back_atlas_number(open.table, first.id);
    glintfx::take_back_atlas_number(open.table, second.id);
    GLINTFX_CHECK(is_slot(handed_out(open.table), 1, 2));
    GLINTFX_CHECK(is_slot(handed_out(open.table), 0, 2));
    GLINTFX_CHECK(is_slot(handed_out(open.table), 2, 1));
}

GLINTFX_TEST(atlas_slots_number_at_the_last_generation_is_never_handed_out_again) {
    open_table open;
    const gltfx_atlas_slot first = handed_out(open.table);
    GLINTFX_CHECK(std::size_t{first.id} < open.table.entries.size);
    open.table.entries.data[first.id].generation = k_atlas_last_generation - 1U;
    glintfx::take_back_atlas_number(open.table, first.id);
    const gltfx_atlas_slot last = handed_out(open.table);
    GLINTFX_CHECK(is_slot(last, 0, k_atlas_last_generation));
    glintfx::take_back_atlas_number(open.table, last.id);
    GLINTFX_CHECK_EQ(open.table.free_head, k_atlas_no_number);
    GLINTFX_CHECK(is_slot(handed_out(open.table), 1, 1));
    GLINTFX_CHECK(!glintfx::is_current_atlas_slot(open.table, last));
}

GLINTFX_TEST(atlas_slots_current_slot_needs_its_number_generation_and_rect) {
    open_table open;
    const gltfx_atlas_slot slot = handed_out(open.table);
    GLINTFX_CHECK(glintfx::is_current_atlas_slot(open.table, slot));
    std::array<gltfx_atlas_slot, 6> changed{slot, slot, slot, slot, slot, slot};
    changed[0].rect.x += 1U;
    changed[1].rect.y += 1U;
    changed[2].rect.width += 1U;
    changed[3].rect.height += 1U;
    changed[4].generation = 0;
    changed[5].id = 1;
    std::size_t not_current = 0;
    for (const gltfx_atlas_slot &one : changed) {
        not_current += glintfx::is_current_atlas_slot(open.table, one) ? 0U : 1U;
    }
    GLINTFX_CHECK_EQ(not_current, changed.size());
    glintfx::take_back_atlas_number(open.table, slot.id);
    GLINTFX_CHECK(!glintfx::is_current_atlas_slot(open.table, slot));
}

GLINTFX_TEST(atlas_slots_take_back_every_number_makes_every_slot_stale) {
    open_table open;
    const gltfx_atlas_slot first = handed_out(open.table);
    const gltfx_atlas_slot second = handed_out(open.table);
    glintfx::take_back_every_atlas_number(open.table);
    GLINTFX_CHECK(!glintfx::is_current_atlas_slot(open.table, first));
    GLINTFX_CHECK(!glintfx::is_current_atlas_slot(open.table, second));
    GLINTFX_CHECK(is_slot(handed_out(open.table), 1, 2));
}

// With no free number, a refused memory refuses the reservation and leaves the table as it was;
// with a free number, the reservation asks for no memory.
GLINTFX_TEST(atlas_slots_refused_memory_refuses_only_a_new_number) {
    open_table open;
    g_armed.refuse = true;
    GLINTFX_CHECK(!glintfx::reserve_atlas_number(open.table));
    GLINTFX_CHECK(open.table.entries.size == 0 && open.table.entries.data == nullptr);
    g_armed.refuse = false;
    const gltfx_atlas_slot slot = handed_out(open.table);
    glintfx::take_back_atlas_number(open.table, slot.id);
    g_armed.refuse = true;
    GLINTFX_CHECK(glintfx::reserve_atlas_number(open.table));
    GLINTFX_CHECK(is_slot(glintfx::hand_out_atlas_number(open.table, k_rect, k_owner), 0, 2));
    g_armed.refuse = false;
}

// Every number below k_atlas_no_number taken: no new number, without asking for memory. The size
// is written into the table, which is restored before it is closed.
GLINTFX_TEST(atlas_slots_no_new_number_past_the_last) {
    open_table open;
    const std::size_t real_size = open.table.entries.size;
    open.table.entries.size = std::size_t{k_atlas_no_number};
    const bool reserved = glintfx::reserve_atlas_number(open.table);
    open.table.entries.size = real_size;
    GLINTFX_CHECK(!reserved);
}
