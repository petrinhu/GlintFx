// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/wayland/power_supply_rule.hpp"

namespace glintfx::platform {

namespace {
[[nodiscard]] bool is_system_battery(const power_supply_entry &entry) noexcept {
    return entry.type == "Battery" && entry.scope != "Device" && entry.present != 0;
}

[[nodiscard]] bool is_source(const power_supply_entry &entry) noexcept {
    return !entry.type.empty() && entry.type != "Battery";
}

[[nodiscard]] bool status_means_energy_is_coming_in(std::string_view status) noexcept {
    return status == "Charging" || status == "Full" || status == "Not charging";
}
} // namespace

void power_supply_tally::add(const power_supply_entry &entry) noexcept {
    if (is_source(entry) && entry.online == 1) {
        any_online_source = true;
    }
    if (is_system_battery(entry)) {
        any_system_battery = true;
        any_discharging = any_discharging || entry.status == "Discharging";
        any_energy_coming_in =
            any_energy_coming_in || status_means_energy_is_coming_in(entry.status);
    }
}

gltfx_power_source power_supply_tally::result() const noexcept {
    if (any_online_source) {
        return gltfx_power_source::mains;
    }
    if (any_discharging) {
        return gltfx_power_source::battery;
    }
    if (any_energy_coming_in) {
        return gltfx_power_source::mains;
    }
    return any_system_battery ? gltfx_power_source::battery : gltfx_power_source::unknown;
}

gltfx_power_source classify_power_supplies(std::span<const power_supply_entry> entries) noexcept {
    power_supply_tally tally;
    for (const power_supply_entry &entry : entries) {
        tally.add(entry);
    }
    return tally.result();
}

} // namespace glintfx::platform
