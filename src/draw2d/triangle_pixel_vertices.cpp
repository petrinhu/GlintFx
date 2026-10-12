// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/triangle_pixel_vertices.hpp"

namespace glintfx::draw2d {

namespace {

// Every vertex of the call, turned by `affine`, is in the pixel range (quad_vertices.hpp).
[[nodiscard]] bool all_in_pixel_range(std::span<const gltfx_vertex_2d_world> vertices,
                                      const pixel_affine &affine) noexcept {
    for (const gltfx_vertex_2d_world &vertex : vertices) {
        if (!pixel_position_in_range(pixel_position_of(vertex.position, affine))) {
            return false;
        }
    }
    return true;
}

} // namespace

triangle_pixel_vertices::triangle_pixel_vertices(batch_allocator allocator_in) noexcept
    : allocator(allocator_in) {}

triangle_pixel_vertices::~triangle_pixel_vertices() { allocator.release(buffer.data); }

triangle_pixels_outcome
triangle_pixel_vertices::convert(std::span<const gltfx_vertex_2d_world> vertices,
                                 const pixel_affine &affine) noexcept {
    buffer.size = 0; // only this call's vertices are held, so the room is asked for from zero
    // The range first: the same call gets the same answer whatever the memory (S2b-fix1, I-7). The
    // turn is computed again below, which is cheaper than holding doubles for every vertex.
    if (!all_in_pixel_range(vertices, affine)) {
        return triangle_pixels_outcome::pixel_position_out_of_range;
    }
    if (!ensure_room<colored_pixel_vertex>(allocator, buffer, vertices.size())) {
        return triangle_pixels_outcome::out_of_memory;
    }
    for (const gltfx_vertex_2d_world &vertex : vertices) {
        buffer.data[buffer.size] = colored_pixel_vertex{
            narrowed_to_pixel(pixel_position_of(vertex.position, affine)), vertex.color};
        ++buffer.size;
    }
    return triangle_pixels_outcome::converted;
}

std::span<const colored_pixel_vertex> triangle_pixel_vertices::converted() const noexcept {
    return std::span<const colored_pixel_vertex>(buffer.data, buffer.size);
}

} // namespace glintfx::draw2d
