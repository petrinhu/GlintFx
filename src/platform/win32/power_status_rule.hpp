// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

#include "platform/port/power_source.hpp"

// platform/win32/power_status_rule.hpp - GFX-PRESET, fatia P2
// (docs/plano-w6b-fatias-5.md sec. 5.1, D-W6b-34, LENTE-2): the PURE rule over
// the two bytes of SYSTEM_POWER_STATUS that GetSystemPowerStatus() (kernel32)
// fills. It sees no Windows header, so it compiles and runs on all five
// systems; only power_source_adapter.cpp (the call itself) is Windows-only.
//
// THE RULE (D-W6b-34, Windows):
//   ACLineStatus == 1                             -> mains
//   ACLineStatus == 255 ("status unknown")        -> unknown
//   BatteryFlag  == 255 ("status unknown")        -> unknown, as its OWN named
//   case: 255 also
//       contains the 128 bit, and until this amendment it fell into `unknown`
//       by accident; a later change to the 128 rule must not break it silently,
//       so it has its own test cell
//   ACLineStatus == 0, BatteryFlag without 128    -> battery (a system battery
//   is present) ACLineStatus == 0, BatteryFlag with 128       -> unknown ("no
//   system battery" together with
//       no mains is an inconsistent reading)
//   any other ACLineStatus                        -> unknown
namespace glintfx::platform {

[[nodiscard]] gltfx_power_source classify_power_status(std::uint8_t ac_line_status,
                                                       std::uint8_t battery_flag) noexcept;

} // namespace glintfx::platform
