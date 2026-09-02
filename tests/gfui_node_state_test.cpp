// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstdint>
#include <cstdio>
#include <string_view>

#include <glintfx/gfui/node_view.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// gfui_node_state_test.cpp - GFSS-NODE-VIEW, fatia A (TODO.md,
// GODS_LAWS.md L-20/L-40): TDD red/green witness for
// glintfx::gfui::gltfx_node_state, gltfx_node_state_count,
// gltfx_node_state_has() and gltfx_node_state_name() - see node_view.
// hpp's own header comment for the design rationale, and node_state.
// cpp's own header comment for the RED stub this test proves against
// before the real table lands.

// GODS_LAWS.md L-40 (piso de varredura nao-vazia, contagem impressa
// mesmo quando passa): fixed at 5 by ESCOPO.md SS2 decision 6's own
// fact 5 ("os cinco estados"), but read from the MECHANICALLY derived
// gltfx_node_state_count below, never a hand-typed 5 - the same
// closed-enumeration discipline gfss/value_kind.cpp's own tests
// already apply.
GLINTFX_TEST(gltfx_node_state_count_is_five) {
    std::printf("gfui_node_state_test: gltfx_node_state_count = %zu\n",
                glintfx::gfui::gltfx_node_state_count);
    GLINTFX_CHECK_EQ(glintfx::gfui::gltfx_node_state_count, static_cast<std::size_t>(5));
}

namespace {

struct state_name_sample {
    glintfx::gfui::gltfx_node_state bit = glintfx::gfui::gltfx_node_state::none;
    std::string_view expected_name;
};

} // namespace

// Enumerates the whole closed space (GODS_LAWS.md L-17 achado: espaco
// pequeno e fechado se enumera inteiro, nunca amostra) - all five
// named bits, one row each, plus the row proving `none` itself
// answers "unknown" (it names no single bit).
GLINTFX_TEST(gltfx_node_state_name_answers_every_bit_and_unknown_for_none) {
    constexpr std::array<state_name_sample, 6> samples{{
        {glintfx::gfui::gltfx_node_state::hover, "hover"},
        {glintfx::gfui::gltfx_node_state::active, "active"},
        {glintfx::gfui::gltfx_node_state::focus, "focus"},
        {glintfx::gfui::gltfx_node_state::focus_visible, "focus_visible"},
        {glintfx::gfui::gltfx_node_state::checked, "checked"},
        {glintfx::gfui::gltfx_node_state::none, "unknown"},
    }};
    std::printf("gfui_node_state_test: %zu bit-name samples checked\n", samples.size());
    for (const state_name_sample &sample : samples) {
        GLINTFX_CHECK(glintfx::gfui::gltfx_node_state_name(sample.bit) == sample.expected_name);
    }
}

// A value combining two bits names neither single bit (docs/api-
// conventions.md R7: an identifier token, never a synthesized
// sentence) - falls through to "unknown", same as `none`.
GLINTFX_TEST(gltfx_node_state_name_of_a_combination_is_unknown) {
    const auto combo = static_cast<glintfx::gfui::gltfx_node_state>(
        static_cast<std::uint8_t>(glintfx::gfui::gltfx_node_state::hover) |
        static_cast<std::uint8_t>(glintfx::gfui::gltfx_node_state::focus));
    GLINTFX_CHECK(glintfx::gfui::gltfx_node_state_name(combo) == std::string_view{"unknown"});
}

GLINTFX_TEST(gltfx_node_state_has_reads_individual_and_combined_bits) {
    const auto hover_and_focus = static_cast<glintfx::gfui::gltfx_node_state>(
        static_cast<std::uint8_t>(glintfx::gfui::gltfx_node_state::hover) |
        static_cast<std::uint8_t>(glintfx::gfui::gltfx_node_state::focus));
    GLINTFX_CHECK(glintfx::gfui::gltfx_node_state_has(hover_and_focus,
                                                      glintfx::gfui::gltfx_node_state::hover));
    GLINTFX_CHECK(glintfx::gfui::gltfx_node_state_has(hover_and_focus,
                                                      glintfx::gfui::gltfx_node_state::focus));
    GLINTFX_CHECK(!glintfx::gfui::gltfx_node_state_has(hover_and_focus,
                                                       glintfx::gfui::gltfx_node_state::active));
    GLINTFX_CHECK(!glintfx::gfui::gltfx_node_state_has(glintfx::gfui::gltfx_node_state::none,
                                                       glintfx::gfui::gltfx_node_state::hover));
}
