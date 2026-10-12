// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/triangle_list_check.hpp"

#include <cmath>
#include <limits>

namespace glintfx::draw2d {

namespace {

// The largest vertex count a 32-bit index names (D-W8-287): one more is refused.
constexpr std::uint64_t k_largest_vertex_count = std::numeric_limits<std::uint32_t>::max();

// The number that the call's counts are about: indices for an indexed call, vertices otherwise.
[[nodiscard]] std::uint64_t counted_items(const triangle_call_counts &counts) noexcept {
    return counts.indexed ? counts.index_count : counts.vertex_count;
}

[[nodiscard]] bool
any_position_not_finite(std::span<const gltfx_vertex_2d_world> vertices) noexcept {
    for (const gltfx_vertex_2d_world &vertex : vertices) {
        if (!std::isfinite(vertex.position.x) || !std::isfinite(vertex.position.y)) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool any_colour_nan(std::span<const gltfx_vertex_2d_world> vertices) noexcept {
    for (const gltfx_vertex_2d_world &vertex : vertices) {
        const gltfx_rgba &colour = vertex.color;
        if (std::isnan(colour.red) || std::isnan(colour.green) || std::isnan(colour.blue) ||
            std::isnan(colour.alpha)) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool any_colour_infinite(std::span<const gltfx_vertex_2d_world> vertices) noexcept {
    for (const gltfx_vertex_2d_world &vertex : vertices) {
        const gltfx_rgba &colour = vertex.color;
        if (std::isinf(colour.red) || std::isinf(colour.green) || std::isinf(colour.blue) ||
            std::isinf(colour.alpha)) {
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
    if (any_position_not_finite(vertices)) {
        return gltfx_draw_2d_refusal::not_a_number;
    }
    if (any_colour_nan(vertices)) {
        return gltfx_draw_2d_refusal::not_a_number;
    }
    if (any_colour_infinite(vertices)) {
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
