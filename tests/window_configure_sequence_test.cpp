// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstdint>
#include <optional>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/wayland/window_configure_sequence.hpp"

// window_configure_sequence_test.cpp - W-C' (docs/plano-w6a-janela.md
// fatia 4): five cases - a real size, the 0x0 "keep current size"
// rule, translating maximized+activated, translating fullscreen while
// silently ignoring codes this project does not track (resizing,
// tiled_left), and the single-use serial ack (D-W5-7).
//
// Registered UNGUARDED in tests/CMakeLists.txt (not inside an
// if(UNIX) block), on purpose: window_configure_sequence.{hpp,cpp}
// touches zero Wayland type (see that header's own comment - the
// state codes are plain std::int32_t, not the generated protocol
// enum), the exact same shape tests/CMakeLists.txt's own comment above
// global_catalog_test documents as a PREVIOUS bug it fixed ("Guarding
// them meant these three cases silently never ran on Windows"). Not
// gating this one repeats the fix instead of the mistake.

using glintfx::platform::window_configure_sequence;

GLINTFX_TEST(configure_with_nonzero_size_reports_has_size_true) {
    window_configure_sequence sequence;

    const auto result = sequence.apply_configure(640, 480, {}, 1);

    GLINTFX_CHECK(result.has_size);
    GLINTFX_CHECK(result.width == 640);
    GLINTFX_CHECK(result.height == 480);
    GLINTFX_CHECK(!result.maximized);
    GLINTFX_CHECK(!result.fullscreen);
    GLINTFX_CHECK(!result.activated);
}

GLINTFX_TEST(configure_with_zero_by_zero_size_reports_has_size_false) {
    // xdg_shell.xml's own rule: 0x0 from the compositor means "no size
    // preference, keep whatever you have" - never "resize to zero".
    window_configure_sequence sequence;

    const auto result = sequence.apply_configure(0, 0, {}, 1);

    GLINTFX_CHECK(!result.has_size);
}

// The MIXED case, missing until an adversarial review proved it by
// mutation (GODS_LAWS.md L-36/L-40, 06/09/2026): the two cases above
// (both zero, both nonzero) pass identically whether apply_configure()
// combines the two dimensions with && or ||, because neither dimension
// is ever zero while the other is nonzero - the review swapped &&
// for || in a copy of the tree, confirmed the mutation reached the
// rebuilt binary, ran this file's own suite in container, and every
// case here stayed green. Only a MIXED size - exactly one dimension
// zero - tells the two operators apart: && (the compositor's own
// contract, xdg_shell.xml - a size hint is a single WIDTHxHEIGHT pair,
// not two independent axes) says a lone zero still means "no size
// preference" for the pair as a whole; || would wrongly report
// has_size == true off the nonzero half alone. The two orders (width
// zero, height zero) are both written out - a fix that only checked
// one operand's position would still leave this exact bug on the
// other side.
GLINTFX_TEST(configure_with_zero_width_only_reports_has_size_false) {
    window_configure_sequence sequence;

    const auto result = sequence.apply_configure(0, 480, {}, 1);

    GLINTFX_CHECK(!result.has_size);
}

GLINTFX_TEST(configure_with_zero_height_only_reports_has_size_false) {
    window_configure_sequence sequence;

    const auto result = sequence.apply_configure(640, 0, {}, 1);

    GLINTFX_CHECK(!result.has_size);
}

GLINTFX_TEST(configure_translates_maximized_and_activated_states) {
    window_configure_sequence sequence;
    const std::array<std::int32_t, 2> states{
        glintfx::platform::k_xdg_toplevel_state_maximized,
        glintfx::platform::k_xdg_toplevel_state_activated,
    };

    const auto result = sequence.apply_configure(800, 600, states, 2);

    GLINTFX_CHECK(result.maximized);
    GLINTFX_CHECK(result.activated);
    GLINTFX_CHECK(!result.fullscreen);
}

GLINTFX_TEST(configure_translates_fullscreen_and_ignores_untracked_states) {
    window_configure_sequence sequence;
    // fullscreen (2), plus resizing (3) and tiled_left (5) - two real
    // xdg_toplevel_state codes this project's v1 deliberately does not
    // track (D-W5-3) - mixed in to prove an untracked code neither
    // crashes nor gets mistaken for one this project does track.
    const std::array<std::int32_t, 3> states{
        glintfx::platform::k_xdg_toplevel_state_fullscreen,
        3,
        5,
    };

    const auto result = sequence.apply_configure(1920, 1080, states, 3);

    GLINTFX_CHECK(result.fullscreen);
    GLINTFX_CHECK(!result.maximized);
    GLINTFX_CHECK(!result.activated);
}

GLINTFX_TEST(take_serial_to_ack_is_single_use) {
    window_configure_sequence sequence;
    static_cast<void>(sequence.apply_configure(640, 480, {}, 7));

    const std::optional<std::uint32_t> first = sequence.take_serial_to_ack();
    GLINTFX_CHECK(first.has_value());
    GLINTFX_CHECK(first.value() == 7);

    // A double ack of the same serial must be impossible BY
    // CONSTRUCTION (D-W5-7) - the second call, before any new
    // apply_configure(), has nothing left to hand back.
    const std::optional<std::uint32_t> second = sequence.take_serial_to_ack();
    GLINTFX_CHECK(!second.has_value());
}
