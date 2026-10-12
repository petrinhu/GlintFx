// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <print>
#include <span>
#include <string_view>

#include <glintfx/core/color.hpp>
#include <glintfx/core/vec2.hpp>
#include <glintfx/draw2d/frame_2d_report.hpp>
#include <glintfx/draw2d/vertex_2d.hpp>

#include "draw2d/triangle_list_check.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// triangle_list_check_test.cpp - R2D-SHAPES, fatia S2b (docs/plano-fechamento-w8-r19.md, §3 S2b; r2
// §3, tabela de doze células): the PURE check of one call of fill_triangles() / fill_indexed_
// triangles(). Enumeration CLOSED: the twelve cells below, printed as "celulas=12"; a count other
// than twelve reproves the test.
//
// WHAT IS PROVED, ALL OF IT ON PURE DATA (no GL, no window, no allocation for the 2^32 cell):
//   - an empty call is nothing and is not a piece (D-W8-287), with or without indices;
//   - the order of the checks: counts first (including too_many_vertices, before the multiple of
//     three), then the index range, then the values (positions, then colours);
//   - the reason names are the tokens of the public vocabulary (frame_2d_report.hpp).

using glintfx::gltfx_draw_2d_refusal;
using glintfx::gltfx_rgba;
using glintfx::gltfx_vec2_world;
using glintfx::gltfx_vertex_2d_world;
using glintfx::draw2d::is_empty_triangle_call;
using glintfx::draw2d::refusal_of_triangle_call;
using glintfx::draw2d::refusal_of_triangle_counts;
using glintfx::draw2d::triangle_call_counts;

namespace {

constexpr gltfx_rgba k_white{1.0F, 1.0F, 1.0F, 1.0F};
constexpr float k_infinity = std::numeric_limits<float>::infinity();
constexpr double k_nan = std::numeric_limits<double>::quiet_NaN();

[[nodiscard]] gltfx_vertex_2d_world vertex_at(double x, double y, gltfx_rgba color = k_white) {
    return gltfx_vertex_2d_world{gltfx_vec2_world{x, y}, color};
}

// What one call IS for the check: nothing (not a piece), accepted, or refused for one reason.
enum class verdict : std::uint8_t { nothing, accepted, refused };

struct outcome {
    verdict kind = verdict::nothing;
    gltfx_draw_2d_refusal reason = gltfx_draw_2d_refusal::none;
};

[[nodiscard]] outcome judge(std::span<const gltfx_vertex_2d_world> vertices,
                            std::span<const std::uint32_t> indices, bool indexed) {
    const triangle_call_counts counts{vertices.size(), indexed ? indices.size() : std::size_t{0},
                                      indexed};
    if (is_empty_triangle_call(counts)) {
        return outcome{verdict::nothing, gltfx_draw_2d_refusal::none};
    }
    const gltfx_draw_2d_refusal reason = refusal_of_triangle_call(vertices, indices, indexed);
    if (reason != gltfx_draw_2d_refusal::none) {
        return outcome{verdict::refused, reason};
    }
    return outcome{verdict::accepted, gltfx_draw_2d_refusal::none};
}

struct cell {
    const char *name = "";
    std::span<const gltfx_vertex_2d_world> vertices{};
    std::span<const std::uint32_t> indices{};
    bool indexed = false;
    verdict kind = verdict::nothing;
    gltfx_draw_2d_refusal reason = gltfx_draw_2d_refusal::none;
    std::string_view token{}; // the public name of `reason`, written here as a literal
};

} // namespace

GLINTFX_TEST(triangle_list_check_closed_enumeration_of_the_twelve_cells) {
    const std::array<gltfx_vertex_2d_world, 3> triangle{vertex_at(0.0, 0.0), vertex_at(10.0, 0.0),
                                                        vertex_at(0.0, 10.0)};
    const std::array<gltfx_vertex_2d_world, 4> quad{vertex_at(0.0, 0.0), vertex_at(10.0, 0.0),
                                                    vertex_at(10.0, 10.0), vertex_at(0.0, 10.0)};
    const std::array<gltfx_vertex_2d_world, 3> not_a_number_x{
        vertex_at(k_nan, 0.0), vertex_at(10.0, 0.0), vertex_at(0.0, 10.0)};
    const std::array<gltfx_vertex_2d_world, 3> infinite_colour{
        vertex_at(0.0, 0.0), vertex_at(10.0, 0.0),
        vertex_at(0.0, 10.0, gltfx_rgba{1.0F, k_infinity, 1.0F, 1.0F})};
    const std::array<std::uint32_t, 6> two_triangles{0U, 1U, 2U, 0U, 2U, 3U};
    const std::array<std::uint32_t, 3> out_of_range{0U, 1U, 4U};
    const std::array<std::uint32_t, 2> two_indices{0U, 1U};
    const std::array<std::uint32_t, 3> first_three{0U, 1U, 2U};
    const std::span<const std::uint32_t> no_indices{};
    const std::span<const gltfx_vertex_2d_world> no_vertices{};

    const std::array<cell, 11> cells{{
        {"1 fill_triangles sem vértice", no_vertices, no_indices, false, verdict::nothing,
         gltfx_draw_2d_refusal::none, "none"},
        {"2 fill_triangles com 3 vértices", triangle, no_indices, false, verdict::accepted,
         gltfx_draw_2d_refusal::none, "none"},
        {"3 fill_triangles com 4 vértices", quad, no_indices, false, verdict::refused,
         gltfx_draw_2d_refusal::count_not_multiple_of_three, "count_not_multiple_of_three"},
        {"4 indexado, 4 vértices, dois triângulos", quad, two_triangles, true, verdict::accepted,
         gltfx_draw_2d_refusal::none, "none"},
        {"5 indexado, índice 4 fora de 4 vértices", quad, out_of_range, true, verdict::refused,
         gltfx_draw_2d_refusal::index_out_of_range, "index_out_of_range"},
        {"6 indexado, 2 índices", quad, two_indices, true, verdict::refused,
         gltfx_draw_2d_refusal::count_not_multiple_of_three, "count_not_multiple_of_three"},
        {"7 indexado, 0 vértices e índices 0,1,2", no_vertices, first_three, true, verdict::refused,
         gltfx_draw_2d_refusal::index_out_of_range, "index_out_of_range"},
        {"8 fill_triangles com x NaN", not_a_number_x, no_indices, false, verdict::refused,
         gltfx_draw_2d_refusal::not_a_number, "not_a_number"},
        {"9 fill_triangles com cor infinita", infinite_colour, no_indices, false, verdict::refused,
         gltfx_draw_2d_refusal::infinite, "infinite"},
        {"10 indexado, x NaN e 2 índices (a contagem vem antes)", not_a_number_x, two_indices, true,
         verdict::refused, gltfx_draw_2d_refusal::count_not_multiple_of_three,
         "count_not_multiple_of_three"},
        {"11 indexado, 3 vértices e sem índice", triangle, no_indices, true, verdict::nothing,
         gltfx_draw_2d_refusal::none, "none"},
    }};

    std::size_t counted = 0;
    for (const cell &row : cells) {
        ++counted;
        const outcome got = judge(row.vertices, row.indices, row.indexed);
        const bool same = got.kind == row.kind && got.reason == row.reason &&
                          glintfx::gltfx_draw_2d_refusal_name(got.reason) == row.token;
        if (!same) {
            std::println("triangle_list_check_test: FALHOU a celula {}", row.name);
        }
        GLINTFX_CHECK(same);
    }

    // Cell 6, second input (NOT a twelfth cell): two indices, one of them out of range. The counts
    // are checked before the range (rule 2 before rule 3), so the refusal names the count. A table
    // that only has {0,1} cannot tell the two orders apart.
    const std::array<std::uint32_t, 2> count_and_range{0U, 4U};
    const outcome count_first = judge(quad, count_and_range, true);
    GLINTFX_CHECK(count_first.kind == verdict::refused);
    GLINTFX_CHECK(count_first.reason == gltfx_draw_2d_refusal::count_not_multiple_of_three);

    // Cell 12: a count of 2^32 vertices is refused as too_many_vertices, by the counts alone.
    ++counted;
    const triangle_call_counts too_many{std::uint64_t{1} << 32U, 0U, false};
    const gltfx_draw_2d_refusal too_many_reason = refusal_of_triangle_counts(too_many);
    GLINTFX_CHECK(too_many_reason == gltfx_draw_2d_refusal::too_many_vertices);
    GLINTFX_CHECK(glintfx::gltfx_draw_2d_refusal_name(too_many_reason) == "too_many_vertices");

    std::println("triangle_list_check_test: celulas={}", counted);
    GLINTFX_CHECK_EQ(counted, std::size_t{12});
}
