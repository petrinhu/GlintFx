// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/win32/power_status_rule.hpp"

namespace glintfx::platform {

namespace {
constexpr std::uint8_t k_status_unknown = 255; // ACLineStatus and BatteryFlag, both
constexpr std::uint8_t k_ac_offline = 0;
constexpr std::uint8_t k_ac_online = 1;
constexpr std::uint8_t k_flag_no_system_battery = 128;
} // namespace

gltfx_power_source classify_power_status(std::uint8_t ac_line_status,
                                         std::uint8_t battery_flag) noexcept {
    if (ac_line_status == k_ac_online) {
        return gltfx_power_source::mains;
    }
    if (ac_line_status == k_status_unknown || battery_flag == k_status_unknown) {
        return gltfx_power_source::unknown;
    }
    if (ac_line_status == k_ac_offline && (battery_flag & k_flag_no_system_battery) == 0) {
        return gltfx_power_source::battery;
    }
    return gltfx_power_source::unknown;
}

} // namespace glintfx::platform
