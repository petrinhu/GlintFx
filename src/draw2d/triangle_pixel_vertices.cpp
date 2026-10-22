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

// The same for the vertices of a textured call (T5).
[[nodiscard]] bool all_in_pixel_range(std::span<const gltfx_textured_vertex_2d> vertices,
                                      const pixel_affine &affine) noexcept {
    for (const gltfx_textured_vertex_2d &vertex : vertices) {
        if (!pixel_position_in_range(pixel_position_of(vertex.position, affine))) {
            return false;
        }
    }
    return true;
}

// Where a texel coordinate reads the texture, in 0..1 across it (T5, D-W8-675).
[[nodiscard]] triangle_texture_read read_of(gltfx_vec2_texel texel,
                                            texture_extent extent) noexcept {
    return triangle_texture_read{static_cast<float>(texel.x / static_cast<double>(extent.width)),
                                 static_cast<float>(texel.y / static_cast<double>(extent.height))};
}

} // namespace

triangle_pixel_vertices::triangle_pixel_vertices(batch_allocator allocator_in) noexcept
    : allocator(allocator_in) {}

triangle_pixel_vertices::~triangle_pixel_vertices() {
    allocator.release(buffer.data);
    allocator.release(read_buffer.data);
}

triangle_pixels_outcome
triangle_pixel_vertices::convert(std::span<const gltfx_vertex_2d_world> vertices,
                                 const pixel_affine &affine) noexcept {
    buffer.size = 0;      // only this call's vertices are held, so the room is asked for from zero
    read_buffer.size = 0; // and an untextured call has no reads (T5)
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

triangle_pixels_outcome
triangle_pixel_vertices::convert_textured(std::span<const gltfx_textured_vertex_2d> vertices,
                                          const pixel_affine &affine,
                                          texture_extent extent) noexcept {
    buffer.size = 0; // only this call's vertices and reads are held
    read_buffer.size = 0;
    // The range first, as in convert(): the same call gets the same answer whatever the memory.
    if (!all_in_pixel_range(vertices, affine)) {
        return triangle_pixels_outcome::pixel_position_out_of_range;
    }
    if (!ensure_room<colored_pixel_vertex>(allocator, buffer, vertices.size()) ||
        !ensure_room<triangle_texture_read>(allocator, read_buffer, vertices.size())) {
        return triangle_pixels_outcome::out_of_memory;
    }
    for (const gltfx_textured_vertex_2d &vertex : vertices) {
        buffer.data[buffer.size] = colored_pixel_vertex{
            narrowed_to_pixel(pixel_position_of(vertex.position, affine)), vertex.tint};
        read_buffer.data[read_buffer.size] = read_of(vertex.texel, extent);
        ++buffer.size;
        ++read_buffer.size;
    }
    return triangle_pixels_outcome::converted;
}

std::span<const colored_pixel_vertex> triangle_pixel_vertices::converted() const noexcept {
    return std::span<const colored_pixel_vertex>(buffer.data, buffer.size);
}

std::span<const triangle_texture_read> triangle_pixel_vertices::reads() const noexcept {
    return std::span<const triangle_texture_read>(read_buffer.data, read_buffer.size);
}

} // namespace glintfx::draw2d
