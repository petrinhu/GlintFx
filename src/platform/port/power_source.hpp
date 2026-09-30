// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

// platform/port/power_source.hpp - GFX-PRESET, the VALUE TYPE "where does the
// machine's power come from" (docs/plano-w6b-fatias-5.md sec. 5.1, D-W6b-34;
// /var/tmp/cto-w7d/PLANO-errata.md D-P1-1).
//
// A value type on its own, apart from the adapter port, on purpose: the pure
// automatic rule (auto_preset_rule.hpp) needs only this type, and must not
// include the header of a system port to get it (GODS_LAWS.md L-17/L-19). P2
// adds src/platform/port/power_source_port.hpp, which INCLUDES this file and
// adds only the concept `power_source_adapter_port`.
//
// Not public: the public face is the integer of the `power_source` option row
// (id 7). The numbers below are that row's DATA contract (a consumer's saved
// settings file stores them), so they are spelled out and never renumbered:
// unknown = 0, mains = 1, battery = 2. P3 ties each one to the public
// k_gltfx_power_source_* constants with a static_assert.
namespace glintfx::platform {

enum class gltfx_power_source : std::uint8_t {
    unknown = 0,
    mains = 1,
    battery = 2,
};

} // namespace glintfx::platform
