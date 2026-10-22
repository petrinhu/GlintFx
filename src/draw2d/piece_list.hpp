// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>

#include <glintfx/core/color.hpp>

#include "draw2d/draw_order.hpp"
#include "draw2d/pod_buffer.hpp"
#include "draw2d/quad_vertices.hpp"
#include "draw2d/triangle_batch.hpp"
#include "draw2d/triangle_store.hpp"

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

// What a textured quad is painted with (T4b-B1): the texture's GL name, where each corner reads it,
// and the tint. Grouped so add_textured() stays within four parameters.
struct textured_quad_paint {
    std::uint32_t texture = 0;
    quad_texture_corners texture_corners = k_role_texture_corners;
    glintfx::gltfx_rgba tint{1.0F, 1.0F, 1.0F, 1.0F};
};

// What the piece is: a quadrilateral (`corners`, one `color`) or a TRIANGLE piece (S2a), whose
// vertices and local indices sit in the piece_list's own stores, at the offsets below.
enum class piece_kind : std::uint8_t { quad, triangles };

struct pending_piece {
    quad_corners_pixel corners{};
    glintfx::gltfx_rgba color{};
    std::int32_t layer = k_default_draw_layer;
    std::uint32_t submission = 0;
    piece_kind kind = piece_kind::quad;
    std::size_t first_vertex = 0; // triangles only: where its vertices start in the vertex store
    std::size_t vertex_count = 0;
    std::size_t first_index = 0; // triangles only: where its LOCAL indices start in the index store
    std::size_t index_count = 0; // 0 when the piece came with no indices (0..n-1 in order)
    // Textured triangles only (R2D-TEXTURE T5, D-W8-674): where the reads of its vertices start in
    // the read store, one read per vertex. Paid by every piece, as the texture fields below are.
    std::size_t first_read = 0;
    // The GL name of the texture, 0 for an untextured piece: a quad's (T4b-B1, D-W8-470) or a
    // triangle piece's (T5). Quads only: where each corner reads it; an untextured quad carries the
    // coordinates by role and q = 1. Inline, so the sort keeps copying one plain value: a texture
    // name and four corners more per piece, paid by every piece.
    std::uint32_t texture = 0;
    quad_texture_corners texture_corners = k_role_texture_corners;
};
static_assert(std::is_trivially_copyable_v<pending_piece>,
              "a pending piece is copied by value through the sort (D-B4-2)");

// One TEXTURED triangle piece for the list (R2D-TEXTURE T5): its geometry (pixel vertices, one read
// per vertex, local indices) and the GL name of its texture (not zero). Grouped so
// add_textured_triangles() stays within four parameters. The field is not called `triangles`: the
// list's own triangle store has that name, and tests/tools/check_noexcept_alloc.py resolves a
// receiver by NAME across the tree (measured: the second type made append() look allocating).
struct textured_triangle_piece {
    textured_triangles geometry;
    std::uint32_t texture = 0;
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

    // Adds one TEXTURED quad (T4b-B1): the texture's GL name (not zero) and where each corner reads
    // it, with `tint` as the color. The same bound and the same refusal as add().
    [[nodiscard]] bool add_textured(const quad_corners_pixel &corners,
                                    const textured_quad_paint &paint, std::int32_t layer) noexcept;

    [[nodiscard]] std::size_t size() const noexcept;

    // Adds one TRIANGLE piece (S2a): its vertices and its local indices are COPIED into the list
    // (the caller's arrays may change after the call). `local_indices` empty means 0..n-1. False,
    // list unchanged, when memory ran out or the list is at its bound.
    [[nodiscard]] bool add_triangles(std::span<const colored_pixel_vertex> vertices,
                                     std::span<const std::uint32_t> local_indices,
                                     std::int32_t layer) noexcept;

    // Adds one TEXTURED triangle piece (T5): its vertices, its reads and its local indices are
    // COPIED into the list, with the texture's GL name. add_triangles() is this call with no reads
    // and texture 0. The same bound and the same refusal: every store the piece needs grows BEFORE
    // anything is written, and an untextured piece asks the read store for nothing.
    [[nodiscard]] bool add_textured_triangles(const textured_triangle_piece &piece,
                                              std::int32_t layer) noexcept;

    // The vertices and the local indices of a TRIANGLE piece of THIS list (empty for a quad).
    [[nodiscard]] std::span<const colored_pixel_vertex>
    triangle_vertices(const pending_piece &piece) const noexcept;
    [[nodiscard]] std::span<const std::uint32_t>
    triangle_indices(const pending_piece &piece) const noexcept;
    // Where each vertex of a TEXTURED triangle piece of THIS list reads its texture (T5): one read
    // per vertex; empty for a quad and for an untextured triangle piece.
    [[nodiscard]] std::span<const triangle_texture_read>
    triangle_reads(const pending_piece &piece) const noexcept;

    // Puts the pieces in paint order, in place. Allocates nothing and cannot fail. Every piece on
    // one layer (the common case) is already in paint order and costs nothing.
    void sort() noexcept;

    // The piece at `position` of the list: the paint order after sort(), the submission order
    // before it. `position` must be below size().
    [[nodiscard]] const pending_piece &painted(std::size_t position) const noexcept;

    // Empties the list and KEEPS its capacity; the submission counter starts again at zero.
    void clear() noexcept;

  private:
    // Keeps `one_layer` true only while every piece is on the layer of the first one.
    void note_layer(std::int32_t layer) noexcept;
    // Room in one go for a triangle piece with these counts, in the piece store and in the triangle
    // store (its reads included, T5); false when refused, nothing written.
    [[nodiscard]] bool room_for_triangle_piece(const textured_triangles &piece) noexcept;

    batch_allocator allocator;
    std::size_t bound;
    pod_buffer<pending_piece> piece_store;
    bool one_layer = true;
    // The vertices and the local indices of the triangle pieces (S2b-0, D-W8-367): the list keeps
    // the order, the store keeps the data.
    triangle_store triangles;
};

} // namespace glintfx::draw2d
