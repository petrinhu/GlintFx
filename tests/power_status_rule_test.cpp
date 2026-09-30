// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstdint>
#include <print>

#include "platform/port/power_source.hpp"
#include "platform/win32/power_status_rule.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// power_status_rule_test.cpp - GFX-PRESET, fatia P2 (docs/plano-w6b-fatias-5.md
// sec. 5.1/5.2, D-W6b-34, LENTE-2): the closed enumeration of the Windows power
// rule over the two bytes of SYSTEM_POWER_STATUS that GetSystemPowerStatus()
// fills. PURE: it compiles and runs on all five systems; only the adapter that
// fills the two bytes is Windows-only.
//
// THE RULE: ACLineStatus 1 -> mains; 255 -> unknown; BatteryFlag 255 -> unknown
// (the documented "status unknown", its OWN named case: 255 also contains the
// 128 bit, and without its own case a later change to the 128 rule would break
// it silently); ACLineStatus 0 without the 128 bit
// ("no system battery") -> battery; ACLineStatus 0 with it -> unknown (no
// battery and no mains is an inconsistent reading); any other ACLineStatus ->
// unknown.
//
// The plan's own table (sec. 5.2) lists seven cells, the adendo's row for P2
// says five: the seven are covered, every one, so neither reading is left
// unproved.
//
// RED, SEEN: before power_status_rule.{hpp,cpp} existed, this file's own
// #include line failed to compile.

using glintfx::platform::classify_power_status;
using glintfx::platform::gltfx_power_source;

GLINTFX_TEST(power_status_rule_closed_7_cell_enumeration) {
    int analyzed = 0;

    // ac 1 -> mains, whatever the battery flag says.
    GLINTFX_CHECK(classify_power_status(1, 0) == gltfx_power_source::mains);
    ++analyzed;
    GLINTFX_CHECK(classify_power_status(1, 128) == gltfx_power_source::mains);
    ++analyzed;
    // ac 0, a battery present (flag without the 128 bit) -> battery.
    GLINTFX_CHECK(classify_power_status(0, 0) == gltfx_power_source::battery);
    ++analyzed;
    // ac 0 with the 128 bit ("no system battery") -> unknown: no battery and no
    // mains.
    GLINTFX_CHECK(classify_power_status(0, 128) == gltfx_power_source::unknown);
    ++analyzed;
    // ac 0 with flag 255 ("status unknown") -> unknown: its OWN cell.
    GLINTFX_CHECK(classify_power_status(0, 255) == gltfx_power_source::unknown);
    ++analyzed;
    // ac 255 ("status unknown") -> unknown.
    GLINTFX_CHECK(classify_power_status(255, 0) == gltfx_power_source::unknown);
    ++analyzed;
    // ac 2 (outside the documented table) -> unknown.
    GLINTFX_CHECK(classify_power_status(2, 0) == gltfx_power_source::unknown);
    ++analyzed;

    GLINTFX_CHECK_EQ(analyzed, 7);
    std::println("power_status_rule_test: {} celula(s) conferida(s)", analyzed);
}
