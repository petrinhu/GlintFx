// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cmath>
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
// WHAT IS PROVED, ALL OF IT ON PURE DATA (no GL, no window, no allocation for the 2^32 cells):
//   - an empty call is nothing and is not a piece (D-W8-287), with or without indices;
//   - the order of the checks: counts first (including too_many_vertices, before the multiple of
//     three), then the index range, then the values;
//   - every value of a vertex is looked at: the six slots (x, y and the four colour channels),
//     enumerated whole (GODS_LAWS.md L-40, item 5), each with a NaN, and the colour ones with an
//     infinity too;
//   - a NaN anywhere in the call is named before an infinity anywhere in the call;
//   - the reason names are the tokens of the public vocabulary (frame_2d_report.hpp).
// S2b-fix1 (N1 review of 00cdbb4e, mutants M1, M2, M5 and M6): cell 12 asks 2^32 + 1 vertices, and
// the cases after the table are the ones the review found missing.

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

    // Cell 12: a count of 2^32 + 1 vertices is refused as too_many_vertices, by the counts alone,
    // and before the multiple of three (2^32 + 1 is not one either).
    ++counted;
    const triangle_call_counts too_many{(std::uint64_t{1} << 32U) + 1U, 0U, false};
    const gltfx_draw_2d_refusal too_many_reason = refusal_of_triangle_counts(too_many);
    GLINTFX_CHECK(too_many_reason == gltfx_draw_2d_refusal::too_many_vertices);
    GLINTFX_CHECK(glintfx::gltfx_draw_2d_refusal_name(too_many_reason) == "too_many_vertices");

    std::println("triangle_list_check_test: celulas={}", counted);
    GLINTFX_CHECK_EQ(counted, std::size_t{12});
}

namespace {

// One vertex of the call `slot` changed to `value`: 0 x, 1 y, 2 red, 3 green, 4 blue, 5 alpha.
[[nodiscard]] gltfx_vertex_2d_world with_value(int slot, double value) {
    gltfx_vertex_2d_world vertex = vertex_at(0.0, 10.0);
    // The colour is float: the same special value, never a narrowing cast of a NaN.
    const float narrow = std::isnan(value)     ? std::numeric_limits<float>::quiet_NaN()
                         : std::signbit(value) ? -std::numeric_limits<float>::infinity()
                                               : std::numeric_limits<float>::infinity();
    switch (slot) {
    case 0:
        vertex.position.x = value;
        break;
    case 1:
        vertex.position.y = value;
        break;
    case 2:
        vertex.color.red = narrow;
        break;
    case 3:
        vertex.color.green = narrow;
        break;
    case 4:
        vertex.color.blue = narrow;
        break;
    default:
        vertex.color.alpha = narrow;
        break;
    }
    return vertex;
}

struct value_row {
    int slot = 0;
    double value = 0.0;
    gltfx_draw_2d_refusal reason = gltfx_draw_2d_refusal::none;
};

} // namespace

// Every value of a vertex is looked at (M1 and M6 of the N1 review: the colour NaN and the y NaN
// were never asked). The rows are the closed enumeration of the slots, printed as "valores=".
GLINTFX_TEST(triangle_list_check_every_value_of_a_vertex_is_looked_at) {
    constexpr double infinity = std::numeric_limits<double>::infinity();
    const std::array<value_row, 14> rows{{
        {0, k_nan, gltfx_draw_2d_refusal::not_a_number},
        {1, k_nan, gltfx_draw_2d_refusal::not_a_number},
        {2, k_nan, gltfx_draw_2d_refusal::not_a_number},
        {3, k_nan, gltfx_draw_2d_refusal::not_a_number},
        {4, k_nan, gltfx_draw_2d_refusal::not_a_number},
        {5, k_nan, gltfx_draw_2d_refusal::not_a_number},
        {2, infinity, gltfx_draw_2d_refusal::infinite},
        {3, infinity, gltfx_draw_2d_refusal::infinite},
        {4, -infinity, gltfx_draw_2d_refusal::infinite},
        {5, infinity, gltfx_draw_2d_refusal::infinite},
        // S2b-fix1 (I-3 of the N1 review, D-W8-287 (c), docs/auditoria-api-shapes.md:392): an
        // infinite POSITION is infinite, the same as a quad corner; not_a_number is the NaN only.
        {0, infinity, gltfx_draw_2d_refusal::infinite},
        {0, -infinity, gltfx_draw_2d_refusal::infinite},
        {1, infinity, gltfx_draw_2d_refusal::infinite},
        {1, -infinity, gltfx_draw_2d_refusal::infinite},
    }};
    std::size_t looked = 0;
    for (const value_row &row : rows) {
        ++looked;
        const std::array<gltfx_vertex_2d_world, 3> call{vertex_at(0.0, 0.0), vertex_at(10.0, 0.0),
                                                        with_value(row.slot, row.value)};
        const gltfx_draw_2d_refusal got = refusal_of_triangle_call(call, {}, false);
        if (got != row.reason) {
            std::println("triangle_list_check_test: FALHOU o valor {} = {}", row.slot, row.value);
        }
        GLINTFX_CHECK(got == row.reason);
    }
    std::println("triangle_list_check_test: valores={}", looked);
    GLINTFX_CHECK_EQ(looked, rows.size());
    GLINTFX_CHECK(looked > 0U);
}

// M5 of the N1 review: an index out of range AND a NaN position in the same call. The index range
// is checked before the values, so the call is refused as index_out_of_range.
GLINTFX_TEST(triangle_list_check_the_index_range_comes_before_the_values) {
    const std::array<gltfx_vertex_2d_world, 4> quad{vertex_at(k_nan, 0.0), vertex_at(10.0, 0.0),
                                                    vertex_at(10.0, 10.0), vertex_at(0.0, 10.0)};
    const std::array<std::uint32_t, 3> out_of_range{0U, 1U, 4U};
    GLINTFX_CHECK(refusal_of_triangle_call(quad, out_of_range, true) ==
                  gltfx_draw_2d_refusal::index_out_of_range);
}

// A NaN anywhere in the call is named before an infinity anywhere in the call, whatever vertex
// and whatever slot holds each of them: the order of the vocabulary (not_a_number, then infinite),
// the same as the quad's (piece_refusal.hpp).
GLINTFX_TEST(triangle_list_check_a_nan_anywhere_comes_before_an_infinity_anywhere) {
    constexpr float infinity = std::numeric_limits<float>::infinity();
    const std::array<gltfx_vertex_2d_world, 3> colour_first{
        vertex_at(0.0, 0.0, gltfx_rgba{infinity, 1.0F, 1.0F, 1.0F}), vertex_at(10.0, 0.0),
        vertex_at(0.0, k_nan)};
    GLINTFX_CHECK(refusal_of_triangle_call(colour_first, {}, false) ==
                  gltfx_draw_2d_refusal::not_a_number);
    const std::array<gltfx_vertex_2d_world, 3> position_first{
        vertex_at(std::numeric_limits<double>::infinity(), 0.0), vertex_at(10.0, 0.0),
        vertex_at(0.0, 10.0,
                  gltfx_rgba{1.0F, 1.0F, 1.0F, std::numeric_limits<float>::quiet_NaN()})};
    GLINTFX_CHECK(refusal_of_triangle_call(position_first, {}, false) ==
                  gltfx_draw_2d_refusal::not_a_number);
}

// M2 of the N1 review: the largest count one under the limit is accepted (2^32 - 1 vertices,
// indexed with three indices: 2^32 - 1 is a multiple of three, and the indexed count is the
// number of indices anyway).
GLINTFX_TEST(triangle_list_check_one_under_the_vertex_limit_is_accepted) {
    const triangle_call_counts under{(std::uint64_t{1} << 32U) - 1U, 3U, true};
    GLINTFX_CHECK(refusal_of_triangle_counts(under) == gltfx_draw_2d_refusal::none);
}

// S2b-fix1 (I-2 of the N1 review; D-W8-287 (d), docs/auditoria-api-shapes.md:392): a 32-bit index
// names the vertices 0 to 2^32 - 1, that is 2^32 of them, so 2^32 vertices are accepted and
// 2^32 + 1 are refused. Indexed, with three indices: 2^32 is not a multiple of three, and a count
// with no indices would be refused for that and prove nothing about the limit.
GLINTFX_TEST(triangle_list_check_the_vertex_limit_is_two_to_the_32_inclusive) {
    const std::uint64_t two_to_the_32 = std::uint64_t{1} << 32U;
    GLINTFX_CHECK(refusal_of_triangle_counts(triangle_call_counts{two_to_the_32, 3U, true}) ==
                  gltfx_draw_2d_refusal::none);
    GLINTFX_CHECK(refusal_of_triangle_counts(triangle_call_counts{two_to_the_32 + 1U, 3U, true}) ==
                  gltfx_draw_2d_refusal::too_many_vertices);
}
