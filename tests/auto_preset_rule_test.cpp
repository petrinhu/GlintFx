// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstdint>
#include <print>

#include <glintfx/platform/gl/gpu.hpp>

#include "platform/gl/auto_preset_rule.hpp"
#include "platform/port/power_source.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// auto_preset_rule_test.cpp - GFX-PRESET, fatia P1 (docs/plano-w6b-fatias-5.md
// sec. 5.1/5.2, D-W6b-34): the closed 12-cell enumeration of the automatic
// rule, {unknown, software, shared, dedicated} x {unknown, mains, battery},
// every cell, no sample (GODS_LAWS.md L-40, project L-20).
//
// THE RULE, exactly: `battery` -> power_saving / on_battery, whatever the GPU;
// otherwise `dedicated` -> performance / dedicated_gpu; `software` -> balanced
// / software_renderer; `shared` -> balanced / shared_gpu; `unknown` -> balanced
// / unknown_gpu. And NO cell ever reads `none` as its reason (the public header
// promises it: every suggestion has a reason).
//
// RED, SEEN: before auto_preset_rule.{hpp,cpp} existed, this file's own
// #include line failed to compile.

using glintfx::gltfx_gpu_kind;
using glintfx::platform::auto_preset_choice;
using glintfx::platform::choose_preset_automatically;
using glintfx::platform::gltfx_power_source;

// The numbers of gltfx_power_source ARE the data contract of the `power_source`
// row (id 7): a consumer's saved settings file stores them. Spelled out, never
// renumbered (D-P1-1).
static_assert(static_cast<std::uint8_t>(gltfx_power_source::unknown) == 0);
static_assert(static_cast<std::uint8_t>(gltfx_power_source::mains) == 1);
static_assert(static_cast<std::uint8_t>(gltfx_power_source::battery) == 2);

namespace {
// THE LITERAL NUMBERS DECIDED (D-W6b-44, the comments of
// gfx_option_registry.hpp:79-98; D-P1-5): asserted here as numbers, NEVER
// through the constants the code itself uses, so that a renumbering of the
// internal header - or the swap to the public k_gltfx_* constants in P3 - is
// caught.
constexpr std::int64_t k_preset_manual = 0;
constexpr std::int64_t k_preset_power_saving = 1;
constexpr std::int64_t k_preset_balanced = 2;
constexpr std::int64_t k_preset_performance = 3;
constexpr std::int64_t k_preset_automatic = 4;
constexpr std::int64_t k_vsync_off = 0;
constexpr std::int64_t k_vsync_on = 1;
constexpr std::int64_t k_auto_reason_none = 0;
constexpr std::int64_t k_auto_reason_on_battery = 1;
constexpr std::int64_t k_auto_reason_software_renderer = 2;
constexpr std::int64_t k_auto_reason_shared_gpu = 3;
constexpr std::int64_t k_auto_reason_dedicated_gpu = 4;
constexpr std::int64_t k_auto_reason_unknown_gpu = 5;

struct cell {
    gltfx_gpu_kind kind;
    gltfx_power_source power;
    std::int64_t preset;
    std::int64_t reason;
};

constexpr gltfx_gpu_kind unk = gltfx_gpu_kind::unknown;
constexpr gltfx_gpu_kind sw = gltfx_gpu_kind::software;
constexpr gltfx_gpu_kind shr = gltfx_gpu_kind::shared;
constexpr gltfx_gpu_kind ded = gltfx_gpu_kind::dedicated;
constexpr gltfx_power_source p_unk = gltfx_power_source::unknown;
constexpr gltfx_power_source p_mains = gltfx_power_source::mains;
constexpr gltfx_power_source p_batt = gltfx_power_source::battery;

// The twelve cells, one line each, in the order {unknown, software, shared,
// dedicated} x {unknown, mains, battery}.
constexpr std::array<cell, 12> k_cells{{
    {unk, p_unk, k_preset_balanced, k_auto_reason_unknown_gpu},
    {unk, p_mains, k_preset_balanced, k_auto_reason_unknown_gpu},
    {unk, p_batt, k_preset_power_saving, k_auto_reason_on_battery},
    {sw, p_unk, k_preset_balanced, k_auto_reason_software_renderer},
    {sw, p_mains, k_preset_balanced, k_auto_reason_software_renderer},
    {sw, p_batt, k_preset_power_saving, k_auto_reason_on_battery},
    {shr, p_unk, k_preset_balanced, k_auto_reason_shared_gpu},
    {shr, p_mains, k_preset_balanced, k_auto_reason_shared_gpu},
    {shr, p_batt, k_preset_power_saving, k_auto_reason_on_battery},
    {ded, p_unk, k_preset_performance, k_auto_reason_dedicated_gpu},
    {ded, p_mains, k_preset_performance, k_auto_reason_dedicated_gpu},
    {ded, p_batt, k_preset_power_saving, k_auto_reason_on_battery},
}};
} // namespace

GLINTFX_TEST(auto_preset_rule_closed_12_cell_enumeration) {
    int analyzed = 0;

    for (const cell &expected : k_cells) {
        const auto_preset_choice got = choose_preset_automatically(expected.kind, expected.power);
        GLINTFX_CHECK_EQ(got.preset, expected.preset);
        GLINTFX_CHECK_EQ(got.reason, expected.reason);
        // The header promises it: no suggestion is ever without a reason.
        GLINTFX_CHECK(got.reason != k_auto_reason_none);
        ++analyzed;
    }

    GLINTFX_CHECK_EQ(analyzed, 12);
    std::println("auto_preset_rule_test: {} celula(s) conferida(s)", analyzed);
}
