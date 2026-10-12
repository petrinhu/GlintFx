// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/triangle_store.hpp"

#include <algorithm>
#include <cassert>

namespace glintfx::draw2d {

triangle_store::triangle_store(batch_allocator allocator_in) noexcept : allocator(allocator_in) {}

triangle_store::~triangle_store() {
    allocator.release(vertex_store.data);
    allocator.release(index_store.data);
}

// The vertices are asked for first, then the indices: the order the allocator is called in.
bool triangle_store::room_for(std::size_t vertex_count, std::size_t index_count) noexcept {
    return ensure_room<colored_pixel_vertex>(allocator, vertex_store, vertex_count) &&
           ensure_room<std::uint32_t>(allocator, index_store, index_count);
}

// The room is made BEFORE anything is written: a refused call leaves both buffers as they were.
bool triangle_store::append(std::span<const colored_pixel_vertex> vertices,
                            std::span<const std::uint32_t> local_indices,
                            triangle_position &where) noexcept {
    if (!room_for(vertices.size(), local_indices.size())) {
        return false;
    }
    where = triangle_position{vertex_store.size, index_store.size};
    std::copy(vertices.begin(), vertices.end(), vertex_store.data + vertex_store.size);
    vertex_store.size += vertices.size();
    std::copy(local_indices.begin(), local_indices.end(), index_store.data + index_store.size);
    index_store.size += local_indices.size();
    return true;
}

std::span<const colored_pixel_vertex> triangle_store::vertices(std::size_t first,
                                                               std::size_t count) const noexcept {
    assert(first <= vertex_store.size && count <= vertex_store.size - first);
    return std::span<const colored_pixel_vertex>(vertex_store.data + first, count);
}

std::span<const std::uint32_t> triangle_store::indices(std::size_t first,
                                                       std::size_t count) const noexcept {
    assert(first <= index_store.size && count <= index_store.size - first);
    return std::span<const std::uint32_t>(index_store.data + first, count);
}

void triangle_store::clear() noexcept {
    vertex_store.size = 0;
    index_store.size = 0;
}

} // namespace glintfx::draw2d
