// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <print>
#include <span>

#include <glintfx/core/rect.hpp>
#include <glintfx/core/transform.hpp>
#include <glintfx/draw2d/stroke.hpp>

#include "draw2d/quad_vertices.hpp"
#include "draw2d/stroke_rect_geometry.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// stroke_rect_geometry_test.cpp - R2D-SHAPES, fatia S4 (D-W8-271, D-W8-273): the two polygons of a
// rectangle outline on the SCREEN (draw2d/stroke_rect_geometry.hpp), for each alignment and each
// unit, with and without a hole, under zoom, turn, mirror and a flattened side, and the points and
// triangles written from them. Pure: no GL, no window.
//
// THE EXPECTED VALUES ARE COMPUTED OUTSIDE THE CODE, BEFORE IT (GODS_LAWS.md L-43): the polygons on
// the identity and under a zoom are written by hand from the plan (plano-fechamento-w8-r2.md, S4,
// item 3: x = 10, y = 10, width 40, height 20, thickness 4); those under a turn, a mirror and a
// flattened side by contas_s4.py (ordem S4, Anexo C), with the house transform written out (scale,
// then rotation, then translation) and an independent offset along the two sides.
// Helpers return prvalues, never a named local (INBOX MSVC-ASAN-PADDING-RETURN).

using glintfx::gltfx_rect_outline_style;
using glintfx::gltfx_rect_world;
using glintfx::gltfx_stroke;
using glintfx::gltfx_stroke_align;
using glintfx::gltfx_stroke_unit;
using glintfx::gltfx_transform;
using glintfx::draw2d::k_rect_outline_max_indices;
using glintfx::draw2d::k_rect_outline_max_points;
using glintfx::draw2d::pixel_affine;
using glintfx::draw2d::pixel_affine_from_transform;
using glintfx::draw2d::pixel_position_wide;
using glintfx::draw2d::rect_outline;
using glintfx::draw2d::rect_outline_of;
using glintfx::draw2d::write_rect_outline_indices;
using glintfx::draw2d::write_rect_outline_points;

namespace {

using corners = std::array<pixel_position_wide, 4>;

constexpr gltfx_rect_world k_rect{{10.0, 10.0}, {40.0, 20.0}};
constexpr double k_tolerance = 1e-9;

[[nodiscard]] gltfx_rect_outline_style style_of(double thickness, gltfx_stroke_unit unit,
                                                gltfx_stroke_align align) {
    return gltfx_rect_outline_style{gltfx_stroke{thickness, unit}, align};
}

[[nodiscard]] pixel_affine affine_of(double tx, double ty, double degrees, double sx, double sy) {
    return pixel_affine_from_transform(gltfx_transform{
        .translation = {tx, ty},
        .rotation = {degrees * std::numbers::pi / 180.0},
        .scale = {sx, sy},
    });
}

[[nodiscard]] pixel_affine identity() { return pixel_affine{}; }

[[nodiscard]] bool near(const pixel_position_wide &got, const pixel_position_wide &want) {
    return std::fabs(got.x - want.x) <= k_tolerance && std::fabs(got.y - want.y) <= k_tolerance;
}

[[nodiscard]] bool same_corners(const corners &got, const corners &want) {
    for (std::size_t k = 0; k < 4; ++k) {
        if (!near(got[k], want[k])) {
            return false;
        }
    }
    return true;
}

// One expected outline: the outer four, the inner four (read only with a hole), and the hole.
struct expected_outline {
    const char *name = nullptr;
    gltfx_rect_world rect{};
    gltfx_rect_outline_style style{};
    pixel_affine affine{};
    corners outer{};
    corners inner{};
    bool has_hole = false;
};

// Checks every row, prints each failure by name and the counts, and returns the failures.
[[nodiscard]] std::size_t failures_of(std::span<const expected_outline> rows, const char *table) {
    std::size_t failed = 0;
    for (const expected_outline &row : rows) {
        const rect_outline got = rect_outline_of(row.rect, row.style, row.affine);
        const bool inner_ok = !row.has_hole || same_corners(got.inner, row.inner);
        if (got.has_hole != row.has_hole || !same_corners(got.outer, row.outer) || !inner_ok) {
            ++failed;
            std::println("stroke_rect_geometry_test: FALHOU {}", row.name);
        }
    }
    std::println("stroke_rect_geometry_test: {} casos={} falhas={}", table, rows.size(), failed);
    return failed;
}

} // namespace

// The three alignments on the identity (plan, S4, item 3), with a hole each.
GLINTFX_TEST(stroke_rect_outline_of_each_alignment_on_the_identity) {
    const std::array<expected_outline, 3> rows{{
        {"centrado", k_rect, style_of(4.0, gltfx_stroke_unit::world, gltfx_stroke_align::centered),
         identity(), corners{{{8.0, 8.0}, {52.0, 8.0}, {52.0, 32.0}, {8.0, 32.0}}},
         corners{{{12.0, 12.0}, {48.0, 12.0}, {48.0, 28.0}, {12.0, 28.0}}}, true},
        {"dentro", k_rect, style_of(4.0, gltfx_stroke_unit::world, gltfx_stroke_align::inside),
         identity(), corners{{{10.0, 10.0}, {50.0, 10.0}, {50.0, 30.0}, {10.0, 30.0}}},
         corners{{{14.0, 14.0}, {46.0, 14.0}, {46.0, 26.0}, {14.0, 26.0}}}, true},
        {"fora", k_rect, style_of(4.0, gltfx_stroke_unit::world, gltfx_stroke_align::outside),
         identity(), corners{{{6.0, 6.0}, {54.0, 6.0}, {54.0, 34.0}, {6.0, 34.0}}},
         corners{{{10.0, 10.0}, {50.0, 10.0}, {50.0, 30.0}, {10.0, 30.0}}}, true},
    }};
    GLINTFX_CHECK_EQ(failures_of(rows, "alinhamentos"), std::size_t{0});
}

// The hole, as a closed table of its borders (D-W8-271, D-S4-2, D-S4-3): a thickness that reaches
// the middle of the rectangle, or a rectangle with no width or no height, has no hole, and the
// outline is the outer polygon alone. No width and no height are each swept in the three
// alignments: centered, the bar around the segment; inside, a piece of zero area; outside, the
// grown rectangle, filled.
GLINTFX_TEST(stroke_rect_hole_closed_table_of_its_borders) {
    const auto world = gltfx_stroke_unit::world;
    const std::array<expected_outline, 12> rows{{
        {"dentro, largura 6 e espessura 4: sem buraco",
         {{10.0, 10.0}, {6.0, 20.0}},
         style_of(4.0, world, gltfx_stroke_align::inside),
         identity(),
         corners{{{10.0, 10.0}, {16.0, 10.0}, {16.0, 30.0}, {10.0, 30.0}}},
         corners{},
         false},
        {"dentro, largura 8 e espessura 4: o de dentro tem largura zero, sem buraco",
         {{10.0, 10.0}, {8.0, 20.0}},
         style_of(4.0, world, gltfx_stroke_align::inside),
         identity(),
         corners{{{10.0, 10.0}, {18.0, 10.0}, {18.0, 30.0}, {10.0, 30.0}}},
         corners{},
         false},
        {"dentro, largura 9 e espessura 4: buraco de largura 1",
         {{10.0, 10.0}, {9.0, 20.0}},
         style_of(4.0, world, gltfx_stroke_align::inside),
         identity(),
         corners{{{10.0, 10.0}, {19.0, 10.0}, {19.0, 30.0}, {10.0, 30.0}}},
         corners{{{14.0, 14.0}, {15.0, 14.0}, {15.0, 26.0}, {14.0, 26.0}}},
         true},
        {"centrado, altura igual a espessura: sem buraco",
         {{10.0, 10.0}, {40.0, 4.0}},
         style_of(4.0, world, gltfx_stroke_align::centered),
         identity(),
         corners{{{8.0, 8.0}, {52.0, 8.0}, {52.0, 16.0}, {8.0, 16.0}}},
         corners{},
         false},
        {"centrado, largura zero: a barra de 4 em volta do segmento",
         {{10.0, 10.0}, {0.0, 20.0}},
         style_of(4.0, world, gltfx_stroke_align::centered),
         identity(),
         corners{{{8.0, 8.0}, {12.0, 8.0}, {12.0, 32.0}, {8.0, 32.0}}},
         corners{},
         false},
        {"dentro, largura zero: peca de area zero, sem buraco",
         {{10.0, 10.0}, {0.0, 20.0}},
         style_of(4.0, world, gltfx_stroke_align::inside),
         identity(),
         corners{{{10.0, 10.0}, {10.0, 10.0}, {10.0, 30.0}, {10.0, 30.0}}},
         corners{},
         false},
        {"fora, largura zero: sem buraco",
         {{10.0, 10.0}, {0.0, 20.0}},
         style_of(4.0, world, gltfx_stroke_align::outside),
         identity(),
         corners{{{6.0, 6.0}, {14.0, 6.0}, {14.0, 34.0}, {6.0, 34.0}}},
         corners{},
         false},
        {"centrado, altura zero: a barra de 4 em volta do segmento",
         {{10.0, 10.0}, {40.0, 0.0}},
         style_of(4.0, world, gltfx_stroke_align::centered),
         identity(),
         corners{{{8.0, 8.0}, {52.0, 8.0}, {52.0, 12.0}, {8.0, 12.0}}},
         corners{},
         false},
        {"dentro, altura zero: peca de area zero, sem buraco",
         {{10.0, 10.0}, {40.0, 0.0}},
         style_of(4.0, world, gltfx_stroke_align::inside),
         identity(),
         corners{{{10.0, 10.0}, {50.0, 10.0}, {50.0, 10.0}, {10.0, 10.0}}},
         corners{},
         false},
        {"fora, altura zero: o retangulo crescido, cheio",
         {{10.0, 10.0}, {40.0, 0.0}},
         style_of(4.0, world, gltfx_stroke_align::outside),
         identity(),
         corners{{{6.0, 6.0}, {54.0, 6.0}, {54.0, 14.0}, {6.0, 14.0}}},
         corners{},
         false},
        {"fora, sem largura nem altura: o quadrado de lado 8",
         {{10.0, 10.0}, {0.0, 0.0}},
         style_of(4.0, world, gltfx_stroke_align::outside),
         identity(),
         corners{{{6.0, 6.0}, {14.0, 6.0}, {14.0, 14.0}, {6.0, 14.0}}},
         corners{},
         false},
        {"espessura zero: buraco igual ao retangulo, faixa de area zero", k_rect,
         style_of(0.0, world, gltfx_stroke_align::centered), identity(),
         corners{{{10.0, 10.0}, {50.0, 10.0}, {50.0, 30.0}, {10.0, 30.0}}},
         corners{{{10.0, 10.0}, {50.0, 10.0}, {50.0, 30.0}, {10.0, 30.0}}}, true},
    }};
    GLINTFX_CHECK_EQ(failures_of(rows, "buraco"), std::size_t{0});
}

// Under a zoom of 2: in pixel unit the thickness stays 4 on the screen; in world unit it becomes 8.
GLINTFX_TEST(stroke_rect_thickness_follows_its_unit_under_zoom) {
    const pixel_affine zoom = affine_of(0.0, 0.0, 0.0, 2.0, 2.0);
    const std::array<expected_outline, 2> rows{{
        {"pixel, zoom 2: espessura 4 na tela", k_rect,
         style_of(4.0, gltfx_stroke_unit::pixel, gltfx_stroke_align::centered), zoom,
         corners{{{18.0, 18.0}, {102.0, 18.0}, {102.0, 62.0}, {18.0, 62.0}}},
         corners{{{22.0, 22.0}, {98.0, 22.0}, {98.0, 58.0}, {22.0, 58.0}}}, true},
        {"mundo, zoom 2: espessura 8 na tela", k_rect,
         style_of(4.0, gltfx_stroke_unit::world, gltfx_stroke_align::centered), zoom,
         corners{{{16.0, 16.0}, {104.0, 16.0}, {104.0, 64.0}, {16.0, 64.0}}},
         corners{{{24.0, 24.0}, {96.0, 24.0}, {96.0, 56.0}, {24.0, 56.0}}}, true},
    }};
    GLINTFX_CHECK_EQ(failures_of(rows, "zoom"), std::size_t{0});
}

// Turned by 30 degrees, zoom 2, moved by (100, 50): the values of contas_s4.py.
GLINTFX_TEST(stroke_rect_outline_under_a_turn_in_each_unit) {
    const pixel_affine turned = affine_of(100.0, 50.0, 30.0, 2.0, 2.0);
    const std::array<expected_outline, 2> rows{{
        {"pixel, giro 30", k_rect,
         style_of(4.0, gltfx_stroke_unit::pixel, gltfx_stroke_align::centered), turned,
         corners{{{106.5884572681199, 74.58845726811988},
                  {179.33459118601274, 116.58845726811988},
                  {157.33459118601274, 154.6935750346352},
                  {84.5884572681199, 112.6935750346352}}},
         corners{{{108.05255888325766, 80.05255888325765},
                  {173.870489570875, 118.05255888325765},
                  {155.870489570875, 149.22947341949745},
                  {90.05255888325766, 111.22947341949744}}},
         true},
        {"mundo, giro 30", k_rect,
         style_of(4.0, gltfx_stroke_unit::world, gltfx_stroke_align::centered), turned,
         corners{{{105.85640646055101, 71.85640646055101},
                  {182.06664199358164, 115.85640646055101},
                  {158.06664199358164, 157.42562584220406},
                  {81.85640646055103, 113.42562584220408}}},
         corners{{{108.78460969082653, 82.78460969082653},
                  {171.1384387633061, 118.78460969082653},
                  {155.1384387633061, 146.49742261192856},
                  {92.78460969082653, 110.49742261192857}}},
         true},
    }};
    GLINTFX_CHECK_EQ(failures_of(rows, "giro"), std::size_t{0});
}

// A mirror and the flattened sides, in pixel unit: the outer polygon grows AWAY from the rectangle
// on the screen, and a side with no direction of its own takes the other's turned a quarter of a
// turn (D-S4-4). The values of contas_s4.py.
GLINTFX_TEST(stroke_rect_pixel_unit_under_a_mirror_and_flattened_sides) {
    const auto pixel = gltfx_stroke_unit::pixel;
    const auto centered = gltfx_stroke_align::centered;
    const std::array<expected_outline, 4> rows{{
        {"espelho (1, -1)", k_rect, style_of(4.0, pixel, centered),
         affine_of(0.0, 0.0, 0.0, 1.0, -1.0),
         corners{{{8.0, -8.0}, {52.0, -8.0}, {52.0, -32.0}, {8.0, -32.0}}},
         corners{{{12.0, -12.0}, {48.0, -12.0}, {48.0, -28.0}, {12.0, -28.0}}}, true},
        {"escala (0, 1): a largura achatada vira +x", k_rect, style_of(4.0, pixel, centered),
         affine_of(0.0, 0.0, 0.0, 0.0, 1.0),
         corners{{{-2.0, 8.0}, {2.0, 8.0}, {2.0, 32.0}, {-2.0, 32.0}}}, corners{}, false},
        {"escala (0, 0): +x e +y da tela", k_rect, style_of(4.0, pixel, centered),
         affine_of(0.0, 0.0, 0.0, 0.0, 0.0),
         corners{{{-2.0, -2.0}, {2.0, -2.0}, {2.0, 2.0}, {-2.0, 2.0}}}, corners{}, false},
        {"giro 90 e escala (0, 1): a largura achatada vira a altura girada", k_rect,
         style_of(4.0, pixel, centered), affine_of(0.0, 0.0, 90.0, 0.0, 1.0),
         corners{{{-8.0, -1.9999999999999993},
                  {-8.0, 2.0000000000000004},
                  {-32.0, 2.0000000000000018},
                  {-32.0, -1.999999999999998}}},
         corners{}, false},
    }};
    GLINTFX_CHECK_EQ(failures_of(rows, "espelho e achatado"), std::size_t{0});
}

// The points and the triangles of an outline: the outer four then the inner four, and the four
// trapezoids split from outer k (D-W8-271); without a hole, the outer quad.
GLINTFX_TEST(stroke_rect_points_and_triangles_with_and_without_a_hole) {
    const rect_outline holed = rect_outline_of(
        k_rect, style_of(4.0, gltfx_stroke_unit::world, gltfx_stroke_align::centered), identity());
    std::array<pixel_position_wide, k_rect_outline_max_points> points{};
    std::array<std::uint32_t, k_rect_outline_max_indices> indices{};
    GLINTFX_CHECK_EQ(write_rect_outline_points(holed, points), std::size_t{8});
    GLINTFX_CHECK(near(points[0], {8.0, 8.0}) && near(points[3], {8.0, 32.0}) &&
                  near(points[4], {12.0, 12.0}) && near(points[7], {12.0, 28.0}));
    GLINTFX_CHECK_EQ(write_rect_outline_indices(true, indices), std::size_t{24});
    const std::array<std::uint32_t, 24> band{0, 1, 5, 0, 5, 4, 1, 2, 6, 1, 6, 5,
                                             2, 3, 7, 2, 7, 6, 3, 0, 4, 3, 4, 7};
    GLINTFX_CHECK(indices == band);
    const rect_outline filled = rect_outline_of(
        gltfx_rect_world{{10.0, 10.0}, {6.0, 20.0}},
        style_of(4.0, gltfx_stroke_unit::world, gltfx_stroke_align::inside), identity());
    GLINTFX_CHECK_EQ(write_rect_outline_points(filled, points), std::size_t{4});
    GLINTFX_CHECK(near(points[2], {16.0, 30.0}));
    std::array<std::uint32_t, k_rect_outline_max_indices> quad{};
    GLINTFX_CHECK_EQ(write_rect_outline_indices(false, quad), std::size_t{6});
    GLINTFX_CHECK(quad[0] == 0 && quad[1] == 1 && quad[2] == 2 && quad[3] == 0 && quad[4] == 2 &&
                  quad[5] == 3);
}

// Storage smaller than the largest outline: nothing is written and the answer is zero.
GLINTFX_TEST(stroke_rect_too_small_storage_writes_nothing) {
    const rect_outline holed = rect_outline_of(
        k_rect, style_of(4.0, gltfx_stroke_unit::world, gltfx_stroke_align::centered), identity());
    std::array<pixel_position_wide, 7> points{};
    std::array<std::uint32_t, 23> indices{};
    GLINTFX_CHECK_EQ(write_rect_outline_points(holed, points), std::size_t{0});
    GLINTFX_CHECK(near(points[0], {0.0, 0.0}));
    GLINTFX_CHECK_EQ(write_rect_outline_indices(true, indices), std::size_t{0});
    GLINTFX_CHECK_EQ(indices[0], std::uint32_t{0});
}
