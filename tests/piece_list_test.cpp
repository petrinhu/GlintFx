// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <cstdint>
#include <print>
#include <vector>

#include <glintfx/core/color.hpp>

#include "draw2d/piece_list.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// piece_list_test.cpp - R2D-BATCH, fatia B4 (docs/auditoria-api-draw2d.md B0 "THE ORDER, frozen"):
// the pending pieces of a frame are painted by (layer, order of submission). PURE data, all five
// systems.
//
// WHAT IS PROVED:
//   - a lower layer is painted first, and on the same layer the submission order is kept;
//   - a piece carries its corners and color through the sort untouched (the piece at a paint
//   position is the
//     one that was submitted, not a copy of another);
//   - every piece on ONE layer (the common case) keeps the submission order;
//   - the sort works IN PLACE and allocates nothing: an allocator that refuses everything after the
//   reserve does not change the paint order (D-B4-2);
//   - clear() starts the submission counter again, which is what stops the order at a flush()
//   barrier;
//   - MEMORY: an armed allocator that fails at EVERY growth point never throws, never half-adds a
//   piece and
//     never loses one; the bound of the list is honored.
//
// The marker of each piece is its x coordinate (an integer), so the paint order can be read as
// numbers.

using glintfx::gltfx_rgba;
using glintfx::gltfx_vec2_screen;
using glintfx::draw2d::batch_allocator;
using glintfx::draw2d::default_batch_allocator;
using glintfx::draw2d::piece_list;
using glintfx::draw2d::quad_corners_pixel;

namespace {

constexpr gltfx_rgba k_red{1.0F, 0.0F, 0.0F, 1.0F};

[[nodiscard]] quad_corners_pixel marked(float marker) {
    return quad_corners_pixel{
        gltfx_vec2_screen{marker, 0.0F}, gltfx_vec2_screen{marker + 1.0F, 0.0F},
        gltfx_vec2_screen{marker + 1.0F, 1.0F}, gltfx_vec2_screen{marker, 1.0F}};
}

[[nodiscard]] std::vector<int> paint_order(const piece_list &list) {
    std::vector<int> order;
    order.reserve(list.size());
    for (std::size_t i = 0; i < list.size(); ++i) {
        order.push_back(static_cast<int>(list.painted(i).corners[0].x));
    }
    return order;
}

struct armed_state {
    int calls = 0;
    int fail_at = 0;
    bool refuse_all = false; // refuses every call from now on (a test sets it after the reserve)
};
thread_local armed_state g_armed;

[[nodiscard]] void *armed_reallocate(void *block, std::size_t bytes) noexcept {
    ++g_armed.calls;
    if (g_armed.refuse_all || (g_armed.fail_at != 0 && g_armed.calls == g_armed.fail_at)) {
        return nullptr; // like realloc: the old block is untouched
    }
    return default_batch_allocator().reallocate(block, bytes);
}
void armed_release(void *block) noexcept { default_batch_allocator().release(block); }
[[nodiscard]] batch_allocator armed() { return batch_allocator{armed_reallocate, armed_release}; }

} // namespace

GLINTFX_TEST(piece_list_paint_order_is_layer_then_submission) {
    piece_list list;
    // markers 10..14 submitted in this order, on layers 3, 1, 3, 2, 1
    GLINTFX_CHECK(list.add(marked(10), k_red, 3));
    GLINTFX_CHECK(list.add(marked(11), k_red, 1));
    GLINTFX_CHECK(list.add(marked(12), k_red, 3));
    GLINTFX_CHECK(list.add(marked(13), k_red, 2));
    GLINTFX_CHECK(list.add(marked(14), k_red, 1));
    list.sort();
    // layer 1: 11 then 14 (submission); layer 2: 13; layer 3: 10 then 12
    GLINTFX_CHECK((paint_order(list) == std::vector<int>{11, 14, 13, 10, 12}));
}

GLINTFX_TEST(piece_list_a_negative_layer_is_below_the_default_layer_zero) {
    piece_list list;
    GLINTFX_CHECK(list.add(marked(1), k_red, 0));
    GLINTFX_CHECK(list.add(marked(2), k_red, -5));
    GLINTFX_CHECK(list.add(marked(3), k_red, 0));
    list.sort();
    GLINTFX_CHECK((paint_order(list) == std::vector<int>{2, 1, 3}));
}

GLINTFX_TEST(piece_list_the_piece_keeps_its_corners_and_color_through_the_sort) {
    piece_list list;
    GLINTFX_CHECK(list.add(marked(1), gltfx_rgba{0.1F, 0.2F, 0.3F, 0.4F}, 9));
    GLINTFX_CHECK(list.add(marked(2), gltfx_rgba{0.5F, 0.6F, 0.7F, 0.8F}, 1));
    list.sort();
    const auto &first = list.painted(0); // the layer-1 piece
    GLINTFX_CHECK(first.corners[0].x == 2.0F && first.corners[2].y == 1.0F);
    GLINTFX_CHECK(first.color.red == 0.5F && first.color.alpha == 0.8F);
    GLINTFX_CHECK_EQ(first.layer, std::int32_t{1});
    const auto &second = list.painted(1);
    GLINTFX_CHECK(second.corners[0].x == 1.0F && second.color.green == 0.2F);
}

// One layer: the submission order, no sort, and it works without ever calling sort() too.
GLINTFX_TEST(piece_list_one_layer_keeps_the_submission_order) {
    piece_list list;
    for (int i = 0; i < 50; ++i) {
        GLINTFX_CHECK(list.add(marked(static_cast<float>(i)), k_red, 7));
    }
    std::vector<int> expected(50);
    for (int i = 0; i < 50; ++i) {
        expected[static_cast<std::size_t>(i)] = i;
    }
    GLINTFX_CHECK((paint_order(list) == expected)); // before sort(): submission order
    list.sort();
    GLINTFX_CHECK((paint_order(list) == expected));
}

// The order does NOT cross clear() (the flush barrier): what is added after it starts a new order.
GLINTFX_TEST(piece_list_clear_starts_a_new_order_and_keeps_the_capacity) {
    g_armed = armed_state{};
    piece_list list(armed());
    for (int i = 0; i < 20; ++i) {
        GLINTFX_CHECK(list.add(marked(static_cast<float>(i)), k_red, 20 - i));
    }
    list.sort();
    list.clear();
    GLINTFX_CHECK_EQ(list.size(), std::size_t{0});
    const int calls_after_first_fill = g_armed.calls;
    GLINTFX_CHECK(list.add(marked(100), k_red, 5));
    GLINTFX_CHECK(list.add(marked(101), k_red, 1));
    list.sort();
    GLINTFX_CHECK((paint_order(list) == std::vector<int>{101, 100}));
    GLINTFX_CHECK_EQ(g_armed.calls,
                     calls_after_first_fill); // the capacity was kept: nothing to grow
}

// reserve(): room in one go, so the fill that follows calls the allocator no more.
GLINTFX_TEST(piece_list_reserve_takes_room_once) {
    g_armed = armed_state{};
    piece_list list(armed());
    GLINTFX_CHECK(list.reserve(64));
    const int after_reserve = g_armed.calls;
    for (int i = 0; i < 64; ++i) {
        GLINTFX_CHECK(list.add(marked(static_cast<float>(i)), k_red, i % 3));
    }
    GLINTFX_CHECK_EQ(g_armed.calls, after_reserve);
    GLINTFX_CHECK(list.reserve(0));
    GLINTFX_CHECK(!list.reserve(static_cast<std::size_t>(-1)));
}

// The bound: the 4th piece of a list of at most 3 is refused, the list unchanged.
GLINTFX_TEST(piece_list_the_bound_of_the_list_is_honored) {
    piece_list list(default_batch_allocator(), 3);
    GLINTFX_CHECK(list.add(marked(1), k_red, 0));
    GLINTFX_CHECK(list.add(marked(2), k_red, 0));
    GLINTFX_CHECK(list.add(marked(3), k_red, 0));
    GLINTFX_CHECK(!list.add(marked(4), k_red, 0));
    GLINTFX_CHECK_EQ(list.size(), std::size_t{3});
    GLINTFX_CHECK(!list.reserve(1)); // no room left under the bound
    GLINTFX_CHECK((paint_order(list) == std::vector<int>{1, 2, 3}));
}

// Memory: an allocator that fails at EACH growth point of a mixed-layer fill. add() never
// half-adds; later calls work again.
GLINTFX_TEST(piece_list_out_of_memory_at_every_point_never_loses_or_half_adds_a_piece) {
    int reference_calls = 0;
    {
        g_armed = armed_state{};
        piece_list list(armed());
        for (int i = 0; i < 40; ++i) {
            GLINTFX_CHECK(list.add(marked(static_cast<float>(i)), k_red, (i * 7) % 5));
        }
        list.sort();
        reference_calls = g_armed.calls;
    }
    GLINTFX_CHECK(reference_calls >= 3); // growth of the pieces (16, 32, 64)
    int analyzed = 0;
    for (int point = 1; point <= reference_calls; ++point) {
        g_armed = armed_state{};
        g_armed.fail_at = point;
        piece_list list(armed());
        std::size_t accepted = 0;
        for (int i = 0; i < 40; ++i) {
            if (list.add(marked(static_cast<float>(i)), k_red, (i * 7) % 5)) {
                ++accepted;
            }
        }
        GLINTFX_CHECK_EQ(list.size(), accepted); // nothing half-added, nothing lost silently
        list.sort();
        // whatever happened, every accepted piece is there exactly once
        const std::vector<int> seen = paint_order(list);
        GLINTFX_CHECK_EQ(seen.size(), accepted);
        std::array<int, 40> count{};
        for (const int marker : seen) {
            ++count[static_cast<std::size_t>(marker)];
        }
        for (const int c : count) {
            GLINTFX_CHECK(c <= 1);
        }
        // the allocator works again: a later piece goes through
        GLINTFX_CHECK(list.add(marked(99), k_red, 0));
        ++analyzed;
    }
    GLINTFX_CHECK_EQ(analyzed, reference_calls);
    std::println("piece_list_test: falta de memoria em {} pontos de crescimento", reference_calls);
}

// A piece added AFTER sort() goes to the end, and the next sort() puts the whole list in paint
// order again (the key of every piece is unique, so the order is total whatever the arrangement
// was).
GLINTFX_TEST(piece_list_a_piece_added_after_sort_is_ordered_by_the_next_sort) {
    piece_list list;
    GLINTFX_CHECK(list.add(marked(1), k_red, 2));
    GLINTFX_CHECK(list.add(marked(2), k_red, 1));
    list.sort();
    GLINTFX_CHECK((paint_order(list) == std::vector<int>{2, 1}));
    GLINTFX_CHECK(list.add(marked(3), k_red, 0));
    list.sort();
    GLINTFX_CHECK((paint_order(list) == std::vector<int>{3, 2, 1}));
    list.sort(); // sorting an ordered list changes nothing
    GLINTFX_CHECK((paint_order(list) == std::vector<int>{3, 2, 1}));
}

// D-B4-2: the paint order never depends on memory. After the reserve, an allocator that refuses
// everything cannot change the order of a frame with three layers, because the sort asks it for
// nothing.
GLINTFX_TEST(piece_list_the_paint_order_does_not_depend_on_memory_after_the_reserve) {
    g_armed = armed_state{};
    piece_list list(armed());
    GLINTFX_CHECK(list.reserve(9));
    g_armed.refuse_all = true;
    const int layers[9] = {2, 0, 1, 2, 0, 1, 1, 2, 0};
    for (int i = 0; i < 9; ++i) {
        GLINTFX_CHECK(list.add(marked(static_cast<float>(i)), k_red, layers[i]));
    }
    const int calls_before_sort = g_armed.calls;
    list.sort();
    GLINTFX_CHECK_EQ(g_armed.calls, calls_before_sort); // the sort called the allocator no more
    // layer 0: 1,4,8; layer 1: 2,5,6; layer 2: 0,3,7 - each in submission order
    GLINTFX_CHECK((paint_order(list) == std::vector<int>{1, 4, 8, 2, 5, 6, 0, 3, 7}));
}
