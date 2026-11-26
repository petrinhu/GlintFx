// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/triangle_batch.hpp"

#include <cassert>
#include <limits>

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

bool triangle_batch::reserve(std::size_t pieces) noexcept {
    if (pieces == 0) {
        return true;
    }
    if (pieces > std::numeric_limits<std::size_t>::max() / k_quad_indices) {
        return false; // 6 indices per piece would overflow the size arithmetic
    }
    return ensure_room<batch_vertex>(allocator, vertex_store, pieces * k_quad_vertices) &&
           ensure_room<std::uint32_t>(allocator, index_store, pieces * k_quad_indices) &&
           ensure_room<draw_run>(allocator, run_store, pieces);
}

bool triangle_batch::add_quad(const quad_corners_pixel &corners, glintfx::gltfx_rgba color,
                              batch_state state) noexcept {
    return add_textured_quad(corners, k_role_texture_corners, color, state);
}

bool triangle_batch::add_textured_quad(const quad_corners_pixel &corners,
                                       const quad_texture_corners &texture_corners,
                                       glintfx::gltfx_rgba color, batch_state state) noexcept {
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
    const std::size_t first_index = index_store.size;
    write_quad(corners, texture_corners, glintfx::gltfx_rgba_premultiplied(color));
    record_run(state, first_index, k_quad_indices, starts_a_run);
    return true;
}

// The four vertices and the six indices of one piece, room already made (D-B7-1, Extract Function).
void triangle_batch::write_quad(const quad_corners_pixel &corners,
                                const quad_texture_corners &texture_corners,
                                glintfx::gltfx_rgba premultiplied) noexcept {
    // By ROLE: top_left, top_right, bottom_right, bottom_left. The coordinate is stored times q,
    // and q itself last (the program divides).
    const auto base = static_cast<std::uint32_t>(vertex_store.size);
    for (std::size_t corner = 0; corner < k_quad_vertices; ++corner) {
        const quad_texture_corner &texture = texture_corners[corner];
        vertex_store.data[vertex_store.size + corner] =
            batch_vertex{corners[corner].x,     corners[corner].y,   texture.u * texture.q,
                         texture.v * texture.q, premultiplied.red,   premultiplied.green,
                         premultiplied.blue,    premultiplied.alpha, texture.q};
    }
    vertex_store.size += k_quad_vertices;

    // The frozen diagonal: (top_left, top_right, bottom_right) and (top_left, bottom_right,
    // bottom_left).
    const std::uint32_t triangles[k_quad_indices] = {base, base + 1, base + 2,
                                                     base, base + 2, base + 3};
    for (std::size_t i = 0; i < k_quad_indices; ++i) {
        index_store.data[index_store.size + i] = triangles[i];
    }
    index_store.size += k_quad_indices;
}

// The piece either opens a run of its own or extends the last one.
void triangle_batch::record_run(batch_state state, std::size_t first_index, std::size_t index_count,
                                bool starts_a_run) noexcept {
    if (starts_a_run) {
        run_store.data[run_store.size] = draw_run{state, first_index, index_count};
        ++run_store.size;
    } else {
        run_store.data[run_store.size - 1].index_count += index_count;
    }
}

// A triangle piece (S2a): room first, then the vertices and the indices, then its run. The indices
// are shifted by the base of the piece, so the batch keeps one index space for every kind of piece.
// Validation of the indices (count, range) is S2b's; this unit takes what it is given.
bool triangle_batch::add_triangles(std::span<const colored_pixel_vertex> vertices,
                                   std::span<const std::uint32_t> local_indices,
                                   batch_state state) noexcept {
    return add_textured_triangles(textured_triangles{vertices, {}, local_indices}, state);
}

bool triangle_batch::add_textured_triangles(const textured_triangles &piece,
                                            batch_state state) noexcept {
    assert(piece.reads.empty() || piece.reads.size() == piece.vertices.size());
    if (piece.vertices.empty()) {
        return true; // nothing to draw: a piece with no vertices adds nothing, and is not dropped
    }
    const std::size_t index_count =
        piece.local_indices.empty() ? piece.vertices.size() : piece.local_indices.size();
    const bool starts_a_run =
        run_store.size == 0 || !(run_store.data[run_store.size - 1].state == state);
    if (!make_room_for_triangles(piece.vertices.size(), index_count, starts_a_run)) {
        ++dropped;
        return false;
    }
    const std::size_t first_index = index_store.size;
    write_triangles(piece);
    record_run(state, first_index, index_count, starts_a_run);
    return true;
}

// The vertex store, then the index store, then the run store: the order the growth is counted in.
bool triangle_batch::make_room_for_triangles(std::size_t vertex_count, std::size_t index_count,
                                             bool starts_a_run) noexcept {
    return ensure_room<batch_vertex>(allocator, vertex_store, vertex_count) &&
           ensure_room<std::uint32_t>(allocator, index_store, index_count) &&
           (!starts_a_run || ensure_room<draw_run>(allocator, run_store, 1));
}

// Called only with the room made. Each vertex keeps its position and gets its color premultiplied
// (one by one, so a half-transparent vertex never grows a dark fringe); the texture coordinate is
// the vertex's read (T5), and inert (0, 0) for an untextured piece, as before. q is 1: a triangle
// is drawn affinely.
void triangle_batch::write_triangles(const textured_triangles &piece) noexcept {
    const auto base = static_cast<std::uint32_t>(vertex_store.size);
    for (std::size_t i = 0; i < piece.vertices.size(); ++i) {
        const colored_pixel_vertex &vertex = piece.vertices[i];
        const triangle_texture_read read =
            piece.reads.empty() ? triangle_texture_read{} : piece.reads[i];
        const glintfx::gltfx_rgba premultiplied = glintfx::gltfx_rgba_premultiplied(vertex.color);
        vertex_store.data[vertex_store.size++] = batch_vertex{vertex.position.x,
                                                              vertex.position.y,
                                                              read.u,
                                                              read.v,
                                                              premultiplied.red,
                                                              premultiplied.green,
                                                              premultiplied.blue,
                                                              premultiplied.alpha,
                                                              1.0F};
    }
    const std::span<const std::uint32_t> local_indices = piece.local_indices;
    const std::size_t count = local_indices.empty() ? piece.vertices.size() : local_indices.size();
    for (std::size_t i = 0; i < count; ++i) {
        const std::uint32_t local =
            local_indices.empty() ? static_cast<std::uint32_t>(i) : local_indices[i];
        index_store.data[index_store.size++] = base + local;
    }
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
