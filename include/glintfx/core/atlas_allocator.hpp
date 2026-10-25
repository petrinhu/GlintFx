// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

#include <glintfx/core/err.hpp>
#include <glintfx/export.hpp>

// core/atlas_allocator.hpp - R2D-TEXTURE. Decides WHERE a rectangle goes inside a fixed area of
// pixels (an atlas), and takes the space BACK when the rectangle is released. It knows no texture,
// no graphics card and no operating system: it only hands out positions, so a texture atlas, a
// glyph cache or a sprite sheet can be built on top of it, and it works without a graphics card.
//
// HOW A RECTANGLE IS PLACED, in this version of the library. The area is cut into horizontal
// shelves, each a band of rows as tall as the rectangle that opened it; a shelf holds rectangles
// side by side, each at most as tall as the shelf. allocate() looks, in this order, for:
//   1. a shelf that already holds a rectangle, at least as tall as the new one and at most one and
//      a half times as tall, with a free run at least as wide: the shortest such shelf, then the
//      topmost, and the leftmost run on it;
//   2. otherwise, the topmost free band at least as tall: a new shelf exactly as tall as the
//      rectangle is opened at the top of that band;
//   3. otherwise, any shelf that already holds a rectangle, is at least as tall and has a free run
//      at least as wide: the shortest, then the topmost, and the leftmost run on it;
//   4. otherwise the call is refused as `no_free_space` (see the refusals below).
// The rectangle takes the left end of the free run. Shelves pack well when many rectangles have
// similar heights (the glyphs of one font size, tiles, the frames of a sprite sheet); a rectangle
// much shorter than the shelf it lands on leaves the rows below it unused until that shelf is
// empty again.
//
// THE SIZE ASKED IS THE SIZE GIVEN. The slot's rect is always exactly `width` by `height`, also
// when it lands in a larger free run; the rest of the run stays free for the next allocation.
//
// RELEASE GIVES THE SPACE BACK. release() frees the slot's rect; free runs that touch on a shelf
// become one; a shelf left with no rectangle becomes a free band again and joins the free bands
// right above and below it, so the space can take a taller rectangle. Once every slot is released,
// the whole area is one free band, as after open(). release_all() does the same for every slot at
// once.
//
// THE SAME CALLS GIVE THE SAME POSITIONS. Positions are computed with whole numbers only: with the
// same version of the library, the same sequence of calls on an allocator of the same size gives
// the same rects on every system and with every compiler. A later version may place differently;
// the steps above are this version's.
//
// A SLOT IS A RECEIPT. allocate() returns a gltfx_atlas_slot: the rect, the number of the cell
// (`id`), how many times that number was handed out (`generation`) and the number of the
// allocator that handed it out (`owner`). release() accepts only the slot of a cell that is in use
// now, exactly as allocate() returned it, and frees nothing otherwise:
//   - a slot whose `owner` is another allocator's is refused (invalid_argument, rejected_value()
//     "slot_other_allocator");
//   - a slot whose cell was already released, a slot never handed out (the default
//     gltfx_atlas_slot, whose owner is 0, among them) and a slot whose rect, id or generation was
//     changed are refused (invalid_argument, rejected_value() "stale_slot"); so releasing an old
//     slot again can never free the space of the cell that took its number later.
// Each time a number is handed out again, its generation grows by one. A number whose generation
// reached 4294967295 is never handed out again, so an old slot can never become current again.
// `owner` is a number of its own for each allocator a process opens (never 0); it repeats only
// after 4294967295 allocators were opened in the same process.
//
// THE REFUSALS, each a gltfx_err with the code and rejected_value() named here, in the order in
// which they are checked:
//   - open(width, height): a zero `width` or `height` (invalid_argument, "width" or "height");
//     the library ran out of memory (out_of_memory, "atlas_open").
//   - allocate(width, height): a moved-from allocator (invalid_argument, "allocator"); a zero
//     `width` or `height` (invalid_argument, "width" or "height"); a `width` wider or a `height`
//     taller than the atlas, which no release can ever make fit (invalid_argument, "width" or
//     "height"); no free place of that size now (no_free_space, "atlas_full"); the library ran
//     out of memory, or no number is left for a new cell, all 4294967295 being in use or retired
//     (out_of_memory, "atlas_allocate"). A refused call changes nothing.
//   - release(slot): a moved-from allocator (invalid_argument, "allocator"); a slot whose `owner`
//     is 0, which no allocator hands out ("stale_slot"); a slot of another allocator
//     ("slot_other_allocator"); any other stale slot ("stale_slot").
// `no_free_space` says the atlas is full NOW: releasing slots, or a second atlas, can make room.
// A rectangle larger than the atlas is `invalid_argument` instead, so a loop that opens a new
// atlas on `no_free_space` never runs forever on a rectangle that cannot fit in any.
//
// COST. allocate() and release() take time that grows linearly with the number of shelves and
// free runs, and the allocator's memory grows with them; release() and release_all() never ask
// for memory.
//
// A MOVED-FROM ALLOCATOR holds nothing: is_open() is false, width() and height() are 0, allocate()
// and release() are refused as above, and release_all() does nothing. An allocator that receives
// another by move assignment gives its own area back first: the slots it handed out before are
// then another allocator's. One allocator must not be used from two threads at the same time; two
// allocators are independent.
//
// Proved by: atlas_allocator_test (each refusal by name in this order but out_of_memory, which
// atlas_allocator_impl_test proves; a fixed sequence of calls with the position of every rect
// written in the test, on every system of the build; the size asked kept in a larger free run; the
// rectangle as large as the atlas, and a larger one refused by name on a full atlas; a shelf of bad
// fits still used; release, reuse of the space and of the number, the full area after every
// release and after release_all(); the stale, default, foreign slots and a slot with each of its
// fields changed; a moved-from allocator; the move assignment).
// Proved by: atlas_geometry_test (the four steps of the placement, the leftmost free run in steps
// 1 and 3, and the joining of free runs and bands); atlas_slots_test (the generation, each field
// of the rect, and the number never handed out again); atlas_allocator_impl_test (out_of_memory at
// each request for memory, with the allocator unchanged after it; release() and release_all()
// asking for none; the move assignment giving the old area back; the owner after the last one).

namespace glintfx {

// A rectangle of whole pixels inside the atlas: the left column `x`, the top row `y`, and the
// size. Trivial value.
struct gltfx_atlas_rect {
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

// The receipt of one allocation (see A SLOT IS A RECEIPT above). Trivial value.
struct gltfx_atlas_slot {
    gltfx_atlas_rect rect{};
    std::uint32_t id = 0;
    std::uint32_t generation = 0;
    std::uint32_t owner = 0;
};

struct atlas_allocator_impl;

class gltfx_atlas_allocator {
  public:
    // An atlas of `width` by `height` pixels, all free. Refusals: see above.
    [[nodiscard]] GLINTFX_API static gltfx_rslt<gltfx_atlas_allocator>
    open(std::uint32_t width, std::uint32_t height) noexcept;

    gltfx_atlas_allocator(const gltfx_atlas_allocator &) = delete;
    gltfx_atlas_allocator &operator=(const gltfx_atlas_allocator &) = delete;
    GLINTFX_API gltfx_atlas_allocator(gltfx_atlas_allocator &&other) noexcept;
    GLINTFX_API gltfx_atlas_allocator &operator=(gltfx_atlas_allocator &&other) noexcept;
    GLINTFX_API ~gltfx_atlas_allocator();

    // False only for a moved-from allocator.
    [[nodiscard]] GLINTFX_API bool is_open() const noexcept;

    // The size given to open(); 0 for a moved-from allocator.
    [[nodiscard]] GLINTFX_API std::uint32_t width() const noexcept;
    [[nodiscard]] GLINTFX_API std::uint32_t height() const noexcept;

    // A rectangle of `width` by `height` pixels, placed as described above. Refusals: see above.
    [[nodiscard]] GLINTFX_API gltfx_rslt<gltfx_atlas_slot> allocate(std::uint32_t width,
                                                                    std::uint32_t height) noexcept;

    // Gives the space of `slot` back. Refusals: see above.
    [[nodiscard]] GLINTFX_API gltfx_rslt<void> release(gltfx_atlas_slot slot) noexcept;

    // Gives the space of every slot in use back, as if each were released: every slot handed out
    // before this call is refused by release() after it.
    GLINTFX_API void release_all() noexcept;

  private:
    explicit gltfx_atlas_allocator(atlas_allocator_impl *interior) noexcept : impl(interior) {}
    atlas_allocator_impl *impl = nullptr;
};

} // namespace glintfx
