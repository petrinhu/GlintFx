// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "draw2d/pod_buffer.hpp"
#include "draw2d/triangle_batch.hpp"

// draw2d/triangle_store.hpp - R2D-SHAPES, fatia S2b-0 (docs/plano-fechamento-w8-r19.md, D-W8-367):
// the DEPOSIT of the triangle pieces of a frame - the pixel vertices and the local indices they
// were submitted with, copied in and held until the flush. It is a subject of its own (GODS_LAWS.md
// L-17): the piece list (piece_list.hpp) decides the ORDER of the pieces, this store only keeps
// what they carry. Memory as the batch has it (pod_buffer.hpp): through a realloc-shaped allocator,
// no std::vector, no exception. A refused room leaves the store exactly as it was.
namespace glintfx::draw2d {

// Where the vertices and the local indices of one triangle piece start in the store.
struct triangle_position {
    std::size_t first_vertex = 0;
    std::size_t first_index = 0;
};

class triangle_store {
  public:
    explicit triangle_store(batch_allocator allocator = default_batch_allocator()) noexcept;
    ~triangle_store();
    triangle_store(const triangle_store &) = delete;
    triangle_store &operator=(const triangle_store &) = delete;
    triangle_store(triangle_store &&) = delete;
    triangle_store &operator=(triangle_store &&) = delete;

    // Room for `vertex_count` more vertices and `index_count` more indices, in one go. False, the
    // store unchanged, when memory ran out or the size would overflow.
    [[nodiscard]] bool room_for(std::size_t vertex_count, std::size_t index_count) noexcept;

    // Copies the vertices and the local indices in at the end, and says where they start. False,
    // the store unchanged, when memory ran out. The caller's arrays may change after the call.
    [[nodiscard]] bool append(std::span<const colored_pixel_vertex> vertices,
                              std::span<const std::uint32_t> local_indices,
                              triangle_position &where) noexcept;

    // The `count` vertices, and the `count` indices, that start at `first`.
    [[nodiscard]] std::span<const colored_pixel_vertex> vertices(std::size_t first,
                                                                 std::size_t count) const noexcept;
    [[nodiscard]] std::span<const std::uint32_t> indices(std::size_t first,
                                                         std::size_t count) const noexcept;

    // Empties the store and KEEPS its capacity.
    void clear() noexcept;

  private:
    batch_allocator allocator;
    pod_buffer<colored_pixel_vertex> vertex_store;
    pod_buffer<std::uint32_t> index_store;
};

} // namespace glintfx::draw2d
