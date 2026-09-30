// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/triangle_batch.hpp"

namespace glintfx::draw2d {

namespace {
constexpr std::size_t k_quad_vertices = 4;
constexpr std::size_t k_quad_indices = 6;
} // namespace

triangle_batch::triangle_batch(batch_allocator allocator_in) noexcept : allocator(allocator_in) {}

triangle_batch::~triangle_batch() {
    allocator.release(vertex_store.data);
    allocator.release(index_store.data);
    allocator.release(run_store.data);
}

bool triangle_batch::add_quad(const quad_corners_pixel &corners, glintfx::gltfx_rgba color,
                              batch_state state) noexcept {
    const bool starts_a_run =
        run_store.size == 0 || !(run_store.data[run_store.size - 1].state == state);

    // Every buffer the piece needs is grown BEFORE anything is written: a failure leaves the batch
    // exactly as it was, and the piece is counted, whole.
    if (!ensure_room<batch_vertex>(allocator, vertex_store, k_quad_vertices) ||
        !ensure_room<std::uint32_t>(allocator, index_store, k_quad_indices) ||
        (starts_a_run && !ensure_room<draw_run>(allocator, run_store, 1))) {
        ++dropped;
        return false;
    }

    // Straight linear color in, premultiplied color stored (the one place it happens).
    const glintfx::gltfx_rgba premultiplied = glintfx::gltfx_rgba_premultiplied(color);

    // Texture coordinates by ROLE: top_left, top_right, bottom_right, bottom_left.
    constexpr float k_u[k_quad_vertices] = {0.0F, 1.0F, 1.0F, 0.0F};
    constexpr float k_v[k_quad_vertices] = {0.0F, 0.0F, 1.0F, 1.0F};
    const auto base = static_cast<std::uint32_t>(vertex_store.size);
    for (std::size_t corner = 0; corner < k_quad_vertices; ++corner) {
        vertex_store.data[vertex_store.size + corner] = batch_vertex{
            corners[corner].x, corners[corner].y,   k_u[corner],        k_v[corner],
            premultiplied.red, premultiplied.green, premultiplied.blue, premultiplied.alpha};
    }
    vertex_store.size += k_quad_vertices;

    // The frozen diagonal: (top_left, top_right, bottom_right) and (top_left, bottom_right,
    // bottom_left).
    const std::uint32_t triangles[k_quad_indices] = {base, base + 1, base + 2,
                                                     base, base + 2, base + 3};
    for (std::size_t i = 0; i < k_quad_indices; ++i) {
        index_store.data[index_store.size + i] = triangles[i];
    }
    const std::size_t first_index = index_store.size;
    index_store.size += k_quad_indices;

    if (starts_a_run) {
        run_store.data[run_store.size] = draw_run{state, first_index, k_quad_indices};
        ++run_store.size;
    } else {
        run_store.data[run_store.size - 1].index_count += k_quad_indices;
    }
    return true;
}

std::span<const batch_vertex> triangle_batch::vertices() const noexcept {
    return std::span<const batch_vertex>(vertex_store.data, vertex_store.size);
}
std::span<const std::uint32_t> triangle_batch::indices() const noexcept {
    return std::span<const std::uint32_t>(index_store.data, index_store.size);
}
std::span<const draw_run> triangle_batch::runs() const noexcept {
    return std::span<const draw_run>(run_store.data, run_store.size);
}
std::size_t triangle_batch::pieces_dropped_out_of_memory() const noexcept { return dropped; }

void triangle_batch::clear() noexcept {
    vertex_store.size = 0;
    index_store.size = 0;
    run_store.size = 0;
}

} // namespace glintfx::draw2d
