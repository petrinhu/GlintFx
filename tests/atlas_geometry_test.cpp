// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <cstdint>

#include <glintfx/core/atlas_allocator.hpp>

#include "core/atlas_geometry.hpp"
#include "core/atlas_memory.hpp"
#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// atlas_geometry_test.cpp - R2D-TEXTURE (core/atlas_geometry.hpp; GODS_LAWS.md L-17, L-20,
// L-22). The shelves and free runs of one atlas, alone: the four steps of the placement, one per
// case, the joining of what is freed, and the memory refused. atlas_geometry.cpp is compiled a
// second time into this test (tests/CMakeLists.txt): it has no GLINTFX_API symbol. Every position
// is a literal worked out by hand from the steps, never read back from the program.

namespace {

using glintfx::atlas_geometry;
using glintfx::atlas_memory;
using glintfx::atlas_place_outcome;
using glintfx::atlas_placement;
using glintfx::atlas_run;
using glintfx::atlas_spot_search;
using glintfx::default_atlas_memory;
using glintfx::gltfx_atlas_rect;

// A memory that answers like realloc until it is told to refuse, and counts the requests.
struct armed_state {
    int requests = 0;
    bool refuse = false;
};
thread_local armed_state g_armed;

[[nodiscard]] void *armed_reallocate(void *block, std::size_t bytes) noexcept {
    ++g_armed.requests;
    if (g_armed.refuse) {
        return nullptr; // like realloc: the old block is untouched
    }
    return default_atlas_memory().reallocate(block, bytes);
}
void armed_release(void *block) noexcept { default_atlas_memory().release(block); }
[[nodiscard]] atlas_memory armed() {
    g_armed = armed_state{};
    return atlas_memory{armed_reallocate, armed_release};
}

// An open geometry, closed at the end of the case.
struct open_geometry {
    atlas_geometry geometry{};
    open_geometry(std::uint32_t width, std::uint32_t height) {
        GLINTFX_CHECK(glintfx::open_atlas_geometry(geometry, armed(), width, height));
    }
    open_geometry(const open_geometry &) = delete;
    open_geometry &operator=(const open_geometry &) = delete;
    open_geometry(open_geometry &&) = delete;
    open_geometry &operator=(open_geometry &&) = delete;
    ~open_geometry() { glintfx::close_atlas_geometry(geometry); }
};

// Places a rectangle that must be placed, and says whether it landed at (x, y).
[[nodiscard]] bool placed_at(atlas_geometry &geometry, gltfx_atlas_rect rect) {
    const atlas_placement placement = glintfx::place_in_atlas(geometry, rect.width, rect.height);
    return placement.outcome == atlas_place_outcome::placed && placement.x == rect.x &&
           placement.y == rect.y;
}

[[nodiscard]] bool same_run(const atlas_run &run, const atlas_run &expected) {
    return run.shelf_y == expected.shelf_y && run.shelf_height == expected.shelf_height &&
           run.x == expected.x && run.width == expected.width && run.taken == expected.taken;
}

[[nodiscard]] bool spot_opens_shelf_on(const atlas_spot_search &search, std::uint32_t shelf_y,
                                       const atlas_geometry &geometry) {
    return search.found && search.spot.opens_shelf &&
           geometry.runs.data[search.spot.run].shelf_y == shelf_y;
}

} // namespace

GLINTFX_TEST(atlas_geometry_opens_as_one_free_band) {
    const open_geometry open(40, 20);
    GLINTFX_CHECK_EQ(open.geometry.runs.size, std::size_t{1});
    GLINTFX_CHECK(same_run(open.geometry.runs.data[0], atlas_run{0, 20, 0, 40, false}));
}

GLINTFX_TEST(atlas_geometry_open_reports_the_memory_refused) {
    atlas_geometry geometry{};
    const atlas_memory memory = armed();
    g_armed.refuse = true;
    GLINTFX_CHECK(!glintfx::open_atlas_geometry(geometry, memory, 8, 8));
    GLINTFX_CHECK(geometry.runs.data == nullptr && geometry.runs.size == 0);
    glintfx::close_atlas_geometry(geometry);
}

// Step 1: of the shelves that hold a rectangle and are good fits, the shortest; on a tie, the
// topmost. A 16 wide atlas: two shelves 8 tall and one 12 tall, each with 6 free columns.
GLINTFX_TEST(atlas_geometry_step_one_takes_the_shortest_good_shelf_then_the_topmost) {
    open_geometry open(16, 64);
    atlas_geometry &geometry = open.geometry;
    GLINTFX_CHECK(placed_at(geometry, {0, 0, 10, 8}));
    GLINTFX_CHECK(placed_at(geometry, {0, 8, 10, 8}));
    GLINTFX_CHECK(placed_at(geometry, {0, 16, 10, 12}));
    GLINTFX_CHECK(placed_at(geometry, {10, 0, 4, 8}));  // shelves 8 and 12 tall fit 8: the shortest
    GLINTFX_CHECK(placed_at(geometry, {14, 0, 2, 7}));  // two shelves 8 tall with room: the topmost
    GLINTFX_CHECK(placed_at(geometry, {10, 8, 6, 8}));  // the top shelf is full: the next 8 tall
    GLINTFX_CHECK(placed_at(geometry, {10, 16, 6, 9})); // 8 is too short, 12 is at most 1.5 times 9
}

// Step 1 passes over a shelf more than one and a half times as tall as the rectangle, even with
// room on it: 8 is more than 1.5 times 5, and at most 1.5 times 6.
GLINTFX_TEST(atlas_geometry_step_one_passes_over_a_shelf_over_one_and_a_half_times_as_tall) {
    open_geometry open(16, 32);
    atlas_geometry &geometry = open.geometry;
    GLINTFX_CHECK(placed_at(geometry, {0, 0, 4, 8}));
    GLINTFX_CHECK(placed_at(geometry, {0, 8, 4, 5}));
    GLINTFX_CHECK(placed_at(geometry, {4, 0, 4, 6}));
}

// Step 2: the topmost free band tall enough. Three full shelves 8 tall, the middle one freed: two
// free bands, at rows 8 and 24.
GLINTFX_TEST(atlas_geometry_step_two_opens_a_shelf_in_the_topmost_band_tall_enough) {
    open_geometry open(16, 32);
    atlas_geometry &geometry = open.geometry;
    GLINTFX_CHECK(placed_at(geometry, {0, 0, 16, 8}));
    GLINTFX_CHECK(placed_at(geometry, {0, 8, 16, 8}));
    GLINTFX_CHECK(placed_at(geometry, {0, 16, 16, 8}));
    glintfx::free_in_atlas(geometry, {0, 8, 16, 8});
    GLINTFX_CHECK(spot_opens_shelf_on(glintfx::find_atlas_spot(geometry, 4, 8), 8, geometry));
    GLINTFX_CHECK(!glintfx::find_atlas_spot(geometry, 4, 9).found);
    GLINTFX_CHECK(placed_at(geometry, {0, 8, 4, 5}));
    GLINTFX_CHECK(same_run(geometry.runs.data[3], atlas_run{13, 3, 0, 16, false}));
}

// Step 3: a shelf that is a bad fit is taken when no good shelf and no band has room.
GLINTFX_TEST(atlas_geometry_step_three_takes_a_bad_fit_when_nothing_else_has_room) {
    open_geometry open(16, 16);
    atlas_geometry &geometry = open.geometry;
    GLINTFX_CHECK(placed_at(geometry, {0, 0, 4, 16}));
    const atlas_spot_search search = glintfx::find_atlas_spot(geometry, 4, 4);
    GLINTFX_CHECK(search.found && !search.spot.opens_shelf);
    GLINTFX_CHECK(placed_at(geometry, {4, 0, 4, 4}));
}

// Steps 1 and 3 take the LEFTMOST free run that is wide enough. Three cells 8 wide on one shelf,
// the left and the right freed: two free runs that both fit, neither next to the other. A good fit
// (step 1, the shelf 8 tall) and a bad fit (step 3, the shelf 16 tall for a rectangle 4 tall) land
// at x = 0, never at x = 16.
GLINTFX_TEST(atlas_geometry_steps_one_and_three_take_the_leftmost_free_run) {
    open_geometry good(24, 8);
    open_geometry bad(24, 16);
    std::uint32_t placed = 0;
    for (std::uint32_t cell = 0; cell < 3; ++cell) {
        placed += placed_at(good.geometry, {cell * 8U, 0, 8, 8}) ? 1U : 0U;
        placed += placed_at(bad.geometry, {cell * 8U, 0, 8, 16}) ? 1U : 0U;
    }
    GLINTFX_CHECK_EQ(placed, 6U);
    glintfx::free_in_atlas(good.geometry, {0, 0, 8, 8});
    glintfx::free_in_atlas(good.geometry, {16, 0, 8, 8});
    glintfx::free_in_atlas(bad.geometry, {0, 0, 8, 16});
    glintfx::free_in_atlas(bad.geometry, {16, 0, 8, 16});
    GLINTFX_CHECK(good.geometry.runs.size == 3 && bad.geometry.runs.size == 3);
    GLINTFX_CHECK(placed_at(good.geometry, {0, 0, 4, 8}));
    GLINTFX_CHECK(placed_at(bad.geometry, {0, 0, 4, 4}));
}

// Three cells on the only shelf, freed middle, left, right: the free runs join into one, and the
// shelf is a free band again.
GLINTFX_TEST(atlas_geometry_freed_runs_join_with_both_neighbours) {
    open_geometry open(30, 8);
    atlas_geometry &geometry = open.geometry;
    GLINTFX_CHECK(placed_at(geometry, {0, 0, 10, 8}));
    GLINTFX_CHECK(placed_at(geometry, {10, 0, 10, 8}));
    GLINTFX_CHECK(placed_at(geometry, {20, 0, 10, 8}));
    glintfx::free_in_atlas(geometry, {10, 0, 10, 8});
    GLINTFX_CHECK_EQ(geometry.runs.size, std::size_t{3});
    glintfx::free_in_atlas(geometry, {0, 0, 10, 8});
    GLINTFX_CHECK_EQ(geometry.runs.size, std::size_t{2});
    GLINTFX_CHECK(same_run(geometry.runs.data[0], atlas_run{0, 8, 0, 20, false}));
    glintfx::free_in_atlas(geometry, {20, 0, 10, 8});
    GLINTFX_CHECK_EQ(geometry.runs.size, std::size_t{1});
    GLINTFX_CHECK(same_run(geometry.runs.data[0], atlas_run{0, 8, 0, 30, false}));
}

// A shelf left empty between two free bands joins both.
GLINTFX_TEST(atlas_geometry_freed_shelf_joins_the_bands_above_and_below) {
    open_geometry open(16, 32);
    atlas_geometry &geometry = open.geometry;
    GLINTFX_CHECK(placed_at(geometry, {0, 0, 16, 8}));
    GLINTFX_CHECK(placed_at(geometry, {0, 8, 16, 8}));
    GLINTFX_CHECK(placed_at(geometry, {0, 16, 16, 8}));
    glintfx::free_in_atlas(geometry, {0, 8, 16, 8});
    GLINTFX_CHECK_EQ(geometry.runs.size, std::size_t{4});
    glintfx::free_in_atlas(geometry, {0, 16, 16, 8});
    GLINTFX_CHECK_EQ(geometry.runs.size, std::size_t{2});
    GLINTFX_CHECK(same_run(geometry.runs.data[1], atlas_run{8, 24, 0, 16, false}));
}

GLINTFX_TEST(atlas_geometry_clear_makes_the_whole_atlas_one_band) {
    open_geometry open(16, 16);
    atlas_geometry &geometry = open.geometry;
    GLINTFX_CHECK(placed_at(geometry, {0, 0, 4, 4}));
    GLINTFX_CHECK(placed_at(geometry, {0, 4, 9, 9}));
    glintfx::clear_atlas_geometry(geometry);
    GLINTFX_CHECK_EQ(geometry.runs.size, std::size_t{1});
    GLINTFX_CHECK(same_run(geometry.runs.data[0], atlas_run{0, 16, 0, 16, false}));
}

// Seven cells 8 wide on the only shelf leave 8 runs, the room the first growth gave. With the
// memory refusing, a placement that needs one more run is refused and the runs are as they were;
// the exact fit, which needs no new run, is still placed.
GLINTFX_TEST(atlas_geometry_refused_memory_leaves_the_runs_as_they_were) {
    open_geometry open(64, 8);
    atlas_geometry &geometry = open.geometry;
    std::uint32_t placed = 0;
    for (std::uint32_t cell = 0; cell < 7; ++cell) {
        placed += placed_at(geometry, {cell * 8U, 0, 8, 8}) ? 1U : 0U;
    }
    GLINTFX_CHECK_EQ(placed, 7U);
    GLINTFX_CHECK(geometry.runs.size == 8 && geometry.runs.capacity == 8);
    std::array<atlas_run, 8> before{};
    for (std::size_t index = 0; index < before.size(); ++index) {
        before[index] = geometry.runs.data[index];
    }
    g_armed.refuse = true;
    const int requests = g_armed.requests;
    GLINTFX_CHECK(glintfx::place_in_atlas(geometry, 4, 8).outcome ==
                  atlas_place_outcome::no_memory);
    GLINTFX_CHECK_EQ(g_armed.requests, requests + 1);
    GLINTFX_CHECK_EQ(geometry.runs.size, std::size_t{8});
    std::size_t unchanged = 0;
    for (std::size_t index = 0; index < before.size(); ++index) {
        unchanged += same_run(geometry.runs.data[index], before[index]) ? 1U : 0U;
    }
    GLINTFX_CHECK_EQ(unchanged, before.size());
    GLINTFX_CHECK(placed_at(geometry, {56, 0, 8, 8}));
    g_armed.refuse = false;
}
