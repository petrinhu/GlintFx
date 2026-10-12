// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/piece_list.hpp"

#include <algorithm>

namespace glintfx::draw2d {

piece_list::piece_list(batch_allocator allocator_in, std::size_t max_pieces) noexcept
    : allocator(allocator_in), bound(max_pieces), triangles(allocator_in) {}

piece_list::~piece_list() { allocator.release(piece_store.data); }

bool piece_list::reserve(std::size_t pieces) noexcept {
    if (pieces > bound - piece_store.size) {
        return false;
    }
    return ensure_room<pending_piece>(allocator, piece_store, pieces);
}

bool piece_list::add(const quad_corners_pixel &corners, glintfx::gltfx_rgba color,
                     std::int32_t layer) noexcept {
    if (piece_store.size >= bound || !ensure_room<pending_piece>(allocator, piece_store, 1)) {
        return false;
    }
    piece_store.data[piece_store.size] = pending_piece{
        corners, color, layer, static_cast<std::uint32_t>(piece_store.size), piece_kind::quad};
    note_layer(layer);
    ++piece_store.size;
    return true;
}

void piece_list::note_layer(std::int32_t layer) noexcept {
    if (piece_store.size > 0 && layer != piece_store.data[0].layer) {
        one_layer = false;
    }
}

// The vertices and the indices are COPIED into the triangle store before the piece is written: the
// piece keeps offsets into it, never a pointer to the caller's arrays.
bool piece_list::add_triangles(std::span<const colored_pixel_vertex> vertices,
                               std::span<const std::uint32_t> local_indices,
                               std::int32_t layer) noexcept {
    if (piece_store.size >= bound ||
        !room_for_triangle_piece(vertices.size(), local_indices.size())) {
        return false;
    }
    triangle_position where{};
    if (!triangles.append(vertices, local_indices, where)) {
        return false; // the room was made above, so this is not reached; nothing was written
    }
    const pending_piece piece{
        .layer = layer,
        .submission = static_cast<std::uint32_t>(piece_store.size),
        .kind = piece_kind::triangles,
        .first_vertex = where.first_vertex,
        .vertex_count = vertices.size(),
        .first_index = where.first_index,
        .index_count = local_indices.size(),
    };
    piece_store.data[piece_store.size] = piece;
    note_layer(layer);
    ++piece_store.size;
    return true;
}

std::span<const colored_pixel_vertex>
piece_list::triangle_vertices(const pending_piece &piece) const noexcept {
    if (piece.kind != piece_kind::triangles) {
        return {};
    }
    return triangles.vertices(piece.first_vertex, piece.vertex_count);
}

std::span<const std::uint32_t>
piece_list::triangle_indices(const pending_piece &piece) const noexcept {
    if (piece.kind != piece_kind::triangles) {
        return {};
    }
    return triangles.indices(piece.first_index, piece.index_count);
}

// The piece store grows first, then the triangle store; nothing is written before both have room.
bool piece_list::room_for_triangle_piece(std::size_t vertex_count,
                                         std::size_t index_count) noexcept {
    return ensure_room<pending_piece>(allocator, piece_store, 1) &&
           triangles.room_for(vertex_count, index_count);
}

std::size_t piece_list::size() const noexcept { return piece_store.size; }

void piece_list::sort() noexcept {
    if (one_layer || piece_store.size < 2) {
        return; // submission order IS the paint order
    }
    // The key (layer, submission) is unique by construction, so an ordinary in-place sort is a
    // total order: reproducible on every system, and no temporary buffer that could need memory.
    std::sort(piece_store.data, piece_store.data + piece_store.size,
              [](const pending_piece &first, const pending_piece &second) {
                  return draw_key_before(draw_key{first.layer, first.submission},
                                         draw_key{second.layer, second.submission});
              });
}

const pending_piece &piece_list::painted(std::size_t position) const noexcept {
    return piece_store.data[position];
}

void piece_list::clear() noexcept {
    piece_store.size = 0;
    triangles.clear();
    one_layer = true;
}

} // namespace glintfx::draw2d
