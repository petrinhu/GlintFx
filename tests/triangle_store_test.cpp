// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <cstdint>

#include <glintfx/core/color.hpp>
#include <glintfx/core/vec2.hpp>

#include "draw2d/triangle_store.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// triangle_store_test.cpp - R2D-SHAPES, fatia S2b-0 (docs/plano-fechamento-w8-r19.md, D-W8-367):
// the store that keeps the vertices and the local indices of the TRIANGLE pieces, a subject of its
// own; the piece list keeps only the order.
//
// WHAT IS PROVED, ALL OF IT ON PURE DATA (no GL, no window, no operating system):
//   - append() returns, for each piece, where its vertices and its indices start in the store;
//   - the store COPIES the indices it is given: the caller may overwrite its own array afterwards;
//   - a refused room (the allocator says no) leaves the store unchanged: nothing is half written;
//   - clear() empties the store and keeps its capacity: the next fill allocates nothing.

using glintfx::gltfx_rgba;
using glintfx::gltfx_vec2_screen;
using glintfx::draw2d::batch_allocator;
using glintfx::draw2d::colored_pixel_vertex;
using glintfx::draw2d::default_batch_allocator;
using glintfx::draw2d::triangle_position;
using glintfx::draw2d::triangle_store;

namespace {

constexpr gltfx_rgba k_red{1.0F, 0.0F, 0.0F, 1.0F};

[[nodiscard]] colored_pixel_vertex vertex_at(float x) {
    return colored_pixel_vertex{gltfx_vec2_screen{x, 0.0F}, k_red};
}

struct armed_state {
    int calls = 0;
    bool refuse_all = false;
};
thread_local armed_state g_armed;

[[nodiscard]] void *armed_reallocate(void *block, std::size_t bytes) noexcept {
    ++g_armed.calls;
    if (g_armed.refuse_all) {
        return nullptr; // like realloc: the old block is untouched
    }
    return default_batch_allocator().reallocate(block, bytes);
}
void armed_release(void *block) noexcept { default_batch_allocator().release(block); }
[[nodiscard]] batch_allocator armed() { return batch_allocator{armed_reallocate, armed_release}; }

} // namespace

GLINTFX_TEST(triangle_store_append_returns_where_each_piece_starts) {
    triangle_store store;
    const std::array<colored_pixel_vertex, 3> first{vertex_at(1.0F), vertex_at(2.0F),
                                                    vertex_at(3.0F)};
    const std::array<colored_pixel_vertex, 3> second{vertex_at(4.0F), vertex_at(5.0F),
                                                     vertex_at(6.0F)};
    const std::array<std::uint32_t, 3> local{0U, 1U, 2U};
    triangle_position where_first{};
    triangle_position where_second{};
    GLINTFX_CHECK(store.append(first, {}, where_first));
    GLINTFX_CHECK(store.append(second, local, where_second));
    GLINTFX_CHECK_EQ(where_first.first_vertex, std::size_t{0});
    GLINTFX_CHECK_EQ(where_first.first_index, std::size_t{0});
    GLINTFX_CHECK_EQ(where_second.first_vertex, std::size_t{3});
    GLINTFX_CHECK_EQ(where_second.first_index, std::size_t{0});
    GLINTFX_CHECK(store.vertices(where_second.first_vertex, 3)[0].position.x == 4.0F);
    GLINTFX_CHECK(store.vertices(0, 3)[2].position.x == 3.0F);
    GLINTFX_CHECK_EQ(store.indices(where_second.first_index, 3).size(), std::size_t{3});
}

// The expected values are the literals written here, not read back from the store.
GLINTFX_TEST(triangle_store_keeps_a_copy_of_the_indices_not_a_reference) {
    triangle_store store;
    std::array<std::uint32_t, 3> local{0U, 1U, 2U};
    const std::array<colored_pixel_vertex, 3> tri{vertex_at(1.0F), vertex_at(2.0F),
                                                  vertex_at(3.0F)};
    triangle_position where{};
    GLINTFX_CHECK(store.append(tri, local, where));
    local = {2U, 1U, 0U};
    const auto stored = store.indices(where.first_index, 3);
    GLINTFX_CHECK_EQ(stored.size(), std::size_t{3});
    GLINTFX_CHECK(stored[0] == 0U && stored[1] == 1U && stored[2] == 2U);
}

GLINTFX_TEST(triangle_store_refused_room_leaves_the_store_unchanged) {
    g_armed = armed_state{};
    triangle_store store(armed());
    const std::array<colored_pixel_vertex, 3> tri{vertex_at(1.0F), vertex_at(2.0F),
                                                  vertex_at(3.0F)};
    g_armed.refuse_all = true;
    triangle_position refused{};
    GLINTFX_CHECK(!store.room_for(3, 3));
    GLINTFX_CHECK(!store.append(tri, {}, refused));
    g_armed.refuse_all = false;
    triangle_position where{};
    GLINTFX_CHECK(store.append(tri, {}, where));
    GLINTFX_CHECK_EQ(where.first_vertex, std::size_t{0}); // the refused call wrote nothing
}

GLINTFX_TEST(triangle_store_clear_empties_it_and_keeps_the_capacity) {
    g_armed = armed_state{};
    triangle_store store(armed());
    const std::array<colored_pixel_vertex, 3> tri{vertex_at(1.0F), vertex_at(2.0F),
                                                  vertex_at(3.0F)};
    const std::array<std::uint32_t, 3> local{0U, 1U, 2U};
    triangle_position where{};
    GLINTFX_CHECK(store.append(tri, local, where));
    const int calls_before_clear = g_armed.calls;
    store.clear();
    triangle_position again{};
    GLINTFX_CHECK(store.append(tri, local, again));
    GLINTFX_CHECK_EQ(again.first_vertex, std::size_t{0});
    GLINTFX_CHECK_EQ(g_armed.calls, calls_before_clear); // the capacity was kept: nothing to grow
}

// R2D-TEXTURE T5 (D-W8-674): the reads of the textured pieces are kept apart, copied, and each
// append says where its reads start; clear() empties them with the rest.
GLINTFX_TEST(triangle_store_reads_are_copied_and_say_where_they_start) {
    triangle_store store;
    std::array<glintfx::draw2d::triangle_texture_read, 3> first{
        {{0.25F, 0.5F}, {0.75F, 0.5F}, {0.5F, 1.0F}}};
    const std::array<glintfx::draw2d::triangle_texture_read, 3> second{
        {{1.0F, 0.0F}, {0.0F, 1.0F}, {0.125F, 0.25F}}};
    std::size_t at_first = 99;
    std::size_t at_second = 99;
    GLINTFX_CHECK(store.append_reads(first, at_first));
    GLINTFX_CHECK(store.append_reads(second, at_second));
    first[0] = {0.0F, 0.0F}; // the caller's array changes after the call
    GLINTFX_CHECK_EQ(at_first, std::size_t{0});
    GLINTFX_CHECK_EQ(at_second, std::size_t{3});
    const auto kept = store.reads(0, 6);
    GLINTFX_CHECK(kept[0].u == 0.25F && kept[0].v == 0.5F);
    GLINTFX_CHECK(kept[5].u == 0.125F && kept[5].v == 0.25F);
    store.clear();
    std::size_t again = 99;
    GLINTFX_CHECK(store.append_reads(second, again));
    GLINTFX_CHECK_EQ(again, std::size_t{0});
}

// An untextured piece asks the read store for nothing: appending three vertices and three indices
// grows exactly two buffers, as before the reads existed (T5).
GLINTFX_TEST(triangle_store_an_untextured_append_grows_no_read_buffer) {
    g_armed = armed_state{};
    triangle_store store(armed());
    const std::array<colored_pixel_vertex, 3> vertices{vertex_at(1.0F), vertex_at(2.0F),
                                                       vertex_at(3.0F)};
    const std::array<std::uint32_t, 3> indices{0U, 1U, 2U};
    triangle_position where{};
    GLINTFX_CHECK(store.append(vertices, indices, where));
    GLINTFX_CHECK_EQ(g_armed.calls, 2);
}

// A refused room for reads leaves the store as it was; the next call works again (T5).
GLINTFX_TEST(triangle_store_refused_reads_leave_the_store_unchanged) {
    g_armed = armed_state{};
    triangle_store store(armed());
    const std::array<glintfx::draw2d::triangle_texture_read, 2> reads{{{0.5F, 0.5F}, {1.0F, 1.0F}}};
    g_armed.refuse_all = true;
    std::size_t first = 99;
    GLINTFX_CHECK(!store.append_reads(reads, first));
    GLINTFX_CHECK_EQ(first, std::size_t{99});
    g_armed.refuse_all = false;
    GLINTFX_CHECK(store.append_reads(reads, first));
    GLINTFX_CHECK_EQ(first, std::size_t{0});
    GLINTFX_CHECK(store.reads(0, 2)[1].u == 1.0F);
}
