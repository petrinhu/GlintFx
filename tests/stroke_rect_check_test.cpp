// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <limits>
#include <print>
#include <string_view>

#include <glintfx/core/color.hpp>
#include <glintfx/core/rect.hpp>
#include <glintfx/draw2d/frame_2d_report.hpp>
#include <glintfx/draw2d/stroke.hpp>

#include "draw2d/stroke_rect_check.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// stroke_rect_check_test.cpp - R2D-SHAPES, fatia S4: the pure check of one stroke_rect() call
// (draw2d/stroke_rect_check.hpp) as a CLOSED table of named cells, then every value of the call
// swept for a NaN and for an infinity. The verdict of each cell is written from the rule of the
// header (one reason, in the order of the vocabulary), never read from the code's output.
// Helpers return prvalues, never a named local (INBOX MSVC-ASAN-PADDING-RETURN).

using glintfx::gltfx_draw_2d_refusal;
using glintfx::gltfx_rect_outline_style;
using glintfx::gltfx_rect_world;
using glintfx::gltfx_rgba;
using glintfx::gltfx_stroke;
using glintfx::gltfx_stroke_align;
using glintfx::gltfx_stroke_unit;
using glintfx::draw2d::refusal_of_stroke_rect;

namespace {

constexpr double k_nan = std::numeric_limits<double>::quiet_NaN();
constexpr double k_inf = std::numeric_limits<double>::infinity();
constexpr float k_nan_f = std::numeric_limits<float>::quiet_NaN();
constexpr float k_inf_f = std::numeric_limits<float>::infinity();

constexpr gltfx_rgba k_white{1.0F, 1.0F, 1.0F, 1.0F};
constexpr gltfx_rect_world k_rect{{10.0, 10.0}, {40.0, 20.0}};

[[nodiscard]] gltfx_rect_outline_style style_of(double thickness) {
    return gltfx_rect_outline_style{gltfx_stroke{thickness, gltfx_stroke_unit::world},
                                    gltfx_stroke_align::centered};
}

[[nodiscard]] gltfx_rect_outline_style with_align(double thickness, gltfx_stroke_align align) {
    return gltfx_rect_outline_style{gltfx_stroke{thickness, gltfx_stroke_unit::world}, align};
}

[[nodiscard]] gltfx_rect_outline_style with_unit(double thickness, gltfx_stroke_unit unit) {
    return gltfx_rect_outline_style{gltfx_stroke{thickness, unit}, gltfx_stroke_align::centered};
}

[[nodiscard]] gltfx_stroke_align unknown_align() {
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) reason: the cast is the case
    return static_cast<gltfx_stroke_align>(9);
}

[[nodiscard]] gltfx_stroke_unit unknown_unit() {
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) reason: the cast is the case
    return static_cast<gltfx_stroke_unit>(9);
}

struct cell {
    const char *name = nullptr;
    gltfx_rect_world rect{};
    gltfx_rect_outline_style style{};
    gltfx_rgba color{};
    gltfx_draw_2d_refusal reason = gltfx_draw_2d_refusal::none;
    std::string_view token;
};

} // namespace

GLINTFX_TEST(stroke_rect_check_closed_table_of_the_cells) {
    const std::array<cell, 18> cells{{
        {"1 contorno valido", k_rect, style_of(4.0), k_white, gltfx_draw_2d_refusal::none, "none"},
        {"2 x do canto NaN",
         {{k_nan, 10.0}, {40.0, 20.0}},
         style_of(4.0),
         k_white,
         gltfx_draw_2d_refusal::not_a_number,
         "not_a_number"},
        {"3 altura NaN",
         {{10.0, 10.0}, {40.0, k_nan}},
         style_of(4.0),
         k_white,
         gltfx_draw_2d_refusal::not_a_number,
         "not_a_number"},
        {"4 espessura NaN", k_rect, style_of(k_nan), k_white, gltfx_draw_2d_refusal::not_a_number,
         "not_a_number"},
        {"5 alfa NaN",
         k_rect,
         style_of(4.0),
         {1.0F, 1.0F, 1.0F, k_nan_f},
         gltfx_draw_2d_refusal::not_a_number,
         "not_a_number"},
        {"6 largura infinita",
         {{10.0, 10.0}, {k_inf, 20.0}},
         style_of(4.0),
         k_white,
         gltfx_draw_2d_refusal::infinite,
         "infinite"},
        {"7 espessura infinita", k_rect, style_of(k_inf), k_white, gltfx_draw_2d_refusal::infinite,
         "infinite"},
        {"8 largura -1",
         {{10.0, 10.0}, {-1.0, 20.0}},
         style_of(4.0),
         k_white,
         gltfx_draw_2d_refusal::negative_size,
         "negative_size"},
        {"9 altura -1",
         {{10.0, 10.0}, {40.0, -1.0}},
         style_of(4.0),
         k_white,
         gltfx_draw_2d_refusal::negative_size,
         "negative_size"},
        {"10 espessura -1", k_rect, style_of(-1.0), k_white, gltfx_draw_2d_refusal::negative_size,
         "negative_size"},
        {"11 largura -0.0 e zero, nao negativa",
         {{10.0, 10.0}, {-0.0, 20.0}},
         style_of(4.0),
         k_white,
         gltfx_draw_2d_refusal::none,
         "none"},
        {"12 espessura zero e aceita (area zero, desenhada)", k_rect, style_of(0.0), k_white,
         gltfx_draw_2d_refusal::none, "none"},
        {"13 retangulo sem largura nem altura e aceito",
         {{10.0, 10.0}, {0.0, 0.0}},
         style_of(4.0),
         k_white,
         gltfx_draw_2d_refusal::none,
         "none"},
        {"14 alinhamento fora da enumeracao", k_rect, with_align(4.0, unknown_align()), k_white,
         gltfx_draw_2d_refusal::unknown_option, "unknown_option"},
        {"15 unidade fora da enumeracao", k_rect, with_unit(4.0, unknown_unit()), k_white,
         gltfx_draw_2d_refusal::unknown_option, "unknown_option"},
        {"16 NaN antes de infinito",
         {{k_nan, 10.0}, {40.0, 20.0}},
         style_of(k_inf),
         k_white,
         gltfx_draw_2d_refusal::not_a_number,
         "not_a_number"},
        {"17 infinito antes de negativo",
         {{10.0, 10.0}, {-1.0, 20.0}},
         style_of(k_inf),
         k_white,
         gltfx_draw_2d_refusal::infinite,
         "infinite"},
        {"18 negativo antes de opcao desconhecida",
         {{10.0, 10.0}, {40.0, -1.0}},
         with_align(4.0, unknown_align()),
         k_white,
         gltfx_draw_2d_refusal::negative_size,
         "negative_size"},
    }};
    std::size_t counted = 0;
    std::size_t failed = 0;
    for (const cell &row : cells) {
        ++counted;
        const gltfx_draw_2d_refusal got = refusal_of_stroke_rect(row.rect, row.style, row.color);
        if (got != row.reason || glintfx::gltfx_draw_2d_refusal_name(got) != row.token) {
            ++failed;
            std::println("stroke_rect_check_test: FALHOU a celula {}", row.name);
        }
    }
    std::println("stroke_rect_check_test: celulas={} falhas={}", counted, failed);
    GLINTFX_CHECK_EQ(counted, std::size_t{18});
    GLINTFX_CHECK_EQ(failed, std::size_t{0});
}

namespace {

// The call with the value at `slot` replaced by `bad`: slots 0 to 4 are the corner, the size and
// the thickness; 5 to 8 the colour.
struct sweep_call {
    gltfx_rect_world rect{};
    gltfx_rect_outline_style style{};
    gltfx_rgba color{};
};

[[nodiscard]] sweep_call call_with(std::size_t slot, double bad, float bad_f) {
    return sweep_call{
        gltfx_rect_world{{slot == 0 ? bad : 10.0, slot == 1 ? bad : 10.0},
                         {slot == 2 ? bad : 40.0, slot == 3 ? bad : 20.0}},
        style_of(slot == 4 ? bad : 4.0),
        gltfx_rgba{slot == 5 ? bad_f : 1.0F, slot == 6 ? bad_f : 1.0F, slot == 7 ? bad_f : 1.0F,
                   slot == 8 ? bad_f : 1.0F},
    };
}

} // namespace

// Every one of the nine values, NaN then infinity: 18 calls, each refused by its own reason.
GLINTFX_TEST(stroke_rect_check_every_value_is_swept) {
    std::size_t counted = 0;
    std::size_t failed = 0;
    for (std::size_t slot = 0; slot < 9; ++slot) {
        const sweep_call with_nan = call_with(slot, k_nan, k_nan_f);
        const sweep_call with_inf = call_with(slot, k_inf, k_inf_f);
        counted += 2;
        if (refusal_of_stroke_rect(with_nan.rect, with_nan.style, with_nan.color) !=
            gltfx_draw_2d_refusal::not_a_number) {
            ++failed;
            std::println("stroke_rect_check_test: FALHOU NaN no valor {}", slot);
        }
        if (refusal_of_stroke_rect(with_inf.rect, with_inf.style, with_inf.color) !=
            gltfx_draw_2d_refusal::infinite) {
            ++failed;
            std::println("stroke_rect_check_test: FALHOU infinito no valor {}", slot);
        }
    }
    std::println("stroke_rect_check_test: valores={} falhas={}", counted, failed);
    GLINTFX_CHECK_EQ(counted, std::size_t{18});
    GLINTFX_CHECK_EQ(failed, std::size_t{0});
}
