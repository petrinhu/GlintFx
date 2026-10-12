// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/triangle_list_check.hpp"

#include <cmath>

namespace glintfx::draw2d {

namespace {

// The largest vertex count a 32-bit index can address: the indices 0 to 2^32 - 1 name 2^32
// vertices, so 2^32 is accepted and one more is refused (D-W8-287 (d); S2b-fix1, I-2).
constexpr std::uint64_t k_largest_vertex_count = std::uint64_t{1} << 32U;

// The number that the call's counts are about: indices for an indexed call, vertices otherwise.
[[nodiscard]] std::uint64_t counted_items(const triangle_call_counts &counts) noexcept {
    return counts.indexed ? counts.index_count : counts.vertex_count;
}

// The six values of a vertex: its position and its four colour channels.
[[nodiscard]] bool vertex_holds_nan(const gltfx_vertex_2d_world &vertex) noexcept {
    const gltfx_rgba &colour = vertex.color;
    return std::isnan(vertex.position.x) || std::isnan(vertex.position.y) ||
           std::isnan(colour.red) || std::isnan(colour.green) || std::isnan(colour.blue) ||
           std::isnan(colour.alpha);
}

[[nodiscard]] bool vertex_holds_infinity(const gltfx_vertex_2d_world &vertex) noexcept {
    const gltfx_rgba &colour = vertex.color;
    return std::isinf(vertex.position.x) || std::isinf(vertex.position.y) ||
           std::isinf(colour.red) || std::isinf(colour.green) || std::isinf(colour.blue) ||
           std::isinf(colour.alpha);
}

[[nodiscard]] bool any_vertex(std::span<const gltfx_vertex_2d_world> vertices,
                              bool (*holds)(const gltfx_vertex_2d_world &) noexcept) noexcept {
    for (const gltfx_vertex_2d_world &vertex : vertices) {
        if (holds(vertex)) {
            return true;
        }
    }
    return false;
}

} // namespace

bool is_empty_triangle_call(const triangle_call_counts &counts) noexcept {
    return counted_items(counts) == 0;
}

gltfx_draw_2d_refusal refusal_of_triangle_counts(const triangle_call_counts &counts) noexcept {
    if (counts.vertex_count > k_largest_vertex_count) {
        return gltfx_draw_2d_refusal::too_many_vertices;
    }
    if (counted_items(counts) % 3U != 0U) {
        return gltfx_draw_2d_refusal::count_not_multiple_of_three;
    }
    return gltfx_draw_2d_refusal::none;
}

gltfx_draw_2d_refusal refusal_of_triangle_indices(std::span<const std::uint32_t> indices,
                                                  std::uint64_t vertex_count) noexcept {
    for (const std::uint32_t index : indices) {
        if (static_cast<std::uint64_t>(index) >= vertex_count) {
            return gltfx_draw_2d_refusal::index_out_of_range;
        }
    }
    return gltfx_draw_2d_refusal::none;
}

gltfx_draw_2d_refusal
refusal_of_triangle_values(std::span<const gltfx_vertex_2d_world> vertices) noexcept {
    // The order of the vocabulary, the same as a quad's (piece_refusal.hpp): a NaN anywhere in the
    // call first, then an infinity anywhere (D-W8-287 (c); S2b-fix1, I-3).
    if (any_vertex(vertices, &vertex_holds_nan)) {
        return gltfx_draw_2d_refusal::not_a_number;
    }
    if (any_vertex(vertices, &vertex_holds_infinity)) {
        return gltfx_draw_2d_refusal::infinite;
    }
    return gltfx_draw_2d_refusal::none;
}

gltfx_draw_2d_refusal refusal_of_triangle_call(std::span<const gltfx_vertex_2d_world> vertices,
                                               std::span<const std::uint32_t> indices,
                                               bool indexed) noexcept {
    const triangle_call_counts counts{vertices.size(), indexed ? indices.size() : std::size_t{0},
                                      indexed};
    const gltfx_draw_2d_refusal by_counts = refusal_of_triangle_counts(counts);
    if (by_counts != gltfx_draw_2d_refusal::none) {
        return by_counts;
    }
    if (indexed) {
        const gltfx_draw_2d_refusal by_index =
            refusal_of_triangle_indices(indices, counts.vertex_count);
        if (by_index != gltfx_draw_2d_refusal::none) {
            return by_index;
        }
    }
    return refusal_of_triangle_values(vertices);
}

} // namespace glintfx::draw2d
