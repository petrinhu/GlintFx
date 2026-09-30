// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/win32/power_source_adapter.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "platform/win32/power_status_rule.hpp"

namespace glintfx::platform {

gltfx_power_source win32_power_source_adapter::read() const noexcept {
    SYSTEM_POWER_STATUS status{};
    if (GetSystemPowerStatus(&status) == 0) {
        return gltfx_power_source::unknown;
    }
    return classify_power_status(status.ACLineStatus, status.BatteryFlag);
}

} // namespace glintfx::platform
