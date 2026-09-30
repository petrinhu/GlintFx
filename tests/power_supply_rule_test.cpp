// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <print>
#include <span>
#include <string_view>
#include <vector>

#include "platform/port/power_source.hpp"
#include "platform/wayland/power_supply_rule.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// power_supply_rule_test.cpp - GFX-PRESET, fatia P2 (docs/plano-w6b-fatias-5.md
// sec. 5.1/5.2, D-W6b-34, F18): the closed enumeration of the Linux power rule
// over the entries of /sys/class/power_supply, every cell, no sample
// (GODS_LAWS.md L-40, project L-20).
//
// THE RULE (D-W6b-34), in this order: (1) an entry is a SOURCE when its type is
// not "Battery", and a SYSTEM BATTERY when its type is "Battery", its scope is
// not "Device" and it is present; a battery with scope "Device" (mouse,
// headset, pen) or absent (an empty bay) enters no rule, and a source is NEVER
// filtered by scope (F18: this machine's USB-C charger announces itself as
// scope Device); an entry with an empty type is unreadable and ignored. (2) any
// source online -> mains. (3) else any system battery Discharging -> battery.
// (4) else any system battery Charging, Full or "Not charging" -> mains. (5)
// else any system battery (status Unknown, empty or unreadable) -> battery. (6)
// else -> unknown.
//
// RED, SEEN: before power_supply_rule.{hpp,cpp} existed, this file's own
// #include line failed to compile.

using glintfx::platform::classify_power_supplies;
using glintfx::platform::gltfx_power_source;
using glintfx::platform::power_supply_entry;
using glintfx::platform::power_supply_tally;

namespace {
constexpr gltfx_power_source k_unknown = gltfx_power_source::unknown;
constexpr gltfx_power_source k_mains = gltfx_power_source::mains;
constexpr gltfx_power_source k_battery = gltfx_power_source::battery;

// One entry, spelled the way sysfs spells it; `present` and `online` as numbers. The views point
// at string literals, which outlive every call (the rule never owns a string: D-P2-1).
[[nodiscard]] power_supply_entry entry(std::string_view type, std::string_view scope, int present,
                                       int online, std::string_view status) {
    power_supply_entry result;
    result.type = type;
    result.scope = scope;
    result.present = present;
    result.online = online;
    result.status = status;
    return result;
}

[[nodiscard]] gltfx_power_source classify(const std::vector<power_supply_entry> &entries) {
    return classify_power_supplies(std::span<const power_supply_entry>(entries));
}
} // namespace

GLINTFX_TEST(power_supply_rule_closed_cell_enumeration) {
    int analyzed = 0;

    // 1. nothing at all.
    GLINTFX_CHECK(classify({}) == k_unknown);
    ++analyzed;
    // 2. Mains online.
    GLINTFX_CHECK(classify({entry("Mains", "", 1, 1, "")}) == k_mains);
    ++analyzed;
    // 3. USB online, scope System.
    GLINTFX_CHECK(classify({entry("USB", "System", 1, 1, "")}) == k_mains);
    ++analyzed;
    // 4. F18: the USB-C charger of this machine, scope Device, online, with a
    // Charging battery.
    GLINTFX_CHECK(classify({entry("USB", "Device", 1, 1, ""),
                            entry("Battery", "System", 1, 0, "Charging")}) == k_mains);
    // ... and the source alone decides: with the battery reading Unknown (rule 5
    // would say battery), a source of scope Device that is online still wins.
    // This is what tells "a source is never filtered by scope" apart from the old
    // rule; the Charging battery above would say mains by rule 4 even if the
    // source were dropped.
    GLINTFX_CHECK(classify({entry("USB", "Device", 1, 1, ""),
                            entry("Battery", "System", 1, 0, "Unknown")}) == k_mains);
    ++analyzed;
    // 5. Mains offline + Battery Discharging.
    GLINTFX_CHECK(classify({entry("Mains", "", 1, 0, ""),
                            entry("Battery", "System", 1, 0, "Discharging")}) == k_battery);
    ++analyzed;
    // 6. Mains offline + Battery Full: the energy comes from a source the sysfs
    // did not list.
    GLINTFX_CHECK(classify({entry("Mains", "", 1, 0, ""),
                            entry("Battery", "System", 1, 0, "Full")}) == k_mains);
    ++analyzed;
    // 7. Only a Battery, Charging.
    GLINTFX_CHECK(classify({entry("Battery", "System", 1, 0, "Charging")}) == k_mains);
    ++analyzed;
    // 8. Only a Battery, "Not charging".
    GLINTFX_CHECK(classify({entry("Battery", "System", 1, 0, "Not charging")}) == k_mains);
    ++analyzed;
    // 9. Only a Battery whose status is Unknown.
    GLINTFX_CHECK(classify({entry("Battery", "System", 1, 0, "Unknown")}) == k_battery);
    ++analyzed;
    // 10. The mouse's battery Discharging (scope Device) + Mains online: the
    // mouse never counts.
    GLINTFX_CHECK(classify({entry("Battery", "Device", 1, 0, "Discharging"),
                            entry("Mains", "", 1, 1, "")}) == k_mains);
    ++analyzed;
    // 11. Only a Device-scope Battery Discharging: a peripheral says nothing
    // about the machine.
    GLINTFX_CHECK(classify({entry("Battery", "Device", 1, 0, "Discharging")}) == k_unknown);
    ++analyzed;
    // 12. A Battery with present = 0 alone: an empty bay.
    GLINTFX_CHECK(classify({entry("Battery", "System", 0, 0, "Discharging")}) == k_unknown);
    ++analyzed;
    // 13. An empty bay does not hide the real battery.
    GLINTFX_CHECK(classify({entry("Battery", "System", 0, 0, "Discharging"),
                            entry("Battery", "System", 1, 0, "Discharging")}) == k_battery);
    ++analyzed;
    // 14. An entry with an empty type is unreadable and ignored (alone ->
    // unknown).
    GLINTFX_CHECK(classify({entry("", "System", 1, 1, "Discharging")}) == k_unknown);
    ++analyzed;

    // 15. The two-battery laptop (a ThinkPad) on battery: one Discharging, the other "Not
    // charging". Rule 3 (Discharging -> battery) has to be tested BEFORE rule 4 ("Not charging"
    // -> mains); the reverse order would read mains here, with no charger anywhere. Both orders
    // of the two entries, so the answer cannot depend on which battery the directory lists first.
    GLINTFX_CHECK(classify({entry("Battery", "System", 1, 0, "Discharging"),
                            entry("Battery", "System", 1, 0, "Not charging")}) == k_battery);
    GLINTFX_CHECK(classify({entry("Battery", "System", 1, 0, "Not charging"),
                            entry("Battery", "System", 1, 0, "Discharging")}) == k_battery);
    ++analyzed;
    // 16. A Battery is never a SOURCE, whatever its `online` says: online = 1 on a Battery that
    // is Discharging is battery, not mains (a rule that counted the Battery type as a source
    // would read mains). The twin: the same Battery with a status that alone says battery.
    GLINTFX_CHECK(classify({entry("Battery", "System", 1, 1, "Discharging")}) == k_battery);
    GLINTFX_CHECK(classify({entry("Battery", "System", 1, 1, "Unknown")}) == k_battery);
    ++analyzed;

    GLINTFX_CHECK_EQ(analyzed, 16);
    std::println("power_supply_rule_test: {} celula(s) conferida(s)", analyzed);
}

// The SAME rule fed one entry at a time (how the adapter uses it: no list is ever built). Every
// prefix of the F18 machine's supplies gives the answer the whole list would, and the tally is
// order-independent.
GLINTFX_TEST(power_supply_tally_one_entry_at_a_time) {
    int analyzed = 0;

    power_supply_tally empty;
    GLINTFX_CHECK(empty.result() == k_unknown);
    ++analyzed;

    // Battery first, charger second, and the reverse: the same answer.
    power_supply_tally battery_first;
    battery_first.add(entry("Battery", "System", 1, 0, "Discharging"));
    GLINTFX_CHECK(battery_first.result() == k_battery);
    battery_first.add(entry("USB", "Device", 1, 1, ""));
    GLINTFX_CHECK(battery_first.result() == k_mains);
    ++analyzed;

    power_supply_tally charger_first;
    charger_first.add(entry("USB", "Device", 1, 1, ""));
    charger_first.add(entry("Battery", "System", 1, 0, "Discharging"));
    GLINTFX_CHECK(charger_first.result() == k_mains);
    ++analyzed;

    GLINTFX_CHECK_EQ(analyzed, 3);
    std::println("power_supply_rule_test: {} celula(s) conferida(s) (uma entrada por vez)",
                 analyzed);
}
