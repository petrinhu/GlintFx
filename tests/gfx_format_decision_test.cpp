// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstddef>
#include <cstdint>
#include <print>

#include "platform/gl/gfx_format_decision.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// gfx_format_decision_test.cpp - D-SRGB-2 (D-SRGB2-1, D-SRGB2-3, slice S2): the one neutral
// decision both context adapters (EGL and WGL) call, proved over its WHOLE input space: the 64
// combinations of the six facts behind a refusal, and the 8 of srgb_option_support(). It runs on
// every system.
//
// The expected values below are written from the ORDER of D-SRGB2-3, never read back from the code:
//   (1) sRGB asked and not advertised                       -> srgb_framebuffer
//   (2) no format with everything: sRGB asked and a format without it exists -> srgb_framebuffer;
//       else MSAA asked -> msaa_samples; else no_format
//   (3) sRGB asked and not confirmed                        -> srgb_framebuffer
//   (4) otherwise                                           -> none
// An impossible combination follows the same literal order: the first rule that matches decides,
// and an input that rule does not read is ignored.

using glintfx::gltfx_gfx_option_support;
using glintfx::platform::decide_gfx_format;
using glintfx::platform::gfx_format_facts;
using glintfx::platform::gfx_format_refusal;
using glintfx::platform::srgb_option_support;

namespace {

constexpr gfx_format_refusal none = gfx_format_refusal::none;
constexpr gfx_format_refusal srgb = gfx_format_refusal::srgb_framebuffer;
constexpr gfx_format_refusal msaa = gfx_format_refusal::msaa_samples;
constexpr gfx_format_refusal no_format = gfx_format_refusal::no_format;

// One row per combination, in binary counting order of the six facts: msaa_requested is the most
// significant bit, srgb_confirmed the least.
struct decision_row {
    bool msaa_requested;
    bool srgb_requested;
    bool srgb_advertised;
    bool format_found;
    bool format_found_without_srgb;
    bool srgb_confirmed;
    gfx_format_refusal expected;
};

constexpr decision_row k_decision_table[] = {
    {false, false, false, false, false, false, no_format},
    {false, false, false, false, false, true, no_format},
    {false, false, false, false, true, false, no_format},
    {false, false, false, false, true, true, no_format},
    {false, false, false, true, false, false, none},
    {false, false, false, true, false, true, none},
    {false, false, false, true, true, false, none},
    {false, false, false, true, true, true, none},
    {false, false, true, false, false, false, no_format},
    {false, false, true, false, false, true, no_format},
    {false, false, true, false, true, false, no_format},
    {false, false, true, false, true, true, no_format},
    {false, false, true, true, false, false, none},
    {false, false, true, true, false, true, none},
    {false, false, true, true, true, false, none},
    {false, false, true, true, true, true, none},
    {false, true, false, false, false, false, srgb},
    {false, true, false, false, false, true, srgb},
    {false, true, false, false, true, false, srgb},
    {false, true, false, false, true, true, srgb},
    {false, true, false, true, false, false, srgb},
    {false, true, false, true, false, true, srgb},
    {false, true, false, true, true, false, srgb},
    {false, true, false, true, true, true, srgb},
    {false, true, true, false, false, false, no_format},
    {false, true, true, false, false, true, no_format},
    {false, true, true, false, true, false, srgb},
    {false, true, true, false, true, true, srgb},
    {false, true, true, true, false, false, srgb},
    {false, true, true, true, false, true, none},
    {false, true, true, true, true, false, srgb},
    {false, true, true, true, true, true, none},
    {true, false, false, false, false, false, msaa},
    {true, false, false, false, false, true, msaa},
    {true, false, false, false, true, false, msaa},
    {true, false, false, false, true, true, msaa},
    {true, false, false, true, false, false, none},
    {true, false, false, true, false, true, none},
    {true, false, false, true, true, false, none},
    {true, false, false, true, true, true, none},
    {true, false, true, false, false, false, msaa},
    {true, false, true, false, false, true, msaa},
    {true, false, true, false, true, false, msaa},
    {true, false, true, false, true, true, msaa},
    {true, false, true, true, false, false, none},
    {true, false, true, true, false, true, none},
    {true, false, true, true, true, false, none},
    {true, false, true, true, true, true, none},
    {true, true, false, false, false, false, srgb},
    {true, true, false, false, false, true, srgb},
    {true, true, false, false, true, false, srgb},
    {true, true, false, false, true, true, srgb},
    {true, true, false, true, false, false, srgb},
    {true, true, false, true, false, true, srgb},
    {true, true, false, true, true, false, srgb},
    {true, true, false, true, true, true, srgb},
    {true, true, true, false, false, false, msaa},
    {true, true, true, false, false, true, msaa},
    {true, true, true, false, true, false, srgb},
    {true, true, true, false, true, true, srgb},
    {true, true, true, true, false, false, srgb},
    {true, true, true, true, false, true, none},
    {true, true, true, true, true, false, srgb},
    {true, true, true, true, true, true, none},
};

constexpr std::size_t k_decision_rows = sizeof(k_decision_table) / sizeof(k_decision_table[0]);

std::size_t bit_pattern_of(const decision_row &row) {
    return (row.msaa_requested ? 32U : 0U) | (row.srgb_requested ? 16U : 0U) |
           (row.srgb_advertised ? 8U : 0U) | (row.format_found ? 4U : 0U) |
           (row.format_found_without_srgb ? 2U : 0U) | (row.srgb_confirmed ? 1U : 0U);
}

gfx_format_facts facts_of(const decision_row &row) {
    gfx_format_facts facts;
    facts.msaa_requested = row.msaa_requested;
    facts.srgb_requested = row.srgb_requested;
    facts.srgb_advertised = row.srgb_advertised;
    facts.format_found = row.format_found;
    facts.format_found_without_srgb = row.format_found_without_srgb;
    facts.srgb_confirmed = row.srgb_confirmed;
    return facts;
}

} // namespace

// The table is itself the enumeration: row i must be the i-th combination, so no combination is
// missing and none is written twice (L-17: a small closed space is enumerated whole).
GLINTFX_TEST(gfx_format_decision_the_table_enumerates_the_whole_space_once) {
    GLINTFX_CHECK_EQ(k_decision_rows, std::size_t{64});
    for (std::size_t i = 0; i < k_decision_rows; ++i) {
        GLINTFX_CHECK_EQ(bit_pattern_of(k_decision_table[i]), i);
    }
}

// L-40: the sweep counts what it checked and refuses to pass on fewer than 64.
GLINTFX_TEST(gfx_format_decision_all_64_combinations_give_the_ordered_refusal) {
    std::size_t checked = 0;
    for (const decision_row &row : k_decision_table) {
        GLINTFX_CHECK(decide_gfx_format(facts_of(row)) == row.expected);
        ++checked;
    }
    std::println("gfx_format_decision_test: {} of 64 combination(s) checked", checked);
    GLINTFX_CHECK_EQ(checked, std::size_t{64});
}

GLINTFX_TEST(gfx_format_decision_the_named_cases_of_the_plan_hold) {
    gfx_format_facts facts; // every fact false
    GLINTFX_CHECK(decide_gfx_format(facts) == no_format);
    // MSAA impossible and sRGB not advertised: the sRGB is blamed, on both systems (rule 1 first).
    facts.msaa_requested = true;
    facts.srgb_requested = true;
    GLINTFX_CHECK(decide_gfx_format(facts) == srgb);
    // sRGB advertised, nothing found, a format without sRGB exists: still the sRGB.
    facts.srgb_advertised = true;
    facts.format_found_without_srgb = true;
    GLINTFX_CHECK(decide_gfx_format(facts) == srgb);
    // ... and without that fallback format, the MSAA is what could not be had.
    facts.format_found_without_srgb = false;
    GLINTFX_CHECK(decide_gfx_format(facts) == msaa);
    // A confirmation with no format found is never read (an impossible combination, rule 2
    // decides).
    facts.srgb_confirmed = true;
    GLINTFX_CHECK(decide_gfx_format(facts) == msaa);
}

namespace {

struct support_row {
    bool requested;
    bool advertised;
    bool confirmed;
    gltfx_gfx_option_support expected;
};

// D-SRGB2-1, written from the prose: asked and opened -> supported (opens only if confirmed); not
// asked
// -> supported if and only if the driver ADVERTISES the extension. Row i is the i-th combination,
// with `requested` the most significant bit.
constexpr gltfx_gfx_option_support yes = gltfx_gfx_option_support::supported;
constexpr gltfx_gfx_option_support no = gltfx_gfx_option_support::unsupported_here;
constexpr support_row k_support_table[] = {
    {false, false, false, no}, {false, false, true, no}, {false, true, false, yes},
    {false, true, true, yes},  {true, false, false, no}, {true, false, true, yes},
    {true, true, false, no},   {true, true, true, yes},
};

} // namespace

GLINTFX_TEST(gfx_format_decision_all_8_srgb_support_combinations_follow_the_one_rule) {
    constexpr std::size_t rows = sizeof(k_support_table) / sizeof(k_support_table[0]);
    std::size_t checked = 0;
    for (std::size_t i = 0; i < rows; ++i) {
        const support_row &row = k_support_table[i];
        const std::size_t pattern =
            (row.requested ? 4U : 0U) | (row.advertised ? 2U : 0U) | (row.confirmed ? 1U : 0U);
        GLINTFX_CHECK_EQ(pattern, i); // the table is the whole space, once
        GLINTFX_CHECK(srgb_option_support(row.requested, row.advertised, row.confirmed) ==
                      row.expected);
        ++checked;
    }
    std::println("gfx_format_decision_test: {} of 8 srgb_option_support combination(s) checked",
                 checked);
    GLINTFX_CHECK_EQ(checked, std::size_t{8});
}
