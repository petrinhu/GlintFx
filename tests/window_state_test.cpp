// SPDX-License-Identifier: AGPL-3.0-or-later
#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/window/window_state.hpp"

// window_state_test.cpp - W-C' (docs/plano-w6a-janela.md fatia 4,
// D-W5-14 ampliada por decision 15/D-W6a-20): seven cases - three
// covering the state v1 freezes (default state, the three window_
// state_bit flags plus the one-way close_requested latch, and logical_
// size at the neutral scale/dpi both CI executors report today) and
// four proving the pixel/logical DERIVATION with SYNTHETIC values
// neither executor ever reports on its own (buffer_scale 2, dpi 144,
// and both reverting to their neutral value) - see window_state.hpp's
// own header comment on why "pixel_size just copies logical_size"
// would pass every OTHER test in this file and still be wrong (risk 4,
// docs/plano-w6a-janela.md sec. 5).

using glintfx::platform::window_state;
using glintfx::platform::window_state_bit;

GLINTFX_TEST(default_state_is_zero_size_no_flags_and_not_close_requested) {
    const window_state state;

    GLINTFX_CHECK(state.logical_size().width == 0);
    GLINTFX_CHECK(state.logical_size().height == 0);
    GLINTFX_CHECK(state.pixel_size().width == 0);
    GLINTFX_CHECK(state.pixel_size().height == 0);
    GLINTFX_CHECK(!state.state(window_state_bit::active));
    GLINTFX_CHECK(!state.state(window_state_bit::maximized));
    GLINTFX_CHECK(!state.state(window_state_bit::fullscreen));
    GLINTFX_CHECK(!state.close_requested());
}

GLINTFX_TEST(logical_size_equals_pixel_size_at_the_neutral_scale_and_dpi) {
    // Both CI executors report scale 1 / DPI 96 today (docs/plano-w6a-
    // janela.md sec. 0, F9, and sec. 5 risk 4's own warning) - this is
    // the case that would ALSO pass if pixel_size() were a bare copy
    // of logical_size(), which is exactly why it is not, by itself,
    // proof of a correct derivation (see the synthetic cases below).
    window_state state;
    state.apply_logical_size(800, 600);

    GLINTFX_CHECK(state.logical_size().width == 800);
    GLINTFX_CHECK(state.logical_size().height == 600);
    GLINTFX_CHECK(state.pixel_size().width == 800);
    GLINTFX_CHECK(state.pixel_size().height == 600);
}

GLINTFX_TEST(state_bits_and_close_requested_are_independent) {
    window_state state;
    state.set_state(window_state_bit::active, true);
    state.set_state(window_state_bit::maximized, true);
    state.request_close();

    GLINTFX_CHECK(state.state(window_state_bit::active));
    GLINTFX_CHECK(state.state(window_state_bit::maximized));
    GLINTFX_CHECK(!state.state(window_state_bit::fullscreen));
    GLINTFX_CHECK(state.close_requested());

    // Clearing one flag never touches the others or the close latch.
    state.set_state(window_state_bit::active, false);
    GLINTFX_CHECK(!state.state(window_state_bit::active));
    GLINTFX_CHECK(state.state(window_state_bit::maximized));
    GLINTFX_CHECK(state.close_requested());
}

GLINTFX_TEST(buffer_scale_two_doubles_the_pixel_size) {
    window_state state;
    state.apply_logical_size(300, 200);
    state.apply_buffer_scale(2);

    GLINTFX_CHECK(state.logical_size().width == 300);
    GLINTFX_CHECK(state.logical_size().height == 200);
    GLINTFX_CHECK(state.pixel_size().width == 600);
    GLINTFX_CHECK(state.pixel_size().height == 400);
}

GLINTFX_TEST(dpi_144_makes_logical_size_two_thirds_of_pixel_size) {
    // 144 / 96 = 1.5, so pixel_size is 1.5x logical_size and
    // logical_size is exactly 2/3 of pixel_size - the fact D-W6a-20's
    // own decision row names verbatim.
    window_state state;
    state.apply_logical_size(300, 200);
    state.apply_dpi(144);

    GLINTFX_CHECK(state.logical_size().width == 300);
    GLINTFX_CHECK(state.logical_size().height == 200);
    GLINTFX_CHECK(state.pixel_size().width == 450);
    GLINTFX_CHECK(state.pixel_size().height == 300);
}

GLINTFX_TEST(buffer_scale_back_to_one_restores_pixel_size_to_logical_size) {
    window_state state;
    state.apply_logical_size(300, 200);
    state.apply_buffer_scale(2);
    state.apply_buffer_scale(1);

    GLINTFX_CHECK(state.pixel_size().width == 300);
    GLINTFX_CHECK(state.pixel_size().height == 200);
}

GLINTFX_TEST(dpi_back_to_96_restores_pixel_size_to_logical_size) {
    window_state state;
    state.apply_logical_size(300, 200);
    state.apply_dpi(144);
    state.apply_dpi(96);

    GLINTFX_CHECK(state.pixel_size().width == 300);
    GLINTFX_CHECK(state.pixel_size().height == 200);
}
