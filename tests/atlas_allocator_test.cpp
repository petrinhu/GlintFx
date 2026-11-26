// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <cstdint>
#include <print>
#include <string_view>
#include <utility>

#include <glintfx/core/atlas_allocator.hpp>
#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// atlas_allocator_test.cpp - R2D-TEXTURE (docs/auditoria-api-texture.md, the atlas allocator;
// GODS_LAWS.md L-04, L-20, L-22, L-40). The public allocator of core/atlas_allocator.hpp, through
// its public API only, so the same cases run on every system of the build.
//
// THE POSITIONS ARE WRITTEN HERE, worked out by hand from the four steps of the public header
// before the implementation existed, never read back from the program. The fixed sequence below
// is the promise "the same calls give the same positions": every leg of the CI runs it, with the
// same literals.
//
// THE CELLS THAT ANSWER A COMPLAINT (read on each page before this file was written): a rectangle
// as large as the atlas fits, one pixel more does not, and on a full atlas a larger one is still
// refused by its side, never as full (bevy #25224); the rect handed out in a
// larger free run is the size asked (SDL_ttf #643); a request that only a shelf of bad fits can
// take is still placed (etagere #36); a released cell's space is used again (Cesium forum,
// "TextureAtlas ... never reuses space"); a full atlas says so by name (fontstash #18, nanovg
// #431); release_all() gives the whole area back (nanovg #114); releasing a stale or never handed
// out slot is a refusal, never undefined behaviour (cogl, "freeing an atlas texture that hasn't
// been allocated").

namespace {

using glintfx::gltfx_atlas_allocator;
using glintfx::gltfx_atlas_rect;
using glintfx::gltfx_atlas_slot;
using glintfx::gltfx_err_code;
using glintfx::gltfx_rslt;

[[nodiscard]] gltfx_atlas_allocator opened(std::uint32_t width, std::uint32_t height) {
    gltfx_rslt<gltfx_atlas_allocator> made = gltfx_atlas_allocator::open(width, height);
    GLINTFX_CHECK(made.has_value());
    return std::move(made.value());
}

[[nodiscard]] bool same_rect(const gltfx_atlas_rect &rect, gltfx_atlas_rect expected) {
    return rect.x == expected.x && rect.y == expected.y && rect.width == expected.width &&
           rect.height == expected.height;
}

// The slot of a call that must succeed.
[[nodiscard]] gltfx_atlas_slot taken(gltfx_atlas_allocator &atlas, std::uint32_t width,
                                     std::uint32_t height) {
    const gltfx_rslt<gltfx_atlas_slot> slot = atlas.allocate(width, height);
    GLINTFX_CHECK(slot.has_value());
    return slot.value();
}

template <typename T>
[[nodiscard]] bool refused_as(const gltfx_rslt<T> &result, gltfx_err_code code,
                              std::string_view name) {
    return result.has_error() && result.err().code() == code &&
           result.err().rejected_value() == name;
}

struct size_case {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::string_view name;
};

} // namespace

GLINTFX_TEST(atlas_allocator_open_refuses_a_zero_side_width_first) {
    GLINTFX_CHECK(
        refused_as(gltfx_atlas_allocator::open(0, 4), gltfx_err_code::invalid_argument, "width"));
    GLINTFX_CHECK(
        refused_as(gltfx_atlas_allocator::open(4, 0), gltfx_err_code::invalid_argument, "height"));
    GLINTFX_CHECK(
        refused_as(gltfx_atlas_allocator::open(0, 0), gltfx_err_code::invalid_argument, "width"));
}

GLINTFX_TEST(atlas_allocator_open_gives_an_open_handle_of_the_size_asked) {
    const gltfx_atlas_allocator atlas = opened(64, 32);
    GLINTFX_CHECK(atlas.is_open());
    GLINTFX_CHECK_EQ(atlas.width(), 64U);
    GLINTFX_CHECK_EQ(atlas.height(), 32U);
}

// One field wrong per row, then two wrong at once to read the order: zero before larger, width
// before height. A 16 by 8 atlas.
GLINTFX_TEST(atlas_allocator_allocate_refuses_each_wrong_size_in_order) {
    gltfx_atlas_allocator atlas = opened(16, 8);
    const std::array<size_case, 7> cases{{{0, 4, "width"},
                                          {4, 0, "height"},
                                          {0, 9, "width"},
                                          {17, 0, "height"},
                                          {17, 4, "width"},
                                          {4, 9, "height"},
                                          {17, 9, "width"}}};
    std::size_t right = 0;
    for (const size_case &row : cases) {
        if (refused_as(atlas.allocate(row.width, row.height), gltfx_err_code::invalid_argument,
                       row.name)) {
            ++right;
        }
    }
    std::println("atlas_allocator_test: {} of {} size refusals right", right, cases.size());
    GLINTFX_CHECK_EQ(right, cases.size());
    GLINTFX_CHECK(same_rect(taken(atlas, 16, 8).rect, {0, 0, 16, 8}));
}

GLINTFX_TEST(atlas_allocator_rectangle_as_large_as_the_atlas_fits_and_one_pixel_more_does_not) {
    gltfx_atlas_allocator atlas = opened(32, 16);
    GLINTFX_CHECK(refused_as(atlas.allocate(33, 16), gltfx_err_code::invalid_argument, "width"));
    GLINTFX_CHECK(refused_as(atlas.allocate(32, 17), gltfx_err_code::invalid_argument, "height"));
    GLINTFX_CHECK(same_rect(taken(atlas, 32, 16).rect, {0, 0, 32, 16}));
}

// The atlas full, as a loop that opens another atlas on no_free_space would see it: a rectangle
// wider or taller than the atlas is still invalid_argument, by its side, and only a rectangle that
// fits an empty atlas is no_free_space.
GLINTFX_TEST(atlas_allocator_full_atlas_still_refuses_a_larger_rectangle_by_its_side) {
    gltfx_atlas_allocator atlas = opened(16, 8);
    const gltfx_atlas_slot whole = taken(atlas, 16, 8);
    GLINTFX_CHECK(refused_as(atlas.allocate(17, 4), gltfx_err_code::invalid_argument, "width"));
    GLINTFX_CHECK(refused_as(atlas.allocate(4, 9), gltfx_err_code::invalid_argument, "height"));
    GLINTFX_CHECK(refused_as(atlas.allocate(16, 8), gltfx_err_code::no_free_space, "atlas_full"));
    GLINTFX_CHECK(atlas.release(whole).has_value());
}

GLINTFX_TEST(atlas_allocator_full_atlas_is_refused_as_no_free_space_and_left_unchanged) {
    gltfx_atlas_allocator atlas = opened(8, 8);
    const gltfx_atlas_slot whole = taken(atlas, 8, 8);
    GLINTFX_CHECK(refused_as(atlas.allocate(1, 1), gltfx_err_code::no_free_space, "atlas_full"));
    GLINTFX_CHECK(atlas.release(whole).has_value());
    GLINTFX_CHECK(same_rect(taken(atlas, 8, 8).rect, {0, 0, 8, 8}));
}

// The fixed sequence on a 64 by 64 atlas, each position worked out from the four steps. The
// comment of each row names the step that places it.
GLINTFX_TEST(atlas_allocator_fixed_sequence_gives_the_written_positions) {
    gltfx_atlas_allocator atlas = opened(64, 64);
    const std::array<gltfx_atlas_rect, 9> expected{{
        {0, 0, 10, 8},   // step 2: the band, a shelf 8 tall at the top
        {10, 0, 20, 8},  // step 1: the shelf 8 tall
        {30, 0, 5, 6},   // step 1: 8 is at most 1.5 times 6
        {0, 8, 12, 16},  // step 2: no shelf is 16 tall, a new one below
        {12, 8, 40, 12}, // step 1: 16 is at most 1.5 times 12
        {0, 24, 29, 4},  // step 2: the shelf 8 tall has room, but it is more than 1.5 times 4
        {0, 28, 64, 36}, // step 2: the rest of the band, all of it
        {29, 24, 3, 3},  // step 1: the shelf 4 tall
        {52, 8, 2, 10},  // step 3: only the shelf 16 tall has room, and 16 is a bad fit for 10
    }};
    std::size_t right = 0;
    for (const gltfx_atlas_rect &rect : expected) {
        if (same_rect(taken(atlas, rect.width, rect.height).rect, rect)) {
            ++right;
        }
    }
    std::println("atlas_allocator_test: {} of {} positions as written", right, expected.size());
    GLINTFX_CHECK_EQ(right, expected.size());
}

GLINTFX_TEST(atlas_allocator_rect_in_a_larger_free_run_is_the_size_asked) {
    gltfx_atlas_allocator atlas = opened(32, 8);
    const gltfx_atlas_slot first = taken(atlas, 10, 8);
    GLINTFX_CHECK(same_rect(taken(atlas, 10, 8).rect, {10, 0, 10, 8}));
    GLINTFX_CHECK(same_rect(taken(atlas, 12, 8).rect, {20, 0, 12, 8}));
    GLINTFX_CHECK(atlas.release(first).has_value());
    GLINTFX_CHECK(same_rect(taken(atlas, 7, 8).rect, {0, 0, 7, 8}));
    GLINTFX_CHECK(same_rect(taken(atlas, 3, 8).rect, {7, 0, 3, 8}));
    GLINTFX_CHECK(refused_as(atlas.allocate(1, 1), gltfx_err_code::no_free_space, "atlas_full"));
}

GLINTFX_TEST(atlas_allocator_released_space_is_used_again_without_end) {
    gltfx_atlas_allocator atlas = opened(16, 16);
    std::size_t rounds = 0;
    for (std::size_t round = 0; round < 1000; ++round) {
        const gltfx_rslt<gltfx_atlas_slot> slot = atlas.allocate(16, 16);
        if (slot.has_value() && same_rect(slot.value().rect, {0, 0, 16, 16}) &&
            atlas.release(slot.value()).has_value()) {
            ++rounds;
        }
    }
    std::println("atlas_allocator_test: {} of 1000 rounds took the whole atlas again", rounds);
    GLINTFX_CHECK_EQ(rounds, std::size_t{1000});
}

// Three cells on a shelf and one on a shelf below, released in an order that leaves a hole
// between two cells first; after the four, the whole atlas is one free band again.
GLINTFX_TEST(atlas_allocator_every_release_gives_the_whole_area_back) {
    gltfx_atlas_allocator atlas = opened(32, 32);
    const gltfx_atlas_slot left = taken(atlas, 8, 8);
    const gltfx_atlas_slot middle = taken(atlas, 8, 8);
    const gltfx_atlas_slot right = taken(atlas, 8, 8);
    const gltfx_atlas_slot below = taken(atlas, 8, 16);
    GLINTFX_CHECK(same_rect(below.rect, {0, 8, 8, 16}));
    GLINTFX_CHECK(atlas.release(middle).has_value());
    GLINTFX_CHECK(atlas.release(left).has_value());
    GLINTFX_CHECK(refused_as(atlas.allocate(32, 32), gltfx_err_code::no_free_space, "atlas_full"));
    GLINTFX_CHECK(atlas.release(right).has_value());
    GLINTFX_CHECK(atlas.release(below).has_value());
    GLINTFX_CHECK(same_rect(taken(atlas, 32, 32).rect, {0, 0, 32, 32}));
}

GLINTFX_TEST(atlas_allocator_release_all_gives_the_whole_area_back_and_every_slot_goes_stale) {
    gltfx_atlas_allocator atlas = opened(32, 32);
    const gltfx_atlas_slot first = taken(atlas, 8, 8);
    const gltfx_atlas_slot second = taken(atlas, 20, 20);
    atlas.release_all();
    GLINTFX_CHECK(refused_as(atlas.release(first), gltfx_err_code::invalid_argument, "stale_slot"));
    GLINTFX_CHECK(
        refused_as(atlas.release(second), gltfx_err_code::invalid_argument, "stale_slot"));
    GLINTFX_CHECK(same_rect(taken(atlas, 32, 32).rect, {0, 0, 32, 32}));
}

GLINTFX_TEST(atlas_allocator_number_is_handed_out_again_with_the_next_generation) {
    gltfx_atlas_allocator atlas = opened(16, 16);
    const gltfx_atlas_slot first = taken(atlas, 4, 4);
    GLINTFX_CHECK(first.id == 0U && first.generation == 1U);
    GLINTFX_CHECK(atlas.release(first).has_value());
    const gltfx_atlas_slot again = taken(atlas, 4, 4);
    GLINTFX_CHECK(again.id == 0U && again.generation == 2U);
    const gltfx_atlas_slot other = taken(atlas, 4, 4);
    GLINTFX_CHECK(other.id == 1U && other.generation == 1U);
}

// The old slot of a number handed out again (same number, same rect, the next generation) is
// refused, and the space of the new owner of the number stays taken.
GLINTFX_TEST(atlas_allocator_old_slot_of_a_number_handed_out_again_frees_nothing) {
    gltfx_atlas_allocator atlas = opened(8, 8);
    const gltfx_atlas_slot old_slot = taken(atlas, 8, 8);
    GLINTFX_CHECK(atlas.release(old_slot).has_value());
    const gltfx_atlas_slot new_slot = taken(atlas, 8, 8);
    GLINTFX_CHECK(new_slot.id == old_slot.id && same_rect(new_slot.rect, {0, 0, 8, 8}));
    GLINTFX_CHECK(
        refused_as(atlas.release(old_slot), gltfx_err_code::invalid_argument, "stale_slot"));
    GLINTFX_CHECK(refused_as(atlas.allocate(1, 1), gltfx_err_code::no_free_space, "atlas_full"));
    GLINTFX_CHECK(atlas.release(new_slot).has_value());
}

GLINTFX_TEST(atlas_allocator_release_refuses_a_default_or_changed_slot_as_stale) {
    gltfx_atlas_allocator atlas = opened(16, 16);
    const gltfx_atlas_slot slot = taken(atlas, 4, 4);
    GLINTFX_CHECK(refused_as(atlas.release(gltfx_atlas_slot{}), gltfx_err_code::invalid_argument,
                             "stale_slot"));
    // One field changed per slot: each of the four of the rect, the number, the generation, the
    // owner.
    std::array<gltfx_atlas_slot, 7> changed{slot, slot, slot, slot, slot, slot, slot};
    changed[0].rect.x += 1U;
    changed[1].rect.y += 1U;
    changed[2].rect.width += 1U;
    changed[3].rect.height += 1U;
    changed[4].id += 1U;
    changed[5].generation += 1U;
    changed[6].owner = 0U;
    std::size_t refused = 0;
    for (const gltfx_atlas_slot &one : changed) {
        if (refused_as(atlas.release(one), gltfx_err_code::invalid_argument, "stale_slot")) {
            ++refused;
        }
    }
    std::println("atlas_allocator_test: {} of {} changed slots refused", refused, changed.size());
    GLINTFX_CHECK_EQ(refused, changed.size());
    GLINTFX_CHECK(atlas.release(slot).has_value());
    GLINTFX_CHECK(refused_as(atlas.release(slot), gltfx_err_code::invalid_argument, "stale_slot"));
}

// Two allocators of the same size hand out the same rect, number and generation first; only the
// owner tells the two slots apart.
GLINTFX_TEST(atlas_allocator_release_refuses_a_slot_of_another_allocator) {
    gltfx_atlas_allocator mine = opened(16, 16);
    gltfx_atlas_allocator theirs = opened(16, 16);
    const gltfx_atlas_slot my_slot = taken(mine, 4, 4);
    const gltfx_atlas_slot their_slot = taken(theirs, 4, 4);
    GLINTFX_CHECK(my_slot.id == their_slot.id && my_slot.generation == their_slot.generation);
    GLINTFX_CHECK(my_slot.owner != 0U && their_slot.owner != 0U);
    GLINTFX_CHECK(my_slot.owner != their_slot.owner);
    GLINTFX_CHECK(refused_as(mine.release(their_slot), gltfx_err_code::invalid_argument,
                             "slot_other_allocator"));
    GLINTFX_CHECK(mine.release(my_slot).has_value());
    GLINTFX_CHECK(theirs.release(their_slot).has_value());
    // Foreign AND stale (released by its own allocator): the owner is read first.
    GLINTFX_CHECK(refused_as(mine.release(their_slot), gltfx_err_code::invalid_argument,
                             "slot_other_allocator"));
}

GLINTFX_TEST(atlas_allocator_moved_from_refuses_by_name_and_holds_nothing) {
    gltfx_atlas_allocator source = opened(16, 16);
    const gltfx_atlas_slot slot = taken(source, 4, 4);
    gltfx_atlas_allocator target = std::move(source);
    // The moved-from handle is read on purpose: its state is the promise under test.
    // NOLINTBEGIN(bugprone-use-after-move,clang-analyzer-cplusplus.Move) reason: see above
    GLINTFX_CHECK(!source.is_open());
    GLINTFX_CHECK(source.width() == 0U && source.height() == 0U);
    // Each with another wrong argument too: the moved-from handle is read first.
    GLINTFX_CHECK(refused_as(source.allocate(0, 0), gltfx_err_code::invalid_argument, "allocator"));
    GLINTFX_CHECK(refused_as(source.release(gltfx_atlas_slot{}), gltfx_err_code::invalid_argument,
                             "allocator"));
    source.release_all();
    // NOLINTEND(bugprone-use-after-move,clang-analyzer-cplusplus.Move) reason: closes the block
    GLINTFX_CHECK(target.is_open() && target.width() == 16U);
    GLINTFX_CHECK(target.release(slot).has_value());
}

// The move assignment: the target gives its own area back and takes the source's. The source's
// slot is the target's now; the target's old slot is another allocator's; the source holds
// nothing.
GLINTFX_TEST(atlas_allocator_move_assignment_takes_the_other_allocator) {
    gltfx_atlas_allocator target = opened(8, 8);
    gltfx_atlas_allocator source = opened(32, 16);
    const gltfx_atlas_slot old_slot = taken(target, 4, 4);
    const gltfx_atlas_slot moved_slot = taken(source, 4, 4);
    target = std::move(source);
    // The moved-from handle is read on purpose: its state is the promise under test.
    // NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move) reason: see above
    GLINTFX_CHECK(!source.is_open());
    GLINTFX_CHECK(target.is_open() && target.width() == 32U && target.height() == 16U);
    GLINTFX_CHECK(refused_as(target.release(old_slot), gltfx_err_code::invalid_argument,
                             "slot_other_allocator"));
    GLINTFX_CHECK(target.release(moved_slot).has_value());
}
