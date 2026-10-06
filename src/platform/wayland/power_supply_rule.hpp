// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <span>
#include <string_view>

#include "platform/port/power_source.hpp"

// platform/wayland/power_supply_rule.hpp - GFX-PRESET, fatia P2 (docs/plano-w6b-fatias-5.md sec.
// 5.1, D-W6b-34, F18; /var/tmp/cto-w7d/PLANO-errata.md D-P2-1): the PURE rule that turns the
// entries of /sys/class/power_supply into where the machine's power comes from. It reads plain data
// and touches nothing of the system, so it compiles and runs on all five systems (like
// drm_gpu_kind.hpp beside it); only the adapter that fills the entries (power_source_adapter.cpp)
// is Linux-only.
//
// NO CONTAINER, NO OWNING STRING (D-P2-1, GODS_LAWS.md L-22, docs/api-conventions.md R3 degrau 1):
// an entry holds `std::string_view`s over a buffer the CALLER owns, and the rule can be fed ONE
// ENTRY AT A TIME (power_supply_tally), so the adapter never materializes a list. The views are
// only read during the call that receives them.
//
// THE RULE, in this order (D-W6b-34, reescrita LENTE-2/LENTE-6, F18):
//   (1) an entry is a SOURCE when its type is not "Battery", and a SYSTEM BATTERY when its type is
//       "Battery", its scope is not "Device" and `present` is not 0. A battery with scope
//       "Device" (mouse, headset, pen) or absent (an empty bay) enters NO rule below. A source is
//       NEVER filtered by scope: F18 measured that this machine's USB-C charger announces itself
//       as scope Device, and dropping it would read "battery" with the charger plugged in. An
//       entry with an EMPTY type is unreadable and ignored.
//   (2) any source with online == 1                                   -> mains
//   (3) else any system battery with status "Discharging"             -> battery
//   (4) else any system battery "Charging", "Full" or "Not charging"  -> mains (energy is coming
//       in, or is held by an external source the sysfs did not list or listed unreadable)
//   (5) else any system battery (status Unknown, empty or unreadable) -> battery
//   (6) else (nothing, only peripherals, only an empty bay)           -> unknown
//
// `present` and `online` are numbers already read by the adapter: a missing `present` file is
// spelled 1 (a battery is present unless the system says otherwise), a missing `online` 0, a
// missing `scope` an empty view (treated as System). A value the adapter could not read - missing,
// or LONGER than its 32-character buffer - arrives as an EMPTY view, never as a truncated prefix
// (fail closed). This atom never sees a file.
namespace glintfx::platform {

struct power_supply_entry {
    std::string_view type;
    std::string_view scope;
    int present = 1;
    int online = 0;
    std::string_view status;
};

// The rule as an accumulator: add every entry, in any order, then ask. Four booleans, no storage.
class power_supply_tally {
  public:
    void add(const power_supply_entry &entry) noexcept;
    [[nodiscard]] gltfx_power_source result() const noexcept;

  private:
    bool any_online_source = false;
    bool any_system_battery = false;
    bool any_discharging = false;
    bool any_energy_coming_in = false;
};

// The rule over a list that already exists (a test, a caller with its own storage).
[[nodiscard]] gltfx_power_source
classify_power_supplies(std::span<const power_supply_entry> entries) noexcept;

} // namespace glintfx::platform
