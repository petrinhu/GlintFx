// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <limits>
#include <print>
#include <string_view>

#include <glintfx/core/color.hpp>
#include <glintfx/core/quad.hpp>
#include <glintfx/core/rect.hpp>
#include <glintfx/draw2d/frame_2d_report.hpp>
#include <glintfx/draw2d/texture_region.hpp>

#include "draw2d/texture_region_check.hpp"
#include "draw2d/texture_region_uv.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// texture_region_check_test.cpp - R2D-TEXTURE, fatia T4b-A (D-W8-290, D-W8-457): the pure checks of
// one draw_texture() or draw_texture_quad() call (draw2d/texture_region_check.hpp) as CLOSED tables
// of named cells, every value swept for a NaN and for an infinity, and the edges of the range. The
// verdict of each cell is written from the rule of the header (one reason, in the order of the
// vocabulary), never read from the code's output. No GL, no window.

using glintfx::gltfx_draw_2d_refusal;
using glintfx::gltfx_quad_world;
using glintfx::gltfx_rect_texel;
using glintfx::gltfx_rect_world;
using glintfx::gltfx_rgba;
using glintfx::gltfx_texture_mirror;
using glintfx::gltfx_texture_paint;
using glintfx::gltfx_texture_region;
using glintfx::gltfx_textured_vertex_2d;
using glintfx::draw2d::refusal_of_region_range;
using glintfx::draw2d::refusal_of_texture_quad;
using glintfx::draw2d::refusal_of_texture_rect;
using glintfx::draw2d::texture_extent;

namespace {

constexpr double k_nan = std::numeric_limits<double>::quiet_NaN();
constexpr double k_inf = std::numeric_limits<double>::infinity();
constexpr float k_nan_f = std::numeric_limits<float>::quiet_NaN();
constexpr float k_inf_f = std::numeric_limits<float>::infinity();
constexpr gltfx_rgba k_white{1.0F, 1.0F, 1.0F, 1.0F};
constexpr gltfx_rect_world k_rect{{10.0, 10.0}, {40.0, 20.0}};
constexpr gltfx_quad_world k_quad{{0.0, 0.0}, {4.0, 0.0}, {3.0, 2.0}, {1.0, 2.0}};

[[nodiscard]] gltfx_texture_paint paint_of(gltfx_rect_texel source, gltfx_texture_mirror mirror) {
    return gltfx_texture_paint{gltfx_texture_region{source, mirror}, k_white};
}

[[nodiscard]] gltfx_texture_paint plain_paint() {
    return paint_of(gltfx_rect_texel{{0.0, 0.0}, {16.0, 16.0}}, gltfx_texture_mirror::none);
}

[[nodiscard]] gltfx_texture_mirror unknown_mirror() {
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) reason: the cast is the case
    return static_cast<gltfx_texture_mirror>(9);
}

struct rect_cell {
    const char *name = nullptr;
    gltfx_texture_paint paint{};
    gltfx_rect_world rect{};
    gltfx_draw_2d_refusal reason = gltfx_draw_2d_refusal::none;
    std::string_view token;
};

} // namespace

GLINTFX_TEST(texture_region_check_closed_table_of_the_rect_cells) {
    const std::array<rect_cell, 12> cells{{
        {"1 chamada valida", plain_paint(), k_rect, gltfx_draw_2d_refusal::none, "none"},
        {"2 x do retangulo NaN",
         plain_paint(),
         {{k_nan, 10.0}, {40.0, 20.0}},
         gltfx_draw_2d_refusal::not_a_number,
         "not_a_number"},
        {"3 x da regiao NaN", paint_of({{k_nan, 0.0}, {16.0, 16.0}}, gltfx_texture_mirror::none),
         k_rect, gltfx_draw_2d_refusal::not_a_number, "not_a_number"},
        {"4 alfa da tinta NaN",
         gltfx_texture_paint{{{{0.0, 0.0}, {16.0, 16.0}}, gltfx_texture_mirror::none},
                             {1.0F, 1.0F, 1.0F, k_nan_f}},
         k_rect, gltfx_draw_2d_refusal::not_a_number, "not_a_number"},
        {"5 largura da regiao infinita",
         paint_of({{0.0, 0.0}, {k_inf, 16.0}}, gltfx_texture_mirror::none), k_rect,
         gltfx_draw_2d_refusal::infinite, "infinite"},
        {"6 largura do retangulo -1",
         plain_paint(),
         {{10.0, 10.0}, {-1.0, 20.0}},
         gltfx_draw_2d_refusal::negative_size,
         "negative_size"},
        {"7 altura da regiao -1", paint_of({{0.0, 0.0}, {16.0, -1.0}}, gltfx_texture_mirror::none),
         k_rect, gltfx_draw_2d_refusal::negative_size, "negative_size"},
        {"8 espelho fora da enumeracao", paint_of({{0.0, 0.0}, {16.0, 16.0}}, unknown_mirror()),
         k_rect, gltfx_draw_2d_refusal::unknown_option, "unknown_option"},
        {"9 regiao de largura zero e aceita",
         paint_of({{0.0, 0.0}, {0.0, 16.0}}, gltfx_texture_mirror::none), k_rect,
         gltfx_draw_2d_refusal::none, "none"},
        {"10 NaN antes de infinito",
         paint_of({{k_inf, 0.0}, {16.0, 16.0}}, gltfx_texture_mirror::none),
         {{k_nan, 10.0}, {40.0, 20.0}},
         gltfx_draw_2d_refusal::not_a_number,
         "not_a_number"},
        {"11 infinito antes de negativo",
         paint_of({{0.0, 0.0}, {-1.0, 16.0}}, gltfx_texture_mirror::none),
         {{k_inf, 10.0}, {40.0, 20.0}},
         gltfx_draw_2d_refusal::infinite,
         "infinite"},
        {"12 negativo antes de opcao desconhecida",
         paint_of({{0.0, 0.0}, {16.0, 16.0}}, unknown_mirror()),
         {{10.0, 10.0}, {40.0, -1.0}},
         gltfx_draw_2d_refusal::negative_size,
         "negative_size"},
    }};
    std::size_t counted = 0;
    std::size_t failed = 0;
    for (const rect_cell &row : cells) {
        ++counted;
        const gltfx_draw_2d_refusal got = refusal_of_texture_rect(row.paint, row.rect);
        if (got != row.reason || glintfx::gltfx_draw_2d_refusal_name(got) != row.token) {
            ++failed;
            std::println("texture_region_check_test: FALHOU a celula {}", row.name);
        }
    }
    std::println("texture_region_check_test: celulas={} falhas={}", counted, failed);
    GLINTFX_CHECK_EQ(counted, std::size_t{12});
    GLINTFX_CHECK_EQ(failed, std::size_t{0});
}

GLINTFX_TEST(texture_region_check_the_quad_cells) {
    GLINTFX_CHECK(refusal_of_texture_quad(plain_paint(), k_quad) == gltfx_draw_2d_refusal::none);
    const gltfx_quad_world nan_corner{{0.0, 0.0}, {4.0, 0.0}, {3.0, k_nan}, {1.0, 2.0}};
    GLINTFX_CHECK(refusal_of_texture_quad(plain_paint(), nan_corner) ==
                  gltfx_draw_2d_refusal::not_a_number);
    const gltfx_quad_world inf_corner{{0.0, 0.0}, {4.0, 0.0}, {3.0, 2.0}, {k_inf, 2.0}};
    GLINTFX_CHECK(refusal_of_texture_quad(plain_paint(), inf_corner) ==
                  gltfx_draw_2d_refusal::infinite);
    GLINTFX_CHECK(
        refusal_of_texture_quad(paint_of({{0.0, 0.0}, {16.0, -1.0}}, gltfx_texture_mirror::none),
                                k_quad) == gltfx_draw_2d_refusal::negative_size);
    GLINTFX_CHECK(refusal_of_texture_quad(paint_of({{0.0, 0.0}, {16.0, 16.0}}, unknown_mirror()),
                                          k_quad) == gltfx_draw_2d_refusal::unknown_option);
    // A quadrilateral with no projective mapping is not a value refusal: the drawing call asks
    // projective_quad.hpp last (degenerate_quad).
    const gltfx_quad_world bow_tie{{0.0, 0.0}, {1.0, 0.0}, {0.0, 1.0}, {1.0, 1.0}};
    GLINTFX_CHECK(refusal_of_texture_quad(plain_paint(), bow_tie) == gltfx_draw_2d_refusal::none);
}

// Every one of the twelve values of draw_texture() (four of the rectangle, four of the region, four
// of the tint) and the sixteen of draw_texture_quad(), NaN then infinity.
GLINTFX_TEST(texture_region_check_every_value_is_swept) {
    std::size_t counted = 0;
    std::size_t failed = 0;
    for (std::size_t slot = 0; slot < 12U; ++slot) {
        for (const bool use_nan : {true, false}) {
            const double bad = use_nan ? k_nan : k_inf;
            const float bad_f = use_nan ? k_nan_f : k_inf_f;
            const gltfx_draw_2d_refusal want =
                use_nan ? gltfx_draw_2d_refusal::not_a_number : gltfx_draw_2d_refusal::infinite;
            const auto value = [&](std::size_t at, double good) { return slot == at ? bad : good; };
            const auto value_f = [&](std::size_t at) { return slot == at ? bad_f : 1.0F; };
            const gltfx_rect_world rect{{value(0, 10.0), value(1, 10.0)},
                                        {value(2, 40.0), value(3, 20.0)}};
            const gltfx_texture_paint paint{
                {{{value(4, 0.0), value(5, 0.0)}, {value(6, 16.0), value(7, 16.0)}},
                 gltfx_texture_mirror::none},
                {value_f(8), value_f(9), value_f(10), value_f(11)}};
            ++counted;
            if (refusal_of_texture_rect(paint, rect) != want) {
                ++failed;
                std::println("texture_region_check_test: FALHOU retangulo posicao {}", slot);
            }
            if (slot < 4U) {
                // the same slot as a corner coordinate of the quadrilateral
                const gltfx_quad_world quad{{value(0, 0.0), value(1, 0.0)},
                                            {value(2, 4.0), value(3, 0.0)},
                                            {3.0, 2.0},
                                            {1.0, 2.0}};
                ++counted;
                if (refusal_of_texture_quad(plain_paint(), quad) != want) {
                    ++failed;
                    std::println("texture_region_check_test: FALHOU quad posicao {}", slot);
                }
            }
        }
    }
    std::println("texture_region_check_test: varredura chamadas={} falhas={}", counted, failed);
    GLINTFX_CHECK_EQ(counted, std::size_t{32});
    GLINTFX_CHECK_EQ(failed, std::size_t{0});
}

GLINTFX_TEST(texture_region_check_the_range_edges) {
    const texture_extent texture{64U, 32U};
    const auto range_of = [&](const gltfx_rect_texel &source) {
        return refusal_of_region_range(source, texture);
    };
    GLINTFX_CHECK(range_of({{0.0, 0.0}, {64.0, 32.0}}) == gltfx_draw_2d_refusal::none);
    GLINTFX_CHECK(range_of({{64.0, 32.0}, {0.0, 0.0}}) == gltfx_draw_2d_refusal::none);
    GLINTFX_CHECK(range_of({{0.5, 0.5}, {63.5, 31.5}}) == gltfx_draw_2d_refusal::none);
    GLINTFX_CHECK(range_of({{-0.5, 0.0}, {1.0, 1.0}}) ==
                  gltfx_draw_2d_refusal::region_out_of_range);
    GLINTFX_CHECK(range_of({{0.0, -1e-9}, {1.0, 1.0}}) ==
                  gltfx_draw_2d_refusal::region_out_of_range);
    GLINTFX_CHECK(range_of({{0.5, 0.0}, {64.0, 1.0}}) ==
                  gltfx_draw_2d_refusal::region_out_of_range);
    GLINTFX_CHECK(range_of({{0.0, 31.0}, {1.0, 1.5}}) ==
                  gltfx_draw_2d_refusal::region_out_of_range);
    GLINTFX_CHECK(glintfx::gltfx_draw_2d_refusal_name(range_of({{65.0, 0.0}, {0.0, 0.0}})) ==
                  "region_out_of_range");
}

// The leader's decision of 14/10 (D-W8-464): the tint a caller does not choose is opaque white, so
// a texture drawn without one shows as it is, never invisible.
GLINTFX_TEST(texture_region_the_tint_you_do_not_choose_is_opaque_white) {
    const gltfx_texture_paint paint{};
    GLINTFX_CHECK(paint.tint.red == 1.0F && paint.tint.green == 1.0F);
    GLINTFX_CHECK(paint.tint.blue == 1.0F && paint.tint.alpha == 1.0F);
    const gltfx_textured_vertex_2d vertex{};
    GLINTFX_CHECK(vertex.tint.red == 1.0F && vertex.tint.green == 1.0F);
    GLINTFX_CHECK(vertex.tint.blue == 1.0F && vertex.tint.alpha == 1.0F);
}
