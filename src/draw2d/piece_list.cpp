// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/piece_list.hpp"

#include <algorithm>

namespace glintfx::draw2d {

piece_list::piece_list(batch_allocator allocator_in, std::size_t max_pieces) noexcept
    : allocator(allocator_in), bound(max_pieces) {}

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
    pending_piece &piece = piece_store.data[piece_store.size];
    piece.corners = corners;
    piece.color = color;
    piece.layer = layer;
    piece.submission = static_cast<std::uint32_t>(piece_store.size);
    if (piece_store.size > 0 && layer != piece_store.data[0].layer) {
        one_layer = false;
    }
    ++piece_store.size;
    return true;
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
    one_layer = true;
}

} // namespace glintfx::draw2d
