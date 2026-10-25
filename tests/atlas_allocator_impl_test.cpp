// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <glintfx/core/atlas_allocator.hpp>
#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

#include "core/atlas_allocator_impl.hpp"
#include "core/atlas_memory.hpp"
#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// atlas_allocator_impl_test.cpp - R2D-TEXTURE (core/atlas_allocator_impl.hpp; GODS_LAWS.md L-20,
// L-22, docs/api-conventions.md R3). The interior of the atlas allocator with a memory armed to
// refuse ONE chosen request: each request for memory that open() and allocate() make is refused
// in turn, and the call answers out_of_memory, with the allocator exactly as it was (the next call
// gives the rect it would have given) and every block given back at the end. With the same memory:
// release() and release_all() ask for none, and the move assignment of the public handle, which
// lives here, gives the old interior back and leaves a handle moved onto itself alone. And the
// owner number after the last one is 1, never 0. The interior and its three atoms are compiled a
// second time into this test (tests/CMakeLists.txt): none has a GLINTFX_API symbol, so the armed
// memory reaches every request on every system, Windows included, with no hook into the
// library's DLL.

namespace {

using glintfx::atlas_allocator_impl;
using glintfx::atlas_memory;
using glintfx::default_atlas_memory;
using glintfx::gltfx_atlas_slot;
using glintfx::gltfx_err_code;
using glintfx::gltfx_rslt;

// Refuses the request numbered `refuse_at` (1 is the first after arming); counts the blocks alive.
struct armed_state {
    int requests = 0;
    int refuse_at = 0;
    int blocks_alive = 0;
};
thread_local armed_state g_armed;

[[nodiscard]] void *armed_reallocate(void *block, std::size_t bytes) noexcept {
    ++g_armed.requests;
    if (g_armed.requests == g_armed.refuse_at) {
        return nullptr; // like realloc: the old block is untouched
    }
    if (block == nullptr) {
        ++g_armed.blocks_alive;
    }
    return default_atlas_memory().reallocate(block, bytes);
}
void armed_release(void *block) noexcept {
    if (block != nullptr) {
        --g_armed.blocks_alive;
    }
    default_atlas_memory().release(block);
}
[[nodiscard]] atlas_memory armed(int refuse_at) {
    g_armed = armed_state{.refuse_at = refuse_at};
    return atlas_memory{armed_reallocate, armed_release};
}

template <typename T>
[[nodiscard]] bool refused_as(const gltfx_rslt<T> &result, gltfx_err_code code,
                              std::string_view name) {
    return result.has_error() && result.err().code() == code &&
           result.err().rejected_value() == name;
}

[[nodiscard]] atlas_allocator_impl *made(std::uint32_t width, std::uint32_t height,
                                         atlas_memory memory) {
    const gltfx_rslt<atlas_allocator_impl *> result =
        glintfx::make_atlas_allocator_impl(width, height, memory);
    GLINTFX_CHECK(result.has_value());
    return result.value();
}

[[nodiscard]] bool slot_at(const gltfx_rslt<gltfx_atlas_slot> &slot, std::uint32_t x,
                           std::uint32_t y) {
    return slot.has_value() && slot.value().rect.x == x && slot.value().rect.y == y;
}

} // namespace

// open() asks for two blocks: the interior, then the first run. Each refused in turn.
GLINTFX_TEST(atlas_allocator_impl_open_refused_memory_is_out_of_memory_and_leaks_nothing) {
    int right = 0;
    for (int refused = 1; refused <= 2; ++refused) {
        const gltfx_rslt<atlas_allocator_impl *> result =
            glintfx::make_atlas_allocator_impl(8, 8, armed(refused));
        const bool clean = g_armed.blocks_alive == 0;
        right += refused_as(result, gltfx_err_code::out_of_memory, "atlas_open") && clean ? 1 : 0;
    }
    GLINTFX_CHECK_EQ(right, 2);
    atlas_allocator_impl *impl = made(8, 8, armed(0));
    GLINTFX_CHECK_EQ(g_armed.requests, 2);
    glintfx::destroy_atlas_allocator_impl(impl);
    GLINTFX_CHECK_EQ(g_armed.blocks_alive, 0);
}

GLINTFX_TEST(atlas_allocator_impl_open_checks_the_size_before_asking_for_memory) {
    const gltfx_rslt<atlas_allocator_impl *> result =
        glintfx::make_atlas_allocator_impl(0, 8, armed(1));
    GLINTFX_CHECK(refused_as(result, gltfx_err_code::invalid_argument, "width"));
    GLINTFX_CHECK_EQ(g_armed.requests, 0);
}

// The first allocate() asks for the first number (request 3 after the two of open()); refused, the
// call is out_of_memory and the runs are untouched: the next call lands at (0, 0).
GLINTFX_TEST(atlas_allocator_impl_refused_number_leaves_the_positions_untouched) {
    atlas_allocator_impl *impl = made(64, 64, armed(3));
    GLINTFX_CHECK(refused_as(glintfx::allocate_in_atlas(impl, 10, 8), gltfx_err_code::out_of_memory,
                             "atlas_allocate"));
    GLINTFX_CHECK_EQ(impl->geometry.runs.size, std::size_t{1});
    GLINTFX_CHECK(slot_at(glintfx::allocate_in_atlas(impl, 10, 8), 0, 0));
    glintfx::destroy_atlas_allocator_impl(impl);
    GLINTFX_CHECK_EQ(g_armed.blocks_alive, 0);
}

// Seven cells 8 wide fill the 8 runs the first growth gave, so the eighth cell needs a ninth run
// (its number still fits the 8 entries the slot table took). With the run growth refused, the
// call is out_of_memory, no number is handed out, and the next call gives the same rect.
GLINTFX_TEST(atlas_allocator_impl_refused_run_hands_out_no_number) {
    atlas_allocator_impl *impl = made(128, 8, armed(0));
    std::uint32_t placed = 0;
    for (std::uint32_t cell = 0; cell < 7; ++cell) {
        placed += slot_at(glintfx::allocate_in_atlas(impl, 8, 8), cell * 8U, 0) ? 1U : 0U;
    }
    GLINTFX_CHECK_EQ(placed, 7U);
    GLINTFX_CHECK_EQ(impl->geometry.runs.size, std::size_t{8});
    const std::size_t numbers = impl->slots.entries.size;
    g_armed.refuse_at = g_armed.requests + 1;
    GLINTFX_CHECK(refused_as(glintfx::allocate_in_atlas(impl, 8, 8), gltfx_err_code::out_of_memory,
                             "atlas_allocate"));
    GLINTFX_CHECK_EQ(impl->slots.entries.size, numbers);
    GLINTFX_CHECK_EQ(impl->geometry.runs.size, std::size_t{8});
    const gltfx_rslt<gltfx_atlas_slot> next = glintfx::allocate_in_atlas(impl, 8, 8);
    GLINTFX_CHECK(slot_at(next, 56, 0) && next.value().id == 7U);
    glintfx::destroy_atlas_allocator_impl(impl);
    GLINTFX_CHECK_EQ(g_armed.blocks_alive, 0);
}

// A full atlas is no_free_space even when the memory would refuse: no room is found before any
// memory is asked for.
GLINTFX_TEST(atlas_allocator_impl_full_atlas_is_no_free_space_before_any_request) {
    atlas_allocator_impl *impl = made(8, 8, armed(0));
    GLINTFX_CHECK(slot_at(glintfx::allocate_in_atlas(impl, 8, 8), 0, 0));
    g_armed.refuse_at = g_armed.requests + 1;
    const int requests = g_armed.requests;
    GLINTFX_CHECK(refused_as(glintfx::allocate_in_atlas(impl, 1, 1), gltfx_err_code::no_free_space,
                             "atlas_full"));
    GLINTFX_CHECK_EQ(g_armed.requests, requests);
    glintfx::destroy_atlas_allocator_impl(impl);
}

GLINTFX_TEST(atlas_allocator_impl_owner_numbers_are_never_zero_and_differ) {
    atlas_allocator_impl *first = made(8, 8, armed(0));
    atlas_allocator_impl *second = made(8, 8, default_atlas_memory());
    GLINTFX_CHECK(first->owner != 0U && second->owner != 0U);
    GLINTFX_CHECK(first->owner != second->owner);
    glintfx::destroy_atlas_allocator_impl(first);
    glintfx::destroy_atlas_allocator_impl(second);
}

// Eight cells take the 8 numbers the first growth of the slot table gave, so the ninth needs a
// second growth of that table (a block that already exists, grown in the realloc way). Refused, the
// call is out_of_memory, the numbers and the runs are untouched, and the next call gives the ninth
// cell.
GLINTFX_TEST(atlas_allocator_impl_refused_second_number_growth_leaves_the_runs_untouched) {
    atlas_allocator_impl *impl = made(128, 8, armed(0));
    std::uint32_t placed = 0;
    for (std::uint32_t cell = 0; cell < 8; ++cell) {
        placed += slot_at(glintfx::allocate_in_atlas(impl, 8, 8), cell * 8U, 0) ? 1U : 0U;
    }
    GLINTFX_CHECK_EQ(placed, 8U);
    GLINTFX_CHECK(impl->slots.entries.size == 8 && impl->slots.entries.capacity == 8);
    const std::size_t runs = impl->geometry.runs.size;
    g_armed.refuse_at = g_armed.requests + 1;
    GLINTFX_CHECK(refused_as(glintfx::allocate_in_atlas(impl, 8, 8), gltfx_err_code::out_of_memory,
                             "atlas_allocate"));
    GLINTFX_CHECK_EQ(impl->slots.entries.size, std::size_t{8});
    GLINTFX_CHECK_EQ(impl->geometry.runs.size, runs);
    const gltfx_rslt<gltfx_atlas_slot> next = glintfx::allocate_in_atlas(impl, 8, 8);
    GLINTFX_CHECK(slot_at(next, 64, 0) && next.value().id == 8U);
    glintfx::destroy_atlas_allocator_impl(impl);
    GLINTFX_CHECK_EQ(g_armed.blocks_alive, 0);
}

// release() and release_all() ask for no memory: with the next request armed to be refused, both
// do their work and the count of requests does not move; then the whole area is free again.
GLINTFX_TEST(atlas_allocator_impl_release_and_release_all_ask_for_no_memory) {
    atlas_allocator_impl *impl = made(32, 8, armed(0));
    const gltfx_rslt<gltfx_atlas_slot> first = glintfx::allocate_in_atlas(impl, 8, 8);
    GLINTFX_CHECK(slot_at(first, 0, 0));
    GLINTFX_CHECK(slot_at(glintfx::allocate_in_atlas(impl, 8, 8), 8, 0));
    g_armed.refuse_at = g_armed.requests + 1;
    const int requests = g_armed.requests;
    GLINTFX_CHECK(glintfx::release_from_atlas(impl, first.value()).has_value());
    const int after_release = g_armed.requests;
    glintfx::release_everything_in_atlas(impl);
    const int after_release_all = g_armed.requests;
    GLINTFX_CHECK_EQ(after_release, requests);
    GLINTFX_CHECK_EQ(after_release_all, requests);
    g_armed.refuse_at = 0;
    GLINTFX_CHECK(slot_at(glintfx::allocate_in_atlas(impl, 32, 8), 0, 0));
    glintfx::destroy_atlas_allocator_impl(impl);
    GLINTFX_CHECK_EQ(g_armed.blocks_alive, 0);
}

// The move assignment of the public handle: the target's old interior is given back (its two
// blocks leave the count), the target holds the source's interior, the source is empty. Moved onto
// itself, a handle keeps its interior and nothing is given back.
GLINTFX_TEST(atlas_allocator_impl_move_gives_the_old_interior_back_and_leaves_itself_alone) {
    const atlas_memory memory = armed(0);
    atlas_allocator_impl *target = made(8, 8, memory);
    atlas_allocator_impl *source = made(16, 16, memory);
    atlas_allocator_impl *const source_interior = source;
    GLINTFX_CHECK_EQ(g_armed.blocks_alive, 4); // two blocks each: the interior and the first run
    glintfx::move_atlas_allocator_impl(target, source);
    GLINTFX_CHECK(target == source_interior && source == nullptr);
    GLINTFX_CHECK_EQ(g_armed.blocks_alive, 2);
    glintfx::move_atlas_allocator_impl(target, target);
    GLINTFX_CHECK(target == source_interior);
    GLINTFX_CHECK_EQ(g_armed.blocks_alive, 2);
    glintfx::destroy_atlas_allocator_impl(target);
    GLINTFX_CHECK_EQ(g_armed.blocks_alive, 0);
}

// The owner after the last one: one more, and 1 after 4294967295, so a process that opens more
// allocators than that never hands out owner 0, the owner of a slot never handed out.
GLINTFX_TEST(atlas_allocator_impl_owner_after_the_last_one_is_one) {
    GLINTFX_CHECK_EQ(glintfx::atlas_owner_after(0U), 1U);
    GLINTFX_CHECK_EQ(glintfx::atlas_owner_after(41U), 42U);
    GLINTFX_CHECK_EQ(glintfx::atlas_owner_after(0xFFFFFFFEU), 0xFFFFFFFFU);
    GLINTFX_CHECK_EQ(glintfx::atlas_owner_after(0xFFFFFFFFU), 1U);
}
