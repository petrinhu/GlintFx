// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <limits>
#include <print>
#include <string_view>

#include <glintfx/core/color.hpp>
#include <glintfx/core/segment.hpp>
#include <glintfx/draw2d/frame_2d_report.hpp>
#include <glintfx/draw2d/stroke.hpp>

#include "draw2d/stroke_line_check.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// stroke_line_check_test.cpp - R2D-SHAPES, fatia S3: the pure check of one stroke_line() call
// (draw2d/stroke_line_check.hpp) as a CLOSED table of named cells, then every value of the call
// swept for a NaN and for an infinity. The verdict of each cell is written from the rule of the
// header (one reason, in the order of the vocabulary), never read from the code's output.

using glintfx::gltfx_draw_2d_refusal;
using glintfx::gltfx_line_cap;
using glintfx::gltfx_line_style;
using glintfx::gltfx_rgba;
using glintfx::gltfx_segment_world;
using glintfx::gltfx_stroke_unit;
using glintfx::draw2d::refusal_of_stroke_line;

namespace {

constexpr double k_nan = std::numeric_limits<double>::quiet_NaN();
constexpr double k_inf = std::numeric_limits<double>::infinity();
constexpr float k_nan_f = std::numeric_limits<float>::quiet_NaN();

constexpr gltfx_rgba k_white{1.0F, 1.0F, 1.0F, 1.0F};
constexpr gltfx_segment_world k_line{{10.0, 10.0}, {50.0, 10.0}};

[[nodiscard]] gltfx_line_style style_of(double thickness) {
    gltfx_line_style style;
    style.stroke.thickness = thickness;
    return style;
}

[[nodiscard]] gltfx_line_cap unknown_cap() {
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) reason: the cast is the case
    return static_cast<gltfx_line_cap>(9);
}

[[nodiscard]] gltfx_stroke_unit unknown_unit() {
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) reason: the cast is the case
    return static_cast<gltfx_stroke_unit>(9);
}

struct cell {
    const char *name = nullptr;
    gltfx_segment_world segment{};
    gltfx_line_style style{};
    gltfx_rgba color{};
    gltfx_draw_2d_refusal reason = gltfx_draw_2d_refusal::none;
    std::string_view token;
};

[[nodiscard]] gltfx_line_style with_cap(gltfx_line_style style, gltfx_line_cap cap) {
    style.cap = cap;
    return style;
}

[[nodiscard]] gltfx_line_style with_unit(gltfx_line_style style, gltfx_stroke_unit unit) {
    style.stroke.unit = unit;
    return style;
}

} // namespace

GLINTFX_TEST(stroke_line_check_closed_table_of_the_cells) {
    const gltfx_segment_world point{{10.0, 10.0}, {10.0, 10.0}};
    const std::array<cell, 16> cells{{
        {"1 linha valida", k_line, style_of(4.0), k_white, gltfx_draw_2d_refusal::none, "none"},
        {"2 x do inicio NaN",
         {{k_nan, 10.0}, {50.0, 10.0}},
         style_of(4.0),
         k_white,
         gltfx_draw_2d_refusal::not_a_number,
         "not_a_number"},
        {"3 espessura NaN", k_line, style_of(k_nan), k_white, gltfx_draw_2d_refusal::not_a_number,
         "not_a_number"},
        {"4 alfa NaN",
         k_line,
         style_of(4.0),
         {1.0F, 1.0F, 1.0F, k_nan_f},
         gltfx_draw_2d_refusal::not_a_number,
         "not_a_number"},
        {"5 y do fim infinito",
         {{10.0, 10.0}, {50.0, k_inf}},
         style_of(4.0),
         k_white,
         gltfx_draw_2d_refusal::infinite,
         "infinite"},
        {"6 espessura infinita", k_line, style_of(k_inf), k_white, gltfx_draw_2d_refusal::infinite,
         "infinite"},
        {"7 espessura -1", k_line, style_of(-1.0), k_white, gltfx_draw_2d_refusal::negative_size,
         "negative_size"},
        {"8 espessura -0.0 e zero, nao negativa", k_line, style_of(-0.0), k_white,
         gltfx_draw_2d_refusal::none, "none"},
        {"9 espessura zero e aceita (area zero, desenhada)", k_line, style_of(0.0), k_white,
         gltfx_draw_2d_refusal::none, "none"},
        {"10 comprimento zero com ponta reta e aceito", point, style_of(4.0), k_white,
         gltfx_draw_2d_refusal::none, "none"},
        {"11 ponta fora da enumeracao", k_line, with_cap(style_of(4.0), unknown_cap()), k_white,
         gltfx_draw_2d_refusal::unknown_option, "unknown_option"},
        {"12 unidade fora da enumeracao", k_line, with_unit(style_of(4.0), unknown_unit()), k_white,
         gltfx_draw_2d_refusal::unknown_option, "unknown_option"},
        {"13 NaN antes de infinito",
         {{k_nan, 10.0}, {50.0, 10.0}},
         style_of(k_inf),
         k_white,
         gltfx_draw_2d_refusal::not_a_number,
         "not_a_number"},
        {"14 infinito antes de negativo", k_line, style_of(-k_inf), k_white,
         gltfx_draw_2d_refusal::infinite, "infinite"},
        {"15 negativo antes de opcao desconhecida", k_line, with_cap(style_of(-1.0), unknown_cap()),
         k_white, gltfx_draw_2d_refusal::negative_size, "negative_size"},
        {"16 NaN antes de opcao desconhecida", k_line, with_cap(style_of(k_nan), unknown_cap()),
         k_white, gltfx_draw_2d_refusal::not_a_number, "not_a_number"},
    }};
    std::size_t counted = 0;
    std::size_t failed = 0;
    for (const cell &row : cells) {
        ++counted;
        const gltfx_draw_2d_refusal got = refusal_of_stroke_line(row.segment, row.style, row.color);
        if (got != row.reason || glintfx::gltfx_draw_2d_refusal_name(got) != row.token) {
            ++failed;
            std::println("stroke_line_check_test: FALHOU a celula {}", row.name);
        }
    }
    std::println("stroke_line_check_test: celulas={} falhas={}", counted, failed);
    GLINTFX_CHECK_EQ(counted, std::size_t{16});
    GLINTFX_CHECK_EQ(failed, std::size_t{0});
}

namespace {

// The call with value `slot` changed to `value`: 0 start x, 1 start y, 2 end x, 3 end y,
// 4 thickness, 5 red, 6 green, 7 blue, 8 alpha.
[[nodiscard]] gltfx_draw_2d_refusal judge_with(int slot, double value) {
    gltfx_segment_world segment = k_line;
    gltfx_line_style style = style_of(4.0);
    gltfx_rgba color = k_white;
    const auto narrow = static_cast<float>(value);
    switch (slot) {
    case 0:
        segment.start.x = value;
        break;
    case 1:
        segment.start.y = value;
        break;
    case 2:
        segment.end.x = value;
        break;
    case 3:
        segment.end.y = value;
        break;
    case 4:
        style.stroke.thickness = value;
        break;
    case 5:
        color.red = narrow;
        break;
    case 6:
        color.green = narrow;
        break;
    case 7:
        color.blue = narrow;
        break;
    default:
        color.alpha = narrow;
        break;
    }
    return refusal_of_stroke_line(segment, style, color);
}

} // namespace

GLINTFX_TEST(stroke_line_check_every_value_of_the_call_is_looked_at) {
    std::size_t looked = 0;
    std::size_t failed = 0;
    for (int slot = 0; slot < 9; ++slot) {
        looked += 2;
        if (judge_with(slot, k_nan) != gltfx_draw_2d_refusal::not_a_number) {
            ++failed;
            std::println("stroke_line_check_test: FALHOU NaN no valor {}", slot);
        }
        if (judge_with(slot, k_inf) != gltfx_draw_2d_refusal::infinite) {
            ++failed;
            std::println("stroke_line_check_test: FALHOU infinito no valor {}", slot);
        }
    }
    std::println("stroke_line_check_test: valores={} falhas={}", looked, failed);
    GLINTFX_CHECK_EQ(looked, std::size_t{18});
    GLINTFX_CHECK_EQ(failed, std::size_t{0});
}

GLINTFX_TEST(stroke_line_check_every_known_cap_and_unit_is_accepted) {
    for (const gltfx_line_cap cap :
         {gltfx_line_cap::flat, gltfx_line_cap::square, gltfx_line_cap::rounded}) {
        for (const gltfx_stroke_unit unit : {gltfx_stroke_unit::world, gltfx_stroke_unit::pixel}) {
            GLINTFX_CHECK(refusal_of_stroke_line(k_line,
                                                 with_unit(with_cap(style_of(4.0), cap), unit),
                                                 k_white) == gltfx_draw_2d_refusal::none);
        }
    }
}
