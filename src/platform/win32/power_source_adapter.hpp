// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "platform/port/power_source.hpp"

// platform/win32/power_source_adapter.hpp - GFX-PRESET, fatia P2
// (docs/plano-w6b-fatias-5.md sec. 5.1, D-W6b-34): the Windows reader of where
// the machine's power comes from. GetSystemPowerStatus (kernel32, an
// operating-system API the dependency-zero rule already allows) fills
// SYSTEM_POWER_STATUS; its two bytes go to the pure rule
// (power_status_rule.hpp). This header names no Windows type, so nothing here
// reaches windows.h; the .cpp is the only place that does.
namespace glintfx::platform {

// The concrete reader selected on Windows (selected_power_source_adapter.hpp).
// Stateless.
class win32_power_source_adapter {
  public:
    // Fresh at every call. The call failing is `unknown`, never an error (a
    // reading that failed says nothing about the machine).
    [[nodiscard]] gltfx_power_source read() const noexcept;
};

} // namespace glintfx::platform
