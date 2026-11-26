// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <print>
#include <span>

#include <glintfx/core/color.hpp>
#include <glintfx/core/rect.hpp>
#include <glintfx/draw2d/stroke.hpp>

#include "draw2d/quad_vertices.hpp"
#include "draw2d/stroke_rect_pixels.hpp"
#include "draw2d/triangle_batch.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// stroke_rect_pixels_test.cpp - R2D-SHAPES, fatia S4: one accepted stroke_rect() call as pixel
// triangles (draw2d/stroke_rect_pixels.hpp), with no window and no GL: the float positions, the
// colour of every vertex, the indices, the AREA the triangles cover, and the refusal of a point the
// vertex program cannot scale. The areas are written by hand before the code, as the outer area
// less the inner one (plano-fechamento-w8-r2.md, S4, item 3; the outside one corrected to
// 48 x 28 - 40 x 20 = 544, ordem S4): triangles that overlapped, or left a gap, would add up to
// another number. Helpers return prvalues, never a named local (INBOX MSVC-ASAN-PADDING-RETURN).

using glintfx::gltfx_rect_outline_style;
using glintfx::gltfx_rect_world;
using glintfx::gltfx_rgba;
using glintfx::gltfx_stroke;
using glintfx::gltfx_stroke_align;
using glintfx::gltfx_stroke_unit;
using glintfx::draw2d::colored_pixel_vertex;
using glintfx::draw2d::pixel_affine;
using glintfx::draw2d::stroke_rect_pixels;
using glintfx::draw2d::stroke_rect_pixels_outcome;

namespace {

constexpr gltfx_rect_world k_rect{{10.0, 10.0}, {40.0, 20.0}};
constexpr gltfx_rgba k_half_red{1.0F, 0.0F, 0.0F, 0.5F};

[[nodiscard]] gltfx_rect_outline_style style_of(double thickness, gltfx_stroke_align align) {
    return gltfx_rect_outline_style{gltfx_stroke{thickness, gltfx_stroke_unit::pixel}, align};
}

// The area the triangles cover, each counted once by its own size (the shoelace formula).
[[nodiscard]] double covered_area(std::span<const colored_pixel_vertex> vertices,
                                  std::span<const std::uint32_t> indices) {
    double total = 0.0;
    for (std::size_t t = 0; t + 2 < indices.size(); t += 3) {
        const auto &a = vertices[indices[t]].position;
        const auto &b = vertices[indices[t + 1]].position;
        const auto &c = vertices[indices[t + 2]].position;
        const double twice = (static_cast<double>(b.x - a.x) * static_cast<double>(c.y - a.y)) -
                             (static_cast<double>(c.x - a.x) * static_cast<double>(b.y - a.y));
        total += std::fabs(twice) / 2.0;
    }
    return total;
}

[[nodiscard]] bool at(const colored_pixel_vertex &vertex, float x, float y) {
    return vertex.position.x == x && vertex.position.y == y;
}

} // namespace

// The centered outline on the identity: 8 vertices, outer then inner, all in the call's colour,
// and the 24 indices of the four trapezoids.
GLINTFX_TEST(stroke_rect_pixels_centered_band_is_eight_vertices_in_its_colour) {
    stroke_rect_pixels built;
    GLINTFX_CHECK(built.build(k_rect, style_of(4.0, gltfx_stroke_align::centered), k_half_red,
                              pixel_affine{}) == stroke_rect_pixels_outcome::built);
    const std::span<const colored_pixel_vertex> vertices = built.vertices();
    GLINTFX_CHECK_EQ(vertices.size(), std::size_t{8});
    GLINTFX_CHECK(at(vertices[0], 8.0F, 8.0F) && at(vertices[1], 52.0F, 8.0F) &&
                  at(vertices[2], 52.0F, 32.0F) && at(vertices[3], 8.0F, 32.0F));
    GLINTFX_CHECK(at(vertices[4], 12.0F, 12.0F) && at(vertices[5], 48.0F, 12.0F) &&
                  at(vertices[6], 48.0F, 28.0F) && at(vertices[7], 12.0F, 28.0F));
    std::size_t coloured = 0;
    for (const colored_pixel_vertex &vertex : vertices) {
        if (vertex.color.red == 1.0F && vertex.color.green == 0.0F && vertex.color.blue == 0.0F &&
            vertex.color.alpha == 0.5F) {
            ++coloured;
        }
    }
    GLINTFX_CHECK_EQ(coloured, std::size_t{8});
    const std::array<std::uint32_t, 24> band{0, 1, 5, 0, 5, 4, 1, 2, 6, 1, 6, 5,
                                             2, 3, 7, 2, 7, 6, 3, 0, 4, 3, 4, 7};
    GLINTFX_CHECK_EQ(built.indices().size(), std::size_t{24});
    std::size_t same = 0;
    for (std::size_t i = 0; i < built.indices().size() && i < band.size(); ++i) {
        same += built.indices()[i] == band[i] ? 1U : 0U;
    }
    GLINTFX_CHECK_EQ(same, std::size_t{24});
}

namespace {

struct area_row {
    const char *name = nullptr;
    gltfx_rect_world rect{};
    gltfx_stroke_align align = gltfx_stroke_align::centered;
    std::size_t index_count = 0;
    double area = 0.0;
};

} // namespace

// The area each alignment covers, from the triangles built: 1056 - 576, 800 - 384, 1344 - 800, and
// the outline with no hole covers its whole outer rectangle, 6 x 20.
GLINTFX_TEST(stroke_rect_pixels_area_of_each_alignment) {
    const std::array<area_row, 4> rows{{
        {"centrado", k_rect, gltfx_stroke_align::centered, 24, 480.0},
        {"dentro", k_rect, gltfx_stroke_align::inside, 24, 416.0},
        {"fora", k_rect, gltfx_stroke_align::outside, 24, 544.0},
        {"dentro sem buraco", {{10.0, 10.0}, {6.0, 20.0}}, gltfx_stroke_align::inside, 6, 120.0},
    }};
    std::size_t failed = 0;
    for (const area_row &row : rows) {
        stroke_rect_pixels built;
        const stroke_rect_pixels_outcome outcome =
            built.build(row.rect, style_of(4.0, row.align), k_half_red, pixel_affine{});
        const double area = covered_area(built.vertices(), built.indices());
        if (outcome != stroke_rect_pixels_outcome::built ||
            built.indices().size() != row.index_count || area != row.area) {
            ++failed;
            std::println("stroke_rect_pixels_test: FALHOU {} (area {})", row.name, area);
        }
    }
    std::println("stroke_rect_pixels_test: areas casos={} falhas={}", rows.size(), failed);
    GLINTFX_CHECK_EQ(failed, std::size_t{0});
}

// A point beyond the pixel range the vertex program can scale (about 1.7e38) refuses the whole
// outline, and nothing of a previous build is left.
GLINTFX_TEST(stroke_rect_pixels_a_point_out_of_the_pixel_range_refuses_the_outline) {
    stroke_rect_pixels built;
    GLINTFX_CHECK(built.build(k_rect, style_of(4.0, gltfx_stroke_align::centered), k_half_red,
                              pixel_affine{}) == stroke_rect_pixels_outcome::built);
    const gltfx_rect_world far{{2.0e38, 10.0}, {40.0, 20.0}};
    GLINTFX_CHECK(
        built.build(far, style_of(4.0, gltfx_stroke_align::centered), k_half_red, pixel_affine{}) ==
        stroke_rect_pixels_outcome::pixel_position_out_of_range);
    GLINTFX_CHECK(built.vertices().empty());
    GLINTFX_CHECK(built.indices().empty());
}
