// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cmath>
#include <cstdint>
#include <limits>
#include <print>
#include <string_view>

#include <glintfx/core/color.hpp>
#include <glintfx/core/rect.hpp>
#include <glintfx/core/vec2.hpp>

#include "draw2d/piece_refusal.hpp"
#include "draw2d/quad_vertices.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// piece_refusal_test.cpp - R2D-BATCH (D-B7-1, I4): which piece is refused, and why. These cells
// MOVED here, with the text of their assertions unchanged, from frame_report_tally_test.cpp, when
// the subject left frame_report_tally.hpp.
//
// THE LITERAL NUMBERS AND TOKENS asserted below are the ones frozen in the header of B0
// (gltfx_draw_2d_refusal: none = 0, not_a_number = 1, infinite = 2, negative_size = 3; tokens
// "none", "not_a_number", "infinite", "negative_size", "unknown"): never through the internal
// constants the code uses (D-P1-5's rule).

using glintfx::gltfx_draw_2d_refusal;
using glintfx::gltfx_draw_2d_refusal_name;
using glintfx::gltfx_rect_world;
using glintfx::gltfx_rgba;
using glintfx::gltfx_vec2_world;
using glintfx::draw2d::quad_corners_world;
using glintfx::draw2d::refusal_of_quad;
using glintfx::draw2d::refusal_of_rect;

namespace {
constexpr double k_nan = std::numeric_limits<double>::quiet_NaN();
constexpr double k_inf = std::numeric_limits<double>::infinity();
constexpr gltfx_rgba k_white{1.0F, 1.0F, 1.0F, 1.0F};

[[nodiscard]] quad_corners_world quad(double x0, double y0, double x1, double y1) {
    return quad_corners_world{gltfx_vec2_world{x0, y0}, gltfx_vec2_world{x1, y0},
                              gltfx_vec2_world{x1, y1}, gltfx_vec2_world{x0, y1}};
}

[[nodiscard]] int number_of(gltfx_draw_2d_refusal reason) { return static_cast<int>(reason); }
} // namespace

GLINTFX_TEST(piece_refusal_cells) {
    int analyzed = 0;

    // The literal numbers and tokens of the vocabulary (frozen in B0), append-only.
    GLINTFX_CHECK_EQ(number_of(gltfx_draw_2d_refusal::none), 0);
    GLINTFX_CHECK_EQ(number_of(gltfx_draw_2d_refusal::not_a_number), 1);
    GLINTFX_CHECK_EQ(number_of(gltfx_draw_2d_refusal::infinite), 2);
    GLINTFX_CHECK_EQ(number_of(gltfx_draw_2d_refusal::negative_size), 3);
    GLINTFX_CHECK(gltfx_draw_2d_refusal_name(gltfx_draw_2d_refusal::none) == "none");
    GLINTFX_CHECK(gltfx_draw_2d_refusal_name(gltfx_draw_2d_refusal::not_a_number) ==
                  "not_a_number");
    GLINTFX_CHECK(gltfx_draw_2d_refusal_name(gltfx_draw_2d_refusal::infinite) == "infinite");
    GLINTFX_CHECK(gltfx_draw_2d_refusal_name(gltfx_draw_2d_refusal::negative_size) ==
                  "negative_size");
    // A value this build does not know reads "unknown" (R4), never undefined behavior.
    // Simulates a value a NEWER glintfx produced that this build's vocabulary has never heard of
    // (R4).
    //
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) reason: the cast is the case
    GLINTFX_CHECK(gltfx_draw_2d_refusal_name(static_cast<gltfx_draw_2d_refusal>(200)) == "unknown");
    ++analyzed;

    // A finite quad is not refused - and a quad with ZERO area is drawn, it just covers no pixel.
    GLINTFX_CHECK(refusal_of_quad(quad(0, 0, 10, 10), k_white) == gltfx_draw_2d_refusal::none);
    GLINTFX_CHECK(refusal_of_quad(quad(5, 5, 5, 5), k_white) == gltfx_draw_2d_refusal::none);
    ++analyzed;
    // A NaN in ANY of the eight coordinates is refused, with the token not_a_number.
    for (int slot = 0; slot < 8; ++slot) {
        quad_corners_world q = quad(0, 0, 10, 10);
        double *const coordinates[8] = {&q[0].x, &q[0].y, &q[1].x, &q[1].y,
                                        &q[2].x, &q[2].y, &q[3].x, &q[3].y};
        *coordinates[slot] = k_nan;
        GLINTFX_CHECK(refusal_of_quad(q, k_white) == gltfx_draw_2d_refusal::not_a_number);
    }
    ++analyzed;
    // An infinity in a coordinate (either sign) is refused as `infinite`.
    {
        quad_corners_world plus = quad(0, 0, 10, 10);
        plus[2].x = k_inf;
        quad_corners_world minus = quad(0, 0, 10, 10);
        minus[3].y = -k_inf;
        GLINTFX_CHECK(refusal_of_quad(plus, k_white) == gltfx_draw_2d_refusal::infinite);
        GLINTFX_CHECK(refusal_of_quad(minus, k_white) == gltfx_draw_2d_refusal::infinite);
        ++analyzed;
    }
    // The color is a value too: a channel that is not a finite number refuses the piece.
    {
        GLINTFX_CHECK(refusal_of_quad(quad(0, 0, 1, 1),
                                      gltfx_rgba{1.0F, static_cast<float>(k_nan), 0.0F, 1.0F}) ==
                      gltfx_draw_2d_refusal::not_a_number);
        GLINTFX_CHECK(refusal_of_quad(quad(0, 0, 1, 1),
                                      gltfx_rgba{static_cast<float>(k_inf), 0.0F, 0.0F, 1.0F}) ==
                      gltfx_draw_2d_refusal::infinite);
        ++analyzed;
    }
    // EVERY color channel is checked, alpha included (a quad or a rectangle whose ONLY problem is
    // one channel): NaN in any of the four is not_a_number, an infinity in any is infinite.
    {
        for (int channel = 0; channel < 4; ++channel) {
            float nan_channels[4] = {1.0F, 1.0F, 1.0F, 1.0F};
            float inf_channels[4] = {1.0F, 1.0F, 1.0F, 1.0F};
            nan_channels[channel] = static_cast<float>(k_nan);
            inf_channels[channel] = static_cast<float>(k_inf);
            const gltfx_rgba with_nan{nan_channels[0], nan_channels[1], nan_channels[2],
                                      nan_channels[3]};
            const gltfx_rgba with_inf{inf_channels[0], inf_channels[1], inf_channels[2],
                                      inf_channels[3]};
            const gltfx_rect_world fine{{0.0, 0.0}, {5.0, 5.0}};
            GLINTFX_CHECK(refusal_of_quad(quad(0, 0, 1, 1), with_nan) ==
                          gltfx_draw_2d_refusal::not_a_number);
            GLINTFX_CHECK(refusal_of_quad(quad(0, 0, 1, 1), with_inf) ==
                          gltfx_draw_2d_refusal::infinite);
            GLINTFX_CHECK(refusal_of_rect(fine, with_nan) == gltfx_draw_2d_refusal::not_a_number);
            GLINTFX_CHECK(refusal_of_rect(fine, with_inf) == gltfx_draw_2d_refusal::infinite);
        }
        ++analyzed;
    }
    // A rectangle with a NEGATIVE width or height is refused as negative_size; zero size is drawn.
    {
        const gltfx_rect_world negative_width{{0.0, 0.0}, {-1.0, 5.0}};
        const gltfx_rect_world negative_height{{0.0, 0.0}, {5.0, -0.5}};
        const gltfx_rect_world zero{{0.0, 0.0}, {0.0, 0.0}};
        const gltfx_rect_world fine{{-3.0, -4.0}, {5.0, 5.0}};
        GLINTFX_CHECK(refusal_of_rect(negative_width, k_white) ==
                      gltfx_draw_2d_refusal::negative_size);
        GLINTFX_CHECK(refusal_of_rect(negative_height, k_white) ==
                      gltfx_draw_2d_refusal::negative_size);
        GLINTFX_CHECK(refusal_of_rect(zero, k_white) == gltfx_draw_2d_refusal::none);
        GLINTFX_CHECK(refusal_of_rect(fine, k_white) == gltfx_draw_2d_refusal::none);
        ++analyzed;
    }
    // A piece with several problems is refused for ONE reason, in the order of the vocabulary:
    // not_a_number first, then infinite, then negative_size.
    {
        const gltfx_rect_world nan_and_negative{{k_nan, 0.0}, {-1.0, 5.0}};
        const gltfx_rect_world inf_and_negative{{0.0, k_inf}, {-1.0, 5.0}};
        const gltfx_rect_world nan_and_inf{{k_nan, k_inf}, {1.0, 1.0}};
        GLINTFX_CHECK(refusal_of_rect(nan_and_negative, k_white) ==
                      gltfx_draw_2d_refusal::not_a_number);
        GLINTFX_CHECK(refusal_of_rect(inf_and_negative, k_white) ==
                      gltfx_draw_2d_refusal::infinite);
        GLINTFX_CHECK(refusal_of_rect(nan_and_inf, k_white) == gltfx_draw_2d_refusal::not_a_number);
        ++analyzed;
    }

    GLINTFX_CHECK_EQ(analyzed, 8);
    std::println("piece_refusal_test: {} celula(s) conferida(s) (recusa)", analyzed);
}
