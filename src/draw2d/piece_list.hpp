// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

#include <glintfx/core/color.hpp>

#include "draw2d/draw_order.hpp"
#include "draw2d/pod_buffer.hpp"
#include "draw2d/quad_vertices.hpp"

// draw2d/piece_list.hpp - R2D-BATCH, fatia B4 (docs/plano-w7d.md sec. 4.3, D-W7D-11,
// docs/auditoria-api- draw2d.md B0 "THE ORDER, frozen"): the PENDING pieces of the frame - each one
// already turned into pixel corners by the batch transform in force when it was submitted - held
// until the frame is flushed, where they are put in PAINT ORDER, (layer, order of submission),
// before they go into the triangle batch. A lower layer is painted first; on the same layer the
// order of submission is kept, every frame, on every system.
//
// The submission counter is the size of the list: it starts again at zero after clear(), which is
// what makes the order NOT cross a flush() barrier (a piece submitted after a flush is painted over
// everything before it, whatever its layer). Memory as the batch has it (pod_buffer.hpp): no
// std::vector, no exception, a piece is added whole or not at all. The sort works IN PLACE on the
// pieces themselves, by the key (layer, submission) each one already carries (D-B4-2): it allocates
// nothing and cannot fail, so the paint order never depends on memory.
namespace glintfx::draw2d {

struct pending_piece {
    quad_corners_pixel corners{};
    glintfx::gltfx_rgba color{};
    std::int32_t layer = k_default_draw_layer;
    std::uint32_t submission = 0;
};

class piece_list {
  public:
    // `max_pieces` bounds the list (the submission counter is 32 bits); a test lowers it.
    explicit piece_list(
        batch_allocator allocator = default_batch_allocator(),
        std::size_t max_pieces = std::numeric_limits<std::uint32_t>::max()) noexcept;
    ~piece_list();
    piece_list(const piece_list &) = delete;
    piece_list &operator=(const piece_list &) = delete;
    piece_list(piece_list &&) = delete;
    piece_list &operator=(piece_list &&) = delete;

    // Room for `pieces` MORE pieces in one go; false (contents unchanged) when memory ran out or
    // the size overflows.
    [[nodiscard]] bool reserve(std::size_t pieces) noexcept;

    // Adds one piece at the end. False, with the list unchanged, when memory ran out or the list is
    // at its bound (the caller counts the piece as dropped).
    [[nodiscard]] bool add(const quad_corners_pixel &corners, glintfx::gltfx_rgba color,
                           std::int32_t layer) noexcept;

    [[nodiscard]] std::size_t size() const noexcept;

    // Puts the pieces in paint order, in place. Allocates nothing and cannot fail. Every piece on
    // one layer (the common case) is already in paint order and costs nothing.
    void sort() noexcept;

    // The piece at `position` of the list: the paint order after sort(), the submission order
    // before it. `position` must be below size().
    [[nodiscard]] const pending_piece &painted(std::size_t position) const noexcept;

    // Empties the list and KEEPS its capacity; the submission counter starts again at zero.
    void clear() noexcept;

  private:
    batch_allocator allocator;
    std::size_t bound;
    pod_buffer<pending_piece> piece_store;
    bool one_layer = true;
};

} // namespace glintfx::draw2d
