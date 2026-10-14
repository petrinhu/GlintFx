// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <limits>
#include <print>
#include <string_view>

#include <glintfx/core/angle.hpp>
#include <glintfx/core/color.hpp>
#include <glintfx/core/ellipse.hpp>
#include <glintfx/draw2d/frame_2d_report.hpp>
#include <glintfx/draw2d/stroke.hpp>

#include "draw2d/ellipse_check.hpp"
#include "draw2d/stroke_unit.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// ellipse_check_test.cpp - R2D-SHAPES, fatia S5-B (D-W8-410, D-W8-414): the pure checks of one
// fill_ellipse() and one stroke_ellipse() call (draw2d/ellipse_check.hpp) as CLOSED tables of named
// cells, then every value of each call swept for a NaN and for an infinity, and the unit check
// extracted to draw2d/stroke_unit.hpp. The verdict of each cell is written from the rule of the
// header (one reason, in the order of the vocabulary), never read from the code's output. Helpers
// return prvalues, never a named local (INBOX MSVC-ASAN-PADDING-RETURN).

using glintfx::gltfx_angle;
using glintfx::gltfx_draw_2d_refusal;
using glintfx::gltfx_ellipse_world;
using glintfx::gltfx_rgba;
using glintfx::gltfx_stroke;
using glintfx::gltfx_stroke_unit;
using glintfx::draw2d::is_known_stroke_unit;
using glintfx::draw2d::refusal_of_fill_ellipse;
using glintfx::draw2d::refusal_of_stroke_ellipse;

namespace {

constexpr double k_nan = std::numeric_limits<double>::quiet_NaN();
constexpr double k_inf = std::numeric_limits<double>::infinity();
constexpr float k_nan_f = std::numeric_limits<float>::quiet_NaN();

constexpr gltfx_rgba k_white{1.0F, 1.0F, 1.0F, 1.0F};

[[nodiscard]] gltfx_ellipse_world ellipse_of(double radius_x, double radius_y) {
    return gltfx_ellipse_world{{30.0, 20.0}, radius_x, radius_y, gltfx_angle{0.5}};
}

[[nodiscard]] gltfx_stroke stroke_of(double thickness) {
    return gltfx_stroke{thickness, gltfx_stroke_unit::world};
}

[[nodiscard]] gltfx_stroke_unit unknown_unit() {
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) reason: the cast is the case
    return static_cast<gltfx_stroke_unit>(9);
}

struct cell {
    const char *name = nullptr;
    bool outlined = false;
    gltfx_ellipse_world ellipse{};
    gltfx_stroke stroke{};
    gltfx_rgba color{};
    gltfx_draw_2d_refusal reason = gltfx_draw_2d_refusal::none;
    std::string_view token;
};

[[nodiscard]] gltfx_draw_2d_refusal refusal_of(const cell &row) {
    return row.outlined ? refusal_of_stroke_ellipse(row.ellipse, row.stroke, row.color)
                        : refusal_of_fill_ellipse(row.ellipse, row.color);
}

} // namespace

GLINTFX_TEST(ellipse_check_closed_table_of_the_cells) {
    const std::array<cell, 20> cells{{
        {"1 elipse cheia valida", false, ellipse_of(20.0, 10.0), stroke_of(1.0), k_white,
         gltfx_draw_2d_refusal::none, "none"},
        {"2 contorno valido", true, ellipse_of(20.0, 10.0), stroke_of(4.0), k_white,
         gltfx_draw_2d_refusal::none, "none"},
        {"3 cheia com raio zero e aceita (area zero, desenhada)", false, ellipse_of(0.0, 0.0),
         stroke_of(1.0), k_white, gltfx_draw_2d_refusal::none, "none"},
        {"4 contorno com raio zero e aceito (o disco da espessura)", true, ellipse_of(0.0, 0.0),
         stroke_of(4.0), k_white, gltfx_draw_2d_refusal::none, "none"},
        {"5 contorno com um raio zero e aceito (a barra de pontas redondas)", true,
         ellipse_of(20.0, 0.0), stroke_of(4.0), k_white, gltfx_draw_2d_refusal::none, "none"},
        {"6 espessura zero e aceita (area zero, desenhada)", true, ellipse_of(20.0, 10.0),
         stroke_of(0.0), k_white, gltfx_draw_2d_refusal::none, "none"},
        {"7 raio -0.0 e zero, nao negativo", false, ellipse_of(-0.0, 10.0), stroke_of(1.0), k_white,
         gltfx_draw_2d_refusal::none, "none"},
        {"8 cheia: raio x -1", false, ellipse_of(-1.0, 10.0), stroke_of(1.0), k_white,
         gltfx_draw_2d_refusal::negative_size, "negative_size"},
        {"9 cheia: raio y -1", false, ellipse_of(20.0, -1.0), stroke_of(1.0), k_white,
         gltfx_draw_2d_refusal::negative_size, "negative_size"},
        {"10 contorno: espessura -1", true, ellipse_of(20.0, 10.0), stroke_of(-1.0), k_white,
         gltfx_draw_2d_refusal::negative_size, "negative_size"},
        {"11 contorno: raio y -1", true, ellipse_of(20.0, -1.0), stroke_of(4.0), k_white,
         gltfx_draw_2d_refusal::negative_size, "negative_size"},
        {"12 cheia: a espessura nao e lida (NaN aceito)", false, ellipse_of(20.0, 10.0),
         stroke_of(k_nan), k_white, gltfx_draw_2d_refusal::none, "none"},
        {"13 contorno: unidade fora da enumeracao", true, ellipse_of(20.0, 10.0),
         gltfx_stroke{4.0, unknown_unit()}, k_white, gltfx_draw_2d_refusal::unknown_option,
         "unknown_option"},
        {"14 cheia: a unidade nao e lida", false, ellipse_of(20.0, 10.0),
         gltfx_stroke{4.0, unknown_unit()}, k_white, gltfx_draw_2d_refusal::none, "none"},
        {"15 rotacao NaN", false, gltfx_ellipse_world{{30.0, 20.0}, 20.0, 10.0, gltfx_angle{k_nan}},
         stroke_of(1.0), k_white, gltfx_draw_2d_refusal::not_a_number, "not_a_number"},
        {"16 rotacao infinita", true,
         gltfx_ellipse_world{{30.0, 20.0}, 20.0, 10.0, gltfx_angle{k_inf}}, stroke_of(4.0), k_white,
         gltfx_draw_2d_refusal::infinite, "infinite"},
        {"17 NaN antes de infinito",
         true,
         gltfx_ellipse_world{{k_inf, 20.0}, 20.0, 10.0, gltfx_angle{0.5}},
         stroke_of(4.0),
         {1.0F, 1.0F, k_nan_f, 1.0F},
         gltfx_draw_2d_refusal::not_a_number,
         "not_a_number"},
        {"18 infinito antes de negativo", true, ellipse_of(-1.0, 10.0), stroke_of(k_inf), k_white,
         gltfx_draw_2d_refusal::infinite, "infinite"},
        {"19 negativo antes de opcao desconhecida", true, ellipse_of(20.0, 10.0),
         gltfx_stroke{-1.0, unknown_unit()}, k_white, gltfx_draw_2d_refusal::negative_size,
         "negative_size"},
        {"20 rotacao enorme e finita e aceita", false,
         gltfx_ellipse_world{{30.0, 20.0}, 20.0, 10.0, gltfx_angle{1.0e300}}, stroke_of(1.0),
         k_white, gltfx_draw_2d_refusal::none, "none"},
    }};
    std::size_t counted = 0;
    std::size_t failed = 0;
    for (const cell &row : cells) {
        ++counted;
        const gltfx_draw_2d_refusal got = refusal_of(row);
        if (got != row.reason || glintfx::gltfx_draw_2d_refusal_name(got) != row.token) {
            ++failed;
            std::println("ellipse_check_test: FALHOU a celula {}", row.name);
        }
    }
    std::println("ellipse_check_test: celulas={} falhas={}", counted, failed);
    GLINTFX_CHECK_EQ(counted, std::size_t{20});
    GLINTFX_CHECK_EQ(failed, std::size_t{0});
}

namespace {

// The call with the value at `slot` replaced by `bad`: slots 0 to 4 are the centre, the two radii
// and the rotation; 5 the thickness; 6 to 9 the colour.
struct sweep_call {
    gltfx_ellipse_world ellipse{};
    gltfx_stroke stroke{};
    gltfx_rgba color{};
};

[[nodiscard]] sweep_call call_with(std::size_t slot, double bad, float bad_f) {
    return sweep_call{
        gltfx_ellipse_world{{slot == 0 ? bad : 30.0, slot == 1 ? bad : 20.0},
                            slot == 2 ? bad : 20.0,
                            slot == 3 ? bad : 10.0,
                            gltfx_angle{slot == 4 ? bad : 0.5}},
        stroke_of(slot == 5 ? bad : 4.0),
        gltfx_rgba{slot == 6 ? bad_f : 1.0F, slot == 7 ? bad_f : 1.0F, slot == 8 ? bad_f : 1.0F,
                   slot == 9 ? bad_f : 1.0F},
    };
}

} // namespace

// Every one of the ten values of an outline, NaN then infinity: 20 calls, each refused by its own
// reason; and the nine of a fill (the thickness is not part of it): 18 more.
GLINTFX_TEST(ellipse_check_every_value_is_swept) {
    std::size_t counted = 0;
    std::size_t failed = 0;
    for (std::size_t slot = 0; slot < 10; ++slot) {
        const sweep_call with_nan = call_with(slot, k_nan, k_nan_f);
        const sweep_call with_inf = call_with(slot, k_inf, std::numeric_limits<float>::infinity());
        counted += 2U;
        if (refusal_of_stroke_ellipse(with_nan.ellipse, with_nan.stroke, with_nan.color) !=
            gltfx_draw_2d_refusal::not_a_number) {
            ++failed;
            std::println("ellipse_check_test: FALHOU contorno NaN na posicao {}", slot);
        }
        if (refusal_of_stroke_ellipse(with_inf.ellipse, with_inf.stroke, with_inf.color) !=
            gltfx_draw_2d_refusal::infinite) {
            ++failed;
            std::println("ellipse_check_test: FALHOU contorno infinito na posicao {}", slot);
        }
        if (slot == 5U) {
            continue;
        }
        counted += 2U;
        if (refusal_of_fill_ellipse(with_nan.ellipse, with_nan.color) !=
            gltfx_draw_2d_refusal::not_a_number) {
            ++failed;
            std::println("ellipse_check_test: FALHOU cheia NaN na posicao {}", slot);
        }
        if (refusal_of_fill_ellipse(with_inf.ellipse, with_inf.color) !=
            gltfx_draw_2d_refusal::infinite) {
            ++failed;
            std::println("ellipse_check_test: FALHOU cheia infinita na posicao {}", slot);
        }
    }
    std::println("ellipse_check_test: varredura chamadas={} falhas={}", counted, failed);
    GLINTFX_CHECK_EQ(counted, std::size_t{38});
    GLINTFX_CHECK_EQ(failed, std::size_t{0});
}

// The unit check moved out of the line and rectangle checks (D-W8-410): the two values are known,
// and a value outside the enumeration is not.
GLINTFX_TEST(ellipse_check_the_stroke_unit_is_known_only_inside_its_enumeration) {
    GLINTFX_CHECK(is_known_stroke_unit(gltfx_stroke_unit::world));
    GLINTFX_CHECK(is_known_stroke_unit(gltfx_stroke_unit::pixel));
    GLINTFX_CHECK(!is_known_stroke_unit(unknown_unit()));
}
