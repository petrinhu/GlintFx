// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <limits>
#include <print>

#include <glintfx/draw2d/renderer_2d_desc.hpp>

#include "draw2d/renderer_2d_desc_check.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// renderer_2d_desc_check_test.cpp - R2D-SHAPES, fatia S5-C1 (D-W8-566): the check open() makes of
// gltfx_renderer_2d_desc::curve_max_error_pixels (draw2d/renderer_2d_desc_check.hpp), as a CLOSED
// table of named cells. The verdict of each cell is written from the rule of the public header (a
// finite number greater than zero, no other interval: D-W8-284), never read from the code's output.

using glintfx::draw2d::is_valid_curve_max_error;

namespace {

struct cell {
    const char *name = nullptr;
    double pixels = 0.0;
    bool accepted = false;
};

constexpr double k_nan = std::numeric_limits<double>::quiet_NaN();
constexpr double k_inf = std::numeric_limits<double>::infinity();
constexpr double k_tiny = std::numeric_limits<double>::denorm_min();
constexpr double k_max = std::numeric_limits<double>::max();

constexpr std::array<cell, 13> k_cells{{
    {"nan", k_nan, false},
    {"mais_infinito", k_inf, false},
    {"menos_infinito", -k_inf, false},
    {"zero", 0.0, false},
    {"menos_zero", -0.0, false},
    {"menos_um_quarto", -0.25, false},
    {"menos_menor_subnormal", -k_tiny, false},
    {"menor_double", std::numeric_limits<double>::lowest(), false},
    {"menor_subnormal", k_tiny, true},
    {"um_trilionesimo", 1e-12, true},
    {"um_quarto", 0.25, true},
    {"oito", 8.0, true},
    {"maior_double", k_max, true},
}};

} // namespace

GLINTFX_TEST(renderer_2d_desc_check_closed_table_of_the_cells) {
    std::size_t checked = 0;
    std::size_t wrong = 0;
    for (const cell &c : k_cells) {
        ++checked;
        if (is_valid_curve_max_error(c.pixels) != c.accepted) {
            ++wrong;
            std::println("renderer_2d_desc_check_test: FALHOU {}", c.name);
        }
    }
    std::println("renderer_2d_desc_check_test: celulas={} erradas={}", checked, wrong);
    GLINTFX_CHECK_EQ(wrong, std::size_t{0});
    GLINTFX_CHECK(checked >= 13U); // L-40: a table that checked nothing is not a pass
}

GLINTFX_TEST(renderer_2d_desc_check_the_default_is_a_quarter_pixel_and_accepted) {
    const glintfx::gltfx_renderer_2d_desc desc{};
    GLINTFX_CHECK(desc.curve_max_error_pixels == 0.25);
    GLINTFX_CHECK(is_valid_curve_max_error(desc.curve_max_error_pixels));
}
