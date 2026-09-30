// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

// platform/gl/gfx_option_values.hpp - GFX-PRESET, fatia P1
// (/var/tmp/cto-w7d/PLANO-errata.md D-P1-5): the numbers of the option values
// the pure atoms speak (the preset table, the automatic rule, the expansion),
// in ONE internal header - never copied into the atoms that use them.
//
// TEMPORARY BY DECISION: the public constants (k_gltfx_preset_*,
// k_gltfx_vsync_*, k_gltfx_auto_choice_reason_*) only exist once P3 edits
// include/glintfx/platform/gl/gfx_option.hpp. P3 DELETES this file and the
// atoms use the public constants; there is no period of coexistence, so there
// is no static_assert of equality: afterwards there is ONE source. What guards
// the swap are the tests of P1, which assert the LITERAL numbers decided
// (manual = 0 ... automatic = 4, off = 0 / on = 1 / adaptive = 2, none = 0 ...
// unknown_gpu = 5 - D-W6b-44 and the comments of gfx_option_registry.hpp) and
// never these constants: a test that read the same constant as the code would
// agree with any renumbering.
//
// THE NUMBERS ARE A DATA CONTRACT (a consumer's saved settings file stores
// them): append-only, never renumbered.
namespace glintfx::platform {

// `preset` (id 5) and, by the same numbers, `suggested_preset` (id 8).
inline constexpr std::int64_t k_preset_manual = 0;
inline constexpr std::int64_t k_preset_power_saving = 1;
inline constexpr std::int64_t k_preset_balanced = 2;
inline constexpr std::int64_t k_preset_performance = 3;
inline constexpr std::int64_t k_preset_automatic = 4;

// `vsync` (id 0).
inline constexpr std::int64_t k_vsync_off = 0;
inline constexpr std::int64_t k_vsync_on = 1;
inline constexpr std::int64_t k_vsync_adaptive = 2;

// `auto_choice_reason` (id 6).
inline constexpr std::int64_t k_auto_reason_none = 0;
inline constexpr std::int64_t k_auto_reason_on_battery = 1;
inline constexpr std::int64_t k_auto_reason_software_renderer = 2;
inline constexpr std::int64_t k_auto_reason_shared_gpu = 3;
inline constexpr std::int64_t k_auto_reason_dedicated_gpu = 4;
inline constexpr std::int64_t k_auto_reason_unknown_gpu = 5;

} // namespace glintfx::platform
