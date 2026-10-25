// SPDX-License-Identifier: AGPL-3.0-or-later
#include "core/atlas_geometry.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

// core/atlas_geometry.cpp - R2D-TEXTURE. The four steps of the placement and the joining of what
// is freed; core/atlas_geometry.hpp says the layout they keep.
namespace glintfx {

namespace {

// What steps 1 and 3 look for: the size, and whether a shelf more than one and a half times as
// tall as the rectangle is passed over (step 1) or taken (step 3).
struct shelf_request {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    bool good_fit_only = false;
};

// One past the last run of the shelf whose first run is `begin`.
std::size_t shelf_end(const atlas_geometry &geometry, std::size_t begin) noexcept {
    std::size_t end = begin + 1;
    while (end < geometry.runs.size &&
           geometry.runs.data[end].shelf_y == geometry.runs.data[begin].shelf_y) {
        ++end;
    }
    return end;
}

bool is_free_band(const atlas_geometry &geometry, const atlas_run &run) noexcept {
    return !run.taken && run.x == 0 && run.width == geometry.width;
}

bool shelf_holds_a_rectangle(const atlas_geometry &geometry, std::size_t begin,
                             std::size_t end) noexcept {
    for (std::size_t index = begin; index < end; ++index) {
        if (geometry.runs.data[index].taken) {
            return true;
        }
    }
    return false;
}

// The leftmost free run of [begin, end) at least `width` wide, or `end` when there is none.
std::size_t leftmost_free_run(const atlas_geometry &geometry, std::size_t begin, std::size_t end,
                              std::uint32_t width) noexcept {
    for (std::size_t index = begin; index < end; ++index) {
        const atlas_run &run = geometry.runs.data[index];
        if (!run.taken && run.width >= width) {
            return index;
        }
    }
    return end;
}

// A shelf at most one and a half times as tall as the rectangle.
bool is_good_fit(std::uint32_t shelf_height, std::uint32_t height) noexcept {
    return std::uint64_t{shelf_height} <= std::uint64_t{height} + (height / 2U);
}

// The spot the shelf [begin, end) offers to `request` under step 1 or 3, or found == false.
atlas_spot_search spot_on_shelf(const atlas_geometry &geometry, std::size_t begin, std::size_t end,
                                const shelf_request &request) noexcept {
    const std::uint32_t shelf_height = geometry.runs.data[begin].shelf_height;
    if (shelf_height < request.height || !shelf_holds_a_rectangle(geometry, begin, end)) {
        return {};
    }
    if (request.good_fit_only && !is_good_fit(shelf_height, request.height)) {
        return {};
    }
    const std::size_t run = leftmost_free_run(geometry, begin, end, request.width);
    if (run == end) {
        return {};
    }
    return atlas_spot_search{.found = true, .spot = {.run = run, .opens_shelf = false}};
}

// Steps 1 and 3: the shortest shelf that offers a spot, the topmost on a tie.
atlas_spot_search shortest_shelf(const atlas_geometry &geometry,
                                 const shelf_request &request) noexcept {
    atlas_spot_search best{};
    std::uint32_t best_height = 0;
    std::size_t begin = 0;
    while (begin < geometry.runs.size) {
        const std::size_t end = shelf_end(geometry, begin);
        const atlas_spot_search here = spot_on_shelf(geometry, begin, end, request);
        const std::uint32_t height = geometry.runs.data[begin].shelf_height;
        if (here.found && (!best.found || height < best_height)) {
            best = here;
            best_height = height;
        }
        begin = end;
    }
    return best;
}

// Step 2: the topmost free band at least `height` tall.
atlas_spot_search topmost_band(const atlas_geometry &geometry, std::uint32_t height) noexcept {
    for (std::size_t index = 0; index < geometry.runs.size; ++index) {
        const atlas_run &run = geometry.runs.data[index];
        if (is_free_band(geometry, run) && run.shelf_height >= height) {
            return atlas_spot_search{.found = true, .spot = {.run = index, .opens_shelf = true}};
        }
    }
    return {};
}

// How many runs taking `spot` adds: the rest of a band below a new shelf, and the rest of a run on
// the right of the rectangle.
std::size_t runs_added(const atlas_geometry &geometry, const atlas_spot &spot, std::uint32_t width,
                       std::uint32_t height) noexcept {
    const atlas_run &run = geometry.runs.data[spot.run];
    std::size_t added = run.width > width ? 1U : 0U;
    if (spot.opens_shelf && run.shelf_height > height) {
        ++added;
    }
    return added;
}

// Writes `run` at `index`, the runs from `index` on one place to the right. The room is reserved.
void insert_run(atlas_geometry &geometry, std::size_t index, const atlas_run &run) noexcept {
    std::memmove(geometry.runs.data + index + 1, geometry.runs.data + index,
                 (geometry.runs.size - index) * sizeof(atlas_run));
    geometry.runs.data[index] = run;
    ++geometry.runs.size;
}

void erase_run(atlas_geometry &geometry, std::size_t index) noexcept {
    std::memmove(geometry.runs.data + index, geometry.runs.data + index + 1,
                 (geometry.runs.size - index - 1) * sizeof(atlas_run));
    --geometry.runs.size;
}

// Cuts a shelf `height` tall off the top of the free band `index`; the rest of the band stays a
// free band right below it.
void open_shelf_in_band(atlas_geometry &geometry, std::size_t index,
                        std::uint32_t height) noexcept {
    atlas_run &band = geometry.runs.data[index];
    if (band.shelf_height == height) {
        return;
    }
    const atlas_run rest{.shelf_y = band.shelf_y + height,
                         .shelf_height = band.shelf_height - height,
                         .x = 0,
                         .width = geometry.width,
                         .taken = false};
    band.shelf_height = height;
    insert_run(geometry, index + 1, rest);
}

// Takes the left `width` of the free run `index`; the rest stays a free run right after it.
void take_left_of_run(atlas_geometry &geometry, std::size_t index, std::uint32_t width) noexcept {
    atlas_run &run = geometry.runs.data[index];
    if (run.width > width) {
        const atlas_run rest{.shelf_y = run.shelf_y,
                             .shelf_height = run.shelf_height,
                             .x = run.x + width,
                             .width = run.width - width,
                             .taken = false};
        run.width = width;
        insert_run(geometry, index + 1, rest);
    }
    geometry.runs.data[index].taken = true;
}

// The run that starts at column `x` of the shelf at row `y`, by binary search on (shelf_y, x); the
// index where it would be when there is none.
std::size_t run_at(const atlas_geometry &geometry, std::uint32_t x, std::uint32_t y) noexcept {
    std::size_t low = 0;
    std::size_t high = geometry.runs.size;
    while (low < high) {
        const std::size_t middle = low + ((high - low) / 2U);
        const atlas_run &run = geometry.runs.data[middle];
        if (run.shelf_y < y || (run.shelf_y == y && run.x < x)) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }
    return low;
}

// Joins the free run `index` with the free runs right before and right after it on its shelf, and
// returns the index of the joined run.
std::size_t join_free_runs(atlas_geometry &geometry, std::size_t index) noexcept {
    atlas_run *runs = geometry.runs.data;
    if (index + 1 < geometry.runs.size && runs[index + 1].shelf_y == runs[index].shelf_y &&
        !runs[index + 1].taken) {
        runs[index].width += runs[index + 1].width;
        erase_run(geometry, index + 1);
    }
    if (index > 0 && runs[index - 1].shelf_y == runs[index].shelf_y && !runs[index - 1].taken) {
        runs[index - 1].width += runs[index].width;
        erase_run(geometry, index);
        --index;
    }
    return index;
}

// When the run `index` is a free band, joins it with the free bands right below and right above.
void join_free_bands(atlas_geometry &geometry, std::size_t index) noexcept {
    atlas_run *runs = geometry.runs.data;
    if (!is_free_band(geometry, runs[index])) {
        return;
    }
    if (index + 1 < geometry.runs.size && is_free_band(geometry, runs[index + 1])) {
        runs[index].shelf_height += runs[index + 1].shelf_height;
        erase_run(geometry, index + 1);
    }
    if (index > 0 && is_free_band(geometry, runs[index - 1])) {
        runs[index - 1].shelf_height += runs[index].shelf_height;
        erase_run(geometry, index);
    }
}

atlas_run whole_band(const atlas_geometry &geometry) noexcept {
    return atlas_run{
        .shelf_y = 0, .shelf_height = geometry.height, .x = 0, .width = geometry.width};
}

} // namespace

bool open_atlas_geometry(atlas_geometry &geometry, atlas_memory memory, std::uint32_t width,
                         std::uint32_t height) noexcept {
    geometry = atlas_geometry{.memory = memory, .runs = {}, .width = width, .height = height};
    if (!reserve_atlas_room(geometry.memory, geometry.runs, 1)) {
        return false;
    }
    geometry.runs.data[0] = whole_band(geometry);
    geometry.runs.size = 1;
    return true;
}

void close_atlas_geometry(atlas_geometry &geometry) noexcept {
    if (geometry.runs.data != nullptr) {
        free_atlas_array(geometry.memory, geometry.runs);
    }
}

atlas_spot_search find_atlas_spot(const atlas_geometry &geometry, std::uint32_t width,
                                  std::uint32_t height) noexcept {
    const atlas_spot_search good_fit = shortest_shelf(
        geometry, shelf_request{.width = width, .height = height, .good_fit_only = true});
    if (good_fit.found) {
        return good_fit;
    }
    const atlas_spot_search band = topmost_band(geometry, height);
    if (band.found) {
        return band;
    }
    return shortest_shelf(geometry,
                          shelf_request{.width = width, .height = height, .good_fit_only = false});
}

atlas_placement place_in_atlas(atlas_geometry &geometry, std::uint32_t width,
                               std::uint32_t height) noexcept {
    const atlas_spot_search search = find_atlas_spot(geometry, width, height);
    if (!search.found) {
        return atlas_placement{.outcome = atlas_place_outcome::no_room};
    }
    if (!reserve_atlas_room(geometry.memory, geometry.runs,
                            runs_added(geometry, search.spot, width, height))) {
        return atlas_placement{.outcome = atlas_place_outcome::no_memory};
    }
    if (search.spot.opens_shelf) {
        open_shelf_in_band(geometry, search.spot.run, height);
    }
    take_left_of_run(geometry, search.spot.run, width);
    const atlas_run &run = geometry.runs.data[search.spot.run];
    return atlas_placement{.outcome = atlas_place_outcome::placed, .x = run.x, .y = run.shelf_y};
}

void free_in_atlas(atlas_geometry &geometry, const gltfx_atlas_rect &rect) noexcept {
    const std::size_t index = run_at(geometry, rect.x, rect.y);
    if (index >= geometry.runs.size || geometry.runs.data[index].x != rect.x ||
        geometry.runs.data[index].shelf_y != rect.y || !geometry.runs.data[index].taken) {
        return; // not a taken run: the caller's check failed, and nothing is touched
    }
    geometry.runs.data[index].taken = false;
    join_free_bands(geometry, join_free_runs(geometry, index));
}

void clear_atlas_geometry(atlas_geometry &geometry) noexcept {
    if (geometry.runs.capacity == 0) {
        return;
    }
    geometry.runs.data[0] = whole_band(geometry);
    geometry.runs.size = 1;
}

} // namespace glintfx
